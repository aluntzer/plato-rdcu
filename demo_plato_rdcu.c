/**
 * @file   demo_spw_rmap.c
 * @author Armin Luntzer (armin.luntzer@univie.ac.at),
 * @date   2018
 *
 * @copyright GPLv2
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * @brief RMAP RDCU usage demonstrator
 *
 * @note clocks and other board-dependent configuration are set up for the
 *	 GR712RC evaluation board (such as the SDRAM as the RDCU SRAM mirror)
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <irq.h>
#include <irq_dispatch.h>

#include <list.h>
#include <io.h>
#include <grspw2.h>
#include <rmap.h>
#include <asm/leon.h>
#include <errors.h>
#include <event_report.h>

#include <gr718b_rmap.h>
#include <rdcu_cmd.h>
#include <rdcu_ctrl.h>
#include <rdcu_rmap.h>

#include <cfg.h>
#include <demo.h>

#include <leon3_grtimer_longcount.h>


/* timer config */

#define CPU_CPS			80000000		/* in Hz */
#define GRTIMER_RELOAD		4
#define GRTIMER_MAX		0xffffffff
#define GRTIMER_TICKS_PER_SEC	((CPU_CPS / (GRTIMER_RELOAD + 1)))

struct grtimer_unit *rtu = (struct grtimer_unit *) LEON3_BASE_ADDRESS_GRTIMER;



#define MAX_PAYLOAD_SIZE	4096

/* XXX include extra for RMAP headers, 128 bytes is plenty */
#undef GRSPW2_DEFAULT_MTU
#define GRSPW2_DEFAULT_MTU (MAX_PAYLOAD_SIZE + 128)


/* a spacewire core configuration */
static struct spw_cfg {
	struct grspw2_core_cfg spw;
	uint32_t *rx_desc;
	uint32_t *tx_desc;
	uint8_t  *rx_data;
	uint8_t  *tx_data;
	uint8_t  *tx_hdr;
} spw_cfg;


/**
 * @brief dummy function for irq_dispatch, grspw2, etc...
 */

void event_report(enum error_class c, enum error_severity s, uint32_t err)
{
	printf("\nEVENT REPORT: class ");

	switch (c) {
	case GRSPW2:
		printf("%s ", "GRSPW2");
		break;
	default:
		printf("%d ", c);
		break;
	}

	switch (s) {
	case NORMAL:
		printf("%s ", "NORMAL");
		break;
	case LOW:
		printf("%s ", "LOW");
		break;
	case MEDIUM:
		printf("%s ", "MEDIUM");
		break;
	case HIGH:
		printf("%s ", "HIGH");
		break;
	default:
		printf("%d ", s);
		break;
	}

	switch (err) {
	case E_SPW_PARITY_ERROR:
		printf("%s\n", "PARITY_ERROR\n");
		break;
	case E_SPW_ESCAPE_ERROR:
		printf("%s\n", "ESCAPE_ERROR\n");
		break;
	case E_SPW_CREDIT_ERROR:
		printf("%s\n", "CREDIT_ERROR\n");
		break;
	default:
		printf("%lu\n", err);
		break;

	}
}


/**
 * tx function for rdcu_ctrl
 *
 * @note you may want to reimplement this function if you use a different
 *	 SpaceWire interface or if you want transport/dump the RMAP
 *	 packets via a different mechanism, e.g. using rdcu_package()
 *
 * @warn If you use rdcu_package() to simply dump generated RMAP command
 *	 packets, you may run into the limit set by TRANS_LOG_SIZE, as
 *	 transactions make an entry in the transaction log, which will only
 *	 free up slots when an ACK with the  corresponding RMAP transaction id
 *	 has been received. So, if you simply want to dump a set of commands,
 *	 and run into issues, increase TRANS_LOG_SIZE by an arbitrary value
 */

static int32_t rmap_tx(const void *hdr,  uint32_t hdr_size,
		       const uint8_t non_crc_bytes,
		       const void *data, uint32_t data_size)
{
	return grspw2_add_pkt(&spw_cfg.spw, hdr, hdr_size, non_crc_bytes, data, data_size);

}


/**
 * rx function for rdcu_ctrl
 *
 * @note you may want to reimplement this function if you use a different
 *	 SpaceWire interface or if you want inject RMAP packets via a
 *	 different mechanism
 */

static uint32_t rmap_rx(uint8_t *pkt)
{
	return grspw2_get_pkt(&spw_cfg.spw, pkt);
}


/**
 * @brief allocate and align a descriptor table as well as data memory for a
 *	  spw core configuration
 */

static void spw_alloc(struct spw_cfg *cfg)
{
	uint32_t mem;


	/*
	 * malloc a rx and tx descriptor table buffer and align to
	 * 1024 bytes (GR712UMRC, p. 111)
	 *
	 * dynamically allocate memory + 1K for alignment (worst case)
	 * 1 buffer per dma channel (GR712 cores only implement one channel)
	 *
	 * NOTE: we don't care about calling free(), because this is a
	 * bare-metal demo, so we just discard the original pointer
	 */

	mem = (uint32_t) calloc(GRSPW2_DESCRIPTOR_TABLE_SIZE
				+ GRSPW2_DESCRIPTOR_TABLE_MEM_BLOCK_ALIGN, 1);

	cfg->rx_desc = (uint32_t *)
		       ((mem + GRSPW2_DESCRIPTOR_TABLE_MEM_BLOCK_ALIGN)
			& ~GRSPW2_DESCRIPTOR_TABLE_MEM_BLOCK_ALIGN);


	mem = (uint32_t) calloc(GRSPW2_DESCRIPTOR_TABLE_SIZE
				+ GRSPW2_DESCRIPTOR_TABLE_MEM_BLOCK_ALIGN, 1);

	cfg->tx_desc = (uint32_t *)
		       ((mem + GRSPW2_DESCRIPTOR_TABLE_MEM_BLOCK_ALIGN)
			& ~GRSPW2_DESCRIPTOR_TABLE_MEM_BLOCK_ALIGN);


	/* malloc rx and tx data buffers: decriptors * packet size */
	cfg->rx_data = (uint8_t *) calloc(GRSPW2_RX_DESCRIPTORS
					  * GRSPW2_DEFAULT_MTU, 1);
	cfg->tx_data = (uint8_t *) calloc(GRSPW2_TX_DESCRIPTORS
					  * GRSPW2_DEFAULT_MTU, 1);

	cfg->tx_hdr = (uint8_t *) calloc(GRSPW2_TX_DESCRIPTORS * HDR_SIZE, 1);
}


/**
 * @brief perform basic initialisation of the spw core
 */

static void spw_init_core(struct spw_cfg *cfg)
{
	/* select GR712 INCLCK */
	set_gr712_spw_clock();

	/* configure for spw core0 */
	grspw2_core_init(&cfg->spw, GRSPW2_BASE_CORE_0,
			 ICU_ADDR, SPW_CLCKDIV_START, SPW_CLCKDIV_RUN,
			 GRSPW2_DEFAULT_MTU, GR712_IRL2_GRSPW2_0,
			 GR712_IRL1_AHBSTAT, 0);

	grspw2_rx_desc_table_init(&cfg->spw,
				  cfg->rx_desc,
				  GRSPW2_DESCRIPTOR_TABLE_SIZE,
				  cfg->rx_data,
				  GRSPW2_DEFAULT_MTU);

	grspw2_tx_desc_table_init(&cfg->spw,
				  cfg->tx_desc,
				  GRSPW2_DESCRIPTOR_TABLE_SIZE,
				  cfg->tx_hdr, HDR_SIZE,
				  cfg->tx_data, GRSPW2_DEFAULT_MTU);
}


/**
 * @brief generate a command packet for the SpW router
 */
__attribute__((unused))
static int rdcu_gen_router_cmd_internal(uint16_t trans_id, uint8_t *cmd,
					uint8_t rmap_cmd_type,
					uint32_t addr, uint32_t size)
{
	int n;

	struct rmap_pkt *pkt;
	const uint8_t dpath[] = {0x0};

	pkt = rmap_create_packet();
	if (!pkt) {
		printf("Error creating packet\n");
		return 0;
	}

	rmap_set_dst(pkt, 0xFE);
	rmap_set_dest_path(pkt, dpath, 1);

	rmap_set_src(pkt, 0x20);
	rmap_set_key(pkt, 0x0);
	rmap_set_cmd(pkt, rmap_cmd_type);
	rmap_set_tr_id(pkt, trans_id);
	rmap_set_data_addr(pkt, addr);
	rmap_set_data_len(pkt, size);

	/* determine header size */
	n = rmap_build_hdr(pkt, NULL);

	if (!cmd) {
		rmap_erase_packet(pkt);
		return n;
	}

	bzero(cmd, n);

	n = rmap_build_hdr(pkt, cmd);

	rmap_erase_packet(pkt);

	return n;
}


/**
 * @brief configure the GR718B router
 *
 * @note plug in to physical port 1 and off we go!
 */

static void gr718b_cfg_router(void)
{
	printf("\nConfiguring GR718B SpW Router."
	       "\nYou can ignore any messages below, unless you get stuck."
	       "\n========================================================\n"
	       "\n");

	printf("Enabling routing table address control for RDCU and ICU "
	       "logical addresses (0x%02X and 0x%02X).\n",
	       RDCU_ADDR, ICU_ADDR);

	gr718b_set_rtactrl_enabled(RDCU_ADDR);
	gr718b_set_rtactrl_enabled(ICU_ADDR);


	printf("Clearing header deletion bit in routing table access control "
	       "for RDCU and ICU logical addresses.\n");

	gr718b_clear_addr_header_deletion(RDCU_ADDR);
	gr718b_clear_addr_header_deletion(ICU_ADDR);


	printf("Enabling routes of logical addresses 0x%02X and 0x%02X to "
	       "physical port addresses 0x%02X and 0x%02X respectively.\n",
	       RDCU_ADDR, ICU_ADDR, RDCU_PHYS_PORT, ICU_PHYS_PORT);

	gr718b_set_route_port(RDCU_ADDR, RDCU_PHYS_PORT);
	gr718b_set_route_port(ICU_ADDR,  ICU_PHYS_PORT);


	printf("Configuring run-state clock divisors (%d) of physical port "
	       "addresses 0x%02X and 0x%02X.\n",
	       SPW_CLCKDIV_RUN, RDCU_PHYS_PORT, ICU_PHYS_PORT);

	gr718b_set_rt_clkdiv(RDCU_PHYS_PORT, SPW_CLCKDIV_RUN - 1);
	gr718b_set_rt_clkdiv(ICU_PHYS_PORT, SPW_CLCKDIV_RUN - 1);


	printf("Enabling time-code transmission on physical port addresses "
	       "0x%02X and 0x%02X.\n", RDCU_PHYS_PORT, ICU_PHYS_PORT);

	gr718b_set_time_code_enable(RDCU_PHYS_PORT);
	gr718b_set_time_code_enable(ICU_PHYS_PORT);


	printf("Setting link-start bits on port addresses 0x%02X and 0x%02X.\n",
	        RDCU_PHYS_PORT, ICU_PHYS_PORT);

	gr718b_set_link_start(RDCU_PHYS_PORT);
	gr718b_set_link_start(ICU_PHYS_PORT);

	printf("\nGR718B configuration complete."
	       "\n==============================\n\n");
}


/**
 * @brief save repeating 3 lines of code..
 *
 * @note prints abort message if pending status is non-zero after 10 retries
 */

static void sync(void)
{
	int cnt = 0;
	printf("syncing...");
	while (rdcu_rmap_sync_status()) {
		printf("pending: %d\n", rdcu_rmap_sync_status());

		if (cnt++ > 10) {
			printf("aborting; de");
			break;
		}

	}
	printf("synced\n");
}


/**
 * @brief retrieve and print the RMAP error counters in the RDCU
 */

static void rdcu_show_rmap_errors(void)
{
	rdcu_sync_rmap_no_reply_err_cntrs();
	rdcu_sync_rmap_last_err();
	rdcu_sync_rmap_pckt_err_cntrs();
	sync();

	printf("RMAP incomplete header errors %d\n",
	       rdcu_get_rmap_incomplete_hdrs());
	printf("RMAP received reply packets %d\n",
	       rdcu_get_rmap_recv_reply_pckts());
	printf("RMAP received non-RMAP packets %d\n",
	       rdcu_get_recv_non_rmap_pckts());

	printf("RMAP last error user code: %X\n",
		rdcu_get_rmap_last_error_user_code());
	printf("RMAP last error standard code: %X\n",
		rdcu_get_rmap_last_error_standard_code());

	printf("RMAP packet with length or content error counter: %d\n",
		rdcu_get_rmap_pckt_errs());
	printf("RMAP operation error counter: %d\n",
		rdcu_get_rmap_oper_errs());
	printf("RMAP command authorization errors: %d\n",
		rdcu_get_rmap_cmd_auth_errs());
	printf("RMAP header errors: %d\n",
		rdcu_get_rmap_hdr_errs());
}


/**
 * @brief verify that data exchange between the local SRAM mirror and
 *	  the RDCU SRAM works as intended
 *
 * @note  This writes the local SRAM mirror with a pattern, then transfers
 *	  the mirror to the RDCU, clears the mirror and retrieves the remote
 *	  data. If the pattern is found throughout the local copy, the transfer-
 *	  cycle was without error.
 *
 * @warn  Since we use the GR712RC development board as our baseline, we
 *	  have set the local mirror to the SDRAM bank at 0x60000000 just
 *	  as we do in rdcu_ctrl_init()
 *
 * yep, it's quick and dirty...
 */

static void rdcu_verify_data_transfers(void)
{
#define MAX_ERR_CNT	10

	int i;
	int cnt = 0;
	int size = (int) RDCU_SRAM_SIZE >> 2;
	uint32_t *ram = (uint32_t *) 0x60000000;



	printf("Performing SRAM transfer verification.\n");

	printf("Clearing local SRAM mirror\n");
	bzero(ram, RDCU_SRAM_SIZE);

	printf("Setting pattern in mirror\n");
	for (i = 0; i < size; i++)
		ram[i] = 0xdeadcafe;



	printf("Begin transfer cycle\n");

	printf("\nMIRROR -> SRAM\n");
	/* now sync the sram chunks to the RDCU */
	if (rdcu_sync_mirror_to_sram(DATASTART, RDCU_SRAM_SIZE,  MAX_PAYLOAD_SIZE))
		printf("BIG FAT TRANSFER ERROR!\n");
	sync();
	printf("\nDONE\n");

	printf("Zeroing mirror...\n");
	bzero(ram, RDCU_SRAM_SIZE);


	printf("\nSRAM -> MIRROR\n");
	/* now sync the sram chunks to the RDCU */
	if (rdcu_sync_sram_to_mirror(DATASTART, RDCU_SRAM_SIZE,  MAX_PAYLOAD_SIZE))
		printf("BIG FAT TRANSFER ERROR!\n");
	sync();
	printf("\nDONE\n");


	printf("Checking pattern in mirror\n");

	for (i = 0; i < size; i++) {
		if (ram[i] != 0xdeadcafe) {
			if (cnt < MAX_ERR_CNT)
				printf("invalid pattern at address %08X: %08lX\n",
				       i << 2, ram[i]);
			cnt++;
		}
	}

	printf("Check complete, %d error(s) encountered (max %d listed)\n\n",
	       cnt, MAX_ERR_CNT);



	return;
}


/**
 * @brief demonstrate a compression cycle
 */

static void rdcu_compression_demo(void)
{
	int cnt;

	/* first, set compression paramters in local mirror registers */
	printf("Configuring compression mode 3, weighting 8\n");
	rdcu_set_compression_mode(3);
	rdcu_set_weighting_param(8);

	printf("Configuring spillover threshold 48, golomb param 4\n");
	rdcu_set_spillover_threshold(48);
	rdcu_set_golomb_param(4);

	printf("Configuring adaptive 1 spillover threshold 35, "
	       "golomb param 3\n");
	rdcu_set_adaptive_1_spillover_threshold(35);
	rdcu_set_adaptive_1_golomb_param(3);

	printf("Configuring adaptive 2 spillover threshold 60, "
	       "golomb param 5\n");
	rdcu_set_adaptive_2_spillover_threshold(60);
	rdcu_set_adaptive_2_golomb_param(5);

	printf("Configuring data start address 0x%08lX\n", DATASTART);
	rdcu_set_data_start_addr(DATASTART);

	printf("Configuring model start address 0x%08lX\n", MODELSTART);
	rdcu_set_model_start_addr(MODELSTART);

	printf("Configuring updated model start address 0x%08lX\n", UPDATED_MODELSTAT);
	rdcu_set_new_model_start_addr(UPDATED_MODELSTAT);

	printf("Configuring compressed start address 0x%08lX\n", COMPRSTART);
	rdcu_set_compr_data_buf_start_addr(COMPRSTART);

	printf("Configuring compressed data length %ld\n", COMPRDATALEN);
	rdcu_set_compr_data_buf_len(COMPRDATALEN);

	printf("Configuring number of samples %ld\n", NUMSAMPLES);
	rdcu_set_num_samples(NUMSAMPLES);

	/* now sync the relevant registers to the RDCU... */
	rdcu_sync_compressor_param1();
	rdcu_sync_compressor_param2();
	rdcu_sync_adaptive_param1();
	rdcu_sync_adaptive_param2();
	rdcu_sync_data_start_addr();
	rdcu_sync_model_start_addr();
	rdcu_sync_new_model_start_addr();
	rdcu_sync_compr_data_buf_start_addr();
	rdcu_sync_compr_data_buf_len();
	rdcu_sync_num_samples();

	/* ...and wait for completion */
	sync();


	/* now set the data in the local mirror... */
	rdcu_write_sram(data, DATASTART, NUMSAMPLES * 2);

	/*...and the model... */
	rdcu_write_sram(model, MODELSTART, NUMSAMPLES * 2);

	/* sync */
	rdcu_sync_mirror_to_sram(DATASTART,  NUMSAMPLES * 2, MAX_PAYLOAD_SIZE);
	rdcu_sync_mirror_to_sram(MODELSTART, NUMSAMPLES * 2, MAX_PAYLOAD_SIZE);


	/* wait */
	sync();


	printf("Configuring compression start bit and starting compression\n");
	rdcu_set_data_compr_start();
	rdcu_sync_compr_ctrl();
	sync();

	/* clear local bit immediately, this is a write-only register.
	 * we would not want to restart compression by accidentially calling
	 * rdcu_sync_compr_ctrl() again
	 */
	rdcu_clear_data_compr_start();


	/* start polling the compression status */
	rdcu_sync_compr_status();
	sync();
	cnt = 0;
	while (!rdcu_get_data_compr_ready()) {

		/* check compression status */
		rdcu_sync_compr_status();
		sync();
		cnt++;

		if (cnt < 5)	/* wait for 5 polls */
			continue;


		printf("Not waiting for compressor to become ready, will "
		       "check status and abort\n");

		rdcu_set_data_compr_interrupt();
		rdcu_sync_compr_ctrl();
		sync();
		rdcu_clear_data_compr_interrupt(); /* always clear locally */

		/* now we may read the error code */
		rdcu_sync_compr_error();
		sync();
		printf("Compressor error code: 0x%02X\n",
		       rdcu_get_compr_error());

		return;
	}

	printf("Compression took %d polling cycles\n\n", cnt);


	printf("Compressor status: ACT: %ld, RDY: %ld, DATA VALID: %ld\n",
	       rdcu_get_data_compr_active(),
	       rdcu_get_data_compr_ready(),
	       rdcu_get_compr_status_valid());

	/* now we may read the error code */
	rdcu_sync_compr_error();
	sync();
	printf("Compressor error code: 0x%02X\n",
	       rdcu_get_compr_error());


	rdcu_sync_compr_data_size();
	sync();
	printf("Compressed data size: %ld\n", rdcu_get_compr_data_size() >> 3);


	/* issue sync back of compressed data */
	if (rdcu_sync_sram_to_mirror(COMPRSTART,
				     (((rdcu_get_compr_data_size() >> 3) + 3) & ~0x3UL),
				     MAX_PAYLOAD_SIZE)) {
		printf("error in rdcu_sync_ram_to_mirror!\n");
	}

	/* wait for it */
	sync();

	/* read compressed data to some buffer and print */
	if (1) {
		uint32_t i;
		uint32_t s = rdcu_get_compr_data_size() >> 3;
		uint8_t *myresult = malloc(s);
		rdcu_read_sram(myresult, COMPRSTART, s);

		printf("\n\nHere's the compressed data (size %lu):\n"
		       "================================\n", s);

		for (i = 0; i < s; i++) {
			printf("%02X ", myresult[i]);
			if (i && !((i+1) % 40))
				printf("\n");
		}
		printf("\n");

		free(myresult);
	}
}



/**
 * @brief exchange some stuff
 */

static void rdcu_demo(void)
{
	struct grtimer_uptime t0, t1;


	grtimer_longcount_get_uptime(rtu, &t0);

	/* get some status info from the RDCU */
	rdcu_sync_fpga_version();
	rdcu_sync_compr_status();
	sync();
	printf("Current FPGA version: %d\n", rdcu_get_fpga_version());
	printf("Compressor status ready: %s\n",
	       rdcu_get_data_compr_ready() ? "yes" : "no");
	printf("Compressor active: %s\n",
	       rdcu_get_data_compr_active() ? "yes" : "no");
	printf("Compressor status interrupted: %s\n",
	       rdcu_get_data_compr_interrupted() ? "yes" : "no");
	printf("Compressor status data valid: %s\n",
	       rdcu_get_compr_status_valid() ? "yes" : "no");

	/* if the compressor is busy, RMAP will respond with a "general error
	 * code" because the control registers are blocked
	 */
	if (rdcu_get_data_compr_active()) {
		printf("Compressor is active, must interrupt or RMAP cannot "
		       "access the data compressor control registers\n");
		rdcu_set_data_compr_interrupt();
		rdcu_sync_compr_ctrl();
		sync();
		rdcu_clear_data_compr_interrupt(); /* always clear locally */
		rdcu_sync_compr_status(); /* read back status */
		sync();

		if (rdcu_get_data_compr_active()) {
			printf("ERRROR: compressor still active, aborting\n");
			return;
		}
	}


	/* change the RDCU link speed to 100 Mbit (divider:1 -> CLKDIV:0) */
	rdcu_set_spw_link_run_clkdiv(0);
	rdcu_sync_spw_link_ctrl();
	sync();
	rdcu_sync_spw_link_status();
	sync();
	printf("RDCU linkdiv now set to: %d\n", rdcu_get_spw_run_clk_div() + 1);


	/* have a look at the RDCU RMAP error counters */
	rdcu_show_rmap_errors();

	grtimer_longcount_get_uptime(rtu, &t1);
	printf("SYNC in %g seconds\n", grtimer_longcount_difftime(rtu, t1, t0));

	/* check transfer program */
	rdcu_verify_data_transfers();

	/* have a look at the RDCU RMAP error counters again*/
	rdcu_show_rmap_errors();

	/* now do some compression work */
	rdcu_compression_demo();
}


int main(void)
{
	uint8_t dpath[] = DPATH;
	uint8_t rpath[] = RPATH;

	/* the grspw driver relies on the IRQ subsystem for link event
	 * detection, so we'll initialise it here
	 */
	irq_dispatch_enable();

        grtimer_longcount_start(rtu, GRTIMER_RELOAD,
				GRTIMER_TICKS_PER_SEC, GRTIMER_MAX);




	/* local SpW port configuration */
	spw_alloc(&spw_cfg);
	spw_init_core(&spw_cfg);

	grspw2_core_start(&spw_cfg.spw);
	grspw2_set_rmap(&spw_cfg.spw);

	/* not really needed, but still disable filters, we want to notice
	 * everything sent to the port
	 */
	grspw2_set_promiscuous(&spw_cfg.spw);


	/* router interface */
	gr718b_rmap_init(ICU_ADDR, rmap_tx, rmap_rx);
	gr718b_cfg_router();


	/* initialise the libraries */
	rdcu_ctrl_init();
	rdcu_rmap_init(GRSPW2_DEFAULT_MTU, rmap_tx, rmap_rx);

	/* set initial link configuration */
	rdcu_set_destination_logical_address(RDCU_ADDR_START);
	rdcu_set_source_logical_address(ICU_ADDR);
	rdcu_set_destination_path(dpath, DPATH_LEN);
	rdcu_set_return_path(rpath, RPATH_LEN);
	rdcu_set_destination_key(RDCU_DEST_KEY);


	/* update target logical address in RDCU core control */
	rdcu_set_rmap_target_logical_address(RDCU_ADDR);
	rdcu_sync_core_ctrl();
	sync();


	/* a direct route has been configured and the remote logical address
	 * was updated, we can drop the path routing now (although it would
	 * still work)
	 */
	rdcu_set_destination_logical_address(RDCU_ADDR);
	rdcu_set_destination_path(NULL, 0);
	rdcu_set_return_path(NULL, 0);

	/* now run the demonstrator */
	rdcu_demo();

	return 0;
}

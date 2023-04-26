/**
 * @file   init_rdcu.c
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2023
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
 * @brief initialisation of the RMAP communication between GR712 and the RDCU
 *
 * @note clocks and other board-dependent configuration are set up for the
 *	 GR712RC evaluation board (such as the SDRAM as the RDCU SRAM mirror)
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <irq.h>
#include <irq_dispatch.h>

#include <grspw2.h>
#include <errors.h>
#include <event_report.h>

#include <gr718b_rmap.h>
#include <rdcu_ctrl.h>
#include <rdcu_rmap.h>

#include <cfg.h>


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

#if 0
       uint8_t blob[8192];
       int n, i;

       n =  rdcu_package(blob, hdr, hdr_size, non_crc_bytes,
                         data, data_size);

       for (i = 0; i < n; i++) {
               printf("%02X ", blob[i]);
               if (i && !((i+1) % 40))
                       printf("\n");
       }
       printf("\n");
#endif
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

	mem = (uint32_t) calloc(1, GRSPW2_DESCRIPTOR_TABLE_SIZE
				+ GRSPW2_DESCRIPTOR_TABLE_MEM_BLOCK_ALIGN);

	cfg->rx_desc = (uint32_t *)
		       ((mem + GRSPW2_DESCRIPTOR_TABLE_MEM_BLOCK_ALIGN)
			& ~GRSPW2_DESCRIPTOR_TABLE_MEM_BLOCK_ALIGN);


	mem = (uint32_t) calloc(1, GRSPW2_DESCRIPTOR_TABLE_SIZE
				+ GRSPW2_DESCRIPTOR_TABLE_MEM_BLOCK_ALIGN);

	cfg->tx_desc = (uint32_t *)
		       ((mem + GRSPW2_DESCRIPTOR_TABLE_MEM_BLOCK_ALIGN)
			& ~GRSPW2_DESCRIPTOR_TABLE_MEM_BLOCK_ALIGN);


	/* malloc rx and tx data buffers: descriptors * packet size */
	cfg->rx_data = (uint8_t *) calloc(1, GRSPW2_RX_DESCRIPTORS
					  * GRSPW2_DEFAULT_MTU);
	cfg->tx_data = (uint8_t *) calloc(1, GRSPW2_TX_DESCRIPTORS
					  * GRSPW2_DEFAULT_MTU);

	cfg->tx_hdr = (uint8_t *) calloc(1, GRSPW2_TX_DESCRIPTORS * HDR_SIZE);
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
 * @brief configure the GR718B router
 *
 * @note plug in to physical port 1 and off we go!
 */

static void gr718b_cfg_router(void)
{
	printf("\nConfiguring GR718B SpW Router."
	       /* "\nYou can ignore any messages below, unless you get stuck." */
	       /* "\n========================================================\n" */
	       "\n");

	/* printf("Enabling routing table address control for RDCU and ICU " */
	/*        "logical addresses (0x%02X and 0x%02X).\n", */
	       /* RDCU_ADDR, ICU_ADDR); */

	gr718b_set_rtactrl_enabled(RDCU_ADDR);
	gr718b_set_rtactrl_enabled(ICU_ADDR);


	/* printf("Clearing header deletion bit in routing table access control " */
	/*        "for RDCU and ICU logical addresses.\n"); */

	gr718b_clear_addr_header_deletion(RDCU_ADDR);
	gr718b_clear_addr_header_deletion(ICU_ADDR);


	/* printf("Enabling routes of logical addresses 0x%02X and 0x%02X to " */
	/*        "physical port addresses 0x%02X and 0x%02X respectively.\n", */
	       /* RDCU_ADDR, ICU_ADDR, RDCU_PHYS_PORT, ICU_PHYS_PORT); */

	gr718b_set_route_port(RDCU_ADDR, RDCU_PHYS_PORT);
	gr718b_set_route_port(ICU_ADDR,  ICU_PHYS_PORT);


	/* printf("Configuring run-state clock divisors (%d) of physical port " */
	/*        "addresses 0x%02X and 0x%02X.\n", */
	       /* SPW_CLCKDIV_RUN, RDCU_PHYS_PORT, ICU_PHYS_PORT); */

	gr718b_set_rt_clkdiv(RDCU_PHYS_PORT, SPW_CLCKDIV_RUN - 1);
	gr718b_set_rt_clkdiv(ICU_PHYS_PORT, SPW_CLCKDIV_RUN - 1);


	/* printf("Enabling time-code transmission on physical port addresses " */
	/*        "0x%02X and 0x%02X.\n", RDCU_PHYS_PORT, ICU_PHYS_PORT); */

	gr718b_set_time_code_enable(RDCU_PHYS_PORT);
	gr718b_set_time_code_enable(ICU_PHYS_PORT);


	/* printf("Setting link-start bits on port addresses 0x%02X and 0x%02X.\n", */
	/*        RDCU_PHYS_PORT, ICU_PHYS_PORT); */

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
 * @brief initialisation of the RMAP communication between GR712 and the RDCU
 */

void init_rdcu(void)
{
	uint8_t dpath[] = DPATH;
	uint8_t rpath[] = RPATH;

	/* the grspw driver relies on the IRQ subsystem for link event
	 * detection, so we'll initialise it here
	 */
	irq_dispatch_enable();

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
	rdcu_rmap_init(MAX_PAYLOAD_SIZE, rmap_tx, rmap_rx);

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

	/* get some status info from the RDCU */
	rdcu_sync_compr_status();

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
			printf("ERROR: compressor still active, aborting\n");
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
}



static int32_t rmap_tx_print(const void *hdr, uint32_t hdr_size,
			       const uint8_t non_crc_bytes, const void *data,
			       uint32_t data_size)
{
	uint8_t blob[8192];
	int n, i;

	n =  rdcu_package(blob, hdr, hdr_size, non_crc_bytes,
			  data, data_size);

	for (i = 0; i < n; i++) {
		printf("%02X ", blob[i]);
		if (i && !((i+1) % 40))
			printf("\n");
	}
	printf("\n");
	return 0;

}


/**
 * @brief Dummy implementation of the rmap_rx function for the rdcu_rmap lib.
 *	We do not want to receive any packages.
 */

static uint32_t rmap_rx_dummy(uint8_t *pkt)
{
	(void)(pkt);
	return 0;
}


int init_rmap_pkt_print(void)
{
	uint8_t icu_addr, rdcu_addr;
	int mtu;

	icu_addr = 0xA7;
	rdcu_addr = 0xEF;
	mtu = 4224;
	rdcu_ctrl_init();
	rdcu_set_source_logical_address(icu_addr);
	rdcu_set_destination_logical_address(rdcu_addr);
	rdcu_set_destination_key(RDCU_DEST_KEY);
	rdcu_rmap_init(mtu, rmap_tx_print, rmap_rx_dummy);

	return 0;
}


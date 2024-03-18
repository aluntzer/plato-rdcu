/**
 * @file   rdcu_rmap.c
 * @author Armin Luntzer (armin.luntzer@univie.ac.at)
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
 * @brief RMAP RDCU link interface
 * @see FPGA Requirement Specification PLATO-IWF-PL-RS-005 Issue 0.7
 *
 *
 * Ideally, we would use asynchronous operations here, with a transaction log
 * that is served by another thread. However, we cannot assume that such
 * features will be available, so we'll do this by maintaining a mirror of the
 * RDCU's registers and memory, where instead of actively blocking with get()
 * and set() RMAP calls, they operate on the local copy and the user issues
 * sync() calls.
 *
 * To monitor the syncronisation status, we maintaining a transaction log
 * tracking the submitted command set. Response packets could be processed
 * by interrupt (or thread), but in this variant, we process the return packets
 * when the user calls rdcu_ctrl_sync_status()
 *
 * This is probably the nicest solution when it comes to call overhead, but it
 * requires 8 MiB of memory for the SRAM mirror and the some for the registers.
 *
 * Note that for simplicity , we assume that there is a working heap allocator
 * available, please adapt all malloc/free calls to your needs, or ask us
 * to do that for you.
 *
 * @note in order to run this on the GR712RC eval board, we set the SRAM mirror
 *	 image to the boards SDRAM in rdcu_ctrl_init() and just malloc() it for
 *	 the PC (see rdcu_ctrl_init)
 *
 * @note The interface requires that you provide an RX and a TX function,
 *	 see rdcu_ctrl_init for the call interface.
 *	 The TX function shall to return 0 on success, everything else
 *	 is considered an error in submission. The RX function shall return
 *	 the size of the packet buffer and accept NULL as call argument, on
 *	 which it shall return the buffer size required to store the next
 *	 pending packet.
 *	 You can use these functions to adapt the actual backend, i.e. use
 *	 your particular SpW interface or just redirect RX/TX to files
 *	 or via a network connection.
 *
 * @note We don't have to serve more than one RDCU at any given time, so we
 *	 track addresses and paths internally in a single instance. This also
 *	 makes the interface less cluttered. Besides, I'm lazy.
 *
 *
 * @warning when operational, we expect to have exclusive control of the SpW link
 *
 * TODO: RMAP command response evaluation
 */


#include <stdlib.h>
#include <string.h>

#include "../common/byteorder.h"
#include "../common/cmp_debug.h"
#include "rmap.h"
#include "rdcu_rmap.h"

#define RDCU_CONFIG_DEBUG 0

static uint8_t rdcu_addr;
static uint8_t icu_addr;

static uint8_t *dpath;	/* destination path (to the RDCU) */
static uint8_t *rpath;	/* return path (to the ICU) */
static uint8_t dpath_len;
static uint8_t rpath_len;

static uint8_t dst_key;	/* destination command key */




 /* generic calls, functions must be provided to init() */
static int32_t (*rmap_tx)(const void *hdr,  uint32_t hdr_size,
			  const uint8_t non_crc_bytes,
			  const void *data, uint32_t data_size);
static uint32_t (*rmap_rx)(uint8_t *pkt);

static uint32_t data_mtu;	/* maximum data transfer size per unit */










/* For now we maintain a simple transaction log that works like this:
 * we allow up to 128 transfers, simply because this is how many response
 * packets the GRSPW2 SpW descriptor table can hold at any one time without
 * blocking the link.
 *
 * Every time a new transfer is to be submitted, we grab the first free
 * slot we encounter in the "in_use" array and use the index as the transaction
 * identifier of the RMAP packet. (Yes, this is potentially slow, as we may have
 * to iterate over the whole array every time if we're very busy with
 * transmissions, so there is room for improvement.)
 *
 * Every time a slot is retrieved, the "pending" counter is incremented to
 * have a fast indicator of the synchronisation status, i.e. if "pending"
 * is not set, the synchronisation procedure is complete and the local data may
 * be read, or the remote data has been written and further commands may be
 * issued.
 *
 * The local (mirror) start address of the requested remote address is stored
 * into the same slot in the "local_addr" array, so we'll know where to put the
 * data if we issue an RMAP_read command. This may be omitted for write
 * commands.
 *
 * Every time a response packet is received, the data (if any) is written to the
 * local address, using the length specified by RMAP packet, so be careful where
 * you place your buffers or registers. On success, the "in_use" slot is cleared
 * and the pending counter is improved.
 *
 * XXX: careful, no locking is used on any of the log data, so this is
 * single-thread-use only!
 *
 */
#define TRANS_LOG_SIZE 64	/* GRSPW2 TX descriptor limit */
static struct {

	uint8_t  in_use[TRANS_LOG_SIZE];
	void    *local_addr[TRANS_LOG_SIZE];

	int pending;
} trans_log;


/**
 * @brief grab a slot in the transaction log
 *
 * @param local_addr the local memory address
 *
 * @returns -1 on no slots, >= 0 for the transaction id
 */

static int trans_log_grab_slot(void *local_addr)
{
	int i;
	int slot = -1;

	for (i = 0; i < TRANS_LOG_SIZE; i++) {

		if (trans_log.in_use[i])
			continue;

		/* got one */
		slot = i;
		trans_log.in_use[slot] = 1;
		trans_log.local_addr[slot] = local_addr;
		trans_log.pending++;
		break;
	}

	return slot;
}


/**
 * @brief release a slot in the transaction log
 *
 * @param slot the id of the slot
 *
 */

static void trans_log_release_slot(int slot)
{

	if (slot < 0)
		return;

	if (slot >= TRANS_LOG_SIZE)
		return;

	if (!trans_log.in_use[slot])
		return;

	trans_log.in_use[slot] = 0;
	trans_log.pending--;
}


/**
 * @brief get the local address for a slot
 *
 * @param slot the id of the slot
 *
 * @returns the address or NULL if not found/slot not in use
 */

static void *trans_log_get_addr(int slot)
{
	if (slot < 0)
		return NULL;

	if (slot >= TRANS_LOG_SIZE)
		return NULL;

	if (!trans_log.in_use[slot])
		return NULL;

	return trans_log.local_addr[slot];
}

/**
 * @brief n rmap command transaction
 *
 * @returns number of packets processed or < 0 on error
 */

static int rdcu_process_rx(void)
{
	uint32_t n;
	int cnt = 0;

	uint32_t *local_addr;

	uint8_t *spw_pckt;

	struct rmap_pkt *rp;


	if (!rmap_rx)
		return -1;

	/* process all pending responses */
	while ((n = rmap_rx(NULL))) {
		/* we received something, allocate enough space for the packet */
		spw_pckt = (uint8_t *) malloc(n);
		if (!spw_pckt) {
			debug_print("malloc() for packet failed!");
			return -1;
		}

		/* read the packet */
		n = rmap_rx(spw_pckt);

		if (!n) {
			debug_print("Unknown error in rmap_rx()");
			free(spw_pckt);
			return -1;
		}

		cnt++;

		if (RDCU_CONFIG_DEBUG)
			rmap_parse_pkt(spw_pckt);

		/* convert format */
		rp = rmap_pkt_from_buffer(spw_pckt, n);
		free(spw_pckt);

		if (!rp) {
			debug_print("Error converting to RMAP packet");
			continue;
		}

		local_addr = trans_log_get_addr(rp->tr_id);

		if (!local_addr) {
			debug_print("Warning: response packet received not in transaction log");
			rmap_erase_packet(rp);
			continue;
		}

		if (rp->data_len & 0x3) {
			debug_print("Error: response packet data size is not a multiple of 4, transaction dropped");
			trans_log_release_slot(rp->tr_id);
			rmap_erase_packet(rp);
			return -1;
		}

		if (rp->data_len) {
			uint8_t crc8;

			/* convert endianness in-place if needed */
#ifdef __LITTLE_ENDIAN
			{
				uint32_t i, tmp;

				for (i = 0; i < rp->data_len; i += sizeof(tmp)) {
					memcpy(&tmp, &rp->data[i], sizeof(tmp));
					be32_to_cpus(&tmp);
					memcpy(&rp->data[i], &tmp, sizeof(tmp));
				}
			}
#endif /* __LITTLE_ENDIAN */

			crc8 = rmap_crc8(rp->data, rp->data_len);
			if (crc8 != rp->data_crc) {
				debug_print("Error: data CRC8 mismatch, data invalid or packet truncated. Transaction dropped");

				trans_log_release_slot(rp->tr_id);
				rmap_erase_packet(rp);
				return -1;
			}

			memcpy(local_addr, rp->data, rp->data_len);
		}


		trans_log_release_slot(rp->tr_id);
		rmap_erase_packet(rp);
	}

	return cnt;
}


/**
 * @brief submit an rmap command transaction
 *
 * @param cmd the rmap command
 * @param cmd_size the size of the rmap command
 * @param data the payload (may be NULL)
 * @param data_size the size of the payload
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_submit_tx(const uint8_t *cmd,  uint32_t cmd_size,
		   const uint8_t *data, uint32_t data_size)
{
	/* try to process pending responses */
	rdcu_process_rx();

	if (!rmap_tx)
		return -1;

	if (RDCU_CONFIG_DEBUG)
		debug_print("Transmitting RMAP command");

	if (rmap_tx(cmd, cmd_size, dpath_len, data, data_size)) {
		debug_print("rmap_tx() returned error!");
		return -1;
	}

	return 0;
}


/**
 * @brief generate an rmap command packet
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @param rmap_cmd_type the rmap command type of the packet
 *
 * @param addr the address to read from or write to
 *
 * @param size the number of bytes to read or write
 *
 * @returns the size of the command data buffer or 0 on error
 */

int rdcu_gen_cmd(uint16_t trans_id, uint8_t *cmd,
		 uint8_t rmap_cmd_type,
		 uint32_t addr, uint32_t size)
{
	int n;

	struct rmap_pkt *pkt;

	pkt = rmap_create_packet();
	if (!pkt) {
		debug_print("Error creating packet");
		return 0;
	}

	rmap_set_dst(pkt, rdcu_addr);
	rmap_set_src(pkt, icu_addr);
	rmap_set_dest_path(pkt, dpath, dpath_len);
	rmap_set_reply_path(pkt, rpath, rpath_len);
	rmap_set_key(pkt, dst_key);
	rmap_set_cmd(pkt, rmap_cmd_type);
	rmap_set_tr_id(pkt, trans_id);
	rmap_set_data_addr(pkt, addr);
	rmap_set_data_len(pkt, size);

	/* determine header size */
	n = rmap_build_hdr(pkt, NULL);

	if (!cmd || n <= 0) {
		rmap_erase_packet(pkt);
		return n;
	}

	memset(cmd, 0, (size_t)n);  /* clear command buffer */

	n = rmap_build_hdr(pkt, cmd);

	rmap_erase_packet(pkt);

	return n;
}





/**
 * @brief submit a sync command
 *
 * @param fn the RDCU command generation function
 * @param addr the local address of the corresponding remote address
 * @param data_len the length of the data payload (0 for read commands)
 *
 * @note data_len must be a multiple of 4
 * @note all data is treated (and byte swapped) as 32 bit words
 *
 * @return 0 on success, otherwise error
 */


int rdcu_sync(int (*fn)(uint16_t trans_id, uint8_t *cmd),
	      void *addr, uint32_t data_len)
{
	int n;
	int slot;

	uint8_t *rmap_cmd;


	if (data_len & 0x3)
		return -1;

	slot = trans_log_grab_slot(addr);
	if (slot < 0 || slot > UINT16_MAX)
		return -1;


	/* determine size of command */
	n = fn((uint16_t)slot, NULL);
	if (n <= 0) {
		debug_print("Error creating command packet");
		return -1;
	}

	rmap_cmd = (uint8_t *)malloc((size_t)n);
	if (!rmap_cmd) {
		debug_print("Error allocating rmap cmd");
		return -1;
	}

	/* now fill actual command */
	n = fn((uint16_t)slot, rmap_cmd);
	if (n <= 0) {
		debug_print("Error creating command packet");
		free(rmap_cmd);
		return -1;
	}

	/* convert endianness if needed */
#ifdef __LITTLE_ENDIAN
	if (data_len) {
		uint32_t i;
		uint32_t *tmp_buf = alloca(data_len);
		uint32_t *p = (uint32_t *)addr;

		for (i = 0; i < (data_len / 4); i++)
			tmp_buf[i] = cpu_to_be32(p[i]);

		addr = tmp_buf;
	}
#endif /* __LITTLE_ENDIAN */

	n = rdcu_submit_tx(rmap_cmd, (uint32_t)n, addr, data_len);
	free(rmap_cmd);

	return n;
}



/**
 * @brief submit a data sync command
 *
 * @param fn an RDCU data transfer generation function
 * @param addr the remote address
 * @param data the local data address
 * @param data_len the length of the data payload
 * @param read 0: write, otherwise read
 *
 * @return 0 on success, < 0: error, > 0: retry
 *
 * @note this one is a little redundant, but otherwise we'd have a lot of
 *	 unused parameters on most of the control functions
 *
 * XXX need a parameter for read...meh...must think of something else
 */


int rdcu_sync_data(int (*fn)(uint16_t trans_id, uint8_t *cmd,
			     uint32_t addr, uint32_t data_len),
		   uint32_t addr, void *data, uint32_t data_len, int read)
{
	int n;
	int slot;

	uint8_t *rmap_cmd;

	rdcu_process_rx();

	slot = trans_log_grab_slot(data);
	if (slot < 0 || slot > UINT16_MAX) {
		if (RDCU_CONFIG_DEBUG)
			debug_print("Error: all slots busy!");
		return 1;
	}


	/* determine size of command */
	n = fn((uint16_t)slot, NULL, addr, data_len);
	if (n <= 0) {
		debug_print("Error creating command packet");
		return -1;
	}


	rmap_cmd = (uint8_t *)malloc((size_t)n);
	if (!rmap_cmd) {
		debug_print("Error allocating rmap cmd");
		return -1;
	}

	/* now fill actual command */
	n = fn((uint16_t)slot, rmap_cmd, addr, data_len);
	if (n <= 0) {
		debug_print("Error creating command packet");
		free(rmap_cmd);
		return -1;
	}

	if (read)
		n = rdcu_submit_tx(rmap_cmd, (uint32_t)n, NULL, 0);
	else
		n = rdcu_submit_tx(rmap_cmd, (uint32_t)n, data, data_len);

	free(rmap_cmd);

	return n;
}



/**
 * @brief create a complete package from header and payload data including CRC8
 *
 * @note this is a helper function to generate complete binary RMAP packet dumps
 *
 * @param blob the blob buffer; if NULL, the function returns the needed size
 *
 * @param[in]  cmd an rmap command buffer
 * @param[in]  cmd_size the size of the rmap command buffer
 * @param[in]  non_crc_bytes leading bytes in the header not path of the CRC
 * @param[in]  data a data buffer (may be NULL)
 * @param[in]  data_size the size of the data buffer (ignored if data is NULL)
 *
 * @note data_size must be a multiple of 4
 * @note this function will convert all data to big endian as 32 bit words
 *
 * @returns the size of the blob or 0 on error
 */

uint32_t rdcu_package(uint8_t *blob,
		      const uint8_t *cmd, uint32_t cmd_size,
		      const uint8_t non_crc_bytes,
		      const uint8_t *data, uint32_t data_size)
{
	uint32_t n;
	int has_data_crc = 0;
	const struct rmap_instruction *ri;


	if (data_size & 0x3)	/* must be multiple of 4 */
		return 0;

	if (!data_size)
		data = NULL;

	if (!cmd_size) {
		blob = NULL;
		return 0;
	}


	/* allocate space for header, header crc, data, data crc */
	n = cmd_size + 1;

	ri = (const struct rmap_instruction *)&cmd[non_crc_bytes + RMAP_INSTRUCTION];

	/* see if the type of command needs a data crc field at the end */
	switch (ri->cmd) {
	case RMAP_READ_MODIFY_WRITE_ADDR_INC:
	case RMAP_WRITE_ADDR_SINGLE:
	case RMAP_WRITE_ADDR_INC:
	case RMAP_WRITE_ADDR_SINGLE_VERIFY:
	case RMAP_WRITE_ADDR_INC_VERIFY:
	case RMAP_WRITE_ADDR_SINGLE_VERIFY_REPLY:
	case RMAP_WRITE_ADDR_INC_VERIFY_REPLY:
	case RMAP_WRITE_ADDR_INC_REPLY:
		has_data_crc = 1;
		n += 1;
		break;
	default:
		break;
	}

	if (data)
		n += data_size;

	if (!blob)
		return n;

	memcpy(&blob[0], cmd, cmd_size);

	blob[cmd_size] = rmap_crc8(&cmd[non_crc_bytes],
				   cmd_size - non_crc_bytes);

	if (data) {
		memcpy(&blob[cmd_size + 1], data, data_size);
		blob[cmd_size + 1 + data_size] = rmap_crc8(&blob[cmd_size + 1], data_size);
	} else {
		/* if no data is present, data crc is 0x0 */
		if (has_data_crc)
			blob[cmd_size + 1] = 0x0;
	}

	return n;
}


/**
 * @brief sets the logical address of the RDCU
 * @param addr the address
 */

void rdcu_set_destination_logical_address(uint8_t addr)
{
	rdcu_addr = addr;
}

/**
 * @brief sets the logical address of the ICU
 * @param addr the address
 */

void rdcu_set_source_logical_address(uint8_t addr)
{
	icu_addr = addr;
}


/**
 * @brief set the destination path to the RDCU
 * @param path a byte array containing the path (may be NULL)
 * @param len the number of elements in the array
 *
 * @returns 0 on success, otherwise error
 *
 * @note the path array is taken as a reference, make sure to keep it around
 *	 the maximum length of the path is 15 elements
 *	 setting either path NULL or len 0 disables destination path addressing
 */

int rdcu_set_destination_path(uint8_t *path, uint8_t len)
{
	if (len > RMAP_MAX_PATH_LEN)
		return -1;

	if (!path || !len) {
		dpath     = NULL;
		dpath_len = 0;
		return 0;
	}

	dpath     = path;
	dpath_len = len;

	return 0;
}


/**
 * @brief set the return path to the ICU
 * @param path a byte array containing the path (may be NULL)
 * @param len the number of elements in the array
 *
 * @returns 0 on success, otherwise error
 *
 * @note the path array is taken as a reference, make sure to keep it around
 *	 the maximum length of the path is 12 elements
 *	 the number of elements must be a multiple of 4 (due to RMAP protocol)
 *	 setting either path NULL or len 0 disables return path addressing
 */

int rdcu_set_return_path(uint8_t *path, uint8_t len)
{
	if (len > RMAP_MAX_REPLY_PATH_LEN)
		return -1;

	if (len & 0x3)
		return -1;	/* not a multiple of 4 */

	if (!path || !len) {
		rpath     = NULL;
		rpath_len = 0;
		return 0;
	}

	rpath     = path;
	rpath_len = len;

	return 0;
}


/**
 * @brief set the destination command key to use
 *
 * @param key the destination key
 */

void rdcu_set_destination_key(uint8_t key)
{
	dst_key = key;
}


/**
 * @brief get the configured data MTU
 *
 * @returns the mtu
 */

uint32_t rdcu_get_data_mtu(void)
{
	return data_mtu;
}


/**
 * @brief get the RDCU <-> ICU mirror RMAP synchronisation status
 *
 * @returns 0: synchronised, > 0: operations pending
 */

int rdcu_rmap_sync_status(void)
{
	/* try to process pending responses */
	rdcu_process_rx();

	return trans_log.pending;
}


/**
 * @brief reset all entries in the RMAP transaction log
 */

void rdcu_rmap_reset_log(void)
{
	memset(trans_log.in_use, 0, sizeof(trans_log.in_use));  /* clear in_use buffer */
	trans_log.pending = 0;
}


/**
 * @brief initialise the rdcu control library
 *
 * @param mtu the maximum data transfer size per unit
 *
 * @param tx a function pointer to transmit an rmap command
 * @param rx function pointer to receive an rmap command
 *
 * @note tx is expected to return 0 on success
 *	 rmap_rx is expected to return the number of packet bytes
 *
 * @returns 0 on success, otherwise error
 */

int rdcu_rmap_init(uint32_t mtu,
		   int32_t (*tx)(const void *hdr,  uint32_t hdr_size,
				 const uint8_t non_crc_bytes,
				 const void *data, uint32_t data_size),
		   uint32_t (*rx)(uint8_t *pkt))
{
	if (!tx)
		return -1;

	if (!rx)
		return -1;

	rmap_tx = tx;
	rmap_rx = rx;

	data_mtu = mtu;

	return 0;
}

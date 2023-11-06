/**
 * @file   rdcu_rmap.h
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
 * @brief RMAP RDCU link interface header file
 */

#ifndef RDCU_RMAP_H
#define RDCU_RMAP_H

#include <stdint.h>



int rdcu_submit_tx(const uint8_t *cmd,  uint32_t cmd_size,
		   const uint8_t *data, uint32_t data_size);


int rdcu_gen_cmd(uint16_t trans_id, uint8_t *cmd,
		 uint8_t rmap_cmd_type,
		 uint32_t addr, uint32_t size);

int rdcu_sync(int (*fn)(uint16_t trans_id, uint8_t *cmd),
	      void *addr, uint32_t data_len);

int rdcu_sync_data(int (*fn)(uint16_t trans_id, uint8_t *cmd,
			     uint32_t addr, uint32_t data_len),
		   uint32_t addr, void *data, uint32_t data_len, int read);

uint32_t rdcu_package(uint8_t *blob,
		      const uint8_t *cmd, uint32_t cmd_size,
		      const uint8_t non_crc_bytes,
		      const uint8_t *data, uint32_t data_size);

void rdcu_set_destination_logical_address(uint8_t addr);

int rdcu_set_destination_path(uint8_t *path, uint8_t len);
int rdcu_set_return_path(uint8_t *path, uint8_t len);
void rdcu_set_source_logical_address(uint8_t addr);
void rdcu_set_destination_key(uint8_t key);
uint32_t rdcu_get_data_mtu(void);

int rdcu_rmap_sync_status(void);

void rdcu_rmap_reset_log(void);

int rdcu_rmap_init(uint32_t mtu,
		   int32_t (*tx)(const void *hdr,  uint32_t hdr_size,
				 const uint8_t non_crc_bytes,
				 const void *data, uint32_t data_size),
		   uint32_t (*rx)(uint8_t *pkt));



#endif /* RDCU_RMAP_H */

/**
 * @file   cmp_rdcu.h
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2019
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
 * @brief hardware compressor control library
 * @see Data Compression User Manual PLATO-UVIE-PL-UM-0001
 */


#ifndef _CMP_RDCU_H_
#define _CMP_RDCU_H_

#include "common/cmp_support.h"
#include "rdcu_compress/cmp_rdcu_cfg.h"


/* Compression Error Register bits definition, see RDCU-FRS-FN-0952 */
#define SMALL_BUFFER_ERR_BIT	0 /* The length for the compressed data buffer is too small */
#define CMP_MODE_ERR_BIT	1 /* The cmp_mode parameter is not set correctly */
#define MODEL_VALUE_ERR_BIT	2 /* The model_value parameter is not set correctly */
#define CMP_PAR_ERR_BIT		3 /* The spill, golomb_par combination is not set correctly */
#define AP1_CMP_PAR_ERR_BIT	4 /* The ap1_spill, ap1_golomb_par combination is not set correctly (only HW compression) */
#define AP2_CMP_PAR_ERR_BIT	5 /* The ap2_spill, ap2_golomb_par combination is not set correctly (only HW compression) */
#define MB_ERR_BIT		6 /* Multi bit error detected by the memory controller (only HW compression) */
#define SLAVE_BUSY_ERR_BIT	7 /* The bus master has received the "slave busy" status (only HW compression) */
#define SLAVE_BLOCKED_ERR_BIT	8 /* The bus master has received the “slave blocked” status */
#define INVALID_ADDRESS_ERR_BIT	9 /* The bus master has received the “invalid address” status */


int rdcu_compress_data(const struct cmp_cfg *cfg);

int rdcu_read_cmp_status(struct cmp_status *status);

int rdcu_read_cmp_info(struct cmp_info *info);

int rdcu_read_cmp_bitstream(const struct cmp_info *info, void *compressed_data);

int rdcu_read_model(const struct cmp_info *info, void *updated_model);

int rdcu_interrupt_compression(void);

void rdcu_enable_interrput_signal(void);
void rdcu_disable_interrput_signal(void);

#endif /* _CMP_RDCU_H_ */

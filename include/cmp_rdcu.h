/**
 * @file   cmp_rdcu.h
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at),
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

#include <cmp_support.h>


/* Compression Error Register bits definition, see RDCU-FRS-FN-0952 */
#define SMALL_BUFFER_ERR_BIT	0x00 /* The length for the compressed data buffer is too small */
#define CMP_MODE_ERR_BIT	0x01 /* The cmp_mode parameter is not set correctly */
#define MODEL_VALUE_ERR_BIT	0x02 /* The model_value parameter is not set correctly */
#define CMP_PAR_ERR_BIT		0x03 /* The spill, golomb_par combination is not set correctly */
#define AP1_CMP_PAR_ERR_BIT	0x04 /* The ap1_spill, ap1_golomb_par combination is not set correctly (only HW compression) */
#define AP2_CMP_PAR_ERR_BIT	0x05 /* The ap2_spill, ap2_golomb_par combination is not set correctly (only HW compression) */
#define MB_ERR_BIT		0x06 /* Multi bit error detected by the memory controller (only HW compression) */
#define SLAVE_BUSY_ERR_BIT	0x07 /* The bus master has received the "slave busy" status (only HW compression) */


struct cmp_cfg rdcu_cfg_create(enum cmp_data_type data_type, enum cmp_mode cmp_mode,
			       uint32_t model_value, uint32_t lossy_par);
int rdcu_cfg_buffers(struct cmp_cfg *cfg, uint16_t *data_to_compress,
		     uint32_t data_samples, uint16_t *model_of_data,
		     uint32_t rdcu_data_adr, uint32_t rdcu_model_adr,
		     uint32_t rdcu_new_model_adr, uint32_t rdcu_buffer_adr,
		     uint32_t rdcu_buffer_lenght);
int rdcu_cfg_imagette(struct cmp_cfg *cfg,
		      uint32_t golomb_par, uint32_t spillover_par,
		      uint32_t ap1_golomb_par, uint32_t ap1_spillover_par,
		      uint32_t ap2_golomb_par, uint32_t ap2_spillover_par);

int rdcu_cmp_cfg_is_invalid(const struct cmp_cfg *cfg);

int rdcu_compress_data(const struct cmp_cfg *cfg);

int rdcu_read_cmp_status(struct cmp_status *status);

int rdcu_read_cmp_info(struct cmp_info *info);

int rdcu_read_cmp_bitstream(const struct cmp_info *info, void *output_buf);

int rdcu_read_model(const struct cmp_info *info, void *model_buf);

int rdcu_interrupt_compression(void);

void rdcu_enable_interrput_signal(void);
void rdcu_disable_interrput_signal(void);

#endif /* _CMP_RDCU_H_ */

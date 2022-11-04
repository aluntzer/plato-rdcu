/**
 * @file   cmp_icu.h
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at),
 * @date   2020
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
 * @brief software compression library
 * @see Data Compression User Manual PLATO-UVIE-PL-UM-0001
 */

#ifndef _CMP_ICU_H_
#define _CMP_ICU_H_

#include <cmp_support.h>

#define CMP_PAR_UNUSED 0

/* create and setup a compression configuration */
struct cmp_cfg cmp_cfg_icu_create(enum cmp_data_type data_type, enum cmp_mode cmp_mode,
				  uint32_t model_value, uint32_t lossy_par);

uint32_t cmp_cfg_icu_buffers(struct cmp_cfg *cfg, void *data_to_compress,
			     uint32_t data_samples, void *model_of_data,
			     void *updated_model, uint32_t *compressed_data,
			     uint32_t compressed_data_len_samples);

int cmp_cfg_icu_imagette(struct cmp_cfg *cfg, uint32_t cmp_par,
			 uint32_t spillover_par);

int cmp_cfg_fx_cob(struct cmp_cfg *cfg,
		   uint32_t cmp_par_exp_flags, uint32_t spillover_exp_flags,
		   uint32_t cmp_par_fx, uint32_t spillover_fx,
		   uint32_t cmp_par_ncob, uint32_t spillover_ncob,
		   uint32_t cmp_par_efx, uint32_t spillover_efx,
		   uint32_t cmp_par_ecob, uint32_t spillover_ecob,
		   uint32_t cmp_par_fx_cob_variance, uint32_t spillover_fx_cob_variance);

int cmp_cfg_aux(struct cmp_cfg *cfg,
		uint32_t cmp_par_mean, uint32_t spillover_mean,
		uint32_t cmp_par_variance, uint32_t spillover_variance,
		uint32_t cmp_par_pixels_error, uint32_t spillover_pixels_error);


/* start the compression */
int icu_compress_data(const struct cmp_cfg *cfg);

#endif /* _CMP_ICU_H_ */

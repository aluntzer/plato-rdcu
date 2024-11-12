/**
 * @file   cmp_max_used_bits.h
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
 * @brief definition and consents of the cmp_max_used_bits structure
 */

#ifndef CMP_MAX_USED_BITS_H
#define CMP_MAX_USED_BITS_H

#include <stdint.h>

#include "cmp_data_types.h"


/**
 * @brief macro to calculate the bit size of a member in a structure.
 *
 * @param type		type of the structure
 * @param member	member of the structure
 *
 * @returns the size of the member in bits
 */

#define member_bit_size(type, member) (sizeof(((type *)0)->member)*8)


/**
 * @brief Structure holding the maximum length of the different data product
 *	types in bits
 */

struct cmp_max_used_bits {
	unsigned int s_exp_flags;
	unsigned int s_fx;
	unsigned int s_efx;
	unsigned int s_ncob; /* s_ncob_x and s_ncob_y */
	unsigned int s_ecob; /* s_ecob_x and s_ncob_y */
	unsigned int f_fx;
	unsigned int f_efx;
	unsigned int f_ncob; /* f_ncob_x and f_ncob_y */
	unsigned int f_ecob; /* f_ecob_x and f_ncob_y */
	unsigned int l_exp_flags;
	unsigned int l_fx;
	unsigned int l_fx_cob_variance; /* l_fx_variance, l_cob_x_variance and l_cob_y_variance */
	unsigned int l_efx;
	unsigned int l_ncob; /* l_ncob_x and l_ncob_y */
	unsigned int l_ecob; /* l_ecob_x and l_ncob_y */
	unsigned int nc_imagette;
	unsigned int saturated_imagette;
	unsigned int nc_offset_mean;
	unsigned int nc_offset_variance;
	unsigned int nc_background_mean;
	unsigned int nc_background_variance;
	unsigned int nc_background_outlier_pixels;
	unsigned int smearing_mean;
	unsigned int smearing_variance_mean;
	unsigned int smearing_outlier_pixels;
	unsigned int fc_imagette;
	unsigned int fc_offset_mean;
	unsigned int fc_offset_variance;
	unsigned int fc_background_mean;
	unsigned int fc_background_variance;
	unsigned int fc_background_outlier_pixels;
};


/**
 * @brief This structure is used to define the maximum number of bits required
 *	to read different data product type fields
 */

static const struct cmp_max_used_bits MAX_USED_BITS = {
	member_bit_size(struct s_fx_efx_ncob_ecob, exp_flags), /* s_fx_exp_flags */
	member_bit_size(struct s_fx_efx_ncob_ecob, fx), /* s_fx */
	member_bit_size(struct s_fx_efx_ncob_ecob, efx), /* s_efx */
	member_bit_size(struct s_fx_efx_ncob_ecob, ncob_x), /* s_ncob_x and s_ncob_y */
	member_bit_size(struct s_fx_efx_ncob_ecob, ecob_x), /* s_ecob_x and s_ncob_y */
	member_bit_size(struct f_fx_efx_ncob_ecob, fx), /* f_fx */
	member_bit_size(struct f_fx_efx_ncob_ecob, efx), /* f_efx */
	member_bit_size(struct f_fx_efx_ncob_ecob, ncob_x), /* f_ncob_x and f_ncob_y */
	member_bit_size(struct f_fx_efx_ncob_ecob, ecob_x), /* f_ecob_x and f_ncob_y */
	24, /* member_bit_size(struct l_fx_efx_ncob_ecob, exp_flags), /1* l_fx_exp_flags *1/ */
	member_bit_size(struct l_fx_efx_ncob_ecob, fx), /* l_fx */
	member_bit_size(struct l_fx_efx_ncob_ecob, fx_variance), /* l_fx_cob_variance */
	member_bit_size(struct l_fx_efx_ncob_ecob, efx), /* l_efx */
	member_bit_size(struct l_fx_efx_ncob_ecob, ncob_x), /* l_ncob_x and l_ncob_y */
	member_bit_size(struct l_fx_efx_ncob_ecob, ecob_x), /* l_ecob_x and l_ncob_y */
	sizeof(uint16_t)*8, /* nc_imagette */
	sizeof(uint16_t)*8, /* saturated_imagette */
	member_bit_size(struct offset, mean), /* nc_offset_mean */
	member_bit_size(struct offset, variance), /* nc_offset_variance */
	member_bit_size(struct background, mean), /* nc_background_mean */
	member_bit_size(struct background, variance), /* nc_background_variance */
	member_bit_size(struct background, outlier_pixels), /* nc_background_outlier_pixels */
	member_bit_size(struct smearing, mean), /* smearing_mean */
	member_bit_size(struct smearing, variance_mean), /* smearing_variance_mean */
	member_bit_size(struct smearing, outlier_pixels), /* smearing_outlier_pixels */
	sizeof(uint16_t)*8, /* fc_imagette */
	member_bit_size(struct offset, mean), /* fc_offset_mean */
	member_bit_size(struct offset, variance), /* fc_offset_variance */
	member_bit_size(struct background, mean), /* fc_background_mean */
	member_bit_size(struct background, variance), /* fc_background_variance */
	member_bit_size(struct background, outlier_pixels), /* fc_background_outlier_pixels */
};


#endif /* CMP_MAX_USED_BITS_H */

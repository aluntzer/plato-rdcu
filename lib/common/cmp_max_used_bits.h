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


/* Up to this number (not included), the maximum used bits registry versions cannot be used by the user. */
#define CMP_MAX_USED_BITS_RESERVED_VERSIONS 32


/* predefined maximum used bits registry constants */
extern const struct cmp_max_used_bits MAX_USED_BITS_SAFE;
extern const struct cmp_max_used_bits MAX_USED_BITS_V1;
#define MAX_USED_BITS MAX_USED_BITS_SAFE


/**
 * @brief Structure holding the maximum length of the different data product types in bits
 */

struct cmp_max_used_bits {
	uint8_t version;
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
	unsigned int l_fx_variance;
	unsigned int l_efx;
	unsigned int l_ncob; /* l_ncob_x and l_ncob_y */
	unsigned int l_ecob; /* l_ecob_x and l_ncob_y */
	unsigned int l_cob_variance; /* l_cob_x_variance and l_cob_y_variance */
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

#endif /* CMP_MAX_USED_BITS_H */

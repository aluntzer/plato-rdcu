/**
 * @file   cmp_max_used_bits.c
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


#include <stdint.h>

#include "cmp_data_types.h"

#define MAX_USED_NC_IMAGETTE_BITS		16
#define MAX_USED_SATURATED_IMAGETTE_BITS	16 /* TBC */
#define MAX_USED_FC_IMAGETTE_BITS		16 /* TBC */

#define MAX_USED_F_FX_BITS	21 /* max exp. int value: (1.078*10^5)/0.1 = 1,078,000 -> 21 bits */
#define MAX_USED_F_EFX_BITS	MAX_USED_F_FX_BITS /* we use the same as f_fx */
#define MAX_USED_F_NCOB_BITS	20 /* max exp. int value: 6/10^−5 = 6*10^5 -> 20 bits */
#define MAX_USED_F_ECOB_BITS	32 /* TBC */

#define MAX_USED_S_FX_EXPOSURE_FLAGS_BITS	2 /* 2 flags + 6 spare bits */
#define MAX_USED_S_FX_BITS			24 /* max exp. int value: (1.078*10^5-34.71)/0.01 = 10,780,000-> 24 bits */
#define MAX_USED_S_EFX_BITS			MAX_USED_S_FX_BITS /* we use the same as s_fx */
#define MAX_USED_S_NCOB_BITS			MAX_USED_F_NCOB_BITS
#define MAX_USED_S_ECOB_BITS			32 /* TBC */

#define MAX_USED_L_FX_EXPOSURE_FLAGS_BITS	24 /* 24 flags */
#define MAX_USED_L_FX_BITS			MAX_USED_S_FX_BITS
#define MAX_USED_L_FX_VARIANCE_BITS		32 /* no maximum value is given in PLATO-LESIA-PDC-TN-0054 */
#define MAX_USED_L_EFX_BITS			MAX_USED_L_FX_BITS /* we use the same as l_fx */
#define MAX_USED_L_NCOB_BITS			MAX_USED_F_NCOB_BITS
#define MAX_USED_L_ECOB_BITS			32 /* TBC */
#define MAX_USED_L_COB_VARIANCE_BITS		25 /* max exp int value: 0.1739/10^−8 = 17390000 -> 25 bits */

#define MAX_USED_NC_OFFSET_MEAN_BITS		2 /* no maximum value is given in PLATO-LESIA-PDC-TN-0054 */
#define MAX_USED_NC_OFFSET_VARIANCE_BITS	10 /* max exp. int value: 9.31/0.01 = 931 -> 10 bits */

#define MAX_USED_NC_BACKGROUND_MEAN_BITS		16 /* max exp. int value: (391.8-(-50))/0.01 = 44,180 -> 16 bits */
#define MAX_USED_NC_BACKGROUND_VARIANCE_BITS		16 /* max exp. int value: 6471/0.1 = 64710 -> 16 bit */
#define MAX_USED_NC_BACKGROUND_OUTLIER_PIXELS_BITS	5 /* maximum = 16 -> 5 bits */

#define MAX_USED_SMEARING_MEAN_BITS		15 /* max exp. int value: (219.9 - -50)/0.01 = 26.990 */
#define MAX_USED_SMEARING_VARIANCE_MEAN_BITS	16 /* no maximum value is given in PLATO-LESIA-PDC-TN-0054 */
#define MAX_USED_SMEARING_OUTLIER_PIXELS_BITS	11 /* maximum = 1200 -> 11 bits */

#define MAX_USED_FC_OFFSET_MEAN_BITS		32 /* no maximum value is given in PLATO-LESIA-PDC-TN-0054 */
#define MAX_USED_FC_OFFSET_VARIANCE_BITS	9  /* max exp. int value: 342/1 = 342 -> 9 bits */
#define MAX_USED_FC_OFFSET_PIXEL_IN_ERROR_BITS	16 /* TBC */

#define MAX_USED_FC_BACKGROUND_MEAN_BITS		10 /* max exp. int value: (35.76-(-50))/0.1 = 858 -> 10 bits*/
#define MAX_USED_FC_BACKGROUND_VARIANCE_BITS		6 /* max exp. int value: 53.9/1 = 54 -> 6 bits */
#define MAX_USED_FC_BACKGROUND_OUTLIER_PIXELS_BITS	16 /* TBC */


#define member_bit_size(type, member) (sizeof(((type *)0)->member)*8)


/* a safe the different data products types in bits */
const struct cmp_max_used_bits MAX_USED_BITS_SAFE = {
	0, /* default version */
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
	member_bit_size(struct l_fx_efx_ncob_ecob, fx_variance), /* l_fx_variance */
	member_bit_size(struct l_fx_efx_ncob_ecob, efx), /* l_efx */
	member_bit_size(struct l_fx_efx_ncob_ecob, ncob_x), /* l_ncob_x and l_ncob_y */
	member_bit_size(struct l_fx_efx_ncob_ecob, ecob_x), /* l_ecob_x and l_ncob_y */
	member_bit_size(struct l_fx_efx_ncob_ecob, cob_x_variance), /* l_cob_x_variance and l_cob_y_variance */
	sizeof(uint16_t)*8, /* nc_imagette */
	sizeof(uint16_t)*8, /* saturated_imagette */
	member_bit_size(struct nc_offset, mean), /* nc_offset_mean */
	member_bit_size(struct nc_offset, variance), /* nc_offset_variance */
	member_bit_size(struct nc_background, mean), /* nc_background_mean */
	member_bit_size(struct nc_background, variance), /* nc_background_variance */
	member_bit_size(struct nc_background, outlier_pixels), /* nc_background_outlier_pixels */
	member_bit_size(struct smearing, mean), /* smearing_mean */
	member_bit_size(struct smearing, variance_mean), /* smearing_variance_mean */
	member_bit_size(struct smearing, outlier_pixels), /* smearing_outlier_pixels */
	sizeof(uint16_t)*8, /* TBC */ /* fc_imagette */
	sizeof(uint32_t)*8, /* TBC TODO: update */ /* fc_offset_mean */
	sizeof(uint32_t)*8, /* TBC TODO: update */ /* fc_offset_variance */
	sizeof(uint32_t)*8, /* TBC TODO: update */ /* fc_offset_pixel_in_error */
	sizeof(uint32_t)*8, /* TBC TODO: update */ /* fc_background_mean */
	sizeof(uint32_t)*8, /* TBC TODO: update */ /* fc_background_variance */
	sizeof(uint32_t)*8, /* TBC TODO: update */ /* fc_background_outlier_pixels */
};


/* the maximum length of the different data products types in bits */
const struct cmp_max_used_bits MAX_USED_BITS_V1 = {
	1, /* default version */
	MAX_USED_S_FX_EXPOSURE_FLAGS_BITS, /* s_fx_exp_flags */
	MAX_USED_S_FX_BITS, /* s_fx */
	MAX_USED_S_EFX_BITS, /* s_efx */
	MAX_USED_S_NCOB_BITS, /* s_ncob_x and s_ncob_y */
	MAX_USED_S_ECOB_BITS, /* s_ecob_x and s_ncob_y */
	MAX_USED_F_FX_BITS, /* f_fx */
	MAX_USED_F_EFX_BITS, /* f_efx */
	MAX_USED_F_NCOB_BITS, /* f_ncob_x and f_ncob_y */
	MAX_USED_F_ECOB_BITS, /* f_ecob_x and f_ncob_y */
	MAX_USED_L_FX_EXPOSURE_FLAGS_BITS, /* l_fx_exp_flags */
	MAX_USED_L_FX_BITS, /* l_fx */
	MAX_USED_L_FX_VARIANCE_BITS, /* l_fx_variance */
	MAX_USED_L_EFX_BITS, /* l_efx */
	MAX_USED_L_NCOB_BITS, /* l_ncob_x and l_ncob_y */
	MAX_USED_L_ECOB_BITS, /* l_ecob_x and l_ncob_y */
	MAX_USED_L_COB_VARIANCE_BITS, /* l_cob_x_variance and l_cob_y_variance */
	MAX_USED_NC_IMAGETTE_BITS, /* nc_imagette */
	MAX_USED_SATURATED_IMAGETTE_BITS, /* saturated_imagette */
	MAX_USED_NC_OFFSET_MEAN_BITS, /* nc_offset_mean */
	MAX_USED_NC_OFFSET_VARIANCE_BITS, /* nc_offset_variance */
	MAX_USED_NC_BACKGROUND_MEAN_BITS, /* nc_background_mean */
	MAX_USED_NC_BACKGROUND_VARIANCE_BITS, /* nc_background_variance */
	MAX_USED_NC_BACKGROUND_OUTLIER_PIXELS_BITS, /* nc_background_outlier_pixels */
	MAX_USED_SMEARING_MEAN_BITS, /* smearing_mean */
	MAX_USED_SMEARING_VARIANCE_MEAN_BITS, /* smearing_variance_mean */
	MAX_USED_SMEARING_OUTLIER_PIXELS_BITS, /* smearing_outlier_pixels */
	MAX_USED_FC_IMAGETTE_BITS, /* fc_imagette */
	MAX_USED_FC_OFFSET_MEAN_BITS, /* fc_offset_mean */
	MAX_USED_FC_OFFSET_VARIANCE_BITS, /* fc_offset_variance */
	MAX_USED_FC_OFFSET_PIXEL_IN_ERROR_BITS, /* fc_offset_pixel_in_error */
	MAX_USED_FC_BACKGROUND_MEAN_BITS, /* fc_background_mean */
	MAX_USED_FC_BACKGROUND_VARIANCE_BITS, /* fc_background_variance */
	MAX_USED_FC_BACKGROUND_OUTLIER_PIXELS_BITS /* fc_background_outlier_pixels */
};


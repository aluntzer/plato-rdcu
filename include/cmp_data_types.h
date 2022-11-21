/**
 * @file   cmp_data_types.h
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2020
 * @brief definition of the different compression data types
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
 * @see for N-DPU packed definition: PLATO-LESIA-PL-RP-0031 Issue: 2.9 (N-DPU->ICU data rate)
 * @see for calculation of the max used bits: PLATO-LESIA-PDC-TN-0054 Issue: 1.7
 *
 * Three data rates (for N-DPU):
 * fast cadence (nominally 25s)
 * short cadence (nominally 50s)
 * long cadence (nominally 600s)
 *
 * The science products are identified as this:
 * exp_flags = selected exposure flags
 * fx = normal light flux
 * ncob = normal center of brightness
 * efx = extended light flux
 * ecob = extended center of brightness
 * The prefixes f, s and l stand for fast, short and long cadence
 */

#ifndef CMP_DATA_TYPE_H
#define CMP_DATA_TYPE_H

#include <stdint.h>

#include <compiler.h>
#include <cmp_support.h>


/* size of the source data header structure for multi entry packet */
#define MULTI_ENTRY_HDR_SIZE 12

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
	unsigned int fc_offset_pixel_in_error;
	unsigned int fc_background_mean;
	unsigned int fc_background_variance;
	unsigned int fc_background_outlier_pixels;
};


/**
 * @brief source data header structure for multi entry packet
 * @note a scientific package contains a multi-entry header followed by multiple
 *	entries of the same entry definition
 * @see PLATO-LESIA-PL-RP-0031(N-DPU->ICU data rate)
 */

__extension__
struct multi_entry_hdr {
	uint32_t timestamp_coarse;
	uint16_t timestamp_fine;
	uint16_t configuration_id;
	uint16_t collection_id;
	uint16_t collection_length;
	uint8_t  entry[];
} __attribute__((packed));
compile_time_assert(sizeof(struct multi_entry_hdr) == MULTI_ENTRY_HDR_SIZE, N_DPU_ICU_MULTI_ENTRY_HDR_SIZE_IS_NOT_CORRECT);
compile_time_assert(sizeof(struct multi_entry_hdr) % sizeof(uint32_t) == 0, N_DPU_ICU_MULTI_ENTRY_HDR_NOT_4_BYTE_ALLIED);


/**
 * @brief short cadence normal light flux entry definition
 */

struct s_fx {
	uint8_t exp_flags; /**< selected exposure flags (2 flags + 6 spare bits) */
	uint32_t fx;       /**< normal light flux */
} __attribute__((packed));


/**
 * @brief short cadence normal and extended light flux entry definition
 */

struct s_fx_efx {
	uint8_t exp_flags;
	uint32_t fx;
	uint32_t efx;
} __attribute__((packed));


/**
 * @brief short cadence normal light flux, normal center of brightness entry definition
 */

struct s_fx_ncob {
	uint8_t exp_flags;
	uint32_t fx;
	uint32_t ncob_x;
	uint32_t ncob_y;
} __attribute__((packed));


/**
 * @brief short cadence normal and extended flux, normal and extended center of brightness entry definition
 */

struct s_fx_efx_ncob_ecob {
	uint8_t exp_flags;
	uint32_t fx;
	uint32_t ncob_x;
	uint32_t ncob_y;
	uint32_t efx;
	uint32_t ecob_x;
	uint32_t ecob_y;
} __attribute__((packed));


/**
 * @brief fast cadence normal light flux entry definition
 */

struct f_fx {
	uint32_t fx;
} __attribute__((packed));


/**
 * @brief fast cadence normal and extended light flux entry definition
 */

struct f_fx_efx {
	uint32_t fx;
	uint32_t efx;
} __attribute__((packed));


/**
 * @brief fast cadence normal light flux, normal center of brightness entry definition
 */

struct f_fx_ncob {
	uint32_t fx;
	uint32_t ncob_x;
	uint32_t ncob_y;
} __attribute__((packed));


/**
 * @brief fast cadence normal and extended flux, normal and extended center of
 *	brightness entry definition
 */

struct f_fx_efx_ncob_ecob {
	uint32_t fx;
	uint32_t ncob_x;
	uint32_t ncob_y;
	uint32_t efx;
	uint32_t ecob_x;
	uint32_t ecob_y;
} __attribute__((packed));


/**
 * @brief long cadence normal light flux entry definition
 */

__extension__
struct l_fx {
	uint32_t exp_flags:24; /* selected exposure flags (24 flags) */
	uint32_t fx;
	uint32_t fx_variance;
} __attribute__((packed));


/**
 * @brief long cadence normal and extended light flux entry definition
 */

__extension__
struct l_fx_efx {
	uint32_t exp_flags:24; /* selected exposure flags (24 flags) */
	uint32_t fx;
	uint32_t efx;
	uint32_t fx_variance;
} __attribute__((packed));


/**
 * @brief long cadence normal light flux, normal center of brightness entry definition
 */

__extension__
struct l_fx_ncob {
	uint32_t exp_flags:24; /* selected exposure flags (24 flags) */
	uint32_t fx;
	uint32_t ncob_x;
	uint32_t ncob_y;
	uint32_t fx_variance;
	uint32_t cob_x_variance;
	uint32_t cob_y_variance;
} __attribute__((packed));


/**
 * @brief long cadence normal and extended flux, normal and extended center of
 *	brightness entry definition
 */

__extension__
struct l_fx_efx_ncob_ecob {
	uint32_t exp_flags:24; /* selected exposure flags (24 flags) */
	uint32_t fx;
	uint32_t ncob_x;
	uint32_t ncob_y;
	uint32_t efx;
	uint32_t ecob_x;
	uint32_t ecob_y;
	uint32_t fx_variance;
	uint32_t cob_x_variance;
	uint32_t cob_y_variance;
} __attribute__((packed));


/**
 * @brief normal offset entry definition
 */

struct nc_offset {
	uint32_t mean;
	uint32_t variance;
} __attribute__((packed));


/**
 * @brief normal background entry definition
 */

struct nc_background {
	uint32_t mean;
	uint32_t variance;
	uint16_t outlier_pixels;
} __attribute__((packed));


/**
 * @brief smearing entry definition
 */

struct smearing {
	uint32_t mean;
	uint16_t variance_mean;
	uint16_t outlier_pixels;
} __attribute__((packed));


/*
 * Set and read the max_used_bits registry, which specify how many bits are
 * needed to represent the highest possible value.
 */
uint8_t cmp_get_max_used_bits_version(void);
struct cmp_max_used_bits cmp_get_max_used_bits(void);
void cmp_set_max_used_bits(const struct cmp_max_used_bits *set_max_used_bits);


size_t size_of_a_sample(enum cmp_data_type data_type);
uint32_t cmp_cal_size_of_data(uint32_t samples, enum cmp_data_type data_type);
int32_t cmp_input_size_to_samples(uint32_t size, enum cmp_data_type data_type);

int cmp_input_big_to_cpu_endianness(void *data, uint32_t data_size_byte,
				    enum cmp_data_type data_type);

#endif /* CMP_DATA_TYPE_H */

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

#include "compiler.h"
#include "../common/cmp_support.h"


/* size of the source data header structure for multi entry packet */
#define MULTI_ENTRY_HDR_SIZE 12


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



size_t size_of_a_sample(enum cmp_data_type data_type);
uint32_t cmp_cal_size_of_data(uint32_t samples, enum cmp_data_type data_type);
int32_t cmp_input_size_to_samples(uint32_t size, enum cmp_data_type data_type);

int cmp_input_big_to_cpu_endianness(void *data, uint32_t data_size_byte,
				    enum cmp_data_type data_type);

#endif /* CMP_DATA_TYPE_H */

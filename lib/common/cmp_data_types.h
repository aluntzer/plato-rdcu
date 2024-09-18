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


/* subservice types for service 212 */
#define SST_NCxx_S_SCIENCE_IMAGETTE		3 /* N-Camera imagette data */
#define SST_NCxx_S_SCIENCE_SAT_IMAGETTE		4 /* Extended imagettes for saturated star extra pixels */
#define SST_NCxx_S_SCIENCE_OFFSET		5 /* Offset values Mean of the pixels of offset windows */
#define SST_NCxx_S_SCIENCE_BACKGROUND		6 /* Background values Mean of the pixels of background windows */
#define SST_NCxx_S_SCIENCE_SMEARING		7 /* Smearing array values */
/* subservice Type 8 is not defined */
#define SST_NCxx_S_SCIENCE_S_FX			9 /* Short cadence FX data using normal masks */
#define SST_NCxx_S_SCIENCE_S_FX_EFX		10 /* Short cadence FX data using normal and extended masks */
#define SST_NCxx_S_SCIENCE_S_FX_NCOB		11 /* Short cadence FX and CoB using normal masks */
#define SST_NCxx_S_SCIENCE_S_FX_EFX_NCOB_ECOB	12 /* Short cadence FX and CoB using normal and extended masks */
#define SST_NCxx_S_SCIENCE_L_FX			13 /* Long cadence FX data using normal masks */
#define SST_NCxx_S_SCIENCE_L_FX_EFX		14 /* Long cadence FX data using normal and extended masks */
#define SST_NCxx_S_SCIENCE_L_FX_NCOB		15 /* Long cadence FX and CoB data using normal masks */
#define SST_NCxx_S_SCIENCE_L_FX_EFX_NCOB_ECOB	16 /* Long cadence FX and CoB data using normal and extended masks */
#define SST_NCxx_S_SCIENCE_F_FX			17 /* Fast cadence FX data using normal masks */
#define SST_NCxx_S_SCIENCE_F_FX_EFX		18 /* Fast cadence FX and CoB using normal and extended masks */
#define SST_NCxx_S_SCIENCE_F_FX_NCOB		19 /* Fast cadence FX and CoB using normal masks */
#define SST_NCxx_S_SCIENCE_F_FX_EFX_NCOB_ECOB	20 /* Fast cadence FX and CoB using normal and extended masks */

/* subservice types for service 228 */
#define SST_FCx_S_SCIENCE_IMAGETTE	1 /* Imagettes from F-camera. */
#define SST_FCx_S_SCIENCE_OFFSET_VALUES	2 /* Offset values Mean of the pixels of offset windows */
#define SST_FCx_S_BACKGROUND_VALUES	25 /* TBC: Background values. Mean of the pixels of background windows */

/* size of a collection (multi entry) header */
#define COLLECTION_HDR_SIZE 12

enum col_packet_type {
	COL_WINDOW_PKT_TYPE = 0,
	COL_SCI_PKTS_TYPE = 1
};


/**
 * @brief source data header structure for collection packet
 * @note a collection package contains a collection header followed by multiple
 *	entries of the same science data
 * @see PLATO-LESIA-PL-RP-0031(N-DPU->ICU data rate)
 */
union collection_id {
	uint16_t collection_id;
	__extension__
	struct {
#ifdef __LITTLE_ENDIAN
		uint16_t sequence_num:7;
		uint16_t ccd_id:2;
		uint16_t subservice:6;
		uint16_t pkt_type:1;
#else
		uint16_t pkt_type:1;
		uint16_t subservice:6;
		uint16_t ccd_id:2;
		uint16_t sequence_num:7;
#endif
	} field __attribute__((packed));
} __attribute__((packed));

__extension__
struct collection_hdr {
	uint64_t timestamp:48;		/**< Time when the science observation was made */
	uint16_t configuration_id;	/**< ID of the configuration of the instrument */
	uint16_t collection_id;		/**< ID of a collection */
	uint16_t collection_length;	/**< Expected number of data bytes in the target science packet */
	char  entry[];
} __attribute__((packed));
compile_time_assert(sizeof(struct collection_hdr) == COLLECTION_HDR_SIZE, N_DPU_ICU_COLLECTION_HDR_SIZE_IS_NOT_CORRECT);
compile_time_assert(sizeof(struct collection_hdr) % sizeof(uint32_t) == 0, N_DPU_ICU_COLLECTION_HDR_NOT_4_BYTE_ALLIED);
/* TODO: compile_time_assert(sizeof(struct collection_hdr.collection_id) == sizeof(union collection_id), N_DPU_ICU_COLLECTION_COLLECTION_ID_DO_NOT_MATCH); */


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
 * @brief normal and fast offset entry definition
 */

struct offset {
	uint32_t mean;
	uint32_t variance;
} __attribute__((packed));


/**
 * @brief normal and fast background entry definition
 */

struct background {
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


/* collection header setter functions */
uint64_t cmp_col_get_timestamp(const struct collection_hdr *col);
uint16_t cmp_col_get_configuration_id(const struct collection_hdr *col);

uint16_t cmp_col_get_col_id(const struct collection_hdr *col);
uint8_t  cmp_col_get_pkt_type(const struct collection_hdr *col);
uint8_t  cmp_col_get_subservice(const struct collection_hdr *col);
uint8_t  cmp_col_get_ccd_id(const struct collection_hdr *col);
uint8_t  cmp_col_get_sequence_num(const struct collection_hdr *col);

uint16_t cmp_col_get_data_length(const struct collection_hdr *col);
uint32_t cmp_col_get_size(const struct collection_hdr *col);


/* collection header getter functions */
int cmp_col_set_timestamp(struct collection_hdr *col, uint64_t timestamp);
int cmp_col_set_configuration_id(struct collection_hdr *col, uint16_t configuration_id);

int cmp_col_set_col_id(struct collection_hdr *col, uint16_t collection_id);
int cmp_col_set_pkt_type(struct collection_hdr *col, uint8_t pkt_type);
int cmp_col_set_subservice(struct collection_hdr *col, uint8_t subservice);
int cmp_col_set_ccd_id(struct collection_hdr *col, uint8_t ccd_id);
int cmp_col_set_sequence_num(struct collection_hdr *col, uint8_t sequence_num);

int cmp_col_set_data_length(struct collection_hdr *col, uint16_t length);

enum cmp_data_type convert_subservice_to_cmp_data_type(uint8_t subservice);
uint8_t convert_cmp_data_type_to_subservice(enum cmp_data_type data_type);

size_t size_of_a_sample(enum cmp_data_type data_type);


/* endianness functions */
int be_to_cpu_data_type(void *data, uint32_t data_size_byte, enum cmp_data_type data_type);
#define cpu_to_be_data_type(data, data_size_byte, data_type) be_to_cpu_data_type(data, data_size_byte, data_type)

int be_to_cpu_chunk(uint8_t *chunk, size_t chunk_size);
#define cpu_to_be_chunk(chunk, chunk_size) be_to_cpu_chunk(chunk, chunk_size)

int cmp_input_big_to_cpu_endianness(void *data, uint32_t data_size_byte,
				    enum cmp_data_type data_type);

#endif /* CMP_DATA_TYPE_H */

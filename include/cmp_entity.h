/**
 * @file   cmp_entity.h
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at),
 * @date   Mai, 2021
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
 * @brief functions and definition to handle a compression entity
 * @see Data Compression User Manual PLATO-UVIE-PL-UM-0001
 *
 * @note this code can also used on a little endian machine
 *
 * @warning: If you create an entity of one data product type and use get/set
 * functions intended for another data product type, it will result in a
 * corrupted entity. Do not do this.
 */


#ifndef CMP_ENTITY_H
#define CMP_ENTITY_H

#include <stdint.h>
#include <compiler.h>

#include <cmp_support.h>


#define CMP_ASW_VERSION_ID		0x1

/* Defined Compression Data Product Types */
enum cmp_ent_data_type {
DATA_TYPE_IMAGETTE = 1,
DATA_TYPE_IMAGETTE_ADAPTIVE,
DATA_TYPE_SAT_IMAGETTE,
DATA_TYPE_SAT_IMAGETTE_ADAPTIVE,
DATA_TYPE_OFFSET,
DATA_TYPE_BACKGROUND,
DATA_TYPE_SMEARING,
DATA_TYPE_S_FX,
DATA_TYPE_S_FX_DFX,
DATA_TYPE_S_FX_NCOB,
DATA_TYPE_S_FX_DFX_NCOB_ECOB,
DATA_TYPE_L_FX,
DATA_TYPE_L_FX_DFX,
DATA_TYPE_L_FX_NCOB,
DATA_TYPE_L_FX_DFX_NCOB_ECOB,
DATA_TYPE_F_FX,
DATA_TYPE_F_FX_DFX,
DATA_TYPE_F_FX_NCOB,
DATA_TYPE_F_FX_DFX_NCOB_ECOB,
DATA_TYPE_F_CAM_IMAGETTE,
DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE,
DATA_TYPE_F_CAM_OFFSET,
DATA_TYPE_F_CAM_BACKGROUND,
};

#define GENERIC_HEADER_SIZE 30
#define SPECIFIC_IMAGETTE_HEADER_SIZE		4
#define SPECIFIC_IMAGETTE_ADAPTIVE_HEADER_SIZE	10
#define SPECIFIC_NON_IMAGETTE_HEADER_SIZE	30  /* TBC */

#define IMAGETTE_HEADER_SIZE						\
	(GENERIC_HEADER_SIZE + SPECIFIC_IMAGETTE_HEADER_SIZE)
#define IMAGETTE_ADAPTIVE_HEADER_SIZE					\
	(GENERIC_HEADER_SIZE + SPECIFIC_IMAGETTE_ADAPTIVE_HEADER_SIZE)
#define NON_IMAGETTE_HEADER_SIZE					\
	(GENERIC_HEADER_SIZE + SPECIFIC_NON_IMAGETTE_HEADER_SIZE)

#define CMP_ENTITY_MAX_SIZE 0xFFFFFFUL


__extension__
struct timestamp {
	int32_t coarse;
	int16_t fine;
} __attribute__((packed));

__extension__
struct imagette_header {
	uint16_t spill_used;		/* Spillover threshold used */
	uint8_t  golomb_par_used;	/* Golomb parameter used */
	union{
		struct {
			uint8_t spare1;
			uint8_t ima_cmp_dat[];		/* compressed data for imagette specific header */
		} __attribute__((packed));
		struct {
			uint16_t ap1_spill_used;	/* Adaptive Spillover threshold used 1 */
			uint8_t  ap1_golomb_par_used;	/* Adaptive Golomb parameter used 1 */
			uint16_t ap2_spill_used;	/* Adaptive Spillover threshold used 2 */
			uint8_t  ap2_golomb_par_used;	/* Adaptive Golomb parameter used 2 */
			uint8_t  spare2;
			uint8_t  ap_ima_cmp_data[];	/* compressed data for adaptive imagette specific header */
		} __attribute__((packed));
	};
} __attribute__((packed));
compile_time_assert(sizeof(struct imagette_header) == SPECIFIC_IMAGETTE_ADAPTIVE_HEADER_SIZE, AP_IMAGETTE_HEADER_T_SIZE_IS_NOT_CORRECT);

__extension__
struct non_imagette_header {
	uint32_t spill_1_used:24;	/* spillover threshold 1 used */
	uint16_t cmp_par_1_used;	/* compression parameter 1 used */
	uint32_t spill_2_used:24;	/* spillover threshold 2 used */
	uint16_t cmp_par_2_used;	/* compression parameter 2 used */
	uint32_t spill_3_used:24;	/* spillover threshold 3 used */
	uint16_t cmp_par_3_used;	/* compression parameter 3 used */
	uint32_t spill_4_used:24;	/* spillover threshold 4 used */
	uint16_t cmp_par_4_used;	/* compression parameter 4 used */
	uint32_t spill_5_used:24;	/* spillover threshold 5 used */
	uint16_t cmp_par_5_used;	/* compression parameter 5 used */
	uint32_t spill_6_used:24;	/* spillover threshold 6 used */
	uint16_t cmp_par_6_used;	/* compression parameter 6 used */
	uint8_t  cmp_data[];
} __attribute__((packed));
compile_time_assert(sizeof(struct non_imagette_header) == SPECIFIC_NON_IMAGETTE_HEADER_SIZE, NON_IMAGETTE_HEADER_T_SIZE_IS_NOT_CORRECT);

__extension__
struct cmp_entity {
	uint16_t asw_version_id;		/* ICU ASW Version ID */
	uint32_t cmp_ent_size:24;		/* Compression Entity Size */
	uint32_t original_size:24;		/* Original Data Size */
	union {
		uint64_t start_timestamp:48;	/* Compression Start Timestamp */
		struct timestamp start_time;
	} __attribute__((packed));
	union {
		uint64_t end_timestamp:48;	/* Compression End Timestamp */
		struct timestamp end_time;
	} __attribute__((packed));
	uint16_t data_type;			/* Data Product Type */
	uint8_t  cmp_mode_used;			/* used Compression Mode */
	uint8_t  model_value_used;		/* used Model Updating Weighing Value */
	uint16_t model_id;			/* Model ID */
	uint8_t  model_counter;			/* Model Counter */
	uint8_t  spare;
	uint16_t lossy_cmp_par_used;		/* used Lossy Compression Parameters */
	union {	/* specific Compression Entity Header for the different Data Product Types */
		struct imagette_header ima;
		struct non_imagette_header non_ima;
	};
} __attribute__((packed));
compile_time_assert(sizeof(struct cmp_entity) == NON_IMAGETTE_HEADER_SIZE, CMP_ENTITY_SIZE_IS_NOT_CORRECT);



/* brief create a compression entity by setting the size of the
 * compression entity and the data product type in the entity header
 */
size_t cmp_ent_create(struct cmp_entity *ent, enum cmp_ent_data_type data_type,
		      uint32_t cmp_size_byte);

/* create a compression entity and set the header fields */
size_t cmp_ent_build(struct cmp_entity *ent, enum cmp_ent_data_type data_type,
		     uint16_t asw_version_id, int64_t start_time,
		     int64_t end_time, uint16_t model_id, uint8_t model_counter,
		     struct cmp_info *info, struct cmp_cfg *cfg);



/* set functions for generic compression entity header */
int cmp_ent_set_asw_version_id(struct cmp_entity *ent, uint32_t asw_version_id);
int cmp_ent_set_size(struct cmp_entity *ent, uint32_t cmp_ent_size);
int cmp_ent_set_original_size(struct cmp_entity *ent, uint32_t original_size);

int cmp_ent_set_start_timestamp(struct cmp_entity *ent, uint64_t start_timestamp);
int cmp_ent_set_coarse_start_time(struct cmp_entity *ent, int32_t coarse_time);
int cmp_ent_set_fine_start_time(struct cmp_entity *ent, int16_t fine_time);

int cmp_ent_set_end_timestamp(struct cmp_entity *ent, uint64_t end_timestamp);
int cmp_ent_set_coarse_end_time(struct cmp_entity *ent, int32_t coarse_time);
int cmp_ent_set_fine_end_time(struct cmp_entity *ent, int16_t fine_time);

int cmp_ent_set_data_type(struct cmp_entity *ent,
			  enum cmp_ent_data_type data_type);
int cmp_ent_set_cmp_mode(struct cmp_entity *ent, uint32_t cmp_mode_used);
int cmp_ent_set_model_value(struct cmp_entity *ent, uint32_t model_value_used);
int cmp_ent_set_model_id(struct cmp_entity *ent, uint32_t model_id);
int cmp_ent_set_model_counter(struct cmp_entity *ent, uint32_t model_counter);
int cmp_ent_set_lossy_cmp_par(struct cmp_entity *ent, uint32_t lossy_cmp_par_used);


/* set functions for specific entity header for imagette and adaptive imagette
 * data product types
 */
int cmp_ent_set_ima_spill(struct cmp_entity *ent, uint32_t spill_used);
int cmp_ent_set_ima_golomb_par(struct cmp_entity *ent, uint32_t golomb_par_used);


/* set functions for specific entity header for adaptive imagette data product
 * types
 */
int cmp_ent_set_ima_ap1_spill(struct cmp_entity *ent, uint32_t ap1_spill_used);
int cmp_ent_set_ima_ap1_golomb_par(struct cmp_entity *ent, uint32_t ap1_golomb_par_used);

int cmp_ent_set_ima_ap2_spill(struct cmp_entity *ent, uint32_t ap2_spill_used);
int cmp_ent_set_ima_ap2_golomb_par(struct cmp_entity *ent, uint32_t ap2_golomb_par_used);


/* set functions for specific entity header for non-imagette data product types */
int cmp_ent_set_non_ima_spill1(struct cmp_entity *ent, uint32_t spill1_used);
int cmp_ent_set_non_ima_cmp_par1(struct cmp_entity *ent, uint32_t cmp_par1_used);

int cmp_ent_set_non_ima_spill2(struct cmp_entity *ent, uint32_t spill2_used);
int cmp_ent_set_non_ima_cmp_par2(struct cmp_entity *ent, uint32_t cmp_par2_used);

int cmp_ent_set_non_ima_spill3(struct cmp_entity *ent, uint32_t spill3_used);
int cmp_ent_set_non_ima_cmp_par3(struct cmp_entity *ent, uint32_t cmp_par3_used);

int cmp_ent_set_non_ima_spill4(struct cmp_entity *ent, uint32_t spill4_used);
int cmp_ent_set_non_ima_cmp_par4(struct cmp_entity *ent, uint32_t cmp_par4_used);

int cmp_ent_set_non_ima_spill5(struct cmp_entity *ent, uint32_t spill5_used);
int cmp_ent_set_non_ima_cmp_par5(struct cmp_entity *ent, uint32_t cmp_par5_used);

int cmp_ent_set_non_ima_spill6(struct cmp_entity *ent, uint32_t spill6_used);
int cmp_ent_set_non_ima_cmp_par6(struct cmp_entity *ent, uint32_t cmp_par6_used);



/* get functions for generic compression entity header */
uint16_t cmp_ent_get_asw_version_id(struct cmp_entity *ent);
uint32_t cmp_ent_get_size(struct cmp_entity *ent);
uint32_t cmp_ent_get_original_size(struct cmp_entity *ent);

uint64_t cmp_ent_get_start_timestamp(struct cmp_entity *ent);
int32_t cmp_ent_get_coarse_start_time(struct cmp_entity *ent);
int16_t cmp_ent_get_fine_start_time(struct cmp_entity *ent);

uint64_t cmp_ent_get_end_timestamp(struct cmp_entity *ent);
int32_t cmp_ent_get_coarse_end_time(struct cmp_entity *ent);
int16_t cmp_ent_get_fine_end_time(struct cmp_entity *ent);

enum cmp_ent_data_type cmp_ent_get_data_type(struct cmp_entity *ent);
uint8_t cmp_ent_get_cmp_mode(struct cmp_entity *ent);
uint8_t cmp_ent_get_model_value_used(struct cmp_entity *ent);

uint16_t cmp_ent_get_model_id(struct cmp_entity *ent);
uint8_t cmp_ent_get_model_counter(struct cmp_entity *ent);
uint16_t cmp_ent_get_lossy_cmp_par(struct cmp_entity *ent);


/* get functions for specific entity header for imagette and adaptive imagette
 * data product types
 */
uint16_t cmp_ent_get_ima_spill(struct cmp_entity *ent);
uint8_t cmp_ent_get_ima_golomb_par(struct cmp_entity *ent);


/* get functions for specific entity header for adaptive imagette data product
 * types
 */
uint16_t cmp_ent_get_ima_ap1_spill(struct cmp_entity *ent);
uint8_t cmp_ent_get_ima_ap1_golomb_par(struct cmp_entity *ent);

uint16_t cmp_ent_get_ima_ap2_spill(struct cmp_entity *ent);
uint8_t cmp_ent_get_ima_ap2_golomb_par(struct cmp_entity *ent);


/* get functions for specific entity header for non-imagette data product types */
uint32_t cmp_ent_get_non_ima_spill1(struct cmp_entity *ent);
uint16_t cmp_ent_get_non_ima_cmp_par1(struct cmp_entity *ent);

uint32_t cmp_ent_get_non_ima_spill2(struct cmp_entity *ent);
uint16_t cmp_ent_get_non_ima_cmp_par2(struct cmp_entity *ent);

uint32_t cmp_ent_get_non_ima_spill3(struct cmp_entity *ent);
uint16_t cmp_ent_get_non_ima_cmp_par3(struct cmp_entity *ent);

uint32_t cmp_ent_get_non_ima_spill4(struct cmp_entity *ent);
uint16_t cmp_ent_get_non_ima_cmp_par4(struct cmp_entity *ent);

uint32_t cmp_ent_get_non_ima_spill5(struct cmp_entity *ent);
uint16_t cmp_ent_get_non_ima_cmp_par5(struct cmp_entity *ent);

uint32_t cmp_ent_get_non_ima_spill6(struct cmp_entity *ent);
uint16_t cmp_ent_get_non_ima_cmp_par6(struct cmp_entity *ent);


/* get function for the compressed data buffer in the entity */
uint8_t *cmp_ent_get_data_buf(struct cmp_entity *ent);



/* print and parse functions */
void cmp_ent_print_header(struct cmp_entity *ent);
void cmp_ent_print_data(struct cmp_entity *ent);
void cmp_ent_print(struct cmp_entity *ent);

void cmp_ent_parse(struct cmp_entity *ent);

#endif /* CMP_ENTITY_H */

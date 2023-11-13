/**
 * @file   cmp_entity.c
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   May 2021
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
 */


#include <stdint.h>
#include <string.h>
#include <stdio.h>

#ifndef ICU_ASW
#  if defined __has_include
#    if __has_include(<time.h>)
#      include <time.h>
#      include <stdlib.h>
#      define HAS_TIME_H 1
#      define my_timercmp(s, t, op) (((s)->tv_sec == (t)->tv_sec) ? ((s)->tv_nsec op (t)->tv_nsec) : ((s)->tv_sec op (t)->tv_sec))
#    endif
#  endif
#endif

#include "byteorder.h"
#include "cmp_debug.h"
#include "cmp_support.h"
#include "cmp_data_types.h"
#include "cmp_entity.h"
#include "leon_inttypes.h"


#ifdef HAS_TIME_H
/* Used as epoch Wed Jan  1 00:00:00 2020 */
#  if defined(_WIN32) || defined(_WIN64)
const struct tm PLATO_EPOCH_DATE = { 0, 0, 0, 1, 0, 120, 0, 0, 0 };
#  else
const struct tm PLATO_EPOCH_DATE = { 0, 0, 0, 1, 0, 120, 0, 0, 0, 0, NULL };
#  endif /* _WIN */
#endif /* time.h */


/**
 * @brief calculate the size of the compression entity header based on a
 *	compression data product type
 *
 * @param data_type	compression data product type
 * @param raw_mode_flag	set this flag if the raw compression mode (CMP_MODE_RAW) is used
 *
 * @returns size of the compression entity header in bytes, 0 on unknown data
 *	type
 */

uint32_t cmp_ent_cal_hdr_size(enum cmp_data_type data_type, int raw_mode_flag)
{
	uint32_t size = 0;

	if (raw_mode_flag) {
		if (!cmp_data_type_is_invalid(data_type))
			/* for raw data we do not need a specific header */
			size = GENERIC_HEADER_SIZE;
	} else {
		switch (data_type) {
		case DATA_TYPE_IMAGETTE:
		case DATA_TYPE_SAT_IMAGETTE:
		case DATA_TYPE_F_CAM_IMAGETTE:
			size = IMAGETTE_HEADER_SIZE;
			break;
		case DATA_TYPE_IMAGETTE_ADAPTIVE:
		case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
		case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
			size = IMAGETTE_ADAPTIVE_HEADER_SIZE;
			break;
		case DATA_TYPE_OFFSET:
		case DATA_TYPE_BACKGROUND:
		case DATA_TYPE_SMEARING:
		case DATA_TYPE_S_FX:
		case DATA_TYPE_S_FX_EFX:
		case DATA_TYPE_S_FX_NCOB:
		case DATA_TYPE_S_FX_EFX_NCOB_ECOB:
		case DATA_TYPE_L_FX:
		case DATA_TYPE_L_FX_EFX:
		case DATA_TYPE_L_FX_NCOB:
		case DATA_TYPE_L_FX_EFX_NCOB_ECOB:
		case DATA_TYPE_F_FX:
		case DATA_TYPE_F_FX_EFX:
		case DATA_TYPE_F_FX_NCOB:
		case DATA_TYPE_F_FX_EFX_NCOB_ECOB:
		case DATA_TYPE_F_CAM_OFFSET:
		case DATA_TYPE_F_CAM_BACKGROUND:
			size = NON_IMAGETTE_HEADER_SIZE;
			break;
		case DATA_TYPE_UNKNOWN:
			size = 0;
			break;
		}
	}
	return size;
}


/**
 * @brief set ICU ASW Version ID in the compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param version_id	the applications software version identifier
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_version_id(struct cmp_entity *ent, uint32_t version_id)
{
	if (!ent)
		return -1;

	ent->version_id = cpu_to_be32(version_id);

	return 0;
}


/**
 * @brief set the size of the compression entity in the entity header
 *
 * @param ent		pointer to a compression entity
 * @param cmp_ent_size	the compression entity size measured in bytes
 *
 * @note maximum size is CMP_ENTITY_MAX_SIZE
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_size(struct cmp_entity *ent, uint32_t cmp_ent_size)
{
	if (!ent)
		return -1;

	if (cmp_ent_size > CMP_ENTITY_MAX_SIZE)
		return -1;

#ifdef __LITTLE_ENDIAN
	ent->cmp_ent_size = cpu_to_be32(cmp_ent_size) >> 8;
#else
	ent->cmp_ent_size = cmp_ent_size;
#endif /* __LITTLE_ENDIAN */

	return 0;
}


/**
 * @brief set the original size of the compressed data in the entity header
 *
 * @param ent		pointer to a compression entity
 * @param original_size	the original size of the compressed data measured in
 *	bytes
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_original_size(struct cmp_entity *ent, uint32_t original_size)
{
	if (!ent)
		return -1;

	if (original_size > 0xFFFFFFUL)
		return -1;

#ifdef __LITTLE_ENDIAN
	ent->original_size = cpu_to_be32(original_size) >> 8;
#else
	ent->original_size = original_size;
#endif /* __LITTLE_ENDIAN */

	return 0;
}


/**
 * @brief set the compression start timestamp in the compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param start_timestamp	compression start timestamp (coarse and fine)
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_start_timestamp(struct cmp_entity *ent, uint64_t start_timestamp)
{
	if (!ent)
		return -1;

	if (start_timestamp > 0xFFFFFFFFFFFFULL)
		return -1;

#ifdef __LITTLE_ENDIAN
	ent->start_timestamp = cpu_to_be64(start_timestamp) >> 16;
#else
	ent->start_timestamp = start_timestamp;
#endif /* __LITTLE_ENDIAN */

	return 0;
}


/**
 * @brief set the coarse time in the compression start timestamp in the
 *	compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param coarse_time	coarse part of the compression start timestamp
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_coarse_start_time(struct cmp_entity *ent, uint32_t coarse_time)
{
	if (!ent)
		return -1;

	ent->start_time.coarse = cpu_to_be32(coarse_time);

	return 0;
}


/**
 * @brief set the fine time in the compression start timestamp in the
 *	compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param fine_time	fine part of the compression start timestamp
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_fine_start_time(struct cmp_entity *ent, uint16_t fine_time)
{
	if (!ent)
		return -1;

	ent->start_time.fine = cpu_to_be16(fine_time);

	return 0;
}


/**
 * @brief set the compression end timestamp in the compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param end_timestamp	compression end timestamp (coarse and fine)
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_end_timestamp(struct cmp_entity *ent, uint64_t end_timestamp)
{
	if (!ent)
		return -1;

	if (end_timestamp > 0xFFFFFFFFFFFFULL)
		return -1;

#ifdef __LITTLE_ENDIAN
	ent->end_timestamp = cpu_to_be64(end_timestamp) >> 16;
#else
	ent->end_timestamp = end_timestamp;
#endif /* __LITTLE_ENDIAN */

	return 0;
}


/**
 * @brief set the coarse time in the compression end timestamp in the
 *	compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param coarse_time	coarse part of the compression end timestamp
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_coarse_end_time(struct cmp_entity *ent, uint32_t coarse_time)
{
	if (!ent)
		return -1;

	ent->end_time.coarse = cpu_to_be32(coarse_time);

	return 0;
}


/**
 * @brief set the fine time in the compression end timestamp in the compression
 *	entity header
 *
 * @param ent		pointer to a compression entity
 * @param fine_time	fine part of the compression end timestamp
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_fine_end_time(struct cmp_entity *ent, uint16_t fine_time)
{
	if (!ent)
		return -1;

	ent->end_time.fine = cpu_to_be16(fine_time);

	return 0;
}


/**
 * @brief set the compression data product type in the compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param data_type	compression data product type
 * @param raw_mode_flag	set this flag if the raw compression mode (CMP_MODE_RAW) is used
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_data_type(struct cmp_entity *ent, enum cmp_data_type data_type,
			  int raw_mode_flag)
{
	if (!ent)
		return -1;

	if (data_type > 0x7FF)
		return -1;

	if (raw_mode_flag)
		data_type |= 1U << RAW_BIT_DATA_TYPE_POS;

	ent->data_type = cpu_to_be16((uint16_t)data_type);

	return 0;
}


/**
 * @brief set the used compression mode in the compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param cmp_mode_used	used compression mode parameter
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_cmp_mode(struct cmp_entity *ent, enum cmp_mode cmp_mode_used)
{
	if (!ent)
		return -1;

	if (cmp_mode_used > UINT8_MAX)
		return -1;

	ent->cmp_mode_used = (uint8_t)cmp_mode_used;

	return 0;
}


/**
 * @brief set the used model weighing value in the compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param model_value_used	used model weighing value parameter
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_model_value(struct cmp_entity *ent, uint32_t model_value_used)
{
	if (!ent)
		return -1;

	if (model_value_used > UINT8_MAX)
		return -1;

	ent->model_value_used = (uint8_t)model_value_used;

	return 0;
}


/**
 * @brief set model id in the compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param model_id	the model identifier
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_model_id(struct cmp_entity *ent, uint32_t model_id)
{
	if (!ent)
		return -1;

	if (model_id > UINT16_MAX)
		return -1;

	ent->model_id = cpu_to_be16((uint16_t)model_id);

	return 0;
}


/**
 * @brief set model counter in the compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param model_counter	the model counter
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_model_counter(struct cmp_entity *ent, uint32_t model_counter)
{
	if (!ent)
		return -1;

	if (model_counter > UINT8_MAX)
		return -1;

	ent->model_counter = (uint8_t)model_counter;

	return 0;
}


/**
 * @brief set version identifier for the maximum used bits registry in the
 *	compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param max_used_bits_version	the identifier for the maximum used bits registry
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_max_used_bits_version(struct cmp_entity *ent, uint8_t max_used_bits_version)
{
	if (!ent)
		return -1;

	ent->max_used_bits_version = max_used_bits_version;

	return 0;
}


/**
 * @brief set the used lossy compression parameter in the compression entity
 *	header
 *
 * @param ent			pointer to a compression entity
 * @param lossy_cmp_par_used	used lossy compression parameter/round parameter
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_lossy_cmp_par(struct cmp_entity *ent, uint32_t lossy_cmp_par_used)
{
	if (!ent)
		return -1;

	if (lossy_cmp_par_used > UINT16_MAX)
		return -1;

	ent->lossy_cmp_par_used = cpu_to_be16((uint16_t)lossy_cmp_par_used);

	return 0;
}


/**
 * @brief set the used spillover threshold parameter in the (adaptive) imagette
 *	specific compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param spill_used	used spillover threshold for imagette data_type
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_ima_spill(struct cmp_entity *ent, uint32_t spill_used)
{
	if (!ent)
		return -1;

	if (spill_used > UINT16_MAX)
		return -1;

	ent->ima.spill_used = cpu_to_be16((uint16_t)spill_used);

	return 0;
}


/**
 * @brief set used Golomb parameter in the (adaptive) imagette specific
 *	compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param golomb_par_used	used Golomb parameter used for imagette
 * data_type
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_ima_golomb_par(struct cmp_entity *ent, uint32_t golomb_par_used)
{
	if (!ent)
		return -1;

	if (golomb_par_used > UINT8_MAX)
		return -1;

	ent->ima.golomb_par_used = (uint8_t)golomb_par_used;

	return 0;
}


/*
 * @brief set the used adaptive 1 spillover threshold parameter in the adaptive
 *	imagette specific compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param ap1_spill_used	used adaptive 1 spillover threshold used for
 *	semi-adaptive compression feature
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_ima_ap1_spill(struct cmp_entity *ent, uint32_t ap1_spill_used)
{
	if (!ent)
		return -1;

	if (ap1_spill_used > UINT16_MAX)
		return -1;

	ent->ima.ap1_spill_used = cpu_to_be16((uint16_t)ap1_spill_used);

	return 0;
}


/**
 * @brief set adaptive 1 Golomb parameter in the adaptive imagette specific
 *	compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param ap1_golomb_par_used	used adaptive 1 Golomb parameter for
 *	semi-adaptive compression feature
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_ima_ap1_golomb_par(struct cmp_entity *ent, uint32_t ap1_golomb_par_used)
{
	if (!ent)
		return -1;

	if (ap1_golomb_par_used > UINT8_MAX)
		return -1;

	ent->ima.ap1_golomb_par_used = (uint8_t)ap1_golomb_par_used;

	return 0;
}


/*
 * @brief set the used adaptive 2 spillover threshold parameter in the adaptive
 *	imagette specific compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param ap2_spill_used	used adaptive 2 spillover threshold used for
 *	semi-adaptive compression feature
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_ima_ap2_spill(struct cmp_entity *ent, uint32_t ap2_spill_used)
{
	if (!ent)
		return -1;

	if (ap2_spill_used > UINT16_MAX)
		return -1;

	ent->ima.ap2_spill_used = cpu_to_be16((uint16_t)ap2_spill_used);

	return 0;
}


/**
 * @brief set adaptive 2 Golomb parameter in the adaptive imagette specific
 *	compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param ap2_golomb_par_used	used adaptive 2 Golomb parameter for
 *	semi-adaptive compression feature
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_ima_ap2_golomb_par(struct cmp_entity *ent, uint32_t ap2_golomb_par_used)
{
	if (!ent)
		return -1;

	if (ap2_golomb_par_used > UINT8_MAX)
		return -1;

	ent->ima.ap2_golomb_par_used = (uint8_t)ap2_golomb_par_used;

	return 0;
}


/*
 * @brief set the used spillover threshold 1 parameter in the non-imagette
 *	specific compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param spill_1_used	used spillover threshold parameter 1 for non-imagette
 *	data_types
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_non_ima_spill1(struct cmp_entity *ent, uint32_t spill_1_used)
{
	if (!ent)
		return -1;

	if (spill_1_used > 0xFFFFFFUL)
		return -1;

#ifdef __LITTLE_ENDIAN
	ent->non_ima.spill_1_used = cpu_to_be32(spill_1_used) >> 8;
#else
	ent->non_ima.spill_1_used = spill_1_used;
#endif /* __LITTLE_ENDIAN */

	return 0;
}


/**
 * @brief set used compression parameter 1 in the non-imagette specific
 *	compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param cmp_par_1_used	used compression parameter 1
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_non_ima_cmp_par1(struct cmp_entity *ent, uint32_t cmp_par_1_used)
{
	if (!ent)
		return -1;

	if (cmp_par_1_used > UINT16_MAX)
		return -1;

	ent->non_ima.cmp_par_1_used = cpu_to_be16((uint16_t)cmp_par_1_used);

	return 0;
}


/*
 * @brief set the used spillover threshold 2 parameter in the non-imagette
 *	specific compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param spill_2_used	used spillover threshold parameter 2 for non-imagette
 *	data_types
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_non_ima_spill2(struct cmp_entity *ent, uint32_t spill_2_used)
{
	if (!ent)
		return -1;

	if (spill_2_used > 0xFFFFFFUL)
		return -1;

#ifdef __LITTLE_ENDIAN
	ent->non_ima.spill_2_used = cpu_to_be32(spill_2_used) >> 8;
#else
	ent->non_ima.spill_2_used = spill_2_used;
#endif /* __LITTLE_ENDIAN */

	return 0;
}


/**
 * @brief set used compression parameter 2 in the non-imagette specific
 *	compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param cmp_par_2_used	used compression parameter 2
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_non_ima_cmp_par2(struct cmp_entity *ent, uint32_t cmp_par_2_used)
{
	if (!ent)
		return -1;

	if (cmp_par_2_used > UINT16_MAX)
		return -1;

	ent->non_ima.cmp_par_2_used = cpu_to_be16((uint16_t)cmp_par_2_used);

	return 0;
}


/*
 * @brief set the used spillover threshold 3 parameter in the non-imagette
 *	specific compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param spill_3_used	used spillover threshold parameter 3 for non-imagette
 *	data_types
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_non_ima_spill3(struct cmp_entity *ent, uint32_t spill_3_used)
{
	if (!ent)
		return -1;

	if (spill_3_used > 0xFFFFFFUL)
		return -1;

#ifdef __LITTLE_ENDIAN
	ent->non_ima.spill_3_used = cpu_to_be32(spill_3_used) >> 8;
#else
	ent->non_ima.spill_3_used = spill_3_used;
#endif /* __LITTLE_ENDIAN */

	return 0;
}


/**
 * @brief set used compression parameter 3 in the non-imagette specific
 *	compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param cmp_par_3_used	used compression parameter 3
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_non_ima_cmp_par3(struct cmp_entity *ent, uint32_t cmp_par_3_used)
{
	if (!ent)
		return -1;

	if (cmp_par_3_used > UINT16_MAX)
		return -1;

	ent->non_ima.cmp_par_3_used = cpu_to_be16((uint16_t)cmp_par_3_used);

	return 0;
}


/*
 * @brief set the used spillover threshold 4 parameter in the non-imagette
 *	specific compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param spill_4_used	used spillover threshold parameter 4 for non-imagette
 *	data_types
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_non_ima_spill4(struct cmp_entity *ent, uint32_t spill_4_used)
{
	if (!ent)
		return -1;

	if (spill_4_used > 0xFFFFFFUL)
		return -1;

#ifdef __LITTLE_ENDIAN
	ent->non_ima.spill_4_used = cpu_to_be32(spill_4_used) >> 8;
#else
	ent->non_ima.spill_4_used = spill_4_used;
#endif /* __LITTLE_ENDIAN */

	return 0;
}


/**
 * @brief set used compression parameter 4 in the non-imagette specific
 *	compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param cmp_par_4_used	used compression parameter 4
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_non_ima_cmp_par4(struct cmp_entity *ent, uint32_t cmp_par_4_used)
{
	if (!ent)
		return -1;

	if (cmp_par_4_used > UINT16_MAX)
		return -1;

	ent->non_ima.cmp_par_4_used = cpu_to_be16((uint16_t)cmp_par_4_used);

	return 0;
}


/*
 * @brief set the used spillover threshold 5 parameter in the non-imagette
 *	specific compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param spill_5_used	used spillover threshold parameter 5 for non-imagette
 *	data_types
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_non_ima_spill5(struct cmp_entity *ent, uint32_t spill_5_used)
{
	if (!ent)
		return -1;

	if (spill_5_used > 0xFFFFFFUL)
		return -1;

#ifdef __LITTLE_ENDIAN
	ent->non_ima.spill_5_used = cpu_to_be32(spill_5_used) >> 8;
#else
	ent->non_ima.spill_5_used = spill_5_used;
#endif /* __LITTLE_ENDIAN */

	return 0;
}


/**
 * @brief set used compression parameter 5 in the non-imagette specific
 *	compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param cmp_par_5_used	used compression parameter 5
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_non_ima_cmp_par5(struct cmp_entity *ent, uint32_t cmp_par_5_used)
{
	if (!ent)
		return -1;

	if (cmp_par_5_used > UINT16_MAX)
		return -1;

	ent->non_ima.cmp_par_5_used = cpu_to_be16((uint16_t)cmp_par_5_used);

	return 0;
}


/*
 * @brief set the used spillover threshold 6 parameter in the non-imagette
 *	specific compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param spill_6_used	used spillover threshold parameter 6 for non-imagette
 *	data_types
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_non_ima_spill6(struct cmp_entity *ent, uint32_t spill_6_used)
{
	if (!ent)
		return -1;

	if (spill_6_used > 0xFFFFFFUL)
		return -1;

#ifdef __LITTLE_ENDIAN
	ent->non_ima.spill_6_used = cpu_to_be32(spill_6_used) >> 8;
#else
	ent->non_ima.spill_6_used = spill_6_used;
#endif /* __LITTLE_ENDIAN */

	return 0;
}


/**
 * @brief set used compression parameter 6 in the non-imagette specific
 *	compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param cmp_par_6_used	used compression parameter 6
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_non_ima_cmp_par6(struct cmp_entity *ent, uint32_t cmp_par_6_used)
{
	if (!ent)
		return -1;

	if (cmp_par_6_used > UINT16_MAX)
		return -1;

	ent->non_ima.cmp_par_6_used = cpu_to_be16((uint16_t)cmp_par_6_used);

	return 0;
}


/**
 * @brief get the ASW version identifier from the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the ASW version identifier on success, 0 on error
 */

uint32_t cmp_ent_get_version_id(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be32_to_cpu(ent->version_id);
}


/**
 * @brief get the size of the compression entity from the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the size of the compression entity in bytes on success, 0 on error
 */

uint32_t cmp_ent_get_size(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

#ifdef __LITTLE_ENDIAN
	return be32_to_cpu(ent->cmp_ent_size) >> 8;
#else
	return ent->cmp_ent_size;
#endif /* __LITTLE_ENDIAN */
}


/**
 * @brief get the original data size from the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the original size of the compressed data in bytes on success, 0 on error
 */

uint32_t cmp_ent_get_original_size(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

#ifdef __LITTLE_ENDIAN
	return be32_to_cpu(ent->original_size) >> 8;
#else
	return ent->original_size;
#endif /* __LITTLE_ENDIAN */
}


/**
 * @brief get the compression start timestamp from the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the compression start timestamp on success, 0 on error
 */

uint64_t cmp_ent_get_start_timestamp(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

#ifdef __LITTLE_ENDIAN
	return be64_to_cpu(ent->start_timestamp) >> 16;
#else
	return ent->start_timestamp;
#endif /* __LITTLE_ENDIAN */
}


/**
 * @brief get the coarse time from the compression start timestamp in the
 *	compression entity header
 *
 * @returns the coarse part of the compression start timestamp on success, 0 on
 *	error
 */

uint32_t cmp_ent_get_coarse_start_time(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be32_to_cpu(ent->start_time.coarse);
}


/**
 * @brief get the fine time from the compression start timestamp in the
 *	compression entity header
 *
 * @returns the fine part of the compression start timestamp on success, 0 on
 *	error
 */

uint16_t cmp_ent_get_fine_start_time(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->start_time.fine);
}


/**
 * @brief get the compression end timestamp from the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the compression end timestamp on success, 0 on error
 */

uint64_t cmp_ent_get_end_timestamp(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

#ifdef __LITTLE_ENDIAN
	return be64_to_cpu(ent->end_timestamp) >> 16;
#else
	return ent->end_timestamp;
#endif /* __LITTLE_ENDIAN */
}


/**
 * @brief get the coarse time from the compression end timestamp in the
 *	compression entity header
 *
 * @returns the coarse part of the compression end timestamp on success, 0 on
 *	error
 */

uint32_t cmp_ent_get_coarse_end_time(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be32_to_cpu(ent->end_time.coarse);
}


/**
 * @brief get the fine time from the compression end timestamp in the
 *	compression entity header
 *
 * @returns the fine part of the compression end timestamp on success, 0 on
 *	error
 */

uint16_t cmp_ent_get_fine_end_time(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->end_time.fine);
}


/**
 * @brief get data_type from the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the data_type NOT including the uncompressed data bit on success,
 *	DATA_TYPE_UNKNOWN on error
 */

enum cmp_data_type cmp_ent_get_data_type(const struct cmp_entity *ent)
{
	enum cmp_data_type data_type;

	if (!ent)
		return DATA_TYPE_UNKNOWN;

	data_type = be16_to_cpu(ent->data_type);
	data_type &= (1U << RAW_BIT_DATA_TYPE_POS)-1; /* remove uncompressed data flag */

	if (cmp_data_type_is_invalid(data_type))
		data_type = DATA_TYPE_UNKNOWN;

	return data_type;
}


/**
 * @brief get the raw bit from the data_type field of the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the data_type raw bit on success, 0 on error
 */

int cmp_ent_get_data_type_raw_bit(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return (be16_to_cpu(ent->data_type) >> RAW_BIT_DATA_TYPE_POS) & 1U;
}


/**
 * @brief get the used compression mode parameter from the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used compression mode on success, 0 on error
 */

uint8_t cmp_ent_get_cmp_mode(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return ent->cmp_mode_used;
}


/**
 * @brief get used model value from the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used model value used on success, 0 on error
 */

uint8_t cmp_ent_get_model_value(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return ent->model_value_used;

}


/**
 * @brief get model id from the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the model identifier on success, 0 on error
 */

uint16_t cmp_ent_get_model_id(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->model_id);
}


/**
 * @brief get the model counter from the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the model counter on success, 0 on error
 */

uint8_t cmp_ent_get_model_counter(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return ent->model_counter;
}


/**
 * @brief get the version identifier for the maximum used bits registry from the
 *	compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the version identifier for the maximum used bits registry on success,
 *	0 on error
 */

uint8_t cmp_ent_get_max_used_bits_version(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return ent->max_used_bits_version;
}


/**
 * @brief get the used lossy compression parameter from the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used lossy compression/round parameter on success, 0 on error
 */

uint16_t cmp_ent_get_lossy_cmp_par(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->lossy_cmp_par_used);
}


/**
 * @brief get the used spillover threshold parameter from the (adaptive)
 *	imagette specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used spillover threshold on success, 0 on error
 */

uint16_t cmp_ent_get_ima_spill(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->ima.spill_used);
}


/**
 * @brief get the used Golomb parameter from the (adaptive) imagette specific
 *	compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used Golomb parameter on success, 0 on error
 */

uint8_t cmp_ent_get_ima_golomb_par(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return ent->ima.golomb_par_used;
}


/**
 * @brief get the used adaptive 1 spillover threshold parameter from the
 *	adaptive imagette specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used adaptive 1 spillover threshold on success, 0 on error
 */

uint16_t cmp_ent_get_ima_ap1_spill(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->ima.ap1_spill_used);
}


/**
 * @brief get the used adaptive 1 Golomb parameter from adaptive imagette
 *	specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used adaptive 1 Golomb parameter on success, 0 on error
 */

uint8_t cmp_ent_get_ima_ap1_golomb_par(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return ent->ima.ap1_golomb_par_used;
}


/**
 * @brief get the used adaptive 2 spillover threshold parameter from the
 *	adaptive imagette specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used adaptive 2 spillover threshold on success, 0 on error
 */

uint16_t cmp_ent_get_ima_ap2_spill(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->ima.ap2_spill_used);
}


/**
 * @brief get the used adaptive 2 spillover threshold parameter from the
 *	adaptive imagette specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used adaptive 2 Golomb parameter on success, 0 on error
 */

uint8_t cmp_ent_get_ima_ap2_golomb_par(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return ent->ima.ap2_golomb_par_used;
}


/**
 * @brief get the used spillover threshold 1 parameter from the non-imagette
 *	specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used spillover threshold 1 parameter on success, 0 on error
 */

uint32_t cmp_ent_get_non_ima_spill1(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

#ifdef __LITTLE_ENDIAN
	return be32_to_cpu(ent->non_ima.spill_1_used) >> 8;
#else
	return ent->non_ima.spill_1_used;
#endif /* __LITTLE_ENDIAN */
}


/**
 * @brief get the used compression parameter 1 from the non-imagette specific
 *	compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used compression parameter 1 on success, 0 on error
 */

uint16_t cmp_ent_get_non_ima_cmp_par1(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->non_ima.cmp_par_1_used);
}


/**
 * @brief get the used spillover threshold 2 parameter from the non-imagette
 *	specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used spillover threshold 2 parameter on success, 0 on error
 */

uint32_t cmp_ent_get_non_ima_spill2(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

#ifdef __LITTLE_ENDIAN
	return be32_to_cpu(ent->non_ima.spill_2_used) >> 8;
#else
	return ent->non_ima.spill_2_used;
#endif /* __LITTLE_ENDIAN */
}


/**
 * @brief get the used compression parameter 2 from the non-imagette specific
 *	compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used compression parameter 2 on success, 0 on error
 */

uint16_t cmp_ent_get_non_ima_cmp_par2(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->non_ima.cmp_par_2_used);
}


/**
 * @brief get the used spillover threshold 3 parameter from the non-imagette
 *	specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used spillover threshold 3 parameter on success, 0 on error
 */

uint32_t cmp_ent_get_non_ima_spill3(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

#ifdef __LITTLE_ENDIAN
	return be32_to_cpu(ent->non_ima.spill_3_used) >> 8;
#else
	return ent->non_ima.spill_3_used;
#endif /* __LITTLE_ENDIAN */
}


/**
 * @brief get the used compression parameter 3 from the non-imagette specific
 *	compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used compression parameter 3 on success, 0 on error
 */

uint16_t cmp_ent_get_non_ima_cmp_par3(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->non_ima.cmp_par_3_used);
}


/**
 * @brief get the used spillover threshold 4 parameter from the non-imagette
 *	specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used spillover threshold 4 parameter on success, 0 on error
 */

uint32_t cmp_ent_get_non_ima_spill4(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

#ifdef __LITTLE_ENDIAN
	return be32_to_cpu(ent->non_ima.spill_4_used) >> 8;
#else
	return ent->non_ima.spill_4_used;
#endif /* __LITTLE_ENDIAN */
}


/**
 * @brief get the used compression parameter 4 from the non-imagette specific
 *	compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used compression parameter 4 on success, 0 on error
 */

uint16_t cmp_ent_get_non_ima_cmp_par4(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->non_ima.cmp_par_4_used);
}


/**
 * @brief get the used spillover threshold 5 parameter from the non-imagette
 *	specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used spillover threshold 5 parameter on success, 0 on error
 */

uint32_t cmp_ent_get_non_ima_spill5(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

#ifdef __LITTLE_ENDIAN
	return be32_to_cpu(ent->non_ima.spill_5_used) >> 8;
#else
	return ent->non_ima.spill_5_used;
#endif /* __LITTLE_ENDIAN */
}


/**
 * @brief get the used compression parameter 5 from the non-imagette specific
 *	compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used compression parameter 5 on success, 0 on error
 */

uint16_t cmp_ent_get_non_ima_cmp_par5(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->non_ima.cmp_par_5_used);
}


/**
 * @brief get the used spillover threshold 6 parameter from the non-imagette
 *	specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used spillover threshold 6 parameter on success, 0 on error
 */

uint32_t cmp_ent_get_non_ima_spill6(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

#ifdef __LITTLE_ENDIAN
	return be32_to_cpu(ent->non_ima.spill_6_used) >> 8;
#else
	return ent->non_ima.spill_6_used;
#endif /* __LITTLE_ENDIAN */
}


/**
 * @brief get the used compression parameter 6 from the non-imagette specific
 *	compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used compression parameter 6 on success, 0 on error
 */

uint16_t cmp_ent_get_non_ima_cmp_par6(const struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->non_ima.cmp_par_6_used);
}


/**
 * @brief get the start address of the compressed data in the compression
 *	entity
 *
 * @param ent	pointer to a compression entity
 *
 * @note this only works if the data_type in the compression entity is set
 *
 * @returns a pointer to buffer where the compressed data are located in entity
 *	on success, NULL on error
 */

void *cmp_ent_get_data_buf(struct cmp_entity *ent)
{
	enum cmp_data_type data_type;

	if (!ent)
		return NULL;

	data_type = cmp_ent_get_data_type(ent);
	if (data_type == DATA_TYPE_UNKNOWN) {
		debug_print("Error: Compression data type not supported.\n");
		return NULL;
	}

	if (cmp_ent_get_data_type_raw_bit(ent))
		return (uint8_t *)ent + GENERIC_HEADER_SIZE;

	switch (data_type) {
	case DATA_TYPE_IMAGETTE:
	case DATA_TYPE_SAT_IMAGETTE:
	case DATA_TYPE_F_CAM_IMAGETTE:
		return ent->ima.ima_cmp_dat;
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		return ent->ima.ap_ima_cmp_data;
	case DATA_TYPE_OFFSET:
	case DATA_TYPE_BACKGROUND:
	case DATA_TYPE_SMEARING:
	case DATA_TYPE_S_FX:
	case DATA_TYPE_S_FX_EFX:
	case DATA_TYPE_S_FX_NCOB:
	case DATA_TYPE_S_FX_EFX_NCOB_ECOB:
	case DATA_TYPE_L_FX:
	case DATA_TYPE_L_FX_EFX:
	case DATA_TYPE_L_FX_NCOB:
	case DATA_TYPE_L_FX_EFX_NCOB_ECOB:
	case DATA_TYPE_F_FX:
	case DATA_TYPE_F_FX_EFX:
	case DATA_TYPE_F_FX_NCOB:
	case DATA_TYPE_F_FX_EFX_NCOB_ECOB:
	case DATA_TYPE_F_CAM_OFFSET:
	case DATA_TYPE_F_CAM_BACKGROUND:
		return ent->non_ima.cmp_data;
	/* LCOV_EXCL_START */
	case DATA_TYPE_UNKNOWN:
	default:
		return NULL;
	/* LCOV_EXCL_STOP */
	}
}


/**
 * @brief copy the data from a compression entity to a buffer
 *
 * @param ent		pointer to the compression entity containing the compressed data
 * @param data_buf	pointer to the destination data buffer to which the
 *	compressed data are copied (can be NULL)
 * @param data_buf_size	size of the destination data buffer
 *
 * @returns the size in bytes to store the compressed data; negative on error
 *
 * @note converts the data to the system endianness
 */

int32_t cmp_ent_get_cmp_data(struct cmp_entity *ent, uint32_t *data_buf,
			     uint32_t data_buf_size)
{
	uint32_t *cmp_ent_data_adr;
	uint32_t cmp_size_byte;

	if (!ent)
		return -1;

	cmp_ent_data_adr = cmp_ent_get_data_buf(ent);
	if (!cmp_ent_data_adr) {
		debug_print("Error: Compression data type is not supported.\n");
		return -1;
	}

	cmp_size_byte = cmp_ent_get_cmp_data_size(ent);
	if (cmp_size_byte & 0x3) {
		debug_print("Error: The compressed data are not correct formatted. Expected multiple of 4 hex words.\n");
		return -1;
	}

	if (data_buf) {
		uint32_t i;
		uint32_t cmp_data_len_32;

		if (cmp_size_byte > data_buf_size) {
			debug_print("Error: data_buf size to small to hold the data.\n");
			return -1;
		}

		memcpy(data_buf, cmp_ent_data_adr, cmp_size_byte);

		cmp_data_len_32 = cmp_size_byte/sizeof(uint32_t);
		for (i = 0; i < cmp_data_len_32; i++)
			be32_to_cpus(&data_buf[i]);
	}

	return (int32_t)cmp_size_byte;
}


/**
 * @brief get the size of the compression entity header based on the set data
 *	product type in compression entity
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the size of the entity header in bytes on success, 0 on error
 */

static uint32_t cmp_ent_get_hdr_size(const struct cmp_entity *ent)
{
	return cmp_ent_cal_hdr_size(cmp_ent_get_data_type(ent),
				    cmp_ent_get_data_type_raw_bit(ent));
}


/**
 * @brief get the size of the compressed data based on the set data product
 *	type and compressed entity size in the compression entity
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the size of the compressed data in bytes on success, 0 on error
 */

uint32_t cmp_ent_get_cmp_data_size(const struct cmp_entity *ent)
{
	uint32_t cmp_ent_size, header_size;

	header_size = cmp_ent_get_hdr_size(ent);
	cmp_ent_size = cmp_ent_get_size(ent);

	if (header_size > cmp_ent_size)
		return 0;

	return cmp_ent_size - header_size;
}


/**
 * @brief write the compression parameters from a compression configuration
 *	into the compression entity header
 * @note NO compressed data are put into the entity and NO change of the entity
 *	size
 *
 * @param ent		pointer to a compression entity
 * @param cfg		pointer to a compression configuration
 * @param cmp_size_bits	size of the compressed data in bits
 *
 * @returns 0 on success, negative on error
 */

int cmp_ent_write_cmp_pars(struct cmp_entity *ent, const struct cmp_cfg *cfg,
			   int cmp_size_bits)
{
	uint32_t ent_cmp_data_size;

	if (!cfg)
		return -1;

	if (cmp_size_bits < 0)
		return -1;

	if (cfg->data_type != cmp_ent_get_data_type(ent)) {
		debug_print("Error: The entity data product type dos not match the configuration data product type.\n");
		return -1;
	}

	if (cmp_ent_get_data_type_raw_bit(ent) != (cfg->cmp_mode == CMP_MODE_RAW)) {
		debug_print("Error: The entity's raw data bit does not match up with the compression mode.\n");
		return -1;
	}

	ent_cmp_data_size = cmp_ent_get_cmp_data_size(ent);

	/* check if the entity can hold the compressed data */
	if (ent_cmp_data_size < cmp_bit_to_4byte((unsigned int)cmp_size_bits)) {
		debug_print("Error: The entity size is to small to hold the compressed data.\n");
		return -2;
	}

	/* set compression parameter fields in the generic entity header */
	if (cmp_ent_set_original_size(ent, cmp_cal_size_of_data(cfg->samples,
								cfg->data_type)))
		return -1;
	if (cmp_ent_set_cmp_mode(ent, cfg->cmp_mode))
		return -1;
	if (cmp_ent_set_model_value(ent, cfg->model_value))
		return -1;
	if (cfg->max_used_bits)
		cmp_ent_set_max_used_bits_version(ent, cfg->max_used_bits->version);
	else
		cmp_ent_set_max_used_bits_version(ent, 0);
	if (cmp_ent_set_lossy_cmp_par(ent, cfg->round))
		return -1;

	if (cfg->cmp_mode == CMP_MODE_RAW) /* no specific header is used for raw data we are done */
		return 0;

	switch (cmp_ent_get_data_type(ent)) {
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		if (cmp_ent_set_ima_ap1_spill(ent, cfg->ap1_spill))
			return -1;
		if (cmp_ent_set_ima_ap1_golomb_par(ent, cfg->ap1_golomb_par))
			return -1;
		if (cmp_ent_set_ima_ap2_spill(ent, cfg->ap2_spill))
			return -1;
		if (cmp_ent_set_ima_ap2_golomb_par(ent, cfg->ap2_golomb_par))
			return -1;
		/* fall through */
	case DATA_TYPE_IMAGETTE:
	case DATA_TYPE_SAT_IMAGETTE:
	case DATA_TYPE_F_CAM_IMAGETTE:
		if (cmp_ent_set_ima_spill(ent, cfg->spill))
			return -1;
		if (cmp_ent_set_ima_golomb_par(ent, cfg->golomb_par))
			return -1;
		break;
	case DATA_TYPE_OFFSET:
	case DATA_TYPE_F_CAM_OFFSET:
	case DATA_TYPE_BACKGROUND:
	case DATA_TYPE_F_CAM_BACKGROUND:
	case DATA_TYPE_SMEARING:
		if (cmp_ent_set_non_ima_cmp_par1(ent, cfg->cmp_par_mean))
			return -1;
		if (cmp_ent_set_non_ima_spill1(ent, cfg->spill_mean))
			return -1;

		if (cmp_ent_set_non_ima_cmp_par2(ent, cfg->cmp_par_variance))
			return -1;
		if (cmp_ent_set_non_ima_spill2(ent, cfg->spill_variance))
			return -1;

		if (cmp_ent_set_non_ima_cmp_par3(ent, cfg->cmp_par_pixels_error))
			return -1;
		if (cmp_ent_set_non_ima_spill3(ent, cfg->spill_pixels_error))
			return -1;

		cmp_ent_set_non_ima_cmp_par4(ent, 0);
		cmp_ent_set_non_ima_spill4(ent, 0);

		cmp_ent_set_non_ima_cmp_par5(ent, 0);
		cmp_ent_set_non_ima_spill5(ent, 0);

		cmp_ent_set_non_ima_cmp_par6(ent, 0);
		cmp_ent_set_non_ima_spill6(ent, 0);
		break;
	case DATA_TYPE_S_FX:
	case DATA_TYPE_S_FX_EFX:
	case DATA_TYPE_S_FX_NCOB:
	case DATA_TYPE_S_FX_EFX_NCOB_ECOB:
	case DATA_TYPE_L_FX:
	case DATA_TYPE_L_FX_EFX:
	case DATA_TYPE_L_FX_NCOB:
	case DATA_TYPE_L_FX_EFX_NCOB_ECOB:
	case DATA_TYPE_F_FX:
	case DATA_TYPE_F_FX_EFX:
	case DATA_TYPE_F_FX_NCOB:
	case DATA_TYPE_F_FX_EFX_NCOB_ECOB:
		if (cmp_ent_set_non_ima_cmp_par1(ent, cfg->cmp_par_exp_flags))
			return -1;
		if (cmp_ent_set_non_ima_spill1(ent, cfg->spill_exp_flags))
			return -1;

		if (cmp_ent_set_non_ima_cmp_par2(ent, cfg->cmp_par_fx))
			return -1;
		if (cmp_ent_set_non_ima_spill2(ent, cfg->spill_fx))
			return -1;

		if (cmp_ent_set_non_ima_cmp_par3(ent, cfg->cmp_par_ncob))
			return -1;
		if (cmp_ent_set_non_ima_spill3(ent, cfg->spill_ncob))
			return -1;

		if (cmp_ent_set_non_ima_cmp_par4(ent, cfg->cmp_par_efx))
			return -1;
		if (cmp_ent_set_non_ima_spill4(ent, cfg->spill_efx))
			return -1;

		if (cmp_ent_set_non_ima_cmp_par5(ent, cfg->cmp_par_ecob))
			return -1;
		if (cmp_ent_set_non_ima_spill5(ent, cfg->spill_ecob))
			return -1;

		if (cmp_ent_set_non_ima_cmp_par6(ent, cfg->cmp_par_fx_cob_variance))
			return -1;
		if (cmp_ent_set_non_ima_spill6(ent, cfg->spill_fx_cob_variance))
			return -1;

		break;
	case DATA_TYPE_UNKNOWN:
	default:
		return -1;
	}

	return 0;
}


/**
 * @brief write the parameters from the RDCU decompression information structure
 *	in the compression entity header
 * @note no compressed data are put into the entity and no change of the entity
 *	size
 *
 * @param ent	pointer to a compression entity
 * @param info	pointer to a decompression information structure
 * @param cfg	pointer to a compression configuration structure for adaptive
 *	compression parameters (can be NULL if non adaptive data_type is used)
 *
 * @returns 0 on success, negative on error
 */

int cmp_ent_write_rdcu_cmp_pars(struct cmp_entity *ent, const struct cmp_info *info,
				const struct cmp_cfg *cfg)
{
	uint32_t ent_cmp_data_size;
	enum cmp_data_type data_type;

	if (!ent)
		return -1;

	if (!info)
		return -1;

	if (info->cmp_err) {
		debug_print("Error: The decompression information contains an compression error.\n");
		return -1;
	}

	data_type = cmp_ent_get_data_type(ent);
	if (!rdcu_supported_data_type_is_used(data_type)) {
		debug_print("Error: The compression data type is not one of the types supported by the RDCU.\n");
		return -1;
	}

	if (cmp_ent_get_data_type_raw_bit(ent) != raw_mode_is_used(info->cmp_mode_used)) {
		debug_print("Error: The entity's raw data bit does not match up with the compression mode.\n");
		return -1;
	}

	/* check if the entity can hold the compressed data */
	ent_cmp_data_size = cmp_ent_get_cmp_data_size(ent);
	if (ent_cmp_data_size < cmp_bit_to_4byte(info->cmp_size)) {
		debug_print("Error: The entity size is to small to hold the compressed data.\n");
		return -2;
	}

	/* set compression parameter fields in the generic entity header */
	if (cmp_ent_set_original_size(ent, cmp_cal_size_of_data(info->samples_used, DATA_TYPE_IMAGETTE)))
		return -1;
	if (cmp_ent_set_cmp_mode(ent, info->cmp_mode_used))
		return -1;
	cmp_ent_set_model_value(ent, info->model_value_used);
	/* The RDCU data compressor does not have the maximum used bit feature,
	 * so the field is set to 0. */
	cmp_ent_set_max_used_bits_version(ent, 0);

	cmp_ent_set_lossy_cmp_par(ent, info->round_used);

	if (raw_mode_is_used(info->cmp_mode_used))
		/* no specific header is used for raw data we are done */
		return 0;

	if (cmp_ent_set_ima_spill(ent, info->spill_used))
		return -1;
	if (cmp_ent_set_ima_golomb_par(ent, info->golomb_par_used))
		return -1;

	/* use the adaptive imagette parameter from the compression configuration
	 * if an adaptive imagette compression data type is ent in the entity
	 */
	if (cmp_ap_imagette_data_type_is_used(data_type)) {
		if (!cfg) {
			debug_print("Error: Need the compression configuration to get the adaptive parameters.\n");
			return -1;
		}
		if (cmp_ent_set_ima_ap1_spill(ent, cfg->ap1_spill))
			return -1;
		if (cmp_ent_set_ima_ap1_golomb_par(ent, cfg->ap1_golomb_par))
			return -1;
		if (cmp_ent_set_ima_ap2_spill(ent, cfg->ap2_spill))
			return -1;
		if (cmp_ent_set_ima_ap2_golomb_par(ent, cfg->ap2_golomb_par))
			return -1;
	}

	return 0;
}


/**
 * @brief create a compression entity by setting the size of the compression
 *	entity and the data product type in the entity header
 *
 * @param ent		pointer to a compression entity; if NULL, the function
 *			returns the needed size
 * @param data_type	compression data product type
 * @param raw_mode_flag	set this flag if the raw compression mode (CMP_MODE_RAW) is used
 * @param cmp_size_byte	size of the compressed data in bytes (should be a multiple of 4)
 *
 * @note if the entity size is smaller than the largest header, the function
 *	rounds up the entity size to the largest header
 *
 * @returns the size of the compression entity or 0 on error
 */

uint32_t cmp_ent_create(struct cmp_entity *ent, enum cmp_data_type data_type,
			int raw_mode_flag, uint32_t cmp_size_byte)
{
	uint32_t hdr_size = cmp_ent_cal_hdr_size(data_type, raw_mode_flag);
	uint32_t ent_size = hdr_size + cmp_size_byte;
	uint32_t ent_size_cpy = ent_size;

	if (!hdr_size)
		return 0;

	/* catch overflows */
	if (cmp_size_byte > CMP_ENTITY_MAX_SIZE)
		return 0;
	if (ent_size > CMP_ENTITY_MAX_SIZE)
		return 0;

	/* to be safe a compression entity should be at least the size of the
	 * largest entity header */
	if (ent_size < sizeof(struct cmp_entity))
		ent_size = sizeof(struct cmp_entity);

	if (!ent)
		return ent_size;

	memset(ent, 0, hdr_size);

	cmp_ent_set_size(ent, ent_size_cpy);
	cmp_ent_set_data_type(ent, data_type, raw_mode_flag);

	return ent_size;
}


/**
 * @brief create a compression entity and set the header fields
 *
 * @note this function simplifies the entity set up by creating an entity and
 *	setting the header fields in one function call
 * @note no compressed data are put into the entity
 *
 * @param ent			pointer to a compression entity; if NULL, the
 *	function returns the needed size
 * @param version_id		applications software version identifier
 * @param start_time		compression start timestamp (coarse and fine)
 * @param end_time		compression end timestamp (coarse and fine)
 * @param model_id		model identifier
 * @param model_counter		model counter
 * @param cfg			pointer to compression configuration (can be NULL)
 * @param cmp_size_bits		length of the compressed data in bits
 *
 * @returns the size of the compression entity or 0 on error
 */

uint32_t cmp_ent_build(struct cmp_entity *ent, uint32_t version_id,
		       uint64_t start_time, uint64_t end_time, uint16_t model_id,
		       uint8_t model_counter, const struct cmp_cfg *cfg, int cmp_size_bits)
{
	uint32_t cmp_size_bytes = cmp_bit_to_4byte((unsigned int)cmp_size_bits); /* TODO: do we need to round up to 4 bytes? */
	uint32_t hdr_size;

	if (!cfg)
		return 0;

	if (cmp_size_bits < 0)
		return 0;

	if (!cmp_ent_create(ent, cfg->data_type, cfg->cmp_mode == CMP_MODE_RAW, cmp_size_bytes))
		return 0;

	if (ent) {
		cmp_ent_set_version_id(ent, version_id);
		if (cmp_ent_set_start_timestamp(ent, start_time))
			return 0;
		if (cmp_ent_set_end_timestamp(ent, end_time))
			return 0;
		cmp_ent_set_model_id(ent, model_id);
		cmp_ent_set_model_counter(ent, model_counter);
		if (cmp_ent_write_cmp_pars(ent, cfg, cmp_size_bits))
			return 0;
	}

	hdr_size = cmp_ent_cal_hdr_size(cfg->data_type, cfg->cmp_mode == CMP_MODE_RAW);

	return hdr_size + cmp_size_bytes;
}


#ifdef HAS_TIME_H
/*
 * @brief Convert a calendar time expressed as a struct tm object to time since
 *	 epoch as a time_t object. The function interprets the input structure
 *	 as representing Universal Coordinated Time (UTC).
 * @note timegm is a GNU C Library extension, not standardized. This function
 *	is used as a portable alternative
 * @note The function is thread-unsafe
 *
 * @param tm	pointer to a broken-down time representation, expressed in
 *	Coordinated Universal Time (UTC)
 *
 * @returns time since epoch as a time_t object on success; or -1 if time cannot
 *	be represented as a time_t object
 *
 * @see http://www.catb.org/esr/time-programming/#_unix_time_and_utc_gmt_zulu
 */

static time_t my_timegm(struct tm *tm)
{
#  if defined(_WIN32) || defined(_WIN64)
	return _mkgmtime(tm);
#  else
	time_t ret;
	char *tz;

	tz = getenv("TZ");
	if (tz)
		tz = strdup(tz);
	setenv("TZ", "", 1);
	tzset();
	ret = mktime(tm);
	if (tz) {
		setenv("TZ", tz, 1);
		free(tz);
	} else
		unsetenv("TZ");
	tzset();
	return ret;
#  endif /* _WIN32 || _WIN64 */
}
#endif /* HAS_TIME_H */


#ifdef HAS_TIME_H
/*
 * @brief Generate a timestamp for the compression header
 *
 * @param ts	pointer to an object of type struct timespec of the
 *	timestamp time, NULL for now
 *
 * @returns returns compression header timestamp or 0 on error
 */

uint64_t cmp_ent_create_timestamp(const struct timespec *ts)
{
	struct tm epoch_date = PLATO_EPOCH_DATE;
	struct timespec epoch = { my_timegm(&epoch_date), 0 };
	struct timespec now = { 0, 0 };
	double seconds;
	uint64_t coarse, fine;

	/* LCOV_EXCL_START */
	/* if time cannot be represented as a time_t object epoch.tv_sec = -1 */
	if (epoch.tv_sec == -1)
		return 0;
	/* LCOV_EXCL_STOP */

	if (ts) {
		if (my_timercmp(ts, &epoch, <)) {
			debug_print("Error: Time is before PLATO epoch.\n");
			return 0;
		}
		now = *ts;
	} else {
		clock_gettime(CLOCK_REALTIME, &now);
	}

	seconds = ((double)now.tv_sec + 1.0e-9 * (double)now.tv_nsec) -
		  ((double)epoch.tv_sec + 1.0e-9 * (double)epoch.tv_nsec);

	coarse = (uint64_t)seconds;
	fine = (uint64_t)((seconds - (double)coarse) * 256 * 256);

	return (coarse << 16) + fine;
}
#endif /* HAS_TIME_H */


/**
 * @brief print the content of the compression entity header
 *
 * @param ent	pointer to a compression entity
 */

void cmp_ent_print_header(const struct cmp_entity *ent)
{
	const uint8_t *p = (const uint8_t *)ent;
	uint32_t hdr_size = cmp_ent_get_hdr_size(ent);
	size_t i;

	for (i = 0; i < hdr_size; ++i) {
		debug_print("%02X ", p[i]);
		if (i && !((i+1) % 32))
			debug_print("\n");
	}
	debug_print("\n");
}


/**
 * @brief print the compressed data of the entity
 *
 * @param ent	pointer to a compression entity
 */

void cmp_ent_print_data(struct cmp_entity *ent)
{
	const uint8_t *p = cmp_ent_get_data_buf(ent);
	size_t data_size = cmp_ent_get_cmp_data_size(ent);
	size_t i;

	if (!p)
		return;

	for (i = 0; i < data_size; ++i) {
		debug_print("%02X ", p[i]);
		if (i && !((i+1) % 32))
			debug_print("\n");
	}
	debug_print("\n");
}


/**
 * @brief print the entire compressed entity header plus data
 *
 * @param ent	pointer to a compression entity
 */

void cmp_ent_print(struct cmp_entity *ent)
{
	debug_print("compression entity header:\n");
	cmp_ent_print_header(ent);
	debug_print("compressed data in the compressed entity:\n");
	cmp_ent_print_data(ent);
}


/**
 * @brief parses the generic compressed entity header
 *
 * @param ent	pointer to a compression entity
 */

static void cmp_ent_parse_generic_header(const struct cmp_entity *ent)
{
	uint32_t version_id, cmp_ent_size, original_size, cmp_mode_used,
		 model_value_used, model_id, model_counter, max_used_bits_version,
		 lossy_cmp_par_used, start_coarse_time, end_coarse_time;
	uint16_t start_fine_time, end_fine_time;
	enum cmp_data_type data_type;
	int raw_bit;

	version_id = cmp_ent_get_version_id(ent);
	if (version_id & CMP_TOOL_VERSION_ID_BIT) {
		uint16_t major = (version_id & 0x7FFF0000U) >> 16U;
		uint16_t minor = version_id & 0xFFFFU;

		debug_print("Compressed with cmp_tool version: %u.%02u\n", major, minor);
	} else
		debug_print("ICU ASW Version ID: %" PRIu32 "\n", version_id);

	cmp_ent_size = cmp_ent_get_size(ent);
	debug_print("Compression Entity Size: %" PRIu32 " byte\n", cmp_ent_size);

	original_size = cmp_ent_get_original_size(ent);
	debug_print("Original Data Size: %" PRIu32 " byte\n", original_size);

	start_coarse_time = cmp_ent_get_coarse_start_time(ent);
	debug_print("Compression Coarse Start Time: %" PRIu32 "\n", start_coarse_time);

	start_fine_time = cmp_ent_get_fine_start_time(ent);
	debug_print("Compression Fine Start Time: %d\n", start_fine_time);

	end_coarse_time = cmp_ent_get_coarse_end_time(ent);
	debug_print("Compression Coarse End Time: %" PRIu32 "\n", end_coarse_time);

	end_fine_time = cmp_ent_get_fine_end_time(ent);
	debug_print("Compression Fine End Time: %d\n", end_fine_time);

#ifdef HAS_TIME_H
	{
		struct tm epoch_date = PLATO_EPOCH_DATE;
		time_t time = my_timegm(&epoch_date) + start_coarse_time;

		debug_print("Data were compressed on (local time): %s", ctime(&time));
	}
#endif
	debug_print("The compression took %f second\n", end_coarse_time - start_coarse_time
		+ ((end_fine_time - start_fine_time)/256./256.));

	data_type = cmp_ent_get_data_type(ent);
	debug_print("Data Product Type: %d\n", data_type);

	raw_bit = cmp_ent_get_data_type_raw_bit(ent);
	debug_print("RAW bit in the Data Product Type is%s set\n", raw_bit ? "" : " not");

	cmp_mode_used = cmp_ent_get_cmp_mode(ent);
	debug_print("Used Compression Mode: %" PRIu32 "\n", cmp_mode_used);

	model_value_used = cmp_ent_get_model_value(ent);
	debug_print("Used Model Updating Weighing Value: %" PRIu32 "\n", model_value_used);

	model_id = cmp_ent_get_model_id(ent);
	debug_print("Model ID: %" PRIu32 "\n", model_id);

	model_counter = cmp_ent_get_model_counter(ent);
	debug_print("Model Counter: %" PRIu32 "\n", model_counter);

	max_used_bits_version = cmp_ent_get_max_used_bits_version(ent);
	debug_print("Maximum Used Bits Registry Version: %" PRIu32 "\n", max_used_bits_version);

	lossy_cmp_par_used = cmp_ent_get_lossy_cmp_par(ent);
	debug_print("Used Lossy Compression Parameters: %" PRIu32 "\n", lossy_cmp_par_used);
}


/**
 * @brief parse the imagette specific compressed entity header
 *
 * @param ent	pointer to a compression entity
 */

static void cmp_ent_parese_imagette_header(const struct cmp_entity *ent)
{
	uint32_t spill_used, golomb_par_used;

	spill_used = cmp_ent_get_ima_spill(ent);
	debug_print("Used Spillover Threshold Parameter: %" PRIu32 "\n", spill_used);

	golomb_par_used = cmp_ent_get_ima_golomb_par(ent);
	debug_print("Used Golomb Parameter: %" PRIu32 "\n", golomb_par_used);
}


/**
 * @brief parse the adaptive imagette specific compressed entity header
 *
 * @param ent	pointer to a compression entity
 */

static void cmp_ent_parese_adaptive_imagette_header(const struct cmp_entity *ent)
{
	uint32_t spill_used, golomb_par_used, ap1_spill_used,
		 ap1_golomb_par_used, ap2_spill_used, ap2_golomb_par_used;

	spill_used = cmp_ent_get_ima_spill(ent);
	debug_print("Used Spillover Threshold Parameter: %" PRIu32 "\n", spill_used);

	golomb_par_used = cmp_ent_get_ima_golomb_par(ent);
	debug_print("Used Golomb Parameter: %" PRIu32 "\n", golomb_par_used);

	ap1_spill_used = cmp_ent_get_ima_ap1_spill(ent);
	debug_print("Used Adaptive 1 Spillover Threshold Parameter: %" PRIu32 "\n", ap1_spill_used);

	ap1_golomb_par_used = cmp_ent_get_ima_ap1_golomb_par(ent);
	debug_print("Used Adaptive 1 Golomb Parameter: %" PRIu32 "\n", ap1_golomb_par_used);

	ap2_spill_used = cmp_ent_get_ima_ap2_spill(ent);
	debug_print("Used Adaptive 2 Spillover Threshold Parameter: %" PRIu32 "\n", ap2_spill_used);

	ap2_golomb_par_used = cmp_ent_get_ima_ap2_golomb_par(ent);
	debug_print("Used Adaptive 2 Golomb Parameter: %" PRIu32 "\n", ap2_golomb_par_used);
}


/**
 * @brief parse the specific compressed entity header
 *
 * @param ent	pointer to a compression entity
 */

static void cmp_ent_parese_specific_header(const struct cmp_entity *ent)
{
	enum cmp_data_type data_type = cmp_ent_get_data_type(ent);

	if (cmp_ent_get_data_type_raw_bit(ent)) {
		debug_print("Uncompressed data bit is set. No specific header is used.\n");
		return;
	}

	switch (data_type) {
	case DATA_TYPE_IMAGETTE:
	case DATA_TYPE_SAT_IMAGETTE:
	case DATA_TYPE_F_CAM_IMAGETTE:
		cmp_ent_parese_imagette_header(ent);
		break;
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		cmp_ent_parese_adaptive_imagette_header(ent);
		break;
	default:
		debug_print("For this data product type no parse functions is implemented!\n");
		break;
	}
}


/**
 * @brief parse the compressed entity header
 *
 * @param ent	pointer to a compression entity
 */

void cmp_ent_parse(struct cmp_entity *ent)
{
	cmp_ent_parse_generic_header(ent);

	cmp_ent_parese_specific_header(ent);
}


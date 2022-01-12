/**
 * @file   cmp_entity.c
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at),
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
#include <stdio.h>
#include <string.h>
#include <limits.h>
#if defined __has_include
#   if __has_include(<time.h>)
#     include <time.h>
#     include <stdlib.h>
#     define HAS_TIME_H 1
#   endif
#endif

#include <cmp_entity.h>
#include <cmp_support.h>
#include <byteorder.h>


#if defined HAS_TIME_H
/* Used as epoch Wed Jan  1 00:00:00 2020 */
#  if defined(_WIN32) || defined(_WIN64)
const struct tm EPOCH_DATE = { 0, 0, 0, 1, 0, 120, 0, 0, 0 };
#  else
const struct tm EPOCH_DATE = { 0, 0, 0, 1, 0, 120, 0, 0, 0, 0, NULL };
#  endif /* _WIN */
#endif /* time.h */


/**
 * @brief calculate the size of the compression entity header based of the data
 *	product type
 *
 * @param data_type	compression entity data product type
 *
 * @returns size of the compression entity header in bytes, 0 on unknown data
 *	type
 */

uint32_t cmp_ent_cal_hdr_size(enum cmp_ent_data_type data_type)
{
	switch (data_type) {
	case DATA_TYPE_IMAGETTE:
	case DATA_TYPE_F_CAM_IMAGETTE:
		return IMAGETTE_HEADER_SIZE;
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		return IMAGETTE_ADAPTIVE_HEADER_SIZE;
	case DATA_TYPE_SAT_IMAGETTE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_OFFSET:
	case DATA_TYPE_BACKGROUND:
	case DATA_TYPE_SMEARING:
	case DATA_TYPE_S_FX:
	case DATA_TYPE_S_FX_DFX:
	case DATA_TYPE_S_FX_NCOB:
	case DATA_TYPE_S_FX_DFX_NCOB_ECOB:
	case DATA_TYPE_L_FX:
	case DATA_TYPE_L_FX_DFX:
	case DATA_TYPE_L_FX_NCOB:
	case DATA_TYPE_L_FX_DFX_NCOB_ECOB:
	case DATA_TYPE_F_FX:
	case DATA_TYPE_F_FX_DFX:
	case DATA_TYPE_F_FX_NCOB:
	case DATA_TYPE_F_FX_DFX_NCOB_ECOB:
	case DATA_TYPE_F_CAM_OFFSET:
	case DATA_TYPE_F_CAM_BACKGROUND:
		return NON_IMAGETTE_HEADER_SIZE;
	case DATA_TYPE_UNKOWN:
		return 0;
	}

	return 0;
}


/**
 * @brief set ICU ASW Version ID in the compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param asw_version_id	the applications software version identifier
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_asw_version_id(struct cmp_entity *ent, uint32_t asw_version_id)
{
	if (!ent)
		return -1;

	if (asw_version_id > UINT16_MAX)
		return -1;

	ent->asw_version_id = cpu_to_be16(asw_version_id);

	return 0;
}


/**
 * @brief set the size of the compression entity in the entity header
 *
 * @param ent		pointer to a compression entity
 * @param cmp_ent_size	the compression entity size measured in bytes
 *
 * @note maximum size is 2^24-1
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
 * @brief set the data product type in the compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param data_type	compression entity data product type
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_data_type(struct cmp_entity *ent,
			  enum cmp_ent_data_type data_type)
{
	if (!ent)
		return -1;

	if ((unsigned int)data_type > 0x7FFF)
		return -1;

	data_type |= cmp_ent_get_data_type_raw_bit(ent) << RAW_BIT_IN_DATA_TYPE;

	ent->data_type = cpu_to_be16(data_type);

	return 0;
}


/**
 * @brief set the raw bit in the data product field of the compression entity header
 *
 * @param ent		pointer to a compression entity
 * @param raw_bit	raw bit is set if raw_bit is non zero
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_data_type_raw_bit(struct cmp_entity *ent, int raw_bit)
{
	uint16_t data_type;

	if (!ent)
		return -1;

	if (raw_bit)
		data_type = cpu_to_be16(ent->data_type) | 1UL << RAW_BIT_IN_DATA_TYPE;
	else
		data_type = cpu_to_be16(ent->data_type) & ~(1UL << RAW_BIT_IN_DATA_TYPE);

	ent->data_type =  cpu_to_be16(data_type);

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

int cmp_ent_set_cmp_mode(struct cmp_entity *ent, uint32_t cmp_mode_used)
{
	if (!ent)
		return -1;

	if (cmp_mode_used > UINT8_MAX)
		return -1;

	ent->cmp_mode_used = cmp_mode_used;

	return 0;
}


/**
 * @brief set the used model value in the compression entity header
 *
 * @param ent			pointer to a compression entity
 * @param model_value_used	used model weighting value parameter
 *
 * @returns 0 on success, otherwise error
 */

int cmp_ent_set_model_value(struct cmp_entity *ent, uint32_t model_value_used)
{
	if (!ent)
		return -1;

	if (model_value_used > UINT8_MAX)
		return -1;

	ent->model_value_used = model_value_used;

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

	ent->model_id = cpu_to_be16(model_id);

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

	ent->model_counter = model_counter;

	return 0;
}


/**
 * @brief set the used lossy compression parameter in the compression entity
 *	header
 *
 * @param ent			pointer to a compression entity
 * @param lossy_cmp_par_used	used lossy compression parameter
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

	ent->ima.spill_used = cpu_to_be16(spill_used);

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

	ent->ima.golomb_par_used = golomb_par_used;

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

	ent->ima.ap1_spill_used = cpu_to_be16(ap1_spill_used);

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

	ent->ima.ap1_golomb_par_used = ap1_golomb_par_used;

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

	ent->ima.ap2_spill_used = cpu_to_be16(ap2_spill_used);

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

	ent->ima.ap2_golomb_par_used = ap2_golomb_par_used;

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

	ent->non_ima.cmp_par_1_used = cpu_to_be16(cmp_par_1_used);

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

	ent->non_ima.cmp_par_2_used = cpu_to_be16(cmp_par_2_used);

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

	ent->non_ima.cmp_par_3_used = cpu_to_be16(cmp_par_3_used);

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

	ent->non_ima.cmp_par_4_used = cpu_to_be16(cmp_par_4_used);

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

	ent->non_ima.cmp_par_5_used = cpu_to_be16(cmp_par_5_used);

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

	ent->non_ima.cmp_par_6_used = cpu_to_be16(cmp_par_6_used);

	return 0;
}


/**
 * @brief get the ASW version identifier form the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the ASW version identifier on success, 0 on error
 */

uint16_t cmp_ent_get_asw_version_id(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->asw_version_id);
}


/**
 * @brief get the size of the compression entity form the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the size of the compression entity in bytes on success, 0 on error
 */

uint32_t cmp_ent_get_size(struct cmp_entity *ent)
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
 * @brief get the original data size form the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the original size of the compressed data in bytes on success, 0 on error
 */

uint32_t cmp_ent_get_original_size(struct cmp_entity *ent)
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
 * @brief get the compression start timestamp form the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the compression start timestamp on success, 0 on error
 */

uint64_t cmp_ent_get_start_timestamp(struct cmp_entity *ent)
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
 * @brief get the coarse time form the compression start timestamp in the
 *	compression entity header
 *
 * @returns the coarse part of the compression start timestamp on success, 0 on
 *	error
 */

uint32_t cmp_ent_get_coarse_start_time(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be32_to_cpu(ent->start_time.coarse);
}


/**
 * @brief get the fine time form the compression start timestamp in the
 *	compression entity header
 *
 * @returns the fine part of the compression start timestamp on success, 0 on
 *	error
 */

uint16_t cmp_ent_get_fine_start_time(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->start_time.fine);
}


/**
 * @brief get the compression end timestamp form the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the compression end timestamp on success, 0 on error
 */

uint64_t cmp_ent_get_end_timestamp(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

#ifdef __LITTLE_ENDIAN
	return be64_to_cpu(ent->end_timestamp << 16);
#else
	return ent->end_timestamp;
#endif /* __LITTLE_ENDIAN */
}


/**
 * @brief get the coarse time form the compression end timestamp in the
 *	compression entity header
 *
 * @returns the coarse part of the compression end timestamp on success, 0 on
 *	error
 */

uint32_t cmp_ent_get_coarse_end_time(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be32_to_cpu(ent->end_time.coarse);
}


/**
 * @brief get the fine time form the compression end timestamp in the
 *	compression entity header
 *
 * @returns the fine part of the compression end timestamp on success, 0 on
 *	error
 */

uint16_t cmp_ent_get_fine_end_time(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->end_time.fine);
}


/**
 * @brief get data_type form the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the data_type on success, DATA_TYPE_UNKOWN on error
 */

enum cmp_ent_data_type cmp_ent_get_data_type(struct cmp_entity *ent)
{
	if (!ent)
		return DATA_TYPE_UNKOWN;

	return be16_to_cpu(ent->data_type) & 0X7FFF;
}


/**
 * @brief get raw bit form the data_type field of the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the data_type raw bit on success, 0 on error
 */

int cmp_ent_get_data_type_raw_bit(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return (be16_to_cpu(ent->data_type) >> RAW_BIT_IN_DATA_TYPE) & 1U;
}


/**
 * @brief get the used compression mode parameter from the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used compression on success, 0 on error
 */

uint8_t cmp_ent_get_cmp_mode(struct cmp_entity *ent)
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

uint8_t cmp_ent_get_model_value_used(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return ent->model_value_used;

}


/**
 * @brief get model id form the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the model identifier on success, 0 on error
 */

uint16_t cmp_ent_get_model_id(struct cmp_entity *ent)
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

uint8_t cmp_ent_get_model_counter(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return ent->model_counter;
}


/**
 * @brief get the used lossy compression parameter form the compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used lossy compression parameter on success, 0 on error
 */

uint16_t cmp_ent_get_lossy_cmp_par(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->lossy_cmp_par_used);
}


/**
 * @brief get the used spillover threshold parameter form the (adaptive)
 *	imagette specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used spillover threshold on success, 0 on error
 */

uint16_t cmp_ent_get_ima_spill(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->ima.spill_used);
}


/**
 * @brief get the used Golomb parameter form the (adaptive) imagette specific
 *	compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used Golomb parameter on success, 0 on error
 */

uint8_t cmp_ent_get_ima_golomb_par(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return ent->ima.golomb_par_used;
}


/**
 * @brief get the used adaptive 1 spillover threshold parameter form the
 *	adaptive imagette specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used adaptive 1 spillover threshold on success, 0 on error
 */

uint16_t cmp_ent_get_ima_ap1_spill(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->ima.ap1_spill_used);
}


/**
 * @brief get the used adaptive 1 Golomb parameter form adaptive imagette
 *	specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used adaptive 1 Golomb parameter on success, 0 on error
 */

uint8_t cmp_ent_get_ima_ap1_golomb_par(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return ent->ima.ap1_golomb_par_used;
}


/**
 * @brief get the used adaptive 2 spillover threshold parameter form the
 *	adaptive imagette specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used adaptive 2 spillover threshold on success, 0 on error
 */

uint16_t cmp_ent_get_ima_ap2_spill(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->ima.ap2_spill_used);
}


/**
 * @brief get the used adaptive 2 spillover threshold parameter form the
 *	adaptive imagette specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used adaptive 2 Golomb parameter on success, 0 on error
 */

uint8_t cmp_ent_get_ima_ap2_golomb_par(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return ent->ima.ap2_golomb_par_used;
}


/**
 * @brief get the used spillover threshold 1 parameter form the non-imagette
 *	specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used spillover threshold 1 parameter on success, 0 on error
 */

uint32_t cmp_ent_get_non_ima_spill1(struct cmp_entity *ent)
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
 * @brief get the used compression parameter 1 form the non-imagette specific
 *	compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used compression parameter 1 on success, 0 on error
 */

uint16_t cmp_ent_get_non_ima_cmp_par1(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->non_ima.cmp_par_1_used);
}


/**
 * @brief get the used spillover threshold 2 parameter form the non-imagette
 *	specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used spillover threshold 2 parameter on success, 0 on error
 */

uint32_t cmp_ent_get_non_ima_spill2(struct cmp_entity *ent)
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
 * @brief get the used compression parameter 2 form the non-imagette specific
 *	compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used compression parameter 2 on success, 0 on error
 */

uint16_t cmp_ent_get_non_ima_cmp_par2(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->non_ima.cmp_par_2_used);
}


/**
 * @brief get the used spillover threshold 3 parameter form the non-imagette
 *	specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used spillover threshold 3 parameter on success, 0 on error
 */

uint32_t cmp_ent_get_non_ima_spill3(struct cmp_entity *ent)
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
 * @brief get the used compression parameter 3 form the non-imagette specific
 *	compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used compression parameter 3 on success, 0 on error
 */

uint16_t cmp_ent_get_non_ima_cmp_par3(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->non_ima.cmp_par_3_used);
}


/**
 * @brief get the used spillover threshold 4 parameter form the non-imagette
 *	specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used spillover threshold 4 parameter on success, 0 on error
 */

uint32_t cmp_ent_get_non_ima_spill4(struct cmp_entity *ent)
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
 * @brief get the used compression parameter 4 form the non-imagette specific
 *	compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used compression parameter 4 on success, 0 on error
 */

uint16_t cmp_ent_get_non_ima_cmp_par4(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->non_ima.cmp_par_4_used);
}


/**
 * @brief get the used spillover threshold 5 parameter form the non-imagette
 *	specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used spillover threshold 5 parameter on success, 0 on error
 */

uint32_t cmp_ent_get_non_ima_spill5(struct cmp_entity *ent)
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
 * @brief get the used compression parameter 5 form the non-imagette specific
 *	compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used compression parameter 5 on success, 0 on error
 */

uint16_t cmp_ent_get_non_ima_cmp_par5(struct cmp_entity *ent)
{
	if (!ent)
		return 0;

	return be16_to_cpu(ent->non_ima.cmp_par_5_used);
}


/**
 * @brief get the used spillover threshold 6 parameter form the non-imagette
 *	specific compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used spillover threshold 6 parameter on success, 0 on error
 */

uint32_t cmp_ent_get_non_ima_spill6(struct cmp_entity *ent)
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
 * @brief get the used compression parameter 6 form the non-imagette specific
 *	compression entity header
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the used compression parameter 6 on success, 0 on error
 */

uint16_t cmp_ent_get_non_ima_cmp_par6(struct cmp_entity *ent)
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
	enum cmp_ent_data_type data_type;

	if (!ent)
		return NULL;

	data_type = cmp_ent_get_data_type(ent);

	switch (data_type) {
	case DATA_TYPE_IMAGETTE:
	case DATA_TYPE_F_CAM_IMAGETTE:
		return ent->ima.ima_cmp_dat;
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		return ent->ima.ap_ima_cmp_data;
	case DATA_TYPE_SAT_IMAGETTE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_OFFSET:
	case DATA_TYPE_BACKGROUND:
	case DATA_TYPE_SMEARING:
	case DATA_TYPE_S_FX:
	case DATA_TYPE_S_FX_DFX:
	case DATA_TYPE_S_FX_NCOB:
	case DATA_TYPE_S_FX_DFX_NCOB_ECOB:
	case DATA_TYPE_L_FX:
	case DATA_TYPE_L_FX_DFX:
	case DATA_TYPE_L_FX_NCOB:
	case DATA_TYPE_L_FX_DFX_NCOB_ECOB:
	case DATA_TYPE_F_FX:
	case DATA_TYPE_F_FX_DFX:
	case DATA_TYPE_F_FX_NCOB:
	case DATA_TYPE_F_FX_DFX_NCOB_ECOB:
	case DATA_TYPE_F_CAM_OFFSET:
	case DATA_TYPE_F_CAM_BACKGROUND:
		return ent->non_ima.cmp_data;
	case DATA_TYPE_UNKOWN:
		return NULL;
	}

	return NULL;
}


/**
 * @brief get the size of the compression entity header based on the set data
 *	product type in compression entity
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the size of the entity header in bytes on success, 0 on error
 */

static uint32_t cmp_ent_get_hdr_size(struct cmp_entity *ent)
{
	return cmp_ent_cal_hdr_size(cmp_ent_get_data_type(ent));
}


/**
 * @brief get the size of the compressed data based on the set data product
 *	type and compressed entity size in the compression entity
 *
 * @param ent	pointer to a compression entity
 *
 * @returns the size of the compressed data in bytes on success, 0 on error
 */

uint32_t cmp_ent_get_cmp_data_size(struct cmp_entity *ent)
{
	uint32_t cmp_ent_size, header_size;

	header_size = cmp_ent_get_hdr_size(ent);
	cmp_ent_size = cmp_ent_get_size(ent);

	if (header_size > cmp_ent_size)
		return 0;

	return cmp_ent_size - header_size;
}


/**
 * @brief set the parameters in the non-imagette specific compression
 *	entity header
 *
 * @param ent	pointer to a compression entity
 * @param info	decompression information structure
 *
 * @returns 0 on success, otherwise error
 *
 * @warning this functions is not implemented jet
 */

static int cmp_ent_write_non_ima_parameters(struct cmp_entity *ent, struct cmp_info *info)
{
	if (!info)
		return -1;
	(void)ent;

	printf("not implemented jet!\n");
	return -1;
#if 0
	if (cmp_ent_set_non_ima_spill1(ent, info->))
		return -1;
	if (cmp_ent_set_non_ima_cmp_par1(ent, info->))
		return -1;

	if (cmp_ent_set_non_ima_spill2(ent, info->))
		return -1;
	if (cmp_ent_set_non_ima_cmp_par2(ent, info->))
		return -1;

	if (cmp_ent_set_non_ima_spill3(ent, info->))
		return -1;
	if (cmp_ent_set_non_ima_cmp_par3(ent, info->))
		return -1;

	if (cmp_ent_set_non_ima_spill4(ent, info->))
		return -1;
	if (cmp_ent_set_non_ima_cmp_par4(ent, info->))
		return -1;

	if (cmp_ent_set_non_ima_spill5(ent, info->))
		return -1;
	if (cmp_ent_set_non_ima_cmp_par5(ent, info->))
		return -1;

	if (cmp_ent_set_non_ima_spill6(ent, info->))
		return -1;
	if (cmp_ent_set_non_ima_cmp_par6(ent, info->))
		return -1;
	return 0;
#endif
}


/**
 * @brief write the compression parameters in the entity header based on the data
 *	product type set in the entity and the provided decompression information
 *	struct
 *
 * @param ent	pointer to a compression entity
 * @param info	decompression information structure
 * @param cfg	compression configuration structure for adaptive compression
 *	parameters (can be NULL)
 *
 * @returns 0 on success, negative on error
 */

static int cmp_ent_write_cmp_pars(struct cmp_entity *ent, struct cmp_info *info,
				struct cmp_cfg *cfg)
{
	uint32_t ent_cmp_data_size;

	if (!info)
		return -1;

	ent_cmp_data_size = cmp_ent_get_cmp_data_size(ent);

	/* check if the entity can hold the compressed data */
	if (ent_cmp_data_size < cmp_bit_to_4byte(info->cmp_size))
		return -2;

	/* set compression parameter fields in the generic entity header */
	if (cmp_ent_set_original_size(ent, cmp_cal_size_of_data(info->samples_used,
								info->cmp_mode_used)))
		return -1;
	if (cmp_ent_set_cmp_mode(ent, info->cmp_mode_used))
		return -1;
	if (cmp_ent_set_data_type_raw_bit(ent, raw_mode_is_used(info->cmp_mode_used)))
		return -1;
	if (cmp_ent_set_model_value(ent, info->model_value_used))
		return -1;
	if (cmp_ent_set_lossy_cmp_par(ent, info->round_used))
		return -1;

	switch (cmp_ent_get_data_type(ent)) {
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		if (!cfg)
			return -1;
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
		if (cmp_ent_set_ima_spill(ent, info->spill_used))
			return -1;
		if (cmp_ent_set_ima_golomb_par(ent, info->golomb_par_used))
			return -1;
		break;
	case DATA_TYPE_OFFSET:
	case DATA_TYPE_BACKGROUND:
	case DATA_TYPE_SMEARING:
	case DATA_TYPE_S_FX:
	case DATA_TYPE_S_FX_DFX:
	case DATA_TYPE_S_FX_NCOB:
	case DATA_TYPE_S_FX_DFX_NCOB_ECOB:
	case DATA_TYPE_L_FX:
	case DATA_TYPE_L_FX_DFX:
	case DATA_TYPE_L_FX_NCOB:
	case DATA_TYPE_L_FX_DFX_NCOB_ECOB:
	case DATA_TYPE_F_FX:
	case DATA_TYPE_F_FX_DFX:
	case DATA_TYPE_F_FX_NCOB:
	case DATA_TYPE_F_FX_DFX_NCOB_ECOB:
	case DATA_TYPE_F_CAM_OFFSET:
	case DATA_TYPE_F_CAM_BACKGROUND:
		if (cmp_ent_write_non_ima_parameters(ent, info))
			return -1;
		break;
	case DATA_TYPE_UNKOWN: /* fall through */
	default:
		return -1;
	}

	return 0;
}


/**
 * @brief create a compression entity by setting the size of the
 * compression entity and the data product type in the entity header
 *
 * @param ent		pointer to a compression entity; if NULL, the function
 *	returns the needed size
 * @param data_type	compression entity data product type
 * @param cmp_size_byte	size of the compressed data in bytes
 *
 * @note if the entity size is smaller than the largest header, the function
 *	rounds up the entity size to the largest header
 *
 * @returns the size of the compression entity or 0 on error
 */

size_t cmp_ent_create(struct cmp_entity *ent, enum cmp_ent_data_type data_type,
		      uint32_t cmp_size_byte)
{
	int err;
	uint32_t hdr_size = cmp_ent_cal_hdr_size(data_type);
	size_t ent_size = hdr_size + cmp_size_byte;
	uint32_t ent_size_cpy = ent_size;

	if (!hdr_size)
		return 0;

	if (cmp_size_byte > CMP_ENTITY_MAX_SIZE)
		return 0;

	if (ent_size < sizeof(struct cmp_entity))
		ent_size = sizeof(struct cmp_entity);

	if (!ent)
		return ent_size;

	memset(ent, 0, hdr_size);

	err = cmp_ent_set_size(ent, ent_size_cpy);
	if (err)
		return 0;

	err = cmp_ent_set_data_type(ent, data_type);
	if (err)
		return 0;

	return ent_size;
}


/**
 * @brief create a compression entity and set the header fields
 *
 * @note this function simplifies the entity setup by creating a entity and
 *	setting the header fields in one function call
 *
 * @param ent			pointer to a compression entity; if NULL, the
 *	function returns the needed size
 * @param data_type		compression entity data product type
 * @param asw_version_id	applications software version identifier
 * @param start_time		compression start timestamp (coarse and fine)
 * @param end_time		compression end timestamp (coarse and fine)
 * @param model_id		model identifier
 * @param model_counter		model counter
 * @param info			decompression information structure
 * @param cfg			compression configuration structure for adaptive
 *	compression parameters (can be NULL)
 *
 * @returns the size of the compression entity or 0 on error
 */

size_t cmp_ent_build(struct cmp_entity *ent, enum cmp_ent_data_type data_type,
		     uint16_t asw_version_id, uint64_t start_time,
		     uint64_t end_time, uint16_t model_id, uint8_t model_counter,
		     struct cmp_info *info, struct cmp_cfg *cfg)
{
	uint32_t ent_size;

	if (!info)
		return 0;

	ent_size = cmp_ent_create(ent, data_type, cmp_bit_to_4byte(info->cmp_size));
	if (!ent_size)
		return 0;

	if (ent) {
		if (cmp_ent_set_asw_version_id(ent, asw_version_id))
			return 0;
		if (cmp_ent_set_start_timestamp(ent, start_time))
			return 0;
		if (cmp_ent_set_end_timestamp(ent, end_time))
			return 0;
		if (cmp_ent_set_model_id(ent, model_id))
			return 0;
		if (cmp_ent_set_model_counter(ent, model_counter))
			return 0;
		if (cmp_ent_write_cmp_pars(ent, info, cfg))
			return 0;
	}

	return ent_size;
}


/**
 * @brief read in read in a imagette compression entity header to a info struct
 *
 * @param ent			pointer to a compression entity
 * @param info			pointer to decompression information structure
 *	to store the read values
 *
 * @returns 0 on success; otherwise error
 */

int cmp_ent_read_imagette_header(struct cmp_entity *ent, struct cmp_info *info)
{
	uint32_t original_size;
	uint32_t sample_size;

	if (!ent)
		return -1;

	if (!info)
		return -1;

	info->cmp_mode_used = cmp_ent_get_cmp_mode(ent);
	info->model_value_used = cmp_ent_get_model_value_used(ent);
	info->round_used = cmp_ent_get_lossy_cmp_par(ent);
	info->spill_used = cmp_ent_get_ima_spill(ent);
	info->golomb_par_used = cmp_ent_get_ima_golomb_par(ent);
	info->cmp_size = cmp_ent_get_cmp_data_size(ent)*CHAR_BIT;

	sample_size = size_of_a_sample(info->cmp_mode_used);
	if (!sample_size) {
		info->samples_used = 0;
		return -1;
	}

	original_size = cmp_ent_get_original_size(ent);
	if (original_size % sample_size != 0) {
		fprintf(stderr, "Error: original_size and cmp_mode compression header field are not compatible.\n");
		info->samples_used = 0;
		return -1;
	}

	info->samples_used = original_size / sample_size;

	if (cmp_ent_get_data_type_raw_bit(ent) != raw_mode_is_used(info->cmp_mode_used)) {
		fprintf(stderr, "Error: The raw bit is set in Data Product Type Filed, but no raw compression mode is used.\n");
		return -1;
	}
	return 0;
}


#if 0
int cmp_ent_read_adaptive_imagette_header(struct cmp_entity *ent, struct cmp_info *info)
{
	if (!ent)
		return -1;

	if (!info)
		return -1;

	if (cmp_ent_get_imagette_header(ent, info))
		return -1;
	/* info->ap1_cmp_size = cmp_ent_get_ap1_cmp_size_bit(ent); */
	/* info->ap2_cmp_size = cmp_ent_get_ap2_cmp_size_bit(ent); */
	/* get ap1_spill and ap1/2 gpar*/

	return 0;
}
#endif


#ifdef HAS_TIME_H
/*
 * @brief Covert a calendar time expressed as a struct tm object to time since
 *	 epoch as a time_t object. The function interprets the input structure
 *	 as representing Universal Coordinated Time (UTC).
 * @note timegm is a GNU C Library extensions, not standardized. This function
 *	is used as a portable alternative
 * @note The function is thread-unsafe
 *
 * @param tm	pointer to a tm object specifying local calendar time to convert
 *
 * @returns time since epoch as a time_t object on success or -1 if time cannot be represented as a time_t object
 *
 * @see http://www.catb.org/esr/time-programming/#_unix_time_and_utc_gmt_zulu
 */

static time_t my_timegm(struct tm *tm)
{
#if defined(_WIN32) || defined(_WIN64)
	return _mkgmtime(tm);
#else
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
#endif
}

#endif


/**
 * @brief print the content of the compression entity header
 *
 * @param ent	pointer to a compression entity
 */

void cmp_ent_print_header(struct cmp_entity *ent)
{
	uint8_t *p = (uint8_t *)ent;
	uint32_t hdr_size = cmp_ent_get_hdr_size(ent);
	size_t i;

	for (i = 0; i < hdr_size; ++i) {
		printf("%02X ", p[i]);
		if (i && !((i+1) % 32))
			printf("\n");
	}
	printf("\n");
}


/**
 * @brief print the compressed data of the entity
 *
 * @param ent	pointer to a compression entity
 */

void cmp_ent_print_data(struct cmp_entity *ent)
{
	uint8_t *p = cmp_ent_get_data_buf(ent);
	size_t data_size = cmp_ent_get_cmp_data_size(ent);
	size_t i;

	if (!p)
		return;

	for (i = 0; i < data_size; ++i) {
		printf("%02X ", p[i]);
		if (i && !((i+1) % 32))
			printf("\n");
	}
	printf("\n");
}


/**
 * @brief print the entire compressed entity header plus data
 *
 * @param ent	pointer to a compression entity
 */

void cmp_ent_print(struct cmp_entity *ent)
{
	printf("compression entity header:\n");
	cmp_ent_print_header(ent);
	printf("compressed data in the compressed entity:\n");
	cmp_ent_print_data(ent);
}


/**
 * @brief parse the generic compressed entity header
 *
 * @param ent	pointer to a compression entity
 */

static void cmp_ent_parse_generic_header(struct cmp_entity *ent)
{
	uint32_t asw_version_id, cmp_ent_size, original_size, cmp_mode_used,
		 model_value_used, model_id, model_counter, lossy_cmp_par_used,
		 start_coarse_time, end_coarse_time;
	uint16_t start_fine_time, end_fine_time;
	enum cmp_ent_data_type data_type;
	int raw_bit;

	asw_version_id = cmp_ent_get_asw_version_id(ent);
	if (asw_version_id & CMP_TOOL_VERSION_ID_BIT) {
		uint16_t major = (asw_version_id & 0x7F00U) >> 8U;
		uint16_t minor = asw_version_id & 0x00FFU;
		printf("Compressed with cmp_tool version: %u.%02u\n", major,minor);
	} else
		printf("ICU ASW Version ID: %lu\n", asw_version_id);

	cmp_ent_size = cmp_ent_get_size(ent);
	printf("Compression Entity Size: %lu byte\n", cmp_ent_size);

	original_size = cmp_ent_get_original_size(ent);
	printf("Original Data Size: %lu byte\n", original_size);

	start_coarse_time = cmp_ent_get_coarse_start_time(ent);
	printf("Compression Coarse Start Time: %lu\n", start_coarse_time);

	start_fine_time = cmp_ent_get_fine_start_time(ent);
	printf("Compression Fine Start Time: %d\n", start_fine_time);

	end_coarse_time = cmp_ent_get_coarse_end_time(ent);
	printf("Compression Coarse End Time: %lu\n", end_coarse_time);

	end_fine_time = cmp_ent_get_fine_end_time(ent);
	printf("Compression Fine End Time: %d\n", end_fine_time);

#ifdef HAS_TIME_H
	{
		struct tm epoch_date = EPOCH_DATE;
		time_t time = my_timegm(&epoch_date) + start_coarse_time;
		printf("Data were compressed on (local time): %s", ctime(&time));
	}
#endif
	printf("The compression took %f second\n", end_coarse_time - start_coarse_time
		+ ((end_fine_time - start_fine_time)/256./256.));

	data_type = cmp_ent_get_data_type(ent);
	if (data_type != DATA_TYPE_UNKOWN)
		printf("Data Product Type: %d\n", data_type);
	else
		printf("Data Product Type: unknown!");

	raw_bit = cmp_ent_get_data_type_raw_bit(ent);
	printf("RAW bit in the Data Product Type is%s set", raw_bit ? "" : "not");

	cmp_mode_used = cmp_ent_get_cmp_mode(ent);
	printf("Used Compression Mode: %lu\n", cmp_mode_used);

	model_value_used = cmp_ent_get_model_value_used(ent);
	printf("Used Model Updating Weighing Value: %lu\n", model_value_used);

	model_id = cmp_ent_get_model_id(ent);
	printf("Model ID: %lu\n", model_id);

	model_counter = cmp_ent_get_model_counter(ent);
	printf("Model Counter: %lu\n", model_counter);

	lossy_cmp_par_used = cmp_ent_get_lossy_cmp_par(ent);
	printf("Used Lossy Compression Parameters: %lu\n", lossy_cmp_par_used);
}


/**
 * @brief parse the imagette specific compressed entity header
 *
 * @param ent	pointer to a compression entity
 */

static void cmp_ent_parese_imagette_header(struct cmp_entity *ent)
{
	uint32_t spill_used, golomb_par_used;

	printf("Not implemented yey!\n");
	spill_used = cmp_ent_get_ima_spill(ent);
	printf("Used Spillover Threshold Parameter: %lu\n", spill_used);

	golomb_par_used = cmp_ent_get_ima_golomb_par(ent);
	printf("Used Golomb Parameter: %lu\n", golomb_par_used);
}


/**
 * @brief parse the adaptive imagette specific compressed entity header
 *
 * @param ent	pointer to a compression entity
 */

static void cmp_ent_parese_adaptive_imagette_header(struct cmp_entity *ent)
{
	uint32_t spill_used, golomb_par_used, ap1_spill_used,
		 ap1_golomb_par_used, ap2_spill_used, ap2_golomb_par_used;

	spill_used = cmp_ent_get_ima_spill(ent);
	printf("Used Spillover Threshold Parameter: %lu\n", spill_used);

	golomb_par_used = cmp_ent_get_ima_golomb_par(ent);
	printf("Used Golomb Parameter: %lu\n", golomb_par_used);

	ap1_spill_used = cmp_ent_get_ima_ap1_spill(ent);
	printf("Used Adaptive 1 Spillover Threshold Parameter: %lu\n", ap1_spill_used);

	ap1_golomb_par_used = cmp_ent_get_ima_ap1_golomb_par(ent);
	printf("Used Adaptive 1 Golomb Parameter: %lu\n", ap1_golomb_par_used);

	ap2_spill_used = cmp_ent_get_ima_ap2_spill(ent);
	printf("Used Adaptive 2 Spillover Threshold Parameter: %lu\n", ap2_spill_used);

	ap2_golomb_par_used = cmp_ent_get_ima_ap2_golomb_par(ent);
	printf("Used Adaptive 2 Golomb Parameter: %lu\n", ap2_golomb_par_used);
}


/**
 * @brief parse the specific compressed entity header
 *
 * @param ent	pointer to a compression entity
 */

static void cmp_ent_parese_specific_header(struct cmp_entity *ent)
{
	enum cmp_ent_data_type data_type = cmp_ent_get_data_type(ent);

	switch (data_type) {
	case DATA_TYPE_IMAGETTE:
		cmp_ent_parese_imagette_header(ent);
		break;
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
		cmp_ent_parese_adaptive_imagette_header(ent);
		break;
	default:
		printf("Data Product Type not supported!\n");
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

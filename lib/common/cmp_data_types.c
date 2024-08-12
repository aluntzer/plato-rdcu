/**
 * @file    cmp_data_types.c
 * @author  Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date    2020
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
 * @brief collection of functions to handle the different compression data types
 */


#include <stdint.h>
#include <limits.h>

#include "byteorder.h"
#include "cmp_debug.h"
#include "cmp_support.h"
#include "cmp_data_types.h"


#ifdef __BIG_ENDIAN
#  define CMP_IS_BIG_ENDIAN 1
#else
#  define CMP_IS_BIG_ENDIAN 0
#endif

/**
 * @brief get the collection timestamp from the collection header
 *
 * @param col	pointer to a collection header
 *
 * @returns the collection timestamp
 */

uint64_t cmp_col_get_timestamp(const struct collection_hdr *col)
{
#ifdef __LITTLE_ENDIAN
	return be64_to_cpu(col->timestamp) >> 16;
#else
	return col->timestamp;
#endif /* __LITTLE_ENDIAN */
}


/**
 * @brief get the configuration identifier from the collection header
 *
 * @param col	pointer to a collection header
 *
 * @returns the configuration identifier
 */

uint16_t cmp_col_get_configuration_id(const struct collection_hdr *col)
{
	return be16_to_cpu(col->configuration_id);
}


/**
 * @brief get the collection identifier from the collection header
 *
 * @param col	pointer to a collection header
 *
 * @returns the collection identifier
 */

uint16_t cmp_col_get_col_id(const struct collection_hdr *col)
{
	return be16_to_cpu(col->collection_id);
}


/**
 * @brief get the packet type bit in the collection identifier field of the
 *	collection header
 *
 * @param col	pointer to a collection header
 *
 * @returns the collection packet type a collection; 1 for science packets and
 *	0 for window packets
 *
 */

uint8_t cmp_col_get_pkt_type(const struct collection_hdr *col)
{
	union collection_id cid;

	cid.collection_id = be16_to_cpu(col->collection_id);
	return cid.field.pkt_type;
}


/**
 * @brief get the subservice field in the collection identifier field of the
 *	collection header
 *
 * @param col	pointer to a collection header
 *
 * @returns the collection subservice type
 */

uint8_t cmp_col_get_subservice(const struct collection_hdr *col)
{
	union collection_id cid;

	cid.collection_id = be16_to_cpu(col->collection_id);
	return cid.field.subservice;
}


/**
 * @brief get the CCD identifier field in the collection identifier field of
 *	the collection header
 *
 * @param col	pointer to a collection header
 *
 * @returns the collection CCD identifier
 */

uint8_t cmp_col_get_ccd_id(const struct collection_hdr *col)
{
	union collection_id cid;

	cid.collection_id = be16_to_cpu(col->collection_id);
	return cid.field.ccd_id;
}


/**
 * @brief get the sequence number field in the collection identifier field of
 *	the collection header
 *
 * @param col	pointer to a collection header
 *
 * @returns the collection sequence number
 */

uint8_t cmp_col_get_sequence_num(const struct collection_hdr *col)
{
	union collection_id cid;

	cid.collection_id = be16_to_cpu(col->collection_id);
	return cid.field.sequence_num;
}


/**
 * @brief get the collection length from the collection header
 *
 * @param col	pointer to a collection header
 *
 * @returns the collection length in bytes
 */

uint16_t cmp_col_get_data_length(const struct collection_hdr *col)
{
	return be16_to_cpu(col->collection_length);
}


/**
 * @brief get the entire collection size (header plus data size)
 *
 * @param col	pointer to a collection header
 *
 * @returns the collection size in bytes
 */

uint32_t cmp_col_get_size(const struct collection_hdr *col)
{
	return COLLECTION_HDR_SIZE + cmp_col_get_data_length(col);
}


/**
 * @brief set the timestamp in the collection header
 *
 * @param col		pointer to a collection header
 * @param timestamp	collection timestamp (coarse and fine)
 *
 * @returns 0 on success, otherwise error
 */

int cmp_col_set_timestamp(struct collection_hdr *col, uint64_t timestamp)
{
	if (!col)
		return -1;
	if (timestamp >> 48)
		return -1;

#ifdef __LITTLE_ENDIAN
	col->timestamp = cpu_to_be64(timestamp) >> 16;
#else
	col->timestamp = timestamp;
#endif /* __LITTLE_ENDIAN */

	return 0;
}


/**
 * @brief set the configuration identifier in the collection header
 *
 * @param col			pointer to a collection header
 * @param configuration_id	configuration identifier
 *
 * @returns 0 on success, otherwise error
 */

int cmp_col_set_configuration_id(struct collection_hdr *col, uint16_t configuration_id)
{
	if (!col)
		return 1;

	col->configuration_id = cpu_to_be16(configuration_id);
	return 0;
}


/**
 * @brief set the collection identifier in the collection header
 *
 * @param col		pointer to a collection header
 * @param collection_id	collection identifier
 *
 * @returns 0 on success, otherwise error
 */

int cmp_col_set_col_id(struct collection_hdr *col, uint16_t collection_id)
{
	if (!col)
		return -1;

	col->collection_id = cpu_to_be16(collection_id);
	return 0;
}


/**
 * @brief set the packet type bit in the collection identifier field of the
 *	collection header
 *
 * @param col		pointer to a collection header
 * @param pkt_type	packet type bit; 1 for science packets and 0 for window packets
 *
 * @returns 0 on success, otherwise error
 */

int cmp_col_set_pkt_type(struct collection_hdr *col, uint8_t pkt_type)
{
	union collection_id cid;

	if (!col)
		return -1;
	if (pkt_type >> 1)
		return -1;

	cid.collection_id = be16_to_cpu(col->collection_id);
	cid.field.pkt_type = pkt_type;
	cmp_col_set_col_id(col, cid.collection_id);
	return 0;
}


/**
 * @brief set the packet subservice field in the collection identifier field of
 *	the collection header
 *
 * @param col		pointer to a collection header
 * @param subservice	collection subservice type
 *
 * @returns 0 on success, otherwise error
 */

int cmp_col_set_subservice(struct collection_hdr *col, uint8_t subservice)
{
	union collection_id cid;

	if (!col)
		return -1;
	if (subservice >> 6)
		return -1;

	cid.collection_id = be16_to_cpu(col->collection_id);
	cid.field.subservice = subservice;
	cmp_col_set_col_id(col, cid.collection_id);
	return 0;
}


/**
 * @brief set the packet CCD identifier field in the collection identifier field
 *	of the collection header
 *
 * @param col		pointer to a collection header
 * @param ccd_id	collection CCD identifier
 *
 * @returns 0 on success, otherwise error
 */

int cmp_col_set_ccd_id(struct collection_hdr *col, uint8_t ccd_id)
{
	union collection_id cid;

	if (!col)
		return -1;
	if (ccd_id >> 2)
		return -1;

	cid.collection_id = be16_to_cpu(col->collection_id);
	cid.field.ccd_id = ccd_id;
	cmp_col_set_col_id(col, cid.collection_id);
	return 0;
}


/**
 * @brief set the collection sequence number bit field in the collection
 *	identifier of the collection header
 *
 * @param col		pointer to a collection header
 * @param sequence_num	collection sequence number
 *
 * @returns 0 on success, otherwise error
 */

int cmp_col_set_sequence_num(struct collection_hdr *col, uint8_t sequence_num)
{
	union collection_id cid;

	if (!col)
		return -1;

	if (sequence_num >> 7)
		return -1;

	cid.collection_id = be16_to_cpu(col->collection_id);
	cid.field.sequence_num = sequence_num;
	return cmp_col_set_col_id(col, cid.collection_id);
}


/**
 * @brief set the collection length in the collection header
 *
 * @param col		pointer to a collection header
 * @param length	length of the collection in bytes TBC: without the
 *			header size itself
 *
 * @returns 0 on success, otherwise error
 */

int cmp_col_set_data_length(struct collection_hdr *col, uint16_t length)
{
	if (!col)
		return -1;

	col->collection_length = cpu_to_be16(length);
	return 0;
}


/**
 * @brief converts a subservice to its associated compression data type
 *
 * @param subservice	collection subservice type
 *
 * @returns the converted compression data type; DATA_TYPE_UNKNOWN if the
 *	subservice is unknown
 */

enum cmp_data_type convert_subservice_to_cmp_data_type(uint8_t subservice)
{
	switch (subservice) {
	case SST_NCxx_S_SCIENCE_IMAGETTE:
		return DATA_TYPE_IMAGETTE;
	case SST_NCxx_S_SCIENCE_SAT_IMAGETTE:
		return DATA_TYPE_SAT_IMAGETTE;
	case SST_NCxx_S_SCIENCE_OFFSET:
		return DATA_TYPE_OFFSET;
	case SST_NCxx_S_SCIENCE_BACKGROUND:
		return DATA_TYPE_BACKGROUND;
	case SST_NCxx_S_SCIENCE_SMEARING:
		return DATA_TYPE_SMEARING;
	case SST_NCxx_S_SCIENCE_S_FX:
		return DATA_TYPE_S_FX;
	case SST_NCxx_S_SCIENCE_S_FX_EFX:
		return DATA_TYPE_S_FX_EFX;
	case SST_NCxx_S_SCIENCE_S_FX_NCOB:
		return DATA_TYPE_S_FX_NCOB;
	case SST_NCxx_S_SCIENCE_S_FX_EFX_NCOB_ECOB:
		return DATA_TYPE_S_FX_EFX_NCOB_ECOB;
	case SST_NCxx_S_SCIENCE_L_FX:
		return DATA_TYPE_L_FX;
	case SST_NCxx_S_SCIENCE_L_FX_EFX:
		return DATA_TYPE_L_FX_EFX;
	case SST_NCxx_S_SCIENCE_L_FX_NCOB:
		return DATA_TYPE_L_FX_NCOB;
	case SST_NCxx_S_SCIENCE_L_FX_EFX_NCOB_ECOB:
		return DATA_TYPE_L_FX_EFX_NCOB_ECOB;
	case SST_NCxx_S_SCIENCE_F_FX:
		return DATA_TYPE_F_FX;
	case SST_NCxx_S_SCIENCE_F_FX_EFX:
		return DATA_TYPE_F_FX_EFX;
	case SST_NCxx_S_SCIENCE_F_FX_NCOB:
		return DATA_TYPE_F_FX_NCOB;
	case SST_NCxx_S_SCIENCE_F_FX_EFX_NCOB_ECOB:
		return DATA_TYPE_F_FX_EFX_NCOB_ECOB;
	case SST_FCx_S_SCIENCE_IMAGETTE:
		return DATA_TYPE_F_CAM_IMAGETTE;
	case SST_FCx_S_SCIENCE_OFFSET_VALUES:
		return DATA_TYPE_F_CAM_OFFSET;
	case SST_FCx_S_BACKGROUND_VALUES:
		return DATA_TYPE_F_CAM_BACKGROUND;
	default:
		return DATA_TYPE_UNKNOWN;
	};
}


/**
 * @brief converts a compression data type to its associated subservice.
 *
 * @param data_type	compression data type
 *
 * @returns the converted subservice; -1 if the data type is unknown.
 */

uint8_t convert_cmp_data_type_to_subservice(enum cmp_data_type data_type)
{
	uint8_t sst = 0;

	switch (data_type) {
	case DATA_TYPE_IMAGETTE:
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
		sst = SST_NCxx_S_SCIENCE_IMAGETTE;
		break;
	case DATA_TYPE_SAT_IMAGETTE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
		sst = SST_NCxx_S_SCIENCE_SAT_IMAGETTE;
		break;
	case DATA_TYPE_OFFSET:
		sst = SST_NCxx_S_SCIENCE_OFFSET;
		break;
	case DATA_TYPE_BACKGROUND:
		sst = SST_NCxx_S_SCIENCE_BACKGROUND;
		break;
	case DATA_TYPE_SMEARING:
		sst = SST_NCxx_S_SCIENCE_SMEARING;
		break;
	case DATA_TYPE_S_FX:
		sst = SST_NCxx_S_SCIENCE_S_FX;
		break;
	case DATA_TYPE_S_FX_EFX:
		sst = SST_NCxx_S_SCIENCE_S_FX_EFX;
		break;
	case DATA_TYPE_S_FX_NCOB:
		sst = SST_NCxx_S_SCIENCE_S_FX_NCOB;
		break;
	case DATA_TYPE_S_FX_EFX_NCOB_ECOB:
		sst = SST_NCxx_S_SCIENCE_S_FX_EFX_NCOB_ECOB;
		break;
	case DATA_TYPE_L_FX:
		sst = SST_NCxx_S_SCIENCE_L_FX;
		break;
	case DATA_TYPE_L_FX_EFX:
		sst = SST_NCxx_S_SCIENCE_L_FX_EFX;
		break;
	case DATA_TYPE_L_FX_NCOB:
		sst = SST_NCxx_S_SCIENCE_L_FX_NCOB;
		break;
	case DATA_TYPE_L_FX_EFX_NCOB_ECOB:
		sst = SST_NCxx_S_SCIENCE_L_FX_EFX_NCOB_ECOB;
		break;
	case DATA_TYPE_F_FX:
		sst = SST_NCxx_S_SCIENCE_F_FX;
		break;
	case DATA_TYPE_F_FX_EFX:
		sst = SST_NCxx_S_SCIENCE_F_FX_EFX;
		break;
	case DATA_TYPE_F_FX_NCOB:
		sst = SST_NCxx_S_SCIENCE_F_FX_NCOB;
		break;
	case DATA_TYPE_F_FX_EFX_NCOB_ECOB:
		sst = SST_NCxx_S_SCIENCE_F_FX_EFX_NCOB_ECOB;
		break;
	case DATA_TYPE_F_CAM_IMAGETTE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		sst = SST_FCx_S_SCIENCE_IMAGETTE;
		break;
	case DATA_TYPE_F_CAM_OFFSET:
		sst = SST_FCx_S_SCIENCE_OFFSET_VALUES;
		break;
	case DATA_TYPE_F_CAM_BACKGROUND:
		sst = SST_FCx_S_BACKGROUND_VALUES;
		break;
	default:
	case DATA_TYPE_UNKNOWN:
		debug_print("Error: Unknown compression data type!");
		sst = (uint8_t)-1;
	};

	return sst;
}


/**
 * @brief calculate the size of a sample for the different compression data type
 *
 * @param data_type	compression data_type
 *
 * @returns the size of a data sample in bytes for the selected compression
 *	data type; zero on unknown data type
 */

size_t size_of_a_sample(enum cmp_data_type data_type)
{
	size_t sample_size = 0;

	switch (data_type) {
	case DATA_TYPE_IMAGETTE:
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_SAT_IMAGETTE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_F_CAM_IMAGETTE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		sample_size = sizeof(uint16_t);
		break;
	case DATA_TYPE_OFFSET:
	case DATA_TYPE_F_CAM_OFFSET:
		sample_size = sizeof(struct offset);
		break;
	case DATA_TYPE_BACKGROUND:
	case DATA_TYPE_F_CAM_BACKGROUND:
		sample_size = sizeof(struct background);
		break;
	case DATA_TYPE_SMEARING:
		sample_size = sizeof(struct smearing);
		break;
	case DATA_TYPE_S_FX:
		sample_size = sizeof(struct s_fx);
		break;
	case DATA_TYPE_S_FX_EFX:
		sample_size = sizeof(struct s_fx_efx);
		break;
	case DATA_TYPE_S_FX_NCOB:
		sample_size = sizeof(struct s_fx_ncob);
		break;
	case DATA_TYPE_S_FX_EFX_NCOB_ECOB:
		sample_size = sizeof(struct s_fx_efx_ncob_ecob);
		break;
	case DATA_TYPE_L_FX:
		sample_size = sizeof(struct l_fx);
		break;
	case DATA_TYPE_L_FX_EFX:
		sample_size = sizeof(struct l_fx_efx);
		break;
	case DATA_TYPE_L_FX_NCOB:
		sample_size = sizeof(struct l_fx_ncob);
		break;
	case DATA_TYPE_L_FX_EFX_NCOB_ECOB:
		sample_size = sizeof(struct l_fx_efx_ncob_ecob);
		break;
	case DATA_TYPE_F_FX:
		sample_size = sizeof(struct f_fx);
		break;
	case DATA_TYPE_F_FX_EFX:
		sample_size = sizeof(struct f_fx_efx);
		break;
	case DATA_TYPE_F_FX_NCOB:
		sample_size = sizeof(struct f_fx_ncob);
		break;
	case DATA_TYPE_F_FX_EFX_NCOB_ECOB:
		sample_size = sizeof(struct f_fx_efx_ncob_ecob);
		break;
	case DATA_TYPE_UNKNOWN:
	default:
		debug_print("Error: Compression data type is not supported.");
		break;
	}
	return sample_size;
}


static uint32_t be24_to_cpu(uint32_t a)
{
#ifdef __LITTLE_ENDIAN
	return be32_to_cpu(a) >> 8;
#else
	return a;
#endif /* __LITTLE_ENDIAN */
}


static void be_to_cpus_16(uint16_t *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i) {
		uint16_t tmp;

		tmp = be16_to_cpu(get_unaligned(&a[i]));
		put_unaligned(tmp, &a[i]);
	}
}


static void be_to_cpus_offset(struct offset *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i) {
		a[i].mean = be32_to_cpu(a[i].mean);
		a[i].variance = be32_to_cpu(a[i].variance);
	}
}


static void be_to_cpus_background(struct background *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i) {
		a[i].mean = be32_to_cpu(a[i].mean);
		a[i].variance = be32_to_cpu(a[i].variance);
		a[i].outlier_pixels = be16_to_cpu(a[i].outlier_pixels);
	}
}


static void be_to_cpus_smearing(struct smearing *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i) {
		a[i].mean = be32_to_cpu(a[i].mean);
		a[i].variance_mean = be16_to_cpu(a[i].variance_mean);
		a[i].outlier_pixels = be16_to_cpu(a[i].outlier_pixels);
	}
}


static void be_to_cpus_s_fx(struct s_fx *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i)
		a[i].fx = be32_to_cpu(a[i].fx);
}


static void be_to_cpus_s_fx_efx(struct s_fx_efx *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i) {
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].efx = be32_to_cpu(a[i].efx);
	}
}


static void be_to_cpus_s_fx_ncob(struct s_fx_ncob *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i) {
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].ncob_x = be32_to_cpu(a[i].ncob_x);
		a[i].ncob_y = be32_to_cpu(a[i].ncob_y);
	}
}


static void be_to_cpus_s_fx_efx_ncob_ecob(struct s_fx_efx_ncob_ecob *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i) {
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].ncob_x = be32_to_cpu(a[i].ncob_x);
		a[i].ncob_y = be32_to_cpu(a[i].ncob_y);
		a[i].efx = be32_to_cpu(a[i].efx);
		a[i].ecob_x = be32_to_cpu(a[i].ecob_x);
		a[i].ecob_y = be32_to_cpu(a[i].ecob_y);
	}
}


static void be_to_cpus_l_fx(struct l_fx *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i) {
		a[i].exp_flags = be24_to_cpu(a[i].exp_flags);
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].fx_variance = be32_to_cpu(a[i].fx_variance);
	}
}


static void be_to_cpus_l_fx_efx(struct l_fx_efx *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i) {
		a[i].exp_flags = be24_to_cpu(a[i].exp_flags);
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].efx = be32_to_cpu(a[i].efx);
		a[i].fx_variance = be32_to_cpu(a[i].fx_variance);
	}
}


static void be_to_cpus_l_fx_ncob(struct l_fx_ncob *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i) {
		a[i].exp_flags = be24_to_cpu(a[i].exp_flags);
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].ncob_x = be32_to_cpu(a[i].ncob_x);
		a[i].ncob_y = be32_to_cpu(a[i].ncob_y);
		a[i].fx_variance = be32_to_cpu(a[i].fx_variance);
		a[i].cob_x_variance = be32_to_cpu(a[i].cob_x_variance);
		a[i].cob_y_variance = be32_to_cpu(a[i].cob_y_variance);
	}
}


static void be_to_cpus_l_fx_efx_ncob_ecob(struct l_fx_efx_ncob_ecob *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i) {
		a[i].exp_flags = be24_to_cpu(a[i].exp_flags);
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].ncob_x = be32_to_cpu(a[i].ncob_x);
		a[i].ncob_y = be32_to_cpu(a[i].ncob_y);
		a[i].efx = be32_to_cpu(a[i].efx);
		a[i].ecob_x = be32_to_cpu(a[i].ecob_x);
		a[i].ecob_y = be32_to_cpu(a[i].ecob_y);
		a[i].fx_variance = be32_to_cpu(a[i].fx_variance);
		a[i].cob_x_variance = be32_to_cpu(a[i].cob_x_variance);
		a[i].cob_y_variance = be32_to_cpu(a[i].cob_y_variance);
	}
}


static void be_to_cpus_f_fx(struct f_fx *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i)
		a[i].fx = be32_to_cpu(a[i].fx);
}


static void be_to_cpus_f_fx_efx(struct f_fx_efx *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i) {
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].efx = be32_to_cpu(a[i].efx);
	}
}


static void be_to_cpus_f_fx_ncob(struct f_fx_ncob *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i) {
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].ncob_x = be32_to_cpu(a[i].ncob_x);
		a[i].ncob_y = be32_to_cpu(a[i].ncob_y);
	}
}


static void be_to_cpus_f_fx_efx_ncob_ecob(struct f_fx_efx_ncob_ecob *a, uint32_t samples)
{
	uint32_t i;

	for (i = 0; i < samples; ++i) {
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].ncob_x = be32_to_cpu(a[i].ncob_x);
		a[i].ncob_y = be32_to_cpu(a[i].ncob_y);
		a[i].efx = be32_to_cpu(a[i].efx);
		a[i].ecob_x = be32_to_cpu(a[i].ecob_x);
		a[i].ecob_y = be32_to_cpu(a[i].ecob_y);
	}
}


/**
 * @brief swaps the endianness of (collection) data from big endian to the CPU
 *	endianness (or vice versa) in place.
 * @note if you want to swap the data of a whole collection, including a
 *	collection header or a chunk of collections use the be_to_cpu_chunk() or
 *	cpu_to_be_chunk() functions
 *
 * @param data			a pointer to the data to swap (not including a
 *				collection header); can be NULL
 * @param data_size_byte	size of the data in bytes
 * @param data_type		compression data type
 *
 * @returns 0 on success; -1 on failure
 */

int be_to_cpu_data_type(void *data, uint32_t data_size_byte, enum cmp_data_type data_type)
{
	uint32_t sample_size = (uint32_t)size_of_a_sample(data_type);
	uint32_t samples;

	if (!data) /* nothing to do */
		return 0;

	if (!sample_size)
		return -1;

	if (data_size_byte % sample_size) {
		debug_print("Error: Can not convert data size in samples.");
		return -1;
	}
	samples = data_size_byte / sample_size;

	if (CMP_IS_BIG_ENDIAN)
		return 0;

	switch (data_type) {
	case DATA_TYPE_IMAGETTE:
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_SAT_IMAGETTE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_F_CAM_IMAGETTE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		be_to_cpus_16(data, samples);
		break;
	case DATA_TYPE_OFFSET:
	case DATA_TYPE_F_CAM_OFFSET:
		be_to_cpus_offset(data, samples);
		break;
	case DATA_TYPE_BACKGROUND:
	case DATA_TYPE_F_CAM_BACKGROUND:
		be_to_cpus_background(data, samples);
		break;
	case DATA_TYPE_SMEARING:
		be_to_cpus_smearing(data, samples);
		break;
	case DATA_TYPE_S_FX:
		be_to_cpus_s_fx(data, samples);
		break;
	case DATA_TYPE_S_FX_EFX:
		be_to_cpus_s_fx_efx(data, samples);
		break;
	case DATA_TYPE_S_FX_NCOB:
		be_to_cpus_s_fx_ncob(data, samples);
		break;
	case DATA_TYPE_S_FX_EFX_NCOB_ECOB:
		be_to_cpus_s_fx_efx_ncob_ecob(data, samples);
		break;
	case DATA_TYPE_L_FX:
		be_to_cpus_l_fx(data, samples);
		break;
	case DATA_TYPE_L_FX_EFX:
		be_to_cpus_l_fx_efx(data, samples);
		break;
	case DATA_TYPE_L_FX_NCOB:
		be_to_cpus_l_fx_ncob(data, samples);
		break;
	case DATA_TYPE_L_FX_EFX_NCOB_ECOB:
		be_to_cpus_l_fx_efx_ncob_ecob(data, samples);
		break;
	case DATA_TYPE_F_FX:
		be_to_cpus_f_fx(data, samples);
		break;
	case DATA_TYPE_F_FX_EFX:
		be_to_cpus_f_fx_efx(data, samples);
		break;
	case DATA_TYPE_F_FX_NCOB:
		be_to_cpus_f_fx_ncob(data, samples);
		break;
	case DATA_TYPE_F_FX_EFX_NCOB_ECOB:
		be_to_cpus_f_fx_efx_ncob_ecob(data, samples);
		break;
	/* LCOV_EXCL_START */
	case DATA_TYPE_UNKNOWN:
	default:
		debug_print("Error: Can not swap endianness for this compression data type.");
		return -1;
	/* LCOV_EXCL_STOP */
	}

	return 0;
}


/**
 * @brief swaps the endianness of chunk data from big endian to the CPU
 *	endianness (or vice versa) in place
 * @note the endianness of the collection header is not changed!
 *
 * @param chunk		pointer to a chunk of collections (can be NULL)
 * @param chunk_size	size in bytes of the chunk
 *
 * @returns 0 on success; -1 on failure
 */

int be_to_cpu_chunk(uint8_t *chunk, size_t chunk_size)
{
	uint8_t *col_p = chunk;

	if (!chunk) /* nothing to do */
		return 0;

	if (chunk_size < COLLECTION_HDR_SIZE)
		return -1;

	while (col_p <= chunk + chunk_size - COLLECTION_HDR_SIZE) {
		struct collection_hdr *col_hdr = (struct collection_hdr *)col_p;
		enum cmp_data_type data_type = convert_subservice_to_cmp_data_type(cmp_col_get_subservice(col_hdr));
		uint32_t data_size = cmp_col_get_data_length(col_hdr);

		col_p += cmp_col_get_size(col_hdr);
		if (col_p > chunk + chunk_size)  /* over read chunk? */
			break;

		if (be_to_cpu_data_type(col_hdr->entry, data_size, data_type))
			return -1;
	}

	if (col_p != chunk + chunk_size) {
		debug_print("Error: The chunk size does not match the sum of the collection sizes.");
		return -1;
	}

	return 0;
}


/**
 * @brief swap the endianness of uncompressed data from big endian to the cpu
 *	endianness (or the other way around) in place
 *
 * @param data			pointer to a data sample
 * @param data_size_byte	size of the data in bytes
 * @param data_type		compression data type
 *
 * @returns 0 on success; non-zero on failure
 */

int cmp_input_big_to_cpu_endianness(void *data, uint32_t data_size_byte,
				    enum cmp_data_type data_type)
{
	if (data && !rdcu_supported_data_type_is_used(data_type)) {
		/* skip collection header for non RDCU data types */
		data = (uint8_t *)data + COLLECTION_HDR_SIZE;
		if (data_size_byte < COLLECTION_HDR_SIZE)
			return -1;
		data_size_byte -= COLLECTION_HDR_SIZE;
	}
	return be_to_cpu_data_type(data, data_size_byte, data_type);
}

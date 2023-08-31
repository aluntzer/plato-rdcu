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


#include <cmp_support.h>
#include <cmp_data_types.h>
#include <cmp_debug.h>
#include <byteorder.h>



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
		sample_size = sizeof(struct nc_offset);
		break;
	case DATA_TYPE_BACKGROUND:
		sample_size = sizeof(struct nc_background);
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
	case DATA_TYPE_F_CAM_OFFSET:
	case DATA_TYPE_F_CAM_BACKGROUND:
	case DATA_TYPE_UNKNOWN:
	default:
		debug_print("Error: Compression data type is not supported.\n");
		break;
	}
	return sample_size;
}


/**
 * @brief calculate the need bytes for the data
 *
 * @param samples	number of data samples
 * @param data_type	compression data_type
 *
 * @note for non-imagette data program types the multi entry header size is added
 *
 * @returns the size in bytes to store the data sample; zero on failure
 */

uint32_t cmp_cal_size_of_data(uint32_t samples, enum cmp_data_type data_type)
{
	size_t s = size_of_a_sample(data_type);
	uint64_t x; /* use 64 bit to catch overflow */

	if (!s)
		return 0;

	x = (uint64_t)s*samples;

	if (!rdcu_supported_data_type_is_used(data_type))
		x += MULTI_ENTRY_HDR_SIZE;

	if (x > UINT_MAX) /* catch overflow */
		return 0;

	return (unsigned int)x;
}


/**
 * @brief calculates the number of samples for a given data size for the
 *	different compression modes
 *
 * @param size		size of the data in bytes
 * @param data_type	compression data type
 *
 * @returns the number samples for the given compression mode; negative on error
 */

int32_t cmp_input_size_to_samples(uint32_t size, enum cmp_data_type data_type)
{
	uint32_t samples_size = (uint32_t)size_of_a_sample(data_type);

	if (!samples_size)
		return -1;

	if (!rdcu_supported_data_type_is_used(data_type)) {
		if (size < MULTI_ENTRY_HDR_SIZE)
			return -1;
		size -= MULTI_ENTRY_HDR_SIZE;
	}

	if (size % samples_size)
		return -1;

	return (int)(size/samples_size);
}


static uint32_t be24_to_cpu(uint32_t a)
{
	return be32_to_cpu(a) >> 8;
}


static void be_to_cpus_16(uint16_t *a, int samples)
{
	int i;

	for (i = 0; i < samples; ++i)
		be16_to_cpus(&a[i]);
}


static void be_to_cpus_nc_offset(struct nc_offset *a, int samples)
{
	int i;

	for (i = 0; i < samples; ++i) {
		a[i].mean = be32_to_cpu(a[i].mean);
		a[i].variance = be32_to_cpu(a[i].variance);
	}
}


static void be_to_cpus_nc_background(struct nc_background *a, int samples)
{
	int i;

	for (i = 0; i < samples; ++i) {
		a[i].mean = be32_to_cpu(a[i].mean);
		a[i].variance = be32_to_cpu(a[i].variance);
		a[i].outlier_pixels = be16_to_cpu(a[i].outlier_pixels);
	}
}


static void be_to_cpus_smearing(struct smearing *a, int samples)
{
	int i;

	for (i = 0; i < samples; ++i) {
		a[i].mean = be32_to_cpu(a[i].mean);
		a[i].variance_mean = be16_to_cpu(a[i].variance_mean);
		a[i].outlier_pixels = be16_to_cpu(a[i].outlier_pixels);
	}
}


static void be_to_cpus_s_fx(struct s_fx *a, int samples)
{
	int i;

	for (i = 0; i < samples; ++i)
		a[i].fx = be32_to_cpu(a[i].fx);
}


static void be_to_cpus_s_fx_efx(struct s_fx_efx *a, int samples)
{
	int i;

	for (i = 0; i < samples; ++i) {
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].efx = be32_to_cpu(a[i].efx);
	}
}


static void be_to_cpus_s_fx_ncob(struct s_fx_ncob *a, int samples)
{
	int i;

	for (i = 0; i < samples; ++i) {
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].ncob_x = be32_to_cpu(a[i].ncob_x);
		a[i].ncob_y = be32_to_cpu(a[i].ncob_y);
	}
}


static void be_to_cpus_s_fx_efx_ncob_ecob(struct s_fx_efx_ncob_ecob *a, int samples)
{
	int i;

	for (i = 0; i < samples; ++i) {
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].ncob_x = be32_to_cpu(a[i].ncob_x);
		a[i].ncob_y = be32_to_cpu(a[i].ncob_y);
		a[i].efx = be32_to_cpu(a[i].efx);
		a[i].ecob_x = be32_to_cpu(a[i].ecob_x);
		a[i].ecob_y = be32_to_cpu(a[i].ecob_y);
	}
}


static void be_to_cpus_l_fx(struct l_fx *a, int samples)
{
	int i;

	for (i = 0; i < samples; ++i) {
		a[i].exp_flags = be24_to_cpu(a[i].exp_flags);
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].fx_variance = be32_to_cpu(a[i].fx_variance);
	}
}


static void be_to_cpus_l_fx_efx(struct l_fx_efx *a, int samples)
{
	int i;

	for (i = 0; i < samples; ++i) {
		a[i].exp_flags = be24_to_cpu(a[i].exp_flags);
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].efx = be32_to_cpu(a[i].efx);
		a[i].fx_variance = be32_to_cpu(a[i].fx_variance);
	}
}


static void be_to_cpus_l_fx_ncob(struct l_fx_ncob *a, int samples)
{
	int i;

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


static void be_to_cpus_l_fx_efx_ncob_ecob(struct l_fx_efx_ncob_ecob *a, int samples)
{
	int i;

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


static void be_to_cpus_f_fx(struct f_fx *a, int samples)
{
	int i;

	for (i = 0; i < samples; ++i)
		a[i].fx = be32_to_cpu(a[i].fx);
}


static void be_to_cpus_f_fx_efx(struct f_fx_efx *a, int samples)
{
	int i;

	for (i = 0; i < samples; ++i) {
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].efx = be32_to_cpu(a[i].efx);
	}
}


static void be_to_cpus_f_fx_ncob(struct f_fx_ncob *a, int samples)
{
	int i;

	for (i = 0; i < samples; ++i) {
		a[i].fx = be32_to_cpu(a[i].fx);
		a[i].ncob_x = be32_to_cpu(a[i].ncob_x);
		a[i].ncob_y = be32_to_cpu(a[i].ncob_y);
	}
}


static void be_to_cpus_f_fx_efx_ncob_ecob(struct f_fx_efx_ncob_ecob *a, int samples)
{
	int i;

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
 * @brief swap the endianness of uncompressed data form big endian to the cpu
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
	int32_t samples = cmp_input_size_to_samples(data_size_byte, data_type);

	if (!data) /* nothing to do */
		return 0;

	if (samples < 0) {
		debug_print("Error: Can not convert data size in samples.\n");
		return -1;
	}

	/* we do not convert the endianness of the multi entry header */
	if (!rdcu_supported_data_type_is_used(data_type))
		data = (uint8_t *)data + MULTI_ENTRY_HDR_SIZE;

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
		be_to_cpus_nc_offset(data, samples);
		break;
	case DATA_TYPE_BACKGROUND:
		be_to_cpus_nc_background(data, samples);
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
	/* TODO: implement F_CAM conversion */
	case DATA_TYPE_F_CAM_OFFSET:
	case DATA_TYPE_F_CAM_BACKGROUND:
	/* LCOV_EXCL_START */
	case DATA_TYPE_UNKNOWN:
	default:
		debug_print("Error: Can not swap endianness for this compression data type.\n");
		return -1;
	/* LCOV_EXCL_STOP */
	}

	return 0;
}

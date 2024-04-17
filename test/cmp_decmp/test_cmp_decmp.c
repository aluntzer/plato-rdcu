/**
 * @file   test_cmp_decmp.c
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2022
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
 * @brief random compression decompression tests
 * @details We generate random data and compress them with random parameters.
 *	After that we  decompress the compression entity and compare the
 *	decompressed data with the original data.
 */


#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <unity.h>
#include "../test_common/test_common.h"

#include <cmp_icu.h>
#include <cmp_chunk.h>
#include <decmp.h>
#include <cmp_data_types.h>
#include <leon_inttypes.h>
#include <byteorder.h>
#include <cmp_error.h>

#if defined __has_include
#  if __has_include(<time.h>)
#    include <time.h>
#    include <unistd.h>
#    define HAS_TIME_H 1
#  endif
#endif

#define ROUND_UP_TO_MULTIPLE_OF_4(x) (((x) + 3U) & ~3U)


/**
 * @brief  Seeds the pseudo-random number generator
 */

void setUp(void)
{
	uint64_t seed;
	static int n;

#ifdef HAS_TIME_H
	seed = (uint64_t)(time(NULL) ^ getpid()  ^ (intptr_t)&setUp);
	seed = 0;
#else
	seed = 1;
#endif

	if (!n) {
		uint32_t high = seed >> 32;
		uint32_t low = seed & 0xFFFFFFFF;
		n = 1;
		cmp_rand_seed(seed);
		printf("seed: 0x%08"PRIx32"%08"PRIx32"\n", high, low);
	}
}


static size_t gen_ima_data(uint16_t *data, enum cmp_data_type data_type,
			   uint32_t samples, const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data) {
		uint32_t max_data_bits;

		switch (data_type) {
		case DATA_TYPE_IMAGETTE:
		case DATA_TYPE_IMAGETTE_ADAPTIVE:
			max_data_bits = max_used_bits->nc_imagette;
			break;
		case DATA_TYPE_SAT_IMAGETTE:
		case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
			max_data_bits = max_used_bits->saturated_imagette;
			break;
		case DATA_TYPE_F_CAM_IMAGETTE:
		case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
			max_data_bits = max_used_bits->fc_imagette;
			break;
		default:
			TEST_FAIL();
		}
		for (i = 0; i < samples; i++)
			data[i] = (uint16_t)cmp_rand_nbits(max_data_bits);
	}
	return sizeof(*data) * samples;
}


static size_t gen_nc_offset_data(struct offset *data, uint32_t samples,
				 const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].mean = cmp_rand_nbits(max_used_bits->nc_offset_mean);
			data[i].variance = cmp_rand_nbits(max_used_bits->nc_offset_variance);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_fc_offset_data(struct offset *data, uint32_t samples,
				 const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].mean = cmp_rand_nbits(max_used_bits->fc_offset_mean);
			data[i].variance = cmp_rand_nbits(max_used_bits->fc_offset_variance);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_nc_background_data(struct background *data, uint32_t samples,
				     const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].mean = cmp_rand_nbits(max_used_bits->nc_background_mean);
			data[i].variance = cmp_rand_nbits(max_used_bits->nc_background_variance);
			data[i].outlier_pixels = (__typeof__(data[i].outlier_pixels))cmp_rand_nbits(max_used_bits->nc_background_outlier_pixels);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_fc_background_data(struct background *data, uint32_t samples,
				     const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].mean = cmp_rand_nbits(max_used_bits->fc_background_mean);
			data[i].variance = cmp_rand_nbits(max_used_bits->fc_background_variance);
			data[i].outlier_pixels = (__typeof__(data[i].outlier_pixels))cmp_rand_nbits(max_used_bits->fc_background_outlier_pixels);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_smearing_data(struct smearing *data, uint32_t samples,
				const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].mean = cmp_rand_nbits(max_used_bits->smearing_mean);
			data[i].variance_mean = (__typeof__(data[i].variance_mean))cmp_rand_nbits(max_used_bits->smearing_variance_mean);
			data[i].outlier_pixels = (__typeof__(data[i].outlier_pixels))cmp_rand_nbits(max_used_bits->smearing_outlier_pixels);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_s_fx_data(struct s_fx *data, uint32_t samples,
			    const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].exp_flags = (__typeof__(data[i].exp_flags))cmp_rand_nbits(max_used_bits->s_exp_flags);
			data[i].fx = cmp_rand_nbits(max_used_bits->s_fx);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_s_fx_efx_data(struct s_fx_efx *data, uint32_t samples,
				const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].exp_flags = (__typeof__(data[i].exp_flags))cmp_rand_nbits(max_used_bits->s_exp_flags);
			data[i].fx = cmp_rand_nbits(max_used_bits->s_fx);
			data[i].efx = cmp_rand_nbits(max_used_bits->s_efx);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_s_fx_ncob_data(struct s_fx_ncob *data, uint32_t samples,
				 const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].exp_flags = (__typeof__(data[i].exp_flags))cmp_rand_nbits(max_used_bits->s_exp_flags);
			data[i].fx = cmp_rand_nbits(max_used_bits->s_fx);
			data[i].ncob_x = cmp_rand_nbits(max_used_bits->s_ncob);
			data[i].ncob_y = cmp_rand_nbits(max_used_bits->s_ncob);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_s_fx_efx_ncob_ecob_data(struct s_fx_efx_ncob_ecob *data, uint32_t samples,
					  const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].exp_flags = (__typeof__(data[i].exp_flags))cmp_rand_nbits(max_used_bits->s_exp_flags);
			data[i].fx = cmp_rand_nbits(max_used_bits->s_fx);
			data[i].ncob_x = cmp_rand_nbits(max_used_bits->s_ncob);
			data[i].ncob_y = cmp_rand_nbits(max_used_bits->s_ncob);
			data[i].efx = cmp_rand_nbits(max_used_bits->s_efx);
			data[i].ecob_x = cmp_rand_nbits(max_used_bits->s_ecob);
			data[i].ecob_y = cmp_rand_nbits(max_used_bits->s_ecob);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_f_fx_data(struct f_fx *data, uint32_t samples,
			    const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data)
		for (i = 0; i < samples; i++)
			data[i].fx = cmp_rand_nbits(max_used_bits->f_fx);
	return sizeof(*data) * samples;
}


static size_t gen_f_fx_efx_data(struct f_fx_efx *data, uint32_t samples,
				const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].fx = cmp_rand_nbits(max_used_bits->f_fx);
			data[i].efx = cmp_rand_nbits(max_used_bits->f_efx);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_f_fx_ncob_data(struct f_fx_ncob *data, uint32_t samples,
				 const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].fx = cmp_rand_nbits(max_used_bits->f_fx);
			data[i].ncob_x = cmp_rand_nbits(max_used_bits->f_ncob);
			data[i].ncob_y = cmp_rand_nbits(max_used_bits->f_ncob);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_f_fx_efx_ncob_ecob_data(struct f_fx_efx_ncob_ecob *data, uint32_t samples,
					  const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].fx = cmp_rand_nbits(max_used_bits->f_fx);
			data[i].ncob_x = cmp_rand_nbits(max_used_bits->f_ncob);
			data[i].ncob_y = cmp_rand_nbits(max_used_bits->f_ncob);
			data[i].efx = cmp_rand_nbits(max_used_bits->f_efx);
			data[i].ecob_x = cmp_rand_nbits(max_used_bits->f_ecob);
			data[i].ecob_y = cmp_rand_nbits(max_used_bits->f_ecob);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_l_fx_data(struct l_fx *data, uint32_t samples,
			    const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].exp_flags = cmp_rand_nbits(max_used_bits->l_exp_flags);
			data[i].fx = cmp_rand_nbits(max_used_bits->l_fx);
			data[i].fx_variance = cmp_rand_nbits(max_used_bits->l_fx_variance);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_l_fx_efx_data(struct l_fx_efx *data, uint32_t samples,
				const struct cmp_max_used_bits *max_used_bits)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].exp_flags = cmp_rand_nbits(max_used_bits->l_exp_flags);
			data[i].fx = cmp_rand_nbits(max_used_bits->l_fx);
			data[i].efx = cmp_rand_nbits(max_used_bits->l_efx);
			data[i].fx_variance = cmp_rand_nbits(max_used_bits->l_fx_variance);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_l_fx_ncob_data(struct l_fx_ncob *data, uint32_t samples,
				 const struct cmp_max_used_bits *max_used_bits)
{
	if (data) {
		uint32_t i;

		for (i = 0; i < samples; i++) {
			data[i].exp_flags = cmp_rand_nbits(max_used_bits->l_exp_flags);
			data[i].fx = cmp_rand_nbits(max_used_bits->l_fx);
			data[i].ncob_x = cmp_rand_nbits(max_used_bits->l_ncob);
			data[i].ncob_y = cmp_rand_nbits(max_used_bits->l_ncob);
			data[i].fx_variance = cmp_rand_nbits(max_used_bits->l_fx_variance);
			data[i].cob_x_variance = cmp_rand_nbits(max_used_bits->l_cob_variance);
			data[i].cob_y_variance = cmp_rand_nbits(max_used_bits->l_cob_variance);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_l_fx_efx_ncob_ecob_data(struct l_fx_efx_ncob_ecob *data, uint32_t samples,
					  const struct cmp_max_used_bits *max_used_bits)
{
	if (data) {
		uint32_t i;

		for (i = 0; i < samples; i++) {
			data[i].exp_flags = cmp_rand_nbits(max_used_bits->l_exp_flags);
			data[i].fx = cmp_rand_nbits(max_used_bits->l_fx);
			data[i].ncob_x = cmp_rand_nbits(max_used_bits->l_ncob);
			data[i].ncob_y = cmp_rand_nbits(max_used_bits->l_ncob);
			data[i].efx = cmp_rand_nbits(max_used_bits->l_efx);
			data[i].ecob_x = cmp_rand_nbits(max_used_bits->l_ecob);
			data[i].ecob_y = cmp_rand_nbits(max_used_bits->l_ecob);
			data[i].fx_variance = cmp_rand_nbits(max_used_bits->l_fx_variance);
			data[i].cob_x_variance = cmp_rand_nbits(max_used_bits->l_cob_variance);
			data[i].cob_y_variance = cmp_rand_nbits(max_used_bits->l_cob_variance);
		}
	}
	return sizeof(*data) * samples;
}


uint32_t generate_random_collection_hdr(struct collection_hdr *col, enum cmp_data_type data_type,
					uint32_t samples)
{
	static uint8_t sequence_num;
	size_t data_size = size_of_a_sample(data_type)*samples;

	TEST_ASSERT(data_size <= UINT16_MAX);

	if (col) {
#ifdef HAS_TIME_H
		TEST_ASSERT_FALSE(cmp_col_set_timestamp(col, cmp_ent_create_timestamp(NULL)));
#else
		TEST_ASSERT_FALSE(cmp_col_set_timestamp(col, 0x150D15AB1ED));
#endif
		TEST_ASSERT_FALSE(cmp_col_set_configuration_id(col, (uint16_t)cmp_rand32()));

		TEST_ASSERT_FALSE(cmp_col_set_pkt_type(col, COL_SCI_PKTS_TYPE));
		TEST_ASSERT_FALSE(cmp_col_set_subservice(col, convert_cmp_data_type_to_subservice(data_type)));
		TEST_ASSERT_FALSE(cmp_col_set_ccd_id(col, (uint8_t)cmp_rand_between(0, 3)));
		TEST_ASSERT_FALSE(cmp_col_set_sequence_num(col, sequence_num++));

		TEST_ASSERT_FALSE(cmp_col_set_data_length(col, (uint16_t)data_size));
	}
	return COLLECTION_HDR_SIZE;
}


/**
 * @brief generates a random collection (with header)
 *
 * @param col		pointer to where the random collection will be stored;
 *			if NULL, the function will only return the size of the
 *			random collection
 * @param data_type	specifies the compression data type of the test data
 * @param samples	the number of random test data samples to generate
 * @param max_used_bits	pointer to a structure that tracks the maximum number of
 *			bits used
 *
 * @return the size of the generated random collection in bytes
 */

size_t generate_random_collection(struct collection_hdr *col, enum cmp_data_type data_type,
				  uint32_t samples, const struct cmp_max_used_bits *max_used_bits)
{
	size_t size;
	void *science_data = NULL;

	if (col)
		science_data = col->entry;

	size = generate_random_collection_hdr(col, data_type, samples);
#if 0
	{	int i;

		for (i = 0; i < size_of_a_sample(data_type)*samples; i++) {
			if (col)
				col->entry[i] = i;
		}
		return size+i;
	}
#endif

	switch (data_type) {
	case DATA_TYPE_IMAGETTE:
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_SAT_IMAGETTE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_F_CAM_IMAGETTE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		size += gen_ima_data(science_data, data_type, samples, max_used_bits);
		break;
	case DATA_TYPE_OFFSET:
		size += gen_nc_offset_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_BACKGROUND:
		size += gen_nc_background_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_SMEARING:
		size += gen_smearing_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_S_FX:
		size += gen_s_fx_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_S_FX_EFX:
		size += gen_s_fx_efx_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_S_FX_NCOB:
		size += gen_s_fx_ncob_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_S_FX_EFX_NCOB_ECOB:
		size += gen_s_fx_efx_ncob_ecob_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_L_FX:
		size += gen_l_fx_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_L_FX_EFX:
		size += gen_l_fx_efx_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_L_FX_NCOB:
		size += gen_l_fx_ncob_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_L_FX_EFX_NCOB_ECOB:
		size += gen_l_fx_efx_ncob_ecob_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_F_FX:
		size += gen_f_fx_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_F_FX_EFX:
		size += gen_f_fx_efx_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_F_FX_NCOB:
		size += gen_f_fx_ncob_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_F_FX_EFX_NCOB_ECOB:
		size += gen_f_fx_efx_ncob_ecob_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_F_CAM_OFFSET:
		size += gen_fc_offset_data(science_data, samples, max_used_bits);
		break;
	case DATA_TYPE_F_CAM_BACKGROUND:
		size += gen_fc_background_data(science_data, samples, max_used_bits);
		break;
	default:
		TEST_FAIL();
	}

	if (col)
		TEST_ASSERT_EQUAL_UINT(size, cmp_col_get_size(col));

	return size;
}

struct chunk_def {
	enum cmp_data_type data_type;
	uint32_t samples;
};


/**
 * @brief generates a random chunk of collections
 *
 * @param chunk		pointer to  where the random chunk will be stored; if
 *			NULL, the function will only return the size of the
 *			random chunk
 * @param col_array	specifies which collections are contained in the chunk
 * @param array_elements	number of elements in the col_array
 * @param max_used_bits	pointer to a structure that tracks the maximum number of
 *			bits used
 *
 * @return the size of the generated random chunk in bytes
 */

static uint32_t generate_random_chunk(void *chunk, struct chunk_def col_array[], size_t array_elements,
				    const struct cmp_max_used_bits *max_used_bits)
{
	size_t i;
	uint32_t chunk_size = 0;
	struct collection_hdr *col = NULL;

	for (i = 0; i < array_elements; i++) {
		if (chunk)
			col = (struct collection_hdr *)((uint8_t *)chunk + chunk_size);

		chunk_size += generate_random_collection(col, col_array[i].data_type,
							 col_array[i].samples, max_used_bits);
	}
	return chunk_size;
}


/**
 * @brief generate random compression configuration
 *
 * @param cfg	pointer to a compression configuration
 */

void generate_random_cmp_cfg(struct cmp_cfg *cfg)
{
	if (cmp_imagette_data_type_is_used(cfg->data_type)) {
		cfg->cmp_par_imagette = cmp_rand_between(MIN_IMA_GOLOMB_PAR, MAX_IMA_GOLOMB_PAR);
		cfg->ap1_golomb_par = cmp_rand_between(MIN_IMA_GOLOMB_PAR, MAX_IMA_GOLOMB_PAR);
		cfg->ap2_golomb_par = cmp_rand_between(MIN_IMA_GOLOMB_PAR, MAX_IMA_GOLOMB_PAR);
		cfg->spill_imagette = cmp_rand_between(MIN_IMA_SPILL, cmp_ima_max_spill(cfg->golomb_par));
		cfg->ap1_spill = cmp_rand_between(MIN_IMA_SPILL, cmp_ima_max_spill(cfg->ap1_golomb_par));
		cfg->ap2_spill = cmp_rand_between(MIN_IMA_SPILL, cmp_ima_max_spill(cfg->ap2_golomb_par));
	} else {
		cfg->cmp_par_1 = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		cfg->cmp_par_2 = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		cfg->cmp_par_3 = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		cfg->cmp_par_4 = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		cfg->cmp_par_5 = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		cfg->cmp_par_6 = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		cfg->spill_par_1 = cmp_rand_between(MIN_NON_IMA_SPILL, cmp_icu_max_spill(cfg->cmp_par_exp_flags));
		cfg->spill_par_2 = cmp_rand_between(MIN_NON_IMA_SPILL, cmp_icu_max_spill(cfg->cmp_par_fx));
		cfg->spill_par_3 = cmp_rand_between(MIN_NON_IMA_SPILL, cmp_icu_max_spill(cfg->cmp_par_ncob));
		cfg->spill_par_4 = cmp_rand_between(MIN_NON_IMA_SPILL, cmp_icu_max_spill(cfg->cmp_par_efx));
		cfg->spill_par_5 = cmp_rand_between(MIN_NON_IMA_SPILL, cmp_icu_max_spill(cfg->cmp_par_ecob));
		cfg->spill_par_6 = cmp_rand_between(MIN_NON_IMA_SPILL, cmp_icu_max_spill(cfg->cmp_par_fx_cob_variance));
	}
}


/**
 * @brief generate random chunk compression parameters
 *
 * @param par	pointer where to store the chunk compression
 */

void generate_random_cmp_par(struct cmp_par *par)
{
	if (par) {
		par->cmp_mode = cmp_rand_between(0, MAX_RDCU_CMP_MODE);
		par->model_value = cmp_rand_between(0, MAX_MODEL_VALUE);
		par->lossy_par = cmp_rand_between(0, MAX_ICU_ROUND);

		par->nc_imagette = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);

		par->s_exp_flags = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->s_fx = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->s_ncob = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->s_efx = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->s_ecob = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);

		par->l_exp_flags = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->l_fx = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->l_ncob = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->l_efx = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->l_ecob = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->l_fx_cob_variance = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);

		par->saturated_imagette = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);

		par->nc_offset_mean = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->nc_offset_variance = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->nc_background_mean = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->nc_background_variance = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->nc_background_outlier_pixels = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);

		par->smearing_mean = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->smearing_variance_mean = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->smearing_outlier_pixels = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);

		par->fc_imagette = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->fc_offset_mean = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->fc_offset_variance = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->fc_background_mean = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->fc_background_variance = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
		par->fc_background_outlier_pixels = cmp_rand_between(MIN_NON_IMA_GOLOMB_PAR, MAX_NON_IMA_GOLOMB_PAR);
	}
}


/**
 * @brief compress the given configuration and decompress it afterwards; finally
 *	compare the results
 *
 * @param cfg	pointer to a compression configuration
 */

void compression_decompression(struct cmp_cfg *cfg)
{
	int cmp_size_bits, s, error;
	uint32_t data_size, cmp_data_size, cmp_ent_size;
	struct cmp_entity *ent;
	void *decompressed_data;
	static void *model_of_data;
	void *updated_model = NULL;

	if (!cfg) {
		free(model_of_data);
		return;
	}

	TEST_ASSERT_NOT_NULL(cfg);

	TEST_ASSERT_NULL(cfg->icu_output_buf);

	data_size = cmp_cal_size_of_data(cfg->samples, cfg->data_type);

	/* create a compression entity */
	cmp_data_size = cmp_cal_size_of_data(cfg->buffer_length, cfg->data_type);
	/* cmp_data_size &= ~0x3U; /1* the size of the compressed data should be a multiple of 4 *1/ */
	TEST_ASSERT_NOT_EQUAL_INT(0, cmp_data_size);

	cmp_ent_size = cmp_ent_create(NULL, cfg->data_type, cfg->cmp_mode == CMP_MODE_RAW, cmp_data_size);
	TEST_ASSERT_NOT_EQUAL_UINT(0, cmp_ent_size);
	ent = malloc(cmp_ent_size); TEST_ASSERT_TRUE(ent);
	cmp_ent_size = cmp_ent_create(ent, cfg->data_type, cfg->cmp_mode == CMP_MODE_RAW, cmp_data_size);
	TEST_ASSERT_NOT_EQUAL_UINT(0, cmp_ent_size);

	/* we put the compressed data direct into the compression entity */
	cfg->icu_output_buf = cmp_ent_get_data_buf(ent);
	TEST_ASSERT_NOT_NULL(cfg->icu_output_buf);

	/* now compress the data */
	cmp_size_bits = icu_compress_data(cfg);

	TEST_ASSERT(cmp_size_bits > 0);

	/* put the compression parameters in the entity header */
	{
		/* mock values */
		uint32_t version_id = ~0U;
		uint64_t start_time = 32;
		uint64_t end_time = 42;
		uint16_t model_id = 0xCAFE;
		uint8_t model_counter = 0;
		uint32_t ent_size;

		ent_size = cmp_ent_build(ent, version_id, start_time, end_time,
					 model_id, model_counter, cfg, cmp_size_bits);
		TEST_ASSERT_NOT_EQUAL_UINT(0, ent_size);
		error = cmp_ent_set_size(ent, ent_size);
		TEST_ASSERT_FALSE(error);
	}

	/* allocate the buffers for decompression */
	TEST_ASSERT_NOT_EQUAL_INT(0, data_size);
	s = decompress_cmp_entiy(ent, model_of_data, NULL, NULL);
	decompressed_data = malloc((size_t)s); TEST_ASSERT_NOT_NULL(decompressed_data);

	if (model_mode_is_used(cfg->cmp_mode)) {
		updated_model = malloc(data_size);
		TEST_ASSERT_NOT_NULL(updated_model);
	}

	/* now we try to decompress the data */
	s = decompress_cmp_entiy(ent, model_of_data, updated_model, decompressed_data);
	TEST_ASSERT_EQUAL_INT(data_size, s);
	TEST_ASSERT_FALSE(memcmp(decompressed_data, cfg->input_buf, data_size));

	if (model_mode_is_used(cfg->cmp_mode)) {
		TEST_ASSERT_NOT_NULL(updated_model);
		TEST_ASSERT_NOT_NULL(model_of_data);
		TEST_ASSERT_FALSE(memcmp(updated_model, cfg->icu_new_model_buf, data_size));
		memcpy(model_of_data, updated_model, data_size);
	} else { /* non-model mode */
		/* reset model */
		free(model_of_data);
		model_of_data = malloc(data_size);
		memcpy(model_of_data, decompressed_data, data_size);
	}

	cfg->icu_output_buf = NULL;
	free(ent);
	free(decompressed_data);
	free(updated_model);
}


/**
 * @brief random RDCU like compression decompression test
 *
 * We generate random imagette data and compress them with random parameters.
 * After that we put the data in a compression entity. We decompress the
 * compression entity and compare the decompressed data with the original data.
 *
 * @test icu_compress_data
 * @test decompress_cmp_entiy
 */

void test_random_round_trip_like_rdcu_compression(void)
{
	enum cmp_data_type data_type;
	enum cmp_mode cmp_mode;
	struct cmp_cfg cfg;
	uint32_t cmp_buffer_size;
	enum {
		MAX_DATA_TO_COMPRESS_SIZE = 0x1000B,
		CMP_BUFFER_FAKTOR = 3 /* compression data buffer size / data to compress buffer size */
	};
	void *data_to_compress1 = malloc(MAX_DATA_TO_COMPRESS_SIZE);
	void *data_to_compress2 = malloc(MAX_DATA_TO_COMPRESS_SIZE);
	void *updated_model = calloc(1, MAX_DATA_TO_COMPRESS_SIZE);

	for (data_type = 1; data_type <= DATA_TYPE_F_CAM_BACKGROUND; data_type++) {
		/* printf("%s\n", data_type2string(data_type)); */
		/* generate random data*/
		size_t size;
		uint32_t samples = cmp_rand_between(1, UINT16_MAX/size_of_a_sample(data_type));
		uint32_t model_value = cmp_rand_between(0, MAX_MODEL_VALUE);

		if (!rdcu_supported_data_type_is_used(data_type))
			continue;

		size = gen_ima_data(NULL, data_type, samples, &MAX_USED_BITS_V1);
		TEST_ASSERT(size <= MAX_DATA_TO_COMPRESS_SIZE);
		size = gen_ima_data(data_to_compress1, data_type, samples, &MAX_USED_BITS_V1);
		TEST_ASSERT(size <= MAX_DATA_TO_COMPRESS_SIZE);
		size = gen_ima_data(data_to_compress2, data_type, samples, &MAX_USED_BITS_V1);
		TEST_ASSERT(size <= MAX_DATA_TO_COMPRESS_SIZE);
		/* for (cmp_mode = CMP_MODE_RAW; cmp_mode <= CMP_MODE_STUFF; cmp_mode++) { */
		for (cmp_mode = CMP_MODE_RAW; cmp_mode <= CMP_MODE_DIFF_MULTI; cmp_mode++) {
			/* printf("cmp_mode: %i\n", cmp_mode); */
			cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value,
						 CMP_LOSSLESS);
			TEST_ASSERT_NOT_EQUAL_INT(cfg.data_type, DATA_TYPE_UNKNOWN);

			generate_random_cmp_cfg(&cfg);

			if (!model_mode_is_used(cmp_mode))
				cmp_buffer_size = cmp_cfg_icu_buffers(&cfg, data_to_compress1,
								      samples, NULL, NULL, NULL, samples*CMP_BUFFER_FAKTOR);
			else
				cmp_buffer_size = cmp_cfg_icu_buffers(&cfg, data_to_compress2,
								      samples, data_to_compress1, updated_model, NULL, samples*CMP_BUFFER_FAKTOR);

			TEST_ASSERT_EQUAL_UINT(cmp_buffer_size, cmp_cal_size_of_data(CMP_BUFFER_FAKTOR*samples, data_type));

			compression_decompression(&cfg);
		}
	}
	compression_decompression(NULL);
	free(data_to_compress1);
	free(data_to_compress2);
	free(updated_model);
}


/**
 * @test icu_compress_data
 */

void test_random_compression_decompress_rdcu_data(void)
{
	struct cmp_cfg cfg;
	struct cmp_info info = {0};
	uint32_t cmp_buffer_size;
	int s, i, cmp_size_bits;
	void *compressed_data;
	uint16_t *decompressed_data;
	enum {N_SAMPLES = 5};
	uint16_t data[N_SAMPLES] = {0, UINT16_MAX, INT16_MAX, 42, 23};
	enum {
		CMP_BUFFER_FAKTOR = 2 /* compression data buffer size / data to compress buffer size */
	};

	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_RAW, 8, CMP_LOSSLESS);
	TEST_ASSERT_NOT_EQUAL_INT(cfg.data_type, DATA_TYPE_UNKNOWN);

	cmp_buffer_size = cmp_cfg_icu_buffers(&cfg, data, N_SAMPLES, NULL, NULL,
					      NULL, N_SAMPLES*CMP_BUFFER_FAKTOR);
	compressed_data = malloc(cmp_buffer_size);
	cmp_buffer_size = cmp_cfg_icu_buffers(&cfg, data, N_SAMPLES, NULL, NULL,
					      compressed_data, N_SAMPLES*CMP_BUFFER_FAKTOR);
	TEST_ASSERT_EQUAL_INT(cmp_buffer_size, cmp_cal_size_of_data(CMP_BUFFER_FAKTOR*N_SAMPLES, DATA_TYPE_IMAGETTE));

	cmp_size_bits = icu_compress_data(&cfg);
	TEST_ASSERT(cmp_size_bits > 0);
	info.cmp_size = (uint32_t)cmp_size_bits;
	info.cmp_mode_used = (uint8_t)cfg.cmp_mode;
	info.model_value_used = (uint8_t)cfg.model_value;
	info.round_used = (uint8_t)cfg.round;
	info.spill_used = cfg.spill;
	info.golomb_par_used = cfg.golomb_par;
	info.samples_used = cfg.samples;
	info.rdcu_new_model_adr_used = cfg.rdcu_new_model_adr;
	info.rdcu_cmp_adr_used = cfg.rdcu_buffer_adr;

	s = decompress_rdcu_data(compressed_data, &info, NULL, NULL, NULL);
	TEST_ASSERT_EQUAL(sizeof(data), s);
	decompressed_data = malloc((size_t)s);
	s = decompress_rdcu_data(compressed_data, &info, NULL, NULL, decompressed_data);
	TEST_ASSERT_EQUAL(sizeof(data), s);

	for (i = 0; i < N_SAMPLES; i++)
		TEST_ASSERT_EQUAL_HEX16(data[i], decompressed_data[i]);

	free(compressed_data);
	free(decompressed_data);
}


static uint32_t chunk_round_trip(void *data, uint32_t data_size,
				 void *model, void *up_model,
				 uint32_t *cmp_data, uint32_t cmp_data_capacity,
				 struct cmp_par *cmp_par, int use_decmp_buf, int use_decmp_up_model)
{
	uint32_t cmp_size;
	void *model_cpy = NULL;

	/* if in-place model update is used (up_model == model), the model
	 * needed for decompression is destroyed; therefore we make a copy
	 */
	if (model) {
		if (up_model == model) {
			model_cpy = TEST_malloc(data_size);
			memcpy(model_cpy, model, data_size);
		} else {
			model_cpy = model;
		}
	}

	cmp_size = compress_chunk(data, data_size, model, up_model,
				  cmp_data, cmp_data_capacity, cmp_par);

#if 0
	{ /* Compress a second time and check for determinism */
		int32_t cSize2;
		void *compressed2 = NULL;
		void *up_model2 = NULL;

		if (compressed)
			compressed2 = FUZZ_malloc(compressedCapacity);

		if (up_model)
			up_model2 = FUZZ_malloc(srcSize);
		cSize2 = compress_chunk((void *)src, srcSize, (void *)model, up_model2,
					   compressed2, compressedCapacity, cmp_par);
		FUZZ_ASSERT(cSize == cSize2);
		FUZZ_ASSERT_MSG(!FUZZ_memcmp(compressed, compressed2, cSize), "Not deterministic!");
		FUZZ_ASSERT_MSG(!FUZZ_memcmp(up_model, compressed2, cSize), "NO deterministic!");
		free(compressed2);
		free(up_model2);
	}
#endif
	if (!cmp_is_error(cmp_size) && cmp_data) {
		void *decmp_data = NULL;
		void *up_model_decmp = NULL;
		int decmp_size;

		decmp_size = decompress_cmp_entiy((struct cmp_entity *)cmp_data, model_cpy, NULL, NULL);
		TEST_ASSERT(decmp_size >= 0);
		TEST_ASSERT_EQUAL((uint32_t)decmp_size, data_size);

		if (use_decmp_buf)
			decmp_data = TEST_malloc(data_size);
		if (use_decmp_up_model)
			up_model_decmp = TEST_malloc(data_size);

		decmp_size = decompress_cmp_entiy((struct cmp_entity *)cmp_data, model_cpy,
						  up_model_decmp, decmp_data);
		TEST_ASSERT(decmp_size >= 0);
		TEST_ASSERT((uint32_t)decmp_size == data_size);

		if (use_decmp_buf) {
			TEST_ASSERT_EQUAL_HEX8_ARRAY(data, decmp_data, data_size);
			TEST_ASSERT(!memcmp(data, decmp_data, data_size));

			/*
			 * the model is only updated when the decompressed_data
			 * buffer is set
			 */
			if (up_model && up_model_decmp)
				TEST_ASSERT(!memcmp(up_model, up_model_decmp, data_size));
		}

		free(decmp_data);
		free(up_model_decmp);
	}

	if (up_model == model)
		free(model_cpy);

	return cmp_size;
}


/**
 * @brief random compression decompression round trip test
 *
 * We generate random data and compress them with random parameters.
 * We decompress the compression entity and compare the decompressed data with
 * the original data.
 *
 * @test compress_chunk
 * @test decompress_cmp_entiy
 */

void test_random_collection_round_trip(void)
{
	enum cmp_data_type data_type;
	enum cmp_mode cmp_mode;
	enum { MAX_DATA_TO_COMPRESS_SIZE = UINT16_MAX};
	uint32_t cmp_data_capacity = COMPRESS_CHUNK_BOUND(MAX_DATA_TO_COMPRESS_SIZE, 1);
#ifdef __sparc__
	void *data          = (void *)0x63000000;
	void *model         = (void *)0x64000000;
	void *updated_model = (void *)0x65000000;
	void *cmp_data      = (void *)0x66000000;
#else /* assume PC */
	void *data = malloc(CMP_ENTITY_MAX_ORIGINAL_SIZE);
	void *model = malloc(MAX_DATA_TO_COMPRESS_SIZE);
	void *updated_model = calloc(1, MAX_DATA_TO_COMPRESS_SIZE);
	void *cmp_data = malloc(cmp_data_capacity);
#endif

	TEST_ASSERT_NOT_NULL(data);
	TEST_ASSERT_NOT_NULL(model);
	TEST_ASSERT_NOT_NULL(updated_model);
	TEST_ASSERT_NOT_NULL(cmp_data);

	for (data_type = 1; data_type <= DATA_TYPE_F_CAM_BACKGROUND; data_type++) {
		/* printf("%s\n", data_type2string(data_type)); */
		/* generate random data*/
		size_t size;
		uint32_t samples = cmp_rand_between(1, UINT16_MAX/size_of_a_sample(data_type));

		size = generate_random_collection(NULL, data_type, samples, &MAX_USED_BITS_SAFE);
		TEST_ASSERT(size <= MAX_DATA_TO_COMPRESS_SIZE);
		size = generate_random_collection(data, data_type, samples, &MAX_USED_BITS_SAFE);
		TEST_ASSERT(size <= MAX_DATA_TO_COMPRESS_SIZE);
		size = generate_random_collection(model, data_type, samples, &MAX_USED_BITS_SAFE);
		TEST_ASSERT(size <= MAX_DATA_TO_COMPRESS_SIZE);

		for (cmp_mode = CMP_MODE_RAW; cmp_mode <= CMP_MODE_DIFF_MULTI; cmp_mode++) {
			struct cmp_par par;
			uint32_t cmp_size;

			generate_random_cmp_par(&par);
			par.cmp_mode = cmp_mode;
			par.lossy_par = CMP_LOSSLESS;

			cmp_size = chunk_round_trip(data, (uint32_t)size, model, updated_model,
						   cmp_data, cmp_data_capacity,
						   &par, 1, model_mode_is_used(par.cmp_mode));
			/* No chunk is defined for fast cadence subservices */
			if (data_type == DATA_TYPE_F_FX || data_type == DATA_TYPE_F_FX_EFX ||
			    data_type == DATA_TYPE_F_FX_NCOB || data_type == DATA_TYPE_F_FX_EFX_NCOB_ECOB)
				TEST_ASSERT_EQUAL_INT(CMP_ERROR_COL_SUBSERVICE_UNSUPPORTED, cmp_get_error_code(cmp_size));
			else
				TEST_ASSERT_FALSE(cmp_is_error(cmp_size));
		}
	}
#ifndef __sparc__
	free(data);
	free(model);
	free(updated_model);
	free(cmp_data);
#endif
}


/**
 * @test compress_chunk
 * @test decompress_cmp_entiy
 */

void test_cmp_collection_raw(void)
{
	struct collection_hdr *col = NULL;
	uint32_t samples = 2;
	uint32_t dst_capacity = 0;
	struct s_fx *data;
	uint32_t *dst = NULL;
	struct cmp_par par = {0};
	const uint32_t col_size = COLLECTION_HDR_SIZE+2*sizeof(struct s_fx);
	const size_t exp_cmp_size_byte = GENERIC_HEADER_SIZE + col_size;
	uint32_t cmp_size_byte;

	par.cmp_mode = CMP_MODE_RAW;

	col = malloc(col_size); TEST_ASSERT_NOT_NULL(col);
	generate_random_collection_hdr(col, DATA_TYPE_S_FX, samples);
	data = (struct s_fx *)col->entry;
	data[0].exp_flags = 0;
	data[0].fx = 0;
	data[1].exp_flags = 0xF0;
	data[1].fx = 0xABCDE0FF;

	cmp_size_byte = compress_chunk(col, col_size, NULL, NULL, dst, dst_capacity, &par);
	TEST_ASSERT_EQUAL_INT(exp_cmp_size_byte, cmp_size_byte);
	dst_capacity = (uint32_t)cmp_size_byte;
	dst = malloc(dst_capacity);
	TEST_ASSERT_NOT_NULL(dst);
	cmp_size_byte = compress_chunk(col, col_size, NULL, NULL, dst, dst_capacity, &par);
	TEST_ASSERT_EQUAL_INT(exp_cmp_size_byte, cmp_size_byte);

	{
		uint8_t *p = (uint8_t *)dst+GENERIC_HEADER_SIZE;
		struct collection_hdr *raw_cmp_col = (struct collection_hdr *)p;
		struct s_fx *raw_cmp_data = (void *)raw_cmp_col->entry;

		/* TEST_ASSERT_EQUAL_UINT(cpu_to_be16(2*sizeof(struct s_fx)), ((uint16_t *)p)[0]); */
		TEST_ASSERT(memcmp(col, raw_cmp_col, COLLECTION_HDR_SIZE) == 0);
		TEST_ASSERT_EQUAL_HEX(data[0].exp_flags, raw_cmp_data[0].exp_flags);
		TEST_ASSERT_EQUAL_HEX(data[0].fx, be32_to_cpu(raw_cmp_data[0].fx));
		TEST_ASSERT_EQUAL_HEX(data[1].exp_flags, raw_cmp_data[1].exp_flags);
		TEST_ASSERT_EQUAL_HEX(data[1].fx, be32_to_cpu(raw_cmp_data[1].fx));
	}
	{ /* decompress the data */
		int decmp_size;
		uint8_t *decompressed_data;

		decmp_size = decompress_cmp_entiy((struct cmp_entity *)dst, NULL, NULL, NULL);
		TEST_ASSERT_EQUAL_INT(col_size, decmp_size);
		decompressed_data = malloc((size_t)decmp_size); TEST_ASSERT_TRUE(decompressed_data);
		decmp_size = decompress_cmp_entiy((struct cmp_entity *)dst, NULL, NULL, decompressed_data);
		TEST_ASSERT_EQUAL_INT(col_size, decmp_size);
		TEST_ASSERT_EQUAL_HEX8_ARRAY(col, decompressed_data, decmp_size);
		free(decompressed_data);
	}

	/* error case: buffer for the compressed data is to small */
	dst_capacity -= 1;
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF_,
			      cmp_get_error_code(compress_chunk(col, col_size, NULL, NULL, dst, dst_capacity, &par)));

	free(col);
	free(dst);
}


/**
 * @test compress_chunk
 * @test decompress_cmp_entiy
 */

void test_cmp_collection_diff(void)
{
	struct collection_hdr *col = NULL;
	uint32_t *dst = NULL;
	uint32_t dst_capacity = 0;
	struct cmp_par par = {0};
	const uint16_t cmp_size_byte_exp = 2;
	struct s_fx *data;
	const uint32_t samples = 2;
	const uint32_t col_size = COLLECTION_HDR_SIZE+samples*sizeof(*data);


	 /* generate test data */
	col = malloc(col_size); TEST_ASSERT_NOT_NULL(col);
	generate_random_collection_hdr(col, DATA_TYPE_S_FX, samples);
	data = (struct s_fx *)col->entry;
	data[0].exp_flags = 0;
	data[0].fx = 0;
	data[1].exp_flags = 1;
	data[1].fx = 1;


	{ /* compress data */
		uint32_t cmp_size_byte;
		const int exp_cmp_size_byte = NON_IMAGETTE_HEADER_SIZE + CMP_COLLECTION_FILD_SIZE
			+ COLLECTION_HDR_SIZE + cmp_size_byte_exp;

		par.cmp_mode = CMP_MODE_DIFF_ZERO;
		par.s_exp_flags = 1;
		par.s_fx = 1;

		cmp_size_byte = compress_chunk(col, col_size, NULL, NULL, dst, dst_capacity, &par);
		TEST_ASSERT_EQUAL_INT(exp_cmp_size_byte, cmp_size_byte);
		dst_capacity = (uint32_t)ROUND_UP_TO_MULTIPLE_OF_4(cmp_size_byte);
		dst = malloc(dst_capacity); TEST_ASSERT_NOT_NULL(dst);
		cmp_size_byte = compress_chunk(col, col_size, NULL, NULL, dst, dst_capacity, &par);
		TEST_ASSERT_EQUAL_INT(exp_cmp_size_byte, cmp_size_byte);
	}

	{ /* check the compressed data */
		uint8_t *p = (uint8_t *)dst;
		uint16_t cmp_collection_size_exp = cpu_to_be16(cmp_size_byte_exp);

		/* TODO: check the entity header */
		p += NON_IMAGETTE_HEADER_SIZE;

		TEST_ASSERT_EQUAL_HEX8_ARRAY(&cmp_collection_size_exp, p, CMP_COLLECTION_FILD_SIZE);
		p += CMP_COLLECTION_FILD_SIZE;

		TEST_ASSERT(memcmp(col, p, COLLECTION_HDR_SIZE) == 0);
		p += COLLECTION_HDR_SIZE;

		TEST_ASSERT_EQUAL_HEX8(0xAE, *p++);
		TEST_ASSERT_EQUAL_HEX8(0xE0, *p++);

		TEST_ASSERT_EQUAL_size_t(dst_capacity, p - (uint8_t *)dst);
	}
	{ /* decompress the data */
		int decmp_size;
		uint8_t *decompressed_data;

		decmp_size = decompress_cmp_entiy((struct cmp_entity *)dst, NULL, NULL, NULL);
		TEST_ASSERT_EQUAL_INT(col_size, decmp_size);
		decompressed_data = malloc((size_t)decmp_size); TEST_ASSERT_TRUE(decompressed_data);
		decmp_size = decompress_cmp_entiy((struct cmp_entity *)dst, NULL, NULL, decompressed_data);
		TEST_ASSERT_EQUAL_INT(col_size, decmp_size);
		TEST_ASSERT_EQUAL_HEX8_ARRAY(col, decompressed_data, decmp_size);
		free(decompressed_data);
	}

	/* error cases dst buffer to small */
	dst_capacity -= 1;
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF_, cmp_get_error_code(compress_chunk(col, col_size, NULL, NULL, dst, dst_capacity, &par)));

	free(col);
	free(dst);
}


/**
 * @test compress_chunk
 * @test decompress_cmp_entiy
 */

void test_cmp_collection_worst_case(void)
{
	struct collection_hdr *col = NULL;
	uint32_t *dst = NULL;
	uint32_t dst_capacity = 0;
	struct cmp_par par = {0};
	const uint16_t cmp_size_byte_exp = 2*sizeof(struct s_fx);
	uint32_t cmp_size_byte;
	struct s_fx *data;
	uint32_t samples = 2;
	const uint32_t col_size = COLLECTION_HDR_SIZE+samples*sizeof(*data);

	/* generate test data */
	col = malloc(col_size); TEST_ASSERT_NOT_NULL(col);
	generate_random_collection_hdr(col, DATA_TYPE_S_FX, samples);
	data = (struct s_fx *)col->entry;
	data[0].exp_flags = 0x4;
	data[0].fx = 0x0000000E;
	data[1].exp_flags = 0x4;
	data[1].fx = 0x00000016;


	{ /* compress data */
		const int exp_cmp_size_byte = NON_IMAGETTE_HEADER_SIZE + CMP_COLLECTION_FILD_SIZE
			+ COLLECTION_HDR_SIZE + cmp_size_byte_exp;

		par.cmp_mode = CMP_MODE_DIFF_ZERO;
		par.s_exp_flags = 1;
		par.s_fx = 1;

		cmp_size_byte = compress_chunk(col, col_size, NULL, NULL, dst, dst_capacity, &par);
		TEST_ASSERT_EQUAL_INT(exp_cmp_size_byte, cmp_size_byte);
		dst_capacity = (uint32_t)cmp_size_byte;
		dst = malloc(dst_capacity); TEST_ASSERT_NOT_NULL(dst);
		memset(dst, 0xFF, dst_capacity);
		cmp_size_byte = compress_chunk(col, col_size, NULL, NULL, dst, dst_capacity, &par);
		TEST_ASSERT_EQUAL_INT(exp_cmp_size_byte, cmp_size_byte);
	}

	{ /* check the compressed data */
		uint8_t *p = (uint8_t *)dst;
		uint16_t cmp_collection_size_exp = cpu_to_be16(cmp_size_byte_exp);

		/* TODO: check the entity header */
		p += NON_IMAGETTE_HEADER_SIZE;

		TEST_ASSERT_EQUAL_HEX8_ARRAY(&cmp_collection_size_exp, p, CMP_COLLECTION_FILD_SIZE);
		p += CMP_COLLECTION_FILD_SIZE;

		TEST_ASSERT(memcmp(col, p, COLLECTION_HDR_SIZE) == 0);
		p += COLLECTION_HDR_SIZE;

		TEST_ASSERT_EQUAL_HEX8(0x04, *p++);
		TEST_ASSERT_EQUAL_HEX8(0x00, *p++);
		TEST_ASSERT_EQUAL_HEX8(0x00, *p++);
		TEST_ASSERT_EQUAL_HEX8(0x00, *p++);
		TEST_ASSERT_EQUAL_HEX8(0x0E, *p++);
		TEST_ASSERT_EQUAL_HEX8(0x04, *p++);
		TEST_ASSERT_EQUAL_HEX8(0x00, *p++);
		TEST_ASSERT_EQUAL_HEX8(0x00, *p++);
		TEST_ASSERT_EQUAL_HEX8(0x00, *p++);
		TEST_ASSERT_EQUAL_HEX8(0x16, *p++);

		TEST_ASSERT_EQUAL_size_t(cmp_size_byte, p - (uint8_t *)dst);
	}
	{ /* decompress the data */
		int decmp_size;
		uint8_t *decompressed_data;

		decmp_size = decompress_cmp_entiy((struct cmp_entity *)dst, NULL, NULL, NULL);
		TEST_ASSERT_EQUAL_INT(col_size, decmp_size);
		decompressed_data = malloc((size_t)decmp_size); TEST_ASSERT_TRUE(decompressed_data);
		decmp_size = decompress_cmp_entiy((struct cmp_entity *)dst, NULL, NULL, decompressed_data);
		TEST_ASSERT_EQUAL_INT(col_size, decmp_size);
		TEST_ASSERT_EQUAL_HEX8_ARRAY(col, decompressed_data, decmp_size);
		free(decompressed_data);
	}
	free(dst);
	free(col);
}


/**
 * @test compress_chunk
 * @test decompress_cmp_entiy
 */

void test_cmp_collection_imagette_worst_case(void)
{
	struct collection_hdr *col = NULL;
	uint32_t *dst = NULL;
	struct cmp_par par = {0};
	uint16_t const cmp_size_byte_exp = 10*sizeof(uint16_t);
	uint32_t cmp_size_byte;
	uint32_t const col_size = COLLECTION_HDR_SIZE + cmp_size_byte_exp;

	{ /* generate test data */
		uint16_t *data;

		col = malloc(col_size); TEST_ASSERT_NOT_NULL(col);
		generate_random_collection_hdr(col, DATA_TYPE_IMAGETTE, 10);

		data = (void *)col->entry;
		data[0] = 0x0102;
		data[1] = 0x0304;
		data[2] = 0x0506;
		data[3] = 0x0708;
		data[4] = 0x090A;
		data[5] = 0x0B0C;
		data[6] = 0x0D0E;
		data[7] = 0x0F10;
		data[8] = 0x1112;
		data[9] = 0x1314;
	}

	{ /* compress data */
		uint32_t dst_capacity = 0;
		const int exp_cmp_size_byte = NON_IMAGETTE_HEADER_SIZE + CMP_COLLECTION_FILD_SIZE
			+ COLLECTION_HDR_SIZE + cmp_size_byte_exp;

		par.cmp_mode = CMP_MODE_DIFF_MULTI;
		par.nc_imagette = 62;

		cmp_size_byte = compress_chunk(col, col_size, NULL, NULL, dst, dst_capacity, &par);
		TEST_ASSERT_EQUAL_INT(exp_cmp_size_byte, cmp_size_byte);
		dst_capacity = (uint32_t)cmp_size_byte;
		dst = malloc(dst_capacity); TEST_ASSERT_NOT_NULL(dst);
		memset(dst, 0xFF, dst_capacity);
		cmp_size_byte = compress_chunk(col, col_size, NULL, NULL, dst, dst_capacity, &par);
		TEST_ASSERT_EQUAL_INT(exp_cmp_size_byte, cmp_size_byte);
	}

	{ /* check the compressed data */
		uint32_t i;
		uint8_t *p = (uint8_t *)dst;
		uint16_t cmp_collection_size_exp = cpu_to_be16(cmp_size_byte_exp);

		/* TODO: check the entity header */
		p += NON_IMAGETTE_HEADER_SIZE;

		TEST_ASSERT_EQUAL_HEX8_ARRAY(&cmp_collection_size_exp, p, CMP_COLLECTION_FILD_SIZE);
		p += CMP_COLLECTION_FILD_SIZE;

		TEST_ASSERT(memcmp(col, p, COLLECTION_HDR_SIZE) == 0);
		p += COLLECTION_HDR_SIZE;

		for (i = 1; i <= col_size-COLLECTION_HDR_SIZE; ++i)
			TEST_ASSERT_EQUAL_HEX8(i, *p++);

		TEST_ASSERT_EQUAL_size_t(cmp_size_byte, p - (uint8_t *)dst);
	}
	{ /* decompress the data */
		int decmp_size;
		uint8_t *decompressed_data;

		decmp_size = decompress_cmp_entiy((struct cmp_entity *)dst, NULL, NULL, NULL);
		TEST_ASSERT_EQUAL_INT(col_size, decmp_size);
		decompressed_data = malloc((size_t)decmp_size); TEST_ASSERT_TRUE(decompressed_data);
		decmp_size = decompress_cmp_entiy((struct cmp_entity *)dst, NULL, NULL, decompressed_data);
		TEST_ASSERT_EQUAL_INT(col_size, decmp_size);
		TEST_ASSERT_EQUAL_HEX8_ARRAY(col, decompressed_data, decmp_size);
		free(decompressed_data);
	}
	free(dst);
	free(col);
}


/**
 * @test compress_chunk
 * @test decompress_cmp_entiy
 */

void test_cmp_decmp_chunk_raw(void)
{
	struct cmp_par par = {0};
	struct chunk_def chunk_def[2] = {{DATA_TYPE_S_FX, 2}, {DATA_TYPE_S_FX_EFX_NCOB_ECOB, 3}};
	uint32_t chunk_size;
	size_t chunk_size_exp = 2*sizeof(struct s_fx) + 3*sizeof(struct s_fx_efx_ncob_ecob) + 2*COLLECTION_HDR_SIZE;
	void *chunk = NULL;
	uint32_t *dst = NULL;
	uint32_t dst_capacity = 0;

	/* generate test data */
	chunk_size = generate_random_chunk(chunk, chunk_def, ARRAY_SIZE(chunk_def), &MAX_USED_BITS_SAFE);
	TEST_ASSERT_EQUAL_size_t(chunk_size_exp, chunk_size);
	chunk = calloc(1, chunk_size);
	TEST_ASSERT_NOT_NULL(chunk);
	chunk_size = generate_random_chunk(chunk, chunk_def, ARRAY_SIZE(chunk_def), &MAX_USED_BITS_SAFE);
	TEST_ASSERT_EQUAL_size_t(chunk_size_exp, chunk_size);

	/* "compress" data */
	{
		size_t cmp_size_exp = GENERIC_HEADER_SIZE + chunk_size_exp;
		uint32_t cmp_size;

		par.cmp_mode = CMP_MODE_RAW;

		cmp_size = compress_chunk(chunk, chunk_size, NULL, NULL, dst, dst_capacity, &par);
		TEST_ASSERT_EQUAL_INT(cmp_size_exp, cmp_size);
		dst_capacity = (uint32_t)cmp_size;
		dst = malloc(dst_capacity); TEST_ASSERT_NOT_NULL(dst);
		cmp_size = compress_chunk(chunk, chunk_size, NULL, NULL, dst, dst_capacity, &par);
		TEST_ASSERT_EQUAL_INT(cmp_size_exp, dst_capacity);
	}

	/* check results */
	{
		uint8_t *p = (uint8_t *)dst;
		struct collection_hdr *col = (struct collection_hdr *)chunk;
		struct s_fx *cmp_data_raw_1;
		struct s_fx *data = (void *)col->entry;
		int i;

		/* TODO: Check entity header */
		TEST_ASSERT_EQUAL_HEX(chunk_size, cmp_ent_get_original_size((struct cmp_entity *)dst));
		TEST_ASSERT_EQUAL_HEX(chunk_size+GENERIC_HEADER_SIZE, cmp_ent_get_size((struct cmp_entity *)dst));

		p += GENERIC_HEADER_SIZE;

		TEST_ASSERT(memcmp(col, p, COLLECTION_HDR_SIZE) == 0);
		p += COLLECTION_HDR_SIZE;

		cmp_data_raw_1 = (struct s_fx *)p;
		TEST_ASSERT_EQUAL_HEX(data[0].exp_flags, cmp_data_raw_1[0].exp_flags);
		TEST_ASSERT_EQUAL_HEX(data[0].fx, be32_to_cpu(cmp_data_raw_1[0].fx));
		TEST_ASSERT_EQUAL_HEX(data[1].exp_flags, cmp_data_raw_1[1].exp_flags);
		TEST_ASSERT_EQUAL_HEX(data[1].fx, be32_to_cpu(cmp_data_raw_1[1].fx));
		p += 2*sizeof(struct s_fx);

		/* check 2nd collection */
		col = (struct collection_hdr *) ((char *)col + cmp_col_get_size(col));
		TEST_ASSERT(memcmp(col, p, COLLECTION_HDR_SIZE) == 0);
		p += COLLECTION_HDR_SIZE;

		for (i = 0; i < 3; i++) {
			struct s_fx_efx_ncob_ecob *raw_cmp_data2 = (struct s_fx_efx_ncob_ecob *)p;
			struct s_fx_efx_ncob_ecob *data2 = (struct s_fx_efx_ncob_ecob *)col->entry;

			TEST_ASSERT_EQUAL_HEX(data2[i].exp_flags, raw_cmp_data2[i].exp_flags);
			TEST_ASSERT_EQUAL_HEX(data2[i].fx, be32_to_cpu(raw_cmp_data2[i].fx));
			TEST_ASSERT_EQUAL_HEX(data2[i].efx, be32_to_cpu(raw_cmp_data2[i].efx));
			TEST_ASSERT_EQUAL_HEX(data2[i].ncob_x, be32_to_cpu(raw_cmp_data2[i].ncob_x));
			TEST_ASSERT_EQUAL_HEX(data2[i].ncob_y, be32_to_cpu(raw_cmp_data2[i].ncob_y));
			TEST_ASSERT_EQUAL_HEX(data2[i].ecob_x, be32_to_cpu(raw_cmp_data2[i].ecob_x));
			TEST_ASSERT_EQUAL_HEX(data2[i].ecob_y, be32_to_cpu(raw_cmp_data2[i].ecob_y));
		}
	}
	{
		void *decompressed_data = NULL;
		int decmp_size = decompress_cmp_entiy((void *)dst, NULL, NULL, decompressed_data);
		TEST_ASSERT_EQUAL_size_t(chunk_size, decmp_size);

		decompressed_data = malloc((size_t)decmp_size); TEST_ASSERT_NOT_NULL(decompressed_data);
		decmp_size = decompress_cmp_entiy((void *)dst, NULL, NULL, decompressed_data);

		TEST_ASSERT_EQUAL_INT(chunk_size, decmp_size);
		TEST_ASSERT_EQUAL_HEX8_ARRAY(chunk, decompressed_data, chunk_size);
		free(decompressed_data);
	}
	{ /* error case: buffer to small for compressed data */
		uint32_t cmp_size;

		dst_capacity -= 1;
		cmp_size = compress_chunk(chunk, chunk_size, NULL, NULL, dst, dst_capacity, &par);
		TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF_, cmp_get_error_code(cmp_size));
	}

	free(dst);
	free(chunk);
}


void test_cmp_decmp_chunk_worst_case(void)
{
	struct chunk_def chunk_def[2] = {{DATA_TYPE_S_FX, 2}, {DATA_TYPE_S_FX_EFX_NCOB_ECOB, 3}};
	enum {CHUNK_SIZE_EXP = 2*sizeof(struct s_fx) + 3*sizeof(struct s_fx_efx_ncob_ecob) + 2*COLLECTION_HDR_SIZE};
	uint32_t chunk_size = CHUNK_SIZE_EXP;
	void *chunk = NULL;
	uint32_t dst[COMPRESS_CHUNK_BOUND(CHUNK_SIZE_EXP, ARRAY_SIZE(chunk_def))/sizeof(uint32_t)];
	uint32_t cmp_size_byte = 0;
	struct cmp_par par = {0};

	{ /* generate test data */
		uint16_t s;
		uint8_t *p, i;

		chunk = malloc(chunk_size); TEST_ASSERT_NOT_NULL(chunk);
		generate_random_collection_hdr(chunk, DATA_TYPE_S_FX, 2);
		p = chunk;
		p += COLLECTION_HDR_SIZE;
		for (i = 0; i < cmp_col_get_data_length(chunk); i++)
			*p++ = i;
		generate_random_collection_hdr((struct collection_hdr *)p, DATA_TYPE_S_FX_EFX_NCOB_ECOB, 3);
		s = cmp_col_get_data_length((struct collection_hdr *)p);
		p += COLLECTION_HDR_SIZE;
		for (i = 0; i < s; i++)
			*p++ = i;
	}


	{ /* "compress" data */
		size_t cmp_size_byte_exp = NON_IMAGETTE_HEADER_SIZE + 2*CMP_COLLECTION_FILD_SIZE + CHUNK_SIZE_EXP;

		par.cmp_mode = CMP_MODE_DIFF_ZERO;
		par.s_exp_flags = 1;
		par.s_fx = 1;
		par.s_efx = 1;
		par.s_ncob = 1;
		par.s_ecob = 1;


		TEST_ASSERT_NOT_NULL(dst);
		cmp_size_byte = compress_chunk(chunk, chunk_size, NULL, NULL, dst, sizeof(dst), &par);
		TEST_ASSERT_EQUAL_INT(cmp_size_byte_exp, cmp_size_byte);
	}

	{ /* check results */
		uint8_t *p = (uint8_t *)dst;
		uint16_t cmp_collection_size_exp = cpu_to_be16(2*sizeof(struct s_fx));
		struct collection_hdr *col = (struct collection_hdr *)chunk;
		struct s_fx *cmp_data_raw_1;
		struct s_fx *data = (void *)col->entry;
		int i;

		/* TODO: Check entity header */
		p += NON_IMAGETTE_HEADER_SIZE;

		TEST_ASSERT_EQUAL_HEX8_ARRAY(&cmp_collection_size_exp, p, CMP_COLLECTION_FILD_SIZE);
		p += CMP_COLLECTION_FILD_SIZE;

		TEST_ASSERT(memcmp(col, p, COLLECTION_HDR_SIZE) == 0);
		p += COLLECTION_HDR_SIZE;

		cmp_data_raw_1 = (struct s_fx *)p;
		TEST_ASSERT_EQUAL_HEX(data[0].exp_flags, cmp_data_raw_1[0].exp_flags);
		TEST_ASSERT_EQUAL_HEX(data[0].fx, be32_to_cpu(cmp_data_raw_1[0].fx));
		TEST_ASSERT_EQUAL_HEX(data[1].exp_flags, cmp_data_raw_1[1].exp_flags);
		TEST_ASSERT_EQUAL_HEX(data[1].fx, be32_to_cpu(cmp_data_raw_1[1].fx));
		p += 2*sizeof(struct s_fx);

		/* check 2nd collection */
		cmp_collection_size_exp = cpu_to_be16(3*sizeof(struct s_fx_efx_ncob_ecob));
		TEST_ASSERT(memcmp(p, &cmp_collection_size_exp, CMP_COLLECTION_FILD_SIZE) == 0);
		p += CMP_COLLECTION_FILD_SIZE;

		col = (struct collection_hdr *) ((char *)col + cmp_col_get_size(col));
		TEST_ASSERT(memcmp(col, p, COLLECTION_HDR_SIZE) == 0);
		p += COLLECTION_HDR_SIZE;

		for (i = 0; i < 3; i++) {
			struct s_fx_efx_ncob_ecob *raw_cmp_data2 = (struct s_fx_efx_ncob_ecob *)p;
			struct s_fx_efx_ncob_ecob *data2 = (struct s_fx_efx_ncob_ecob *)col->entry;

			TEST_ASSERT_EQUAL_HEX(data2[i].exp_flags, raw_cmp_data2[i].exp_flags);
			TEST_ASSERT_EQUAL_HEX(data2[i].fx, be32_to_cpu(raw_cmp_data2[i].fx));
			TEST_ASSERT_EQUAL_HEX(data2[i].efx, be32_to_cpu(raw_cmp_data2[i].efx));
			TEST_ASSERT_EQUAL_HEX(data2[i].ncob_x, be32_to_cpu(raw_cmp_data2[i].ncob_x));
			TEST_ASSERT_EQUAL_HEX(data2[i].ncob_y, be32_to_cpu(raw_cmp_data2[i].ncob_y));
			TEST_ASSERT_EQUAL_HEX(data2[i].ecob_x, be32_to_cpu(raw_cmp_data2[i].ecob_x));
			TEST_ASSERT_EQUAL_HEX(data2[i].ecob_y, be32_to_cpu(raw_cmp_data2[i].ecob_y));
		}
	}
	{
		void *decompressed_data = NULL;
		int decmp_size = decompress_cmp_entiy((void *)dst, NULL, NULL, decompressed_data);

		TEST_ASSERT_EQUAL_size_t(chunk_size, decmp_size);
		decompressed_data = malloc((size_t)decmp_size); TEST_ASSERT_NOT_NULL(decompressed_data);
		decmp_size = decompress_cmp_entiy((void *)dst, NULL, NULL, decompressed_data);

		TEST_ASSERT_EQUAL_INT(chunk_size, decmp_size);
		TEST_ASSERT_EQUAL_HEX8_ARRAY(chunk, decompressed_data, chunk_size);
		free(decompressed_data);
	}

	/* error case: buffer to small for compressed data */
	cmp_size_byte = compress_chunk(chunk, chunk_size, NULL, NULL, dst, chunk_size, &par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF_, cmp_get_error_code(cmp_size_byte));

	free(chunk);
}


void test_cmp_decmp_diff(void)
{
	struct chunk_def chunk_def[2] = {{DATA_TYPE_S_FX, 2}, {DATA_TYPE_S_FX_EFX_NCOB_ECOB, 3}};
	uint32_t chunk_size;
	void *chunk = NULL;
	uint32_t *dst = NULL;

	{ /* generate test data */
		struct s_fx *col_data1;
		struct s_fx_efx_ncob_ecob *col_data2;
		struct collection_hdr *col;
		size_t chunk_size_exp = 2*sizeof(struct s_fx) + 3*sizeof(struct s_fx_efx_ncob_ecob) + 2*COLLECTION_HDR_SIZE;

		chunk_size = generate_random_chunk(chunk, chunk_def, ARRAY_SIZE(chunk_def), &MAX_USED_BITS_SAFE);
		TEST_ASSERT_EQUAL_size_t(chunk_size_exp, chunk_size);
		chunk = calloc(1, chunk_size);
		TEST_ASSERT_NOT_NULL(chunk);
		chunk_size = generate_random_chunk(chunk, chunk_def, ARRAY_SIZE(chunk_def), &MAX_USED_BITS_SAFE);
		TEST_ASSERT_EQUAL_size_t(chunk_size_exp, chunk_size);

		col = (struct collection_hdr *)chunk;
		col_data1 = (struct s_fx *)(col->entry);
		col_data1[0].exp_flags = 0;
		col_data1[0].fx = 0;
		col_data1[1].exp_flags = 1;
		col_data1[1].fx = 1;

		col = (struct collection_hdr *)((char *)col + cmp_col_get_size(col));
		col_data2 = (struct s_fx_efx_ncob_ecob *)(col->entry);
		col_data2[0].exp_flags = 0;
		col_data2[0].fx = 1;
		col_data2[0].efx = 2;
		col_data2[0].ncob_x = 0;
		col_data2[0].ncob_y = 1;
		col_data2[0].ecob_x = 3;
		col_data2[0].ecob_y = 7;
		col_data2[1].exp_flags = 1;
		col_data2[1].fx = 1;
		col_data2[1].efx = 1;
		col_data2[1].ncob_x = 1;
		col_data2[1].ncob_y = 2;
		col_data2[1].ecob_x = 1;
		col_data2[1].ecob_y = 1;
		col_data2[2].exp_flags = 2;
		col_data2[2].fx = 2;
		col_data2[2].efx = 2;
		col_data2[2].ncob_x = 2;
		col_data2[2].ncob_y = 45;
		col_data2[2].ecob_x = 2;
		col_data2[2].ecob_y = 2;
	}

	{ /* compress data */
		struct cmp_par par = {0};
		uint32_t dst_capacity = 0;
		uint32_t cmp_size;

		par.cmp_mode = CMP_MODE_DIFF_ZERO;
		par.s_exp_flags = 1;
		par.s_fx = 2;
		par.s_efx = 3;
		par.s_ncob = 4;
		par.s_ecob = 5;


		cmp_size = compress_chunk(chunk, chunk_size, NULL, NULL, dst, dst_capacity, &par);
		TEST_ASSERT_GREATER_THAN_INT(0, cmp_size);
		dst_capacity = (uint32_t)ROUND_UP_TO_MULTIPLE_OF_4(cmp_size);
		dst = malloc(dst_capacity);
		TEST_ASSERT_NOT_NULL(dst);
		cmp_size = compress_chunk(chunk, chunk_size, NULL, NULL, dst, dst_capacity, &par);
		TEST_ASSERT_GREATER_THAN_INT(0, cmp_size);
	}
	{
		void *decompressed_data = NULL;
		int decmp_size = decompress_cmp_entiy((void *)dst, NULL, NULL,
						      decompressed_data);
		TEST_ASSERT_EQUAL_size_t(chunk_size, decmp_size);
		decompressed_data = malloc((size_t)decmp_size); TEST_ASSERT_NOT_NULL(decompressed_data);
		decmp_size = decompress_cmp_entiy((void *)dst, NULL, NULL,
						  decompressed_data);

		TEST_ASSERT_EQUAL_INT(chunk_size, decmp_size);
		TEST_ASSERT_EQUAL_HEX8_ARRAY(chunk, decompressed_data, chunk_size);
		free(decompressed_data);
	}
	free(dst);
	free(chunk);
}

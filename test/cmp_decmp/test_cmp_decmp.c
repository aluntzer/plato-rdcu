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
#include <math.h>

#include <unity.h>
#include "../test_common/test_common.h"
#include "../test_common/chunk_round_trip.h"
#include "cmp_support.h"

#include <cmp_icu.h>
#include <cmp_chunk.h>
#include <decmp.h>
#include <cmp_data_types.h>
#include <cmp_rdcu_cfg.h>
#include <leon_inttypes.h>
#include <byteorder.h>
#include <cmp_error.h>
#include <cmp_max_used_bits.h>
#include <cmp_cal_up_model.h>

#ifndef ICU_ASW
#  if defined __has_include
#    if __has_include(<time.h>) && !defined(__sparc__)
#      include <time.h>
#      include <unistd.h>
#      define HAS_TIME_H 1
#    endif
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


/**
 * @brief generate a geometric distribution (bernoulli trial with probability p)
 *
 * prob(k) =  p (1 - p)^k for k = 0, 1, 2, 3, ...
 *
 * @param p	probability of geometric distribution (0 < p <= 1)
 *
 * @returns random number following a geometric distribution
 */

static uint32_t cmp_rand_geometric(double p)
{
	double u = ldexp(cmp_rand32(), -32); /*see: https://www.pcg-random.org/using-pcg-c-basic.html */

	if (p >= 1.0)
		return 0;

	return (uint32_t)(log(u) / log(1 - p));
}


/**
 * @brief generate geometric distribution data with a specified number of bits
 *
 * @param n_bits	number of bits for the output (1 <= n_bits <= 32)
 * @param extra		pointer to a double containing the probability of
 *			geometric distribution (0 < p <= 1)
 *
 * @returns random number following a geometric distribution, masked with n_bits
 */

static uint32_t gen_geometric_data(uint32_t n_bits, void *extra)
{
	double *p = (double *)extra;
	uint32_t mask;

	TEST_ASSERT(n_bits > 0);
	TEST_ASSERT(n_bits <= 32);
	TEST_ASSERT_NOT_NULL(p);
	TEST_ASSERT(*p > 0);
	TEST_ASSERT(*p <= 1.0);

	mask = ~0U >> (32 - n_bits);
	return cmp_rand_geometric(*p) & mask;
}


/**
 * @brief Generate uniform distribution data with a specified number of bits
 *
 * @param n_bits	number of bits for the output (1 <= n_bits <= 32)
 * @param unused	unused parameter
 *
 * @returns random number following a uniform distribution, masked with n_bits
 */

static uint32_t gen_uniform_data(uint32_t n_bits, void *unused UNUSED)
{
	return cmp_rand_nbits(n_bits);
}


static size_t gen_ima_data(uint16_t *data, enum cmp_data_type data_type,
			   uint32_t samples,
			   uint32_t (*gen_data_f)(uint32_t max_data_bits, void *extra),
			   void *extra)
{
	uint32_t i;

	if (data) {
		uint32_t max_data_bits;

		switch (data_type) {
		case DATA_TYPE_IMAGETTE:
		case DATA_TYPE_IMAGETTE_ADAPTIVE:
			max_data_bits = MAX_USED_BITS.nc_imagette;
			break;
		case DATA_TYPE_SAT_IMAGETTE:
		case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
			max_data_bits = MAX_USED_BITS.saturated_imagette;
			break;
		case DATA_TYPE_F_CAM_IMAGETTE:
		case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
			max_data_bits = MAX_USED_BITS.fc_imagette;
			break;
		default:
			TEST_FAIL();
		}
		for (i = 0; i < samples; i++)
			data[i] = (uint16_t)gen_data_f(max_data_bits, extra);
	}
	return sizeof(*data) * samples;
}


static size_t gen_nc_offset_data(struct offset *data, uint32_t samples,
				 uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].mean = gen_data_f(MAX_USED_BITS.nc_offset_mean, extra);
			data[i].variance = gen_data_f(MAX_USED_BITS.nc_offset_variance, extra);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_fc_offset_data(struct offset *data, uint32_t samples,
				 uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].mean = gen_data_f(MAX_USED_BITS.fc_offset_mean, extra);
			data[i].variance = gen_data_f(MAX_USED_BITS.fc_offset_variance, extra);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_nc_background_data(struct background *data, uint32_t samples,
				     uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].mean = gen_data_f(MAX_USED_BITS.nc_background_mean, extra);
			data[i].variance = gen_data_f(MAX_USED_BITS.nc_background_variance, extra);
			data[i].outlier_pixels =
				(__typeof__(data[i].outlier_pixels))gen_data_f(MAX_USED_BITS.nc_background_outlier_pixels, extra);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_fc_background_data(struct background *data, uint32_t samples,
				     uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].mean = gen_data_f(MAX_USED_BITS.fc_background_mean, extra);
			data[i].variance = gen_data_f(MAX_USED_BITS.fc_background_variance, extra);
			data[i].outlier_pixels =
				(__typeof__(data[i].outlier_pixels))gen_data_f(MAX_USED_BITS.fc_background_outlier_pixels, extra);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_smearing_data(struct smearing *data, uint32_t samples,
				uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].mean = gen_data_f(MAX_USED_BITS.smearing_mean, extra);
			data[i].variance_mean =
				(__typeof__(data[i].variance_mean))gen_data_f(MAX_USED_BITS.smearing_variance_mean, extra);
			data[i].outlier_pixels =
				(__typeof__(data[i].outlier_pixels))gen_data_f(MAX_USED_BITS.smearing_outlier_pixels, extra);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_s_fx_data(struct s_fx *data, uint32_t samples,
			    uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].exp_flags =
				(__typeof__(data[i].exp_flags))gen_data_f(MAX_USED_BITS.s_exp_flags, extra);
			data[i].fx = gen_data_f(MAX_USED_BITS.s_fx, extra);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_s_fx_efx_data(struct s_fx_efx *data, uint32_t samples,
				uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].exp_flags =
				(__typeof__(data[i].exp_flags))gen_data_f(MAX_USED_BITS.s_exp_flags, extra);
			data[i].fx = gen_data_f(MAX_USED_BITS.s_fx, extra);
			data[i].efx = gen_data_f(MAX_USED_BITS.s_efx, extra);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_s_fx_ncob_data(struct s_fx_ncob *data, uint32_t samples,
				 uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].exp_flags =
				(__typeof__(data[i].exp_flags))gen_data_f(MAX_USED_BITS.s_exp_flags, extra);
			data[i].fx = gen_data_f(MAX_USED_BITS.s_fx, extra);
			data[i].ncob_x = gen_data_f(MAX_USED_BITS.s_ncob, extra);
			data[i].ncob_y = gen_data_f(MAX_USED_BITS.s_ncob, extra);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_s_fx_efx_ncob_ecob_data(struct s_fx_efx_ncob_ecob *data, uint32_t samples,
					  uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].exp_flags =
				(__typeof__(data[i].exp_flags))gen_data_f(MAX_USED_BITS.s_exp_flags, extra);
			data[i].fx = gen_data_f(MAX_USED_BITS.s_fx, extra);
			data[i].ncob_x = gen_data_f(MAX_USED_BITS.s_ncob, extra);
			data[i].ncob_y = gen_data_f(MAX_USED_BITS.s_ncob, extra);
			data[i].efx = gen_data_f(MAX_USED_BITS.s_efx, extra);
			data[i].ecob_x = gen_data_f(MAX_USED_BITS.s_ecob, extra);
			data[i].ecob_y = gen_data_f(MAX_USED_BITS.s_ecob, extra);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_f_fx_data(struct f_fx *data, uint32_t samples,
			    uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	uint32_t i;

	if (data)
		for (i = 0; i < samples; i++)
			data[i].fx = gen_data_f(MAX_USED_BITS.f_fx, extra);
	return sizeof(*data) * samples;
}


static size_t gen_f_fx_efx_data(struct f_fx_efx *data, uint32_t samples,
				uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].fx = gen_data_f(MAX_USED_BITS.f_fx, extra);
			data[i].efx = gen_data_f(MAX_USED_BITS.f_efx, extra);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_f_fx_ncob_data(struct f_fx_ncob *data, uint32_t samples,
				 uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].fx = gen_data_f(MAX_USED_BITS.f_fx, extra);
			data[i].ncob_x = gen_data_f(MAX_USED_BITS.f_ncob, extra);
			data[i].ncob_y = gen_data_f(MAX_USED_BITS.f_ncob, extra);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_f_fx_efx_ncob_ecob_data(struct f_fx_efx_ncob_ecob *data, uint32_t samples,
					  uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].fx = gen_data_f(MAX_USED_BITS.f_fx, extra);
			data[i].ncob_x = gen_data_f(MAX_USED_BITS.f_ncob, extra);
			data[i].ncob_y = gen_data_f(MAX_USED_BITS.f_ncob, extra);
			data[i].efx = gen_data_f(MAX_USED_BITS.f_efx, extra);
			data[i].ecob_x = gen_data_f(MAX_USED_BITS.f_ecob, extra);
			data[i].ecob_y = gen_data_f(MAX_USED_BITS.f_ecob, extra);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_l_fx_data(struct l_fx *data, uint32_t samples,
			    uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].exp_flags = gen_data_f(MAX_USED_BITS.l_exp_flags, extra);
			data[i].fx = gen_data_f(MAX_USED_BITS.l_fx, extra);
			data[i].fx_variance = gen_data_f(MAX_USED_BITS.l_fx_cob_variance, extra);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_l_fx_efx_data(struct l_fx_efx *data, uint32_t samples,
				uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	uint32_t i;

	if (data) {
		for (i = 0; i < samples; i++) {
			data[i].exp_flags = gen_data_f(MAX_USED_BITS.l_exp_flags, extra);
			data[i].fx = gen_data_f(MAX_USED_BITS.l_fx, extra);
			data[i].efx = gen_data_f(MAX_USED_BITS.l_efx, extra);
			data[i].fx_variance = gen_data_f(MAX_USED_BITS.l_fx_cob_variance, extra);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_l_fx_ncob_data(struct l_fx_ncob *data, uint32_t samples,
				 uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	if (data) {
		uint32_t i;

		for (i = 0; i < samples; i++) {
			data[i].exp_flags = gen_data_f(MAX_USED_BITS.l_exp_flags, extra);
			data[i].fx = gen_data_f(MAX_USED_BITS.l_fx, extra);
			data[i].ncob_x = gen_data_f(MAX_USED_BITS.l_ncob, extra);
			data[i].ncob_y = gen_data_f(MAX_USED_BITS.l_ncob, extra);
			data[i].fx_variance = gen_data_f(MAX_USED_BITS.l_fx_cob_variance, extra);
			data[i].cob_x_variance = gen_data_f(MAX_USED_BITS.l_fx_cob_variance, extra);
			data[i].cob_y_variance = gen_data_f(MAX_USED_BITS.l_fx_cob_variance, extra);
		}
	}
	return sizeof(*data) * samples;
}


static size_t gen_l_fx_efx_ncob_ecob_data(struct l_fx_efx_ncob_ecob *data, uint32_t samples,
					  uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	if (data) {
		uint32_t i;

		for (i = 0; i < samples; i++) {
			data[i].exp_flags = gen_data_f(MAX_USED_BITS.l_exp_flags, extra);
			data[i].fx = gen_data_f(MAX_USED_BITS.l_fx, extra);
			data[i].ncob_x = gen_data_f(MAX_USED_BITS.l_ncob, extra);
			data[i].ncob_y = gen_data_f(MAX_USED_BITS.l_ncob, extra);
			data[i].efx = gen_data_f(MAX_USED_BITS.l_efx, extra);
			data[i].ecob_x = gen_data_f(MAX_USED_BITS.l_ecob, extra);
			data[i].ecob_y = gen_data_f(MAX_USED_BITS.l_ecob, extra);
			data[i].fx_variance = gen_data_f(MAX_USED_BITS.l_fx_cob_variance, extra);
			data[i].cob_x_variance = gen_data_f(MAX_USED_BITS.l_fx_cob_variance, extra);
			data[i].cob_y_variance = gen_data_f(MAX_USED_BITS.l_fx_cob_variance, extra);
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
 * @param gen_data_f	function pointer to a data generation function
 * @param extra		pointer to additional data required by the data
 *			generation function
 *
 * @return the size of the generated random collection in bytes
 */

size_t generate_random_collection(struct collection_hdr *col, enum cmp_data_type data_type,
				  uint32_t samples, uint32_t (*gen_data_f)(uint32_t, void *),
				  void *extra)
{
	size_t size;
	void *science_data = NULL;

	if (col)
		science_data = col->entry;

	size = generate_random_collection_hdr(col, data_type, samples);

	switch (data_type) {
	case DATA_TYPE_IMAGETTE:
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_SAT_IMAGETTE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_F_CAM_IMAGETTE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		size += gen_ima_data(science_data, data_type, samples,
				     gen_data_f, extra);
		break;
	case DATA_TYPE_OFFSET:
		size += gen_nc_offset_data(science_data, samples, gen_data_f, extra);
		break;
	case DATA_TYPE_BACKGROUND:
		size += gen_nc_background_data(science_data, samples,
					       gen_data_f, extra);
		break;
	case DATA_TYPE_SMEARING:
		size += gen_smearing_data(science_data, samples, gen_data_f, extra);
		break;
	case DATA_TYPE_S_FX:
		size += gen_s_fx_data(science_data, samples, gen_data_f, extra);
		break;
	case DATA_TYPE_S_FX_EFX:
		size += gen_s_fx_efx_data(science_data, samples, gen_data_f, extra);
		break;
	case DATA_TYPE_S_FX_NCOB:
		size += gen_s_fx_ncob_data(science_data, samples, gen_data_f, extra);
		break;
	case DATA_TYPE_S_FX_EFX_NCOB_ECOB:
		size += gen_s_fx_efx_ncob_ecob_data(science_data, samples,
						    gen_data_f, extra);
		break;
	case DATA_TYPE_L_FX:
		size += gen_l_fx_data(science_data, samples, gen_data_f, extra);
		break;
	case DATA_TYPE_L_FX_EFX:
		size += gen_l_fx_efx_data(science_data, samples, gen_data_f, extra);
		break;
	case DATA_TYPE_L_FX_NCOB:
		size += gen_l_fx_ncob_data(science_data, samples, gen_data_f, extra);
		break;
	case DATA_TYPE_L_FX_EFX_NCOB_ECOB:
		size += gen_l_fx_efx_ncob_ecob_data(science_data, samples,
						    gen_data_f, extra);
		break;
	case DATA_TYPE_F_FX:
		size += gen_f_fx_data(science_data, samples, gen_data_f, extra);
		break;
	case DATA_TYPE_F_FX_EFX:
		size += gen_f_fx_efx_data(science_data, samples, gen_data_f, extra);
		break;
	case DATA_TYPE_F_FX_NCOB:
		size += gen_f_fx_ncob_data(science_data, samples, gen_data_f, extra);
		break;
	case DATA_TYPE_F_FX_EFX_NCOB_ECOB:
		size += gen_f_fx_efx_ncob_ecob_data(science_data, samples,
						    gen_data_f, extra);
		break;
	case DATA_TYPE_F_CAM_OFFSET:
		size += gen_fc_offset_data(science_data, samples, gen_data_f, extra);
		break;
	case DATA_TYPE_F_CAM_BACKGROUND:
		size += gen_fc_background_data(science_data, samples,
					       gen_data_f, extra);
		break;
	case DATA_TYPE_CHUNK:
	case DATA_TYPE_UNKNOWN:
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
 * @param gen_data_f	function pointer to a data generation function
 * @param extra		pointer to additional data required by the data
 *			generation function
 *
 * @return the size of the generated random chunk in bytes
 */

static uint32_t generate_random_chunk(void *chunk, struct chunk_def col_array[],
				      size_t array_elements,
				      uint32_t (*gen_data_f)(uint32_t, void*), void *extra)
{
	size_t i;
	uint32_t chunk_size = 0;
	struct collection_hdr *col = NULL;

	for (i = 0; i < array_elements; i++) {
		if (chunk)
			col = (struct collection_hdr *)((uint8_t *)chunk + chunk_size);

		chunk_size += generate_random_collection(col, col_array[i].data_type,
							 col_array[i].samples, gen_data_f,
							 extra);
	}
	return chunk_size;
}


/**
 * @brief generate random compression configuration
 *
 * @param rcfg	pointer to a RDCU compression configuration
 */

void generate_random_rdcu_cfg(struct rdcu_cfg *rcfg)
{
		rcfg->golomb_par = cmp_rand_between(MIN_IMA_GOLOMB_PAR, MAX_IMA_GOLOMB_PAR);
		rcfg->ap1_golomb_par = cmp_rand_between(MIN_IMA_GOLOMB_PAR, MAX_IMA_GOLOMB_PAR);
		rcfg->ap2_golomb_par = cmp_rand_between(MIN_IMA_GOLOMB_PAR, MAX_IMA_GOLOMB_PAR);
		rcfg->spill = cmp_rand_between(MIN_IMA_SPILL, cmp_ima_max_spill(rcfg->golomb_par));
		rcfg->ap1_spill = cmp_rand_between(MIN_IMA_SPILL, cmp_ima_max_spill(rcfg->ap1_golomb_par));
		rcfg->ap2_spill = cmp_rand_between(MIN_IMA_SPILL, cmp_ima_max_spill(rcfg->ap2_golomb_par));
}


/**
 * @brief generate random chunk compression parameters
 *
 * @param par	pointer where to store the chunk compression
 */

void generate_random_cmp_par(struct cmp_par *par)
{
	if (!par)
		return;

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


/**
 * @brief compress the given configuration and decompress it afterwards; finally
 *	compare the results
 *
 * @param rcfg	pointer to a RDCU compression configuration
 */

void compression_decompression_like_rdcu(struct rdcu_cfg *rcfg)
{
	int s, error;
	uint32_t cmp_size_bits, data_size, cmp_data_size, cmp_ent_size;
	struct cmp_entity *ent;
	void *decompressed_data;
	static void *model_of_data;
	void *updated_model = NULL;
	struct cmp_info info;

	if (!rcfg) {
		free(model_of_data);
		return;
	}

	TEST_ASSERT_NOT_NULL(rcfg);

	TEST_ASSERT_NULL(rcfg->icu_output_buf);

	data_size = rcfg->samples * sizeof(uint16_t);
	TEST_ASSERT_NOT_EQUAL_UINT(0, data_size);

	/* create a compression entity */
	cmp_data_size = rcfg->buffer_length * sizeof(uint16_t);
	TEST_ASSERT_NOT_EQUAL_UINT(0, cmp_data_size);

	cmp_ent_size = cmp_ent_create(NULL, DATA_TYPE_IMAGETTE, rcfg->cmp_mode == CMP_MODE_RAW, cmp_data_size);
	TEST_ASSERT_NOT_EQUAL_UINT(0, cmp_ent_size);
	ent = malloc(cmp_ent_size); TEST_ASSERT_TRUE(ent);
	cmp_ent_size = cmp_ent_create(ent, DATA_TYPE_IMAGETTE, rcfg->cmp_mode == CMP_MODE_RAW, cmp_data_size);
	TEST_ASSERT_NOT_EQUAL_UINT(0, cmp_ent_size);

	/* we put the compressed data direct into the compression entity */
	rcfg->icu_output_buf = cmp_ent_get_data_buf(ent);
	TEST_ASSERT_NOT_NULL(rcfg->icu_output_buf);

	/* now compress the data */
	cmp_size_bits = compress_like_rdcu(rcfg, &info);
	TEST_ASSERT(!cmp_is_error(cmp_size_bits));

	/* put the compression parameters in the entity header */
	cmp_ent_size = cmp_ent_create(ent, DATA_TYPE_IMAGETTE, rcfg->cmp_mode == CMP_MODE_RAW,
				      cmp_bit_to_byte(cmp_size_bits));
	TEST_ASSERT_NOT_EQUAL_UINT(0, cmp_ent_size);
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, rcfg);
	TEST_ASSERT_FALSE(error);

	/* allocate the buffers for decompression */
	TEST_ASSERT_NOT_EQUAL_INT(0, data_size);
	s = decompress_cmp_entiy(ent, model_of_data, NULL, NULL);
	decompressed_data = malloc((size_t)s); TEST_ASSERT_NOT_NULL(decompressed_data);

	if (model_mode_is_used(rcfg->cmp_mode)) {
		updated_model = malloc(data_size);
		TEST_ASSERT_NOT_NULL(updated_model);
	}

	/* now we try to decompress the data */
	s = decompress_cmp_entiy(ent, model_of_data, updated_model, decompressed_data);
	TEST_ASSERT_EQUAL_INT(data_size, s);
	TEST_ASSERT_FALSE(memcmp(decompressed_data, rcfg->input_buf, data_size));

	if (model_mode_is_used(rcfg->cmp_mode)) {
		TEST_ASSERT_NOT_NULL(updated_model);
		TEST_ASSERT_NOT_NULL(model_of_data);
		TEST_ASSERT_FALSE(memcmp(updated_model, rcfg->icu_new_model_buf, data_size));
		memcpy(model_of_data, updated_model, data_size);
	} else { /* non-model mode */
		/* reset model */
		free(model_of_data);
		model_of_data = malloc(data_size);
		memcpy(model_of_data, decompressed_data, data_size);
	}

	rcfg->icu_output_buf = NULL;
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
	enum cmp_data_type data_type = DATA_TYPE_IMAGETTE;
	enum cmp_mode cmp_mode;
	struct rdcu_cfg rcfg;
	enum {
		MAX_DATA_TO_COMPRESS_SIZE = 0x1000B,
		CMP_BUFFER_FAKTOR = 3 /* compression data buffer size / data to compress buffer size */
	};
	void *data_to_compress1 = malloc(MAX_DATA_TO_COMPRESS_SIZE);
	void *data_to_compress2 = malloc(MAX_DATA_TO_COMPRESS_SIZE);
	void *updated_model = calloc(1, MAX_DATA_TO_COMPRESS_SIZE);
	int run;

	for (run = 0; run < 2; run++) {
		uint32_t (*gen_data_f)(uint32_t max_data_bits, void *extra);
		void *extra;
		double p = 0.01;
		size_t size;
		uint32_t samples, model_value;

		/* generate random data*/
		switch (run) {
		case 0:
			gen_data_f = gen_uniform_data;
			extra = NULL;
			break;
		case 1:
			gen_data_f = gen_geometric_data;
			extra = &p;
			break;
		default:
			TEST_FAIL();
		}
		samples = cmp_rand_between(1, UINT16_MAX/size_of_a_sample(data_type));
		model_value = cmp_rand_between(0, MAX_MODEL_VALUE);

		if (!rdcu_supported_data_type_is_used(data_type))
			continue;

		size = gen_ima_data(NULL, data_type, samples, gen_data_f, extra);
		TEST_ASSERT(size <= MAX_DATA_TO_COMPRESS_SIZE);
		size = gen_ima_data(data_to_compress1, data_type, samples, gen_data_f, extra);
		TEST_ASSERT(size <= MAX_DATA_TO_COMPRESS_SIZE);
		size = gen_ima_data(data_to_compress2, data_type, samples, gen_data_f, extra);
		TEST_ASSERT(size <= MAX_DATA_TO_COMPRESS_SIZE);
		/* for (cmp_mode = CMP_MODE_RAW; cmp_mode <= CMP_MODE_STUFF; cmp_mode++) { */
		for (cmp_mode = CMP_MODE_RAW; cmp_mode <= CMP_MODE_DIFF_MULTI; cmp_mode++) {
			/* printf("cmp_mode: %i\n", cmp_mode); */
			int error = rdcu_cfg_create(&rcfg, cmp_mode,
						    model_value, CMP_LOSSLESS);
			TEST_ASSERT_FALSE(error);

			generate_random_rdcu_cfg(&rcfg);

			if (!model_mode_is_used(cmp_mode)) {
				rcfg.input_buf = data_to_compress1;
				rcfg.samples = samples;
				rcfg.model_buf = NULL;
				rcfg.icu_new_model_buf = NULL;
				rcfg.icu_output_buf = NULL;
				rcfg.buffer_length = samples*CMP_BUFFER_FAKTOR;
			} else {
				rcfg.input_buf = data_to_compress2;
				rcfg.samples = samples;
				rcfg.model_buf = data_to_compress1;
				rcfg.icu_new_model_buf = updated_model;
				rcfg.icu_output_buf = NULL;
				rcfg.buffer_length = samples*CMP_BUFFER_FAKTOR;
			}

			compression_decompression_like_rdcu(&rcfg);
		}
	}
	compression_decompression_like_rdcu(NULL);
	free(data_to_compress1);
	free(data_to_compress2);
	free(updated_model);
}


/**
 * @test icu_compress_data
 */

void test_random_compression_decompress_rdcu_data(void)
{
	struct rdcu_cfg rcfg;
	struct cmp_info info = {0};
	int error, s, i;
	uint32_t cmp_size_bits;
	void *compressed_data;
	uint16_t *decompressed_data;
	enum {N_SAMPLES = 5};
	uint16_t data[N_SAMPLES] = {0, UINT16_MAX, INT16_MAX, 42, 23};
	enum {
		CMP_BUFFER_FAKTOR = 2 /* compression data buffer size / data to compress buffer size */
	};

	compressed_data = malloc(sizeof(uint16_t)*N_SAMPLES*CMP_BUFFER_FAKTOR);
	error = rdcu_cfg_create(&rcfg, CMP_MODE_RAW, 8, CMP_LOSSLESS);
	TEST_ASSERT_FALSE(error);

	rcfg.input_buf = data;
	rcfg.samples = N_SAMPLES;
	rcfg.icu_output_buf = compressed_data;
	rcfg.buffer_length = CMP_BUFFER_FAKTOR*N_SAMPLES;

	cmp_size_bits = compress_like_rdcu(&rcfg, &info);
	TEST_ASSERT(!cmp_is_error(cmp_size_bits));

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
	uint32_t cmp_data_capacity = COMPRESS_CHUNK_BOUND(MAX_DATA_TO_COMPRESS_SIZE, 1U);
	int run;
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

	for (run = 0; run < 2; run++) {
		uint32_t (*gen_data_f)(uint32_t max_data_bits, void *extra);
		void *extra;
		double p = 0.01;

		switch (run) {
		case 0:
			gen_data_f = gen_uniform_data;
			extra = NULL;
			break;
		case 1:
			gen_data_f = gen_geometric_data;
			extra = &p;
			break;
		default:
			TEST_FAIL();
		}

		for (data_type = 1; data_type <= DATA_TYPE_F_CAM_BACKGROUND; data_type++) {
			/* printf("%s\n", data_type2string(data_type)); */
			/* generate random data*/
			size_t size;
			uint32_t samples = cmp_rand_between(1, UINT16_MAX/size_of_a_sample(data_type) - COLLECTION_HDR_SIZE);

			size = generate_random_collection(NULL, data_type, samples, gen_data_f, extra);
			TEST_ASSERT(size <= MAX_DATA_TO_COMPRESS_SIZE);
			size = generate_random_collection(data, data_type, samples, gen_data_f, extra);
			TEST_ASSERT(size <= MAX_DATA_TO_COMPRESS_SIZE);
			size = generate_random_collection(model, data_type, samples, gen_data_f, extra);
			TEST_ASSERT(size <= MAX_DATA_TO_COMPRESS_SIZE);

			for (cmp_mode = CMP_MODE_RAW; cmp_mode <= CMP_MODE_DIFF_MULTI; cmp_mode++) {
				struct cmp_par par;
				uint32_t cmp_size, cmp_size2;

				cmp_data_capacity = COMPRESS_CHUNK_BOUND(MAX_DATA_TO_COMPRESS_SIZE, 1U);

				generate_random_cmp_par(&par);
				par.cmp_mode = cmp_mode;
				par.lossy_par = CMP_LOSSLESS;

				cmp_size = chunk_round_trip(data, (uint32_t)size, model, updated_model,
							   cmp_data, cmp_data_capacity,
							   &par, 1, model_mode_is_used(par.cmp_mode));
				/* No chunk is defined for fast cadence subservices */
				if (data_type == DATA_TYPE_F_FX || data_type == DATA_TYPE_F_FX_EFX ||
				    data_type == DATA_TYPE_F_FX_NCOB || data_type == DATA_TYPE_F_FX_EFX_NCOB_ECOB) {
					TEST_ASSERT_EQUAL_INT(CMP_ERROR_COL_SUBSERVICE_UNSUPPORTED, cmp_get_error_code(cmp_size));
					continue;
				} else {
					TEST_ASSERT_EQUAL_INT(CMP_ERROR_NO_ERROR, cmp_get_error_code(cmp_size));
				}


				/* test with minimum compressed data capacity */
				cmp_data_capacity = ROUND_UP_TO_MULTIPLE_OF_4(cmp_size);
				cmp_size2 = chunk_round_trip(data, (uint32_t)size, model, updated_model,
							     cmp_data, cmp_data_capacity,
							     &par, 1, model_mode_is_used(par.cmp_mode));

				TEST_ASSERT_EQUAL_UINT(cmp_size, cmp_size2);
				TEST_ASSERT_FALSE(cmp_is_error(cmp_size2));

				/* error: the capacity for the compressed data is to small */
				for (cmp_data_capacity = cmp_size2 - 1;
				     cmp_data_capacity <= cmp_size - 32  && cmp_data_capacity > 1;
				     cmp_data_capacity--) {
					cmp_size = chunk_round_trip(data, (uint32_t)size, model, updated_model,
								    cmp_data, cmp_data_capacity,
								    &par, 1, model_mode_is_used(par.cmp_mode));
					TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(cmp_size));
				}

				cmp_data_capacity = cmp_size2 - cmp_rand_between(1, cmp_size2);
				cmp_size = chunk_round_trip(data, (uint32_t)size, model, updated_model,
							    cmp_data, cmp_data_capacity,
							    &par, 1, model_mode_is_used(par.cmp_mode));
				TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(cmp_size));
			}
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
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER,
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
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(compress_chunk(col, col_size, NULL, NULL, dst, dst_capacity, &par)));

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
	chunk_size = generate_random_chunk(chunk, chunk_def, ARRAY_SIZE(chunk_def), gen_uniform_data, NULL);
	TEST_ASSERT_EQUAL_size_t(chunk_size_exp, chunk_size);
	chunk = calloc(1, chunk_size);
	TEST_ASSERT_NOT_NULL(chunk);
	chunk_size = generate_random_chunk(chunk, chunk_def, ARRAY_SIZE(chunk_def), gen_uniform_data, NULL);
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
		TEST_ASSERT_EQUAL_INT(cmp_size_exp, cmp_size);
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
		TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(cmp_size));
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

		chunk = calloc(1, chunk_size); TEST_ASSERT_NOT_NULL(chunk);
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
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(cmp_size_byte));

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

		chunk_size = generate_random_chunk(chunk, chunk_def, ARRAY_SIZE(chunk_def), gen_uniform_data, NULL);
		TEST_ASSERT_EQUAL_size_t(chunk_size_exp, chunk_size);
		chunk = calloc(1, chunk_size);
		TEST_ASSERT_NOT_NULL(chunk);
		chunk_size = generate_random_chunk(chunk, chunk_def, ARRAY_SIZE(chunk_def), gen_uniform_data, NULL);
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

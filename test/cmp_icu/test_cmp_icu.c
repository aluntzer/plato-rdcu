/**
 * @file   test_cmp_icu.c
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
 * @brief software compression tests
 */


#include <stdlib.h>
#include <inttypes.h>

#if defined __has_include
#  if __has_include(<time.h>)
#    include <time.h>
#    include <unistd.h>
#    define HAS_TIME_H 1
#  endif
#endif

#include "unity.h"
#include "../test_common/test_common.h"

#include <cmp_icu.h>
#include "../../lib/icu_compress/cmp_icu.c" /* this is a hack to test static functions */


/**
 * @brief  Seeds the pseudo-random number generator used by rand()
 */

void setUp(void)
{
	uint64_t seed;
	static int n;

#if HAS_TIME_H
	seed = (uint64_t)(time(NULL) ^ getpid()  ^ (intptr_t)&setUp);
#else
	seed = 1;
#endif

	if (!n) {
		n = 1;
		cmp_rand_seed(seed);
		printf("seed: %"PRIu64"\n", seed);
	}
}


/**
 * @test cmp_cfg_icu_create
 */

void test_cmp_cfg_icu_create(void)
{
	struct cmp_cfg cfg;
	enum cmp_data_type data_type;
	enum cmp_mode cmp_mode;
	uint32_t model_value, lossy_par;
	/* TODO: change that when DATA_TYPE_BACKGROUND and
	 * DATA_TYPE_F_CAM_BACKGROUND are implemented */
	const enum cmp_data_type biggest_data_type = DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE;

	/* wrong data type tests */
	data_type = DATA_TYPE_UNKNOWN; /* not valid data type */
	cmp_mode = CMP_MODE_RAW;
	model_value = 0;
	lossy_par = CMP_LOSSLESS;
	cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL_INT(DATA_TYPE_UNKNOWN, cfg.data_type);
	memset(&cfg, 0, sizeof(cfg));

	data_type = biggest_data_type + 1; /* not valid data type */
	cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL_INT(DATA_TYPE_UNKNOWN, cfg.data_type);
	memset(&cfg, 0, sizeof(cfg));

	data_type = biggest_data_type;
	cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL_INT(biggest_data_type, cfg.data_type);
	TEST_ASSERT_EQUAL_INT(CMP_MODE_RAW, cfg.cmp_mode);
	TEST_ASSERT_EQUAL_INT(0, cfg.model_value);
	TEST_ASSERT_EQUAL_INT(0, cfg.round);
	memset(&cfg, 0, sizeof(cfg));

	/* this should work */
	data_type = DATA_TYPE_IMAGETTE;
	cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL_INT(DATA_TYPE_IMAGETTE, cfg.data_type);
	TEST_ASSERT_EQUAL_INT(CMP_MODE_RAW, cfg.cmp_mode);
	TEST_ASSERT_EQUAL_INT(0, cfg.model_value);
	TEST_ASSERT_EQUAL_INT(0, cfg.round);
	memset(&cfg, 0, sizeof(cfg));

	/* wrong compression mode tests */
	cmp_mode = CMP_MODE_STUFF + 1;
	cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL_INT(DATA_TYPE_UNKNOWN, cfg.data_type);
	memset(&cfg, 0, sizeof(cfg));

	cmp_mode = (enum cmp_mode)-1;
	cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL_INT(DATA_TYPE_UNKNOWN, cfg.data_type);
	memset(&cfg, 0, sizeof(cfg));

	/* this should work */
	cmp_mode = CMP_MODE_STUFF;
	cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL_INT(DATA_TYPE_IMAGETTE, cfg.data_type);
	TEST_ASSERT_EQUAL_INT(CMP_MODE_STUFF, cfg.cmp_mode);
	TEST_ASSERT_EQUAL_INT(0, cfg.model_value);
	TEST_ASSERT_EQUAL_INT(0, cfg.round);
	memset(&cfg, 0, sizeof(cfg));

	/* wrong model_value tests */
	cmp_mode = CMP_MODE_MODEL_MULTI; /* model value checks only active on model mode */
	model_value = MAX_MODEL_VALUE + 1;
	cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL_INT(DATA_TYPE_UNKNOWN, cfg.data_type);

	model_value = -1U;
	cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL_INT(DATA_TYPE_UNKNOWN, cfg.data_type);

	/* this should work */
	model_value = MAX_MODEL_VALUE;
	cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL_INT(DATA_TYPE_IMAGETTE, cfg.data_type);
	TEST_ASSERT_EQUAL_INT(CMP_MODE_MODEL_MULTI, cfg.cmp_mode);
	TEST_ASSERT_EQUAL_INT(16, cfg.model_value);
	TEST_ASSERT_EQUAL_INT(0, cfg.round);

	/* no checks for model mode -> no model cmp_mode */
	cmp_mode = CMP_MODE_STUFF;
	model_value = MAX_MODEL_VALUE + 1;
	cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL_INT(DATA_TYPE_IMAGETTE, cfg.data_type);
	TEST_ASSERT_EQUAL_INT(CMP_MODE_STUFF, cfg.cmp_mode);
	TEST_ASSERT_EQUAL_INT(MAX_MODEL_VALUE + 1, cfg.model_value);
	TEST_ASSERT_EQUAL_INT(0, cfg.round);
	model_value = MAX_MODEL_VALUE;

	/* wrong lossy_par tests */
	lossy_par = MAX_ICU_ROUND + 1;
	cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL_INT(DATA_TYPE_UNKNOWN, cfg.data_type);

	lossy_par = -1U;
	cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL_INT(DATA_TYPE_UNKNOWN, cfg.data_type);

	/* this should work */
	lossy_par = MAX_ICU_ROUND;
	cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL_INT(DATA_TYPE_IMAGETTE, cfg.data_type);
	TEST_ASSERT_EQUAL_INT(CMP_MODE_STUFF, cfg.cmp_mode);
	TEST_ASSERT_EQUAL_INT(16, cfg.model_value);
	TEST_ASSERT_EQUAL_INT(3, cfg.round);

	/* random test */
	data_type = cmp_rand_between(DATA_TYPE_IMAGETTE, biggest_data_type);
	cmp_mode = cmp_rand_between(CMP_MODE_RAW, CMP_MODE_STUFF);
	model_value = cmp_rand_between(0, MAX_MODEL_VALUE);
	lossy_par = cmp_rand_between(CMP_LOSSLESS, MAX_ICU_ROUND);
	cfg = cmp_cfg_icu_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL_INT(data_type, cfg.data_type);
	TEST_ASSERT_EQUAL_INT(cmp_mode, cfg.cmp_mode);
	TEST_ASSERT_EQUAL_INT(model_value, cfg.model_value);
	TEST_ASSERT_EQUAL_INT(lossy_par, cfg.round);
}


/**
 * @test cmp_cfg_icu_buffers
 */

void test_cmp_cfg_icu_buffers(void)
{
	struct cmp_cfg cfg;
	void *data_to_compress;
	uint32_t data_samples;
	void *model_of_data;
	void *updated_model;
	uint32_t *compressed_data;
	uint32_t compressed_data_len_samples;
	size_t s;
	uint16_t ima_data[4] = {42, 23, 0, 0xFFFF};
	uint16_t ima_model[4] = {0xC, 0xA, 0xFF, 0xE};
	uint16_t ima_up_model[4] = {0};
	uint32_t cmp_data[2] = {0};

	/* error case: unknown  data_type */
	cfg = cmp_cfg_icu_create(DATA_TYPE_UNKNOWN, CMP_MODE_DIFF_ZERO, 16, CMP_LOSSLESS);
	data_to_compress = ima_data;
	data_samples = 4;
	model_of_data = NULL;
	updated_model = NULL;
	compressed_data = cmp_data;
	compressed_data_len_samples = 4;
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(0, s);

	/* error case: no data test */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_DIFF_ZERO, 16, CMP_LOSSLESS);
	data_to_compress = NULL; /* no data set */
	data_samples = 4;
	model_of_data = NULL;
	updated_model = NULL;
	compressed_data = cmp_data;
	compressed_data_len_samples = 4;
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(0, s);

	/* now its should work */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_DIFF_ZERO, 16, CMP_LOSSLESS);
	data_to_compress = ima_data;
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(8, s);
	TEST_ASSERT_EQUAL(ima_data, cfg.input_buf);
	TEST_ASSERT_EQUAL_INT(NULL, cfg.model_buf);
	TEST_ASSERT_EQUAL_INT(4, cfg.samples);
	TEST_ASSERT_EQUAL(NULL, cfg.icu_new_model_buf);
	TEST_ASSERT_EQUAL(cmp_data, cfg.icu_output_buf);
	TEST_ASSERT_EQUAL_INT(4, cfg.buffer_length);

	/* error case: model mode and no model */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_ZERO, 16, CMP_LOSSLESS);
	model_of_data = NULL;
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(0, s);

	/* now its should work */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_ZERO, 16, CMP_LOSSLESS);
	model_of_data = ima_model;
	updated_model = ima_model;
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(8, s);
	TEST_ASSERT_EQUAL(ima_data, cfg.input_buf);
	TEST_ASSERT_EQUAL_INT(ima_model, cfg.model_buf);
	TEST_ASSERT_EQUAL_INT(4, cfg.samples);
	TEST_ASSERT_EQUAL(ima_model, cfg.icu_new_model_buf);
	TEST_ASSERT_EQUAL(cmp_data, cfg.icu_output_buf);
	TEST_ASSERT_EQUAL_INT(4, cfg.buffer_length);

	/* error case: data == model */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_ZERO, 16, CMP_LOSSLESS);
	data_to_compress = ima_data;
	model_of_data = ima_data;
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(0, s);

	/* error case: data == compressed_data */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_ZERO, 16, CMP_LOSSLESS);
	data_to_compress = ima_data;
	model_of_data = ima_model;
	compressed_data = (void *)ima_data;
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(0, s);

	/* error case: data == updated_model */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_ZERO, 16, CMP_LOSSLESS);
	data_to_compress = ima_data;
	model_of_data = ima_model;
	updated_model = ima_data;
	compressed_data = (void *)ima_data;
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(0, s);

	/* error case: model == compressed_data */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_ZERO, 16, CMP_LOSSLESS);
	data_to_compress = ima_data;
	model_of_data = ima_model;
	compressed_data = (void *)ima_model;
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(0, s);

	/* error case: updated_model == compressed_data */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_ZERO, 16, CMP_LOSSLESS);
	data_to_compress = ima_data;
	model_of_data = ima_model;
	updated_model = ima_up_model;
	compressed_data = (void *)ima_up_model;
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(0, s);

	/* warning case: samples = 0 */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_ZERO, 16, CMP_LOSSLESS);
	data_to_compress = ima_data;
	data_samples = 0;
	model_of_data = ima_model;
	updated_model = ima_up_model;
	compressed_data = cmp_data;
	compressed_data_len_samples = 4;
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(8, s);
	TEST_ASSERT_EQUAL(ima_data, cfg.input_buf);
	TEST_ASSERT_EQUAL_INT(ima_model, cfg.model_buf);
	TEST_ASSERT_EQUAL_INT(0, cfg.samples);
	TEST_ASSERT_EQUAL(ima_up_model, cfg.icu_new_model_buf);
	TEST_ASSERT_EQUAL(cmp_data, cfg.icu_output_buf);
	TEST_ASSERT_EQUAL_INT(4, cfg.buffer_length);
	memset(&cfg, 0, sizeof(cfg));

	/* error case: compressed_data_len_samples = 0 */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_ZERO, 16, CMP_LOSSLESS);
	data_samples = 4;
	compressed_data_len_samples = 0;
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(0, s);

	/* this should now work */
	/* if data_samples = 0 -> compressed_data_len_samples = 0 is allowed */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_ZERO, 16, CMP_LOSSLESS);
	data_samples = 0;
	compressed_data_len_samples = 0;
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(0, s); /* not an error, it is the size of the compressed data */
	TEST_ASSERT_EQUAL(ima_data, cfg.input_buf);
	TEST_ASSERT_EQUAL_INT(ima_model, cfg.model_buf);
	TEST_ASSERT_EQUAL_INT(0, cfg.samples);
	TEST_ASSERT_EQUAL(ima_up_model, cfg.icu_new_model_buf);
	TEST_ASSERT_EQUAL(cmp_data, cfg.icu_output_buf);
	TEST_ASSERT_EQUAL_INT(0, cfg.buffer_length);

	/* this should now work */
	/* if compressed_data = NULL -> compressed_data_len_samples = 0 is allowed */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_ZERO, 16, CMP_LOSSLESS);
	data_samples = 4;
	compressed_data = NULL;
	compressed_data_len_samples = 0;
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(0, s); /* not an error, it is the size of the compressed data */
	TEST_ASSERT_EQUAL(ima_data, cfg.input_buf);
	TEST_ASSERT_EQUAL_INT(ima_model, cfg.model_buf);
	TEST_ASSERT_EQUAL_INT(4, cfg.samples);
	TEST_ASSERT_EQUAL(ima_up_model, cfg.icu_new_model_buf);
	TEST_ASSERT_EQUAL(NULL, cfg.icu_output_buf);
	TEST_ASSERT_EQUAL_INT(0, cfg.buffer_length);

	/* error case: RAW mode compressed_data smaller than data_samples */
	compressed_data = cmp_data;
	compressed_data_len_samples = 3;
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_RAW, 16, CMP_LOSSLESS);
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(0, s);

	/* this should now work */
	compressed_data = NULL;
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_RAW, 16, CMP_LOSSLESS);
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(6, s);
	TEST_ASSERT_EQUAL(ima_data, cfg.input_buf);
	TEST_ASSERT_EQUAL_INT(ima_model, cfg.model_buf);
	TEST_ASSERT_EQUAL_INT(4, cfg.samples);
	TEST_ASSERT_EQUAL(ima_up_model, cfg.icu_new_model_buf);
	TEST_ASSERT_EQUAL(NULL, cfg.icu_output_buf);
	TEST_ASSERT_EQUAL_INT(3, cfg.buffer_length);

	/* this should also now work */
	compressed_data = cmp_data;
	compressed_data_len_samples = 4;
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_RAW, 16, CMP_LOSSLESS);
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(8, s);
	TEST_ASSERT_EQUAL(ima_data, cfg.input_buf);
	TEST_ASSERT_EQUAL_INT(ima_model, cfg.model_buf);
	TEST_ASSERT_EQUAL_INT(4, cfg.samples);
	TEST_ASSERT_EQUAL(ima_up_model, cfg.icu_new_model_buf);
	TEST_ASSERT_EQUAL(cmp_data, cfg.icu_output_buf);
	TEST_ASSERT_EQUAL_INT(4, cfg.buffer_length);

	/* error case: compressed data buffer bigger than max compression entity
	 * data size */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_DIFF_ZERO, 16, CMP_LOSSLESS);
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				0x7FFFED+1);
	TEST_ASSERT_EQUAL_size_t(0, s);

	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_DIFF_ZERO, 16, CMP_LOSSLESS);
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				0x7FFFFFFF);
	TEST_ASSERT_EQUAL_size_t(0, s);

	/* this should also now work */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_DIFF_ZERO, 16, CMP_LOSSLESS);
	s = cmp_cfg_icu_buffers(&cfg, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				0x7FFFED);
	TEST_ASSERT_EQUAL_size_t(0xFFFFDA, s);

	/* error case: cfg = NULL */
	s = cmp_cfg_icu_buffers(NULL, data_to_compress, data_samples,
				model_of_data, updated_model, compressed_data,
				compressed_data_len_samples);
	TEST_ASSERT_EQUAL_size_t(0, s);
}


/**
 * @test cmp_cfg_icu_imagette
 */

void test_cmp_cfg_icu_imagette(void)
{
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par;
	uint32_t spillover_par;
	enum cmp_data_type data_type;

	int error;

	/* lowest values 1d/model mode */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_ZERO, 0, CMP_LOSSLESS);
	cmp_par = MIN_IMA_GOLOMB_PAR;
	spillover_par = MIN_IMA_SPILL;
	error = cmp_cfg_icu_imagette(&cfg, cmp_par, spillover_par);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cfg.golomb_par, 1);
	TEST_ASSERT_EQUAL_INT(cfg.spill, 2);

	/* highest values 1d/model mode */
	cfg = cmp_cfg_icu_create(DATA_TYPE_F_CAM_IMAGETTE, CMP_MODE_DIFF_MULTI, 16, CMP_LOSSLESS);
	cmp_par = MAX_IMA_GOLOMB_PAR;
	spillover_par = cmp_ima_max_spill(cmp_par);
	error = cmp_cfg_icu_imagette(&cfg, cmp_par, spillover_par);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cfg.golomb_par, MAX_IMA_GOLOMB_PAR);
	TEST_ASSERT_EQUAL_INT(cfg.spill, cmp_ima_max_spill(MAX_IMA_GOLOMB_PAR));

	/* wrong data type  test */
	for (data_type = 0; data_type <= DATA_TYPE_F_CAM_BACKGROUND; data_type++) {
		cfg = cmp_cfg_icu_create(data_type, CMP_MODE_DIFF_MULTI, 16, CMP_LOSSLESS);
		error = cmp_cfg_icu_imagette(&cfg, cmp_par, spillover_par);
		if (data_type == DATA_TYPE_IMAGETTE ||
		    data_type == DATA_TYPE_SAT_IMAGETTE ||
		    data_type == DATA_TYPE_F_CAM_IMAGETTE) {
			TEST_ASSERT_FALSE(error);
			TEST_ASSERT_EQUAL_INT(data_type, cfg.data_type);
			TEST_ASSERT_EQUAL_INT(cfg.golomb_par, MAX_IMA_GOLOMB_PAR);
			TEST_ASSERT_EQUAL_INT(cfg.spill, cmp_ima_max_spill(MAX_IMA_GOLOMB_PAR));
		} else {
			TEST_ASSERT_TRUE(error);
		}
	}

	/* model/1d MODE tests */

	/* cmp_par to big */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_MULTI, 16, CMP_LOSSLESS);
	cmp_par = MAX_IMA_GOLOMB_PAR + 1;
	spillover_par = MIN_IMA_SPILL;
	error = cmp_cfg_icu_imagette(&cfg, cmp_par, spillover_par);
	TEST_ASSERT_TRUE(error);
	/* ignore in RAW MODE */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_RAW, 16, CMP_LOSSLESS);
	error = cmp_cfg_icu_imagette(&cfg, cmp_par, spillover_par);
	TEST_ASSERT_FALSE(error);

	/* cmp_par to small */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_MULTI, 16, CMP_LOSSLESS);
	cmp_par = MIN_IMA_GOLOMB_PAR - 1;
	spillover_par = MIN_IMA_SPILL;
	error = cmp_cfg_icu_imagette(&cfg, cmp_par, spillover_par);
	TEST_ASSERT_TRUE(error);
	/* ignore in RAW MODE */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_RAW, 16, CMP_LOSSLESS);
	error = cmp_cfg_icu_imagette(&cfg, cmp_par, spillover_par);
	TEST_ASSERT_FALSE(error);

	/* spillover_par to big */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_MULTI, 16, CMP_LOSSLESS);
	cmp_par = MIN_IMA_GOLOMB_PAR;
	spillover_par = cmp_ima_max_spill(cmp_par)+1;
	error = cmp_cfg_icu_imagette(&cfg, cmp_par, spillover_par);
	TEST_ASSERT_TRUE(error);
	/* ignore in RAW MODE */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_RAW, 16, CMP_LOSSLESS);
	error = cmp_cfg_icu_imagette(&cfg, cmp_par, spillover_par);
	TEST_ASSERT_FALSE(error);

	/* spillover_par to small */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_DIFF_ZERO, 0, CMP_LOSSLESS);
	cmp_par = MAX_IMA_GOLOMB_PAR;
	spillover_par = MIN_IMA_SPILL-1;
	error = cmp_cfg_icu_imagette(&cfg, cmp_par, spillover_par);
	TEST_ASSERT_TRUE(error);
	/* ignore in RAW MODE */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_RAW, 16, CMP_LOSSLESS);
	error = cmp_cfg_icu_imagette(&cfg, cmp_par, spillover_par);
	TEST_ASSERT_FALSE(error);

	/* CMP_MODE_STUFF tests */
	spillover_par = ~0U; /* is ignored */

	/* highest values STUFF MODE */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_STUFF, ~0U, CMP_LOSSLESS);
	cmp_par = MAX_STUFF_CMP_PAR;
	error = cmp_cfg_icu_imagette(&cfg, cmp_par, spillover_par);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cfg.golomb_par, 32);

	/* lowest values STUFF MODE */
	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_STUFF, ~0U, CMP_LOSSLESS);
	cmp_par = 0;
	error = cmp_cfg_icu_imagette(&cfg, cmp_par, spillover_par);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cfg.golomb_par, 0);

	/* cmp_par to big */
	cfg = cmp_cfg_icu_create(DATA_TYPE_SAT_IMAGETTE, CMP_MODE_STUFF, ~0U, CMP_LOSSLESS);
	cmp_par = MAX_STUFF_CMP_PAR+1;
	error = cmp_cfg_icu_imagette(&cfg, cmp_par, spillover_par);
	TEST_ASSERT_TRUE(error);

	/* cfg = NULL test*/
	error = cmp_cfg_icu_imagette(NULL, cmp_par, spillover_par);
	TEST_ASSERT_TRUE(error);

	/* invalid compression mode  test*/
	cfg = cmp_cfg_icu_create(DATA_TYPE_SAT_IMAGETTE, CMP_MODE_STUFF+1, ~0U, CMP_LOSSLESS);
	cmp_par = MAX_STUFF_CMP_PAR+1;
	error = cmp_cfg_icu_imagette(&cfg, cmp_par, spillover_par);
	TEST_ASSERT_TRUE(error);
}


/**
 * @test cmp_cfg_fx_cob
 */

void test_cmp_cfg_fx_cob(void)
{
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_exp_flags = 2;
	uint32_t spillover_exp_flags = 2;
	uint32_t cmp_par_fx = 2;
	uint32_t spillover_fx = 2;
	uint32_t cmp_par_ncob = 2;
	uint32_t spillover_ncob = 2;
	uint32_t cmp_par_efx = 2;
	uint32_t spillover_efx = 2;
	uint32_t cmp_par_ecob = 2;
	uint32_t spillover_ecob = 2;
	uint32_t cmp_par_fx_cob_variance = 2;
	uint32_t spillover_fx_cob_variance = 2;
	int error;
	enum cmp_data_type data_type;


	/* wrong data type test */
	for (data_type = 0; data_type <= DATA_TYPE_F_CAM_BACKGROUND; data_type++) {
		cfg = cmp_cfg_icu_create(data_type, CMP_MODE_MODEL_ZERO, 16, CMP_LOSSLESS);
		error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
				       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
				       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
				       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
		if (data_type == DATA_TYPE_S_FX ||
		    data_type == DATA_TYPE_S_FX_EFX ||
		    data_type == DATA_TYPE_S_FX_NCOB ||
		    data_type == DATA_TYPE_S_FX_EFX_NCOB_ECOB ||
		    data_type == DATA_TYPE_L_FX ||
		    data_type == DATA_TYPE_L_FX_EFX ||
		    data_type == DATA_TYPE_L_FX_NCOB ||
		    data_type == DATA_TYPE_L_FX_EFX_NCOB_ECOB ||
		    data_type == DATA_TYPE_F_FX ||
		    data_type == DATA_TYPE_F_FX_EFX ||
		    data_type == DATA_TYPE_F_FX_NCOB ||
		    data_type == DATA_TYPE_F_FX_EFX_NCOB_ECOB) {
			TEST_ASSERT_FALSE(error);
			TEST_ASSERT_EQUAL_INT(data_type, cfg.data_type);
			TEST_ASSERT_EQUAL_INT(2, cfg.cmp_par_fx);
			TEST_ASSERT_EQUAL_INT(2, cfg.spill_fx);
			TEST_ASSERT_EQUAL_INT(2, cfg.cmp_par_exp_flags);
			TEST_ASSERT_EQUAL_INT(2, cfg.spill_exp_flags);
			TEST_ASSERT_EQUAL_INT(2, cfg.cmp_par_efx);
			TEST_ASSERT_EQUAL_INT(2, cfg.spill_efx);
			TEST_ASSERT_EQUAL_INT(2, cfg.cmp_par_ncob);
			TEST_ASSERT_EQUAL_INT(2, cfg.spill_ncob);
			TEST_ASSERT_EQUAL_INT(2, cfg.cmp_par_ecob);
			TEST_ASSERT_EQUAL_INT(2, cfg.spill_ecob);
			TEST_ASSERT_EQUAL_INT(2, cfg.cmp_par_fx_cob_variance);
			TEST_ASSERT_EQUAL_INT(2, cfg.spill_fx_cob_variance);
		} else {
			TEST_ASSERT_TRUE(error);
		}
	}

	/* cfg == NULL test */
	error = cmp_cfg_fx_cob(NULL, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_TRUE(error);

	/* test DATA_TYPE_S_FX */
	cfg = cmp_cfg_icu_create(DATA_TYPE_S_FX, CMP_MODE_MODEL_ZERO, 16, CMP_LOSSLESS);
	cmp_par_exp_flags = MAX_NON_IMA_GOLOMB_PAR;
	spillover_exp_flags = cmp_icu_max_spill(cmp_par_exp_flags);
	cmp_par_fx = MIN_NON_IMA_GOLOMB_PAR;
	spillover_fx = MIN_NON_IMA_SPILL;
	cmp_par_ncob = ~0U; /* invalid parameter */
	spillover_ncob = ~0U; /* invalid parameter */
	cmp_par_efx = ~0U; /* invalid parameter */
	spillover_efx = ~0U; /* invalid parameter */
	cmp_par_ecob = ~0U; /* invalid parameter */
	spillover_ecob = ~0U; /* invalid parameter */
	cmp_par_fx_cob_variance = ~0U; /* invalid parameter */
	spillover_fx_cob_variance = ~0U; /* invalid parameter */

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx, cfg.cmp_par_fx);
	TEST_ASSERT_EQUAL_INT(spillover_fx, cfg.spill_fx);
	TEST_ASSERT_EQUAL_INT(cmp_par_exp_flags, cfg.cmp_par_exp_flags);
	TEST_ASSERT_EQUAL_INT(spillover_exp_flags, cfg.spill_exp_flags);

	/* invalid spillover_exp_flags parameter */
	cmp_par_exp_flags = MAX_NON_IMA_GOLOMB_PAR;
	spillover_exp_flags = cmp_icu_max_spill(cmp_par_exp_flags)+1;
	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_TRUE(error);

	/* invalid cmp_par_fx parameter */
	cmp_par_exp_flags = MAX_NON_IMA_GOLOMB_PAR;
	spillover_exp_flags = cmp_icu_max_spill(cmp_par_exp_flags);
	cmp_par_fx = MIN_NON_IMA_GOLOMB_PAR - 1;
	spillover_fx = MIN_NON_IMA_SPILL;
	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_TRUE(error);


	/* test DATA_TYPE_S_FX_EFX */
	cfg = cmp_cfg_icu_create(DATA_TYPE_S_FX_EFX, CMP_MODE_MODEL_ZERO, 0, 1);
	cmp_par_exp_flags = MAX_NON_IMA_GOLOMB_PAR;
	spillover_exp_flags = cmp_icu_max_spill(cmp_par_exp_flags);
	cmp_par_fx = MIN_NON_IMA_GOLOMB_PAR;
	spillover_fx = MIN_NON_IMA_SPILL;
	cmp_par_ncob = ~0U; /* invalid parameter */
	spillover_ncob = ~0U; /* invalid parameter */
	cmp_par_efx = 23;
	spillover_efx = 42;
	cmp_par_ecob = ~0U; /* invalid parameter */
	spillover_ecob = ~0U; /* invalid parameter */
	cmp_par_fx_cob_variance = ~0U; /* invalid parameter */
	spillover_fx_cob_variance = ~0U; /* invalid parameter */
	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx, cfg.cmp_par_fx);
	TEST_ASSERT_EQUAL_INT(spillover_fx, cfg.spill_fx);
	TEST_ASSERT_EQUAL_INT(cmp_par_exp_flags, cfg.cmp_par_exp_flags);
	TEST_ASSERT_EQUAL_INT(spillover_exp_flags, cfg.spill_exp_flags);
	TEST_ASSERT_EQUAL_INT(cmp_par_efx, cfg.cmp_par_efx);
	TEST_ASSERT_EQUAL_INT(spillover_efx, cfg.spill_efx);

	/* invalid spillover_efx parameter */
	spillover_efx = 0;
	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_TRUE(error);


	/* test DATA_TYPE_S_FX_NCOB */
	cfg = cmp_cfg_icu_create(DATA_TYPE_S_FX_NCOB, CMP_MODE_MODEL_ZERO, 0, 1);
	cmp_par_exp_flags = MAX_NON_IMA_GOLOMB_PAR;
	spillover_exp_flags = cmp_icu_max_spill(cmp_par_exp_flags);
	cmp_par_fx = MIN_NON_IMA_GOLOMB_PAR;
	spillover_fx = MIN_NON_IMA_SPILL;
	cmp_par_ncob = 19;
	spillover_ncob = 5;
	cmp_par_efx = ~0U; /* invalid parameter */
	spillover_efx = ~0U; /* invalid parameter */
	cmp_par_ecob = ~0U; /* invalid parameter */
	spillover_ecob = ~0U; /* invalid parameter */
	cmp_par_fx_cob_variance = ~0U; /* invalid parameter */
	spillover_fx_cob_variance = ~0U; /* invalid parameter */
	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx, cfg.cmp_par_fx);
	TEST_ASSERT_EQUAL_INT(spillover_fx, cfg.spill_fx);
	TEST_ASSERT_EQUAL_INT(cmp_par_exp_flags, cfg.cmp_par_exp_flags);
	TEST_ASSERT_EQUAL_INT(spillover_exp_flags, cfg.spill_exp_flags);
	TEST_ASSERT_EQUAL_INT(cmp_par_ncob, cfg.cmp_par_ncob);
	TEST_ASSERT_EQUAL_INT(spillover_ncob, cfg.spill_ncob);

	/* invalid cmp_par_ncob parameter */
	cmp_par_ncob = 0;
	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_TRUE(error);


	/* test DATA_TYPE_S_FX_EFX_NCOB_ECOB */
	cfg = cmp_cfg_icu_create(DATA_TYPE_S_FX_EFX_NCOB_ECOB, CMP_MODE_DIFF_ZERO, 7, CMP_LOSSLESS);
	cmp_par_exp_flags = MAX_NON_IMA_GOLOMB_PAR;
	spillover_exp_flags = cmp_icu_max_spill(cmp_par_exp_flags);
	cmp_par_fx = MIN_NON_IMA_GOLOMB_PAR;
	spillover_fx = MIN_NON_IMA_SPILL;
	cmp_par_ncob = 19;
	spillover_ncob = 5;
	cmp_par_efx = 23;
	spillover_efx = 42;
	cmp_par_ecob = MAX_NON_IMA_GOLOMB_PAR;
	spillover_ecob = MIN_NON_IMA_SPILL;
	cmp_par_fx_cob_variance = ~0U; /* invalid parameter */
	spillover_fx_cob_variance = ~0U; /* invalid parameter */
	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx, cfg.cmp_par_fx);
	TEST_ASSERT_EQUAL_INT(spillover_fx, cfg.spill_fx);
	TEST_ASSERT_EQUAL_INT(cmp_par_exp_flags, cfg.cmp_par_exp_flags);
	TEST_ASSERT_EQUAL_INT(spillover_exp_flags, cfg.spill_exp_flags);
	TEST_ASSERT_EQUAL_INT(cmp_par_ncob, cfg.cmp_par_ncob);
	TEST_ASSERT_EQUAL_INT(spillover_ncob, cfg.spill_ncob);
	TEST_ASSERT_EQUAL_INT(cmp_par_efx, cfg.cmp_par_efx);
	TEST_ASSERT_EQUAL_INT(spillover_efx, cfg.spill_efx);
	TEST_ASSERT_EQUAL_INT(cmp_par_ecob, cfg.cmp_par_ecob);
	TEST_ASSERT_EQUAL_INT(spillover_ecob, cfg.spill_ecob);

	/* invalid cmp_par_ecob parameter */
	cmp_par_ecob = -1U;
	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_TRUE(error);


	/* DATA_TYPE_L_FX */
	cfg = cmp_cfg_icu_create(DATA_TYPE_L_FX, CMP_MODE_DIFF_ZERO, 7, CMP_LOSSLESS);
	cmp_par_exp_flags = MAX_NON_IMA_GOLOMB_PAR;
	spillover_exp_flags = cmp_icu_max_spill(cmp_par_exp_flags);
	cmp_par_fx = MIN_NON_IMA_GOLOMB_PAR;
	spillover_fx = MIN_NON_IMA_SPILL;
	cmp_par_ncob = ~0U; /* invalid parameter */
	spillover_ncob = ~0U; /* invalid parameter */
	cmp_par_efx = ~0U; /* invalid parameter */
	spillover_efx = ~0U; /* invalid parameter */
	cmp_par_ecob = ~0U; /* invalid parameter */
	spillover_ecob = ~0U; /* invalid parameter */
	cmp_par_fx_cob_variance = 30;
	spillover_fx_cob_variance = 8;

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx, cfg.cmp_par_fx);
	TEST_ASSERT_EQUAL_INT(spillover_fx, cfg.spill_fx);
	TEST_ASSERT_EQUAL_INT(cmp_par_exp_flags, cfg.cmp_par_exp_flags);
	TEST_ASSERT_EQUAL_INT(spillover_exp_flags, cfg.spill_exp_flags);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx_cob_variance, cfg.cmp_par_fx_cob_variance);
	TEST_ASSERT_EQUAL_INT(spillover_fx_cob_variance, cfg.spill_fx_cob_variance);

	/* invalid spillover_fx_cob_variance parameter */
	spillover_fx_cob_variance = 1;
	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_TRUE(error);


	/* DATA_TYPE_L_FX_EFX */
	cfg = cmp_cfg_icu_create(DATA_TYPE_L_FX_EFX, CMP_MODE_DIFF_ZERO, 7, CMP_LOSSLESS);
	cmp_par_exp_flags = MAX_NON_IMA_GOLOMB_PAR;
	spillover_exp_flags = cmp_icu_max_spill(cmp_par_exp_flags);
	cmp_par_fx = MIN_NON_IMA_GOLOMB_PAR;
	spillover_fx = MIN_NON_IMA_SPILL;
	cmp_par_ncob = ~0U; /* invalid parameter */
	spillover_ncob = ~0U; /* invalid parameter */
	cmp_par_efx = 23;
	spillover_efx = 42;
	cmp_par_ecob = ~0U; /* invalid parameter */
	spillover_ecob = ~0U; /* invalid parameter */
	cmp_par_fx_cob_variance = 30;
	spillover_fx_cob_variance = 8;

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx, cfg.cmp_par_fx);
	TEST_ASSERT_EQUAL_INT(spillover_fx, cfg.spill_fx);
	TEST_ASSERT_EQUAL_INT(cmp_par_exp_flags, cfg.cmp_par_exp_flags);
	TEST_ASSERT_EQUAL_INT(spillover_exp_flags, cfg.spill_exp_flags);
	TEST_ASSERT_EQUAL_INT(cmp_par_efx, cfg.cmp_par_efx);
	TEST_ASSERT_EQUAL_INT(spillover_efx, cfg.spill_efx);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx_cob_variance, cfg.cmp_par_fx_cob_variance);
	TEST_ASSERT_EQUAL_INT(spillover_fx_cob_variance, cfg.spill_fx_cob_variance);


	/* DATA_TYPE_L_FX_NCOB */
	cfg = cmp_cfg_icu_create(DATA_TYPE_L_FX_NCOB, CMP_MODE_DIFF_ZERO, 7, CMP_LOSSLESS);
	cmp_par_exp_flags = MAX_NON_IMA_GOLOMB_PAR;
	spillover_exp_flags = cmp_icu_max_spill(cmp_par_exp_flags);
	cmp_par_fx = MIN_NON_IMA_GOLOMB_PAR;
	spillover_fx = MIN_NON_IMA_SPILL;
	cmp_par_ncob = 19;
	spillover_ncob = 5;
	cmp_par_efx = ~0U; /* invalid parameter */
	spillover_efx = ~0U; /* invalid parameter */
	cmp_par_ecob = ~0U; /* invalid parameter */
	spillover_ecob = ~0U; /* invalid parameter */
	cmp_par_fx_cob_variance = 30;
	spillover_fx_cob_variance = 8;

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx, cfg.cmp_par_fx);
	TEST_ASSERT_EQUAL_INT(spillover_fx, cfg.spill_fx);
	TEST_ASSERT_EQUAL_INT(cmp_par_exp_flags, cfg.cmp_par_exp_flags);
	TEST_ASSERT_EQUAL_INT(spillover_exp_flags, cfg.spill_exp_flags);
	TEST_ASSERT_EQUAL_INT(cmp_par_ncob, cfg.cmp_par_ncob);
	TEST_ASSERT_EQUAL_INT(spillover_ncob, cfg.spill_ncob);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx_cob_variance, cfg.cmp_par_fx_cob_variance);
	TEST_ASSERT_EQUAL_INT(spillover_fx_cob_variance, cfg.spill_fx_cob_variance);


	/* DATA_TYPE_L_FX_EFX_NCOB_ECOB */
	cfg = cmp_cfg_icu_create(DATA_TYPE_L_FX_EFX_NCOB_ECOB, CMP_MODE_DIFF_ZERO, 7, CMP_LOSSLESS);
	cmp_par_exp_flags = MAX_NON_IMA_GOLOMB_PAR;
	spillover_exp_flags = cmp_icu_max_spill(cmp_par_exp_flags);
	cmp_par_fx = MIN_NON_IMA_GOLOMB_PAR;
	spillover_fx = MIN_NON_IMA_SPILL;
	cmp_par_ncob = 19;
	spillover_ncob = 5;
	cmp_par_efx = 23;
	spillover_efx = 42;
	cmp_par_ecob = MAX_NON_IMA_GOLOMB_PAR;
	spillover_ecob = MIN_NON_IMA_SPILL;
	cmp_par_fx_cob_variance = 30;
	spillover_fx_cob_variance = 8;

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx, cfg.cmp_par_fx);
	TEST_ASSERT_EQUAL_INT(spillover_fx, cfg.spill_fx);
	TEST_ASSERT_EQUAL_INT(cmp_par_exp_flags, cfg.cmp_par_exp_flags);
	TEST_ASSERT_EQUAL_INT(spillover_exp_flags, cfg.spill_exp_flags);
	TEST_ASSERT_EQUAL_INT(cmp_par_efx, cfg.cmp_par_efx);
	TEST_ASSERT_EQUAL_INT(spillover_efx, cfg.spill_efx);
	TEST_ASSERT_EQUAL_INT(cmp_par_ncob, cfg.cmp_par_ncob);
	TEST_ASSERT_EQUAL_INT(spillover_ncob, cfg.spill_ncob);
	TEST_ASSERT_EQUAL_INT(cmp_par_ecob, cfg.cmp_par_ecob);
	TEST_ASSERT_EQUAL_INT(spillover_ecob, cfg.spill_ecob);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx_cob_variance, cfg.cmp_par_fx_cob_variance);
	TEST_ASSERT_EQUAL_INT(spillover_fx_cob_variance, cfg.spill_fx_cob_variance);


	/* DATA_TYPE_F_FX */
	cfg = cmp_cfg_icu_create(DATA_TYPE_F_FX, CMP_MODE_DIFF_ZERO, 7, CMP_LOSSLESS);
	cmp_par_exp_flags = ~0U; /* invalid parameter */
	spillover_exp_flags = ~0U; /* invalid parameter */
	cmp_par_fx = MIN_NON_IMA_GOLOMB_PAR;
	spillover_fx = MIN_NON_IMA_SPILL;
	cmp_par_ncob = ~0U; /* invalid parameter */
	spillover_ncob = ~0U; /* invalid parameter */
	cmp_par_efx = ~0U; /* invalid parameter */
	spillover_efx = ~0U; /* invalid parameter */
	cmp_par_ecob = ~0U; /* invalid parameter */
	spillover_ecob = ~0U; /* invalid parameter */
	cmp_par_fx_cob_variance = ~0U; /* invalid parameter */
	spillover_fx_cob_variance = ~0U; /* invalid parameter */

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx, cfg.cmp_par_fx);
	TEST_ASSERT_EQUAL_INT(spillover_fx, cfg.spill_fx);


	/* DATA_TYPE_F_FX_EFX */
	cfg = cmp_cfg_icu_create(DATA_TYPE_F_FX_EFX, CMP_MODE_DIFF_ZERO, 7, CMP_LOSSLESS);
	cmp_par_exp_flags = ~0U; /* invalid parameter */
	spillover_exp_flags = ~0U; /* invalid parameter */
	cmp_par_fx = MIN_NON_IMA_GOLOMB_PAR;
	spillover_fx = MIN_NON_IMA_SPILL;
	cmp_par_ncob = ~0U; /* invalid parameter */
	spillover_ncob = ~0U; /* invalid parameter */
	cmp_par_efx = 23;
	spillover_efx = 42;
	cmp_par_ecob = ~0U; /* invalid parameter */
	spillover_ecob = ~0U; /* invalid parameter */
	cmp_par_fx_cob_variance = ~0U; /* invalid parameter */
	spillover_fx_cob_variance = ~0U; /* invalid parameter */

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx, cfg.cmp_par_fx);
	TEST_ASSERT_EQUAL_INT(spillover_fx, cfg.spill_fx);
	TEST_ASSERT_EQUAL_INT(cmp_par_efx, cfg.cmp_par_efx);
	TEST_ASSERT_EQUAL_INT(spillover_efx, cfg.spill_efx);


	/* DATA_TYPE_F_FX_NCOB */
	cfg = cmp_cfg_icu_create(DATA_TYPE_F_FX_NCOB, CMP_MODE_DIFF_ZERO, 7, CMP_LOSSLESS);
	cmp_par_exp_flags = ~0U; /* invalid parameter */
	spillover_exp_flags = ~0U; /* invalid parameter */
	cmp_par_fx = MIN_NON_IMA_GOLOMB_PAR;
	spillover_fx = MIN_NON_IMA_SPILL;
	cmp_par_ncob = MIN_NON_IMA_GOLOMB_PAR;
	spillover_ncob = cmp_icu_max_spill(cmp_par_ncob);
	cmp_par_efx = ~0U; /* invalid parameter */
	spillover_efx = ~0U; /* invalid parameter */
	cmp_par_ecob = ~0U; /* invalid parameter */
	spillover_ecob = ~0U; /* invalid parameter */
	cmp_par_fx_cob_variance = ~0U; /* invalid parameter */
	spillover_fx_cob_variance = ~0U; /* invalid parameter */

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx, cfg.cmp_par_fx);
	TEST_ASSERT_EQUAL_INT(spillover_fx, cfg.spill_fx);
	TEST_ASSERT_EQUAL_INT(cmp_par_ncob, cfg.cmp_par_ncob);
	TEST_ASSERT_EQUAL_INT(spillover_ncob, cfg.spill_ncob);


	/* DATA_TYPE_F_FX_EFX_NCOB_ECOB */
	cfg = cmp_cfg_icu_create(DATA_TYPE_F_FX_EFX_NCOB_ECOB, CMP_MODE_DIFF_ZERO, 7, CMP_LOSSLESS);
	cmp_par_exp_flags = ~0U; /* invalid parameter */
	spillover_exp_flags = ~0U; /* invalid parameter */
	cmp_par_fx = MIN_NON_IMA_GOLOMB_PAR;
	spillover_fx = MIN_NON_IMA_SPILL;
	cmp_par_ncob = MIN_NON_IMA_GOLOMB_PAR;
	spillover_ncob = cmp_icu_max_spill(cmp_par_ncob);
	cmp_par_efx = 23;
	spillover_efx = 42;
	cmp_par_ecob = MAX_NON_IMA_GOLOMB_PAR;
	spillover_ecob = MIN_NON_IMA_SPILL;
	cmp_par_fx_cob_variance = ~0U; /* invalid parameter */
	spillover_fx_cob_variance = ~0U; /* invalid parameter */

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(cmp_par_fx, cfg.cmp_par_fx);
	TEST_ASSERT_EQUAL_INT(spillover_fx, cfg.spill_fx);
	TEST_ASSERT_EQUAL_INT(cmp_par_ncob, cfg.cmp_par_ncob);
	TEST_ASSERT_EQUAL_INT(spillover_ncob, cfg.spill_ncob);
	TEST_ASSERT_EQUAL_INT(cmp_par_efx, cfg.cmp_par_efx);
	TEST_ASSERT_EQUAL_INT(spillover_efx, cfg.spill_efx);
	TEST_ASSERT_EQUAL_INT(cmp_par_ecob, cfg.cmp_par_ecob);
	TEST_ASSERT_EQUAL_INT(spillover_ecob, cfg.spill_ecob);
}

/**
 * @test cmp_cfg_aux
 */

void test_cmp_cfg_aux(void)
{	struct cmp_cfg cfg;
	uint32_t cmp_par_mean = 2;
	uint32_t spillover_mean = 2;
	uint32_t cmp_par_variance = 2;
	uint32_t spillover_variance = 2;
	uint32_t cmp_par_pixels_error = 2;
	uint32_t spillover_pixels_error = 2;
	int error;
	enum cmp_data_type data_type;

	/* wrong data type test */
	for (data_type = 0; data_type <= DATA_TYPE_F_CAM_BACKGROUND; data_type++) {
		cfg = cmp_cfg_icu_create(data_type, CMP_MODE_MODEL_ZERO, 16, CMP_LOSSLESS);
		error = cmp_cfg_aux(&cfg, cmp_par_mean, spillover_mean,
				    cmp_par_variance, spillover_variance,
				    cmp_par_pixels_error, spillover_pixels_error);
		if (data_type == DATA_TYPE_OFFSET ||
		    data_type == DATA_TYPE_BACKGROUND ||
		    data_type == DATA_TYPE_SMEARING
		    /* data_type == DATA_TYPE_F_CAM_OFFSET || */
		    /* data_type == DATA_TYPE_F_CAM_BACKGROUND */
		    ) {
			TEST_ASSERT_FALSE(error);
			TEST_ASSERT_EQUAL_INT(data_type, cfg.data_type);
			TEST_ASSERT_EQUAL_INT(2, cfg.cmp_par_mean);
			TEST_ASSERT_EQUAL_INT(2, cfg.spill_mean);
			TEST_ASSERT_EQUAL_INT(2, cfg.cmp_par_variance);
			TEST_ASSERT_EQUAL_INT(2, cfg.spill_variance);
			TEST_ASSERT_EQUAL_INT(2, cfg.cmp_par_pixels_error);
			TEST_ASSERT_EQUAL_INT(2, cfg.spill_pixels_error);
		} else {
			TEST_ASSERT_TRUE(error);
		}
	}

	/* cfg == NULL test */
	error = cmp_cfg_aux(NULL, cmp_par_mean, spillover_mean,
			    cmp_par_variance, spillover_variance,
			    cmp_par_pixels_error, spillover_pixels_error);
	TEST_ASSERT_TRUE(error);


	/* DATA_TYPE_OFFSET */
	cfg = cmp_cfg_icu_create(DATA_TYPE_OFFSET, CMP_MODE_DIFF_ZERO, 7, CMP_LOSSLESS);
	cmp_par_mean = MIN_NON_IMA_GOLOMB_PAR;
	spillover_mean = cmp_icu_max_spill(MIN_NON_IMA_GOLOMB_PAR);
	cmp_par_variance = MIN_NON_IMA_GOLOMB_PAR;
	spillover_variance = MIN_NON_IMA_SPILL;
	cmp_par_pixels_error = ~0U;
	spillover_pixels_error = ~0U;

	error = cmp_cfg_aux(&cfg, cmp_par_mean, spillover_mean,
			    cmp_par_variance, spillover_variance,
			    cmp_par_pixels_error, spillover_pixels_error);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(MIN_NON_IMA_GOLOMB_PAR, cfg.cmp_par_mean);
	TEST_ASSERT_EQUAL_INT(cmp_icu_max_spill(MIN_NON_IMA_GOLOMB_PAR), cfg.spill_mean);
	TEST_ASSERT_EQUAL_INT(MIN_NON_IMA_GOLOMB_PAR, cfg.cmp_par_variance);
	TEST_ASSERT_EQUAL_INT(2, cfg.spill_variance);

	/* This should fail */
	cmp_par_mean = MIN_NON_IMA_GOLOMB_PAR-1;
	error = cmp_cfg_aux(&cfg, cmp_par_mean, spillover_mean,
			    cmp_par_variance, spillover_variance,
			    cmp_par_pixels_error, spillover_pixels_error);
	TEST_ASSERT_TRUE(error);


	/* DATA_TYPE_BACKGROUND */
	cfg = cmp_cfg_icu_create(DATA_TYPE_BACKGROUND, CMP_MODE_DIFF_ZERO, 7, CMP_LOSSLESS);
	cmp_par_mean = MAX_NON_IMA_GOLOMB_PAR;
	spillover_mean = cmp_icu_max_spill(MAX_NON_IMA_GOLOMB_PAR);
	cmp_par_variance = MIN_NON_IMA_GOLOMB_PAR;
	spillover_variance = MIN_NON_IMA_SPILL;
	cmp_par_pixels_error = 42;
	spillover_pixels_error = 23;

	error = cmp_cfg_aux(&cfg, cmp_par_mean, spillover_mean,
			    cmp_par_variance, spillover_variance,
			    cmp_par_pixels_error, spillover_pixels_error);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(MAX_NON_IMA_GOLOMB_PAR, cfg.cmp_par_mean);
	TEST_ASSERT_EQUAL_INT(cmp_icu_max_spill(MAX_NON_IMA_GOLOMB_PAR), cfg.spill_mean);
	TEST_ASSERT_EQUAL_INT(MIN_NON_IMA_GOLOMB_PAR, cfg.cmp_par_variance);
	TEST_ASSERT_EQUAL_INT(MIN_NON_IMA_SPILL, cfg.spill_variance);
	TEST_ASSERT_EQUAL_INT(42, cfg.cmp_par_pixels_error);
	TEST_ASSERT_EQUAL_INT(23, cfg.spill_pixels_error);

	/* This should fail */
	cmp_par_variance = MIN_NON_IMA_GOLOMB_PAR-1;
	error = cmp_cfg_aux(&cfg, cmp_par_mean, spillover_mean,
			    cmp_par_variance, spillover_variance,
			    cmp_par_pixels_error, spillover_pixels_error);
	TEST_ASSERT_TRUE(error);


	/* DATA_TYPE_SMEARING */
	cfg = cmp_cfg_icu_create(DATA_TYPE_SMEARING, CMP_MODE_DIFF_ZERO, 7, CMP_LOSSLESS);
	cmp_par_mean = MAX_NON_IMA_GOLOMB_PAR;
	spillover_mean = cmp_icu_max_spill(MAX_NON_IMA_GOLOMB_PAR);
	cmp_par_variance = MIN_NON_IMA_GOLOMB_PAR;
	spillover_variance = MIN_NON_IMA_SPILL;
	cmp_par_pixels_error = 42;
	spillover_pixels_error = 23;

	error = cmp_cfg_aux(&cfg, cmp_par_mean, spillover_mean,
			    cmp_par_variance, spillover_variance,
			    cmp_par_pixels_error, spillover_pixels_error);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL_INT(MAX_NON_IMA_GOLOMB_PAR, cfg.cmp_par_mean);
	TEST_ASSERT_EQUAL_INT(cmp_icu_max_spill(MAX_NON_IMA_GOLOMB_PAR), cfg.spill_mean);
	TEST_ASSERT_EQUAL_INT(MIN_NON_IMA_GOLOMB_PAR, cfg.cmp_par_variance);
	TEST_ASSERT_EQUAL_INT(MIN_NON_IMA_SPILL, cfg.spill_variance);
	TEST_ASSERT_EQUAL_INT(42, cfg.cmp_par_pixels_error);
	TEST_ASSERT_EQUAL_INT(23, cfg.spill_pixels_error);

	/* This should fail */
	spillover_pixels_error = cmp_icu_max_spill(42)+1;
	error = cmp_cfg_aux(&cfg, cmp_par_mean, spillover_mean,
			    cmp_par_variance, spillover_variance,
			    cmp_par_pixels_error, spillover_pixels_error);
	TEST_ASSERT_TRUE(error);

#if 0
TODO: implemented F_CAM DATA_TYPE_F_CAM_OFFSET and DATA_TYPE_F_CAM_BACKGROUND
	/* DATA_TYPE_F_CAM_OFFSET */
	cfg = cmp_cfg_icu_create(DATA_TYPE_F_CAM_OFFSET, CMP_MODE_DIFF_ZERO, 7, CMP_LOSSLESS);
	/* DATA_TYPE_F_CAM_BACKGROUND */
	cfg = cmp_cfg_icu_create(DATA_TYPE_F_CAM_BACKGROUND, CMP_MODE_DIFF_ZERO, 7, CMP_LOSSLESS);
#endif
}


/**
 * @test map_to_pos
 */

void test_map_to_pos(void)
{
	uint32_t value_to_map;
	uint32_t max_data_bits;
	uint32_t mapped_value;

	/* test mapping 32 bits values */
	max_data_bits = 32;

	value_to_map = 0;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(0, mapped_value);

	value_to_map = -1U;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(1, mapped_value);

	value_to_map = 1;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(2, mapped_value);

	value_to_map = 42;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(84, mapped_value);

	value_to_map = INT32_MAX;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_HEX(UINT32_MAX-1, mapped_value);

	value_to_map = (uint32_t)INT32_MIN;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_HEX(UINT32_MAX, mapped_value);

	/* test mapping 16 bits values */
	max_data_bits = 16;

	value_to_map = -1U;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(1, mapped_value);

	/* test mapping 6 bits values */
	max_data_bits = 6;

	value_to_map = 0;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(0, mapped_value);

	value_to_map = -1U;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(1, mapped_value);

	value_to_map = UINT32_MAX;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(1, mapped_value);

	value_to_map = -1U & 0x3FU;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(1, mapped_value);

	value_to_map = 63;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(1, mapped_value);

	value_to_map = 1;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(2, mapped_value);

	value_to_map = 31;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(62, mapped_value);

	value_to_map = -33U; /* aka 31 */
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(62, mapped_value);

	value_to_map = -32U;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(63, mapped_value);

	value_to_map = 32;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(63, mapped_value);
}


/**
 * @test put_n_bits32
 */

#define SDP_PB_N 3


static void init_PB32_arrays(uint32_t *z, uint32_t *o)
{
	uint32_t i;

	/* init testarray with all 0 and all 1 */
	for (i = 0; i < SDP_PB_N; i++) {
		z[i] = 0;
		o[i] = 0xffffffff;
	}
}


void test_put_n_bits32(void)
{
	uint32_t v, n;
	int o, rval; /* return value */
	uint32_t testarray0[SDP_PB_N];
	uint32_t testarray1[SDP_PB_N];
	const uint32_t l = sizeof(testarray0) * CHAR_BIT;

	/* hereafter, the value is v,
	 * the number of bits to write is n,
	 * the offset of the bit is o,
	 * the max length the bitstream in bits is l
	 */

	init_PB32_arrays(testarray0, testarray1);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);
	TEST_ASSERT(testarray0[2] == 0);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);
	TEST_ASSERT(testarray1[2] == 0xffffffff);

	/*** n=0 ***/

	/* do not write, left border */
	v = 0; n = 0; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(0, rval);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(0, rval);
	TEST_ASSERT(testarray1[0] == 0xffffffff);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(0, rval);

	v = 0xffffffff; n = 0; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(0, rval);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(0, rval);
	TEST_ASSERT(testarray1[0] == 0xffffffff);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(0, rval);

	/* do not write, right border */
	v = 0; n = 0; o = l;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(l, rval);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(l, rval);
	TEST_ASSERT(testarray1[0] == 0xffffffff);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(l, rval);

	/* test value = 0xffffffff; N = 0 */
	v = 0xffffffff; n = 0; o = l;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(l, rval);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(l, rval);
	TEST_ASSERT(testarray1[0] == 0xffffffff);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(l, rval);

	/*** n=1 ***/

	/* left border, write 0 */
	v = 0; n = 1; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(1, rval);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(1, rval);
	TEST_ASSERT(testarray1[0] == 0x7fffffff);

	/* left border, write 1 */
	v = 1; n = 1; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(1, rval);
	TEST_ASSERT(testarray0[0] == 0x80000000);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(1, rval);
	TEST_ASSERT(testarray1[0] == 0xffffffff);

	/* left border, write 32 */
	v = 0xf0f0abcd; n = 32; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 32);
	TEST_ASSERT(testarray0[0] == 0xf0f0abcd);
	TEST_ASSERT(testarray0[1] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 32);
	TEST_ASSERT(testarray1[0] == 0xf0f0abcd);
	TEST_ASSERT(testarray1[1] == 0xffffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* middle, write 2 bits */
	v = 3; n = 2; o = 29;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 31);
	TEST_ASSERT(testarray0[0] == 0x6);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT_EQUAL_INT(rval, 31);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/*** n=5, unsegmented ***/

	/* left border, write 0 */
	v = 0; n = 5; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 5);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT(testarray1[0] == 0x07ffffff);
	TEST_ASSERT_EQUAL_INT(rval, 5);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* left border, write 11111 */
	v = 0x1f; n = 5; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 5);
	TEST_ASSERT(testarray0[0] == 0xf8000000);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 5);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* middle, write 0 */
	v = 0; n = 5; o = 7;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 12);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 12);
	TEST_ASSERT(testarray1[0] == 0xfe0fffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* middle, write 11111 */
	v = 0x1f; n = 5; o = 7;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 12);
	TEST_ASSERT(testarray0[0] == 0x01f00000);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 12);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* right, write 0 */
	v = 0; n = 5; o = 91;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 96);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 96);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);
	TEST_ASSERT(testarray1[2] == 0xffffffe0);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* right, write 11111 */
	v = 0x1f; n = 5; o = 91;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 96);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);
	TEST_ASSERT(testarray0[2] == 0x0000001f);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 96);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);
	TEST_ASSERT(testarray1[2] == 0xffffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* 32 bit, write 0 */
	v = 0; n = 32; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 32);
	TEST_ASSERT(testarray0[0] == 0x00000000);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 32);
	TEST_ASSERT(testarray1[0] == 0x00000000);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* 32 bit, write -1 */
	v = 0xffffffff; n = 32; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 32);
	TEST_ASSERT(testarray0[0] == 0xffffffff);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 32);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* SEGMENTED cases */
	/* 5 bit, write 0 */
	v = 0; n = 5; o = 62;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 67);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);
	TEST_ASSERT(testarray0[2] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 67);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xfffffffc);
	TEST_ASSERT(testarray1[2] == 0x1fffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* 5 bit, write 1f */
	v = 0x1f; n = 5; o = 62;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 67);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 3);
	TEST_ASSERT(testarray0[2] == 0xe0000000);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 67);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);
	TEST_ASSERT(testarray1[2] == 0xffffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* 32 bit, write 0 */
	v = 0; n = 32; o = 1;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 33);
	TEST_ASSERT(testarray0[0] == 0x00000000);
	TEST_ASSERT(testarray0[1] == 0x00000000);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 33);
	TEST_ASSERT(testarray1[0] == 0x80000000);
	TEST_ASSERT(testarray1[1] == 0x7fffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* 32 bit, write -1 */
	v = 0xffffffff; n = 32; o = 1;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 33);
	TEST_ASSERT(testarray0[0] == 0x7fffffff);
	TEST_ASSERT(testarray0[1] == 0x80000000);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 33);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* test NULL buffer */
	v = 0; n = 0; o = 0;
	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(rval, 0);

	v = 0; n = 1; o = 0;
	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(rval, 1);

	v = 0; n = 5; o = 31;
	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(rval, 36);

	v = 0; n = 2; o = 95;
	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(rval, 97); /* rval can be longer than l */

	/* value larger than n allows */
	v = 0x7f; n = 6; o = 10;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(16, rval);
	TEST_ASSERT(testarray0[0] == 0x003f0000);
	TEST_ASSERT(testarray0[1] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(16, rval);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(16, rval);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	v = 0xffffffff; n = 6; o = 10;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(16, rval);
	TEST_ASSERT(testarray0[0] == 0x003f0000);
	TEST_ASSERT(testarray0[1] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(16, rval);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(16, rval);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/*** error cases ***/
	/* n too large */
	v = 0x0; n = 33; o = 1;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, -1);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(rval, -1);

	/* try to put too much in the bitstream */
	v = 0x1; n = 1; o = 96;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, rval);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);
	TEST_ASSERT(testarray0[2] == 0);

	/* this should work (if bitstream=NULL no length check) */
	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(97, rval);

	/* offset lager than max_stream_len(l) */
	v = 0x0; n = 32; o = INT32_MAX;
	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, rval);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);
	TEST_ASSERT(testarray1[2] == 0xffffffff);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT(rval < 0);

	/* negative offset */
	v = 0x0; n = 0; o = -1;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(-1, rval);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(-1, rval);

	v = 0x0; n = 0; o = -2;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(-1, rval);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(-1, rval);
}


/**
 * @test rice_encoder
 */

void test_rice_encoder(void)
{
	uint32_t value, g_par, log2_g_par, cw, cw_len;
	const uint32_t MAX_GOLOMB_PAR = 0x80000000;

	/* test minimum Golomb parameter */
	value = 0; log2_g_par = ilog_2(MIN_NON_IMA_GOLOMB_PAR); g_par = 1U << log2_g_par; cw = ~0U;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(1, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 31;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFE, cw);

	/* test some arbitrary values */
	value = 0; log2_g_par = 4; g_par = 1U << log2_g_par; cw = ~0U;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(5, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 1;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(5, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x1, cw);

	value = 42;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(7, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x6a, cw);

	value = 446;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFEE, cw);

	value = 447;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFEF, cw);

	/* test maximum Golomb parameter for rice_encoder */
	value = 0; log2_g_par = ilog_2(MAX_GOLOMB_PAR); g_par = 1U << log2_g_par; cw = ~0U;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 1;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x1, cw);

	value = 0x7FFFFFFE;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x7FFFFFFE, cw);

	value = 0x7FFFFFFF;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x7FFFFFFF, cw);
}


/**
 * @test golomb_encoder
 */

void test_golomb_encoder(void)
{
	uint32_t value, g_par, log2_g_par, cw, cw_len;
	const uint32_t MAX_GOLOMB_PAR = 0x80000000;

	/* test minimum Golomb parameter */
	value = 0; g_par = MIN_NON_IMA_GOLOMB_PAR; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(1, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 31;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFE, cw);

	/* error case: value larger than allowed */
	value = 32; g_par = 1; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_GREATER_THAN_UINT(32, cw_len);

	/* error case: value larger than allowed */
	value = 33; g_par = 1; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_GREATER_THAN_UINT(32, cw_len);

#if 0
	/* error case: value larger than allowed overflow in returned len */
	value = UINT32_MAX; g_par = 1; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_GREATER_THAN_UINT(32, cw_len);
#endif

	/* test some arbitrary values with g_par = 16 */
	value = 0; g_par = 16; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(5, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 1;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(5, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x1, cw);

	value = 42;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(7, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x6a, cw);

	value = 446;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFEE, cw);

	value = 447;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFEF, cw);


	/* test some arbitrary values with g_par = 3 */
	value = 0; g_par = 3; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(2, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 1;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(3, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x2, cw);

	value = 42;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(16, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFC, cw);

	value = 44;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(17, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x1FFFB, cw);

	value = 88;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFA, cw);

	value = 89;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFB, cw);

	/* test some arbitrary values with g_par = 0x7FFFFFFF */
	value = 0; g_par = 0x7FFFFFFF; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(31, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 1;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x2, cw);

	value = 0x7FFFFFFE;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x7FFFFFFF, cw);

	value = 0x7FFFFFFF;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x80000000, cw);

	/* test maximum Golomb parameter for golomb_encoder */
	value = 0; g_par = MAX_GOLOMB_PAR; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 1; g_par = MAX_GOLOMB_PAR; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x1, cw);

	value = 0x7FFFFFFE;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x7FFFFFFE, cw);

	value = 0x7FFFFFFF;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x7FFFFFFF, cw);

	value = 0; g_par = 0xFFFFFFFF; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);
}


/**
 * @test encode_value_zero
 */

void test_encode_value_zero(void)
{
	uint32_t data, model;
	int stream_len;
	struct encoder_setupt setup = {0};
	uint32_t bitstream[3] = {0};

	/* setup the setup */
	setup.encoder_par1 = 1;
	setup.encoder_par2 = ilog_2(setup.encoder_par1);
	setup.spillover_par = 32;
	setup.max_data_bits = 32;
	setup.generate_cw_f = rice_encoder;
	setup.bitstream_adr = bitstream;
	setup.max_stream_len = sizeof(bitstream) * CHAR_BIT;

	stream_len = 0;

	data = 0; model = 0;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(2, stream_len);
	TEST_ASSERT_EQUAL_HEX(0x80000000, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[2]);

	data = 5; model = 0;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(14, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xBFF80000, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[2]);

	data = 2; model = 7;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(25, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xBFFBFF00, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[2]);

	/* zero escape mechanism */
	data = 100; model = 42;
	/* (100-42)*2+1=117 -> cw 0 + 0x0000_0000_0000_0075 */
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(58, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xBFFBFF00, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0x00001D40, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[2]);

	/* test overflow */
	data = (uint32_t)INT32_MIN; model = 0;
	/* (INT32_MIN)*-2-1+1=0(overflow) -> cw 0 + 0x0000_0000_0000_0000 */
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(91, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xBFFBFF00, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0x00001D40, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[2]);

	/* small buffer error */
	data = 23; model = 26;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, stream_len);

	/* reset bitstream to all bits set */
	bitstream[0] = ~0U;
	bitstream[1] = ~0U;
	bitstream[2] = ~0U;
	stream_len = 0;

	/* we use now values with maximum 6 bits */
	setup.max_data_bits = 6;

	/* lowest value before zero encoding */
	data = 53; model = 38;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(32, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFE, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, bitstream[2]);

	/* lowest value with zero encoding */
	data = 0; model = 16;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(39, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFE, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0x41FFFFFF, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, bitstream[2]);

	/* maximum positive value to encode */
	data = 31; model = 0;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(46, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFE, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0x40FFFFFF, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, bitstream[2]);

	/* maximum negative value to encode */
	data = 0; model = 32;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(53, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFE, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0x40FC07FF, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, bitstream[2]);

	/* small buffer error when creating the zero escape symbol*/
	bitstream[0] = 0;
	bitstream[1] = 0;
	bitstream[2] = 0;
	stream_len = 32;
	setup.max_stream_len = 32;
	data = 31; model = 0;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, stream_len);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[2]);
}


/**
 * @test encode_value_multi
 */

void test_encode_value_multi(void)
{
	uint32_t data, model;
	int stream_len;
	struct encoder_setupt setup = {0};
	uint32_t bitstream[4] = {0};

	/* setup the setup */
	setup.encoder_par1 = 1;
	setup.encoder_par2 = ilog_2(setup.encoder_par1);
	setup.spillover_par = 16;
	setup.max_data_bits = 32;
	setup.generate_cw_f = golomb_encoder;
	setup.bitstream_adr = bitstream;
	setup.max_stream_len = sizeof(bitstream) * CHAR_BIT;

	stream_len = 0;

	data = 0; model = 0;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(1, stream_len);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[3]);

	data = 0; model = 1;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(3, stream_len);
	TEST_ASSERT_EQUAL_HEX(0x40000000, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[3]);

	data = 1+23; model = 0+23;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(6, stream_len);
	TEST_ASSERT_EQUAL_HEX(0x58000000, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[3]);

	/* highest value without multi outlier encoding */
	data = 0+42; model = 8+42;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(22, stream_len);
	TEST_ASSERT_EQUAL_HEX(0x5BFFF800, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[3]);

	/* lowest value with multi outlier encoding */
	data = 8+42; model = 0+42;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(41, stream_len);
	TEST_ASSERT_EQUAL_HEX(0x5BFFFBFF, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0xFC000000, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[3]);

	/* highest value with multi outlier encoding */
	data = (uint32_t)INT32_MIN; model = 0;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(105, stream_len);
	TEST_ASSERT_EQUAL_HEX(0x5BFFFBFF, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0xFC7FFFFF, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0xFF7FFFFF, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0xF7800000, bitstream[3]);

	/* small buffer error */
	data = 0; model = 38;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, stream_len);

	/* small buffer error when creating the multi escape symbol*/
	bitstream[0] = 0;
	bitstream[1] = 0;
	setup.max_stream_len = 32;

	stream_len = 32;
	data = 31; model = 0;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, stream_len);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[1]);
}


/**
 * @test encode_value
 */

void test_encode_value(void)
{
	struct encoder_setupt setup = {0};
	uint32_t bitstream[4] = {0};
	uint32_t data, model;
	int cmp_size;

	setup.encode_method_f = encode_value_none;
	setup.bitstream_adr = bitstream;
	setup.max_stream_len = 128;
	cmp_size = 0;

	/* test 32 bit input */
	setup.encoder_par1 = 32;
	setup.max_data_bits = 32;
	setup.lossy_par = 0;

	data = 0; model = 0;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_INT(32, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[3]);

	data = UINT32_MAX; model = 0;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_INT(64, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[3]);

	/* test rounding */
	setup.lossy_par = 1;
	data = UINT32_MAX; model = 0;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_INT(96, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0x7FFFFFFF, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[3]);

	setup.lossy_par = 2;
	data = 0x3; model = 0;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_INT(128, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0x7FFFFFFF, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[3]);

	/* small buffer error bitstream can not hold more data*/
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, cmp_size);

	/* reset bitstream */
	bitstream[0] = 0;
	bitstream[1] = 0;
	bitstream[2] = 0;
	bitstream[3] = 0;
	cmp_size = 0;

	/* test 31 bit input */
	setup.encoder_par1 = 31;
	setup.max_data_bits = 31;
	setup.lossy_par = 0;

	data = 0; model = 0;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_INT(31, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[3]);

	data = 0x7FFFFFFF; model = 0;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_INT(62, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0x00000001, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFC, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[3]);

	/* round = 1 */
	setup.lossy_par = 1;
	data = UINT32_MAX; model = UINT32_MAX;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_INT(93, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0x00000001, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFF8, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[3]);

	/* data are bigger than max_data_bits */
	setup.lossy_par = 0;
	data = UINT32_MAX; model = 0;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_size);

	/* model are bigger than max_data_bits */
	setup.lossy_par = 0;
	cmp_size = 93;
	data = 0; model = UINT32_MAX;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_size);
}


/**
 * @test configure_encoder_setup
 */

void test_configure_encoder_setup(void)
{
	struct encoder_setupt setup;
	uint32_t cmp_par;
	uint32_t spillover;
	uint32_t lossy_par;
	uint32_t max_data_bits;
	struct cmp_cfg cfg;
	int error;

	/* test Golomb encoder zero escape mechanism */
	cmp_par = 42;
	spillover = 23;
	lossy_par = 0;
	max_data_bits = 15;
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_MODEL_ZERO;
	cfg.icu_output_buf = (void *)123;
	cfg.buffer_length = 2;
	error = configure_encoder_setup(&setup, cmp_par, spillover, lossy_par,
					max_data_bits, &cfg);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL(golomb_encoder, setup.generate_cw_f); /* pointer to the code word encoder */
	TEST_ASSERT_EQUAL(encode_value_zero, setup.encode_method_f); /* pointer to the encoding function */
	TEST_ASSERT_EQUAL(123, setup.bitstream_adr); /* start address of the compressed data bitstream */
	TEST_ASSERT_EQUAL_INT(32, setup.max_stream_len); /* maximum length of the bitstream/icu_output_buf in bits */
	TEST_ASSERT_EQUAL_INT(42, setup.encoder_par1); /* encoding parameter 1 */
	TEST_ASSERT_EQUAL_INT(5, setup.encoder_par2); /* encoding parameter 2 */
	TEST_ASSERT_EQUAL_INT(23, setup.spillover_par); /* outlier parameter */
	TEST_ASSERT_EQUAL_INT(0, setup.lossy_par); /* lossy compression parameter */
	TEST_ASSERT_EQUAL_INT(15, setup.max_data_bits); /* how many bits are needed to represent the highest possible value */
	memset(&setup, 0, sizeof(setup));

	/* test Rice encoder multi escape mechanism */
	cmp_par = 32;
	spillover = 23;
	lossy_par = 0;
	max_data_bits = 32;
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_DIFF_MULTI;
	cfg.icu_output_buf = (void *)123;
	cfg.buffer_length = 2;
	error = configure_encoder_setup(&setup, cmp_par, spillover, lossy_par,
					max_data_bits, &cfg);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL(rice_encoder, setup.generate_cw_f); /* pointer to the code word encoder */
	TEST_ASSERT_EQUAL(encode_value_multi, setup.encode_method_f); /* pointer to the encoding function */
	TEST_ASSERT_EQUAL(123, setup.bitstream_adr); /* start address of the compressed data bitstream */
	TEST_ASSERT_EQUAL_INT(32, setup.max_stream_len); /* maximum length of the bitstream/icu_output_buf in bits */
	TEST_ASSERT_EQUAL_INT(32, setup.encoder_par1); /* encoding parameter 1 */
	TEST_ASSERT_EQUAL_INT(5, setup.encoder_par2); /* encoding parameter 2 */
	TEST_ASSERT_EQUAL_INT(23, setup.spillover_par); /* outlier parameter */
	TEST_ASSERT_EQUAL_INT(0, setup.lossy_par); /* lossy compression parameter */
	TEST_ASSERT_EQUAL_INT(32, setup.max_data_bits); /* how many bits are needed to represent the highest possible value */
	memset(&setup, 0, sizeof(setup));

	/* test CMP_MODE_STUFF */
	cmp_par = 32;
	spillover = ~0U;
	lossy_par = 1;
	max_data_bits = 32;
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_STUFF;
	cfg.icu_output_buf = (void *)123;
	cfg.buffer_length = 2;
	error = configure_encoder_setup(&setup, cmp_par, spillover, lossy_par,
					max_data_bits, &cfg);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL(NULL, setup.generate_cw_f); /* pointer to the code word encoder */
	TEST_ASSERT_EQUAL(encode_value_none, setup.encode_method_f); /* pointer to the encoding function */
	TEST_ASSERT_EQUAL(123, setup.bitstream_adr); /* start address of the compressed data bitstream */
	TEST_ASSERT_EQUAL_INT(32, setup.max_stream_len); /* maximum length of the bitstream/icu_output_buf in bits */
	TEST_ASSERT_EQUAL_INT(32, setup.encoder_par1); /* encoding parameter 1 */
	TEST_ASSERT_EQUAL_INT(0, setup.encoder_par2); /* encoding parameter 2 */
	TEST_ASSERT_EQUAL_INT(0, setup.spillover_par); /* outlier parameter */
	TEST_ASSERT_EQUAL_INT(1, setup.lossy_par); /* lossy compression parameter */
	TEST_ASSERT_EQUAL_INT(32, setup.max_data_bits); /* how many bits are needed to represent the highest possible value */
	memset(&setup, 0, sizeof(setup));

	/* test max_used_bits = 33 */
	cmp_par = 32;
	spillover = 23;
	lossy_par = 0;
	max_data_bits = 33;
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_DIFF_MULTI;
	cfg.icu_output_buf = (void *)123;
	cfg.buffer_length = 2;
	error = configure_encoder_setup(&setup, cmp_par, spillover, lossy_par,
					max_data_bits, &cfg);
	TEST_ASSERT_TRUE(error);
	memset(&setup, 0, sizeof(setup));

	/* cmp_par = 0 test */
	cmp_par = 0;
	spillover = 23;
	lossy_par = 0;
	max_data_bits = 32;
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_DIFF_MULTI;
	cfg.icu_output_buf = (void *)123;
	cfg.buffer_length = 2;
	error = configure_encoder_setup(&setup, cmp_par, spillover, lossy_par,
					max_data_bits, &cfg);
	TEST_ASSERT_TRUE(error);
	memset(&setup, 0, sizeof(setup));

	/* cmp_par = 0 test STUFF MODE this should work*/
	cmp_par = 0;
	spillover = 23;
	lossy_par = 0;
	max_data_bits = 32;
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_STUFF;
	cfg.icu_output_buf = (void *)123;
	cfg.buffer_length = 2;
	error = configure_encoder_setup(&setup, cmp_par, spillover, lossy_par,
					max_data_bits, &cfg);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL(NULL, setup.generate_cw_f); /* pointer to the code word encoder */
	TEST_ASSERT_EQUAL(encode_value_none, setup.encode_method_f); /* pointer to the encoding function */
	TEST_ASSERT_EQUAL(123, setup.bitstream_adr); /* start address of the compressed data bitstream */
	TEST_ASSERT_EQUAL_INT(32, setup.max_stream_len); /* maximum length of the bitstream/icu_output_buf in bits */
	TEST_ASSERT_EQUAL_INT(0, setup.encoder_par1); /* encoding parameter 1 */
	TEST_ASSERT_EQUAL_INT(0, setup.encoder_par2); /* encoding parameter 2 */
	TEST_ASSERT_EQUAL_INT(0, setup.spillover_par); /* outlier parameter */
	TEST_ASSERT_EQUAL_INT(0, setup.lossy_par); /* lossy compression parameter */
	TEST_ASSERT_EQUAL_INT(0, setup.max_data_bits); /* how many bits are needed to represent the highest possible value */
	memset(&setup, 0, sizeof(setup));

	/* cmp_mode = STUFF_MODE +1  */
	cmp_par = 32;
	spillover = 23;
	lossy_par = 0;
	max_data_bits = 1;
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_STUFF+1;
	cfg.icu_output_buf = (void *)123;
	cfg.buffer_length = 2;
	error = configure_encoder_setup(&setup, cmp_par, spillover, lossy_par,
					max_data_bits, &cfg);
	TEST_ASSERT_TRUE(error);
	memset(&setup, 0, sizeof(setup));

	/* setup = NULL test */
	cmp_par = 42;
	spillover = 23;
	lossy_par = 0;
	max_data_bits = 15;
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_MODEL_ZERO;
	cfg.icu_output_buf = (void *)123;
	cfg.buffer_length = 2;
	error = configure_encoder_setup(NULL, cmp_par, spillover, lossy_par,
					max_data_bits, &cfg);
	TEST_ASSERT_TRUE(error);
	memset(&setup, 0, sizeof(setup));

	/* error case: cmp_mode = CMP_MODE_RAW test */
	cmp_par = 42;
	spillover = 23;
	lossy_par = 0;
	max_data_bits = 15;
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_RAW;
	cfg.icu_output_buf = (void *)123;
	cfg.buffer_length = 2;
	error = configure_encoder_setup(&setup, cmp_par, spillover, lossy_par,
					max_data_bits, &cfg);
	TEST_ASSERT_TRUE(error);
	memset(&setup, 0, sizeof(setup));

	/* cfg = NULL test */
	cmp_par = 42;
	spillover = 23;
	lossy_par = 0;
	max_data_bits = 15;
	error = configure_encoder_setup(&setup, cmp_par, spillover, lossy_par,
					max_data_bits, NULL);
	TEST_ASSERT_TRUE(error);
	memset(&setup, 0, sizeof(setup));
}


/**
 * @test compress_imagette
 */

void test_compress_imagette_diff(void)
{
	uint16_t data[] = {0xFFFF, 1, 0, 42, 0x8000, 0x7FFF, 0xFFFF};
	uint32_t output_buf[3] = {0xFFFF, 0xFFFF, 0xFFFF};
	uint32_t output_buf_size;
	struct cmp_cfg cfg = {0};
	int error, cmp_size;

	uint32_t golomb_par = 1;
	uint32_t spill = 8;
	uint32_t samples = 7;

	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_DIFF_ZERO,
				 CMP_PAR_UNUSED, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);
	output_buf_size = cmp_cfg_icu_buffers(&cfg, data, samples, NULL, NULL,
					      (uint32_t *)output_buf, samples);
	TEST_ASSERT_EQUAL_INT(samples*sizeof(uint16_t), output_buf_size);

	error = cmp_cfg_icu_imagette(&cfg, golomb_par, spill);
	TEST_ASSERT_FALSE(error);

	cmp_size = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(66, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0xDF6002AB, be32_to_cpu(output_buf[0]));
	TEST_ASSERT_EQUAL_HEX(0xFEB70000, be32_to_cpu(output_buf[1]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(output_buf[2]));

	/* test: icu_output_buf = NULL */
	cfg.icu_output_buf = NULL;
	cmp_size = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(66, cmp_size);
}


/**
 * @test compress_imagette
 */

void test_compress_imagette_model(void)
{
	uint16_t data[]  = {0x0000, 0x0001, 0x0042, 0x8000, 0x7FFF, 0xFFFF, 0xFFFF};
	uint16_t model[] = {0x0000, 0xFFFF, 0xF301, 0x8FFF, 0x0000, 0xFFFF, 0x0000};
	uint16_t model_up[7] = {0};
	uint32_t output_buf[3] = {~0U, ~0U, ~0U};
	uint32_t output_buf_size;
	struct cmp_cfg cfg = {0};
	uint32_t model_value = 8;
	uint32_t samples = 7;
	uint32_t buffer_length = 8;
	uint32_t golomb_par = 3;
	uint32_t spill = 8;
	int cmp_size, error;

	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_MULTI,
				 model_value, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);
	output_buf_size = cmp_cfg_icu_buffers(&cfg, data, samples, model, model_up,
					      output_buf, buffer_length);
	TEST_ASSERT_EQUAL_INT(buffer_length*sizeof(uint16_t), output_buf_size);
	error = cmp_cfg_icu_imagette(&cfg, golomb_par, spill);
	TEST_ASSERT_FALSE(error);

	cmp_size = icu_compress_data(&cfg);

	TEST_ASSERT_EQUAL_INT(76, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0x2BDB4F5E, be32_to_cpu(output_buf[0]));
	TEST_ASSERT_EQUAL_HEX(0xDFF5F9FF, be32_to_cpu(output_buf[1]));
	TEST_ASSERT_EQUAL_HEX(0xEC200000, be32_to_cpu(output_buf[2]));

	TEST_ASSERT_EQUAL_HEX(0x0000, model_up[0]);
	TEST_ASSERT_EQUAL_HEX(0x8000, model_up[1]);
	TEST_ASSERT_EQUAL_HEX(0x79A1, model_up[2]);
	TEST_ASSERT_EQUAL_HEX(0x87FF, model_up[3]);
	TEST_ASSERT_EQUAL_HEX(0x3FFF, model_up[4]);
	TEST_ASSERT_EQUAL_HEX(0xFFFF, model_up[5]);
	TEST_ASSERT_EQUAL_HEX(0x7FFF, model_up[6]);


	/* error case: model mode without model data */
	cfg.model_buf = NULL; /* this is the error */
	cmp_size = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL(-1, cmp_size);
}


/**
 * @test compress_imagette
 */

void test_compress_imagette_stuff(void)
{
	uint16_t data[] = {0x0, 0x1, 0x23, 0x42, 0x8000, 0x7FFF, 0xFFFF};
	uint32_t output_buf[4] = {0};
	struct cmp_cfg cfg = {0};

	int cmp_size, error;
	uint8_t output_buf_exp[] = {
		0x00, 0x00, 0x00, 0x01,
		0x00, 0x23, 0x00, 0x42,
		0x80, 0x00, 0x7F, 0xFF,
		0xFF, 0xFF, 0x00, 0x00};
	uint32_t *output_buf_exp_32;

	uint32_t samples = 7;
	uint32_t buffer_length = 8;
	uint32_t cmp_par = 16; /* how many used bits has the maximum data value */
	uint32_t output_buf_size;


	cfg = cmp_cfg_icu_create(DATA_TYPE_IMAGETTE, CMP_MODE_STUFF,
				 CMP_PAR_UNUSED, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);
	output_buf_size = cmp_cfg_icu_buffers(&cfg, data, samples, NULL, NULL,
					      output_buf, buffer_length);
	TEST_ASSERT_EQUAL_INT(buffer_length*sizeof(uint16_t), output_buf_size);
	error = cmp_cfg_icu_imagette(&cfg, cmp_par, CMP_PAR_UNUSED);
	TEST_ASSERT_FALSE(error);

	cmp_size = icu_compress_data(&cfg);

	output_buf_exp_32 = (uint32_t *)output_buf_exp;
	TEST_ASSERT_EQUAL_INT(7*16, cmp_size);
	TEST_ASSERT_EQUAL_HEX16(output_buf_exp_32[0], output_buf[0]);
	TEST_ASSERT_EQUAL_HEX16(output_buf_exp_32[1], output_buf[1]);
	TEST_ASSERT_EQUAL_HEX16(output_buf_exp_32[2], output_buf[2]);
	TEST_ASSERT_EQUAL_HEX16(output_buf_exp_32[3], output_buf[3]);
}


/**
 * @test compress_imagette
 */

void test_compress_imagette_raw(void)
{
	uint16_t data[] = {0x0, 0x1, 0x23, 0x42, (uint16_t)INT16_MIN, INT16_MAX, UINT16_MAX};
	uint16_t output_buf[7] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	struct cmp_cfg cfg = {0};
	int cmp_size;

	cfg.cmp_mode = CMP_MODE_RAW;
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.model_buf = NULL;
	cfg.input_buf = data;
	cfg.samples = 7;
	cfg.icu_output_buf = (uint32_t *)output_buf;
	cfg.buffer_length = 7;

	cmp_size = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(7*16, cmp_size);
	TEST_ASSERT_EQUAL_HEX16(0x0, be16_to_cpu(output_buf[0]));
	TEST_ASSERT_EQUAL_HEX16(0x1, be16_to_cpu(output_buf[1]));
	TEST_ASSERT_EQUAL_HEX16(0x23, be16_to_cpu(output_buf[2]));
	TEST_ASSERT_EQUAL_HEX16(0x42, be16_to_cpu(output_buf[3]));
	TEST_ASSERT_EQUAL_HEX16(INT16_MIN, be16_to_cpu(output_buf[4]));
	TEST_ASSERT_EQUAL_HEX16(INT16_MAX, be16_to_cpu(output_buf[5]));
	TEST_ASSERT_EQUAL_HEX16(UINT16_MAX, be16_to_cpu(output_buf[6]));


	/* compressed data buf = NULL test */
	memset(&cfg, 0, sizeof(struct cmp_cfg));
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.input_buf = data;
	cfg.samples = 7;
	cfg.icu_output_buf = NULL;
	cfg.buffer_length = 7;
	cfg.max_used_bits = &MAX_USED_BITS_SAFE;

	cmp_size = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(7*16, cmp_size);


	/* error case: input_buf = NULL */
	memset(&cfg, 0, sizeof(struct cmp_cfg));
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.input_buf = NULL; /* no data to compress */
	cfg.samples = 7;
	cfg.icu_output_buf = (uint32_t *)output_buf;
	cfg.buffer_length = 7;
	cfg.max_used_bits = &MAX_USED_BITS_SAFE;

	cmp_size = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_size);


	/* error case: compressed data buffer to small */
	memset(&cfg, 0, sizeof(struct cmp_cfg));
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.input_buf = data;
	cfg.samples = 7;
	cfg.icu_output_buf = (uint32_t *)output_buf;
	cfg.buffer_length = 6; /* the buffer is to small */
	cfg.max_used_bits = &MAX_USED_BITS_SAFE;

	cmp_size = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, cmp_size);
}


/**
 * @test compress_imagette
 */

void test_compress_imagette_error_cases(void)
{
	uint16_t data[] = {0xFFFF, 1, 0, 42, 0x8000, 0x7FFF, 0xFFFF};
	uint32_t output_buf[2] = {0xFFFF, 0xFFFF};
	struct cmp_cfg cfg = {0};
	int cmp_size, error;
	struct cmp_max_used_bits my_max_used_bits;

	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_DIFF_ZERO;
	cfg.input_buf = NULL;
	cfg.samples = 0;  /* nothing to compress */
	cfg.golomb_par = 1;
	cfg.spill = 8;
	cfg.icu_output_buf = NULL;
	cfg.buffer_length = 0;
	cfg.max_used_bits = &MAX_USED_BITS_SAFE;

	cmp_size = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(0, cmp_size);


	/* compressed data buffer to small test */
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_DIFF_ZERO;
	cfg.input_buf = data;
	cfg.samples = 7;
	cfg.golomb_par = 1;
	cfg.spill = 8;
	cfg.icu_output_buf = (uint32_t *)output_buf;
	cfg.buffer_length = 4;
	cfg.max_used_bits = &MAX_USED_BITS_SAFE;

	cmp_size = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, cmp_size);

	/* compressed data buffer to small test part 2 */
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_DIFF_ZERO;
	cfg.input_buf = data;
	cfg.samples = 7;
	cfg.golomb_par = 1;
	cfg.spill = 8;
	cfg.icu_output_buf = (uint32_t *)output_buf;
	cfg.buffer_length = 1;
	cfg.max_used_bits = &MAX_USED_BITS_SAFE;

	cmp_size = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, cmp_size);

	/* error invalid data_type */
	cfg.data_type = DATA_TYPE_UNKNOWN;
	cfg.cmp_mode = CMP_MODE_DIFF_ZERO;
	cfg.input_buf = data;
	cfg.samples = 7;
	cfg.golomb_par = 1;
	cfg.spill = 8;
	cfg.icu_output_buf = (uint32_t *)output_buf;
	cfg.buffer_length = 4;
	cfg.max_used_bits = &MAX_USED_BITS_SAFE;
	cmp_size = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_size);

	cfg.data_type = DATA_TYPE_F_CAM_BACKGROUND+1;
	cmp_size = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_size);

	/* error in setup */
	my_max_used_bits = MAX_USED_BITS_SAFE;
	my_max_used_bits.nc_imagette = 33;
	error = cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	TEST_ASSERT_TRUE(error);
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_DIFF_ZERO;
	cfg.input_buf = data;
	cfg.samples = 2;
	cfg.golomb_par = 1;
	cfg.spill = 8;
	cfg.icu_output_buf = (uint32_t *)output_buf;
	cfg.buffer_length = 4;
	cfg.max_used_bits = &my_max_used_bits;

	cmp_size = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_size);

}


/**
 * @test compress_multi_entry_hdr
 */

void test_compress_multi_entry_hdr(void)
{
	int stream_len;
	uint8_t data[MULTI_ENTRY_HDR_SIZE];
	uint8_t model[MULTI_ENTRY_HDR_SIZE];
	uint8_t up_model[MULTI_ENTRY_HDR_SIZE];
	uint8_t cmp_data[MULTI_ENTRY_HDR_SIZE];
	uint8_t *data_p = NULL;
	uint8_t *model_p = NULL;
	uint8_t *up_model_p = NULL;

	memset(data, 0x42, sizeof(data));

	/* no data; no cmp_data no model test */
	/* no data; no model; no up_model; no cmp_data */
	stream_len = compress_multi_entry_hdr((void **)&data_p, (void **)&model_p,
					      (void **)&up_model_p, NULL);
	TEST_ASSERT_EQUAL_INT(96, stream_len);

	/* no model; no up_model */
	data_p = data;
	stream_len = compress_multi_entry_hdr((void **)&data_p, (void **)&model_p,
					      (void **)&up_model_p, cmp_data);
	TEST_ASSERT_EQUAL_INT(96, stream_len);
	TEST_ASSERT_FALSE(memcmp(cmp_data, data, MULTI_ENTRY_HDR_SIZE));
	TEST_ASSERT_EQUAL(data_p-data, MULTI_ENTRY_HDR_SIZE);

	/* no up_model */
	memset(cmp_data, 0, sizeof(cmp_data));
	data_p = data;
	model_p = model;
	up_model_p = NULL;
	stream_len = compress_multi_entry_hdr((void **)&data_p, (void **)&model_p,
					      (void **)&up_model_p, cmp_data);
	TEST_ASSERT_EQUAL_INT(96, stream_len);
	TEST_ASSERT_FALSE(memcmp(cmp_data, data, MULTI_ENTRY_HDR_SIZE));
	TEST_ASSERT_EQUAL(data_p-data, MULTI_ENTRY_HDR_SIZE);
	TEST_ASSERT_EQUAL(model_p-model, MULTI_ENTRY_HDR_SIZE);

	/* all buffer test */
	memset(cmp_data, 0, sizeof(cmp_data));
	data_p = data;
	model_p = model;
	up_model_p = up_model;
	stream_len = compress_multi_entry_hdr((void **)&data_p, (void **)&model_p,
					      (void **)&up_model_p, cmp_data);
	TEST_ASSERT_EQUAL_INT(96, stream_len);
	TEST_ASSERT_FALSE(memcmp(cmp_data, data, MULTI_ENTRY_HDR_SIZE));
	TEST_ASSERT_FALSE(memcmp(up_model, data, MULTI_ENTRY_HDR_SIZE));
	TEST_ASSERT_EQUAL(data_p-data, MULTI_ENTRY_HDR_SIZE);
	TEST_ASSERT_EQUAL(model_p-model, MULTI_ENTRY_HDR_SIZE);
	TEST_ASSERT_EQUAL(up_model_p-up_model, MULTI_ENTRY_HDR_SIZE);

	/* all buffer test; no cmp_data */
	memset(cmp_data, 0, sizeof(cmp_data));
	data_p = data;
	model_p = model;
	up_model_p = up_model;
	stream_len = compress_multi_entry_hdr((void **)&data_p, (void **)&model_p,
					      (void **)&up_model_p, NULL);
	TEST_ASSERT_EQUAL_INT(96, stream_len);
	TEST_ASSERT_FALSE(memcmp(up_model, data, MULTI_ENTRY_HDR_SIZE));
	TEST_ASSERT_EQUAL(data_p-data, MULTI_ENTRY_HDR_SIZE);
	TEST_ASSERT_EQUAL(model_p-model, MULTI_ENTRY_HDR_SIZE);
	TEST_ASSERT_EQUAL(up_model_p-up_model, MULTI_ENTRY_HDR_SIZE);

	/* no data, use up_model test */
	memset(cmp_data, 0, sizeof(cmp_data));
	data_p = NULL;
	model_p = model;
	up_model_p = up_model;
	stream_len = compress_multi_entry_hdr((void **)&data_p, (void **)&model_p,
					      (void **)&up_model_p, NULL);
	TEST_ASSERT_EQUAL_INT(96, stream_len);
	TEST_ASSERT_EQUAL(model_p-model, MULTI_ENTRY_HDR_SIZE);
	TEST_ASSERT_EQUAL(up_model_p-up_model, MULTI_ENTRY_HDR_SIZE);
}


void test_compress_s_fx_raw(void)
{
	struct s_fx data[7];
	struct cmp_cfg cfg = {0};
	int cmp_size, cmp_size_exp;
	size_t i;
	struct multi_entry_hdr *hdr;

	cfg.data_type = DATA_TYPE_S_FX;
	cfg.model_buf = NULL;
	cfg.samples = 7;
	cfg.input_buf = malloc(cmp_cal_size_of_data(cfg.samples, cfg.data_type));
	cfg.buffer_length = 7;
	cfg.icu_output_buf = malloc(cmp_cal_size_of_data(cfg.buffer_length, cfg.data_type));
	TEST_ASSERT_NOT_NULL(cfg.icu_output_buf);
	TEST_ASSERT_NOT_NULL(cfg.input_buf);

	data[0].exp_flags = 0x0;
	data[0].fx = 0x0;
	data[1].exp_flags = 0x1;
	data[1].fx = 0x1;
	data[2].exp_flags = 0x2;
	data[2].fx = 0x23;
	data[3].exp_flags = 0x3;
	data[3].fx = 0x42;
	data[4].exp_flags = 0x0;
	data[4].fx = (uint32_t)INT32_MIN;
	data[5].exp_flags = 0x3;
	data[5].fx = INT32_MAX;
	data[6].exp_flags = 0x1;
	data[6].fx = UINT32_MAX;

	hdr = cfg.input_buf;
	memset(hdr, 0x42, sizeof(struct multi_entry_hdr));
	memcpy(hdr->entry, data, sizeof(data));

	cmp_size = icu_compress_data(&cfg);

	cmp_size_exp = (sizeof(data) + sizeof(struct multi_entry_hdr)) * CHAR_BIT;
	TEST_ASSERT_EQUAL_INT(cmp_size_exp, cmp_size);

	for (i = 0; i < ARRAY_SIZE(data); i++) {
		struct s_fx *p;

		hdr = (struct multi_entry_hdr *)cfg.icu_output_buf;
		p = (struct s_fx *)hdr->entry;

		TEST_ASSERT_EQUAL_HEX(data[i].exp_flags, p[i].exp_flags);
		TEST_ASSERT_EQUAL_HEX(data[i].fx, cpu_to_be32(p[i].fx));
	}

	free(cfg.input_buf);
	free(cfg.icu_output_buf);
}


void test_compress_s_fx_staff(void)
{
	struct s_fx data[5];
	struct cmp_cfg cfg = {0};
	int cmp_size, cmp_size_exp;
	struct multi_entry_hdr *hdr;
	uint32_t *cmp_data;

	/* setup configuration */
	cfg.data_type = DATA_TYPE_S_FX;
	cfg.cmp_mode = CMP_MODE_STUFF;
	cfg.samples = 5;
	cfg.input_buf = malloc(cmp_cal_size_of_data(cfg.samples, cfg.data_type));
	TEST_ASSERT_NOT_NULL(cfg.input_buf);
	cfg.buffer_length = 5;
	cfg.icu_output_buf = malloc(cmp_cal_size_of_data(cfg.buffer_length, cfg.data_type));
	TEST_ASSERT_NOT_NULL(cfg.icu_output_buf);
	cfg.cmp_par_exp_flags = 2;
	cfg.cmp_par_fx = 21;
	cfg.max_used_bits = &MAX_USED_BITS_V1;

	/* generate input data */
	hdr = cfg.input_buf;
	/* use dummy data for the header */
	memset(hdr, 0x42, sizeof(struct multi_entry_hdr));
	data[0].exp_flags = 0x0;
	data[0].fx = 0x0;
	data[1].exp_flags = 0x1;
	data[1].fx = 0x1;
	data[2].exp_flags = 0x2;
	data[2].fx = 0x23;
	data[3].exp_flags = 0x3;
	data[3].fx = 0x42;
	data[4].exp_flags = 0x0;
	data[4].fx = 0x001FFFFF;
	memcpy(hdr->entry, data, sizeof(data));

	cmp_size = icu_compress_data(&cfg);

	cmp_size_exp = 5 * (2 + 21) + MULTI_ENTRY_HDR_SIZE * CHAR_BIT;
	TEST_ASSERT_EQUAL_INT(cmp_size_exp, cmp_size);
	TEST_ASSERT_FALSE(memcmp(cfg.input_buf, cfg.icu_output_buf, MULTI_ENTRY_HDR_SIZE));
	hdr = (void *)cfg.icu_output_buf;
	cmp_data = calloc(4, sizeof(*cmp_data));
	memcpy(cmp_data, hdr->entry, 4 * sizeof(*cmp_data));
	TEST_ASSERT_EQUAL_HEX(0x00000080, be32_to_cpu(cmp_data[0]));
	TEST_ASSERT_EQUAL_HEX(0x00060001, be32_to_cpu(cmp_data[1]));
	TEST_ASSERT_EQUAL_HEX(0x1E000423, be32_to_cpu(cmp_data[2]));
	TEST_ASSERT_EQUAL_HEX(0xFFFFE000, be32_to_cpu(cmp_data[3]));

	free(cmp_data);
	free(cfg.input_buf);
	free(cfg.icu_output_buf);
}


void test_compress_s_fx_model_multi(void)
{
	struct s_fx data[6], model[6];
	struct s_fx *up_model_buf;
	struct cmp_cfg cfg = {0};
	int cmp_size;
	struct multi_entry_hdr *hdr;
	uint32_t *cmp_data;
	struct cmp_max_used_bits my_max_used_bits;

	/* setup configuration */
	cfg.data_type = DATA_TYPE_S_FX;
	cfg.cmp_mode = CMP_MODE_MODEL_MULTI;
	cfg.model_value = 11;
	cfg.samples = 6;
	cfg.input_buf = malloc(cmp_cal_size_of_data(cfg.samples, cfg.data_type));
	TEST_ASSERT_NOT_NULL(cfg.input_buf);
	cfg.model_buf = malloc(cmp_cal_size_of_data(cfg.samples, cfg.data_type));
	TEST_ASSERT_NOT_NULL(cfg.model_buf);
	cfg.icu_new_model_buf = malloc(cmp_cal_size_of_data(cfg.samples, cfg.data_type));
	TEST_ASSERT_NOT_NULL(cfg.icu_new_model_buf);
	cfg.buffer_length = 6;
	cfg.icu_output_buf = malloc(cmp_cal_size_of_data(cfg.buffer_length, cfg.data_type));
	TEST_ASSERT_NOT_NULL(cfg.icu_output_buf);
	cfg.cmp_par_exp_flags = 1;
	cfg.spill_exp_flags = 8;
	cfg.cmp_par_fx = 3;
	cfg.spill_fx = 35;


	/* generate input data */
	hdr = cfg.input_buf;
	/* use dummy data for the header */
	memset(hdr, 0x42, sizeof(struct multi_entry_hdr));
	data[0].exp_flags = 0x0;
	data[0].fx = 0x0;
	data[1].exp_flags = 0x1;
	data[1].fx = 0x1;
	data[2].exp_flags = 0x2;
	data[2].fx = 0x23;
	data[3].exp_flags = 0x3;
	data[3].fx = 0x42;
	data[4].exp_flags = 0x0;
	data[4].fx = 0x001FFFFF;
	data[5].exp_flags = 0x0;
	data[5].fx = 0x0;
	memcpy(hdr->entry, data, sizeof(data));

	/* generate model data */
	hdr = cfg.model_buf;
	/* use dummy data for the header */
	memset(hdr, 0x41, sizeof(struct multi_entry_hdr));
	model[0].exp_flags = 0x0;
	model[0].fx = 0x0;
	model[1].exp_flags = 0x3;
	model[1].fx = 0x1;
	model[2].exp_flags = 0x0;
	model[2].fx = 0x42;
	model[3].exp_flags = 0x0;
	model[3].fx = 0x23;
	model[4].exp_flags = 0x3;
	model[4].fx = 0x0;
	model[5].exp_flags = 0x2;
	model[5].fx = 0x001FFFFF;
	memcpy(hdr->entry, model, sizeof(model));

	my_max_used_bits = MAX_USED_BITS_SAFE;
	my_max_used_bits.s_exp_flags = 2;
	my_max_used_bits.s_fx = 21;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);

	cmp_size = icu_compress_data(&cfg);

	TEST_ASSERT_EQUAL_INT(166, cmp_size);
	TEST_ASSERT_FALSE(memcmp(cfg.input_buf, cfg.icu_output_buf, MULTI_ENTRY_HDR_SIZE));
	cmp_data = &cfg.icu_output_buf[MULTI_ENTRY_HDR_SIZE/sizeof(uint32_t)];
	TEST_ASSERT_EQUAL_HEX(0x1C77FFA6, be32_to_cpu(cmp_data[0]));
	TEST_ASSERT_EQUAL_HEX(0xAFFF4DE5, be32_to_cpu(cmp_data[1]));
	TEST_ASSERT_EQUAL_HEX(0xCC000000, be32_to_cpu(cmp_data[2]));

	hdr = cfg.icu_new_model_buf;
	up_model_buf = (struct s_fx *)hdr->entry;
	TEST_ASSERT_FALSE(memcmp(hdr, cfg.icu_output_buf, MULTI_ENTRY_HDR_SIZE));
	TEST_ASSERT_EQUAL_HEX(0x0, up_model_buf[0].exp_flags);
	TEST_ASSERT_EQUAL_HEX(0x0, up_model_buf[0].fx);
	TEST_ASSERT_EQUAL_HEX(0x2, up_model_buf[1].exp_flags);
	TEST_ASSERT_EQUAL_HEX(0x1, up_model_buf[1].fx);
	TEST_ASSERT_EQUAL_HEX(0x0, up_model_buf[2].exp_flags);
	TEST_ASSERT_EQUAL_HEX(0x38, up_model_buf[2].fx);
	TEST_ASSERT_EQUAL_HEX(0x0, up_model_buf[3].exp_flags);
	TEST_ASSERT_EQUAL_HEX(0x2C, up_model_buf[3].fx);
	TEST_ASSERT_EQUAL_HEX(0x2, up_model_buf[4].exp_flags);
	TEST_ASSERT_EQUAL_HEX(0x9FFFF, up_model_buf[4].fx);
	TEST_ASSERT_EQUAL_HEX(0x1, up_model_buf[5].exp_flags);
	TEST_ASSERT_EQUAL_HEX(0x15FFFF, up_model_buf[5].fx);

	free(cfg.input_buf);
	free(cfg.model_buf);
	free(cfg.icu_new_model_buf);
	free(cfg.icu_output_buf);
}


/**
 * @test compress_s_fx
 */

void test_compress_s_fx_error_cases(void)
{
	int error, cmp_bits;
	uint32_t compressed_data_size;
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_exp_flags = MAX_NON_IMA_GOLOMB_PAR;
	uint32_t spillover_exp_flags = 6;
	uint32_t cmp_par_fx = 2;
	uint32_t spillover_fx = 8;
	uint8_t data_to_compress[MULTI_ENTRY_HDR_SIZE+3*sizeof(struct s_fx)] = {0};
	uint8_t compressed_data[MULTI_ENTRY_HDR_SIZE+1*sizeof(struct s_fx)] = {0};
	struct cmp_max_used_bits my_max_used_bits = MAX_USED_BITS_SAFE;
	struct s_fx *data_p = (struct s_fx *)&data_to_compress[MULTI_ENTRY_HDR_SIZE];

	cfg = cmp_cfg_icu_create(DATA_TYPE_S_FX, CMP_MODE_DIFF_ZERO, 0, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, CMP_PAR_UNUSED,
			       CMP_PAR_UNUSED, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       CMP_PAR_UNUSED, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       CMP_PAR_UNUSED);
	TEST_ASSERT_FALSE(error);

	compressed_data_size = cmp_cfg_icu_buffers(&cfg, data_to_compress, 3, NULL,
						   NULL, (uint32_t *)compressed_data, 1);
	TEST_ASSERT_EQUAL_INT(sizeof(compressed_data), compressed_data_size);

	my_max_used_bits.s_exp_flags = 2;
	my_max_used_bits.s_fx = 21;
	error = cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	TEST_ASSERT_FALSE(error);

	/* test if data are higher than max used bits value */
	data_p[0].fx = 0x200000; /* has more than 21 bits (my_max_used_bits.s_fx) */
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	/* compressed data are to small for the compressed_data buffer */
	my_max_used_bits.s_exp_flags = 8;
	my_max_used_bits.s_fx = 32;
	error = cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	TEST_ASSERT_FALSE(error);
	memset(data_to_compress, 0xff, sizeof(data_to_compress));
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, cmp_bits);

	my_max_used_bits.s_exp_flags = 33; /* more than 32 bits are not allowed */
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.s_exp_flags = 32;
	my_max_used_bits.s_fx = 33; /* more than 32 bits are not allowed */
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);
}


/**
 * @test compress_s_fx_efx
 */

void test_compress_s_fx_efx_error_cases(void)
{
	int error, cmp_bits;
	uint32_t compressed_data_size;
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_exp_flags = 2;
	uint32_t spillover_exp_flags = 6;
	uint32_t cmp_par_fx = 1;
	uint32_t spillover_fx = 8;
	uint32_t cmp_par_efx = MAX_NON_IMA_GOLOMB_PAR;
	uint32_t spillover_efx = cmp_icu_max_spill(MAX_NON_IMA_GOLOMB_PAR);
	uint8_t data_to_compress[MULTI_ENTRY_HDR_SIZE+2*sizeof(struct s_fx_efx)] = {0};
	uint8_t compressed_data[MULTI_ENTRY_HDR_SIZE+1*sizeof(struct s_fx_efx)] = {0};
	struct cmp_max_used_bits my_max_used_bits = MAX_USED_BITS_SAFE;
	struct s_fx_efx *data_p = (struct s_fx_efx *)&data_to_compress[MULTI_ENTRY_HDR_SIZE];

	cfg = cmp_cfg_icu_create(DATA_TYPE_S_FX_EFX, CMP_MODE_DIFF_MULTI, 0, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, CMP_PAR_UNUSED,
			       CMP_PAR_UNUSED, cmp_par_efx, spillover_efx,
			       CMP_PAR_UNUSED, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       CMP_PAR_UNUSED);
	TEST_ASSERT_FALSE(error);

	compressed_data_size = cmp_cfg_icu_buffers(&cfg, data_to_compress, 2, NULL,
						   NULL, (uint32_t *)compressed_data, 1);
	TEST_ASSERT_EQUAL_INT(sizeof(compressed_data), compressed_data_size);

	my_max_used_bits.s_exp_flags = 2;
	my_max_used_bits.s_fx = 21;
	my_max_used_bits.s_efx = 16;
	error = cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	TEST_ASSERT_FALSE(error);

	/* test if data are higher than max used bits value */
	data_p[0].exp_flags = 0x4; /* has more than 2 bits (my_max_used_bits.s_exp_flags) */
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[0].exp_flags = 0x3;
	data_p[1].fx = 0x200000; /* has more than 21 bits (my_max_used_bits.fx) */
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[1].fx = 0x1FFFFF;
	data_p[1].efx = 0x100000; /* has more than 16 bits (my_max_used_bits.efx) */
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	/* error case exp_flag setup */
	my_max_used_bits.s_exp_flags = 33;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	/* error case fx setup */
	my_max_used_bits.s_exp_flags = 2;
	my_max_used_bits.s_fx = 33;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	/* error case efx setup */
	my_max_used_bits.s_fx = 21;
	my_max_used_bits.s_efx = 33;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);
}


/**
 * @test compress_s_fx_ncob
 */

void test_compress_s_fx_ncob_error_cases(void)
{
	int error, cmp_bits;
	uint32_t compressed_data_size;
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_exp_flags = 3;
	uint32_t spillover_exp_flags = 6;
	uint32_t cmp_par_fx = 1;
	uint32_t spillover_fx = 8;
	uint32_t cmp_par_ncob = MAX_NON_IMA_GOLOMB_PAR;
	uint32_t spillover_ncob = cmp_icu_max_spill(MAX_NON_IMA_GOLOMB_PAR);
	uint8_t data_to_compress[MULTI_ENTRY_HDR_SIZE+3*sizeof(struct s_fx_ncob)] = {0};
	uint8_t model_data[MULTI_ENTRY_HDR_SIZE+3*sizeof(struct s_fx_ncob)] = {0};
	uint8_t compressed_data[MULTI_ENTRY_HDR_SIZE+1*sizeof(struct s_fx_ncob)] = {0};
	struct cmp_max_used_bits my_max_used_bits = MAX_USED_BITS_SAFE;
	struct s_fx_ncob *data_p = (struct s_fx_ncob *)&data_to_compress[MULTI_ENTRY_HDR_SIZE];


	my_max_used_bits.s_exp_flags = 2;
	my_max_used_bits.s_fx = 21;
	my_max_used_bits.s_ncob = 31;

	cfg = cmp_cfg_icu_create(DATA_TYPE_S_FX_NCOB, CMP_MODE_MODEL_ZERO, 0, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       CMP_PAR_UNUSED, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       CMP_PAR_UNUSED, CMP_PAR_UNUSED, CMP_PAR_UNUSED);
	TEST_ASSERT_FALSE(error);

	error = cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	TEST_ASSERT_FALSE(error);

	compressed_data_size = cmp_cfg_icu_buffers(&cfg, data_to_compress, 3, model_data,
						   NULL, (uint32_t *)compressed_data, 1);
	TEST_ASSERT_EQUAL_INT(sizeof(compressed_data), compressed_data_size);

	/* the compressed_data buffer is to small */
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, cmp_bits);

	/* test if data are higher than max used bits value */
	data_p[2].exp_flags = 0x4; /* has more than 2 bits (my_max_used_bits.s_exp_flags) */
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].exp_flags = 0x3;
	data_p[1].fx = 0x200000; /* value to high */
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[1].fx = 0x1FFFFF; /* value to high */
	data_p[0].ncob_y = 0x80000000; /* value to high */
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);
	data_p[0].ncob_y = 0x7FFFFFFF; /* value to high */

	/* error case exp_flag setup */
	my_max_used_bits.s_exp_flags = 33;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	/* error case fx setup */
	my_max_used_bits.s_exp_flags = 2;
	my_max_used_bits.s_fx = 33;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	/* error case efx setup */
	my_max_used_bits.s_fx = 21;
	my_max_used_bits.s_ncob = 33;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);
}


/**
 * @test compress_s_fx_efx_ncob_ecob
 */

void test_compress_s_fx_efx_ncob_ecob_error_cases(void)
{
	int error, cmp_bits;
	uint32_t compressed_data_size;
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_exp_flags = 3;
	uint32_t spillover_exp_flags = 6;
	uint32_t cmp_par_fx = 1;
	uint32_t spillover_fx = 8;
	uint32_t cmp_par_ncob = MAX_NON_IMA_GOLOMB_PAR;
	uint32_t spillover_ncob = cmp_icu_max_spill(MAX_NON_IMA_GOLOMB_PAR);
	uint32_t cmp_par_efx = MAX_NON_IMA_GOLOMB_PAR;
	uint32_t spillover_efx =  cmp_icu_max_spill(MAX_NON_IMA_GOLOMB_PAR);
	uint32_t cmp_par_ecob = 23;
	uint32_t spillover_ecob = cmp_icu_max_spill(23);
	uint8_t data_to_compress[MULTI_ENTRY_HDR_SIZE+3*sizeof(struct s_fx_efx_ncob_ecob)] = {0};
	uint8_t model_data[MULTI_ENTRY_HDR_SIZE+3*sizeof(struct s_fx_efx_ncob_ecob)] = {0};
	uint8_t compressed_data[MULTI_ENTRY_HDR_SIZE+1*sizeof(struct s_fx_efx_ncob_ecob)] = {0};
	struct cmp_max_used_bits my_max_used_bits = MAX_USED_BITS_SAFE;
	struct s_fx_efx_ncob_ecob *data_p = (struct s_fx_efx_ncob_ecob *)&data_to_compress[MULTI_ENTRY_HDR_SIZE];


	cfg = cmp_cfg_icu_create(DATA_TYPE_S_FX_EFX_NCOB_ECOB, CMP_MODE_MODEL_ZERO, 0, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob,
			       spillover_ncob, cmp_par_efx, spillover_efx,
			       cmp_par_ecob, spillover_ecob, CMP_PAR_UNUSED, CMP_PAR_UNUSED);

	TEST_ASSERT_FALSE(error);

	compressed_data_size = cmp_cfg_icu_buffers(&cfg, data_to_compress, 3, model_data,
						   NULL, (uint32_t *)compressed_data, 1);
	TEST_ASSERT_EQUAL_INT(sizeof(compressed_data), compressed_data_size);

	my_max_used_bits.s_exp_flags = 2;
	my_max_used_bits.s_fx = 21;
	my_max_used_bits.s_ncob = 31;
	my_max_used_bits.s_efx = 23;
	my_max_used_bits.s_ecob = 7;
	error = cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	TEST_ASSERT_FALSE(error);

	/* the compressed_data buffer is to small */
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, cmp_bits);

	/* test if data are higher than max used bits value */
	data_p[2].exp_flags = 0x4;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].exp_flags = 0x3;
	data_p[2].fx = 0x200000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].fx = 0x1FFFFF;
	data_p[1].ncob_x = 0x80000000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[1].ncob_x = 0x7FFFFFFF;
	data_p[1].ncob_y = 0x80000000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[1].ncob_y = 0x7FFFFFFF;
	data_p[1].efx = 0x800000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[1].efx = 0x7FFFFF;
	data_p[1].ecob_y = 0x80;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);
	data_p[1].ecob_y = 0x7F;

	/* error case exp_flag setup */
	my_max_used_bits.s_exp_flags = 33;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	/* error case fx setup */
	my_max_used_bits.s_exp_flags = 32;
	my_max_used_bits.s_fx = 33;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	/* error case efx setup */
	my_max_used_bits.s_fx = 32;
	my_max_used_bits.s_ncob = 33;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.s_ncob = 32;
	my_max_used_bits.s_efx = 33;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.s_efx = 32;
	my_max_used_bits.s_ecob = 33;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);
	my_max_used_bits.s_ecob = 32;
}


/**
 * @test compress_f_fx
 */

void test_compress_f_fx_error_cases(void)
{
	int error, cmp_bits;
	uint32_t compressed_data_size;
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_fx = MAX_NON_IMA_GOLOMB_PAR;
	uint32_t spillover_fx = 8;
	uint8_t data_to_compress[MULTI_ENTRY_HDR_SIZE+3*sizeof(struct f_fx)] = {0};
	uint8_t compressed_data[MULTI_ENTRY_HDR_SIZE+1*sizeof(struct f_fx)] = {0};
	struct cmp_max_used_bits my_max_used_bits = MAX_USED_BITS_SAFE;

	my_max_used_bits.f_fx = 23;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);

	cfg = cmp_cfg_icu_create(DATA_TYPE_F_FX, CMP_MODE_DIFF_ZERO, 0, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);

	error = cmp_cfg_fx_cob(&cfg, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       cmp_par_fx, spillover_fx, CMP_PAR_UNUSED,
			       CMP_PAR_UNUSED, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       CMP_PAR_UNUSED, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       CMP_PAR_UNUSED);
	TEST_ASSERT_FALSE(error);

	compressed_data_size = cmp_cfg_icu_buffers(&cfg, data_to_compress, 3, NULL, NULL, (uint32_t *)compressed_data, 1);
	TEST_ASSERT_EQUAL_INT(sizeof(compressed_data), compressed_data_size);

	/* compressed data are to small for the compressed_data buffer */
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, cmp_bits);

	my_max_used_bits.f_fx = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);
}


/**
 * @test compress_f_fx_efx
 */

void test_compress_f_fx_efx_error_cases(void)
{
	int error, cmp_bits;
	uint32_t compressed_data_size;
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_fx = 1;
	uint32_t spillover_fx = 8;
	uint32_t cmp_par_efx = 1;
	uint32_t spillover_efx = 8;
	uint8_t data_to_compress[MULTI_ENTRY_HDR_SIZE+2*sizeof(struct f_fx_efx)] = {0};
	uint8_t compressed_data[MULTI_ENTRY_HDR_SIZE+1*sizeof(struct f_fx_efx)] = {0};
	struct cmp_max_used_bits my_max_used_bits = MAX_USED_BITS_SAFE;
	struct f_fx_efx *data_p = (struct f_fx_efx *)&data_to_compress[MULTI_ENTRY_HDR_SIZE];

	my_max_used_bits.f_fx = 23;
	my_max_used_bits.f_efx = 31;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);

	cfg = cmp_cfg_icu_create(DATA_TYPE_F_FX_EFX, CMP_MODE_DIFF_ZERO, 0, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);

	error = cmp_cfg_fx_cob(&cfg, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       cmp_par_fx, spillover_fx, CMP_PAR_UNUSED,
			       CMP_PAR_UNUSED, cmp_par_efx, spillover_efx,
			       CMP_PAR_UNUSED, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       CMP_PAR_UNUSED);
	TEST_ASSERT_FALSE(error);

	compressed_data_size = cmp_cfg_icu_buffers(&cfg, data_to_compress, 2, NULL, NULL,
						   (uint32_t *)compressed_data, 1);
	TEST_ASSERT_EQUAL_INT(sizeof(compressed_data), compressed_data_size);

	/* compressed data are to small for the compressed_data buffer */
	data_p[0].fx = 42;
	data_p[0].efx = 42;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, cmp_bits);

	/* efx value is to big for the max used bits values */
	data_p[0].efx = 0x80000000;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);
	data_p[0].efx = 0x7FFFFFFF;

	my_max_used_bits.f_fx = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.f_fx = 32;
	my_max_used_bits.f_efx = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);
}


/**
 * @test compress_f_fx_ncob
 */

void test_compress_f_fx_ncob_error_cases(void)
{
	int error, cmp_bits;
	uint32_t compressed_data_size;
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_fx = MAX_NON_IMA_GOLOMB_PAR;
	uint32_t spillover_fx = 8;
	uint32_t cmp_par_ncob = 1;
	uint32_t spillover_ncob = 8;
	uint8_t data_to_compress[MULTI_ENTRY_HDR_SIZE+2*sizeof(struct f_fx_ncob)] = {0};
	uint8_t compressed_data[MULTI_ENTRY_HDR_SIZE+1*sizeof(struct f_fx_ncob)] = {0};
	struct cmp_max_used_bits my_max_used_bits = MAX_USED_BITS_SAFE;
	struct f_fx_ncob *data_p = (struct f_fx_ncob *)&data_to_compress[MULTI_ENTRY_HDR_SIZE];

	my_max_used_bits.f_fx = 31;
	my_max_used_bits.f_ncob = 23;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);

	cfg = cmp_cfg_icu_create(DATA_TYPE_F_FX_NCOB, CMP_MODE_DIFF_ZERO, 0, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);

	error = cmp_cfg_fx_cob(&cfg, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       CMP_PAR_UNUSED, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       CMP_PAR_UNUSED, CMP_PAR_UNUSED, CMP_PAR_UNUSED);
	TEST_ASSERT_FALSE(error);

	compressed_data_size = cmp_cfg_icu_buffers(&cfg, data_to_compress, 2, NULL, NULL,
						   (uint32_t *)compressed_data, 1);
	TEST_ASSERT_EQUAL_INT(sizeof(compressed_data), compressed_data_size);

	/* compressed data are to small for the compressed_data buffer */
	data_p[0].fx = 42;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, cmp_bits);

	/* value is to big for the max used bits values */
	data_p[0].ncob_x = 0x800000;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);
	data_p[0].ncob_x = 0x7FFFFF;
	data_p[0].ncob_y = 0x800000;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);
	data_p[0].ncob_y = 0x7FFFFF;

	my_max_used_bits.f_fx = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.f_fx = 32;
	my_max_used_bits.f_ncob = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);
}


/**
 * @test compress_f_fx_efx_ncob_ecob
 */

void test_compress_f_fx_efx_ncob_ecob(void)
{
	int error, cmp_bits;
	uint32_t compressed_data_size;
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_fx = 1;
	uint32_t spillover_fx = 8;
	uint32_t cmp_par_ncob = 2;
	uint32_t spillover_ncob = 10;
	uint32_t cmp_par_efx = 3;
	uint32_t spillover_efx = 44;
	uint32_t cmp_par_ecob = 5;
	uint32_t spillover_ecob = 55;
	uint8_t data_to_compress[MULTI_ENTRY_HDR_SIZE+4*sizeof(struct f_fx_efx_ncob_ecob)] = {0};
	uint8_t compressed_data[MULTI_ENTRY_HDR_SIZE+1*sizeof(struct f_fx_efx_ncob_ecob)] = {0};
	struct cmp_max_used_bits my_max_used_bits = MAX_USED_BITS_SAFE;
	struct f_fx_efx_ncob_ecob *data_p = (struct f_fx_efx_ncob_ecob *)&data_to_compress[MULTI_ENTRY_HDR_SIZE];

	cfg = cmp_cfg_icu_create(DATA_TYPE_F_FX_EFX_NCOB_ECOB, CMP_MODE_DIFF_ZERO, 0, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);

	error = cmp_cfg_fx_cob(&cfg, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       CMP_PAR_UNUSED, CMP_PAR_UNUSED);
	TEST_ASSERT_FALSE(error);

	compressed_data_size = cmp_cfg_icu_buffers(&cfg, data_to_compress, 4, NULL, NULL,
						   (uint32_t *)compressed_data, 1);
	TEST_ASSERT_EQUAL_INT(sizeof(compressed_data), compressed_data_size);

	my_max_used_bits.f_fx = 31;
	my_max_used_bits.f_ncob = 3;
	my_max_used_bits.f_efx = 16;
	my_max_used_bits.f_ecob = 8;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);

	/* value is to big for the max used bits values */
	data_p[3].fx = 0x80000000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[3].fx = 0x80000000-1;
	data_p[2].ncob_x = 0x8;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].ncob_x = 0x7;
	data_p[1].ncob_y = 0x8;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[1].ncob_y = 0x7;
	data_p[0].efx = 0x10000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[0].efx = 0x10000-1;
	data_p[2].ecob_x = 0x100;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].ecob_x = 0x100-1;
	data_p[3].ecob_y = 0x100;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);
	data_p[3].ecob_y = 0x100-1;

	my_max_used_bits.f_fx = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.f_fx = 32;
	my_max_used_bits.f_ncob = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.f_ncob = 32;
	my_max_used_bits.f_efx = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.f_efx = 32;
	my_max_used_bits.f_ecob = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);
}


/**
 * @test compress_l_fx
 */

void test_compress_l_fx_error_cases(void)
{
	int error, cmp_bits;
	uint32_t compressed_data_size;
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_exp_flags = 3;
	uint32_t spillover_exp_flags = 10;
	uint32_t cmp_par_fx = 1;
	uint32_t spillover_fx = 8;
	uint32_t cmp_par_fx_cob_variance = 30;
	uint32_t spillover_fx_cob_variance = 8;
	uint8_t data_to_compress[MULTI_ENTRY_HDR_SIZE+3*sizeof(struct l_fx)] = {0};
	uint8_t compressed_data[MULTI_ENTRY_HDR_SIZE+1*sizeof(struct l_fx)] = {0};
	struct cmp_max_used_bits my_max_used_bits = MAX_USED_BITS_SAFE;
	struct l_fx *data_p = (struct l_fx *)&data_to_compress[MULTI_ENTRY_HDR_SIZE];

	cfg = cmp_cfg_icu_create(DATA_TYPE_L_FX, CMP_MODE_DIFF_ZERO, 0, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       CMP_PAR_UNUSED, CMP_PAR_UNUSED, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);

	compressed_data_size = cmp_cfg_icu_buffers(&cfg, data_to_compress, 3, NULL, NULL,
						   (uint32_t *)compressed_data, 1);
	TEST_ASSERT_EQUAL_INT(sizeof(compressed_data), compressed_data_size);

	my_max_used_bits.l_exp_flags = 23;
	my_max_used_bits.l_fx = 31;
	my_max_used_bits.l_efx = 1;
	my_max_used_bits.l_fx_variance = 23;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);

	/* value is to big for the max used bits values */
	data_p[2].exp_flags = 0x800000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].exp_flags = 0x800000-1;
	data_p[2].fx = 0x80000000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].fx = 0x80000000-1;
	data_p[0].fx_variance = 0x800000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[0].fx_variance = 0x800000-1;

	my_max_used_bits.l_exp_flags = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.l_exp_flags = 32;
	my_max_used_bits.l_fx = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.l_fx = 32;
	my_max_used_bits.l_fx_variance = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);
}


/**
 * @test compress_l_fx_efx
 */

void test_compress_l_fx_efx_error_cases(void)
{
	int error, cmp_bits;
	uint32_t compressed_data_size;
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_exp_flags = MAX_NON_IMA_GOLOMB_PAR;
	uint32_t spillover_exp_flags = cmp_icu_max_spill(cmp_par_exp_flags);
	uint32_t cmp_par_fx = 1;
	uint32_t spillover_fx = 8;
	uint32_t cmp_par_efx = 3;
	uint32_t spillover_efx = 44;
	uint32_t cmp_par_fx_cob_variance = 30;
	uint32_t spillover_fx_cob_variance = 8;
	uint8_t data_to_compress[MULTI_ENTRY_HDR_SIZE+3*sizeof(struct l_fx_efx)] = {0};
	uint8_t compressed_data[MULTI_ENTRY_HDR_SIZE+1*sizeof(struct l_fx_efx)] = {0};
	struct cmp_max_used_bits my_max_used_bits = MAX_USED_BITS_SAFE;
	struct l_fx_efx *data_p = (struct l_fx_efx *)&data_to_compress[MULTI_ENTRY_HDR_SIZE];

	cfg = cmp_cfg_icu_create(DATA_TYPE_L_FX_EFX, CMP_MODE_DIFF_ZERO, 0, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       cmp_par_efx, spillover_efx, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);

	compressed_data_size = cmp_cfg_icu_buffers(&cfg, data_to_compress, 3, NULL, NULL,
						   (uint32_t *)compressed_data, 1);
	TEST_ASSERT_EQUAL_INT(sizeof(compressed_data), compressed_data_size);

	my_max_used_bits.l_exp_flags = 23;
	my_max_used_bits.l_fx = 31;
	my_max_used_bits.l_efx = 1;
	my_max_used_bits.l_fx_variance = 23;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);

	/* value is to big for the max used bits values */
	data_p[2].exp_flags = 0x800000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].exp_flags = 0x800000-1;
	data_p[2].fx = 0x80000000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].fx = 0x80000000-1;
	data_p[1].efx = 0x2;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[1].efx = 0x1;
	data_p[0].fx_variance = 0x800000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[0].fx_variance = 0x800000-1;

	my_max_used_bits.l_exp_flags = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.l_exp_flags = 32;
	my_max_used_bits.l_fx = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.l_fx = 32;
	my_max_used_bits.l_efx = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.l_efx = 32;
	my_max_used_bits.l_fx_variance = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);
}


/**
 * @test compress_l_fx_ncob
 */

void test_compress_l_fx_ncob_error_cases(void)
{
	int error, cmp_bits;
	uint32_t compressed_data_size;
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_exp_flags = MAX_NON_IMA_GOLOMB_PAR;
	uint32_t spillover_exp_flags = cmp_icu_max_spill(cmp_par_exp_flags);
	uint32_t cmp_par_fx = 1;
	uint32_t spillover_fx = 8;
	uint32_t cmp_par_ncob = 2;
	uint32_t spillover_ncob = 10;
	uint32_t cmp_par_fx_cob_variance = 30;
	uint32_t spillover_fx_cob_variance = 8;
	uint8_t data_to_compress[MULTI_ENTRY_HDR_SIZE+3*sizeof(struct l_fx_ncob)] = {0};
	uint8_t compressed_data[MULTI_ENTRY_HDR_SIZE+1*sizeof(struct l_fx_ncob)] = {0};
	struct cmp_max_used_bits my_max_used_bits = MAX_USED_BITS_SAFE;
	struct l_fx_ncob *data_p = (struct l_fx_ncob *)&data_to_compress[MULTI_ENTRY_HDR_SIZE];

	cfg = cmp_cfg_icu_create(DATA_TYPE_L_FX_NCOB, CMP_MODE_DIFF_ZERO, 0, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       CMP_PAR_UNUSED, CMP_PAR_UNUSED, CMP_PAR_UNUSED, CMP_PAR_UNUSED,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);

	compressed_data_size = cmp_cfg_icu_buffers(&cfg, data_to_compress, 3, NULL, NULL,
						   (uint32_t *)compressed_data, 1);
	TEST_ASSERT_EQUAL_INT(sizeof(compressed_data), compressed_data_size);

	my_max_used_bits.l_exp_flags = 23;
	my_max_used_bits.l_fx = 31;
	my_max_used_bits.l_ncob = 2;
	my_max_used_bits.l_fx_variance = 23;
	my_max_used_bits.l_cob_variance = 11;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);

	/* value is to big for the max used bits values */
	data_p[2].exp_flags = 0x800000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].exp_flags = 0x800000-1;
	data_p[2].fx = 0x80000000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].fx = 0x80000000-1;
	data_p[2].ncob_x = 0x4;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].ncob_x = 0x3;
	data_p[2].ncob_y = 0x4;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].ncob_y = 0x3;
	data_p[0].fx_variance = 0x800000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[0].fx_variance = 0x800000-1;
	data_p[2].cob_x_variance = 0x800;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].cob_x_variance = 0x800-1;
	data_p[2].cob_y_variance = 0x800;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].cob_y_variance = 0x800-1;

	my_max_used_bits.l_exp_flags = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.l_exp_flags = 32;
	my_max_used_bits.l_fx = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.l_fx = 32;
	my_max_used_bits.l_ncob = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.l_ncob = 32;
	my_max_used_bits.l_fx_variance = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.l_fx_variance = 32;
	my_max_used_bits.l_cob_variance = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);
}


/**
 * @test compress_l_fx_efx_ncob_ecob
 */

void test_compress_l_fx_efx_ncob_ecob_error_cases(void)
{
	int error, cmp_bits;
	uint32_t compressed_data_size;
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_exp_flags = MAX_NON_IMA_GOLOMB_PAR;
	uint32_t spillover_exp_flags = cmp_icu_max_spill(cmp_par_exp_flags);
	uint32_t cmp_par_fx = 1;
	uint32_t spillover_fx = 8;
	uint32_t cmp_par_ncob = 2;
	uint32_t spillover_ncob = 10;
	uint32_t cmp_par_efx = 3;
	uint32_t spillover_efx = 44;
	uint32_t cmp_par_ecob = 5;
	uint32_t spillover_ecob = 55;
	uint32_t cmp_par_fx_cob_variance = 30;
	uint32_t spillover_fx_cob_variance = 8;
	uint8_t data_to_compress[MULTI_ENTRY_HDR_SIZE+3*sizeof(struct l_fx_efx_ncob_ecob)] = {0};
	uint8_t compressed_data[MULTI_ENTRY_HDR_SIZE+1*sizeof(struct l_fx_efx_ncob_ecob)] = {0};
	struct cmp_max_used_bits my_max_used_bits = MAX_USED_BITS_SAFE;
	struct l_fx_efx_ncob_ecob *data_p = (struct l_fx_efx_ncob_ecob *)&data_to_compress[MULTI_ENTRY_HDR_SIZE];

	cfg = cmp_cfg_icu_create(DATA_TYPE_L_FX_EFX_NCOB_ECOB, CMP_MODE_DIFF_ZERO, 0, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);

	error = cmp_cfg_fx_cob(&cfg, cmp_par_exp_flags, spillover_exp_flags,
			       cmp_par_fx, spillover_fx, cmp_par_ncob, spillover_ncob,
			       cmp_par_efx, spillover_efx, cmp_par_ecob, spillover_ecob,
			       cmp_par_fx_cob_variance, spillover_fx_cob_variance);
	TEST_ASSERT_FALSE(error);

	compressed_data_size = cmp_cfg_icu_buffers(&cfg, data_to_compress, 3, NULL, NULL,
						   (uint32_t *)compressed_data, 1);
	TEST_ASSERT_EQUAL_INT(sizeof(compressed_data), compressed_data_size);

	my_max_used_bits.l_exp_flags = 23;
	my_max_used_bits.l_fx = 31;
	my_max_used_bits.l_ncob = 2;
	my_max_used_bits.l_efx = 1;
	my_max_used_bits.l_ecob = 3;
	my_max_used_bits.l_fx_variance = 23;
	my_max_used_bits.l_cob_variance = 11;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);

	/* value is to big for the max used bits values */
	data_p[2].exp_flags = 0x800000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].exp_flags = 0x800000-1;
	data_p[2].fx = 0x80000000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].fx = 0x80000000-1;
	data_p[2].ncob_x = 0x4;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].ncob_x = 0x3;
	data_p[2].ncob_y = 0x4;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].ncob_y = 0x3;
	data_p[1].efx = 0x2;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[1].efx = 0x1;
	data_p[1].ecob_x = 0x8;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[1].ecob_x = 0x7;
	data_p[1].ecob_y = 0x8;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[1].ecob_y = 0x7;
	data_p[0].fx_variance = 0x800000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[0].fx_variance = 0x800000-1;
	data_p[2].cob_x_variance = 0x800;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].cob_x_variance = 0x800-1;
	data_p[2].cob_y_variance = 0x800;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[2].cob_y_variance = 0x800-1;

	my_max_used_bits.l_exp_flags = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.l_exp_flags = 32;
	my_max_used_bits.l_fx = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.l_fx = 32;
	my_max_used_bits.l_ncob = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.l_ncob = 32;
	my_max_used_bits.l_efx = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.l_efx = 32;
	my_max_used_bits.l_ecob = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.l_ecob = 32;
	my_max_used_bits.l_fx_variance = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.l_fx_variance = 32;
	my_max_used_bits.l_cob_variance = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);
}


/**
 * @test compress_nc_offset
 */

void test_compress_nc_offset_error_cases(void)
{
	int error, cmp_bits;
	uint32_t compressed_data_size;
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_mean = 1;
	uint32_t spillover_mean = 2;
	uint32_t cmp_par_variance = MAX_NON_IMA_GOLOMB_PAR;
	uint32_t spillover_variance = cmp_icu_max_spill(MAX_NON_IMA_GOLOMB_PAR);
	uint8_t data_to_compress[MULTI_ENTRY_HDR_SIZE+3*sizeof(struct nc_offset)] = {0};
	uint8_t compressed_data[MULTI_ENTRY_HDR_SIZE+1*sizeof(struct nc_offset)] = {0};
	struct cmp_max_used_bits my_max_used_bits = MAX_USED_BITS_SAFE;
	struct nc_offset *data_p = (struct nc_offset *)&data_to_compress[MULTI_ENTRY_HDR_SIZE];

	cfg = cmp_cfg_icu_create(DATA_TYPE_OFFSET, CMP_MODE_DIFF_MULTI, 0, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);

	error = cmp_cfg_aux(&cfg, cmp_par_mean, spillover_mean,
			    cmp_par_variance, spillover_variance,
			    CMP_PAR_UNUSED, CMP_PAR_UNUSED);
	TEST_ASSERT_FALSE(error);

	compressed_data_size = cmp_cfg_icu_buffers(&cfg, data_to_compress, 3, NULL, NULL,
						   (uint32_t *)compressed_data, 1);
	TEST_ASSERT_EQUAL_INT(sizeof(compressed_data), compressed_data_size);

	my_max_used_bits.nc_offset_mean = 1;
	my_max_used_bits.nc_offset_variance = 31;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);

	/* value is to big for the max used bits values */
	data_p[0].mean = 0x2;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[0].mean = 0x1;
	data_p[1].variance = 0x80000000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[1].variance = 0x80000000-1;

	my_max_used_bits.nc_offset_mean = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.nc_offset_mean = 32;
	my_max_used_bits.nc_offset_variance = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);
}


/**
 * @test compress_nc_background
 */

void test_compress_nc_background_error_cases(void)
{
	int error, cmp_bits;
	uint32_t compressed_data_size;
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_mean = 1;
	uint32_t spillover_mean = 2;
	uint32_t cmp_par_variance = MAX_NON_IMA_GOLOMB_PAR;
	uint32_t spillover_variance = cmp_icu_max_spill(MAX_NON_IMA_GOLOMB_PAR);
	uint32_t cmp_par_pixels_error = 23;
	uint32_t spillover_pixels_error = 42;
	uint8_t data_to_compress[MULTI_ENTRY_HDR_SIZE+3*sizeof(struct nc_background)] = {0};
	uint8_t compressed_data[MULTI_ENTRY_HDR_SIZE+1*sizeof(struct nc_background)] = {0};
	struct cmp_max_used_bits my_max_used_bits = MAX_USED_BITS_SAFE;
	struct nc_background *data_p = (struct nc_background *)&data_to_compress[MULTI_ENTRY_HDR_SIZE];

	cfg = cmp_cfg_icu_create(DATA_TYPE_BACKGROUND, CMP_MODE_DIFF_MULTI, 0, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);

	error = cmp_cfg_aux(&cfg, cmp_par_mean, spillover_mean,
			    cmp_par_variance, spillover_variance,
			    cmp_par_pixels_error, spillover_pixels_error);
	TEST_ASSERT_FALSE(error);

	compressed_data_size = cmp_cfg_icu_buffers(&cfg, data_to_compress, 3, NULL, NULL,
						   (uint32_t *)compressed_data, 1);
	TEST_ASSERT_EQUAL_INT(sizeof(compressed_data), compressed_data_size);

	my_max_used_bits.nc_background_mean = 1;
	my_max_used_bits.nc_background_variance = 31;
	my_max_used_bits.nc_background_outlier_pixels = 2;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);

	/* value is to big for the max used bits values */
	data_p[0].mean = 0x2;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[0].mean = 0x1;
	data_p[1].variance = 0x80000000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[1].variance = 0x80000000-1;
	data_p[1].outlier_pixels = 0x4;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[1].outlier_pixels = 0x3;

	my_max_used_bits.nc_background_mean = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.nc_background_mean = 32;
	my_max_used_bits.nc_background_variance = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.nc_background_variance = 32;
	my_max_used_bits.nc_background_outlier_pixels = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);
}


/**
 * @test compress_smearing
 */

void test_compress_smearing_error_cases(void)
{
	int error, cmp_bits;
	uint32_t compressed_data_size;
	struct cmp_cfg cfg = {0};
	uint32_t cmp_par_mean = 1;
	uint32_t spillover_mean = 2;
	uint32_t cmp_par_variance = MAX_NON_IMA_GOLOMB_PAR;
	uint32_t spillover_variance = cmp_icu_max_spill(MAX_NON_IMA_GOLOMB_PAR);
	uint32_t cmp_par_pixels_error = 23;
	uint32_t spillover_pixels_error = 42;
	uint8_t data_to_compress[MULTI_ENTRY_HDR_SIZE+3*sizeof(struct smearing)] = {0};
	uint8_t compressed_data[MULTI_ENTRY_HDR_SIZE+1*sizeof(struct smearing)] = {0};
	struct cmp_max_used_bits my_max_used_bits = MAX_USED_BITS_SAFE;
	struct smearing *data_p = (struct smearing *)&data_to_compress[MULTI_ENTRY_HDR_SIZE];

	cfg = cmp_cfg_icu_create(DATA_TYPE_SMEARING, CMP_MODE_DIFF_MULTI, 0, CMP_LOSSLESS);
	TEST_ASSERT(cfg.data_type != DATA_TYPE_UNKNOWN);

	error = cmp_cfg_aux(&cfg, cmp_par_mean, spillover_mean,
			    cmp_par_variance, spillover_variance,
			    cmp_par_pixels_error, spillover_pixels_error);
	TEST_ASSERT_FALSE(error);

	compressed_data_size = cmp_cfg_icu_buffers(&cfg, data_to_compress, 3, NULL, NULL,
						   (uint32_t *)compressed_data, 1);
	TEST_ASSERT_EQUAL_INT(sizeof(compressed_data), compressed_data_size);

	my_max_used_bits.smearing_mean = 1;
	my_max_used_bits.smearing_variance_mean = 15;
	my_max_used_bits.smearing_outlier_pixels = 2;
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);

	/* value is to big for the max used bits values */
	data_p[0].mean = 0x2;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[0].mean = 0x1;
	data_p[1].variance_mean = 0x8000;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[1].variance_mean = 0x8000-1;
	data_p[1].outlier_pixels = 0x4;
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_HIGH_VALUE, cmp_bits);

	data_p[1].outlier_pixels = 0x3;

	my_max_used_bits.smearing_mean = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.smearing_mean = 32;
	my_max_used_bits.smearing_variance_mean = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);

	my_max_used_bits.smearing_variance_mean = 32;
	my_max_used_bits.smearing_outlier_pixels = 33; /* more than 32 bits are not allowed */
	cmp_cfg_icu_max_used_bits(&cfg, &my_max_used_bits);
	cmp_bits = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL_INT(-1, cmp_bits);
}


/**
 * @test pad_bitstream
 */

void test_pad_bitstream(void)
{
	struct cmp_cfg cfg = {0};
	int cmp_size;
	int cmp_size_return;
	uint32_t cmp_data[3];
	const int MAX_BIT_LEN = 96;

	memset(cmp_data, 0xFF, sizeof(cmp_data));
	cfg.icu_output_buf = cmp_data;
	cfg.data_type = DATA_TYPE_IMAGETTE; /* 16 bit samples */
	cfg.buffer_length = 6; /* 6 * 16 bit samples -> 3 * 32 bit */

	/* test negative cmp_size */
	cmp_size = -1;
	cmp_size_return = pad_bitstream(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(-1, cmp_size_return);
	cmp_size = -3;
	cmp_size_return = pad_bitstream(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(-3, cmp_size_return);

	/* test RAW_MODE */
	cfg.cmp_mode = CMP_MODE_RAW;
	cmp_size = MAX_BIT_LEN;
	cmp_size_return = pad_bitstream(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(MAX_BIT_LEN, cmp_size_return);
	TEST_ASSERT_EQUAL_INT(cmp_data[0], 0xFFFFFFFF);
	TEST_ASSERT_EQUAL_INT(cmp_data[1], 0xFFFFFFFF);
	TEST_ASSERT_EQUAL_INT(cmp_data[2], 0xFFFFFFFF);

	/* test Normal operation */
	cfg.cmp_mode = CMP_MODE_MODEL_MULTI;
	cmp_size = 0;
	/* set the first 32 bits zero no change should occur */
	cmp_size = put_n_bits32(0, 32, cmp_size, cfg.icu_output_buf, MAX_BIT_LEN);
	cmp_size_return = pad_bitstream(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(cmp_size, cmp_size_return);
	TEST_ASSERT_EQUAL_INT(cmp_data[0], 0);
	TEST_ASSERT_EQUAL_INT(cmp_data[1], 0xFFFFFFFF);
	TEST_ASSERT_EQUAL_INT(cmp_data[2], 0xFFFFFFFF);

	/* set the first 33 bits zero; and checks the padding  */
	cmp_size = put_n_bits32(0, 1, cmp_size, cfg.icu_output_buf, MAX_BIT_LEN);
	cmp_size_return = pad_bitstream(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(cmp_size, cmp_size_return);
	TEST_ASSERT_EQUAL_INT(cmp_data[0], 0);
	TEST_ASSERT_EQUAL_INT(cmp_data[1], 0);
	TEST_ASSERT_EQUAL_INT(cmp_data[2], 0xFFFFFFFF);

	/* set the first 63 bits zero; and checks the padding  */
	cmp_data[1] = 0xFFFFFFFF;
	cmp_size = 32;
	cmp_size = put_n_bits32(0, 31, cmp_size, cfg.icu_output_buf, MAX_BIT_LEN);
	cmp_size_return = pad_bitstream(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(cmp_size, cmp_size_return);
	TEST_ASSERT_EQUAL_INT(cmp_data[0], 0);
	TEST_ASSERT_EQUAL_INT(cmp_data[1], 0);
	TEST_ASSERT_EQUAL_INT(cmp_data[2], 0xFFFFFFFF);

	/* error case the rest of the compressed data are to small dor a 32 bit
	 * access  */
	cfg.buffer_length = 5;
	cmp_size = 64;
	cmp_size = put_n_bits32(0, 1, cmp_size, cfg.icu_output_buf, MAX_BIT_LEN);
	cmp_size_return = pad_bitstream(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUF, cmp_size_return);
}


/**
 * @test cmp_data_to_big_endian
 */

void test_cmp_data_to_big_endian_error_cases(void)
{
	struct cmp_cfg cfg = {0};
	int cmp_size;
	int cmp_size_return;
	uint16_t cmp_data[3] = {0x0123, 0x4567, 0x89AB};
	uint32_t output_buf[2] = {0};
	uint8_t *p;

	memcpy(output_buf, cmp_data, sizeof(cmp_data));
	cfg.icu_output_buf = output_buf;

	/* this should work */
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_RAW;
	cmp_size = 48;
	cmp_size_return = cmp_data_to_big_endian(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(cmp_size_return, 48);
	p = (uint8_t *)cfg.icu_output_buf;
	TEST_ASSERT_EQUAL(p[0], 0x01);
	TEST_ASSERT_EQUAL(p[1], 0x23);
	TEST_ASSERT_EQUAL(p[2], 0x45);
	TEST_ASSERT_EQUAL(p[3], 0x67);
	TEST_ASSERT_EQUAL(p[4], 0x89);
	TEST_ASSERT_EQUAL(p[5], 0xAB);

	/* error cases */
	cmp_data[0] = 0x0123;
	cmp_data[1] = 0x4567;
	cmp_data[2] = 0x89AB;
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_RAW;
	cmp_size = 47; /* wrong size */
	cmp_size_return = cmp_data_to_big_endian(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(cmp_size_return, -1);

	cmp_data[0] = 0x0123;
	cmp_data[1] = 0x4567;
	cmp_data[2] = 0x89AB;
	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_RAW;
	cmp_size = 49; /* wrong size */
	cmp_size_return = cmp_data_to_big_endian(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(cmp_size_return, -1);

	cmp_data[0] = 0x0123;
	cmp_data[1] = 0x4567;
	cmp_data[2] = 0x89AB;
	cfg.data_type = DATA_TYPE_UNKNOWN; /* wrong data_type */
	cfg.cmp_mode = CMP_MODE_RAW;
	cmp_size = 48;
	cmp_size_return = cmp_data_to_big_endian(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(cmp_size_return, -1);
}


/**
 * @test icu_compress_data
 */

void test_icu_compress_data_error_cases(void)
{
	int cmp_size;
	struct cmp_cfg cfg = {0};

	/* cfg = NULL test */
	cmp_size = icu_compress_data(NULL);
	TEST_ASSERT_EQUAL(-1, cmp_size);

	/* samples = 0 test */
	cfg.samples = 0;
	cmp_size = icu_compress_data(&cfg);
	TEST_ASSERT_EQUAL(0, cmp_size);
}


/**
 * @test zero_escape_mech_is_used
 */

void test_zero_escape_mech_is_used(void)
{
	enum cmp_mode cmp_mode;

	for (cmp_mode = 0; cmp_mode <= CMP_MODE_STUFF; cmp_mode++) {
		int res = zero_escape_mech_is_used(cmp_mode);

		if (cmp_mode == CMP_MODE_DIFF_ZERO ||
		    cmp_mode == CMP_MODE_MODEL_ZERO)
			TEST_ASSERT_TRUE(res);
		else
			TEST_ASSERT_FALSE(res);
	}
}


void test_support_function_call_NULL(void)
{
	TEST_ASSERT_TRUE(cmp_cfg_gen_par_is_invalid(NULL, ICU_CHECK));
	TEST_ASSERT_TRUE(cmp_cfg_icu_buffers_is_invalid(NULL));
	TEST_ASSERT_TRUE(cmp_cfg_imagette_is_invalid(NULL, ICU_CHECK));
	TEST_ASSERT_TRUE(cmp_cfg_fx_cob_is_invalid(NULL));
	TEST_ASSERT_TRUE(cmp_cfg_aux_is_invalid(NULL));
	TEST_ASSERT_TRUE(cmp_cfg_icu_is_invalid(NULL));
	TEST_ASSERT_TRUE(cmp_cfg_fx_cob_get_need_pars(DATA_TYPE_S_FX, NULL));
}


/**
 * @test test_print_cmp_info
 */

void test_print_cmp_info(void)
{
	struct cmp_info info;

	info.cmp_mode_used = 1;
	info.spill_used = 2;
	info.golomb_par_used = 3;
	info.samples_used = 4;
	info.cmp_size = 5;
	info.ap1_cmp_size = 6;
	info.ap2_cmp_size = 7;
	info.rdcu_new_model_adr_used = 8;
	info.rdcu_cmp_adr_used = 9;
	info.model_value_used = 10;
	info.round_used = 11;
	info.cmp_err = 12;

	print_cmp_info(&info);
	print_cmp_info(NULL);
}

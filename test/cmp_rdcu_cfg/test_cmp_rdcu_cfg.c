/**
 * @file test_cmp_rdcu_cfg.c
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at),
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
 * @brief hardware compressor configuration tests
 */

#include <stdint.h>

#include <unity.h>
#include <cmp_rdcu_cfg.h>
#include <rdcu_cmd.h>


/**
 * @test rdcu_cfg_create
 */

void test_rdcu_cfg_create(void)
{
	struct cmp_cfg cfg;
	enum cmp_data_type data_type;
	enum cmp_mode cmp_mode;
	uint32_t model_value, lossy_par;

	/* wrong data type tests */
	data_type = DATA_TYPE_UNKNOWN; /* not valid data type */
	cmp_mode = CMP_MODE_RAW;
	model_value = 0;
	lossy_par = CMP_LOSSLESS;
	cfg = rdcu_cfg_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL(DATA_TYPE_UNKNOWN, cfg.data_type);

	/* data_type not supported by RDCU */
	data_type = DATA_TYPE_OFFSET;
	cfg = rdcu_cfg_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL(DATA_TYPE_UNKNOWN, cfg.data_type);

	data_type = -1; /* not valid data type */
	cfg = rdcu_cfg_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL(DATA_TYPE_UNKNOWN, cfg.data_type);

	 /*now it should work */
	data_type = DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE;
	cfg = rdcu_cfg_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL(data_type, cfg.data_type);
	TEST_ASSERT_EQUAL(cmp_mode, cfg.cmp_mode);
	TEST_ASSERT_EQUAL(model_value, cfg.model_value);
	TEST_ASSERT_EQUAL(lossy_par, cfg.round);


	/* wrong compression mode tests */
	cmp_mode = CMP_MODE_STUFF; /* not a RDCU compression mode */
	cfg = rdcu_cfg_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL(DATA_TYPE_UNKNOWN, cfg.data_type);

	cmp_mode = -1;
	cfg = rdcu_cfg_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL(DATA_TYPE_UNKNOWN, cfg.data_type);

	/* this should work */
	cmp_mode = 4;
	cfg = rdcu_cfg_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL(data_type, cfg.data_type);
	TEST_ASSERT_EQUAL(cmp_mode, cfg.cmp_mode);
	TEST_ASSERT_EQUAL(model_value, cfg.model_value);
	TEST_ASSERT_EQUAL(lossy_par, cfg.round);


	/* wrong model_value tests */
	cmp_mode = CMP_MODE_DIFF_ZERO;
	model_value = MAX_MODEL_VALUE + 1;
	cfg = rdcu_cfg_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL(DATA_TYPE_UNKNOWN, cfg.data_type);

	cmp_mode = CMP_MODE_RAW;
	model_value = -1U;
	cfg = rdcu_cfg_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL(DATA_TYPE_UNKNOWN, cfg.data_type);

	/* this should work */
	model_value = MAX_MODEL_VALUE;
	cfg = rdcu_cfg_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL(data_type, cfg.data_type);
	TEST_ASSERT_EQUAL(cmp_mode, cfg.cmp_mode);
	TEST_ASSERT_EQUAL(model_value, cfg.model_value);
	TEST_ASSERT_EQUAL(lossy_par, cfg.round);


	/* wrong lossy_par tests */
	lossy_par = MAX_RDCU_ROUND + 1;
	cfg = rdcu_cfg_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL(DATA_TYPE_UNKNOWN, cfg.data_type);

	lossy_par = -1U;
	cfg = rdcu_cfg_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL(DATA_TYPE_UNKNOWN, cfg.data_type);

	/* this should work */
	lossy_par = MAX_RDCU_ROUND;
	cfg = rdcu_cfg_create(data_type, cmp_mode, model_value, lossy_par);
	TEST_ASSERT_EQUAL(data_type, cfg.data_type);
	TEST_ASSERT_EQUAL(cmp_mode, cfg.cmp_mode);
	TEST_ASSERT_EQUAL(model_value, cfg.model_value);
	TEST_ASSERT_EQUAL(lossy_par, cfg.round);


	/* wrong data type  test */
	for (data_type = 0; data_type <= DATA_TYPE_F_CAM_BACKGROUND; data_type++) {
		cfg = rdcu_cfg_create(data_type, CMP_MODE_DIFF_MULTI, MAX_MODEL_VALUE, CMP_LOSSLESS);
		if (data_type == DATA_TYPE_IMAGETTE ||
		    data_type == DATA_TYPE_IMAGETTE_ADAPTIVE ||
		    data_type == DATA_TYPE_SAT_IMAGETTE ||
		    data_type == DATA_TYPE_SAT_IMAGETTE_ADAPTIVE ||
		    data_type == DATA_TYPE_F_CAM_IMAGETTE ||
		    data_type == DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE) {
			TEST_ASSERT_EQUAL(data_type, cfg.data_type);
			TEST_ASSERT_EQUAL(CMP_MODE_DIFF_MULTI, cfg.cmp_mode);
			TEST_ASSERT_EQUAL(MAX_MODEL_VALUE, cfg.model_value);
			TEST_ASSERT_EQUAL(CMP_LOSSLESS, cfg.round);
		} else {
			TEST_ASSERT_EQUAL(DATA_TYPE_UNKNOWN, cfg.data_type);
		}
	}
}


/**
 * @test rdcu_cfg_buffers
 */

void test_rdcu_cfg_buffers_raw_diff(void)
{
	int error;
	struct cmp_cfg cfg;
	uint16_t data_to_compress[4] = {0x23, 0x42, 0xFF, 0x32};
	uint32_t data_samples = 4;
	uint32_t rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr;
	uint32_t rdcu_buffer_adr, rdcu_buffer_lenght;


	/* test a RAW mode buffer configuration */
	cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE_ADAPTIVE, CMP_MODE_RAW,
			      MAX_MODEL_VALUE, CMP_LOSSLESS);

	rdcu_model_adr = 0x0;
	rdcu_new_model_adr = 0x0;
	rdcu_data_adr = 0x0;
	rdcu_buffer_adr = 0x8;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, NULL,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL(data_to_compress, cfg.input_buf);
	TEST_ASSERT_EQUAL(data_samples, cfg.samples);
	TEST_ASSERT_EQUAL(NULL, cfg.model_buf);
	TEST_ASSERT_EQUAL(rdcu_data_adr, cfg.rdcu_data_adr);
	TEST_ASSERT_EQUAL(rdcu_model_adr, cfg.rdcu_model_adr);
	TEST_ASSERT_EQUAL(rdcu_new_model_adr, cfg.rdcu_new_model_adr);
	TEST_ASSERT_EQUAL(rdcu_buffer_adr, cfg.rdcu_buffer_adr);
	TEST_ASSERT_EQUAL(rdcu_buffer_lenght, cfg.buffer_length);

	/* set input buffer to NULL */
	error = rdcu_cfg_buffers(&cfg, NULL, data_samples, NULL,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_FALSE(error);

	/* error: destination buffer to small */
	rdcu_data_adr = 0x0;
	rdcu_buffer_adr = 0x8;
	rdcu_buffer_lenght = 3;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, NULL,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	/* error: data and compressed buffer overlap */
	rdcu_data_adr = 0x0;
	rdcu_buffer_adr = 0x4;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, NULL,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	/* get a diff config */
	cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE, CMP_MODE_DIFF_MULTI,
			      MAX_MODEL_VALUE, CMP_LOSSLESS);
	rdcu_data_adr = 0x4;
	rdcu_buffer_adr = 0x0;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, NULL,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	/* error: data or SRAM addresses out of allowed range */
	rdcu_data_adr = RDCU_SRAM_END & ~0x3UL;
	rdcu_buffer_adr = 0x8;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, NULL,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	rdcu_data_adr = 0x0;
	rdcu_buffer_adr = 0x8;
	rdcu_buffer_lenght = RDCU_SRAM_SIZE;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, NULL,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	rdcu_data_adr = 0xFFFFFFFC;
	rdcu_buffer_adr = 0x8;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, NULL,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	rdcu_data_adr = 0x0;
	rdcu_buffer_adr = 0x8;
	rdcu_buffer_lenght = UINT32_MAX;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, NULL,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	/* error: data and buffer addresses are not 4 byte aligned */
	rdcu_data_adr = 0x2;
	rdcu_buffer_adr = 0x10;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, NULL,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	rdcu_data_adr = 0x0;
	rdcu_buffer_adr = 0x9;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, NULL,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	/* error: cfg = NULL */
	error = rdcu_cfg_buffers(NULL, data_to_compress, data_samples, NULL,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(-1, error);
}


/**
 * @test rdcu_cfg_buffers
 */

void test_rdcu_cfg_buffers_model(void)
{
	int error;
	struct cmp_cfg cfg;
	uint16_t data_to_compress[4] = {0x23, 0x42, 0xFF, 0x32};
	uint16_t model_of_data[4] = {0xFF, 0x12, 0x34, 0xAB};
	uint32_t data_samples = 4;
	uint32_t rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr;
	uint32_t rdcu_buffer_adr, rdcu_buffer_lenght;


	/* test a RAW mode buffer configuration */
	cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE, CMP_MODE_MODEL_MULTI,
			      MAX_MODEL_VALUE, CMP_LOSSLESS);

	rdcu_data_adr = 0x0;
	rdcu_model_adr = 0x8;
	rdcu_new_model_adr = 0x10;
	rdcu_buffer_adr = 0x18;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, model_of_data,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL(data_to_compress, cfg.input_buf);
	TEST_ASSERT_EQUAL(data_samples, cfg.samples);
	TEST_ASSERT_EQUAL(model_of_data, cfg.model_buf);
	TEST_ASSERT_EQUAL(rdcu_data_adr, cfg.rdcu_data_adr);
	TEST_ASSERT_EQUAL(rdcu_model_adr, cfg.rdcu_model_adr);
	TEST_ASSERT_EQUAL(rdcu_new_model_adr, cfg.rdcu_new_model_adr);
	TEST_ASSERT_EQUAL(rdcu_buffer_adr, cfg.rdcu_buffer_adr);
	TEST_ASSERT_EQUAL(rdcu_buffer_lenght, cfg.buffer_length);

	/* data and model buffers are NULL */
	rdcu_new_model_adr = rdcu_model_adr;
	error = rdcu_cfg_buffers(&cfg, NULL, data_samples, NULL,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_FALSE(error);

	/* error: data and model buffer are the same */
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, data_to_compress,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	/* error: model address not 4 byte aligned */
	rdcu_data_adr = 0x0;
	rdcu_model_adr = 0xA; /* not 4 byte aligned */
	rdcu_new_model_adr = rdcu_model_adr;
	rdcu_buffer_adr = 0x14;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, model_of_data,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	/* error: model address not in SRAM range */
	rdcu_data_adr = 0x0;
	rdcu_model_adr = 0xFFFFFFFC; /* not in SRAM range */
	rdcu_new_model_adr = rdcu_model_adr;
	rdcu_buffer_adr = 0x10;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, model_of_data,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	/* error: data and model rdcu buffers overlaps */
	rdcu_data_adr = 0x0;
	rdcu_model_adr = 0x4; /* overlaps with data buffer */
	rdcu_new_model_adr = rdcu_model_adr;
	rdcu_buffer_adr = 0x10;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, model_of_data,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	/* error: compressed buffer and model rdcu buffers overlaps */
	rdcu_data_adr = 0x0;
	rdcu_model_adr = 0xC; /* overlaps with compressed data buffer */
	rdcu_new_model_adr = rdcu_model_adr;
	rdcu_buffer_adr = 0x10;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, model_of_data,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	/** test with the updated_model buffer **/

	/* error: updated_model address not 4 byte aligned */
	rdcu_data_adr = 0x0;
	rdcu_model_adr = 0x8;
	rdcu_new_model_adr = 0x11; /* not 4 byte aligned */
	rdcu_buffer_adr = 0x1C;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, model_of_data,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	/* error: updated_model address not in SRAM range */
	rdcu_data_adr = 0x0;
	rdcu_model_adr = 0x8;
	rdcu_new_model_adr = 0xFFFFFFFC; /* not in SRAM range */
	rdcu_buffer_adr = 0x18;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, model_of_data,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	/* error: data and updated model rdcu buffers overlaps */
	rdcu_data_adr = 0x8;
	rdcu_model_adr = 0x0;
	rdcu_new_model_adr = 0xC; /* overlaps with data buffer */
	rdcu_buffer_adr = 0x18;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, model_of_data,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	/* error: compressed buffer and updated_model rdcu buffers overlaps */
	rdcu_data_adr = 0x0;
	rdcu_model_adr = 0x8;
	rdcu_new_model_adr = 0x14; /* overlaps with compressed data buffer */
	rdcu_buffer_adr = 0x18;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, model_of_data,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

	/* error: model and updated_model rdcu buffers overlaps */
	rdcu_data_adr = 0x0;
	rdcu_model_adr = 0x8;
	rdcu_new_model_adr = 0xC; /* overlaps with model buffer */
	rdcu_buffer_adr = 0x18;
	rdcu_buffer_lenght = 4;
	error = rdcu_cfg_buffers(&cfg, data_to_compress, data_samples, model_of_data,
				 rdcu_data_adr, rdcu_model_adr, rdcu_new_model_adr,
				 rdcu_buffer_adr, rdcu_buffer_lenght);
	TEST_ASSERT_EQUAL(1, error);

}


/**
 * @test rdcu_cfg_imagette
 */

void test_rdcu_cfg_imagette(void)
{
	int error;
	struct cmp_cfg cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE, CMP_MODE_RAW,
					     10, CMP_LOSSLESS);
	uint32_t golomb_par, spillover_par, ap1_golomb_par, ap1_spillover_par,
		 ap2_golomb_par, ap2_spillover_par;

	golomb_par = MIN_IMA_GOLOMB_PAR;
	spillover_par = MIN_IMA_SPILL;
	ap1_golomb_par = MIN_IMA_GOLOMB_PAR;
	ap1_spillover_par = MIN_IMA_SPILL;
	ap2_golomb_par = MIN_IMA_GOLOMB_PAR;
	ap2_spillover_par = MIN_IMA_SPILL;

	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL(golomb_par, cfg.golomb_par);
	TEST_ASSERT_EQUAL(spillover_par, cfg.spill);
	TEST_ASSERT_EQUAL(ap1_golomb_par, cfg.ap1_golomb_par);
	TEST_ASSERT_EQUAL(ap1_spillover_par, cfg.ap1_spill);
	TEST_ASSERT_EQUAL(ap2_golomb_par, cfg.ap2_golomb_par);
	TEST_ASSERT_EQUAL(ap2_spillover_par, cfg.ap2_spill);


	/* wrong golomb_par */
	golomb_par = MIN_IMA_GOLOMB_PAR - 1;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_TRUE(error);

	golomb_par = MAX_IMA_GOLOMB_PAR + 1;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_TRUE(error);

	/* this should work */
	golomb_par = MAX_IMA_GOLOMB_PAR;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL(golomb_par, cfg.golomb_par);
	TEST_ASSERT_EQUAL(spillover_par, cfg.spill);
	TEST_ASSERT_EQUAL(ap1_golomb_par, cfg.ap1_golomb_par);
	TEST_ASSERT_EQUAL(ap1_spillover_par, cfg.ap1_spill);
	TEST_ASSERT_EQUAL(ap2_golomb_par, cfg.ap2_golomb_par);
	TEST_ASSERT_EQUAL(ap2_spillover_par, cfg.ap2_spill);


	/* wrong ap1_golomb_par */
	ap1_golomb_par = MIN_IMA_GOLOMB_PAR - 1;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_TRUE(error);

	ap1_golomb_par = MAX_IMA_GOLOMB_PAR + 1;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_TRUE(error);

	/* this should work */
	ap1_golomb_par = MAX_IMA_GOLOMB_PAR;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL(golomb_par, cfg.golomb_par);
	TEST_ASSERT_EQUAL(spillover_par, cfg.spill);
	TEST_ASSERT_EQUAL(ap1_golomb_par, cfg.ap1_golomb_par);
	TEST_ASSERT_EQUAL(ap1_spillover_par, cfg.ap1_spill);
	TEST_ASSERT_EQUAL(ap2_golomb_par, cfg.ap2_golomb_par);
	TEST_ASSERT_EQUAL(ap2_spillover_par, cfg.ap2_spill);


	/* wrong ap2_golomb_par */
	cfg.cmp_mode = CMP_MODE_DIFF_ZERO;
	ap2_golomb_par = MIN_IMA_GOLOMB_PAR - 1;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_TRUE(error);

	ap2_golomb_par = MAX_IMA_GOLOMB_PAR + 1;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_TRUE(error);

	/* this should work */
	ap2_golomb_par = MAX_IMA_GOLOMB_PAR;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL(golomb_par, cfg.golomb_par);
	TEST_ASSERT_EQUAL(spillover_par, cfg.spill);
	TEST_ASSERT_EQUAL(ap1_golomb_par, cfg.ap1_golomb_par);
	TEST_ASSERT_EQUAL(ap1_spillover_par, cfg.ap1_spill);
	TEST_ASSERT_EQUAL(ap2_golomb_par, cfg.ap2_golomb_par);
	TEST_ASSERT_EQUAL(ap2_spillover_par, cfg.ap2_spill);


	/* wrong spillover_par */
	golomb_par = MIN_IMA_GOLOMB_PAR;
	spillover_par = cmp_ima_max_spill(golomb_par)+1;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	golomb_par = MAX_IMA_GOLOMB_PAR;
	spillover_par = cmp_ima_max_spill(golomb_par)+1;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);

	golomb_par = MIN_IMA_GOLOMB_PAR;
	spillover_par = MIN_IMA_SPILL - 1;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_EQUAL(1, error);

	/* this should work */
	golomb_par = MAX_IMA_GOLOMB_PAR;
	spillover_par = cmp_ima_max_spill(golomb_par);
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL(golomb_par, cfg.golomb_par);
	TEST_ASSERT_EQUAL(spillover_par, cfg.spill);
	TEST_ASSERT_EQUAL(ap1_golomb_par, cfg.ap1_golomb_par);
	TEST_ASSERT_EQUAL(ap1_spillover_par, cfg.ap1_spill);
	TEST_ASSERT_EQUAL(ap2_golomb_par, cfg.ap2_golomb_par);
	TEST_ASSERT_EQUAL(ap2_spillover_par, cfg.ap2_spill);


	/* wrong ap1_spillover_par */
	ap1_golomb_par = MIN_IMA_GOLOMB_PAR;
	ap1_spillover_par = cmp_ima_max_spill(golomb_par)+1;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	ap1_golomb_par = MAX_IMA_GOLOMB_PAR;
	ap1_spillover_par = cmp_ima_max_spill(golomb_par)+1;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);

	ap1_golomb_par = MIN_IMA_GOLOMB_PAR;
	ap1_spillover_par = MIN_IMA_SPILL - 1;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_EQUAL(1, error);

	/* this should work */
	ap1_golomb_par = MAX_IMA_GOLOMB_PAR;
	ap1_spillover_par = cmp_ima_max_spill(golomb_par);
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL(golomb_par, cfg.golomb_par);
	TEST_ASSERT_EQUAL(spillover_par, cfg.spill);
	TEST_ASSERT_EQUAL(ap1_golomb_par, cfg.ap1_golomb_par);
	TEST_ASSERT_EQUAL(ap1_spillover_par, cfg.ap1_spill);
	TEST_ASSERT_EQUAL(ap2_golomb_par, cfg.ap2_golomb_par);
	TEST_ASSERT_EQUAL(ap2_spillover_par, cfg.ap2_spill);


	/* wrong ap2_spillover_par */
	ap2_golomb_par = MIN_IMA_GOLOMB_PAR;
	ap2_spillover_par = cmp_ima_max_spill(golomb_par)+1;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	ap2_golomb_par = MAX_IMA_GOLOMB_PAR;
	ap2_spillover_par = cmp_ima_max_spill(golomb_par)+1;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);

	ap2_golomb_par = MIN_IMA_GOLOMB_PAR;
	ap2_spillover_par = MIN_IMA_SPILL - 1;
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_EQUAL(1, error);

	/* this should work */
	ap2_golomb_par = MAX_IMA_GOLOMB_PAR;
	ap2_spillover_par = cmp_ima_max_spill(golomb_par);
	error = rdcu_cfg_imagette(&cfg, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par,
				  ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_FALSE(error);
	TEST_ASSERT_EQUAL(golomb_par, cfg.golomb_par);
	TEST_ASSERT_EQUAL(spillover_par, cfg.spill);
	TEST_ASSERT_EQUAL(ap1_golomb_par, cfg.ap1_golomb_par);
	TEST_ASSERT_EQUAL(ap1_spillover_par, cfg.ap1_spill);
	TEST_ASSERT_EQUAL(ap2_golomb_par, cfg.ap2_golomb_par);
	TEST_ASSERT_EQUAL(ap2_spillover_par, cfg.ap2_spill); /* cfg = NULL test */ error = rdcu_cfg_imagette(NULL, golomb_par, spillover_par,
				  ap1_golomb_par, ap1_spillover_par, ap2_golomb_par, ap2_spillover_par);
	TEST_ASSERT_EQUAL(-1, error);
}


/**
 * @test rdcu_cmp_cfg_is_invalid
 */

void test_rdcu_cmp_cfg_is_invalid(void)
{
	int error;
	struct cmp_cfg cfg;
	uint16_t data[1] = {1};
	uint16_t model[1] = {2};

	/* diff test */
	cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE, CMP_DEF_IMA_DIFF_CMP_MODE,
			      CMP_DEF_IMA_DIFF_MODEL_VALUE, CMP_DEF_IMA_DIFF_LOSSY_PAR);
	error = rdcu_cfg_buffers(&cfg, data, 1, NULL, CMP_DEF_IMA_DIFF_RDCU_DATA_ADR,
				 CMP_DEF_IMA_DIFF_RDCU_MODEL_ADR, CMP_DEF_IMA_DIFF_RDCU_UP_MODEL_ADR,
				 CMP_DEF_IMA_DIFF_RDCU_BUFFER_ADR, 1);
	TEST_ASSERT_FALSE(error);
	error = rdcu_cfg_imagette(&cfg,
				  CMP_DEF_IMA_DIFF_GOLOMB_PAR, CMP_DEF_IMA_DIFF_SPILL_PAR,
				  CMP_DEF_IMA_DIFF_AP1_GOLOMB_PAR, CMP_DEF_IMA_DIFF_AP1_SPILL_PAR,
				  CMP_DEF_IMA_DIFF_AP2_GOLOMB_PAR, CMP_DEF_IMA_DIFF_AP2_SPILL_PAR);
	TEST_ASSERT_FALSE(error);
	error = rdcu_cmp_cfg_is_invalid(&cfg);
	TEST_ASSERT_FALSE(error);

	/* model test */
	cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE, CMP_DEF_IMA_MODEL_CMP_MODE,
			      CMP_DEF_IMA_MODEL_MODEL_VALUE, CMP_DEF_IMA_MODEL_LOSSY_PAR);
	error = rdcu_cfg_buffers(&cfg, data, 1, model, CMP_DEF_IMA_MODEL_RDCU_DATA_ADR,
				 CMP_DEF_IMA_MODEL_RDCU_MODEL_ADR, CMP_DEF_IMA_MODEL_RDCU_UP_MODEL_ADR,
				 CMP_DEF_IMA_MODEL_RDCU_BUFFER_ADR, 1);
	TEST_ASSERT_FALSE(error);
	error = rdcu_cfg_imagette(&cfg,
				  CMP_DEF_IMA_MODEL_GOLOMB_PAR, CMP_DEF_IMA_MODEL_SPILL_PAR,
				  CMP_DEF_IMA_MODEL_AP1_GOLOMB_PAR, CMP_DEF_IMA_MODEL_AP1_SPILL_PAR,
				  CMP_DEF_IMA_MODEL_AP2_GOLOMB_PAR, CMP_DEF_IMA_MODEL_AP2_SPILL_PAR);
	TEST_ASSERT_FALSE(error);
	error = rdcu_cmp_cfg_is_invalid(&cfg);
	TEST_ASSERT_FALSE(error);

	/* test warnings */
	cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE, CMP_DEF_IMA_MODEL_CMP_MODE,
			      CMP_DEF_IMA_MODEL_MODEL_VALUE, CMP_DEF_IMA_MODEL_LOSSY_PAR);
	error = rdcu_cfg_buffers(&cfg, NULL, 0, NULL, CMP_DEF_IMA_MODEL_RDCU_DATA_ADR,
				 CMP_DEF_IMA_MODEL_RDCU_MODEL_ADR, CMP_DEF_IMA_MODEL_RDCU_UP_MODEL_ADR,
				 CMP_DEF_IMA_MODEL_RDCU_BUFFER_ADR, 1);
	error = rdcu_cfg_imagette(&cfg,
				  CMP_DEF_IMA_MODEL_GOLOMB_PAR, CMP_DEF_IMA_MODEL_SPILL_PAR,
				  CMP_DEF_IMA_MODEL_AP1_GOLOMB_PAR, CMP_DEF_IMA_MODEL_AP1_SPILL_PAR,
				  CMP_DEF_IMA_MODEL_AP2_GOLOMB_PAR, CMP_DEF_IMA_MODEL_AP2_SPILL_PAR);
	cfg.icu_new_model_buf = data;
	cfg.icu_output_buf = (void *)model;
	error = rdcu_cmp_cfg_is_invalid(&cfg);
	TEST_ASSERT_FALSE(error);

	/* error: cfg is NULL */
	error = rdcu_cmp_cfg_is_invalid(NULL);
	TEST_ASSERT_TRUE(error);

	/* error: buffer length = 0 */
	cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE, CMP_DEF_IMA_MODEL_CMP_MODE,
			      CMP_DEF_IMA_MODEL_MODEL_VALUE, CMP_DEF_IMA_MODEL_LOSSY_PAR);
	error = rdcu_cfg_buffers(&cfg, data, 1, model, CMP_DEF_IMA_MODEL_RDCU_DATA_ADR,
				 CMP_DEF_IMA_MODEL_RDCU_MODEL_ADR, CMP_DEF_IMA_MODEL_RDCU_UP_MODEL_ADR,
				 CMP_DEF_IMA_MODEL_RDCU_BUFFER_ADR, 0);
	error = rdcu_cfg_imagette(&cfg,
				  CMP_DEF_IMA_MODEL_GOLOMB_PAR, CMP_DEF_IMA_MODEL_SPILL_PAR,
				  CMP_DEF_IMA_MODEL_AP1_GOLOMB_PAR, CMP_DEF_IMA_MODEL_AP1_SPILL_PAR,
				  CMP_DEF_IMA_MODEL_AP2_GOLOMB_PAR, CMP_DEF_IMA_MODEL_AP2_SPILL_PAR);
	error = rdcu_cmp_cfg_is_invalid(&cfg);
	TEST_ASSERT_TRUE(error);

	/* error: wrong gen par */
	cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE, CMP_DEF_IMA_DIFF_CMP_MODE,
			      MAX_MODEL_VALUE+1, CMP_DEF_IMA_DIFF_LOSSY_PAR);
	cfg.data_type = DATA_TYPE_IMAGETTE;
	error = rdcu_cfg_buffers(&cfg, data, 1, NULL, CMP_DEF_IMA_DIFF_RDCU_DATA_ADR,
				 CMP_DEF_IMA_DIFF_RDCU_MODEL_ADR, CMP_DEF_IMA_DIFF_RDCU_UP_MODEL_ADR,
				 CMP_DEF_IMA_DIFF_RDCU_BUFFER_ADR, 1);
	error = rdcu_cfg_imagette(&cfg,
				  CMP_DEF_IMA_DIFF_GOLOMB_PAR, CMP_DEF_IMA_DIFF_SPILL_PAR,
				  CMP_DEF_IMA_DIFF_AP1_GOLOMB_PAR, CMP_DEF_IMA_DIFF_AP1_SPILL_PAR,
				  CMP_DEF_IMA_DIFF_AP2_GOLOMB_PAR, CMP_DEF_IMA_DIFF_AP2_SPILL_PAR);
	error = rdcu_cmp_cfg_is_invalid(&cfg);
	TEST_ASSERT_TRUE(error);

	/* error: wrong buffers config */
	cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE, CMP_DEF_IMA_DIFF_CMP_MODE,
			      CMP_DEF_IMA_DIFF_MODEL_VALUE, CMP_DEF_IMA_DIFF_LOSSY_PAR);
	error = rdcu_cfg_buffers(&cfg, data, 1, NULL, RDCU_SRAM_END+4,
				 CMP_DEF_IMA_DIFF_RDCU_MODEL_ADR, CMP_DEF_IMA_DIFF_RDCU_UP_MODEL_ADR,
				 CMP_DEF_IMA_DIFF_RDCU_BUFFER_ADR, 1);
	error = rdcu_cfg_imagette(&cfg,
				  CMP_DEF_IMA_DIFF_GOLOMB_PAR, CMP_DEF_IMA_DIFF_SPILL_PAR,
				  CMP_DEF_IMA_DIFF_AP1_GOLOMB_PAR, CMP_DEF_IMA_DIFF_AP1_SPILL_PAR,
				  CMP_DEF_IMA_DIFF_AP2_GOLOMB_PAR, CMP_DEF_IMA_DIFF_AP2_SPILL_PAR);
	error = rdcu_cmp_cfg_is_invalid(&cfg);
	TEST_ASSERT_TRUE(error);

	/* error: wrong compression parameter test */
	cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE, CMP_DEF_IMA_DIFF_CMP_MODE,
			      CMP_DEF_IMA_DIFF_MODEL_VALUE, CMP_DEF_IMA_DIFF_LOSSY_PAR);
	error = rdcu_cfg_buffers(&cfg, data, 1, NULL, CMP_DEF_IMA_DIFF_RDCU_DATA_ADR,
				 CMP_DEF_IMA_DIFF_RDCU_MODEL_ADR, CMP_DEF_IMA_DIFF_RDCU_UP_MODEL_ADR,
				 CMP_DEF_IMA_DIFF_RDCU_BUFFER_ADR, 1);
	error = rdcu_cfg_imagette(&cfg,
				  MAX_IMA_GOLOMB_PAR+1, CMP_DEF_IMA_DIFF_SPILL_PAR,
				  CMP_DEF_IMA_DIFF_AP1_GOLOMB_PAR, CMP_DEF_IMA_DIFF_AP1_SPILL_PAR,
				  CMP_DEF_IMA_DIFF_AP2_GOLOMB_PAR, CMP_DEF_IMA_DIFF_AP2_SPILL_PAR);
	error = rdcu_cmp_cfg_is_invalid(&cfg);
	TEST_ASSERT_TRUE(error);
}

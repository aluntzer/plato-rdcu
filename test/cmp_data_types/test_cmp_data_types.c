/**
 * @file test_cmp_entity.h
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
 * @brief compression data types tests
 */


#include <stdint.h>

#include <unity.h>
#include <cmp_data_types.h>


/**
 * @test cmp_set_max_us
 */

void test_cmp_set_max_used_bits(void)
{
	struct cmp_max_used_bits set_max_used_bits = cmp_get_max_used_bits();
	cmp_set_max_used_bits(&set_max_used_bits);
	cmp_set_max_used_bits(NULL);
}


/**
 * @test cmp_get_max_used_bits
 */

void test_cmp_get_max_used_bits(void)
{
	struct cmp_max_used_bits max_used_bits = cmp_get_max_used_bits();

	TEST_ASSERT_EQUAL_INT(max_used_bits.nc_imagette, MAX_USED_NC_IMAGETTE_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.saturated_imagette, MAX_USED_SATURATED_IMAGETTE_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.fc_imagette, MAX_USED_FC_IMAGETTE_BITS);

	TEST_ASSERT_EQUAL_INT(max_used_bits.f_fx, MAX_USED_F_FX_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.f_efx, MAX_USED_F_EFX_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.f_ncob, MAX_USED_F_NCOB_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.f_ecob, MAX_USED_F_ECOB_BITS);

	TEST_ASSERT_EQUAL_INT(max_used_bits.s_exp_flags, MAX_USED_S_FX_EXPOSURE_FLAGS_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.s_fx, MAX_USED_S_FX_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.s_efx, MAX_USED_S_EFX_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.s_ncob, MAX_USED_S_NCOB_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.s_ecob, MAX_USED_S_ECOB_BITS);

	TEST_ASSERT_EQUAL_INT(max_used_bits.l_fx_variance, MAX_USED_L_FX_VARIANCE_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.l_efx, MAX_USED_L_EFX_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.l_ncob, MAX_USED_L_NCOB_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.l_ecob, MAX_USED_L_ECOB_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.l_cob_variance, MAX_USED_L_COB_VARIANCE_BITS);

	TEST_ASSERT_EQUAL_INT(max_used_bits.nc_offset_mean, MAX_USED_NC_OFFSET_MEAN_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.nc_offset_variance, MAX_USED_NC_OFFSET_VARIANCE_BITS);

	TEST_ASSERT_EQUAL_INT(max_used_bits.nc_background_mean, MAX_USED_NC_BACKGROUND_MEAN_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.nc_background_variance, MAX_USED_NC_BACKGROUND_VARIANCE_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.nc_background_outlier_pixels, MAX_USED_NC_BACKGROUND_OUTLIER_PIXELS_BITS);

	TEST_ASSERT_EQUAL_INT(max_used_bits.smearing_mean, MAX_USED_SMEARING_MEAN_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.smearing_variance_mean, MAX_USED_SMEARING_VARIANCE_MEAN_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.smearing_outlier_pixels, MAX_USED_SMEARING_OUTLIER_PIXELS_BITS);

	TEST_ASSERT_EQUAL_INT(max_used_bits.fc_offset_mean, MAX_USED_FC_OFFSET_MEAN_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.fc_offset_variance, MAX_USED_FC_OFFSET_VARIANCE_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.fc_offset_pixel_in_error, MAX_USED_FC_OFFSET_PIXEL_IN_ERROR_BITS);

	TEST_ASSERT_EQUAL_INT(max_used_bits.fc_background_mean, MAX_USED_FC_BACKGROUND_MEAN_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.fc_background_variance, MAX_USED_FC_BACKGROUND_VARIANCE_BITS);
	TEST_ASSERT_EQUAL_INT(max_used_bits.fc_background_outlier_pixels, MAX_USED_FC_BACKGROUND_OUTLIER_PIXELS_BITS);
}


/**
 * @test size_of_a_sample
 */

void test_size_of_a_sample(void)
{
	size_t size;
	/* TODO: implied DATA_TYPE_F_CAM_OFFSET and DATA_TYPE_F_CAM_BACKGROUND DATA_TYPE_F_CAM_BACKGROUND*/
	size = size_of_a_sample(DATA_TYPE_F_CAM_OFFSET);
	TEST_ASSERT_EQUAL(0, size);
	size = size_of_a_sample(DATA_TYPE_F_CAM_BACKGROUND);
	TEST_ASSERT_EQUAL(0, size);

	/* test error cases */
	size = size_of_a_sample(DATA_TYPE_UNKNOWN);
	TEST_ASSERT_EQUAL(0, size);
	size = size_of_a_sample(DATA_TYPE_F_CAM_BACKGROUND +1);
	TEST_ASSERT_EQUAL(0, size);
}


/**
 * @test cmp_cal_size_of_data
 */

void test_cmp_cal_size_of_data(void)
{
	uint32_t s;

	s = cmp_cal_size_of_data(1, DATA_TYPE_IMAGETTE);
	TEST_ASSERT_EQUAL_UINT(sizeof(uint16_t), s);

	s = cmp_cal_size_of_data(32, DATA_TYPE_IMAGETTE);
	TEST_ASSERT_EQUAL_UINT(32 * sizeof(uint16_t), s);

	s = cmp_cal_size_of_data(1, DATA_TYPE_F_FX);
	TEST_ASSERT_EQUAL_UINT(sizeof(struct f_fx)+MULTI_ENTRY_HDR_SIZE, s);

	s = cmp_cal_size_of_data(4, DATA_TYPE_F_FX);
	TEST_ASSERT_EQUAL_UINT(4*sizeof(struct f_fx)+MULTI_ENTRY_HDR_SIZE, s);

	/* overflow tests */
	s = cmp_cal_size_of_data(0x1999999A, DATA_TYPE_BACKGROUND);
	TEST_ASSERT_EQUAL_UINT(0, s);
	s = cmp_cal_size_of_data(0x19999999, DATA_TYPE_BACKGROUND);
	TEST_ASSERT_EQUAL_UINT(0, s);
	s = cmp_cal_size_of_data(UINT_MAX, DATA_TYPE_L_FX_EFX_NCOB_ECOB);
	TEST_ASSERT_EQUAL_UINT(0, s);
}


/**
 * @test cmp_input_size_to_samples
 */

void test_cmp_input_size_to_samples(void)
{
	enum cmp_data_type data_type;
	uint32_t size, samples;
	int32_t samples_get;

	data_type = DATA_TYPE_IMAGETTE;
	samples = 42;
	size = cmp_cal_size_of_data(samples, data_type);
	samples_get = cmp_input_size_to_samples(size, data_type);
	TEST_ASSERT_EQUAL(samples, samples_get);

	data_type = DATA_TYPE_IMAGETTE;
	samples = 0;
	size = cmp_cal_size_of_data(samples, data_type);
	samples_get = cmp_input_size_to_samples(size, data_type);
	TEST_ASSERT_EQUAL(samples, samples_get);

	data_type = DATA_TYPE_S_FX_NCOB;
	samples = 42;
	size = cmp_cal_size_of_data(samples, data_type);
	samples_get = cmp_input_size_to_samples(size, data_type);
	TEST_ASSERT_EQUAL(samples, samples_get);

	data_type = DATA_TYPE_S_FX_NCOB;
	samples = 0;
	size = cmp_cal_size_of_data(samples, data_type);
	samples_get = cmp_input_size_to_samples(size, data_type);
	TEST_ASSERT_EQUAL(samples, samples_get);

	/* error cases */
	data_type = DATA_TYPE_S_FX_NCOB;
	size = MULTI_ENTRY_HDR_SIZE - 1;
	samples_get = cmp_input_size_to_samples(size, data_type);
	data_type = DATA_TYPE_S_FX_NCOB;

	data_type = DATA_TYPE_S_FX_NCOB;
	size = MULTI_ENTRY_HDR_SIZE + 4*sizeof(struct s_fx_ncob) - 1;
	samples_get = cmp_input_size_to_samples(size, data_type);
	TEST_ASSERT_EQUAL(-1, samples_get);
	TEST_ASSERT_EQUAL(-1, samples_get);
}


/**
 * @test cmp_input_big_to_cpu_endianness
 */

void test_cmp_input_big_to_cpu_endianness(void)
{
	int error;
	void *data;
	uint8_t data_err[3] = {0x01, 0xFF, 0x42};
	uint32_t data_size_byte;
	enum cmp_data_type data_type;

	/* data = NULL test */
	data = NULL;
	data_size_byte = 0;
	data_type = DATA_TYPE_IMAGETTE;
	error = cmp_input_big_to_cpu_endianness(data, data_size_byte, data_type);
	TEST_ASSERT_EQUAL(0, error);

	/* error cases */
	data = data_err;
	data_size_byte = 3;
	data_type = DATA_TYPE_IMAGETTE;
	error = cmp_input_big_to_cpu_endianness(data, data_size_byte, data_type);
	TEST_ASSERT_EQUAL(-1, error);

	data = data_err;
	data_size_byte = 0;
	data_type = DATA_TYPE_UNKNOWN;
	error = cmp_input_big_to_cpu_endianness(data, data_size_byte, data_type);
	TEST_ASSERT_EQUAL(-1, error);
}

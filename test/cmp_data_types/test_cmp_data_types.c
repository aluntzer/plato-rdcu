/**
 * @file test_cmp_data_types.c
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
 * @brief compression data types tests
 */


#include <stdint.h>

#include <unity.h>
#include <cmp_data_types.h>


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
	size = size_of_a_sample(DATA_TYPE_F_CAM_BACKGROUND+1);
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
	TEST_ASSERT_EQUAL(-1, samples_get);
	data_type = DATA_TYPE_S_FX_NCOB;

	size = MULTI_ENTRY_HDR_SIZE + 4*sizeof(struct s_fx_ncob) - 1;
	samples_get = cmp_input_size_to_samples(size, data_type);
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

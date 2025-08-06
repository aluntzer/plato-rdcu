/**
 * @file   test_cmp_entity.c
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
 * @brief compression entity tests
 */


#include <stdlib.h>
#include <string.h>

#ifndef ICU_ASW
#  if defined __has_include
#    if __has_include(<time.h>)
#      include <time.h>
#      define HAS_TIME_H 1
#    endif
#  endif
#endif

#include <unity.h>

#include <cmp_entity.h>
#include <cmp_data_types.h>
#include <decmp.h>


/**
 * @test cmp_ent_cal_hdr_size
 */

void test_cmp_ent_cal_hdr_size(void)
{
	uint32_t hdr_size;
	enum cmp_data_type data_type;
	int raw_mode_flag;

	/* raw_mode test */
	raw_mode_flag = 1;
	for (data_type = DATA_TYPE_IMAGETTE; data_type <= DATA_TYPE_F_CAM_BACKGROUND; data_type++) {
		hdr_size = cmp_ent_cal_hdr_size(data_type, raw_mode_flag);
		TEST_ASSERT_EQUAL_INT(GENERIC_HEADER_SIZE, hdr_size);

	}
	/* non raw_mode test */
	raw_mode_flag = 0;
	for (data_type = DATA_TYPE_IMAGETTE; data_type <= DATA_TYPE_F_CAM_BACKGROUND; data_type++) {
		hdr_size = cmp_ent_cal_hdr_size(data_type, raw_mode_flag);
		if (cmp_imagette_data_type_is_used(data_type)) {
			if (cmp_ap_imagette_data_type_is_used(data_type))
				TEST_ASSERT_EQUAL_INT(IMAGETTE_ADAPTIVE_HEADER_SIZE, hdr_size);
			else
				TEST_ASSERT_EQUAL_INT(IMAGETTE_HEADER_SIZE, hdr_size);
		} else {
			TEST_ASSERT_EQUAL_INT(NON_IMAGETTE_HEADER_SIZE, hdr_size);
		}

	}

	/* error case raw_mode*/
	raw_mode_flag = 1;
	data_type = DATA_TYPE_UNKNOWN;
	hdr_size = cmp_ent_cal_hdr_size(data_type, raw_mode_flag);
	TEST_ASSERT_EQUAL_INT(0, hdr_size);

	data_type = (enum cmp_data_type)~0;
	hdr_size = cmp_ent_cal_hdr_size(data_type, raw_mode_flag);
	TEST_ASSERT_EQUAL_INT(0, hdr_size);

	/* error case non raw_mode*/
	raw_mode_flag = 0;
	data_type = DATA_TYPE_UNKNOWN;
	hdr_size = cmp_ent_cal_hdr_size(data_type, raw_mode_flag);
	TEST_ASSERT_EQUAL_INT(0, hdr_size);

	data_type = (enum cmp_data_type)~0;
	hdr_size = cmp_ent_cal_hdr_size(data_type, raw_mode_flag);
	TEST_ASSERT_EQUAL_INT(0, hdr_size);
}


/**
 * @test cmp_ent_set_version_id
 * @test cmp_ent_get_version_id
 */

void test_ent_version_id(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t version_id;
	uint32_t version_id_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	version_id = 0x12345678;
	error = cmp_ent_set_version_id(&ent, version_id);
	TEST_ASSERT_FALSE(error);

	version_id_read = cmp_ent_get_version_id(&ent);
	TEST_ASSERT_EQUAL_UINT32(version_id, version_id_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[0]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[1]);
	TEST_ASSERT_EQUAL_HEX(0x56, entity_p[2]);
	TEST_ASSERT_EQUAL_HEX(0x78, entity_p[3]);

	/* error cases */
	error = cmp_ent_set_version_id(NULL, version_id);
	TEST_ASSERT_TRUE(error);
	version_id_read = cmp_ent_get_version_id(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, version_id_read);
}


/**
 * @test cmp_ent_set_size
 * @test cmp_ent_get_size
 */

void test_ent_size(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t size;
	uint32_t size_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	size = 0x123456;
	error = cmp_ent_set_size(&ent, size);
	TEST_ASSERT_FALSE(error);

	size_read = cmp_ent_get_size(&ent);
	TEST_ASSERT_EQUAL_UINT32(size, size_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[4]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[5]);
	TEST_ASSERT_EQUAL_HEX(0x56, entity_p[6]);

	/* error cases */
	size = 0x1234567;
	error = cmp_ent_set_size(&ent, size);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_size(NULL, size);
	TEST_ASSERT_TRUE(error);
	size_read = cmp_ent_get_size(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, size_read);
}


/**
 * @test cmp_ent_set_original_size
 * @test cmp_ent_get_original_size
 */

void test_ent_original_size(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t original_size;
	uint32_t original_size_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	original_size = 0x123456;
	error = cmp_ent_set_original_size(&ent, original_size);
	TEST_ASSERT_FALSE(error);

	original_size_read = cmp_ent_get_original_size(&ent);
	TEST_ASSERT_EQUAL_UINT32(original_size, original_size_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[7]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[8]);
	TEST_ASSERT_EQUAL_HEX(0x56, entity_p[9]);

	/* error cases */
	original_size = 0x1234567;
	error = cmp_ent_set_original_size(&ent, original_size);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_original_size(NULL, original_size);
	TEST_ASSERT_TRUE(error);
	original_size_read = cmp_ent_get_original_size(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, original_size_read);
}


/**
 * @test cmp_ent_set_start_timestamp
 * @test cmp_ent_get_start_timestamp
 */

void test_ent_start_timestamp(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint64_t start_timestamp;
	uint64_t start_timestamp_read;
	uint32_t coarse_start_timestamp_read;
	uint16_t fine_start_timestamp_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	start_timestamp = 0x123456789ABCULL;
	error = cmp_ent_set_start_timestamp(&ent, start_timestamp);
	TEST_ASSERT_FALSE(error);

	start_timestamp_read = cmp_ent_get_start_timestamp(&ent);
	TEST_ASSERT_EQUAL(start_timestamp, start_timestamp_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[10]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[11]);
	TEST_ASSERT_EQUAL_HEX(0x56, entity_p[12]);
	TEST_ASSERT_EQUAL_HEX(0x78, entity_p[13]);
	TEST_ASSERT_EQUAL_HEX(0x9A, entity_p[14]);
	TEST_ASSERT_EQUAL_HEX(0xBC, entity_p[15]);

	coarse_start_timestamp_read = cmp_ent_get_coarse_start_time(&ent);
	TEST_ASSERT_EQUAL(0x12345678, coarse_start_timestamp_read);
	fine_start_timestamp_read = cmp_ent_get_fine_start_time(&ent);
	TEST_ASSERT_EQUAL(0x9ABC, fine_start_timestamp_read);

	/* error cases */
	start_timestamp = 0x1000000000000ULL;
	error = cmp_ent_set_start_timestamp(&ent, start_timestamp);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_start_timestamp(NULL, start_timestamp);
	TEST_ASSERT_TRUE(error);
	start_timestamp_read = cmp_ent_get_start_timestamp(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, start_timestamp_read);
}


/**
 * @test cmp_ent_set_coarse_start_time
 * @test cmp_ent_get_coarse_start_time
 */

void test_ent_coarse_start_time(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t coarse_start_time;
	uint32_t coarse_start_time_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	coarse_start_time = 0x12345678;
	error = cmp_ent_set_coarse_start_time(&ent, coarse_start_time);
	TEST_ASSERT_FALSE(error);

	coarse_start_time_read = cmp_ent_get_coarse_start_time(&ent);
	TEST_ASSERT_EQUAL_UINT32(coarse_start_time, coarse_start_time_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[10]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[11]);
	TEST_ASSERT_EQUAL_HEX(0x56, entity_p[12]);
	TEST_ASSERT_EQUAL_HEX(0x78, entity_p[13]);

	/* error cases */
	error = cmp_ent_set_coarse_start_time(NULL, coarse_start_time);
	TEST_ASSERT_TRUE(error);
	coarse_start_time_read = cmp_ent_get_coarse_start_time(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, coarse_start_time_read);
}


/**
 * @test cmp_ent_set_fine_start_time
 * @test cmp_ent_get_fine_start_time
 */

void test_ent_fine_start_time(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint16_t fine_start_time;
	uint16_t fine_start_time_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	fine_start_time = 0x1234;
	error = cmp_ent_set_fine_start_time(&ent, fine_start_time);
	TEST_ASSERT_FALSE(error);

	fine_start_time_read = cmp_ent_get_fine_start_time(&ent);
	TEST_ASSERT_EQUAL_UINT32(fine_start_time, fine_start_time_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[14]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[15]);

	/* error cases */
	error = cmp_ent_set_fine_start_time(NULL, fine_start_time);
	TEST_ASSERT_TRUE(error);
	fine_start_time_read = cmp_ent_get_fine_start_time(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, fine_start_time_read);
}


/**
 * @test cmp_ent_set_end_timestamp
 * @test cmp_ent_get_end_timestamp
 */

void test_ent_end_timestamp(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint64_t end_timestamp;
	uint64_t end_timestamp_read;
	uint32_t coarse_end_timestamp_read;
	uint16_t fine_end_timestamp_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	end_timestamp	= 0x123456789ABCULL;
	error = cmp_ent_set_end_timestamp(&ent, end_timestamp);
	TEST_ASSERT_FALSE(error);

	end_timestamp_read = cmp_ent_get_end_timestamp(&ent);
	TEST_ASSERT_EQUAL(end_timestamp, end_timestamp_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[16]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[17]);
	TEST_ASSERT_EQUAL_HEX(0x56, entity_p[18]);
	TEST_ASSERT_EQUAL_HEX(0x78, entity_p[19]);
	TEST_ASSERT_EQUAL_HEX(0x9A, entity_p[20]);
	TEST_ASSERT_EQUAL_HEX(0xBC, entity_p[21]);

	coarse_end_timestamp_read = cmp_ent_get_coarse_end_time(&ent);
	TEST_ASSERT_EQUAL(0x12345678, coarse_end_timestamp_read);
	fine_end_timestamp_read = cmp_ent_get_fine_end_time(&ent);
	TEST_ASSERT_EQUAL(0x9ABC, fine_end_timestamp_read);

	/* error cases */
	end_timestamp = 0x1000000000000ULL;
	error = cmp_ent_set_end_timestamp(&ent, end_timestamp);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_end_timestamp(NULL, end_timestamp);
	TEST_ASSERT_TRUE(error);
	end_timestamp_read = cmp_ent_get_end_timestamp(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, end_timestamp_read);
}


/**
 * @test cmp_ent_set_coarse_end_time
 * @test cmp_ent_get_coarse_end_time
 */

void test_ent_coarse_end_time(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t coarse_end_time;
	uint32_t coarse_end_time_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	coarse_end_time = 0x12345678;
	error = cmp_ent_set_coarse_end_time(&ent, coarse_end_time);
	TEST_ASSERT_FALSE(error);

	coarse_end_time_read = cmp_ent_get_coarse_end_time(&ent);
	TEST_ASSERT_EQUAL_UINT32(coarse_end_time, coarse_end_time_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[16]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[17]);
	TEST_ASSERT_EQUAL_HEX(0x56, entity_p[18]);
	TEST_ASSERT_EQUAL_HEX(0x78, entity_p[19]);

	/* error cases */
	error = cmp_ent_set_coarse_end_time(NULL, coarse_end_time);
	TEST_ASSERT_TRUE(error);
	coarse_end_time_read = cmp_ent_get_coarse_end_time(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, coarse_end_time_read);
}


/**
 * @test cmp_ent_set_fine_end_time
 * @test cmp_ent_get_fine_end_time
 */

void test_ent_fine_end_time(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint16_t fine_end_time;
	uint16_t fine_end_time_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	fine_end_time = 0x1234;
	error = cmp_ent_set_fine_end_time(&ent, fine_end_time);
	TEST_ASSERT_FALSE(error);

	fine_end_time_read = cmp_ent_get_fine_end_time(&ent);
	TEST_ASSERT_EQUAL_UINT32(fine_end_time, fine_end_time_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[20]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[21]);

	/* error cases */
	error = cmp_ent_set_fine_end_time(NULL, fine_end_time);
	TEST_ASSERT_TRUE(error);
	fine_end_time_read = cmp_ent_get_fine_end_time(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, fine_end_time_read);
}


/**
 * @test cmp_ent_set_data_type
 * @test cmp_ent_get_data_type
 * @test cmp_ent_get_data_type_raw_bit
 */

void test_cmp_ent_data_type(void)
{
	int error;
	struct cmp_entity ent = {0};
	enum cmp_data_type data_type, data_type_read;
	int raw_mode_flag, raw_mode_flag_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	/* test raw_mode */
	raw_mode_flag = 1;
	data_type = DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE;
	error = cmp_ent_set_data_type(&ent, data_type, raw_mode_flag);
	TEST_ASSERT_FALSE(error);

	data_type_read = cmp_ent_get_data_type(&ent);
	TEST_ASSERT_EQUAL_HEX32(data_type, data_type_read);
	raw_mode_flag_read = cmp_ent_get_data_type_raw_bit(&ent);
	TEST_ASSERT_EQUAL(raw_mode_flag, raw_mode_flag_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x80, entity_p[22]);
	TEST_ASSERT_EQUAL_HEX(21, entity_p[23]);

	/* test non raw_mode */
	raw_mode_flag = 0;
	data_type = DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE;
	error = cmp_ent_set_data_type(&ent, data_type, raw_mode_flag);
	TEST_ASSERT_FALSE(error);

	data_type_read = cmp_ent_get_data_type(&ent);
	TEST_ASSERT_EQUAL_HEX32(data_type, data_type_read);
	raw_mode_flag_read = cmp_ent_get_data_type_raw_bit(&ent);
	TEST_ASSERT_EQUAL(raw_mode_flag, raw_mode_flag_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0, entity_p[22]);
	TEST_ASSERT_EQUAL_HEX(21, entity_p[23]);


	/* error cases */
	raw_mode_flag = 0;
	data_type = (enum cmp_data_type)0x8000;
	error = cmp_ent_set_data_type(&ent, data_type, raw_mode_flag);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_data_type(NULL, data_type, raw_mode_flag);
	TEST_ASSERT_TRUE(error);
	data_type_read = cmp_ent_get_data_type(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, data_type_read);
	raw_mode_flag_read = cmp_ent_get_data_type_raw_bit(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, raw_mode_flag_read);
}


/**
 * @test cmp_ent_set_cmp_mode
 * @test cmp_ent_get_cmp_mode
 */

void test_ent_cmp_mode(void)
{
	int error;
	struct cmp_entity ent = {0};
	enum cmp_mode cmp_mode, cmp_mode_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	cmp_mode = (enum cmp_mode)0x12;
	error = cmp_ent_set_cmp_mode(&ent, cmp_mode);
	TEST_ASSERT_FALSE(error);

	cmp_mode_read = cmp_ent_get_cmp_mode(&ent);
	TEST_ASSERT_EQUAL_UINT32(cmp_mode, cmp_mode_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[24]);

	/* error cases */
	cmp_mode = (enum cmp_mode)0x100;
	error = cmp_ent_set_cmp_mode(&ent, cmp_mode);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_cmp_mode(NULL, cmp_mode);
	TEST_ASSERT_TRUE(error);
	cmp_mode_read = cmp_ent_get_cmp_mode(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, cmp_mode_read);
}


/**
 * @test cmp_ent_set_model_value
 * @test cmp_ent_get_model_value
 */

void test_ent_model_value(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t model_value, model_value_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	model_value = 0x12;
	error = cmp_ent_set_model_value(&ent, model_value);
	TEST_ASSERT_FALSE(error);

	model_value_read = cmp_ent_get_model_value(&ent);
	TEST_ASSERT_EQUAL_UINT32(model_value, model_value_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[25]);

	/* error cases */
	model_value = 0x100;
	error = cmp_ent_set_model_value(&ent, model_value);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_model_value(NULL, model_value);
	TEST_ASSERT_TRUE(error);
	model_value_read = cmp_ent_get_model_value(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, model_value_read);
}


/**
 * @test cmp_ent_set_model_id
 * @test cmp_ent_get_model_id
 */

void test_ent_model_id(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t model_id, model_id_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	model_id = 0x1234;
	error = cmp_ent_set_model_id(&ent, model_id);
	TEST_ASSERT_FALSE(error);

	model_id_read = cmp_ent_get_model_id(&ent);
	TEST_ASSERT_EQUAL_UINT32(model_id, model_id_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[26]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[27]);

	/* error cases */
	model_id = 0x10000;
	error = cmp_ent_set_model_id(&ent, model_id);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_model_id(NULL, model_id);
	TEST_ASSERT_TRUE(error);
	model_id_read = cmp_ent_get_model_id(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, model_id_read);
}


/**
 * @test cmp_ent_set_model_counter
 * @test cmp_ent_get_model_counter
 */

void test_ent_model_counter(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t model_counter, model_counter_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	model_counter = 0x12;
	error = cmp_ent_set_model_counter(&ent, model_counter);
	TEST_ASSERT_FALSE(error);

	model_counter_read = cmp_ent_get_model_counter(&ent);
	TEST_ASSERT_EQUAL_UINT32(model_counter, model_counter_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[28]);

	/* error cases */
	model_counter = 0x100;
	error = cmp_ent_set_model_counter(&ent, model_counter);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_model_counter(NULL, model_counter);
	TEST_ASSERT_TRUE(error);
	model_counter_read = cmp_ent_get_model_counter(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, model_counter_read);
}


/**
 * @test cmp_ent_set_reserved
 * @test cmp_ent_get_reserved
 */

void test_ent_reserved(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint8_t reserved, reserved_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	reserved = 0x12;
	error = cmp_ent_set_reserved(&ent, reserved);
	TEST_ASSERT_FALSE(error);

	reserved_read = cmp_ent_get_reserved(&ent);
	TEST_ASSERT_EQUAL_UINT32(reserved, reserved_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[29]);

	/* error cases */
	error = cmp_ent_set_reserved(NULL, reserved);
	TEST_ASSERT_TRUE(error);
	reserved_read = cmp_ent_get_reserved(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, reserved_read);
}


/**
 * @test cmp_ent_set_lossy_cmp_par
 * @test cmp_ent_get_lossy_cmp_par
 */

void test_ent_lossy_cmp_par(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t lossy_cmp_par, lossy_cmp_par_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	lossy_cmp_par = 0x1234;
	error = cmp_ent_set_lossy_cmp_par(&ent, lossy_cmp_par);
	TEST_ASSERT_FALSE(error);

	lossy_cmp_par_read = cmp_ent_get_lossy_cmp_par(&ent);
	TEST_ASSERT_EQUAL_UINT32(lossy_cmp_par, lossy_cmp_par_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[30]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[31]);

	/* error cases */
	lossy_cmp_par = 0x10000;
	error = cmp_ent_set_lossy_cmp_par(&ent, lossy_cmp_par);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_lossy_cmp_par(NULL, lossy_cmp_par);
	TEST_ASSERT_TRUE(error);
	lossy_cmp_par_read = cmp_ent_get_lossy_cmp_par(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, lossy_cmp_par_read);
}


/**
 * @test cmp_ent_set_ima_spill
 * @test cmp_ent_get_ima_spill
 */

void test_ent_ima_spill(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t ima_spill, ima_spill_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	ima_spill = 0x1234;
	error = cmp_ent_set_ima_spill(&ent, ima_spill);
	TEST_ASSERT_FALSE(error);

	ima_spill_read = cmp_ent_get_ima_spill(&ent);
	TEST_ASSERT_EQUAL_UINT32(ima_spill, ima_spill_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[32]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[33]);

	/* error cases */
	ima_spill = 0x10000;
	error = cmp_ent_set_ima_spill(&ent, ima_spill);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_ima_spill(NULL, ima_spill);
	TEST_ASSERT_TRUE(error);
	ima_spill_read = cmp_ent_get_ima_spill(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, ima_spill_read);
}


/**
 * @test cmp_ent_set_ima_golomb_par
 * @test cmp_ent_get_ima_golomb_par
 */

void test_ent_ima_golomb_par(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t ima_golomb_par, ima_golomb_par_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	ima_golomb_par = 0x12;
	error = cmp_ent_set_ima_golomb_par(&ent, ima_golomb_par);
	TEST_ASSERT_FALSE(error);

	ima_golomb_par_read = cmp_ent_get_ima_golomb_par(&ent);
	TEST_ASSERT_EQUAL_UINT32(ima_golomb_par, ima_golomb_par_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[34]);

	/* error cases */
	ima_golomb_par = 0x100;
	error = cmp_ent_set_ima_golomb_par(&ent, ima_golomb_par);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_ima_golomb_par(NULL, ima_golomb_par);
	TEST_ASSERT_TRUE(error);
	ima_golomb_par_read = cmp_ent_get_ima_golomb_par(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, ima_golomb_par_read);
}


/**
 * @test cmp_ent_set_ima_ap1_spill
 * @test cmp_ent_get_ima_ap1_spill
 */

void test_ent_ima_ap1_spill(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t ima_ap1_spill, ima_ap1_spill_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	ima_ap1_spill = 0x1234;
	error = cmp_ent_set_ima_ap1_spill(&ent, ima_ap1_spill);
	TEST_ASSERT_FALSE(error);

	ima_ap1_spill_read = cmp_ent_get_ima_ap1_spill(&ent);
	TEST_ASSERT_EQUAL_UINT32(ima_ap1_spill, ima_ap1_spill_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[35]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[36]);

	/* error cases */
	ima_ap1_spill = 0x10000;
	error = cmp_ent_set_ima_ap1_spill(&ent, ima_ap1_spill);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_ima_ap1_spill(NULL, ima_ap1_spill);
	TEST_ASSERT_TRUE(error);
	ima_ap1_spill_read = cmp_ent_get_ima_ap1_spill(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, ima_ap1_spill_read);
}


/**
 * @test cmp_ent_set_ima_ap1_golomb_par
 * @test cmp_ent_get_ima_ap1_golomb_par
 */

void test_ent_ima_ap1_golomb_par(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t ima_ap1_golomb_par, ima_ap1_golomb_par_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	ima_ap1_golomb_par = 0x12;
	error = cmp_ent_set_ima_ap1_golomb_par(&ent, ima_ap1_golomb_par);
	TEST_ASSERT_FALSE(error);

	ima_ap1_golomb_par_read = cmp_ent_get_ima_ap1_golomb_par(&ent);
	TEST_ASSERT_EQUAL_UINT32(ima_ap1_golomb_par, ima_ap1_golomb_par_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[37]);

	/* error cases */
	ima_ap1_golomb_par = 0x100;
	error = cmp_ent_set_ima_ap1_golomb_par(&ent, ima_ap1_golomb_par);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_ima_ap1_golomb_par(NULL, ima_ap1_golomb_par);
	TEST_ASSERT_TRUE(error);
	ima_ap1_golomb_par_read = cmp_ent_get_ima_ap1_golomb_par(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, ima_ap1_golomb_par_read);
}


/**
 * @test cmp_ent_set_ima_ap2_spill
 * @test cmp_ent_get_ima_ap2_spill
 */

void test_ent_ima_ap2_spill(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t ima_ap2_spill, ima_ap2_spill_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	ima_ap2_spill = 0x1234;
	error = cmp_ent_set_ima_ap2_spill(&ent, ima_ap2_spill);
	TEST_ASSERT_FALSE(error);

	ima_ap2_spill_read = cmp_ent_get_ima_ap2_spill(&ent);
	TEST_ASSERT_EQUAL_UINT32(ima_ap2_spill, ima_ap2_spill_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[38]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[39]);

	/* error cases */
	ima_ap2_spill = 0x10000;
	error = cmp_ent_set_ima_ap2_spill(&ent, ima_ap2_spill);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_ima_ap2_spill(NULL, ima_ap2_spill);
	TEST_ASSERT_TRUE(error);
	ima_ap2_spill_read = cmp_ent_get_ima_ap2_spill(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, ima_ap2_spill_read);
}


/**
 * @test cmp_ent_set_ima_ap2_golomb_par
 * @test cmp_ent_get_ima_ap2_golomb_par
 */

void test_ent_ima_ap2_golomb_par(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t ima_ap2_golomb_par, ima_ap2_golomb_par_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	ima_ap2_golomb_par = 0x12;
	error = cmp_ent_set_ima_ap2_golomb_par(&ent, ima_ap2_golomb_par);
	TEST_ASSERT_FALSE(error);

	ima_ap2_golomb_par_read = cmp_ent_get_ima_ap2_golomb_par(&ent);
	TEST_ASSERT_EQUAL_UINT32(ima_ap2_golomb_par, ima_ap2_golomb_par_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[40]);

	/* error cases */
	ima_ap2_golomb_par = 0x100;
	error = cmp_ent_set_ima_ap2_golomb_par(&ent, ima_ap2_golomb_par);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_ima_ap2_golomb_par(NULL, ima_ap2_golomb_par);
	TEST_ASSERT_TRUE(error);
	ima_ap2_golomb_par_read = cmp_ent_get_ima_ap2_golomb_par(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, ima_ap2_golomb_par_read);
}


/**
 * @test cmp_ent_set_non_ima_spill1
 * @test cmp_ent_get_non_ima_spill1
 */

void test_ent_non_ima_spill1(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t non_ima_spill1, non_ima_spill1_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	non_ima_spill1 = 0x123456;
	error = cmp_ent_set_non_ima_spill1(&ent, non_ima_spill1);
	TEST_ASSERT_FALSE(error);

	non_ima_spill1_read = cmp_ent_get_non_ima_spill1(&ent);
	TEST_ASSERT_EQUAL_UINT32(non_ima_spill1, non_ima_spill1_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[32]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[33]);
	TEST_ASSERT_EQUAL_HEX(0x56, entity_p[34]);

	/* error cases */
	non_ima_spill1 = 0x1000000;
	error = cmp_ent_set_non_ima_spill1(&ent, non_ima_spill1);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_non_ima_spill1(NULL, non_ima_spill1);
	TEST_ASSERT_TRUE(error);
	non_ima_spill1_read = cmp_ent_get_non_ima_spill1(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, non_ima_spill1_read);
}


/**
 * @test cmp_ent_set_non_ima_cmp_par1
 * @test cmp_ent_get_non_ima_cmp_par1
 */

void test_ent_non_ima_cmp_par1(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t non_ima_cmp_par1, non_ima_cmp_par1_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	non_ima_cmp_par1 = 0x1234;
	error = cmp_ent_set_non_ima_cmp_par1(&ent, non_ima_cmp_par1);
	TEST_ASSERT_FALSE(error);

	non_ima_cmp_par1_read = cmp_ent_get_non_ima_cmp_par1(&ent);
	TEST_ASSERT_EQUAL_UINT32(non_ima_cmp_par1, non_ima_cmp_par1_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[35]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[36]);

	/* error cases */
	non_ima_cmp_par1 = 0x10000;
	error = cmp_ent_set_non_ima_cmp_par1(&ent, non_ima_cmp_par1);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_non_ima_cmp_par1(NULL, non_ima_cmp_par1);
	TEST_ASSERT_TRUE(error);
	non_ima_cmp_par1_read = cmp_ent_get_non_ima_cmp_par1(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, non_ima_cmp_par1_read);
}


/**
 * @test cmp_ent_set_non_ima_spill2
 * @test cmp_ent_get_non_ima_spill2
 */

void test_ent_non_ima_spill2(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t non_ima_spill2, non_ima_spill2_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	non_ima_spill2 = 0x123456;
	error = cmp_ent_set_non_ima_spill2(&ent, non_ima_spill2);
	TEST_ASSERT_FALSE(error);

	non_ima_spill2_read = cmp_ent_get_non_ima_spill2(&ent);
	TEST_ASSERT_EQUAL_UINT32(non_ima_spill2, non_ima_spill2_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[37]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[38]);
	TEST_ASSERT_EQUAL_HEX(0x56, entity_p[39]);

	/* error cases */
	non_ima_spill2 = 0x1000000;
	error = cmp_ent_set_non_ima_spill2(&ent, non_ima_spill2);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_non_ima_spill2(NULL, non_ima_spill2);
	TEST_ASSERT_TRUE(error);
	non_ima_spill2_read = cmp_ent_get_non_ima_spill2(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, non_ima_spill2_read);
}


/**
 * @test cmp_ent_set_non_ima_cmp_par2
 * @test cmp_ent_get_non_ima_cmp_par2
 */

void test_ent_non_ima_cmp_par2(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t non_ima_cmp_par2, non_ima_cmp_par2_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	non_ima_cmp_par2 = 0x1234;
	error = cmp_ent_set_non_ima_cmp_par2(&ent, non_ima_cmp_par2);
	TEST_ASSERT_FALSE(error);

	non_ima_cmp_par2_read = cmp_ent_get_non_ima_cmp_par2(&ent);
	TEST_ASSERT_EQUAL_UINT32(non_ima_cmp_par2, non_ima_cmp_par2_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[40]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[41]);

	/* error cases */
	non_ima_cmp_par2 = 0x10000;
	error = cmp_ent_set_non_ima_cmp_par2(&ent, non_ima_cmp_par2);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_non_ima_cmp_par2(NULL, non_ima_cmp_par2);
	TEST_ASSERT_TRUE(error);
	non_ima_cmp_par2_read = cmp_ent_get_non_ima_cmp_par2(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, non_ima_cmp_par2_read);
}


/**
 * @test cmp_ent_set_non_ima_spill3
 * @test cmp_ent_get_non_ima_spill3
 */

void test_ent_non_ima_spill3(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t non_ima_spill3, non_ima_spill3_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	non_ima_spill3 = 0x123456;
	error = cmp_ent_set_non_ima_spill3(&ent, non_ima_spill3);
	TEST_ASSERT_FALSE(error);

	non_ima_spill3_read = cmp_ent_get_non_ima_spill3(&ent);
	TEST_ASSERT_EQUAL_UINT32(non_ima_spill3, non_ima_spill3_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[42]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[43]);
	TEST_ASSERT_EQUAL_HEX(0x56, entity_p[44]);

	/* error cases */
	non_ima_spill3 = 0x1000000;
	error = cmp_ent_set_non_ima_spill3(&ent, non_ima_spill3);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_non_ima_spill3(NULL, non_ima_spill3);
	TEST_ASSERT_TRUE(error);
	non_ima_spill3_read = cmp_ent_get_non_ima_spill3(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, non_ima_spill3_read);
}


/**
 * @test cmp_ent_set_non_ima_cmp_par3
 * @test cmp_ent_get_non_ima_cmp_par3
 */

void test_ent_non_ima_cmp_par3(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t non_ima_cmp_par3, non_ima_cmp_par3_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	non_ima_cmp_par3 = 0x1234;
	error = cmp_ent_set_non_ima_cmp_par3(&ent, non_ima_cmp_par3);
	TEST_ASSERT_FALSE(error);

	non_ima_cmp_par3_read = cmp_ent_get_non_ima_cmp_par3(&ent);
	TEST_ASSERT_EQUAL_UINT32(non_ima_cmp_par3, non_ima_cmp_par3_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[45]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[46]);

	/* error cases */
	non_ima_cmp_par3 = 0x10000;
	error = cmp_ent_set_non_ima_cmp_par3(&ent, non_ima_cmp_par3);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_non_ima_cmp_par3(NULL, non_ima_cmp_par3);
	TEST_ASSERT_TRUE(error);
	non_ima_cmp_par3_read = cmp_ent_get_non_ima_cmp_par3(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, non_ima_cmp_par3_read);
}


/**
 * @test cmp_ent_set_non_ima_spill4
 * @test cmp_ent_get_non_ima_spill4
 */

void test_ent_non_ima_spill4(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t non_ima_spill4, non_ima_spill4_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	non_ima_spill4 = 0x123456;
	error = cmp_ent_set_non_ima_spill4(&ent, non_ima_spill4);
	TEST_ASSERT_FALSE(error);

	non_ima_spill4_read = cmp_ent_get_non_ima_spill4(&ent);
	TEST_ASSERT_EQUAL_UINT32(non_ima_spill4, non_ima_spill4_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[47]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[48]);
	TEST_ASSERT_EQUAL_HEX(0x56, entity_p[49]);

	/* error cases */
	non_ima_spill4 = 0x1000000;
	error = cmp_ent_set_non_ima_spill4(&ent, non_ima_spill4);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_non_ima_spill4(NULL, non_ima_spill4);
	TEST_ASSERT_TRUE(error);
	non_ima_spill4_read = cmp_ent_get_non_ima_spill4(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, non_ima_spill4_read);
}


/**
 * @test cmp_ent_set_non_ima_cmp_par4
 * @test cmp_ent_get_non_ima_cmp_par4
 */

void test_ent_non_ima_cmp_par4(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t non_ima_cmp_par4, non_ima_cmp_par4_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	non_ima_cmp_par4 = 0x1234;
	error = cmp_ent_set_non_ima_cmp_par4(&ent, non_ima_cmp_par4);
	TEST_ASSERT_FALSE(error);

	non_ima_cmp_par4_read = cmp_ent_get_non_ima_cmp_par4(&ent);
	TEST_ASSERT_EQUAL_UINT32(non_ima_cmp_par4, non_ima_cmp_par4_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[50]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[51]);

	/* error cases */
	non_ima_cmp_par4 = 0x10000;
	error = cmp_ent_set_non_ima_cmp_par4(&ent, non_ima_cmp_par4);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_non_ima_cmp_par4(NULL, non_ima_cmp_par4);
	TEST_ASSERT_TRUE(error);
	non_ima_cmp_par4_read = cmp_ent_get_non_ima_cmp_par4(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, non_ima_cmp_par4_read);
}


/**
 * @test cmp_ent_set_non_ima_spill5
 * @test cmp_ent_get_non_ima_spill5
 */

void test_ent_non_ima_spill5(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t non_ima_spill5, non_ima_spill5_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	non_ima_spill5 = 0x123456;
	error = cmp_ent_set_non_ima_spill5(&ent, non_ima_spill5);
	TEST_ASSERT_FALSE(error);

	non_ima_spill5_read = cmp_ent_get_non_ima_spill5(&ent);
	TEST_ASSERT_EQUAL_UINT32(non_ima_spill5, non_ima_spill5_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[52]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[53]);
	TEST_ASSERT_EQUAL_HEX(0x56, entity_p[54]);

	/* error cases */
	non_ima_spill5 = 0x1000000;
	error = cmp_ent_set_non_ima_spill5(&ent, non_ima_spill5);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_non_ima_spill5(NULL, non_ima_spill5);
	TEST_ASSERT_TRUE(error);
	non_ima_spill5_read = cmp_ent_get_non_ima_spill5(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, non_ima_spill5_read);
}


/**
 * @test cmp_ent_set_non_ima_cmp_par5
 * @test cmp_ent_get_non_ima_cmp_par5
 */

void test_ent_non_ima_cmp_par5(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t non_ima_cmp_par5, non_ima_cmp_par5_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	non_ima_cmp_par5 = 0x1234;
	error = cmp_ent_set_non_ima_cmp_par5(&ent, non_ima_cmp_par5);
	TEST_ASSERT_FALSE(error);

	non_ima_cmp_par5_read = cmp_ent_get_non_ima_cmp_par5(&ent);
	TEST_ASSERT_EQUAL_UINT32(non_ima_cmp_par5, non_ima_cmp_par5_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[55]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[56]);

	/* error cases */
	non_ima_cmp_par5 = 0x10000;
	error = cmp_ent_set_non_ima_cmp_par5(&ent, non_ima_cmp_par5);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_non_ima_cmp_par5(NULL, non_ima_cmp_par5);
	TEST_ASSERT_TRUE(error);
	non_ima_cmp_par5_read = cmp_ent_get_non_ima_cmp_par5(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, non_ima_cmp_par5_read);
}


/**
 * @test cmp_ent_set_non_ima_spill6
 * @test cmp_ent_get_non_ima_spill6
 */

void test_ent_non_ima_spill6(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t non_ima_spill6, non_ima_spill6_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	non_ima_spill6 = 0x123456;
	error = cmp_ent_set_non_ima_spill6(&ent, non_ima_spill6);
	TEST_ASSERT_FALSE(error);

	non_ima_spill6_read = cmp_ent_get_non_ima_spill6(&ent);
	TEST_ASSERT_EQUAL_UINT32(non_ima_spill6, non_ima_spill6_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[57]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[58]);
	TEST_ASSERT_EQUAL_HEX(0x56, entity_p[59]);

	/* error cases */
	non_ima_spill6 = 0x1000000;
	error = cmp_ent_set_non_ima_spill6(&ent, non_ima_spill6);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_non_ima_spill6(NULL, non_ima_spill6);
	TEST_ASSERT_TRUE(error);
	non_ima_spill6_read = cmp_ent_get_non_ima_spill6(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, non_ima_spill6_read);
}


/**
 * @test cmp_ent_set_non_ima_cmp_par6
 * @test cmp_ent_get_non_ima_cmp_par6
 */

void test_ent_non_ima_cmp_par6(void)
{
	int error;
	struct cmp_entity ent = {0};
	uint32_t non_ima_cmp_par6, non_ima_cmp_par6_read;
	const uint8_t *entity_p = (const uint8_t *)&ent;

	non_ima_cmp_par6 = 0x1234;
	error = cmp_ent_set_non_ima_cmp_par6(&ent, non_ima_cmp_par6);
	TEST_ASSERT_FALSE(error);

	non_ima_cmp_par6_read = cmp_ent_get_non_ima_cmp_par6(&ent);
	TEST_ASSERT_EQUAL_UINT32(non_ima_cmp_par6, non_ima_cmp_par6_read);

	/* check the right position in the header */
	TEST_ASSERT_EQUAL_HEX(0x12, entity_p[60]);
	TEST_ASSERT_EQUAL_HEX(0x34, entity_p[61]);

	/* error cases */
	non_ima_cmp_par6 = 0x10000;
	error = cmp_ent_set_non_ima_cmp_par6(&ent, non_ima_cmp_par6);
	TEST_ASSERT_TRUE(error);
	error = cmp_ent_set_non_ima_cmp_par6(NULL, non_ima_cmp_par6);
	TEST_ASSERT_TRUE(error);
	non_ima_cmp_par6_read = cmp_ent_get_non_ima_cmp_par6(NULL);
	TEST_ASSERT_EQUAL_UINT32(0, non_ima_cmp_par6_read);
}


/**
 * @test cmp_ent_get_data_buf
 */

void test_cmp_ent_get_data_buf(void)
{
	enum cmp_data_type data_type;
	struct cmp_entity ent = {0};
	const char *adr;
	uint32_t s, hdr_size;
	int error;

	for (data_type = DATA_TYPE_IMAGETTE;
	     data_type <= DATA_TYPE_F_CAM_BACKGROUND;
	     data_type++) {
		s = cmp_ent_create(&ent, data_type, 0, 0);
		TEST_ASSERT_NOT_EQUAL_INT(0, s);

		adr = cmp_ent_get_data_buf(&ent);
		TEST_ASSERT_NOT_NULL(adr);

		hdr_size = cmp_ent_cal_hdr_size(data_type, 0);
		TEST_ASSERT_EQUAL_INT(hdr_size, adr-(char *)&ent);
	}

	/* RAW mode test */
	for (data_type = DATA_TYPE_IMAGETTE;
	     data_type <= DATA_TYPE_CHUNK;
	     data_type++) {
		s = cmp_ent_create(&ent, data_type, 1, 0);
		TEST_ASSERT_NOT_EQUAL_INT(0, s);

		adr = cmp_ent_get_data_buf(&ent);
		TEST_ASSERT_NOT_NULL(adr);

		hdr_size = cmp_ent_cal_hdr_size(data_type, 1);
		TEST_ASSERT_EQUAL_INT(hdr_size, adr-(char *)&ent);
	}

	/* ent = NULL test */
	adr = cmp_ent_get_data_buf(NULL);
	TEST_ASSERT_NULL(adr);

	/* compression data type not supported test */
	error = cmp_ent_set_data_type(&ent, DATA_TYPE_UNKNOWN, 0);
	TEST_ASSERT_FALSE(error);
	adr = cmp_ent_get_data_buf(&ent);
	TEST_ASSERT_NULL(adr);

	error = cmp_ent_set_data_type(&ent, (enum cmp_data_type)234, 1);
	TEST_ASSERT_FALSE(error);
	adr = cmp_ent_get_data_buf(&ent);
	TEST_ASSERT_NULL(adr);
}


/**
 * @test cmp_ent_get_data_buf_const
 */

void test_cmp_ent_get_data_buf_const(void)
{
	enum cmp_data_type data_type;
	struct cmp_entity ent = {0};
	const char *adr;
	uint32_t s, hdr_size;
	int error;

	for (data_type = DATA_TYPE_IMAGETTE;
	     data_type <= DATA_TYPE_F_CAM_BACKGROUND;
	     data_type++) {
		s = cmp_ent_create(&ent, data_type, 0, 0);
		TEST_ASSERT_NOT_EQUAL_INT(0, s);

		adr = cmp_ent_get_data_buf_const(&ent);
		TEST_ASSERT_NOT_NULL(adr);

		hdr_size = cmp_ent_cal_hdr_size(data_type, 0);
		TEST_ASSERT_EQUAL_INT(hdr_size, adr-(char *)&ent);
	}

	/* RAW mode test */
	for (data_type = DATA_TYPE_IMAGETTE;
	     data_type <= DATA_TYPE_CHUNK;
	     data_type++) {
		s = cmp_ent_create(&ent, data_type, 1, 0);
		TEST_ASSERT_NOT_EQUAL_INT(0, s);

		adr = cmp_ent_get_data_buf_const(&ent);
		TEST_ASSERT_NOT_NULL(adr);

		hdr_size = cmp_ent_cal_hdr_size(data_type, 1);
		TEST_ASSERT_EQUAL_INT(hdr_size, adr-(char *)&ent);
	}

	/* ent = NULL test */
	adr = cmp_ent_get_data_buf_const(NULL);
	TEST_ASSERT_NULL(adr);

	/* compression data type not supported test */
	error = cmp_ent_set_data_type(&ent, DATA_TYPE_UNKNOWN, 0);
	TEST_ASSERT_FALSE(error);
	adr = cmp_ent_get_data_buf_const(&ent);
	TEST_ASSERT_NULL(adr);

	error = cmp_ent_set_data_type(&ent, (enum cmp_data_type)234, 1);
	TEST_ASSERT_FALSE(error);
	adr = cmp_ent_get_data_buf_const(&ent);
	TEST_ASSERT_NULL(adr);
}


/**
 * @test cmp_ent_get_cmp_data
 */

void test_cmp_ent_get_cmp_data(void)
{
	int32_t size;
	struct cmp_entity *ent;
	uint32_t *data_buf;
	uint32_t s;
	uint8_t i;
	uint8_t *data_p;

	/* setup compression entity */
	s = cmp_ent_create(NULL, DATA_TYPE_S_FX, 0, 12);
	TEST_ASSERT_EQUAL_INT(NON_IMAGETTE_HEADER_SIZE + 12, s);
	ent = malloc(s); TEST_ASSERT_NOT_NULL(ent);
	s = cmp_ent_create(ent, DATA_TYPE_S_FX, 0, 12);
	TEST_ASSERT_EQUAL_INT(NON_IMAGETTE_HEADER_SIZE + 12, s);
	data_p = cmp_ent_get_data_buf(ent);
	for (i = 0; i < 12 ; ++i)
		data_p[i] = i;

	size = cmp_ent_get_cmp_data(ent, NULL, 0);
	TEST_ASSERT_EQUAL_INT(12, size);
	data_buf = malloc((size_t)size); TEST_ASSERT_NOT_NULL(data_buf);

	size = cmp_ent_get_cmp_data(ent, data_buf, 12);
	TEST_ASSERT_EQUAL_INT(12, size);
	TEST_ASSERT_EQUAL_HEX32(0x00010203, data_buf[0]);
	TEST_ASSERT_EQUAL_HEX32(0x04050607, data_buf[1]);
	TEST_ASSERT_EQUAL_HEX32(0x08090A0B, data_buf[2]);

	/* error cases */
	size = cmp_ent_get_cmp_data(NULL, data_buf, 12);
	TEST_ASSERT_EQUAL_INT(-1, size);

	cmp_ent_set_size(ent, NON_IMAGETTE_HEADER_SIZE + 11);
	size = cmp_ent_get_cmp_data(ent, NULL, 12);
	TEST_ASSERT_EQUAL_INT(-1, size);
	cmp_ent_set_size(ent, NON_IMAGETTE_HEADER_SIZE + 12);

	size = cmp_ent_get_cmp_data(ent, data_buf, 11);
	TEST_ASSERT_EQUAL_INT(-1, size);

	cmp_ent_set_data_type(ent, DATA_TYPE_UNKNOWN, 0);
	size = cmp_ent_get_cmp_data(ent, data_buf, 12);
	TEST_ASSERT_EQUAL_INT(-1, size);

	free(ent);
	free(data_buf);
}


/**
 * @test cmp_ent_get_cmp_data_size
 */

void test_cmp_ent_get_cmp_data_size(void)
{
	uint32_t cmp_data_size;
	struct cmp_entity ent;

	cmp_ent_set_data_type(&ent, DATA_TYPE_L_FX_EFX, 0);
	cmp_ent_set_size(&ent, 100);
	cmp_data_size = cmp_ent_get_cmp_data_size(&ent);
	TEST_ASSERT_EQUAL_UINT(100-NON_IMAGETTE_HEADER_SIZE, cmp_data_size);

	/* raw mode test */
	cmp_ent_set_data_type(&ent, DATA_TYPE_L_FX_EFX, 1);
	cmp_data_size = cmp_ent_get_cmp_data_size(&ent);
	TEST_ASSERT_EQUAL_UINT(100-GENERIC_HEADER_SIZE, cmp_data_size);

	/* error case */

	cmp_ent_set_data_type(&ent, DATA_TYPE_L_FX_NCOB, 0);
	cmp_ent_set_size(&ent, NON_IMAGETTE_HEADER_SIZE-1);
	cmp_data_size = cmp_ent_get_cmp_data_size(&ent);
	TEST_ASSERT_EQUAL_UINT(0, cmp_data_size);
}


/**
 * @test cmp_ent_write_rdcu_cmp_pars
 */

void test_cmp_ent_write_rdcu_cmp_pars(void)
{
	int error;
	uint32_t size;
	struct cmp_entity *ent;
	struct cmp_info info;
	struct rdcu_cfg rcfg;

	info.cmp_mode_used = CMP_MODE_DIFF_ZERO;
	info.spill_used = 42;
	info.golomb_par_used = 23;
	info.samples_used = 9;
	info.cmp_size = 96;
	info.model_value_used = 6;
	info.round_used = 1;
	info.cmp_err = 0;

	/* create a imagette compression entity */
	size = cmp_ent_create(NULL, DATA_TYPE_IMAGETTE, info.cmp_mode_used == CMP_MODE_RAW, 12);
	TEST_ASSERT_NOT_EQUAL_INT(0, size);
	ent = malloc(size); TEST_ASSERT_NOT_NULL(ent);
	size = cmp_ent_create(ent, DATA_TYPE_IMAGETTE, info.cmp_mode_used == CMP_MODE_RAW, 12);
	TEST_ASSERT_NOT_EQUAL_INT(0, size);

	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, NULL);
	TEST_ASSERT_FALSE(error);

	TEST_ASSERT_EQUAL_INT(DATA_TYPE_IMAGETTE, cmp_ent_get_data_type(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_data_type_raw_bit(ent));
	TEST_ASSERT_EQUAL_INT(12, cmp_ent_get_cmp_data_size(ent));

	TEST_ASSERT_EQUAL_INT(info.samples_used * sizeof(uint16_t), cmp_ent_get_original_size(ent));
	TEST_ASSERT_EQUAL_INT(info.cmp_mode_used, cmp_ent_get_cmp_mode(ent));
	TEST_ASSERT_EQUAL_INT(info.model_value_used, cmp_ent_get_model_value(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_reserved(ent));
	TEST_ASSERT_EQUAL_INT(info.round_used, cmp_ent_get_lossy_cmp_par(ent));

	TEST_ASSERT_EQUAL_INT(info.spill_used, cmp_ent_get_ima_spill(ent));
	TEST_ASSERT_EQUAL_INT(info.golomb_par_used, cmp_ent_get_ima_golomb_par(ent));

	free(ent);

	/* raw mode test */
	/* create a raw imagette compression entity */
	info.cmp_mode_used = CMP_MODE_RAW;
	size = cmp_ent_create(NULL, DATA_TYPE_IMAGETTE, info.cmp_mode_used == CMP_MODE_RAW, 12);
	TEST_ASSERT_NOT_EQUAL_INT(0, size);
	ent = malloc(size); TEST_ASSERT_NOT_NULL(ent);
	size = cmp_ent_create(ent, DATA_TYPE_IMAGETTE, info.cmp_mode_used == CMP_MODE_RAW, 12);
	TEST_ASSERT_NOT_EQUAL_INT(0, size);

	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, NULL);
	TEST_ASSERT_FALSE(error);

	TEST_ASSERT_EQUAL_INT(DATA_TYPE_IMAGETTE, cmp_ent_get_data_type(ent));
	TEST_ASSERT_EQUAL_INT(1, cmp_ent_get_data_type_raw_bit(ent));
	TEST_ASSERT_EQUAL_INT(12, cmp_ent_get_cmp_data_size(ent));

	TEST_ASSERT_EQUAL_INT(info.samples_used * sizeof(uint16_t), cmp_ent_get_original_size(ent));
	TEST_ASSERT_EQUAL_INT(info.cmp_mode_used, cmp_ent_get_cmp_mode(ent));
	TEST_ASSERT_EQUAL_INT(info.model_value_used, cmp_ent_get_model_value(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_reserved(ent));
	TEST_ASSERT_EQUAL_INT(info.round_used, cmp_ent_get_lossy_cmp_par(ent));

	free(ent);

	/* adaptive configuration */
	info.cmp_mode_used = CMP_MODE_MODEL_MULTI;
	rcfg.ap1_golomb_par = 0xFF;
	rcfg.ap1_spill = 1;
	rcfg.ap2_golomb_par = 0x32;
	rcfg.ap2_spill = 201;

	/* create a adaptive imagette compression entity */
	size = cmp_ent_create(NULL, DATA_TYPE_IMAGETTE_ADAPTIVE, info.cmp_mode_used == CMP_MODE_RAW, 12);
	TEST_ASSERT_NOT_EQUAL_INT(0, size);
	ent = malloc(size); TEST_ASSERT_NOT_NULL(ent);
	size = cmp_ent_create(ent, DATA_TYPE_IMAGETTE_ADAPTIVE, info.cmp_mode_used == CMP_MODE_RAW, 12);
	TEST_ASSERT_NOT_EQUAL_INT(0, size);

	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_FALSE(error);

	TEST_ASSERT_EQUAL_INT(DATA_TYPE_IMAGETTE_ADAPTIVE, cmp_ent_get_data_type(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_data_type_raw_bit(ent));
	TEST_ASSERT_EQUAL_INT(12, cmp_ent_get_cmp_data_size(ent));

	TEST_ASSERT_EQUAL_INT(info.samples_used * sizeof(uint16_t), cmp_ent_get_original_size(ent));
	TEST_ASSERT_EQUAL_INT(info.cmp_mode_used, cmp_ent_get_cmp_mode(ent));
	TEST_ASSERT_EQUAL_INT(info.model_value_used, cmp_ent_get_model_value(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_reserved(ent));
	TEST_ASSERT_EQUAL_INT(info.round_used, cmp_ent_get_lossy_cmp_par(ent));

	TEST_ASSERT_EQUAL_INT(info.spill_used, cmp_ent_get_ima_spill(ent));
	TEST_ASSERT_EQUAL_INT(info.golomb_par_used, cmp_ent_get_ima_golomb_par(ent));
	TEST_ASSERT_EQUAL_INT(rcfg.ap1_spill, cmp_ent_get_ima_ap1_spill(ent));
	TEST_ASSERT_EQUAL_INT(rcfg.ap1_golomb_par, cmp_ent_get_ima_ap1_golomb_par(ent));
	TEST_ASSERT_EQUAL_INT(rcfg.ap2_spill, cmp_ent_get_ima_ap2_spill(ent));
	TEST_ASSERT_EQUAL_INT(rcfg.ap2_golomb_par, cmp_ent_get_ima_ap2_golomb_par(ent));


	/** error cases **/

	/* ent = NULL */
	error = cmp_ent_write_rdcu_cmp_pars(NULL, &info, &rcfg);
	TEST_ASSERT_TRUE(error);

	/* info = NULL */
	error = cmp_ent_write_rdcu_cmp_pars(ent, NULL, &rcfg);
	TEST_ASSERT_TRUE(error);

	/* cfg = NULL and adaptive data type */
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, NULL);
	TEST_ASSERT_TRUE(error);

	/* compressed data are to big for the compression entity */
	info.cmp_size = 12*8 + 1;
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_TRUE(error);
	info.cmp_size = 1;

	/* wrong data_type */
	cmp_ent_set_data_type(ent, DATA_TYPE_S_FX, 0);
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_TRUE(error);
	cmp_ent_set_data_type(ent, DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE, 0);
	/* this should work */
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_FALSE(error);

	/* original_size to high */
	info.samples_used = 0x800000;
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_TRUE(error);
	info.samples_used = 0x7FFFFF;
	/* this should work */
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_FALSE(error);

	/* cmp_mode to high */
	info.cmp_mode_used = 0x100;
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_TRUE(error);
	info.cmp_mode_used = 0xFF;
	/* this should work */
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_FALSE(error);

	TEST_ASSERT_EQUAL_INT(1, sizeof(info.model_value_used));
	TEST_ASSERT_EQUAL_INT(1, sizeof(info.round_used));
#if 0
	/* max model_value to high */
	info.model_value_used = 0x100;
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &cfg);
	TEST_ASSERT_TRUE(error);
	info.model_value_used = 0xFF;
	/* this should work */
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &cfg);
	TEST_ASSERT_FALSE(error);

	/* max lossy_cmp_par to high */
	info.round = 0x10000;
	error = cmp_ent_write_cmp_pars(ent, &cfg, cmp_size_bits);
	TEST_ASSERT_TRUE(error);
	info.round = 0xFFFF;
	/* this should work */
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &cfg);
	TEST_ASSERT_FALSE(error);
#endif

	/* spill to high */
	info.spill_used = 0x10000;
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_TRUE(error);
	info.spill_used = 0xFFFF;
	/* this should work */
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_FALSE(error);

	/* golomb_par to high */
	info.golomb_par_used = 0x100;
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_TRUE(error);
	info.golomb_par_used = 0xFF;
	/* this should work */
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_FALSE(error);

	/* adaptive 1 spill to high */
	rcfg.ap1_spill = 0x10000;
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_TRUE(error);
	rcfg.ap1_spill = 0xFFFF;
	/* this should work */
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_FALSE(error);

	/* adaptive 1  golomb_par to high */
	rcfg.ap1_golomb_par = 0x100;
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_TRUE(error);
	rcfg.ap1_golomb_par = 0xFF;
	/* this should work */
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_FALSE(error);

	/* adaptive 2 spill to high */
	rcfg.ap2_spill = 0x10000;
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_TRUE(error);
	rcfg.ap2_spill = 0xFFFF;
	/* this should work */
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_FALSE(error);

	/* adaptive 2  golomb_par to high */
	rcfg.ap2_golomb_par = 0x100;
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_TRUE(error);
	rcfg.ap2_golomb_par = 0xFF;
	/* this should work */
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_FALSE(error);

	/* The entity's raw data bit is not set, but the configuration contains raw data */
	info.cmp_mode_used = CMP_MODE_RAW;
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_TRUE(error);
	info.cmp_mode_used = CMP_MODE_MODEL_MULTI;
	/* this should work */
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_FALSE(error);

	/* The entity's raw data bit is set, but the configuration contains no raw data */
	cmp_ent_set_data_type(ent, DATA_TYPE_IMAGETTE_ADAPTIVE, 1); /* set raw bit */
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_TRUE(error);
	cmp_ent_set_data_type(ent, DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE, 0);
	/* this should work */
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_FALSE(error);

	/* compression error set */
	info.cmp_err = 1;
	error = cmp_ent_write_rdcu_cmp_pars(ent, &info, &rcfg);
	TEST_ASSERT_TRUE(error);
	info.cmp_err = 0;

	free(ent);
}


/**
 * @test cmp_ent_create
 */

void test_cmp_ent_create(void)
{

	uint32_t size;
	struct cmp_entity *ent;
	enum cmp_data_type data_type;
	int raw_mode_flag;
	uint32_t cmp_size_byte;

	/* create a empty compression entity */
	data_type = DATA_TYPE_IMAGETTE;
	raw_mode_flag = 0;
	cmp_size_byte = 0;
	size = cmp_ent_create(NULL, data_type, raw_mode_flag, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT32(sizeof(struct cmp_entity), size);
	ent = malloc(size); TEST_ASSERT_NOT_NULL(ent);
	size = cmp_ent_create(ent, data_type, raw_mode_flag, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT32(sizeof(struct cmp_entity), size);

	TEST_ASSERT_EQUAL_INT(DATA_TYPE_IMAGETTE, cmp_ent_get_data_type(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_data_type_raw_bit(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_cmp_data_size(ent));

	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_original_size(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_cmp_mode(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_model_value(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_reserved(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_lossy_cmp_par(ent));

	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_ima_spill(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_ima_golomb_par(ent));
	free(ent);

	/* create a compression entity */
	data_type = DATA_TYPE_IMAGETTE;
	raw_mode_flag = 0;
	cmp_size_byte = 100;
	size = cmp_ent_create(NULL, data_type, raw_mode_flag, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT32(IMAGETTE_HEADER_SIZE + cmp_size_byte, size);
	ent = malloc(size); TEST_ASSERT_NOT_NULL(ent);
	size = cmp_ent_create(ent, data_type, raw_mode_flag, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT32(IMAGETTE_HEADER_SIZE + cmp_size_byte, size);

	TEST_ASSERT_EQUAL_INT(DATA_TYPE_IMAGETTE, cmp_ent_get_data_type(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_data_type_raw_bit(ent));
	TEST_ASSERT_EQUAL_INT(100, cmp_ent_get_cmp_data_size(ent));

	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_original_size(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_cmp_mode(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_model_value(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_reserved(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_lossy_cmp_par(ent));

	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_ima_spill(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_ima_golomb_par(ent));
	free(ent);

	/* create a raw compression entity */
	data_type = DATA_TYPE_SMEARING;
	raw_mode_flag = 1;
	cmp_size_byte = 100;
	size = cmp_ent_create(NULL, data_type, raw_mode_flag, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT32(GENERIC_HEADER_SIZE + cmp_size_byte, size);
	ent = malloc(size); TEST_ASSERT_NOT_NULL(ent);
	size = cmp_ent_create(ent, data_type, raw_mode_flag, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT32(GENERIC_HEADER_SIZE + cmp_size_byte, size);

	TEST_ASSERT_EQUAL_INT(DATA_TYPE_SMEARING, cmp_ent_get_data_type(ent));
	TEST_ASSERT_EQUAL_INT(1, cmp_ent_get_data_type_raw_bit(ent));
	TEST_ASSERT_EQUAL_INT(100, cmp_ent_get_cmp_data_size(ent));

	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_original_size(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_cmp_mode(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_model_value(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_reserved(ent));
	TEST_ASSERT_EQUAL_INT(0, cmp_ent_get_lossy_cmp_par(ent));
	free(ent);

	/** error cases **/
	/* unknown data type */
	data_type = DATA_TYPE_UNKNOWN;
	raw_mode_flag = 1;
	cmp_size_byte = 100;
	size = cmp_ent_create(NULL, data_type, raw_mode_flag, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT32(0, size);
	data_type = (enum cmp_data_type)0xFFF; /* undefined data type */
	raw_mode_flag = 1;
	cmp_size_byte = 100;
	size = cmp_ent_create(NULL, data_type, raw_mode_flag, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT32(0, size);

	/* cmp_size_byte to high */
	data_type = DATA_TYPE_S_FX;
	raw_mode_flag = 0;
	cmp_size_byte = CMP_ENTITY_MAX_SIZE+1;
	size = cmp_ent_create(NULL, data_type, raw_mode_flag, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT32(0, size);
	cmp_size_byte = CMP_ENTITY_MAX_SIZE;
	size = cmp_ent_create(NULL, data_type, raw_mode_flag, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT32(0, size);
	/* this should work */
	cmp_size_byte = CMP_ENTITY_MAX_SIZE - NON_IMAGETTE_HEADER_SIZE;
	size = cmp_ent_create(NULL, data_type, raw_mode_flag, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT32(NON_IMAGETTE_HEADER_SIZE + cmp_size_byte, size);
}


/**
 * @test cmp_ent_create_timestamp
 */

void test_cmp_ent_create_timestamp(void)
{
#ifdef HAS_TIME_H
	uint64_t timestamp, timestamp1, timestamp2;
	struct timespec ts;
	const time_t EPOCH = 1577836800;

	ts.tv_sec = EPOCH;
	ts.tv_nsec = 0;
	timestamp1 = cmp_ent_create_timestamp(&ts);
	ts.tv_sec = EPOCH + 1;
	ts.tv_nsec = 15259;
	timestamp2 = cmp_ent_create_timestamp(&ts);
	TEST_ASSERT_EQUAL_HEX(0x10001, timestamp2-timestamp1);

	/* create a current time */
	timestamp = cmp_ent_create_timestamp(NULL);
	TEST_ASSERT_NOT_EQUAL_INT(0, timestamp);

#if !defined(_WIN32) && !defined(_WIN64)
	/* test my_timegm function */
	setenv("TZ", "/etc/localtime", 0);
	timestamp = cmp_ent_create_timestamp(NULL);
	TEST_ASSERT_NOT_EQUAL_INT(0, timestamp);
	TEST_ASSERT_EQUAL_STRING("/etc/localtime", getenv("TZ"));
	unsetenv("TZ");
#endif

	/* error cases */
	/* ts before epoch */
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	timestamp = cmp_ent_create_timestamp(&ts);
	TEST_ASSERT_EQUAL(0, timestamp);
#endif
}


/**
 * @test cmp_ent_print
 */

void test_cmp_ent_print(void)
{
	size_t size;
	struct cmp_entity *ent;

	uint32_t version_id = 42;
	uint64_t start_timestamp = 100;
	uint64_t end_timestamp = 200;
	uint16_t model_id = 12;
	uint8_t model_counter = 23;
	uint32_t data_type = DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE;
	uint32_t cmp_mode = CMP_MODE_MODEL_MULTI;
	uint32_t model_value_used = 11;
	uint32_t lossy_cmp_par_used = 2;
	uint32_t original_size = 18;
	uint32_t spill = MIN_IMA_SPILL;
	uint32_t golomb_par = MAX_IMA_GOLOMB_PAR;
	uint32_t ap1_spill = 555;
	uint32_t ap1_golomb_par = 14;
	uint32_t ap2_spill = 333;
	uint32_t ap2_golomb_par = 43;
	uint32_t cmp_size_byte = 60;
	uint8_t reserved = 42;

	size = cmp_ent_create(NULL, data_type, 0, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT(IMAGETTE_ADAPTIVE_HEADER_SIZE+60, size);
	ent = calloc(1, size); TEST_ASSERT_NOT_NULL(ent);
	size = cmp_ent_create(ent, data_type, 0, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT(IMAGETTE_ADAPTIVE_HEADER_SIZE+60, size);

	cmp_ent_set_version_id(ent, version_id);

	cmp_ent_set_original_size(ent, original_size);

	cmp_ent_set_start_timestamp(ent, start_timestamp);
	cmp_ent_set_end_timestamp(ent, end_timestamp);

	cmp_ent_set_cmp_mode(ent, cmp_mode);
	cmp_ent_set_model_value(ent, model_value_used);
	cmp_ent_set_model_id(ent, model_id);
	cmp_ent_set_model_counter(ent, model_counter);
	cmp_ent_set_reserved(ent, reserved);
	cmp_ent_set_lossy_cmp_par(ent, lossy_cmp_par_used);

	cmp_ent_set_ima_spill(ent, spill);
	cmp_ent_set_ima_golomb_par(ent, golomb_par);

	cmp_ent_set_ima_ap1_spill(ent, ap1_spill);
	cmp_ent_set_ima_ap1_golomb_par(ent, ap1_golomb_par);

	cmp_ent_set_ima_ap2_spill(ent, ap2_spill);
	cmp_ent_set_ima_ap2_golomb_par(ent, ap2_golomb_par);


	cmp_ent_print(ent);

	/* error case */
	cmp_ent_print(NULL);

	free(ent);
}


/**
 * @test cmp_ent_parse
 */

void test_cmp_ent_parse(void)
{
	size_t size;
	struct cmp_entity *ent;

	uint32_t version_id = 42;
	uint64_t start_timestamp = 100;
	uint64_t end_timestamp = 200;
	uint16_t model_id = 12;
	uint8_t model_counter = 23;
	uint32_t data_type = DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE;
	uint32_t cmp_mode = CMP_MODE_MODEL_MULTI;
	uint32_t model_value_used = 11;
	uint32_t lossy_cmp_par_used = 2;
	uint32_t original_size = 18;
	uint32_t spill = MIN_IMA_SPILL;
	uint32_t golomb_par = MAX_IMA_GOLOMB_PAR;
	uint32_t ap1_spill = 555;
	uint32_t ap1_golomb_par = 14;
	uint32_t ap2_spill = 333;
	uint32_t ap2_golomb_par = 43;
	uint32_t cmp_size_byte = 60;
	uint8_t resvered = 42;

	size = cmp_ent_create(NULL, data_type, 0, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT(IMAGETTE_ADAPTIVE_HEADER_SIZE+60, size);
	ent = calloc(1, size); TEST_ASSERT_NOT_NULL(ent);
	size = cmp_ent_create(ent, data_type, 0, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT(IMAGETTE_ADAPTIVE_HEADER_SIZE+60, size);

	cmp_ent_set_version_id(ent, version_id);

	cmp_ent_set_original_size(ent, original_size);

	cmp_ent_set_start_timestamp(ent, start_timestamp);
	cmp_ent_set_end_timestamp(ent, end_timestamp);

	cmp_ent_set_cmp_mode(ent, cmp_mode);
	cmp_ent_set_model_value(ent, model_value_used);
	cmp_ent_set_model_id(ent, model_id);
	cmp_ent_set_model_counter(ent, model_counter);
	cmp_ent_set_reserved(ent, resvered);
	cmp_ent_set_lossy_cmp_par(ent, lossy_cmp_par_used);

	cmp_ent_set_ima_spill(ent, spill);
	cmp_ent_set_ima_golomb_par(ent, golomb_par);

	cmp_ent_set_ima_ap1_spill(ent, ap1_spill);
	cmp_ent_set_ima_ap1_golomb_par(ent, ap1_golomb_par);

	cmp_ent_set_ima_ap2_spill(ent, ap2_spill);
	cmp_ent_set_ima_ap2_golomb_par(ent, ap2_golomb_par);

	cmp_ent_parse(ent);


	data_type = DATA_TYPE_IMAGETTE;
	size = cmp_ent_create(ent, data_type, 0, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT(IMAGETTE_HEADER_SIZE+60, size);

	cmp_ent_parse(ent);


	data_type = DATA_TYPE_IMAGETTE;
	cmp_mode = CMP_MODE_RAW;
	version_id = 0x800F0003;
	size = cmp_ent_create(ent, data_type, cmp_mode == CMP_MODE_RAW, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT(GENERIC_HEADER_SIZE+60, size);
	cmp_ent_set_version_id(ent, version_id);
	cmp_ent_set_cmp_mode(ent, cmp_mode);

	cmp_ent_parse(ent);


	data_type = DATA_TYPE_CHUNK;
	cmp_mode = CMP_MODE_MODEL_ZERO;
	cmp_ent_set_cmp_mode(ent, cmp_mode);
	size = cmp_ent_create(ent, data_type, cmp_mode == CMP_MODE_RAW, cmp_size_byte);
	TEST_ASSERT_EQUAL_UINT(NON_IMAGETTE_HEADER_SIZE+60, size);

	cmp_ent_parse(ent);

	/* unknown data product type */
	cmp_ent_set_data_type(ent, DATA_TYPE_UNKNOWN, 0);
	cmp_ent_parse(ent);

	free(ent);
}

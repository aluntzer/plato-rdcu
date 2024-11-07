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
#include <string.h>
#include <stdlib.h>

#include <unity.h>
#include <cmp_data_types.h>


/**
 * @test cmp_collection_get_subservie
 */

void test_cmp_col_get_subservice(void)
{
	struct collection_hdr col;

	memset(&col, 0, sizeof(struct collection_hdr));
	memset(&col.collection_id, 0x7E, 1); /* set all bit of the subservice field */

	TEST_ASSERT_EQUAL(0x3F, cmp_col_get_subservice(&col));
	cmp_col_set_subservice(&col, 0x35);
	TEST_ASSERT_EQUAL(0x35, cmp_col_get_subservice(&col));
}

void test_cmp_col_get_and_set(void)
{
	int err;
	size_t i;
	struct collection_hdr *col = malloc(sizeof(struct collection_hdr));
	uint8_t *u8_p = (uint8_t *)col;
	uint64_t timestamp;
	uint16_t configuration_id, collection_id, collection_length;
	uint8_t pkt_type, subservice, ccd_id, sequence_num;

	TEST_ASSERT_TRUE(col);

	memset(col, 0, sizeof(struct collection_hdr));

	err = cmp_col_set_timestamp(col, 0x000102030405);
	TEST_ASSERT_FALSE(err);
	timestamp = cmp_col_get_timestamp(col);
	TEST_ASSERT_EQUAL(0x000102030405, timestamp);
	/* error cases */
	err = cmp_col_set_timestamp(NULL, 0x000102030405);
	TEST_ASSERT_TRUE(err);
	err = cmp_col_set_timestamp(col, 0x1000000000000);
	TEST_ASSERT_TRUE(err);

	err = cmp_col_set_configuration_id(col, 0x0607);
	TEST_ASSERT_FALSE(err);
	configuration_id = cmp_col_get_configuration_id(col);
	TEST_ASSERT_EQUAL_HEX16(0x0607, configuration_id);
	/* error cases */
	err = cmp_col_set_configuration_id(NULL, 0x0607);
	TEST_ASSERT_TRUE(err);


	err = cmp_col_set_pkt_type(col, 1);
	TEST_ASSERT_FALSE(err);
	pkt_type = cmp_col_get_pkt_type(col);
	TEST_ASSERT_EQUAL_HEX16(pkt_type, pkt_type);
	TEST_ASSERT_EQUAL_HEX16(0x8000, cmp_col_get_col_id(col));
	/* error cases */
	err = cmp_col_set_pkt_type(col, 2);
	TEST_ASSERT_TRUE(err);
	err = cmp_col_set_pkt_type(NULL, 1);
	TEST_ASSERT_TRUE(err);
	err = cmp_col_set_pkt_type(col, 0);
	TEST_ASSERT_FALSE(err);

	err = cmp_col_set_subservice(col, 0x3F);
	TEST_ASSERT_FALSE(err);
	subservice = cmp_col_get_subservice(col);
	TEST_ASSERT_EQUAL_HEX16(subservice, subservice);
	TEST_ASSERT_EQUAL_HEX16(0x7E00, cmp_col_get_col_id(col));
	/* error cases */
	err = cmp_col_set_subservice(col, 0x40);
	TEST_ASSERT_TRUE(err);
	err = cmp_col_set_subservice(NULL, 0x3F);
	TEST_ASSERT_TRUE(err);
	err = cmp_col_set_subservice(col, 0);
	TEST_ASSERT_FALSE(err);

	err = cmp_col_set_ccd_id(col, 0x3);
	TEST_ASSERT_FALSE(err);
	ccd_id = cmp_col_get_ccd_id(col);
	TEST_ASSERT_EQUAL_HEX16(ccd_id, ccd_id);
	TEST_ASSERT_EQUAL_HEX16(0x0180, cmp_col_get_col_id(col));
	/* error cases */
	err = cmp_col_set_ccd_id(col, 0x4);
	TEST_ASSERT_TRUE(err);
	err = cmp_col_set_ccd_id(NULL, 0x3);
	TEST_ASSERT_TRUE(err);
	err = cmp_col_set_ccd_id(col, 0);
	TEST_ASSERT_FALSE(err);

	err = cmp_col_set_sequence_num(col, 0x7F);
	TEST_ASSERT_FALSE(err);
	sequence_num = cmp_col_get_sequence_num(col);
	TEST_ASSERT_EQUAL_HEX16(0x7F, sequence_num);
	TEST_ASSERT_EQUAL_HEX16(0x007F, cmp_col_get_col_id(col));
	/* error cases */
	err = cmp_col_set_sequence_num(col, 0x80);
	TEST_ASSERT_TRUE(err);
	err = cmp_col_set_sequence_num(NULL, 0x7F);
	TEST_ASSERT_TRUE(err);


	err = cmp_col_set_col_id(col, 0x0809);
	TEST_ASSERT_FALSE(err);
	collection_id = cmp_col_get_col_id(col);
	TEST_ASSERT_EQUAL_HEX16(0x0809, collection_id);
	/* error cases */
	err = cmp_col_set_col_id(NULL, 0x0809);
	TEST_ASSERT_TRUE(err);

	err = cmp_col_set_data_length(col, 0x0A0B);
	TEST_ASSERT_FALSE(err);
	collection_length = cmp_col_get_data_length(col);
	TEST_ASSERT_EQUAL_HEX16(0x0A0B, collection_length);
	/* error cases */
	err = cmp_col_set_data_length(NULL, 0x0A0B);
	TEST_ASSERT_TRUE(err);

	for (i = 0; i < sizeof(struct collection_hdr); i++) {
		TEST_ASSERT_EQUAL_HEX8(i, u8_p[i]);
	}
	free(col);

}


/**
 * @test size_of_a_sample
 */

void test_size_of_a_sample(void)
{
	size_t size;

	/* test error cases */
	size = size_of_a_sample(DATA_TYPE_UNKNOWN);
	TEST_ASSERT_EQUAL(0, size);
	size = size_of_a_sample(DATA_TYPE_F_CAM_BACKGROUND+1);
	TEST_ASSERT_EQUAL(0, size);

	TEST_ASSERT_EQUAL(sizeof(uint16_t), size_of_a_sample(DATA_TYPE_IMAGETTE));
	TEST_ASSERT_EQUAL(sizeof(uint16_t), size_of_a_sample(DATA_TYPE_IMAGETTE_ADAPTIVE));
	TEST_ASSERT_EQUAL(sizeof(uint16_t), size_of_a_sample(DATA_TYPE_SAT_IMAGETTE));
	TEST_ASSERT_EQUAL(sizeof(uint16_t), size_of_a_sample(DATA_TYPE_SAT_IMAGETTE_ADAPTIVE));
	TEST_ASSERT_EQUAL(sizeof(struct offset), size_of_a_sample(DATA_TYPE_OFFSET));
	TEST_ASSERT_EQUAL(sizeof(struct background), size_of_a_sample(DATA_TYPE_BACKGROUND));
	TEST_ASSERT_EQUAL(sizeof(struct smearing), size_of_a_sample(DATA_TYPE_SMEARING));
	TEST_ASSERT_EQUAL(sizeof(struct s_fx), size_of_a_sample(DATA_TYPE_S_FX));
	TEST_ASSERT_EQUAL(sizeof(struct s_fx_efx), size_of_a_sample(DATA_TYPE_S_FX_EFX));
	TEST_ASSERT_EQUAL(sizeof(struct s_fx_ncob), size_of_a_sample(DATA_TYPE_S_FX_NCOB));
	TEST_ASSERT_EQUAL(sizeof(struct s_fx_efx_ncob_ecob), size_of_a_sample(DATA_TYPE_S_FX_EFX_NCOB_ECOB));
	TEST_ASSERT_EQUAL(sizeof(struct l_fx), size_of_a_sample(DATA_TYPE_L_FX));
	TEST_ASSERT_EQUAL(sizeof(struct l_fx_efx), size_of_a_sample(DATA_TYPE_L_FX_EFX));
	TEST_ASSERT_EQUAL(sizeof(struct l_fx_ncob), size_of_a_sample(DATA_TYPE_L_FX_NCOB));
	TEST_ASSERT_EQUAL(sizeof(struct l_fx_efx_ncob_ecob), size_of_a_sample(DATA_TYPE_L_FX_EFX_NCOB_ECOB));
	TEST_ASSERT_EQUAL(sizeof(struct f_fx), size_of_a_sample(DATA_TYPE_F_FX));
	TEST_ASSERT_EQUAL(sizeof(struct f_fx_efx), size_of_a_sample(DATA_TYPE_F_FX_EFX));
	TEST_ASSERT_EQUAL(sizeof(struct f_fx_ncob), size_of_a_sample(DATA_TYPE_F_FX_NCOB));
	TEST_ASSERT_EQUAL(sizeof(struct f_fx_efx_ncob_ecob), size_of_a_sample(DATA_TYPE_F_FX_EFX_NCOB_ECOB));
	TEST_ASSERT_EQUAL(sizeof(uint16_t), size_of_a_sample(DATA_TYPE_F_CAM_IMAGETTE));
	TEST_ASSERT_EQUAL(sizeof(uint16_t), size_of_a_sample(DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE));
	TEST_ASSERT_EQUAL(sizeof(struct offset), size_of_a_sample(DATA_TYPE_F_CAM_OFFSET));
	TEST_ASSERT_EQUAL(sizeof(struct background), size_of_a_sample(DATA_TYPE_F_CAM_BACKGROUND));
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
	TEST_ASSERT_EQUAL_UINT(sizeof(struct f_fx)+COLLECTION_HDR_SIZE, s);

	s = cmp_cal_size_of_data(4, DATA_TYPE_F_FX);
	TEST_ASSERT_EQUAL_UINT(4*sizeof(struct f_fx)+COLLECTION_HDR_SIZE, s);

	/* error cases */
	s = cmp_cal_size_of_data(33, DATA_TYPE_UNKNOWN);
	TEST_ASSERT_EQUAL_UINT(0, s);

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
	size = COLLECTION_HDR_SIZE - 1;
	samples_get = cmp_input_size_to_samples(size, data_type);
	TEST_ASSERT_EQUAL(-1, samples_get);
	data_type = DATA_TYPE_S_FX_NCOB;

	size = COLLECTION_HDR_SIZE + 4*sizeof(struct s_fx_ncob) - 1;
	samples_get = cmp_input_size_to_samples(size, data_type);
	TEST_ASSERT_EQUAL(-1, samples_get);
}


static void check_endianness(void* data, size_t size, enum cmp_data_type data_type)
{
	int error;
	uint8_t *p_8 = data;
	size_t i;

	TEST_ASSERT_TRUE(size > COLLECTION_HDR_SIZE);

	error = cmp_input_big_to_cpu_endianness(data, (uint32_t)size, data_type);
	TEST_ASSERT_FALSE(error);

	for (i = 0; i < COLLECTION_HDR_SIZE; i++)
		TEST_ASSERT_EQUAL(0, p_8[i]);

	for (i = 0; i < size-COLLECTION_HDR_SIZE; i++)
		TEST_ASSERT_EQUAL((uint8_t)i, p_8[COLLECTION_HDR_SIZE+i]);
}


/**
 * @test cmp_input_big_to_cpu_endianness
 */

void test_cmp_input_big_to_cpu_endianness(void)
{
	enum cmp_data_type data_type;

	{
		int error;
		uint16_t data[2] = {0x0001, 0x0203};
		uint8_t data_cmp[4] = {0x00, 0x01, 0x02, 0x03};

		data_type = DATA_TYPE_SAT_IMAGETTE;

		error = cmp_input_big_to_cpu_endianness(data, sizeof(data), data_type);
		TEST_ASSERT_FALSE(error);
		TEST_ASSERT_EQUAL_MEMORY(data, data_cmp, sizeof(data_cmp));
	}
	{
		struct {
			uint8_t hdr[COLLECTION_HDR_SIZE];
			struct offset entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_OFFSET;

		data.entry[0].mean     = 0x00010203;
		data.entry[0].variance = 0x04050607;
		data.entry[1].mean     = 0x08090A0B;
		data.entry[1].variance = 0x0C0D0E0F;

		check_endianness(&data, sizeof(data), data_type);

		data_type = DATA_TYPE_F_CAM_OFFSET;

		data.entry[0].mean     = 0x00010203;
		data.entry[0].variance = 0x04050607;
		data.entry[1].mean     = 0x08090A0B;
		data.entry[1].variance = 0x0C0D0E0F;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[COLLECTION_HDR_SIZE];
			struct background entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_BACKGROUND;

		data.entry[0].mean           = 0x00010203;
		data.entry[0].variance       = 0x04050607;
		data.entry[0].outlier_pixels = 0x0809;
		data.entry[1].mean           = 0x0A0B0C0D;
		data.entry[1].variance       = 0x0E0F1011;
		data.entry[1].outlier_pixels = 0x1213;

		check_endianness(&data, sizeof(data), data_type);

		data_type = DATA_TYPE_F_CAM_BACKGROUND;

		data.entry[0].mean           = 0x00010203;
		data.entry[0].variance       = 0x04050607;
		data.entry[0].outlier_pixels = 0x0809;
		data.entry[1].mean           = 0x0A0B0C0D;
		data.entry[1].variance       = 0x0E0F1011;
		data.entry[1].outlier_pixels = 0x1213;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[COLLECTION_HDR_SIZE];
			struct smearing entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_SMEARING;

		data.entry[0].mean           = 0x00010203;
		data.entry[0].variance_mean  = 0x0405;
		data.entry[0].outlier_pixels = 0x0607;
		data.entry[1].mean           = 0x08090A0B;
		data.entry[1].variance_mean  = 0x0C0D;
		data.entry[1].outlier_pixels = 0x0E0F;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[COLLECTION_HDR_SIZE];
			struct s_fx entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_S_FX;

		data.entry[0].exp_flags = 0x00;
		data.entry[0].fx        = 0x01020304;
		data.entry[1].exp_flags = 0x05;
		data.entry[1].fx        = 0x06070809;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[COLLECTION_HDR_SIZE];
			struct s_fx_efx entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_S_FX_EFX;

		data.entry[0].exp_flags = 0x00;
		data.entry[0].fx        = 0x01020304;
		data.entry[0].efx       = 0x05060708;
		data.entry[1].exp_flags = 0x09;
		data.entry[1].fx        = 0x0A0B0C0D;
		data.entry[1].efx       = 0x0E0F1011;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[COLLECTION_HDR_SIZE];
			struct s_fx_ncob entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_S_FX_NCOB;

		data.entry[0].exp_flags = 0x00;
		data.entry[0].fx        = 0x01020304;
		data.entry[0].ncob_x    = 0x05060708;
		data.entry[0].ncob_y    = 0x090A0B0C;
		data.entry[1].exp_flags = 0x0D;
		data.entry[1].fx        = 0x0E0F1011;
		data.entry[1].ncob_x    = 0x12131415;
		data.entry[1].ncob_y    = 0x16171819;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[COLLECTION_HDR_SIZE];
			struct s_fx_efx_ncob_ecob entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_S_FX_EFX_NCOB_ECOB;

		data.entry[0].exp_flags = 0x00;
		data.entry[0].fx        = 0x01020304;
		data.entry[0].ncob_x    = 0x05060708;
		data.entry[0].ncob_y    = 0x090A0B0C;
		data.entry[0].efx       = 0x0D0E0F10;
		data.entry[0].ecob_x    = 0x11121314;
		data.entry[0].ecob_y    = 0x15161718;
		data.entry[1].exp_flags = 0x19;
		data.entry[1].fx        = 0x1A1B1C1D;
		data.entry[1].ncob_x    = 0x1E1F2021;
		data.entry[1].ncob_y    = 0x22232425;
		data.entry[1].efx       = 0x26272829;
		data.entry[1].ecob_x    = 0x2A2B2C2D;
		data.entry[1].ecob_y    = 0x2E2F3031;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[COLLECTION_HDR_SIZE];
			struct f_fx entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_F_FX;

		data.entry[0].fx = 0x00010203;
		data.entry[1].fx = 0x04050607;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[COLLECTION_HDR_SIZE];
			struct f_fx_efx entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_F_FX_EFX;

		data.entry[0].fx  = 0x00010203;
		data.entry[0].efx = 0x04050607;
		data.entry[1].fx  = 0x08090A0B;
		data.entry[1].efx = 0x0C0D0E0F;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[COLLECTION_HDR_SIZE];
			struct f_fx_ncob entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_F_FX_NCOB;

		data.entry[0].fx     = 0x00010203;
		data.entry[0].ncob_x = 0x04050607;
		data.entry[0].ncob_y = 0x08090A0B;
		data.entry[1].fx     = 0x0C0D0E0F;
		data.entry[1].ncob_x = 0x10111213;
		data.entry[1].ncob_y = 0x14151617;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[COLLECTION_HDR_SIZE];
			struct f_fx_efx_ncob_ecob entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_F_FX_EFX_NCOB_ECOB;

		data.entry[0].fx     = 0x00010203;
		data.entry[0].ncob_x = 0x04050607;
		data.entry[0].ncob_y = 0x08090A0B;
		data.entry[0].efx    = 0x0C0D0E0F;
		data.entry[0].ecob_x = 0x10111213;
		data.entry[0].ecob_y = 0x14151617;
		data.entry[1].fx     = 0x18191A1B;
		data.entry[1].ncob_x = 0x1C1D1E1F;
		data.entry[1].ncob_y = 0x20212223;
		data.entry[1].efx    = 0x24252627;
		data.entry[1].ecob_x = 0x28292A2B;
		data.entry[1].ecob_y = 0x2C2D2E2F;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[COLLECTION_HDR_SIZE];
			struct l_fx entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_L_FX;

		data.entry[0].exp_flags   = 0x000102;
		data.entry[0].fx          = 0x03040506;
		data.entry[0].fx_variance = 0x0708090A;
		data.entry[1].exp_flags   = 0x0B0C0D;
		data.entry[1].fx          = 0x0E0F1011;
		data.entry[1].fx_variance = 0x12131415;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[COLLECTION_HDR_SIZE];
			struct l_fx_efx entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_L_FX_EFX;

		data.entry[0].exp_flags   = 0x000102;
		data.entry[0].fx          = 0x03040506;
		data.entry[0].efx         = 0x0708090A;
		data.entry[0].fx_variance = 0x0B0C0D0E;
		data.entry[1].exp_flags   = 0x0F1011;
		data.entry[1].fx          = 0x12131415;
		data.entry[1].efx         = 0x16171819;
		data.entry[1].fx_variance = 0x1A1B1C1D;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[COLLECTION_HDR_SIZE];
			struct l_fx_ncob entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_L_FX_NCOB;

		data.entry[0].exp_flags      = 0x000102;
		data.entry[0].fx             = 0x03040506;
		data.entry[0].ncob_x         = 0x0708090A;
		data.entry[0].ncob_y         = 0x0B0C0D0E;
		data.entry[0].fx_variance    = 0x0F101112;
		data.entry[0].cob_x_variance = 0x13141516;
		data.entry[0].cob_y_variance = 0x1718191A;
		data.entry[1].exp_flags      = 0x1B1C1D;
		data.entry[1].fx             = 0x1E1F2021;
		data.entry[1].ncob_x         = 0x22232425;
		data.entry[1].ncob_y         = 0x26272829;
		data.entry[1].fx_variance    = 0x2A2B2C2D;
		data.entry[1].cob_x_variance = 0x2E2F3031;
		data.entry[1].cob_y_variance = 0x32333435;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[COLLECTION_HDR_SIZE];
			struct l_fx_efx_ncob_ecob entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_L_FX_EFX_NCOB_ECOB;

		data.entry[0].exp_flags      = 0x000102;
		data.entry[0].fx             = 0x03040506;
		data.entry[0].ncob_x         = 0x0708090A;
		data.entry[0].ncob_y         = 0x0B0C0D0E;
		data.entry[0].efx            = 0x0F101112;
		data.entry[0].ecob_x         = 0x13141516;
		data.entry[0].ecob_y         = 0x1718191A;
		data.entry[0].fx_variance    = 0x1B1C1D1E;
		data.entry[0].cob_x_variance = 0x1F202122;
		data.entry[0].cob_y_variance = 0x23242526;
		data.entry[1].exp_flags      = 0x272829;
		data.entry[1].fx             = 0x2A2B2C2D;
		data.entry[1].ncob_x         = 0x2E2F3031;
		data.entry[1].ncob_y         = 0x32333435;
		data.entry[1].efx            = 0x36373839;
		data.entry[1].ecob_x         = 0x3A3B3C3D;
		data.entry[1].ecob_y         = 0x3E3F4041;
		data.entry[1].fx_variance    = 0x42434445;
		data.entry[1].cob_x_variance = 0x46474849;
		data.entry[1].cob_y_variance = 0x4A4B4C4D;

		check_endianness(&data, sizeof(data), data_type);
	}
}


/**
 * @test cmp_input_big_to_cpu_endianness
 */

void test_cmp_input_big_to_cpu_endianness_error_cases(void)
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

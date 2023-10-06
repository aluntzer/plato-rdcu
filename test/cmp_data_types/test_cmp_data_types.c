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


	TEST_ASSERT_EQUAL(sizeof(uint16_t), size_of_a_sample(DATA_TYPE_IMAGETTE));
	TEST_ASSERT_EQUAL(sizeof(uint16_t), size_of_a_sample(DATA_TYPE_IMAGETTE_ADAPTIVE));
	TEST_ASSERT_EQUAL(sizeof(uint16_t), size_of_a_sample(DATA_TYPE_SAT_IMAGETTE));
	TEST_ASSERT_EQUAL(sizeof(uint16_t), size_of_a_sample(DATA_TYPE_SAT_IMAGETTE_ADAPTIVE));
	TEST_ASSERT_EQUAL(sizeof(struct nc_offset), size_of_a_sample(DATA_TYPE_OFFSET));
	TEST_ASSERT_EQUAL(sizeof(struct nc_background), size_of_a_sample(DATA_TYPE_BACKGROUND));
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
	/* TODO: TEST_ASSERT_EQUAL(sizeof(struct ), size_of_a_sample(DATA_TYPE_F_CAM_OFFSET)); */
	/* TODO: TEST_ASSERT_EQUAL(sizeof(struct ), size_of_a_sample(DATA_TYPE_F_CAM_BACKGROUND */
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
	size = MULTI_ENTRY_HDR_SIZE - 1;
	samples_get = cmp_input_size_to_samples(size, data_type);
	TEST_ASSERT_EQUAL(-1, samples_get);
	data_type = DATA_TYPE_S_FX_NCOB;

	size = MULTI_ENTRY_HDR_SIZE + 4*sizeof(struct s_fx_ncob) - 1;
	samples_get = cmp_input_size_to_samples(size, data_type);
	TEST_ASSERT_EQUAL(-1, samples_get);
}


static void check_endianness(void* data, size_t size, enum cmp_data_type data_type)
{
	int error;
	uint8_t *p_8 = data;
	size_t i;

	TEST_ASSERT_TRUE(size > MULTI_ENTRY_HDR_SIZE);

	error = cmp_input_big_to_cpu_endianness(data, (uint32_t)size, data_type);
	TEST_ASSERT_FALSE(error);

	for (i = 0; i < MULTI_ENTRY_HDR_SIZE; i++)
		TEST_ASSERT_EQUAL(0, p_8[i]);

	for (i = 0; i < size-MULTI_ENTRY_HDR_SIZE; i++)
		TEST_ASSERT_EQUAL((uint8_t)i, p_8[MULTI_ENTRY_HDR_SIZE+i]);
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
			uint8_t hdr[MULTI_ENTRY_HDR_SIZE];
			struct nc_offset entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_OFFSET;

		data.entry[0].mean     = 0x00010203;
		data.entry[0].variance = 0x04050607;
		data.entry[1].mean     = 0x08090A0B;
		data.entry[1].variance = 0x0C0D0E0F;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[MULTI_ENTRY_HDR_SIZE];
			struct nc_background entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_BACKGROUND;

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
			uint8_t hdr[MULTI_ENTRY_HDR_SIZE];
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
			uint8_t hdr[MULTI_ENTRY_HDR_SIZE];
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
			uint8_t hdr[MULTI_ENTRY_HDR_SIZE];
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
			uint8_t hdr[MULTI_ENTRY_HDR_SIZE];
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
			uint8_t hdr[MULTI_ENTRY_HDR_SIZE];
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
			uint8_t hdr[MULTI_ENTRY_HDR_SIZE];
			struct f_fx entry[2];
		} __attribute__((packed)) data = {0};

		data_type = DATA_TYPE_F_FX;

		data.entry[0].fx = 0x00010203;
		data.entry[1].fx = 0x04050607;

		check_endianness(&data, sizeof(data), data_type);
	}
	{
		struct {
			uint8_t hdr[MULTI_ENTRY_HDR_SIZE];
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
			uint8_t hdr[MULTI_ENTRY_HDR_SIZE];
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
			uint8_t hdr[MULTI_ENTRY_HDR_SIZE];
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
			uint8_t hdr[MULTI_ENTRY_HDR_SIZE];
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
			uint8_t hdr[MULTI_ENTRY_HDR_SIZE];
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
			uint8_t hdr[MULTI_ENTRY_HDR_SIZE];
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
			uint8_t hdr[MULTI_ENTRY_HDR_SIZE];
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

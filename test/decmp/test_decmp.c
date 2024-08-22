/**
 * @file   test_decmp.c
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
 * @brief decompression tests
 */


#include <string.h>
#include <stdlib.h>

#include <unity.h>

#include <compiler.h>
#include <cmp_entity.h>
#include <cmp_data_types.h>
#include "../../lib/icu_compress/cmp_icu.c" /* .c file included to test static functions */
#include "../../lib/decompress/decmp.c" /* .c file included to test static functions */

#define MAX_VALID_CW_LEM 32


/**
 * @test bit_init_decoder
 * @test bit_read_bits32
 * @test bit_read_bits
 * @test bit_refill
 */

void test_bitstream(void)
{
	uint8_t i, data[12];
	struct bit_decoder dec;
	size_t ret;
	int status;
	uint32_t read_bits;

	for (i = 0; i < sizeof(data); ++i)
		data[i] = i;

	ret = bit_init_decoder(&dec, data, sizeof(data));
	TEST_ASSERT_EQUAL_size_t(sizeof(data), ret);

	read_bits = bit_read_bits32(&dec, 31);
	TEST_ASSERT_EQUAL_HEX32(0x00010203>>1, read_bits);
	TEST_ASSERT_EQUAL_INT(31, dec.bits_consumed);

	status = bit_refill(&dec);
	TEST_ASSERT_EQUAL_INT(BIT_UNFINISHED, status);
	TEST_ASSERT_EQUAL_INT(7, dec.bits_consumed);
	status = bit_refill(&dec);
	TEST_ASSERT_EQUAL_INT(BIT_UNFINISHED, status);
	TEST_ASSERT_EQUAL_INT(7, dec.bits_consumed);
	TEST_ASSERT_FALSE(bit_end_of_stream(&dec));

	read_bits = bit_read_bits32(&dec, 32);
	TEST_ASSERT_EQUAL_HEX32(0x82028303, read_bits);
	TEST_ASSERT_EQUAL_INT(39, dec.bits_consumed);
	read_bits = bit_read_bits32(&dec, 1);
	TEST_ASSERT_EQUAL_HEX32(1, read_bits);
	TEST_ASSERT_EQUAL_INT(40, dec.bits_consumed);

	status = bit_refill(&dec);
	TEST_ASSERT_EQUAL_INT(BIT_END_OF_BUFFER, status);
	TEST_ASSERT_EQUAL_INT(32, dec.bits_consumed);
	status = bit_refill(&dec);
	TEST_ASSERT_EQUAL_INT(BIT_END_OF_BUFFER, status);
	TEST_ASSERT_EQUAL_INT(32, dec.bits_consumed);
	TEST_ASSERT_FALSE(bit_end_of_stream(&dec));

	read_bits = bit_read_bits32(&dec, 32);
	TEST_ASSERT_EQUAL_HEX32(0x08090A0B, read_bits);
	status = bit_refill(&dec);
	TEST_ASSERT_EQUAL_INT(BIT_ALL_READ_IN, status);
	status = bit_refill(&dec);
	TEST_ASSERT_EQUAL_INT(BIT_ALL_READ_IN, status);
	TEST_ASSERT_TRUE(bit_end_of_stream(&dec));

	bit_read_bits32(&dec, 1);
	status = bit_refill(&dec);
	TEST_ASSERT_EQUAL_INT(BIT_OVERFLOW, status);
	status = bit_refill(&dec);
	TEST_ASSERT_EQUAL_INT(BIT_OVERFLOW, status);
	TEST_ASSERT_FALSE(bit_end_of_stream(&dec));

	bit_read_bits(&dec, 57);
	status = bit_refill(&dec);
	TEST_ASSERT_EQUAL_INT(BIT_OVERFLOW, status);
	bit_read_bits(&dec, 57);
	bit_read_bits(&dec, 57);
	bit_read_bits(&dec, 57);
	bit_read_bits(&dec, 57);
	TEST_ASSERT_EQUAL_INT(BIT_OVERFLOW, status);

	{
		uint8_t k, j;
		uint8_t buf[9];
		size_t s;

		for (k = 0; k < 8; k++) {
			memset(data, 0, sizeof(data));
			for (j = 0; j < k; j++)
				buf[j] = j;
			s = bit_init_decoder(&dec, buf, j);
			TEST_ASSERT_EQUAL_size_t(j, s);
			for (j = 0; j < k; j++)
				TEST_ASSERT_EQUAL_UINT(j, bit_read_bits(&dec, 8));
			TEST_ASSERT_EQUAL(1, bit_end_of_stream(&dec));
			TEST_ASSERT_EQUAL_INT(BIT_ALL_READ_IN, bit_refill(&dec));
		}
	}
}


/**
 * @test unary_decoder
 */

void test_unary_decoder(void)
{
	uint32_t leading_ones;
	struct bit_decoder dec;
	uint32_t unused_1 = 0;
	uint32_t unused_2 = 0;
	uint32_t value;
	size_t ret;


	value = 0;
	ret = bit_init_decoder(&dec, &value, sizeof(value));
	TEST_ASSERT_EQUAL_size_t(sizeof(value), ret);
	leading_ones = unary_decoder(&dec, unused_1, unused_2);
	TEST_ASSERT_EQUAL_INT(0, leading_ones);
	leading_ones = unary_decoder(&dec, unused_1, unused_2);
	TEST_ASSERT_EQUAL_INT(0, leading_ones);
	TEST_ASSERT_EQUAL_INT(BIT_END_OF_BUFFER, bit_refill(&dec));

	value = be32_to_cpu(0x7FFFFFFF);
	ret = bit_init_decoder(&dec, &value, sizeof(value));
	TEST_ASSERT_EQUAL_size_t(sizeof(value), ret);
	leading_ones = unary_decoder(&dec, unused_1, unused_2);
	TEST_ASSERT_EQUAL_INT(0, leading_ones);
	leading_ones = unary_decoder(&dec, unused_1, unused_2);
	TEST_ASSERT_EQUAL_INT(31, leading_ones);
	TEST_ASSERT_EQUAL_INT(BIT_OVERFLOW, bit_refill(&dec));

	value = be32_to_cpu(0x80000000);
	ret = bit_init_decoder(&dec, &value, sizeof(value));
	TEST_ASSERT_EQUAL_size_t(sizeof(value), ret);
	leading_ones = unary_decoder(&dec, unused_1, unused_2);
	TEST_ASSERT_EQUAL_INT(1, leading_ones);
	TEST_ASSERT_EQUAL_INT(BIT_END_OF_BUFFER, bit_refill(&dec));
	leading_ones = unary_decoder(&dec, unused_1, unused_2);
	TEST_ASSERT_EQUAL_INT(0, leading_ones);
	TEST_ASSERT_EQUAL_INT(BIT_END_OF_BUFFER, bit_refill(&dec));

	value = be32_to_cpu(0xBFFFFFFF);
	ret = bit_init_decoder(&dec, &value, sizeof(value));
	TEST_ASSERT_EQUAL_size_t(sizeof(value), ret);
	leading_ones = unary_decoder(&dec, unused_1, unused_2);
	TEST_ASSERT_EQUAL_INT(1, leading_ones);
	TEST_ASSERT_EQUAL_INT(BIT_END_OF_BUFFER, bit_refill(&dec));
	leading_ones = unary_decoder(&dec, unused_1, unused_2);
	TEST_ASSERT_EQUAL_INT(30, leading_ones);
	TEST_ASSERT_EQUAL_INT(BIT_OVERFLOW, bit_refill(&dec));

	value = be32_to_cpu(0xFFFF0000);
	ret = bit_init_decoder(&dec, &value, sizeof(value));
	TEST_ASSERT_EQUAL_size_t(sizeof(value), ret);
	leading_ones = unary_decoder(&dec, unused_1, unused_2);
	TEST_ASSERT_EQUAL_INT(16, leading_ones);
	TEST_ASSERT_EQUAL_INT(BIT_END_OF_BUFFER, bit_refill(&dec));

	value = be32_to_cpu(0xFFFF7FFF);
	ret = bit_init_decoder(&dec, &value, sizeof(value));
	TEST_ASSERT_EQUAL_size_t(sizeof(value), ret);
	leading_ones = unary_decoder(&dec, unused_1, unused_2);
	TEST_ASSERT_EQUAL_INT(16, leading_ones);
	TEST_ASSERT_EQUAL_INT(BIT_END_OF_BUFFER, bit_refill(&dec));

	value = be32_to_cpu(0xFFFFFFFE);
	ret = bit_init_decoder(&dec, &value, sizeof(value));
	TEST_ASSERT_EQUAL_size_t(sizeof(value), ret);
	leading_ones = unary_decoder(&dec, unused_1, unused_2);
	TEST_ASSERT_EQUAL_INT(31, leading_ones);
	TEST_ASSERT_EQUAL_INT(BIT_ALL_READ_IN, bit_refill(&dec));

	value = be32_to_cpu(0xFFFFFFFF);
	ret = bit_init_decoder(&dec, &value, sizeof(value));
	TEST_ASSERT_EQUAL_size_t(sizeof(value), ret);
	leading_ones = unary_decoder(&dec, unused_1, unused_2);
	TEST_ASSERT_EQUAL_INT(32, leading_ones);
	TEST_ASSERT_EQUAL_INT(BIT_OVERFLOW, bit_refill(&dec));

	{	uint64_t value64 = ~0ULL;

		ret = bit_init_decoder(&dec, &value64, sizeof(value64));
		TEST_ASSERT_EQUAL_size_t(sizeof(value64), ret);
		leading_ones = unary_decoder(&dec, unused_1, unused_2);
		TEST_ASSERT_EQUAL_INT(64, leading_ones);
		TEST_ASSERT_EQUAL_INT(BIT_OVERFLOW, bit_refill(&dec));

		value64 = be64_to_cpu(0xFFFFFFFF00000000);
		ret = bit_init_decoder(&dec, &value64, sizeof(value64));
		TEST_ASSERT_EQUAL_size_t(sizeof(value64), ret);
		leading_ones = unary_decoder(&dec, unused_1, unused_2);
		TEST_ASSERT_EQUAL_INT(32, leading_ones);
		/* TEST_ASSERT_EQUAL_INT(BIT_OVERFLOW, bit_refill(&dec)); */
	}
}


/**
 * @test rice_decoder
 */

void test_rice_decoder(void)
{
	struct bit_decoder dec;
	uint64_t bitstream;
	uint32_t m, log2_m, decoded_cw;
	size_t buf_size;

	/* log2_m = 0 test */
	log2_m = 0; m = 1U << log2_m;

	bitstream = 0;
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = unary_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(1, dec.bits_consumed);
	TEST_ASSERT_EQUAL(0, decoded_cw);

	bitstream = cpu_to_be64(0x7FFFFFFFFFFFFFFF); /* 0b0111...11 */
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = unary_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(1, dec.bits_consumed);
	TEST_ASSERT_EQUAL(0, decoded_cw);

	bitstream = cpu_to_be64(0x8000000000000000);
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = unary_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(2, dec.bits_consumed);
	TEST_ASSERT_EQUAL(1, decoded_cw);

	bitstream = cpu_to_be64(0xFFFFFFFE00000000);
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = unary_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed);
	TEST_ASSERT_EQUAL(31, decoded_cw);

	bitstream = cpu_to_be64(0xFFFFFFFFFFFFFFFE);
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = unary_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(64, dec.bits_consumed);
	TEST_ASSERT_EQUAL(63, decoded_cw);
	TEST_ASSERT_EQUAL_INT(BIT_ALL_READ_IN, bit_refill(&dec));

	bitstream = cpu_to_be64(0xFFFFFFFF00000000);
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = unary_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(33, dec.bits_consumed);
	TEST_ASSERT_EQUAL(32, decoded_cw);

	/* invalid code word (longer than 32 bit) */
	bitstream = cpu_to_be64(0xFFFFFFFFFFFFFFFF);
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = unary_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(65, dec.bits_consumed);
	TEST_ASSERT_EQUAL(64, decoded_cw);
	TEST_ASSERT_EQUAL_INT(BIT_OVERFLOW, bit_refill(&dec));

	/* log2_m = 1 test */
	log2_m = 1; m = 1U << log2_m;

	bitstream = 0;
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = rice_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(2, dec.bits_consumed);
	TEST_ASSERT_EQUAL(0, decoded_cw);

	bitstream = cpu_to_be64(0x4000000000000000);
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = rice_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(2, dec.bits_consumed);
	TEST_ASSERT_EQUAL(1, decoded_cw);

	bitstream = cpu_to_be64(0xFFFFFFFC00000000);
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = rice_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed);
	TEST_ASSERT_EQUAL(60, decoded_cw);

	bitstream = cpu_to_be64(0xFFFFFFFD00000000);
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = rice_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed);
	TEST_ASSERT_EQUAL(61, decoded_cw);

	/* invalid code word (longer than 32 bit) */
	bitstream = cpu_to_be64(0xFFFFFFFE00000000);
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = rice_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(33, dec.bits_consumed);
	TEST_ASSERT_EQUAL(62, decoded_cw);
	TEST_ASSERT_GREATER_THAN_UINT(MAX_VALID_CW_LEM, dec.bits_consumed);

	/* log2_m = 31 test */
	log2_m = 31; m = 1U << log2_m;

	bitstream = 0;
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = rice_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed);
	TEST_ASSERT_EQUAL(0, decoded_cw);

	bitstream = cpu_to_be64(0x0000000100000000);
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = rice_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed);
	TEST_ASSERT_EQUAL(1, decoded_cw);

	bitstream = cpu_to_be64(0x7FFFFFFE00000000);
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = rice_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed);
	TEST_ASSERT_EQUAL(0X7FFFFFFE, decoded_cw);

	bitstream = cpu_to_be64(0x7FFFFFFD00000000);
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	decoded_cw = rice_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed);
	TEST_ASSERT_EQUAL(0X7FFFFFFD, decoded_cw);

	/* invalid code word (longer than 32 bit) */
	bitstream = cpu_to_be64(0x8000000000000000);
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	TEST_ASSERT_EQUAL_size_t(sizeof(bitstream), buf_size);
	rice_decoder(&dec, m, log2_m);
	TEST_ASSERT_GREATER_THAN_UINT(MAX_VALID_CW_LEM, dec.bits_consumed);

#if 0
/* this case is prevented by an assert */

	/* test log_2 to big */
	log2_m = 32; m = 1 << log2_m;
	bitstream = 0x00000000;
	buf_size = bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = rice_decoder(&dec, m, log2_m);
	TEST_ASSERT_GREATER_THAN_UINT(MAX_VALID_CW_LEM, dec.bits_consumed);

	bitstream = cpu_to_be32(0xE000000000000000);
	log2_m = 33; m = 1 << log2_m;
	decoded_cw = rice_decoder(&dec, m, log2_m);
	TEST_ASSERT_GREATER_THAN_UINT(MAX_VALID_CW_LEM, dec.bits_consumed);


	log2_m = UINT_MAX; m = 1 << log2_m;
	decoded_cw = rice_decoder(&dec, m, log2_m);
	TEST_ASSERT_GREATER_THAN_UINT(MAX_VALID_CW_LEM, dec.bits_consumed);
#endif
}


/**
 * @test golomb_decoder
 */

void test_golomb_decoder(void)
{
	struct bit_decoder dec;
	uint32_t bitstream;
	unsigned int m;
	unsigned int log2_m;
	uint32_t decoded_cw;

	/* m = 1 test */
	m = 1;
	log2_m = ilog_2(m);

	bitstream = 0;
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(1, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(0, decoded_cw);

	bitstream = cpu_to_be32(0x7FFFFFFF); /* 0b0111...11 */
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(1, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(0, decoded_cw);

	bitstream = cpu_to_be32(0x80000000);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(2, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(1, decoded_cw);

	bitstream = cpu_to_be32(0xFFFFFFFE);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(31, decoded_cw);

	/* invalid code word (longer than 32 bit) */
	bitstream = 0xFFFFFFFF;
	golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_GREATER_THAN_UINT(MAX_VALID_CW_LEM, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL_INT(BIT_OVERFLOW, bit_refill(&dec));


	/* m = 2 test */
	m = 2;
	log2_m = ilog_2(m);

	bitstream = 0;
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(2, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(0, decoded_cw);

	bitstream = cpu_to_be32(0x40000000);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(2, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(1, decoded_cw);

	bitstream = cpu_to_be32(0xFFFFFFFC);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(60, decoded_cw);

	bitstream = cpu_to_be32(0xFFFFFFFD);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(61, decoded_cw);

	/* invalid code word (longer than 32 bit) */
	bitstream = cpu_to_be32(0xFFFFFFFE);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_GREATER_THAN_UINT(MAX_VALID_CW_LEM, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL_INT(BIT_OVERFLOW, bit_refill(&dec));


	/* m = 3 test */
	m = 3;
	log2_m = ilog_2(m);

	bitstream = 0;
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(2, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(0, decoded_cw);

	bitstream = cpu_to_be32(0x40000000);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(3, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(1, decoded_cw);

	bitstream = cpu_to_be32(0x60000000);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(3, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(2, decoded_cw);

	bitstream = cpu_to_be32(0x80000000);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(3, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(3, decoded_cw);

	bitstream = cpu_to_be32(0xA0000000);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(4, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(4, decoded_cw);

	bitstream = cpu_to_be32(0xFFFFFFFB);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(89, decoded_cw);

	bitstream = cpu_to_be32(0xFFFFFFFC);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(90, decoded_cw);

	/* invalid code word (longer than 32 bit) */
	bitstream = cpu_to_be32(0xFFFFFFFD);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_GREATER_THAN_UINT(MAX_VALID_CW_LEM, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL_INT(BIT_OVERFLOW, bit_refill(&dec));



	/* m = 0x7FFFFFFF test */
	m = 0x7FFFFFFF;
	log2_m = ilog_2(m);

	bitstream = 0;
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(31, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(0, decoded_cw);

	bitstream = cpu_to_be32(0x2);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(1, decoded_cw);

	bitstream = cpu_to_be32(0x7FFFFFFF);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(0x7FFFFFFE, decoded_cw);

	bitstream = cpu_to_be32(0x80000000);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(0x7FFFFFFF, decoded_cw);

	/* invalid code word (longer than 32 bit) */
	bitstream = cpu_to_be32(0X80000001);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_GREATER_THAN_UINT(MAX_VALID_CW_LEM, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL_INT(BIT_OVERFLOW, bit_refill(&dec));


	/* m = 0x80000000 test */
	m = 0x80000000;
	log2_m = ilog_2(m);

	bitstream = 0;
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(0, decoded_cw);

	bitstream = cpu_to_be32(1);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(1, decoded_cw);

	bitstream = cpu_to_be32(0x7FFFFFFE);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(0x7FFFFFFE, decoded_cw);

	bitstream = cpu_to_be32(0x7FFFFFFD);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(32, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL(0x7FFFFFFD, decoded_cw);

	/* invalid code word (longer than 32 bit) */
	bitstream = cpu_to_be32(0x80000000);
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_GREATER_THAN_UINT(MAX_VALID_CW_LEM, dec.bits_consumed-32);
	TEST_ASSERT_EQUAL_INT(BIT_OVERFLOW, bit_refill(&dec));

#if 0
	this case is prevented by an assert

	/* test log_2 to big */
	bitstream = 0x00000000;
	bit_init_decoder(&dec, &bitstream, sizeof(bitstream));
	log2_m = 33;
	decoded_cw = golomb_decoder(&dec, m, log2_m);
	TEST_ASSERT_EQUAL(0, dec.bits_consumed-32);
#endif
}


/**
 * @test select_decoder
 */

void test_select_decoder(void)
{
	decoder_ptr decoder;
	uint32_t golomb_par;

	golomb_par = 1;
	decoder = select_decoder(golomb_par);
	TEST_ASSERT_EQUAL(unary_decoder, decoder);

	golomb_par = 0x80000000;
	decoder = select_decoder(golomb_par);
	TEST_ASSERT_EQUAL(rice_decoder, decoder);

	golomb_par = 3;
	decoder = select_decoder(golomb_par);
	TEST_ASSERT_EQUAL(golomb_decoder, decoder);

	golomb_par = 0x7FFFFFFF;
	decoder = select_decoder(golomb_par);
	TEST_ASSERT_EQUAL(golomb_decoder, decoder);

#if 0
this case is prevented by an assert
	golomb_par = 0;
	decoder = select_decoder(golomb_par);
	TEST_ASSERT_EQUAL(NULL, decoder);
#endif
}


/**
 * @test decode_zero
 */

void test_decode_zero(void)
{
	uint32_t decoded_value = ~0U;
	uint64_t cmp_data = {0x88449FC000800000};
	struct bit_decoder dec = {0};
	struct decoder_setup setup = {0};
	uint32_t spillover = 8;
	int err;

	cpu_to_be64s(&cmp_data);
	bit_init_decoder(&dec, &cmp_data, sizeof(cmp_data));
	configure_decoder_setup(&setup, &dec, CMP_MODE_DIFF_ZERO, 1, spillover, CMP_LOSSLESS, 16);


	err = decode_zero(&setup, &decoded_value);
	TEST_ASSERT_EQUAL_HEX(0, decoded_value);
	TEST_ASSERT_FALSE(err);
	err = decode_zero(&setup, &decoded_value);
	TEST_ASSERT_EQUAL_HEX(0x4223, decoded_value);
	TEST_ASSERT_FALSE(err);
	err = decode_zero(&setup, &decoded_value);
	TEST_ASSERT_FALSE(err);
	TEST_ASSERT_EQUAL_HEX(6, decoded_value);
	err = decode_zero(&setup, &decoded_value);
	TEST_ASSERT_FALSE(err);
	TEST_ASSERT_EQUAL_HEX(7, decoded_value);
	err = decode_zero(&setup, &decoded_value);
	TEST_ASSERT_EQUAL_HEX(0xFFFF, decoded_value);
	TEST_ASSERT_FALSE(err);
	TEST_ASSERT_EQUAL_INT(BIT_END_OF_BUFFER, bit_refill(&dec));

	/* error case: read over the cmp_data buffer 1 */
	err = decode_zero(&setup, &decoded_value);
	TEST_ASSERT_TRUE(err);

	/* error case: read over the cmp_data buffer 2 */
	cmp_data = cpu_to_be64(0x0001000000000000); /* 8 encoded > spill_over */
	bit_init_decoder(&dec, &cmp_data, sizeof(cmp_data));
	bit_consume_bits(&dec, 64);
	err = decode_zero(&setup, &decoded_value);
	TEST_ASSERT_TRUE(err);

	 /* error case: decoded value larger than the outlier parameter */
	cmp_data = cpu_to_be64(0xFF00000000000000); /* 7 encoded > spill_over */
	bit_init_decoder(&dec, &cmp_data, sizeof(cmp_data));
	err = decode_zero(&setup, &decoded_value);
	TEST_ASSERT_TRUE(err);
	/* this should work */
	cmp_data = cpu_to_be64(0xFE00000000000000); /* 8 encoded > spill_over */
	bit_init_decoder(&dec, &cmp_data, sizeof(cmp_data));
	err = decode_zero(&setup, &decoded_value);
	TEST_ASSERT_FALSE(err);
	TEST_ASSERT_EQUAL_HEX(6, decoded_value);

	/* error case: value after escape symbol smaller that spillover */
	cmp_data = cpu_to_be64(0x003000000000000); /* 0 encoded + 6 unencoded < spill_over */
	bit_init_decoder(&dec, &cmp_data, sizeof(cmp_data));
	err = decode_zero(&setup, &decoded_value);
	TEST_ASSERT_TRUE(err);
	/* this should work */
	cmp_data = cpu_to_be64(0x004000000000000); /* 0 encoded + 7 unencoded < spill_over */
	bit_init_decoder(&dec, &cmp_data, sizeof(cmp_data));
	err = decode_zero(&setup, &decoded_value);
	TEST_ASSERT_EQUAL_HEX(7, decoded_value);
	TEST_ASSERT_FALSE(err);
}


/**
 * @test decode_zero
 */

void test_zero_refill_needed(void)
{
	uint32_t decoded_value = ~0U;
	uint64_t cmp_data[] = {0x0000000200000003, 0xFFFFFFFC00000000};
	struct bit_decoder dec = {0};
	struct decoder_setup setup = {0};
	uint32_t spillover = 8;
	uint32_t m = 1<<30;
	int err;

	cpu_to_be64s(&cmp_data[0]);
	cpu_to_be64s(&cmp_data[1]);
	bit_init_decoder(&dec, cmp_data, sizeof(cmp_data));
	configure_decoder_setup(&setup, &dec, CMP_MODE_DIFF_ZERO, m, spillover, CMP_LOSSLESS, 32);

	err = decode_zero(&setup, &decoded_value);
	TEST_ASSERT_EQUAL_HEX(0, decoded_value);
	TEST_ASSERT_FALSE(err);
	err = decode_zero(&setup, &decoded_value);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFE, decoded_value);
	TEST_ASSERT_FALSE(err);
}


/**
 * @test decode_multi
 */

void test_decode_multi(void)
{
	uint32_t decoded_value = ~0U;
	uint32_t cmp_data[] = {0x16B66DF8, 0x84360000};
	struct decoder_setup setup = {0};
	struct bit_decoder dec = {0};
	int err;

	cpu_to_be32s(&cmp_data[0]);
	cpu_to_be32s(&cmp_data[1]);

	bit_init_decoder(&dec, cmp_data, sizeof(cmp_data));
	configure_decoder_setup(&setup, &dec, CMP_MODE_DIFF_MULTI, 3, 8, CMP_LOSSLESS, 16);


	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_FALSE(err);
	TEST_ASSERT_EQUAL_HEX(0, decoded_value);
	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_FALSE(err);
	TEST_ASSERT_EQUAL_HEX(1, decoded_value);
	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_FALSE(err);
	TEST_ASSERT_EQUAL_HEX(7, decoded_value);
	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_FALSE(err);
	TEST_ASSERT_EQUAL_HEX(8, decoded_value);
	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_FALSE(err);
	TEST_ASSERT_EQUAL_HEX(9, decoded_value);
	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_FALSE(err);
	TEST_ASSERT_EQUAL_HEX(0x4223, decoded_value);
	/* TEST_ASSERT_EQUAL_INT(47, stream_pos); */

	/* error cases unencoded_len > 32 */

	configure_decoder_setup(&setup, &dec, CMP_MODE_DIFF_MULTI, 3, 8, CMP_LOSSLESS, 16);

	/* 0xFF -> 24 = spill(8)+16 -> unencoded_len = 34 bits */
	cmp_data[0] = cpu_to_be32(0xFF000000);
	cmp_data[1] = cpu_to_be32(0x00000000);
	bit_init_decoder(&dec, cmp_data, sizeof(cmp_data));

	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_EQUAL_INT(-1, err);


	/* 0xFA -> 16 = spill(8)+8 -> unencoded_len = 17 bits -> larger than 16 bit max_used_bits */
	cmp_data[0] = cpu_to_be32(0xFA000000);
	cmp_data[1] = cpu_to_be32(0x00000000);
	bit_init_decoder(&dec, cmp_data, sizeof(cmp_data));

	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_EQUAL_INT(-1, err);

	/* this should work */
	cmp_data[0] = cpu_to_be32(0xF9000200);
	cmp_data[1] = cpu_to_be32(0x00000000);
	bit_init_decoder(&dec, cmp_data, sizeof(cmp_data));

	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_FALSE(err);
	/* TEST_ASSERT_EQUAL_INT(7+16, stream_pos); */
	TEST_ASSERT_EQUAL_HEX(0x8001+8, decoded_value);

	/* error cases unencoded_val is not plausible */
	/* unencoded_len = 4; unencoded_val =0b0011 */
	cmp_data[0] = cpu_to_be32(0xEC000000);
	cmp_data[1] = cpu_to_be32(0x00000000);
	bit_init_decoder(&dec, cmp_data, sizeof(cmp_data));

	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_EQUAL_INT(-1, err);

	/* unencoded_len = 16; unencoded_val =0x3FFF */
	cmp_data[0] = cpu_to_be32(0xF87FFE00);
	cmp_data[1] = cpu_to_be32(0x00000000);
	bit_init_decoder(&dec, cmp_data, sizeof(cmp_data));

	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_EQUAL_INT(-1, err);
	/* unencoded_len = 16; unencoded_val =0x3FFF */
	cmp_data[0] = cpu_to_be32(0xF87FFE00);
	cmp_data[1] = cpu_to_be32(0x00000000);
	bit_init_decoder(&dec, cmp_data, sizeof(cmp_data));

	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_EQUAL_INT(-1, err);

	/* decoded value smaller that outlier */
	cmp_data[0] = cpu_to_be32(0xF9FFFE00);
	cmp_data[1] = cpu_to_be32(0x00000000);
	bit_init_decoder(&dec, cmp_data, sizeof(cmp_data));

	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_EQUAL_INT(-1, err);
}


/**
 * @test decode_multi
 */

void test_multi_refill_needed(void)
{
	uint32_t decoded_value = ~0U;
	uint8_t cmp_data[] = {0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
	uint32_t cmp_data2[2];
	struct bit_decoder dec = {0};
	struct decoder_setup setup = {0};
	uint32_t spillover = 16;
	uint32_t m = 1;
	int err;

	bit_init_decoder(&dec, cmp_data, sizeof(cmp_data));
	configure_decoder_setup(&setup, &dec, CMP_MODE_DIFF_ZERO, m, spillover, CMP_LOSSLESS, 32);

	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_FALSE(err);
	TEST_ASSERT_EQUAL_HEX(0, decoded_value);
	/* this only works with a 2nd refill */
	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_FALSE(err);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, decoded_value);
	/* 2nd refill should fail */
	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_TRUE(err);


	/* decoded value smaller that outlier */
	configure_decoder_setup(&setup, &dec, CMP_MODE_DIFF_ZERO, m, spillover, CMP_LOSSLESS, 16);
	cmp_data2[0] = cpu_to_be32(0xFF7FFFFF);
	cmp_data2[1] = cpu_to_be32(0x7FFF8000);
	bit_init_decoder(&dec, cmp_data2, 6); /* bitstream is to short */

	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_FALSE(err);
	TEST_ASSERT_EQUAL_HEX(8, decoded_value);

	/* 2nd refill should fail and outlier small than the outlier trigger */
	err = decode_multi(&setup, &decoded_value);
	TEST_ASSERT_EQUAL_INT(CORRUPTION_DETECTED, err);
}


/**
 * @test re_map_to_pos
 */

void test_re_map_to_pos(void)
{
	int j;
	uint32_t input, result;
	unsigned int max_value_bits;

	input = (uint32_t)INT32_MIN;
	result = re_map_to_pos(map_to_pos(input, 32));
	TEST_ASSERT_EQUAL_INT32(input, result);

	input = INT32_MAX;
	result = re_map_to_pos(map_to_pos(input, 32));
	TEST_ASSERT_EQUAL_INT32(input, result);

	input = -1U;
	result = re_map_to_pos(map_to_pos(input, 32));
	TEST_ASSERT_EQUAL_INT32(input, result);

	input = 0;
	result = re_map_to_pos(map_to_pos(input, 32));
	TEST_ASSERT_EQUAL_INT32(input, result);

	input = 1; max_value_bits = 6;
	result = re_map_to_pos(map_to_pos(input, max_value_bits));
	TEST_ASSERT_EQUAL_INT32(input, result);

	for (j = -16; j < 15; j++) {
		uint32_t map_val =  map_to_pos((uint32_t)j, 16) & 0x3F;

		result = re_map_to_pos(map_val);
		TEST_ASSERT_EQUAL_INT32(j, result);
	}

	for (j = INT16_MIN; j < INT16_MAX; j++) {
		uint32_t map_val =  map_to_pos((uint32_t)j, 16) & 0xFFFF;

		result = re_map_to_pos(map_val);
		TEST_ASSERT_EQUAL_INT32(j, result);
	}
#if 0
	for (j = INT32_MIN; j < INT32_MAX; j++) {
		result = re_map_to_pos(map_to_pos((uint32_t)j, 32));
		TEST_ASSERT_EQUAL_INT32(j, result);
	}
#endif
}


/**
 * @test decompress_cmp_entiy
 */

void test_cmp_decmp_rdcu_raw(void)
{
	int cmp_size, decmp_size;
	size_t s, i;
	struct rdcu_cfg rcfg = {0};
	struct cmp_info info;
	uint16_t data[] = {0, 1, 2, 0x42, (uint16_t)INT16_MIN, INT16_MAX, UINT16_MAX};
	uint32_t *compressed_data;
	uint16_t *decompressed_data;
	struct cmp_entity *ent;

	rcfg.cmp_mode = CMP_MODE_RAW;
	rcfg.input_buf = data;
	rcfg.samples = ARRAY_SIZE(data);
	rcfg.buffer_length = ARRAY_SIZE(data);

	compressed_data = malloc(sizeof(data));
	TEST_ASSERT_TRUE(compressed_data);
	rcfg.icu_output_buf = compressed_data;

	cmp_size = compress_like_rdcu(&rcfg, &info);
	TEST_ASSERT_EQUAL_INT(sizeof(data)*CHAR_BIT, cmp_size);

	s = cmp_ent_create(NULL, DATA_TYPE_IMAGETTE , rcfg.cmp_mode == CMP_MODE_RAW,
			   cmp_bit_to_byte((unsigned int)cmp_size));
	TEST_ASSERT_TRUE(s);
	ent = malloc(s);
	TEST_ASSERT_TRUE(ent);
	s = cmp_ent_create(ent, DATA_TYPE_IMAGETTE , rcfg.cmp_mode == CMP_MODE_RAW,
			   cmp_bit_to_byte((unsigned int)cmp_size));
	TEST_ASSERT_TRUE(s);
	TEST_ASSERT_FALSE(cmp_ent_write_rdcu_cmp_pars(ent, &info, NULL));

	memcpy(cmp_ent_get_data_buf(ent), compressed_data, ((unsigned int)cmp_size+7)/8);

	decmp_size = decompress_cmp_entiy(ent, NULL, NULL, NULL);
	TEST_ASSERT_EQUAL_INT(sizeof(data), decmp_size);
	decompressed_data = malloc((size_t)decmp_size);
	TEST_ASSERT_TRUE(decompressed_data);
	decmp_size = decompress_cmp_entiy(ent, NULL, NULL, decompressed_data);
	TEST_ASSERT_EQUAL_INT(sizeof(data), decmp_size);

	for (i = 0; i < ARRAY_SIZE(data); ++i)
		TEST_ASSERT_EQUAL_INT(data[i], decompressed_data[i]);


	free(compressed_data);
	free(ent);
	free(decompressed_data);
}


/**
 * @test decompress_imagette
 */

void test_decompress_imagette_model(void)
{
	uint16_t data[5]  = {0};
	uint16_t model[5] = {0, 1, 2, 3, 4};
	uint16_t up_model[5]  = {0};
	uint32_t cmp_data[2] = {0};
	struct cmp_cfg cfg = {0};
	struct bit_decoder dec;
	int err;

	cmp_data[0] = cpu_to_be32(0x49240000);

	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.cmp_mode = CMP_MODE_MODEL_MULTI;
	cfg.input_buf = data;
	cfg.model_buf = model;
	cfg.icu_new_model_buf = up_model;
	cfg.icu_output_buf = cmp_data;
	cfg.buffer_length = 4;
	cfg.samples = 5;
	cfg.model_value = 16;
	cfg.cmp_par_imagette = 4;
	cfg.spill_imagette = 48;

	bit_init_decoder(&dec, cfg.icu_output_buf, cfg.buffer_length);

	err = decompress_imagette(&cfg, &dec, RDCU_DECOMPRESSION);
	TEST_ASSERT_FALSE(err);
	TEST_ASSERT_EQUAL_HEX(1, data[0]);
	TEST_ASSERT_EQUAL_HEX(2, data[1]);
	TEST_ASSERT_EQUAL_HEX(3, data[2]);
	TEST_ASSERT_EQUAL_HEX(4, data[3]);
	TEST_ASSERT_EQUAL_HEX(5, data[4]);

	TEST_ASSERT_EQUAL_HEX(0, up_model[0]);
	TEST_ASSERT_EQUAL_HEX(1, up_model[1]);
	TEST_ASSERT_EQUAL_HEX(2, up_model[2]);
	TEST_ASSERT_EQUAL_HEX(3, up_model[3]);
	TEST_ASSERT_EQUAL_HEX(4, up_model[4]);
}


/**
 * @test decompress_cmp_entiy
 */

void test_decompress_imagette_chunk_raw(void)
{
	int decmp_size;
	size_t i;
	uint16_t data[] = {0, 1, 2, 0x42, (uint16_t)INT16_MIN, INT16_MAX, UINT16_MAX};
	uint8_t *decompressed_data;
	struct cmp_entity *ent;
	uint32_t ent_size;
	uint32_t chunk_size = 2*(COLLECTION_HDR_SIZE + sizeof(data));
	uint8_t *chunk = calloc(1, chunk_size); TEST_ASSERT_TRUE(chunk);

	for (i = 0; i < 2; i++) {
		struct collection_hdr *col = (struct collection_hdr *)(chunk+chunk_size/2*i);

		TEST_ASSERT_FALSE(cmp_col_set_subservice(col, SST_NCxx_S_SCIENCE_IMAGETTE));
		TEST_ASSERT_FALSE(cmp_col_set_data_length(col, sizeof(data)));
		TEST_ASSERT_FALSE(cmp_col_set_timestamp(col, 0x0102030400607));
		memcpy(col->entry, data, sizeof(data));
	}

	ent_size = cmp_ent_create(NULL, DATA_TYPE_CHUNK, 1, chunk_size);
	TEST_ASSERT_EQUAL_INT(GENERIC_HEADER_SIZE+chunk_size, ent_size);
	ent = malloc(ent_size); TEST_ASSERT_TRUE(ent);
	ent_size = cmp_ent_create(ent, DATA_TYPE_CHUNK, 1, chunk_size);
	TEST_ASSERT_EQUAL_INT(GENERIC_HEADER_SIZE+chunk_size, ent_size);
	TEST_ASSERT_FALSE(cmp_ent_set_original_size(ent, chunk_size));
	memcpy(cmp_ent_get_data_buf(ent), chunk, chunk_size);
	TEST_ASSERT_FALSE(cpu_to_be_chunk(cmp_ent_get_data_buf(ent), chunk_size));

	decmp_size = decompress_cmp_entiy(ent, NULL, NULL, NULL);
	TEST_ASSERT_EQUAL_INT(chunk_size, decmp_size);
	decompressed_data = malloc((size_t)decmp_size);
	TEST_ASSERT_TRUE(decompressed_data);
	decmp_size = decompress_cmp_entiy(ent, NULL, NULL, decompressed_data);
	TEST_ASSERT_EQUAL_INT(chunk_size, decmp_size);

	for (i = 0; i < chunk_size; ++i)
		TEST_ASSERT_EQUAL_INT(chunk[i], decompressed_data[i]);


	free(chunk);
	free(ent);
	free(decompressed_data);
}


void test_decompression_error_cases(void)
{
	/* TODO: error cases model decompression without a model Buffer */
	/* TODO: error cases wrong cmp parameter; model value; usw */
}

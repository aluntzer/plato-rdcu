/**
 * @file    n_dpu_pkt.c
 * @author  Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date    2020
 * @brief definition of the N-DPU packets entrys
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
 */


#include <stddef.h>
#include <stdio.h>

#include "../include/n_dpu_pkt.h"
#include "../include/cmp_support.h"


/**
 * @brief rounding down the least significant digits of a uint16_t data buffer
 *
 * @note this step involves data loss (if round > 0)
 * @note change the data buffer in-place
 *
 * @param  data_buf	uint16_t formatted data buffer
 * @param  samples	the size of the data buffer measured in uint16_t samples
 * @param  round	number of bits to round; if zero no rounding takes place
 *
 * @returns 0 on success, error otherwise
 */

int lossy_rounding_16(uint16_t *data_buf, unsigned int samples, unsigned int
		      round)
{
	size_t i;

	if (!samples)
		return 0;

	if (!data_buf)
		return -1;

	/* round 0 means loss less compression, no further processing is
	 * necessary */
	if (round == 0)
		return 0;

	for (i = 0; i < samples; i++)
		data_buf[i] = round_down(data_buf[i], round);  /* this is the lossy step */

	return 0;
}


/**
 * @brief rounding back the least significant digits of the data buffer
 *
 * @param data_buf	pointer to the data to process
 * @param samples_used	the size of the data and model buffer in 16 bit units
 * @param round_used	used number of bits to round; if zero no rounding takes place
 *
 * @returns 0 on success, error otherwise
 */

int de_lossy_rounding_16(uint16_t *data_buf, uint32_t samples_used, uint32_t
			 round_used)
{
	size_t i;

	if (!samples_used)
		return 0;

	if (!data_buf)
		return -1;

	/* round 0 means loss less compression, no further processing is necessary */
	if (round_used == 0)
		return 0;

	for (i = 0; i < samples_used; i++) {
		/* check if data are not to big for a overflow */
		uint16_t mask = (uint16_t)(~0 << (16-round_used));
		if (data_buf[i] & mask) {
			printf("learn how to print debug messages!");
			return -1;
		}
		data_buf[i] = round_back(data_buf[i], round_used);
	}
	return 0;
}


/**
 * @brief rounding down the least significant digits of a uint32_t data buffer
 *
 * @note this step involves data loss (if round > 0)
 * @note change the data buffer in-place
 *
 * @param  data_buf	a uint32_t formatted data buffer
 * @param  samples	the size of the data buffer measured in uint16_t samples
 * @param  round	number of bits to round; if zero no rounding takes place
 *
 * @returns 0 on success, error otherwise
 */

int lossy_rounding_32(uint32_t *data_buf, unsigned int samples, unsigned int
		      round)
{
	size_t i;

	if (!samples)
		return 0;

	if (!data_buf)
		return -1;

	/* round 0 means loss less compression, no further processing is
	 * necessary */
	if (round == 0)
		return 0;

	for (i = 0; i < samples; i++)
		data_buf[i] = round_down(data_buf[i], round);  /* this is the lossy step */

	return 0;
}


int de_lossy_rounding_32(uint32_t *data_buf, uint32_t samples_used, uint32_t
			 round_used)
{
	size_t i;

	if (!samples_used)
		return 0;

	if (!data_buf)
		return -1;

	/* round 0 means loss less compression, no further processing is necessary */
	if (round_used == 0)
		return 0;

	for (i = 0; i < samples_used; i++) {
		/* check if data are not to big for a overflow */
		uint32_t mask = (uint32_t)(~0 << (32-round_used));
		if (data_buf[i] & mask) {
			printf("learn how to print debug messages!");
			return -1;
		}
		data_buf[i] = round_back(data_buf[i], round_used);
	}
	return 0;
}


struct S_FX sub_S_FX(struct S_FX a, struct S_FX b)
{
	struct S_FX result;

	result.EXPOSURE_FLAGS = a.EXPOSURE_FLAGS - b.EXPOSURE_FLAGS;
	result.FX = a.FX - b.FX;

	return result;
}


struct S_FX add_S_FX(struct S_FX a, struct S_FX b)
{
	struct S_FX result;

	result.EXPOSURE_FLAGS = a.EXPOSURE_FLAGS + b.EXPOSURE_FLAGS;
	result.FX = a.FX + b.FX;

	return result;
}


/**
 * @brief rounding down the least significant digits of a S_FX data buffer
 *
 * @note this step involves data loss (if round > 0)
 * @note change the data buffer in-place
 * @note the exposure_flags are not rounded
 *
 * @param  data_buf	a S_FX formatted data buffer
 * @param  samples	the size of the data buffer measured in S_FX samples
 * @param  round	number of bits to round; if zero no rounding takes place
 *
 * @returns 0 on success, error otherwise
 */

int lossy_rounding_S_FX(struct S_FX *data_buf, unsigned int samples, unsigned
			int round)
{
	size_t i;

	if (!samples)
		return 0;

	if (!data_buf)
		return -1;

	/* round 0 means loss less compression, no further processing is
	 * necessary */
	if (round == 0)
		return 0;

	for (i = 0; i < samples; i++)
		data_buf[i].FX = round_down(data_buf[i].FX, round);

	return 0;
}


int de_lossy_rounding_S_FX(struct S_FX *data_buf, unsigned int samples_used,
			   unsigned int round_used)
{
	size_t i;

	if (!samples_used)
		return 0;

	if (!data_buf)
		return -1;

	if (round_used == 0) /* round 0 means loss less compression, no further processing is necessary */
		return 0;

	for (i = 0; i < samples_used; i++) {
		uint32_t mask = (~0U << (32-round_used));

		if (data_buf[i].FX & mask) {
#if DEBUG
			printf("learn how to print debug messages!");
#endif
			return -1;
		}

		data_buf[i].FX = round_back(data_buf[i].FX, round_used);
	}
	return 0;
}


struct S_FX cal_up_model_S_FX(struct S_FX data_buf, struct S_FX model_buf,
			      unsigned int model_value)
{
	struct S_FX result;

	result.EXPOSURE_FLAGS = (uint8_t)cal_up_model(data_buf.EXPOSURE_FLAGS,
						      model_buf.EXPOSURE_FLAGS,
						      model_value);
	result.FX = cal_up_model(data_buf.FX, model_buf.FX, model_value);

	return result;
}


struct S_FX_EFX sub_S_FX_EFX(struct S_FX_EFX a, struct S_FX_EFX b)
{
	struct S_FX_EFX result;

	result.EXPOSURE_FLAGS = a.EXPOSURE_FLAGS - b.EXPOSURE_FLAGS;
	result.FX = a.FX - b.FX;
	result.EFX = a.EFX - b.EFX;

	return result;
}


struct S_FX_EFX add_S_FX_EFX(struct S_FX_EFX a, struct S_FX_EFX b)
{
	struct S_FX_EFX result;

	result.EXPOSURE_FLAGS = a.EXPOSURE_FLAGS + b.EXPOSURE_FLAGS;
	result.FX = a.FX + b.FX;
	result.EFX = a.EFX + b.EFX;

	return result;
}


/**
 * @brief rounding down the least significant digits of a S_FX_EFX data buffer
 *
 * @note this step involves data loss (if round > 0)
 * @note change the data buffer in-place
 * @note the exposure_flags are not rounded
 *
 * @param  data_buf	a S_FX_EFX formatted data buffer
 * @param  samples	the size of the data buffer measured in S_FX_EFX samples
 * @param  round	number of bits to round; if zero no rounding takes place
 *
 * @returns 0 on success, error otherwise
 */

int lossy_rounding_S_FX_EFX(struct S_FX_EFX *data, unsigned int samples,
			    unsigned int round)
{
	size_t i;

	if (!samples)
		return 0;

	if (!data)
		return -1;

	/* round 0 means loss less compression, no further processing is
	 * necessary */
	if (round == 0)
		return 0;

	for (i = 0; i < samples; i++)
	{
		data[i].FX = round_down(data[i].FX, round);
		data[i].EFX = round_down(data[i].EFX, round);
	}
	return 0;
}


int de_lossy_rounding_S_FX_EFX(struct S_FX_EFX *data_buf, unsigned int
			       samples_used, unsigned int round_used)
{
	size_t i;

	if (!samples_used)
		return 0;

	if (!data_buf)
		return -1;

	if (round_used == 0) /* round 0 means loss less compression, no further processing is necessary */
		return 0;

	for (i = 0; i < samples_used; i++) {
		uint32_t mask = (~0U << (32-round_used));

		if (data_buf[i].FX & mask) {
			printf("learn how to print debug messages!");
			return -1;
		}
		if (data_buf[i].EFX & mask) {
			printf("learn how to print debug messages!");
			return -1;
		}

		data_buf[i].FX = round_back(data_buf[i].FX, round_used);
		data_buf[i].EFX = round_back(data_buf[i].EFX, round_used);
	}
	return 0;
}


struct S_FX_EFX cal_up_model_S_FX_EFX(struct S_FX_EFX data_buf, struct S_FX_EFX
				      model_buf, unsigned int model_value)
{
	struct S_FX_EFX result;

	result.EXPOSURE_FLAGS =
		(uint8_t)cal_up_model(data_buf.EXPOSURE_FLAGS,
				      model_buf.EXPOSURE_FLAGS, model_value);
	result.FX = cal_up_model(data_buf.FX, model_buf.FX, model_value);
	result.EFX = cal_up_model(data_buf.EFX, model_buf.FX, model_value);

	return result;
}


struct S_FX_NCOB sub_S_FX_NCOB(struct S_FX_NCOB a, struct S_FX_NCOB b)
{
	struct S_FX_NCOB result;

	result.EXPOSURE_FLAGS = a.EXPOSURE_FLAGS - b.EXPOSURE_FLAGS;
	result.FX = a.FX - b.FX;
	result.NCOB_X = a.NCOB_X - b.NCOB_X;
	result.NCOB_Y = a.NCOB_Y - b.NCOB_Y;

	return result;
}


struct S_FX_NCOB add_S_FX_NCOB(struct S_FX_NCOB a, struct S_FX_NCOB b)
{
	struct S_FX_NCOB result;

	result.EXPOSURE_FLAGS = a.EXPOSURE_FLAGS + b.EXPOSURE_FLAGS;
	result.FX = a.FX + b.FX;
	result.NCOB_X = a.NCOB_X + b.NCOB_X;
	result.NCOB_Y = a.NCOB_Y + b.NCOB_Y;

	return result;
}


/**
 * @brief rounding down the least significant digits of a S_FX_NCOB data buffer
 *
 * @note this step involves data loss (if round > 0)
 * @note change the data buffer in-place
 * @note the exposure_flags are not rounded
 *
 * @param  data_buf	a S_FX_NCOB formatted data buffer
 * @param  samples	the size of the data buffer measured in S_FX_NCOB samples
 * @param  round		number of bits to round; if zero no rounding takes place
 *
 * @returns 0 on success, error otherwise
 */

int lossy_rounding_S_FX_NCOB(struct S_FX_NCOB *data_buf, unsigned int samples,
			     unsigned int round)
{
	size_t i;

	if (!samples)
		return 0;

	if (!data_buf)
		return -1;

	/* round 0 means loss less compression, no further processing is
	 * necessary */
	if (round == 0)
		return 0;

	for (i = 0; i < samples; i++)
	{
		data_buf[i].FX = round_down(data_buf[i].FX, round);
		data_buf[i].NCOB_X = round_down(data_buf[i].NCOB_X, round);
		data_buf[i].NCOB_Y = round_down(data_buf[i].NCOB_Y, round);
	}
	return 0;
}


int de_lossy_rounding_S_FX_NCOB(struct S_FX_NCOB *data_buf, unsigned int
				samples_used, unsigned int round_used)
{
	size_t i;

	if (!samples_used)
		return 0;

	if (!data_buf)
		return -1;

	if (round_used == 0) /* round 0 means loss less compression, no further processing is necessary */
		return 0;

	for (i = 0; i < samples_used; i++) {
		uint32_t mask = (~0U << (32-round_used));

		if (data_buf[i].FX & mask) {
			printf("learn how to print debug messages!");
			return -1;
		}
		if (data_buf[i].NCOB_X & mask) {
			printf("learn how to print debug messages!");
			return -1;
		}
		if (data_buf[i].NCOB_Y & mask) {
			printf("learn how to print debug messages!");
			return -1;
		}

		data_buf[i].FX = round_back(data_buf[i].FX, round_used);
		data_buf[i].NCOB_X = round_back(data_buf[i].NCOB_X, round_used);
		data_buf[i].NCOB_Y = round_back(data_buf[i].NCOB_Y, round_used);
	}
	return 0;
}


struct S_FX_NCOB cal_up_model_S_FX_NCOB(struct S_FX_NCOB data_buf, struct S_FX_NCOB
					model_buf, unsigned int model_value)
{
	struct S_FX_NCOB result;

	result.EXPOSURE_FLAGS =
		(uint8_t)cal_up_model(data_buf.EXPOSURE_FLAGS,
				      model_buf.EXPOSURE_FLAGS, model_value);
	result.FX = cal_up_model(data_buf.FX, model_buf.FX, model_value);
	result.NCOB_X = cal_up_model(data_buf.NCOB_X, model_buf.NCOB_X, model_value);
	result.NCOB_Y = cal_up_model(data_buf.NCOB_Y, model_buf.NCOB_Y, model_value);

	return result;
}


struct S_FX_EFX_NCOB_ECOB sub_S_FX_EFX_NCOB_ECOB(struct S_FX_EFX_NCOB_ECOB a,
						 struct S_FX_EFX_NCOB_ECOB b)
{
	struct S_FX_EFX_NCOB_ECOB result;

	result.EXPOSURE_FLAGS = a.EXPOSURE_FLAGS - b.EXPOSURE_FLAGS;
	result.FX = a.FX - b.FX;
	result.NCOB_X = a.NCOB_X - b.NCOB_X;
	result.NCOB_Y = a.NCOB_Y - b.NCOB_Y;
	result.EFX = a.EFX - b.EFX;
	result.ECOB_X = a.ECOB_X - b.ECOB_X;
	result.ECOB_Y = a.ECOB_Y - b.ECOB_Y;

	return result;
}


struct S_FX_EFX_NCOB_ECOB add_S_FX_EFX_NCOB_ECOB(struct S_FX_EFX_NCOB_ECOB a,
						 struct S_FX_EFX_NCOB_ECOB b)
{
	struct S_FX_EFX_NCOB_ECOB result;

	result.EXPOSURE_FLAGS = a.EXPOSURE_FLAGS + b.EXPOSURE_FLAGS;
	result.FX = a.FX + b.FX;
	result.NCOB_X = a.NCOB_X + b.NCOB_X;
	result.NCOB_Y = a.NCOB_Y + b.NCOB_Y;
	result.EFX = a.EFX + b.EFX;
	result.ECOB_X = a.ECOB_X + b.ECOB_X;
	result.ECOB_Y = a.ECOB_Y + b.ECOB_Y;

	return result;
}


/**
 * @brief rounding down the least significant digits of a S_FX_EFX_NCOB_ECOB data
 *	buffer
 *
 * @note this step involves data loss (if round > 0)
 * @note change the data buffer in-place
 * @note the exposure_flags are not rounded
 *
 * @param  data_buf	a S_FX_EFX_NCOB_ECOB formatted data buffer
 * @param  samples	the size of the data buffer measured in
 *	S_FX_EFX_NCOB_ECOB samples
 * @param  round		number of bits to round; if zero no rounding takes place
 *
 * @returns 0 on success, error otherwise
 */

int lossy_rounding_S_FX_EFX_NCOB_ECOB(struct S_FX_EFX_NCOB_ECOB *data_buf,
				      unsigned int samples, unsigned int round)
{
	size_t i;

	if (!samples)
		return 0;

	if (!data_buf)
		return -1;

	if (round == 0) /* round 0 means loss less compression, no further processing is necessary */
		return 0;

	for (i = 0; i < samples; i++)
	{
		data_buf[i].FX = round_down(data_buf[i].FX, round);
		data_buf[i].NCOB_X = round_down(data_buf[i].NCOB_X, round);
		data_buf[i].NCOB_Y = round_down(data_buf[i].NCOB_Y, round);
		data_buf[i].EFX = round_down(data_buf[i].EFX, round);
		data_buf[i].ECOB_X = round_down(data_buf[i].ECOB_X, round);
		data_buf[i].ECOB_Y = round_down(data_buf[i].ECOB_Y, round);
	}
	return 0;
}


int de_lossy_rounding_S_FX_EFX_NCOB_ECOB(struct S_FX_EFX_NCOB_ECOB *data_buf,
					 unsigned int samples_used,
					 unsigned int round_used)
{
	size_t i;

	if (!samples_used)
		return 0;

	if (!data_buf)
		return -1;

	if (round_used == 0) /* round 0 means loss less compression, no further processing is necessary */
		return 0;

	for (i = 0; i < samples_used; i++)
	{
		uint32_t mask = (~0U << (32-round_used));

		if (data_buf[i].FX & mask) {
			printf("learn how to print debug messages!");
			return -1;
		}
		if (data_buf[i].NCOB_X & mask) {
			printf("learn how to print debug messages!");
			return -1;
		}
		if (data_buf[i].NCOB_Y & mask) {
			printf("learn how to print debug messages!");
			return -1;
		}
		if (data_buf[i].EFX & mask) {
			printf("learn how to print debug messages!");
			return -1;
		}
		if (data_buf[i].ECOB_X & mask) {
			printf("learn how to print debug messages!");
			return -1;
		}
		if (data_buf[i].ECOB_Y & mask) {
			printf("learn how to print debug messages!");
			return -1;
		}

		data_buf[i].FX = round_back(data_buf[i].FX, round_used);
		data_buf[i].NCOB_X = round_back(data_buf[i].NCOB_X, round_used);
		data_buf[i].NCOB_Y = round_back(data_buf[i].NCOB_Y, round_used);
		data_buf[i].EFX = round_back(data_buf[i].EFX, round_used);
		data_buf[i].ECOB_X = round_back(data_buf[i].ECOB_X, round_used);
		data_buf[i].ECOB_Y = round_back(data_buf[i].ECOB_Y, round_used);
	}
	return 0;
}


struct S_FX_EFX_NCOB_ECOB cal_up_model_S_FX_EFX_NCOB_ECOB
	(struct S_FX_EFX_NCOB_ECOB data_buf, struct S_FX_EFX_NCOB_ECOB
	 model_buf, unsigned int model_value)
{
	struct S_FX_EFX_NCOB_ECOB result;

	result.EXPOSURE_FLAGS =
		(uint8_t)cal_up_model(data_buf.EXPOSURE_FLAGS,
				      model_buf.EXPOSURE_FLAGS, model_value);
	result.FX = cal_up_model(data_buf.FX, model_buf.FX, model_value);
	result.NCOB_X = cal_up_model(data_buf.NCOB_X, model_buf.NCOB_X, model_value);
	result.NCOB_Y = cal_up_model(data_buf.NCOB_Y, model_buf.NCOB_Y, model_value);
	result.EFX = cal_up_model(data_buf.EFX, model_buf.EFX, model_value);
	result.ECOB_X = cal_up_model(data_buf.ECOB_X, model_buf.ECOB_X, model_value);
	result.ECOB_Y = cal_up_model(data_buf.ECOB_Y, model_buf.ECOB_Y, model_value);

	return result;
}

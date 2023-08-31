/**
 * @file   cmp_cal_up_model.h
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2023
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
 * @brief functions to calculate the update (new) model
 */

#ifndef CMP_CAL_UP_MODEL_H
#define CMP_CAL_UP_MODEL_H

#include <stdint.h>


/* the maximal model values used in the update equation for the new model */
#define MAX_MODEL_VALUE 16U


/**
 * @brief method for lossy rounding
 * @note This function is implemented as a macro for the sake of performance
 *
 * @param value	the value to round
 * @param round	rounding parameter
 *
 * @return rounded value
 */

#define round_fwd(value, round) ((uint32_t)(value) >> (round))


/**
 * @brief inverse method for lossy rounding
 * @note This function is implemented as a macro for the sake of performance
 *
 * @param value	the value to round back
 * @param round	rounding parameter
 *
 * @return back rounded value
 */

#define round_inv(value, round) ((uint32_t)(value) << (round))


/**
 * @brief implantation of the model update equation
 * @note check before that model_value is not greater than MAX_MODEL_VALUE
 *
 * @param data		data to process
 * @param model		(current) model of the data to process
 * @param model_value	model weighting parameter
 * @param round		routing parameter
 *
 * @returns (new) updated model
 */

#define cmp_up_model(data, model, model_value, round)								\
	__extension__												\
	({													\
		__typeof__(data) __ret;										\
		switch (sizeof(data)) {										\
			case sizeof(uint8_t):									\
			case sizeof(uint16_t):									\
				__ret = (__typeof__(__ret))cmp_up_model16(data, model, model_value, round);	\
				break;										\
			case sizeof(uint32_t):									\
				__ret = (__typeof__(__ret))cmp_up_model32(data, model, model_value, round);	\
				break;										\
		}												\
		__ret;												\
	})


/* fast calculation  for data size smaller that uint32_t  */
static inline uint16_t cmp_up_model16(uint32_t data, uint32_t model,
				      unsigned int model_value, unsigned int round)
{
	/* round and round back input because for decompression the accurate
	 * data values are not available
	 */
	uint32_t const weighted_data = round_inv(round_fwd(data, round), round)
		* (MAX_MODEL_VALUE - model_value);
	uint32_t const weighted_model = model * model_value;

	/* truncation is intended */
	return (uint16_t)((weighted_model + weighted_data) / MAX_MODEL_VALUE);
}


/* slow calculation for uint32_t data size */
static inline uint32_t cmp_up_model32(uint32_t data, uint32_t model,
				      unsigned int model_value, unsigned int round)
{
	/* round and round back input because for decompression the accurate
	 * data values are not available
	 * cast to uint64_t to prevent overflow in the multiplication
	 */
	uint64_t const weighted_data = (uint64_t)round_inv(round_fwd(data, round), round)
		* (MAX_MODEL_VALUE - model_value);
	uint64_t const weighted_model = (uint64_t)model * model_value;

	/* truncation is intended */
	return (uint32_t)((weighted_model + weighted_data) / MAX_MODEL_VALUE);
}

#endif /* CMP_CAL_UP_MODEL */

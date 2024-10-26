/**
 * @file   cmp_chunk.h
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2024
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
 * @brief software chunk compression library
 * @see Data Compression User Manual PLATO-UVIE-PL-UM-0001
 */

#ifndef CMP_CHUNK_H
#define CMP_CHUNK_H

#include <stddef.h>
#include <stdint.h>

#include "common/cmp_support.h"
#include "common/cmp_entity.h"
#include "common/cmp_error_list.h"

/* valid specific compression parameter ranges for chunk compression
 * (every parameter except cmp_mode, model_value, lossy_par)
 */
#define MIN_CHUNK_CMP_PAR	1U
#define MAX_CHUNK_CMP_PAR	UINT16_MAX /* the compression entity does not allow larger values */

#define ROUND_UP_TO_4(x) ((((x)+3)/4)*4)

#define COMPRESS_CHUNK_BOUND_UNSAFE(chunk_size, num_col) (	\
	ROUND_UP_TO_4(NON_IMAGETTE_HEADER_SIZE +		\
		      (num_col) * CMP_COLLECTION_FILD_SIZE +	\
		      (chunk_size)				\
		      )						\
)


/**
 * @brief returns the maximum compressed size in a worst case scenario
 *
 * In case the input data is not compressible
 * This macro is primarily useful for compilation-time evaluation
 * (stack memory allocation for example)
 *
 * @note if the number of collections is not know you can use the
 *	compress_chunk_cmp_size_bound function
 *
 * @param chunk_size	size in bytes of the chunk
 * @param num_col	number of collections in the chunk
 *
 * @returns maximum compressed size for chunk compression; 0 on error
 */

#define COMPRESS_CHUNK_BOUND(chunk_size, num_col) (					\
	(num_col) > 0 &&								\
	(num_col) <= CMP_ENTITY_MAX_SIZE/COLLECTION_HDR_SIZE &&				\
	(chunk_size) >= COLLECTION_HDR_SIZE * (num_col) &&				\
	(chunk_size) <= CMP_ENTITY_MAX_SIZE &&						\
	COMPRESS_CHUNK_BOUND_UNSAFE(chunk_size, num_col) <= CMP_ENTITY_MAX_SIZE ?	\
	COMPRESS_CHUNK_BOUND_UNSAFE(chunk_size, num_col) : 0				\
)


/**
 * @struct cmp_par
 * @brief structure containing all the compression parameters needed for chunk compression
 */

struct cmp_par {
	enum cmp_mode cmp_mode;		/**< compression mode parameter */
	uint32_t model_value;		/**< model weighting parameter */
	uint32_t lossy_par;		/**< lossy compression parameter */

	uint32_t nc_imagette;		/**< compression parameter for imagette compression */

	uint32_t s_exp_flags;		/**< compression parameter for short cadence exposure flags data */
	uint32_t s_fx;			/**< compression parameter for short cadence normal flux data */
	uint32_t s_ncob;		/**< compression parameter for short cadence normal center of brightness data */
	uint32_t s_efx;			/**< compression parameter for short cadence extended flux data  */
	uint32_t s_ecob;		/**< compression parameter for short cadence extended center of brightness data */

	uint32_t l_exp_flags;		/**< compression parameter for long cadence exposure flags data */
	uint32_t l_fx;			/**< compression parameter for long cadence normal flux data */
	uint32_t l_ncob;		/**< compression parameter for long cadence normal center of brightness data */
	uint32_t l_efx;			/**< compression parameter for long cadence extended flux data using extended mask */
	uint32_t l_ecob;		/**< compression parameter for long cadence extended center of brightness data */
	uint32_t l_fx_cob_variance;	/**< compression parameter for long cadence flux/COB variance data */

	uint32_t saturated_imagette;	/**< compression parameter for saturated imagette data */

	uint32_t nc_offset_mean;		/**< compression parameter for normal camera offset mean data */
	uint32_t nc_offset_variance;		/**< compression parameter for normal camera offset variance data */
	uint32_t nc_background_mean;		/**< compression parameter for normal camera background mean data */
	uint32_t nc_background_variance;	/**< compression parameter for normal camera background variance data */
	uint32_t nc_background_outlier_pixels;	/**< compression parameter for normal camera background outlier pixels data */

	uint32_t smearing_mean;			/**< compression parameter for smearing mean data */
	uint32_t smearing_variance_mean;	/**< compression parameter for smearing variance mean data */
	uint32_t smearing_outlier_pixels;	/**< compression parameter for smearing outlier pixels data */

	uint32_t fc_imagette;			/**< compression parameter for fast camera imagette data */
	uint32_t fc_offset_mean;		/**< compression parameter for fast camera offset mean data */
	uint32_t fc_offset_variance;		/**< compression parameter for fast camera offset variance data */
	uint32_t fc_background_mean;		/**< compression parameter for fast camera background mean data */
	uint32_t fc_background_variance;	/**< compression parameter for fast camera background variance data */
	uint32_t fc_background_outlier_pixels;	/**< compression parameter for fast camera background outlier pixels data */
};


/**
 * @brief returns the maximum compressed size in a worst case scenario
 *
 * In case the input data is not compressible
 * This function is primarily useful for memory allocation purposes
 * (destination buffer size).
 *
 * @note if the number of collections is known you can use the
 *	COMPRESS_CHUNK_BOUND macro for compilation-time evaluation
 *	(stack memory allocation for example)
 *
 * @param chunk		pointer to the chunk you want to compress
 * @param chunk_size	size of the chunk in bytes
 *
 * @returns maximum compressed size for a chunk compression on success or an
 *	error code if it fails (which can be tested with cmp_is_error())
 */

uint32_t compress_chunk_cmp_size_bound(const void *chunk, size_t chunk_size);


/**
 * @brief initialise the compress_chunk() function
 *
 * If not initialised the compress_chunk() function sets the timestamps and
 * version_id in the compression entity header to zero
 *
 * @param return_timestamp	pointer to a function returning a current 48-bit
 *				timestamp
 * @param version_id		application software version identifier
 */

void compress_chunk_init(uint64_t (*return_timestamp)(void), uint32_t version_id);


/**
 * @brief compress a data chunk consisting of put together data collections
 *
 * @param chunk			pointer to the chunk to be compressed
 * @param chunk_size		byte size of the chunk
 * @param chunk_model		pointer to a model of a chunk; has the same size
 *				as the chunk (can be NULL if no model compression
 *				mode is used)
 * @param updated_chunk_model	pointer to store the updated model for the next
 *				model mode compression; has the same size as the
 *				chunk (can be the same as the model_of_data
 *				buffer for in-place update or NULL if updated
 *				model is not needed)
 * @param dst			destination pointer to the compressed data
 *				buffer; has to be 4-byte aligned; can be NULL to
 *				only get the compressed data size
 * @param dst_capacity		capacity of the dst buffer; it's recommended to
 *				provide a dst_capacity >=
 *				compress_chunk_cmp_size_bound(chunk, chunk_size)
 *				as it eliminates one potential failure scenario:
 *				not enough space in the dst buffer to write the
 *				compressed data; size is internally rounded down
 *				to a multiple of 4
 * @param cmp_par		pointer to a compression parameters struct
 * @returns the byte size of the compressed data or an error code if it
 *	fails (which can be tested with cmp_is_error())
 */

uint32_t compress_chunk(const void *chunk, uint32_t chunk_size,
			const void *chunk_model, void *updated_chunk_model,
			uint32_t *dst, uint32_t dst_capacity,
			const struct cmp_par *cmp_par);


/**
 * @brief set the model id and model counter in the compression entity header
 *
 * @param dst		pointer to the compressed data (starting with a
 *			compression entity header)
 * @param dst_size	byte size of the dst buffer
 * @param model_id	model identifier; for identifying entities that originate
 *			from the same starting model
 * @param model_counter	model_counter; counts how many times the model was
 *			updated; for non model mode compression use 0
 *
 * @returns the byte size of the dst buffer (= dst_size) on success or an error
 *	code if it fails (which can be tested with cmp_is_error())
 */

uint32_t compress_chunk_set_model_id_and_counter(void *dst, uint32_t dst_size,
						 uint16_t model_id, uint8_t model_counter);


/**
 * @brief tells if a result is an error code
 *
 * @param code	return value to check
 *
 * @returns non-zero if the code is an error
 */

unsigned int cmp_is_error(uint32_t code);


/**
 * @brief provides a readable string from a compression return value (useful for debugging)
 *
 * @param code	compression return value to describe
 *
 * @returns a pointer to a string literal that describes the error code.
 */

const char* cmp_get_error_name(uint32_t code);


/**
 * @brief convert a function result into a cmp_error enum
 *
 * @param code	compression return value to get the error code
 *
 * @returns error code
 */

enum cmp_error cmp_get_error_code(uint32_t code);


/**
 * @brief get a string describing an error code
 *
 * @param code	the error code to describe, obtain with cmp_get_error_code()
 *
 * @returns a pointer to a string literal that describes the error code.
 */

const char* cmp_get_error_string(enum cmp_error code);


#endif /* CMP_CHUNK_H */

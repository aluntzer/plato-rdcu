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

#include "common/cmp_entity.h"


#define ROUND_UP_TO_4(x) ((((x)+3)*4)/4)


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

#define COMPRESS_CHUNK_BOUND(chunk_size, num_col) ( \
	(uint32_t)ROUND_UP_TO_4(NON_IMAGETTE_HEADER_SIZE+(chunk_size)+(num_col)*CMP_COLLECTION_FILD_SIZE) > (uint32_t)CMP_ENTITY_MAX_SIZE ? 0 \
	: ROUND_UP_TO_4(NON_IMAGETTE_HEADER_SIZE+(chunk_size)+(num_col)*CMP_COLLECTION_FILD_SIZE) \
)


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
 * @param chunk		pointer to the chunk you want compress
 * @param chunk_size	size of the chunk in bytes
 *
 * @returns maximum compressed size for a chunk compression; 0 on error
 */

uint32_t compress_chunk_cmp_size_bound(void *chunk, uint32_t chunk_size);


/**
 * @brief initialise the compress_chunk() function
 *
 * If not initialised the compress_chunk() function sets the timestamps and
 * version_id in the compression entity header to zero
 *
 * @param return_timestamp	pointer to a function returning a current 48-bit
 *				timestamp
 * @param version_id		version identifier of the applications software
 */

void compress_chunk_init(uint64_t(return_timestamp)(void), uint32_t version_id);


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
 *				buffer; has to be 4-byte aligned (can be NULL)
 * @param dst_capacity		capacity of the dst buffer;  it's recommended to
 *				provide a dst_capacity >=
 *				compress_chunk_cmp_size_bound(chunk, chunk_size)
 *				as it eliminates one potential failure scenario:
 *				not enough space in the dst buffer to write the
 *				compressed data; size is round down to a multiple
 *				of 4
 * @returns the byte size of the compressed_data buffer on success; negative on
 *	error, CMP_ERROR_SMALL_BUF (-2) if the compressed data buffer is too
 *	small to hold the whole compressed data
 */

int32_t compress_chunk(void *chunk, uint32_t chunk_size,
		       void *chunk_model, void *updated_chunk_model,
		       uint32_t *dst, uint32_t dst_capacity,
		       const struct cmp_par *cmp_par);

/**
 * @brief set the model id and model counter in the compression entity header
 *
 * @param dst		pointer to the compressed data starting with a
 *			compression entity header
 * @param dst_size	byte size of the dst buffer
 * @param model_id	model identifier; for identifying entities that originate
 *			from the same starting model
 * @param model_counter	model_counter; counts how many times the model was
 *			updated; for non model mode compression use 0
 *
 * @returns 0 on success, otherwise error
 */

int compress_chunk_set_model_id_and_counter(uint32_t *dst, int dst_size,
					    uint16_t model_id, uint8_t model_counter);

#endif /* CMP_CHUNK_H */

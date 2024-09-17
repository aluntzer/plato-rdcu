/**
 * @file   cmp_error.c
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
 * @brief handling errors functions
 */

#include <stdint.h>

#include "cmp_error.h"
#include "cmp_error_list.h"
#include "../cmp_chunk.h"


/**
 * @brief tells if a result is an error code
 *
 * @param code	return value to check
 *
 * @returns non-zero if the code is an error
 */

unsigned int cmp_is_error(uint32_t code)
{
	return (code > CMP_ERROR(MAX_CODE));
}


/**
 * @brief convert a function result into a cmp_error enum
 *
 * @param code	compression return value to get the error code
 *
 * @returns error code
 */

enum cmp_error cmp_get_error_code(uint32_t code)
{
	if (cmp_is_error(code))
		return (enum cmp_error)(0-code);

	return CMP_ERROR_NO_ERROR;
}


/**
 * @brief get a string describing an error code
 *
 * @param code	the error code to describe, obtain with cmp_get_error_code()
 *
 * @returns a pointer to a string literal that describes the error code.
 */

const char* cmp_get_error_string(enum cmp_error code)
{
#ifdef CMP_STRIP_ERROR_STRINGS
	(void)code;
	return "Error strings stripped";
#else
	static const char* const notErrorCode = "Unspecified error code";
	switch (code) {
	case CMP_ERROR_NO_ERROR:
		return "No error detected";
	case CMP_ERROR_GENERIC:
		return "Error (generic)";
	case CMP_ERROR_SMALL_BUF_:
		return "Destination buffer is too small to hold the whole compressed data";
	case CMP_ERROR_DATA_VALUE_TOO_LARGE:
		return "Data value is larger than expected";


	case CMP_ERROR_PAR_GENERIC:
		return "Compression mode or model value or lossy rounding parameter is unsupported";
	case CMP_ERROR_PAR_SPECIFIC:
		return "Specific compression parameters or combination is unsupported";
	case CMP_ERROR_PAR_BUFFERS:
		return "Buffer related parameter is not valid";
	case CMP_ERROR_PAR_NULL:
		return "Pointer to the compression parameters structure is NULL";
	case CMP_ERROR_PAR_NO_MODEL:
		return "Model need for model mode compression";

	case CMP_ERROR_CHUNK_NULL:
		return "Pointer to the chunk is NULL. No data, no compression";
	case CMP_ERROR_CHUNK_TOO_LARGE:
		return "Chunk size too large";
	case CMP_ERROR_CHUNK_TOO_SMALL:
		return "Chunk size too small. Minimum size is the size of a collection header";
	case CMP_ERROR_CHUNK_SIZE_INCONSISTENT:
		return "Chunk size is not consistent with the sum of the sizes in the compression headers. Chunk size may be wrong?";
	case CMP_ERROR_CHUNK_SUBSERVICE_INCONSISTENT:
		return "The chunk contains collections with an incompatible combination of subservices";

	case CMP_ERROR_COL_SUBSERVICE_UNSUPPORTED:
		return "Unsupported collection subservice";
	case CMP_ERROR_COL_SIZE_INCONSISTENT:
		return "Inconsistency detected between the collection subservice and data length";

	case CMP_ERROR_ENTITY_NULL:
		return "Compression entity pointer is NULL";
	case CMP_ERROR_ENTITY_TOO_SMALL:
		return "Compression entity size is too small";
	case CMP_ERROR_ENTITY_HEADER:
		return "An error occurred while generating the compression entity header";
	case CMP_ERROR_ENTITY_TIMESTAMP:
		return "Timestamp too large for the compression entity header";

	case CMP_ERROR_INT_DECODER:
		return "Internal decoder error occurred";
	case CMP_ERROR_INT_DATA_TYPE_UNSUPPORTED:
		return "Internal error: Data type not supported";
	case CMP_ERROR_INT_CMP_COL_TOO_LARGE:
		return "Internal error: compressed collection to large";

	case CMP_ERROR_MAX_CODE:
	default:
		return notErrorCode;
    }
#endif
}


/**
 * @brief provides a readable string from a compression return value (useful for debugging)
 *
 * @param code	compression return value to describe
 *
 * @returns a pointer to a string literal that describes the error code.
 */

const char* cmp_get_error_name(uint32_t code)
{
	return cmp_get_error_string(cmp_get_error_code(code));
}

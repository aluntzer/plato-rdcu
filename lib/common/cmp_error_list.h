/**
 * @file   cmp_error_list.h
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
 * @brief definition of the compression error codes
 */

#ifndef CMP_ERROR_LIST_H
#define CMP_ERROR_LIST_H


/**
 * @brief compression error codes
 * @warning error codes (name and value) are TBC
 */

enum cmp_error {
	CMP_ERROR_NO_ERROR = 0,
	CMP_ERROR_GENERIC = 1,
	CMP_ERROR_SMALL_BUF_ = 2,
	CMP_ERROR_DATA_VALUE_TOO_LARGE = 3,
	/* compression parameter errors */
	CMP_ERROR_PAR_GENERIC = 20,
	CMP_ERROR_PAR_SPECIFIC = 21,
	CMP_ERROR_PAR_BUFFERS = 22,
	/* chunk errors */
	CMP_ERROR_CHUNK_NULL = 40,
	CMP_ERROR_CHUNK_TOO_LARGE = 41,
	CMP_ERROR_CHUNK_TOO_SMALL = 42,
	CMP_ERROR_CHUNK_SIZE_INCONSISTENT = 43,
	CMP_ERROR_CHUNK_SUBSERVICE_INCONSISTENT = 44,
	/* collection errors */
	CMP_ERROR_COL_SUBSERVICE_UNSUPPORTED = 60,
	CMP_ERROR_COL_SIZE_INCONSISTENT = 61,
	/* compression entity errors */
	CMP_ERROR_ENTITY_NULL = 80,
	CMP_ERROR_ENTITY_TOO_SMALL = 81,
	CMP_ERROR_ENTITY_HEADER = 82,
	CMP_ERROR_ENTITY_TIMESTAMP = 83,
	/* internal compressor errors */
	CMP_ERROR_INT_DECODER = 100,
	CMP_ERROR_INT_DATA_TYPE_UNSUPPORTED = 101,
	CMP_ERROR_INT_CMP_COL_TOO_LARGE = 102,

	CMP_ERROR_MAX_CODE = 127 /* Prefer using cmp_is_error() for error checking. */
};

#endif /* CMP_ERROR_LIST_H */

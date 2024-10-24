/**
 * @file   cmp_chunk_type.h
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
 * @brief functions and definitions for determining the chunk type of PLATO data
 */

#ifndef CMP_CHUNK_TYPE_H
#define CMP_CHUNK_TYPE_H

#include "../common/cmp_data_types.h"


/**
 * @brief types of chunks containing different types of collections
 *	according to DetailedBudgetWorking_2023-10-11
 */

enum chunk_type {
	CHUNK_TYPE_UNKNOWN,
	CHUNK_TYPE_NCAM_IMAGETTE,
	CHUNK_TYPE_SHORT_CADENCE,
	CHUNK_TYPE_LONG_CADENCE,
	CHUNK_TYPE_SAT_IMAGETTE,
	CHUNK_TYPE_OFFSET_BACKGROUND, /* N-CAM */
	CHUNK_TYPE_SMEARING,
	CHUNK_TYPE_F_CHAIN
};


enum chunk_type cmp_col_get_chunk_type(const struct collection_hdr *col);

#endif /* CMP_CHUNK_TYPE_H */

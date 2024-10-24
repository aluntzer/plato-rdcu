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

#include "cmp_chunk_type.h"
#include "../common/cmp_debug.h"
#include "../common/cmp_data_types.h"


/**
 * @brief get the chunk_type of a collection
 * @details map a sub-service to a chunk service according to
 *	DetailedBudgetWorking_2023-10-11
 *
 * @param col	pointer to a collection header
 *
 * @returns chunk type of the collection, CHUNK_TYPE_UNKNOWN on
 *	failure
 */

enum chunk_type cmp_col_get_chunk_type(const struct collection_hdr *col)
{
	enum chunk_type chunk_type;

	switch (cmp_col_get_subservice(col)) {
	case SST_NCxx_S_SCIENCE_IMAGETTE:
		chunk_type = CHUNK_TYPE_NCAM_IMAGETTE;
		break;
	case SST_NCxx_S_SCIENCE_SAT_IMAGETTE:
		chunk_type = CHUNK_TYPE_SAT_IMAGETTE;
		break;
	case SST_NCxx_S_SCIENCE_OFFSET:
	case SST_NCxx_S_SCIENCE_BACKGROUND:
		chunk_type = CHUNK_TYPE_OFFSET_BACKGROUND;
		break;
	case SST_NCxx_S_SCIENCE_SMEARING:
		chunk_type = CHUNK_TYPE_SMEARING;
		break;
	case SST_NCxx_S_SCIENCE_S_FX:
	case SST_NCxx_S_SCIENCE_S_FX_EFX:
	case SST_NCxx_S_SCIENCE_S_FX_NCOB:
	case SST_NCxx_S_SCIENCE_S_FX_EFX_NCOB_ECOB:
		chunk_type = CHUNK_TYPE_SHORT_CADENCE;
		break;
	case SST_NCxx_S_SCIENCE_L_FX:
	case SST_NCxx_S_SCIENCE_L_FX_EFX:
	case SST_NCxx_S_SCIENCE_L_FX_NCOB:
	case SST_NCxx_S_SCIENCE_L_FX_EFX_NCOB_ECOB:
		chunk_type = CHUNK_TYPE_LONG_CADENCE;
		break;
	case SST_FCx_S_SCIENCE_IMAGETTE:
	case SST_FCx_S_SCIENCE_OFFSET_VALUES:
	case SST_FCx_S_BACKGROUND_VALUES:
		chunk_type = CHUNK_TYPE_F_CHAIN;
		break;
	case SST_NCxx_S_SCIENCE_F_FX:
	case SST_NCxx_S_SCIENCE_F_FX_EFX:
	case SST_NCxx_S_SCIENCE_F_FX_NCOB:
	case SST_NCxx_S_SCIENCE_F_FX_EFX_NCOB_ECOB:
		debug_print("Error: No chunk is defined for fast cadence subservices");
		/* fall through */
	default:
		chunk_type = CHUNK_TYPE_UNKNOWN;
		break;
	}

	return chunk_type;
}

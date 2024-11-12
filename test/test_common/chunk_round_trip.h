/**
 * @file chunk_round_trip.h
 * @date 2024
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
 * @brief chunk compression/decompression
 */


#ifndef CHUNK_ROUND_TRIP_H
#define CHUNK_ROUND_TRIP_H

#include <stdint.h>
#include "../../lib/cmp_chunk.h"


uint32_t chunk_round_trip(const void *chunk, uint32_t chunk_size,
			  const void *chunk_model, void *updated_chunk_model,
			  uint32_t *dst, uint32_t dst_capacity,
			  const struct cmp_par *cmp_par, int use_decmp_buf,
			  int use_decmp_up_model);


#endif /* CHUNK_ROUND_TRIP_H */


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


uint32_t chunk_round_trip(void *data, uint32_t data_size,
			  void *model, void *up_model,
			  uint32_t *cmp_data, uint32_t cmp_data_capacity,
			  struct cmp_par *cmp_par, int use_decmp_buf, int
			  use_decmp_up_model);


#endif /* CHUNK_ROUND_TRIP_H */


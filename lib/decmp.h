/**
 * @file   decmp.h
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2020
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
 * @brief software decompression library
 */

#ifndef DECMP_H
#define DECMP_H

#include <stdint.h>

#include "common/cmp_entity.h"
#include "common/cmp_support.h"

int decompress_cmp_entiy(const struct cmp_entity *ent, const void *model_of_data,
			 void *up_model_buf, void *decompressed_data);

int decompress_rdcu_data(const uint32_t *compressed_data, const struct cmp_info *info,
			 const uint16_t *model_of_data, uint16_t *up_model_buf,
			 uint16_t *decompressed_data);

#endif /* DECMP_H */

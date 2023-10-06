/**
 * @file   cmp_max_used_bits_list.h
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
 * @brief Implement a list that can hold an arbitrary number of different
 *	cmp_max_used_bits structs
 *
 * @warning not intended for use with the flight software
 */


#ifndef CMP_MAX_USED_LIST_H
#define CMP_MAX_USED_LIST_H

#include <stdint.h>

#include "../common/cmp_data_types.h"


struct cmp_max_used_bits *cmp_max_used_bits_list_get(uint8_t version);

int cmp_max_used_bits_list_add(struct cmp_max_used_bits const *item);

int cmp_max_used_bits_list_delet(uint8_t version);

void cmp_max_used_bits_list_empty(void);

#endif /* CMP_MAX_USED_LIST_H */

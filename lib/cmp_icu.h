/**
 * @file   cmp_icu.h
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
 * @brief software compression library
 * @see Data Compression User Manual PLATO-UVIE-PL-UM-0001
 */

#ifndef CMP_ICU_H
#define CMP_ICU_H

#include <stdint.h>

#include "common/cmp_support.h"

uint32_t compress_like_rdcu(const struct rdcu_cfg *rcfg, struct cmp_info *info);

#endif /* CMP_ICU_H */

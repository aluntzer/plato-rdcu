/**
 * @file   cmp_rdcu_testing.h
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
 * @brief function to test RDCU compression
 * @warning not indented for production use; only for testing
 */

#ifndef CMP_RDCU_TESTING_H
#define CMP_RDCU_TESTING_H

#include "../common/cmp_support.h"

int rdcu_start_compression(void);
int rdcu_inject_edac_error(const struct rdcu_cfg *rcfg, uint32_t addr);
int rdcu_compress_data_parallel(const struct rdcu_cfg *rcfg,
				const struct cmp_info *last_info);

#endif /* CMP_RDCU_TESTING_H */

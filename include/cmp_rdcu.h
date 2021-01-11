/**
 * @file   cmp_rdcu.h
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at),
 * @date   2019
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
 */


#ifndef _CMP_RDCU_H_
#define _CMP_RDCU_H_

#include <stdint.h>
#include "../include/cmp_support.h"


int rdcu_compress_data(const struct cmp_cfg *cfg);

int rdcu_read_cmp_status(struct cmp_status *status);

int rdcu_read_cmp_info(struct cmp_info *info);

int rdcu_read_cmp_bitstream(const struct cmp_info *info, void *output_buf);

int rdcu_read_model(const struct cmp_info *info, void *model_buf);

int rdcu_interrupt_compression(void);

#endif /* _CMP_RDCU_H_ */

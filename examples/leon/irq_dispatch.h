/**
 * @file    leon/irq_dispatch.h
 * @ingroup irq_dispatch
 * @author  Armin Luntzer (armin.luntzer@univie.ac.at)
 * @date    December, 2013
 *
 * @copyright GPLv2
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef IRQ_DISPATCH_H
#define IRQ_DISPATCH_H

#include <stdint.h>

enum prty {PRIORITY_NOW, PRIORITY_LATER};

int32_t irq_dispatch_enable(void);

void irq_queue_execute(void);

void irq_set_level(uint32_t irq_mask, uint32_t level);

int32_t irl1_register_callback(int32_t irq, enum prty priority, int32_t (* callback)(void *userdata), void *userdata);
int32_t irl2_register_callback(int32_t irq, enum prty priority, int32_t (* callback)(void *userdata), void *userdata);

int32_t irl1_deregister_callback(int32_t irq, int32_t(* callback)(void *userdata), void *userdata);
int32_t irl2_deregister_callback(int32_t irq, int32_t(* callback)(void *userdata), void *userdata);


#endif

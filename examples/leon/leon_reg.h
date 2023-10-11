/**
 * @file leon/leon_reg.h
 *
 * @author  Armin Luntzer (armin.luntzer@univie.ac.at)
 * @date    2015
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
 * @brief layout of some registers in the GR712RC
 *
 */

#ifndef LEON_REG_H
#define LEON_REG_H

#include <stdint.h>

/**
 * helper macro to fill unused 4-byte slots with "unusedXX" variables
 * yes, this needs to be done exactly like that, or __COUNTER__ will not be expanded
 */
#define CONCAT(x, y)		x##y
#define MACRO_CONCAT(x, y) 	CONCAT(x, y)
#define UNUSED_UINT32_SLOT	 uint32_t MACRO_CONCAT(unused, __COUNTER__)


#define CORE1553BRM_REG_BASE		0xFFF00000

/* LEON3 APB base address is at 0x80000000
 * documentation appears to be  conflicting sometimes, your mileage may vary
 * offsets from base are indicated to the right for orientation
 */

#define LEON3_BASE_ADDRESS_APB		0x80000000

#define LEON3_BASE_ADDRESS_FTMCTRL	0x80000000
#define LEON3_BASE_ADDRESS_APBUART	0x80000100
#define LEON3_BASE_ADDRESS_IRQMP	0x80000200
#define LEON3_BASE_ADDRESS_GPTIMER	0x80000300
#define LEON3_BASE_ADDRESS_GRGPIO_1	0x80000900
#define LEON3_BASE_ADDRESS_GRGPIO_2	0x80000A00
#define LEON3_BASE_ADDRESS_AHBSTAT	0x80000F00
#define LEON3_BASE_ADDRESS_GRTIMER	0x80100600

/* FTMCTRL Memory Controller Registers [p. 44] */

struct leon3_ftmctrl_registermap {
	 uint32_t mcfg1;
	 uint32_t mcfg2;
	 uint32_t mcfg3;
};

struct leon3_apbuart_registermap {
	 uint32_t data;
	 uint32_t status;
	 uint32_t ctrl;
	 uint32_t scaler;
};

struct leon3_grgpio_registermap {
	uint32_t ioport_data;
	uint32_t ioport_output_value;
	uint32_t ioport_direction;
	uint32_t irq_mask;
	uint32_t irq_polarity;
	uint32_t irq_edge;
};

struct leon3_irqctrl_registermap {
	uint32_t irq_level;			/* 0x00	*/
	uint32_t irq_pending;			/* 0x04 */
	uint32_t irq_force;			/* 0x08 */
	uint32_t irq_clear;			/* 0x0C */
	uint32_t mp_status;			/* 0x10 */
	uint32_t mp_broadcast;			/* 0x14 */
	UNUSED_UINT32_SLOT;			/* 0x18 */
	UNUSED_UINT32_SLOT;			/* 0x1C */
	UNUSED_UINT32_SLOT;			/* 0x20 */
	UNUSED_UINT32_SLOT;			/* 0x24 */
	UNUSED_UINT32_SLOT;			/* 0x28 */
	UNUSED_UINT32_SLOT;			/* 0x2C */
	UNUSED_UINT32_SLOT;			/* 0x30 */
	UNUSED_UINT32_SLOT;			/* 0x34 */
	UNUSED_UINT32_SLOT;			/* 0x38 */
	UNUSED_UINT32_SLOT;			/* 0x3C */
	uint32_t irq_mpmask[2];			/* 0x40 CPU 0 */
						/* 0x44 CPU 1 */
	UNUSED_UINT32_SLOT;			/* 0x48 */
	UNUSED_UINT32_SLOT;			/* 0x4C */
	UNUSED_UINT32_SLOT;			/* 0x50 */
	UNUSED_UINT32_SLOT;			/* 0x54 */
	UNUSED_UINT32_SLOT;			/* 0x58 */
	UNUSED_UINT32_SLOT;			/* 0x5C */
	UNUSED_UINT32_SLOT;			/* 0x60 */
	UNUSED_UINT32_SLOT;			/* 0x64 */
	UNUSED_UINT32_SLOT;			/* 0x68 */
	UNUSED_UINT32_SLOT;			/* 0x6C */
	UNUSED_UINT32_SLOT;			/* 0x70 */
	UNUSED_UINT32_SLOT;			/* 0x74 */
	UNUSED_UINT32_SLOT;			/* 0x78 */
	UNUSED_UINT32_SLOT;			/* 0x7C */
	uint32_t irq_mpforce[2];		/* 0x80 CPU 0*/
						/* 0x84 CPU 1*/
	UNUSED_UINT32_SLOT;			/* 0x88 */
	UNUSED_UINT32_SLOT;			/* 0x8C */
	UNUSED_UINT32_SLOT;			/* 0x90 */
	UNUSED_UINT32_SLOT;			/* 0x94 */
	UNUSED_UINT32_SLOT;			/* 0x98 */
	UNUSED_UINT32_SLOT;			/* 0x9C */
	UNUSED_UINT32_SLOT;			/* 0xA0 */
	UNUSED_UINT32_SLOT;			/* 0xA4 */
	UNUSED_UINT32_SLOT;			/* 0xA8 */
	UNUSED_UINT32_SLOT;			/* 0xAC */
	UNUSED_UINT32_SLOT;			/* 0xB0 */
	UNUSED_UINT32_SLOT;			/* 0xB4 */
	UNUSED_UINT32_SLOT;			/* 0xB8 */
	UNUSED_UINT32_SLOT;			/* 0xBC */
	uint32_t extended_irq_id[2];		/* 0xC0 CPU 0*/
						/* 0xC4 CPU 1*/
};



struct leon3_ahbstat_registermap {
	uint32_t status;
	uint32_t failing_address;
};



struct gptimer_timer {
	uint32_t value;
	uint32_t reload;
	uint32_t ctrl;
	uint32_t unused;
};

struct gptimer_unit {
	uint32_t scaler;
	uint32_t scaler_reload;
	uint32_t config;
	uint32_t unused;
	struct gptimer_timer timer[4];
};

struct grtimer_timer {
	uint32_t value;
	uint32_t reload;
	uint32_t ctrl;
	uint32_t latch_value;
};

struct grtimer_unit {
	uint32_t scaler;
	uint32_t scaler_reload;
	uint32_t config;

	uint32_t irq_select;

	struct grtimer_timer timer[2];
};

#endif

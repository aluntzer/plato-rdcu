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
	uint32_t unused_0;			/* 0x18 */
	uint32_t unused_1;			/* 0x1C */
	uint32_t unused_2;			/* 0x20 */
	uint32_t unused_3;			/* 0x24 */
	uint32_t unused_4;			/* 0x28 */
	uint32_t unused_5;			/* 0x2C */
	uint32_t unused_6;			/* 0x30 */
	uint32_t unused_7;			/* 0x34 */
	uint32_t unused_8;			/* 0x38 */
	uint32_t unused_9;			/* 0x3C */
	uint32_t irq_mpmask[2];			/* 0x40 CPU 0 */
						/* 0x44 CPU 1 */
	uint32_t unused_10;			/* 0x48 */
	uint32_t unused_11;			/* 0x4C */
	uint32_t unused_12;			/* 0x50 */
	uint32_t unused_13;			/* 0x54 */
	uint32_t unused_14;			/* 0x58 */
	uint32_t unused_15;			/* 0x5C */
	uint32_t unused_16;			/* 0x60 */
	uint32_t unused_17;			/* 0x64 */
	uint32_t unused_18;			/* 0x68 */
	uint32_t unused_19;			/* 0x6C */
	uint32_t unused_20;			/* 0x70 */
	uint32_t unused_21;			/* 0x74 */
	uint32_t unused_22;			/* 0x78 */
	uint32_t unused_23;			/* 0x7C */
	uint32_t irq_mpforce[2];		/* 0x80 CPU 0*/
						/* 0x84 CPU 1*/
	uint32_t unused_24;			/* 0x88 */
	uint32_t unused_25;			/* 0x8C */
	uint32_t unused_26;			/* 0x90 */
	uint32_t unused_27;			/* 0x94 */
	uint32_t unused_28;			/* 0x98 */
	uint32_t unused_29;			/* 0x9C */
	uint32_t unused_30;			/* 0xA0 */
	uint32_t unused_31;			/* 0xA4 */
	uint32_t unused_32;			/* 0xA8 */
	uint32_t unused_33;			/* 0xAC */
	uint32_t unused_34;			/* 0xB0 */
	uint32_t unused_35;			/* 0xB4 */
	uint32_t unused_36;			/* 0xB8 */
	uint32_t unused_37;			/* 0xBC */
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

/**
 * @file   leon3_timers.h
 * @ingroup timing
 * @author Armin Luntzer (armin.luntzer@univie.ac.at),
 * @date   July, 2016
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
 */


#ifndef LEON3_TIMERS_H
#define LEON3_TIMERS_H


#define LEON3_TIMER_EN	0x00000001       /* enable counting */
#define LEON3_TIMER_RS	0x00000002       /* restart from timer reload value */
#define LEON3_TIMER_LD	0x00000004       /* load counter    */
#define LEON3_TIMER_IE	0x00000008       /* irq enable      */
#define LEON3_TIMER_IP	0x00000010       /* irq pending (clear by writing 0 */
#define LEON3_TIMER_CH	0x00000020       /* chain with preceeding timer */

#define LEON3_CFG_TIMERS_MASK	0x00000007
#define LEON3_CFG_IRQNUM_MASK	0x000000f8
#define LEON3_CFG_IRQNUM_SHIFT	       0x3

#endif

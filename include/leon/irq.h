/**
 * @file leon/irq.h
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
 * @brief a list of IRQ numbers used in the GR712RC
 */


#ifndef IRQ_H
#define IRQ_H

/**
 * Note: some IRQs are shared betweend devices; these devices cannot usually be used a the same time and
 * are selected by programming the appropriate jumpers on the board
 */

/**
 *  IRQs 1-15 are external I/O interrupts
 */

#define GR712_IRL1_AHBSTAT	1	/* AHB status register, bus error				*/
#define GR712_IRL1_APBUART_0	2	/* UART0 RX/TX interrupt (uart0)				*/
#define GR712_IRL1_GRGPIO_0	3	/* GPIO0							*/
#define GR712_IRL1_GRGPIO_1	4	/* GPIO1							*/
#define GR712_IRL1_CANOC_0	5	/* OC CAN AHB interface (core 1)				*/
#define GR712_IRL1_CANOC_1	6	/* OC CAN AHB interface (core 2)				*/
#define GR712_IRL1_GRTIMER	7	/* Timer Unit with Latches (grtimer0); underflow		*/
#define GR712_IRL1_GPTIMER_0	8	/* Modular Timer Unit 0 (gptimer0)				*/
#define GR712_IRL1_GPTIMER_1	9	/* Modular Timer Unit 1						*/
#define GR712_IRL1_GPTIMER_2	10	/* Modular Timer Unit 2						*/
#define GR712_IRL1_GPTIMER_3	11	/* Modular Timer Unit 3						*/
#define GR712_IRL1_IRQMP	12	/* Multi-Processor Interrupt Control (chained IRQ controller)	*/
#define GR712_IRL1_SPICTRL	13	/* SPI Controller	(spi0)					*/
#define GR712_IRL1_SLINK	13	/* SLINK Master		(adev14)				*/
#define GR712_IRL1_B1553BRM	14	/* AMBA Wrapper for Core1553BRM (b1553brm0)			*/
#define GR712_IRL1_GRETH	14	/* GR Ethernet MAC (greth0)					*/
#define GR712_IRL1_GRTC		14	/* CCSDS Telecommand Decoder (grtc0)				*/
#define GR712_IRL1_SATCAN	14	/* SatCAN controller (satcan0)					*/
/* IRQ 15 is not maskable and not mapped to a particular function in GR712 */


/**
 * IRQ 16-31 are generated as IRQ 12 on the chained interrupt controller
 */

#define GR712_IRL2_ASCS		16	/* ASCS Master (adev27)						*/
#define GR712_IRL2_APBUART_1	17	/* UART1 RX/TX interrupt (uart1)				*/
#define GR712_IRL2_APBUART_2	18	/* UART2 RX/TX interrupt (uart2)				*/
#define GR712_IRL2_APBUART_3	19	/* UART3 RX/TX interrupt (uart3)				*/
#define GR712_IRL2_APBUART_4	20	/* UART4 RX/TX interrupt (uart4)				*/
#define GR712_IRL2_APBUART_5	21	/* UART5 RX/TX interrupt (uart5)				*/
#define GR712_IRL2_GRSPW2_0	22	/* GRSPW2 SpaceWire Serial Link RX/TX data interrupt (grspw0)	*/
#define GR712_IRL2_GRSPW2_1	23	/* GRSPW2 SpaceWire Serial Link RX/TX data interrupt (grspw1)	*/
#define GR712_IRL2_GRSPW2_2	24	/* GRSPW2 SpaceWire Serial Link RX/TX data interrupt (grspw2)	*/
#define GR712_IRL2_GRSPW2_3	25	/* GRSPW2 SpaceWire Serial Link RX/TX data interrupt (grspw3)	*/
#define GR712_IRL2_GRSPW2_4	26	/* GRSPW2 SpaceWire Serial Link RX/TX data interrupt (grspw4)	*/
#define GR712_IRL2_GRSPW2_5	27	/* GRSPW2 SpaceWire Serial Link RX/TX data interrupt (grspw5)	*/
#define GR712_IRL2_I2CMST	28	/* AMBA Wrapper for OC I2C-Master  (i2cmst0)			*/
#define GR712_IRL2_GRTM		29	/* CCSDS Telemetry Encoder interrupt				*/
#define GR712_IRL2_GRTMTS	30	/* CCSDS Telemetry Encoder time strobe interrupt		*/


/**
 * chained IRQ controller interrupt
 */

#define IRL1_EXTENDED_INT	GR712_IRL1_IRQMP


/** aliases */

#define GR712_IRL1_GPIO1	1
#define GR712_IRL1_GPIO2	2
#define GR712_IRL1_GPIO3	3
#define GR712_IRL1_GPIO4	4
#define GR712_IRL1_GPIO5	5
#define GR712_IRL1_GPIO6	6
#define GR712_IRL1_GPIO7	7
#define GR712_IRL1_GPIO8	8
#define GR712_IRL1_GPIO9	9
#define GR712_IRL1_GPIO10	10
#define GR712_IRL1_GPIO11	11
#define GR712_IRL1_GPIO12	12
#define GR712_IRL1_GPIO13	13
#define GR712_IRL1_GPIO14	14
#define GR712_IRL1_GPIO15	15










#endif

/**
 * @file   cfg.h
 * @author Armin Luntzer (armin.luntzer@univie.ac.at),
 * @date   2018
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
 * @brief RMAP RDCU usage demonstrator configuration
 */

#ifndef _CFG_H_
#define _CFG_H_

#include <stdint.h>


/* node addresses of ICU and RDCU */
#define ICU_ADDR	0x28	/* PLATO-IWF-PL-TR-0042-d0-1_RDCU_Functional_Test_PT1 */
#define RDCU_ADDR	0x5A	/* as above */
#define RDCU_ADDR_START	0xFE	/* RDCU-FRS-FN-0256 */

/* physical router ports
 * see PLATO-IWF-PL-DW-0021-d0-1_RDCU_PT_MAIN_Circuit_Diagram
 * and PLATO-IWF-PL-DW-0022-d0-1_RDCU_PT_DB_Circuit_Diagram
 */

#define ICU_PHYS_PORT	0x01
#define RDCU_PHYS_PORT	0x11

#define DPATH	{RDCU_PHYS_PORT}
#define RPATH	{0x00, 0x00, 0x00, ICU_PHYS_PORT}
#define DPATH_LEN	1
#define RPATH_LEN	4

/* default RDCU destkey, see RDCU-FRS-FN-0260 */
#define RDCU_DEST_KEY	0x0


/* set header size maximum so we can fit the largest possible rmap commands
 * given the hardware limit (HEADERLEN is 8 bits wide, see GR712RC UM, p. 112)
 */
#define HDR_SIZE	0xFF



/* default dividers for GR712RC eval board: 10 Mbit start, 100 Mbit run */
/* XXX: check if we have to set the link start/run speeds to 10 Mbit for
 * both states initally, and reconfigure later, as there will be a RMAP command
 * for the RDCU configuring it to 100 Mbit
 */
#define SPW_CLCKDIV_START	10
#define SPW_CLCKDIV_RUN		1

#endif /* _CFG_H_ */

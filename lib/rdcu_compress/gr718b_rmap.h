/**
 * @file   gr718b_rmap.h
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
 * @see GR718B 18x SpaceWire Router 2018 Data Sheet and User's Manual
 *
 * @note only the currently needed subset of GR718B functionality is implemented
 *
 */


#ifndef _GR718B_RMAP_H_
#define _GR718B_RMAP_H_

#include <stdint.h>



#define GR718B_GRSPWROUTER_RMAP_START		0x00000000UL
#define GR718B_GRSPWROUTER_RMAP_END		0x00001FFCUL

#define GR718B_RMAP_CFG_PORT			0
#define GR718B_RMAP_SIST_PORT			19

/* externally connectable SpW ports (physical addresses) */
#define GR718B_PHYS_PORT_START			1
#define GR718B_PHYS_PORT_END			19
#define GR718B_PHYS_PORTS	(GR718B_PHYS_PORT_END - GR718B_PHYS_PORT_START)

/* logical addresses */
#define GR718B_LOG_ADDR_START			32
#define GR718B_LOG_ADDR_END			255

/* port 0 logical address and rmap command key, see GR718B-DS-UM-v3.3 6.5.1 */
#define GR718B_RMAP_CFG_PORT_TLA		0xFE
#define GR718B_RMAP_CFG_PORT_DEST_KEY		0x00


/* configuration register base addresses, see GR718B-DS-UM-v3.3 6.5.3 */
/* routing table port mapping */
#define GR718B_RMAP_RTPMAP_BASE		0x00000000UL
/* routing table address control */
#define GR718B_RMAP_RTACTRL_BASE	0x00000400UL
/* port control */
#define GR718B_RMAP_PCTRL_BASE		0x00000800UL
/* physical port status */
#define GR718B_RMAP_PSTS_BASE		0x00000880UL



/* bits in a RTACTRL register */
#define GR718B_RTACTRL_HDRDEL_BIT		0
#define GR718B_RTACTRL_PRTY_BIT			1
#define GR718B_RTACTRL_ENABLE_BIT		2
#define GR718B_RTACTRL_SPILL_BIT		3


/* bits in a PCTRL register */

#define GR718B_PCTRL_LINK_START_BIT		 1
#define GR718B_PCTRL_TIME_CODE_ENABLE_BIT	 5

#define GR718B_PCTRL_RUN_CLK_DIV_SHIFT		24
#define GR718B_PCTRL_RUN_CLK_DIV_WIDTH		 8






/* RTPMAP */
int gr718b_set_route_port(uint8_t spw_addr, uint8_t port);
int gr718b_clear_route_port(uint8_t addr, uint8_t port);


/* RTACTRL */
int gr718b_set_addr_header_deletion(uint8_t addr);
int gr718b_clear_addr_header_deletion(uint8_t addr);

int gr718b_set_rtactrl_enabled(uint8_t addr);
int gr718b_clear_rtactrl_enabled(uint8_t addr);


/* PCTRL */
int gr718b_set_rt_clkdiv(uint8_t port, uint8_t clkdiv);

int gr718b_set_link_start(uint8_t port);
int gr718b_clear_link_start(uint8_t port);

int gr718b_set_time_code_enable(uint8_t port);
int gr718b_clear_time_code_enable(uint8_t port);






int gr718b_rmap_init(uint8_t addr,
		     int32_t (*tx)(const void *hdr,  uint32_t hdr_size,
				   const uint8_t non_crc_bytes,
				   const void *data, uint32_t data_size),
		     uint32_t (*rx)(uint8_t *pkt));






#endif /* _GR718B_RMAP_H_ */

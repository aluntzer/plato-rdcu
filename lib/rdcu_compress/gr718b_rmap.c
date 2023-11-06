/**
 * @file   gr718b_rmap.c
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
 * @brief RMAP control library for the GR718B SpaceWire Router
 *
 * @see GR718B 18x SpaceWire Router 2018 Data Sheet and User's Manual
 *
 * @note this implements only a subset of functions relevant to the switch
 *	 matrix, not the board as a whole
 *
 * 	 The interface requires that you provide an RX and a TX function,
 *	 see rdcu_ctrl_init for the call interface.
 *	 The TX function shall to return 0 on success, everything else
 *	 is considered an error in submission. The RX function shall return
 *	 the size of the packet buffer and accept NULL as call argument, on
 *	 which it shall return the buffer size required to store the next
 *	 pending packet.
 *	 You can use these functions to adapt the actual backend, i.e. use
 *	 your particular SpW interface or just redirect RX/TX to files
 *	 or via a network connection.
 *
 * @warn - when operational, we expect to have exclusive control of the SpW link
 *	 - we actively wait for an RMAP response following each command, so
 *	   this has the potential to get stuck if the remote is dead;
 *	   add timeout detection suitable to your operating system, board or
 *	   particular SpW interface
 */


#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rmap.h"
#include "gr718b_rmap.h"


/* source logical address */
static uint8_t src_tla;

/* generic calls, functions must be provided to init() */
static int32_t (*rmap_tx)(const void *hdr,  uint32_t hdr_size,
			  const uint8_t non_crc_bytes,
			  const void *data, uint32_t data_size);
static uint32_t (*rmap_rx)(uint8_t *pkt);



/**
 * @brief generate an rmap command packet for the GR718B rmap configuration port
 *
 * @param trans_id a transaction identifier
 *
 * @param cmd the command buffer; if NULL, the function returns the needed size
 *
 * @param rmap_cmd_type the rmap command type of the packet
 *
 * @param addr the address to read from or write to
 *
 * @param size the number of bytes to read or write
 *
 * @returns the size of the command data buffer or 0 on error
 */

int gr718b_gen_cmd(uint16_t trans_id, uint8_t *cmd,
		   uint8_t rmap_cmd_type,
		   uint32_t addr, uint32_t size)
{
	int n;

	struct rmap_pkt *pkt;
	const uint8_t dpath[] = {GR718B_RMAP_CFG_PORT};

	pkt = rmap_create_packet();
	if (!pkt) {
		printf("Error creating packet\n");
		return 0;
	}

	rmap_set_dst(pkt, GR718B_RMAP_CFG_PORT_TLA);
	rmap_set_dest_path(pkt, dpath, 1);

	rmap_set_src(pkt, src_tla);
	rmap_set_key(pkt, GR718B_RMAP_CFG_PORT_DEST_KEY);
	rmap_set_cmd(pkt, rmap_cmd_type);
	rmap_set_tr_id(pkt, trans_id);
	rmap_set_data_addr(pkt, addr);
	rmap_set_data_len(pkt, size);

	/* determine header size */
	n = rmap_build_hdr(pkt, NULL);

	if (!cmd) {
		rmap_erase_packet(pkt);
		return n;
	}

	memset(cmd, 0, n);  /* clear command buffer */

	n = rmap_build_hdr(pkt, cmd);

	rmap_erase_packet(pkt);

	return n;
}


/**
 * @brief issue a RMW RMAP command to configure a 4 byte wide register
 *
 * @param reg the address of the register
 * @param data the bits to set
 * @param mask the mask to apply
 *
 * @note all operations on a register are reg = (reg & ~mask) | (data & mask)
 *
 * @returns 0 on success, otherwise error
 */

static int gr718b_rmw_reg(uint32_t reg, uint32_t data, uint32_t mask)
{
	int n;
	uint32_t s = 0;

	uint8_t cmd[32];
	uint8_t payload[8];


	if (!rmap_tx)
		return -1;

	if (!rmap_rx)
		return -1;


	/* let's avoid type punning... */
	memcpy(&payload[0], (void *) &data, 4);
	memcpy(&payload[4], (void *) &mask, 4);

	n = gr718b_gen_cmd(0x0, cmd, RMAP_READ_MODIFY_WRITE_ADDR_INC, reg, 8);
	n = rmap_tx(cmd, n, 1, payload, 8);
	if (n) {
		printf("Error in rmap_tx()\n");
		return -1;
	}

	do {
		s = rmap_rx(NULL);
	} while (!s);
	if (s > sizeof(cmd)) {
		printf("Error in rmap_rx(). Response larger than expected.\n");
		return -1;
	}
	rmap_rx(cmd);
#ifndef SKIP_CMP_PAR_CHECK
	rmap_parse_pkt(cmd);
#endif
	return 0;
}


/**
 * @brief get the address of a routing table port mapping (RTPMAP) register
 * @param spw_addr the corresponding SpW physical or logical address
 *
 * @returns the computed register address
 *
 * @warn This does not verify the validity of the spw_addr paramter, make sure
 *	 to use gr718b_verify_port(), gr718b_verify_addr() etc. beforehand.
 */

static uint32_t gr718b_get_rmap_rtpmap_reg(uint8_t spw_addr)
{
	/* the registers are 4 bytes wide */
	return  GR718B_RMAP_RTPMAP_BASE + (uint32_t) spw_addr * 4UL;
}


/**
 * @brief get the address of a routing table address control (RTACTRL) register
 * @param spw_addr the corresponding SpW physical or logical address
 *
 * @returns the computed register address
 *
 * @warn This does not verify the validity of the spw_addr paramter, make sure
 *	 to use gr718b_verify_port(), gr718b_verify_addr() etc. beforehand.
 */

static uint32_t gr718b_get_rmap_rtactrl_reg(uint8_t spw_addr)
{
	/* the registers are 4 bytes wide */
	return  GR718B_RMAP_RTACTRL_BASE + (uint32_t) spw_addr * 4UL;
}


/**
 * @brief get the address of a port control (PCTRL) register
 * @param spw_addr the corresponding SpW physical address
 *
 * @returns the computed register address
 *
 * @warn This does not verify the validity of the spw_addr paramter, make sure
 *	 to use gr718b_verify_port_physical() beforehand.
 */

static uint32_t gr718b_get_rmap_pctrl_reg(uint8_t port)
{
	/* the registers are 4 bytes wide */
	return  GR718B_RMAP_PCTRL_BASE + (uint32_t) port * 4UL;
}


/**
 * @brief verify whether a physical port (1-19) is in a valid range
 *
 * @param port the port number to verify
 *
 * @returns 0 if valid, otherwise error
 *
 * @note RMAP config port 0 is special, so this is not covered here
 */

static int gr718b_verify_port(uint8_t port)
{
	if (!port)
		return -1;

	if (port > GR718B_PHYS_PORT_END)
		return -1;

	return 0;
}


/**
 * @brief verify whether a physical (1-19) or logical address (32-255) is in a
 *	  valid range
 *
 * @param addr the address to verify
 *
 * @returns 0 if valid, otherwise error
 *
 * @note RMAP config port address 0 is special, so this is not covered here
 */

static int gr718b_verify_addr(uint8_t addr)
{
	if (!addr)
		return -1;

	if (addr > GR718B_PHYS_PORT_END)
		if (addr < GR718B_LOG_ADDR_START)
			return -1;

	return 0;
}


/**
 * @brief verify whether a SpW address correspons to a physical port (1-19)
 *
 * @param addr the address to verify
 *
 * @returns 0 if valid, otherwise error
 */

static int gr718b_verify_port_physical(uint8_t addr)
{
	if (gr718b_verify_addr(addr))
		return -1;

	if (addr > GR718B_PHYS_PORT_END)
		return -1;

	return 0;
}


/**
 * @brief set a route for a physical/logical address to a physical port
 *
 * @param addr the physical/logical address to enable (1-19, 32-255)
 * @param port the physical port to route towards (1-19)
 *
 * @returns 0 on success, otherwise error
 */

int gr718b_set_route_port(uint8_t addr, uint8_t port)
{
	uint32_t reg;
	uint32_t bits;


	if (gr718b_verify_port(port))
		return -1;

	if (gr718b_verify_addr(addr))
		return -1;

	bits = (1UL << port);
	reg = gr718b_get_rmap_rtpmap_reg(addr);

	/* data == mask */
	return gr718b_rmw_reg(reg, bits, bits);
}


/**
 * @brief clear a route for a physical/logical address to a physical port
 *
 * @param addr the physical/logical address to enable (1-19, 32-255)
 * @param port the physical port to route towards (1-19)
 *
 * @returns 0 on success, otherwise error
 */

int gr718b_clear_route_port(uint8_t addr, uint8_t port)
{
	uint32_t reg;
	uint32_t mask;


	if (gr718b_verify_port(port))
		return -1;

	if (gr718b_verify_addr(addr))
		return -1;

	mask = (1UL << port);
	reg = gr718b_get_rmap_rtpmap_reg(addr);

	/* data == mask */
	return gr718b_rmw_reg(reg, 0x0, mask);
}



/**
 * @brief set a routing table access control logical address header deletion bit
 *
 * @param addr the logical address to enable (32-255, physical ports
 *        are always enabled)
 *
 * @returns 0 on success, otherwise error
 */

int gr718b_set_addr_header_deletion(uint8_t addr)
{
	uint32_t reg;
	uint32_t bits;


	if (gr718b_verify_addr(addr))
		return -1;

	/* always enabled */
	if (addr <= GR718B_PHYS_PORT_END)
		return 0;


	bits = (1UL << GR718B_RTACTRL_HDRDEL_BIT);
	reg = gr718b_get_rmap_rtactrl_reg(addr);

	/* data == mask */
	return gr718b_rmw_reg(reg, bits, bits);
}


/**
 * @brief clear a routing table access control logical address header
 *	  deletion bit
 *
 * @param addr the logical address to dsiable (32-255, physical ports
 *        are always enabled and cannot be disabled)
 *
 * @returns 0 on success, otherwise error
 */

int gr718b_clear_addr_header_deletion(uint8_t addr)
{
	uint32_t reg;
	uint32_t mask;


	if (gr718b_verify_addr(addr))
		return -1;

	/* always enabled */
	if (addr <= GR718B_PHYS_PORT_END)
		return 0;


	mask = (1UL << GR718B_RTACTRL_HDRDEL_BIT);
	reg = gr718b_get_rmap_rtactrl_reg(addr);

	return gr718b_rmw_reg(reg, 0x0, mask);
}




/**
 * @brief set a routing table access control enabled bit
 *
 * @param addr the logical address to enable (32-255, physical ports
 *        are always enabled)
 *
 * @returns 0 on success, otherwise error
 */

int gr718b_set_rtactrl_enabled(uint8_t addr)
{
	uint32_t reg;
	uint32_t bits;


	if (gr718b_verify_addr(addr))
		return -1;

	/* always enabled */
	if (addr <= GR718B_PHYS_PORT_END)
		return 0;


	bits = (1UL << GR718B_RTACTRL_ENABLE_BIT);
	reg = gr718b_get_rmap_rtactrl_reg(addr);

	/* data == mask */
	return gr718b_rmw_reg(reg, bits, bits);
}


/**
 * @brief clear a routing table access control enable bit
 *
 * @param addr the logical address to dsiable (32-255, physical ports
 *        are always enabled and cannot be disabled)
 *
 * @returns 0 on success, otherwise error
 */

int gr718b_clear_rtactrl_enabled(uint8_t addr)
{
	uint32_t reg;
	uint32_t mask;


	if (gr718b_verify_addr(addr))
		return -1;

	/* always enabled */
	if (addr <= GR718B_PHYS_PORT_END)
		return 0;


	mask = (1UL << GR718B_RTACTRL_ENABLE_BIT);
	reg = gr718b_get_rmap_rtactrl_reg(addr);

	return gr718b_rmw_reg(reg, 0x0, mask);
}


/**
 * @brief set the run state clock divider of a physical port
 *
 * @param addr the physical address of the port to configure (1-19)
 * @param clkdiv the run-state clock divisor to set (clkdiv = divisor - 1)
 *
 * @returns 0 on success, otherwise error
 */

int gr718b_set_rt_clkdiv(uint8_t port, uint8_t clkdiv)
{
	uint32_t reg;
	uint32_t data;

	const uint32_t mask = ((1UL << GR718B_PCTRL_RUN_CLK_DIV_WIDTH) - 1)
		<< GR718B_PCTRL_RUN_CLK_DIV_SHIFT;


	if (gr718b_verify_port_physical(port))
		return -1;

	data = ((uint32_t) clkdiv << GR718B_PCTRL_RUN_CLK_DIV_SHIFT);
	reg = gr718b_get_rmap_pctrl_reg(port);

	return gr718b_rmw_reg(reg, data, mask);
}


/**
 * @brief set a port control link start bit
 *
 * @param addr the physical address of the port to configure (1-19)
 *
 * @returns 0 on success, otherwise error
 */

int gr718b_set_link_start(uint8_t port)
{
	uint32_t reg;
	uint32_t bits;


	if (gr718b_verify_port_physical(port))
		return -1;

	bits = (1UL << GR718B_PCTRL_LINK_START_BIT);
	reg = gr718b_get_rmap_pctrl_reg(port);

	/* data == mask */
	return gr718b_rmw_reg(reg, bits, bits);
}


/**
 * @brief clear a port control link start bit
 *
 * @param addr the physical address of the port to configure (1-19)
 *
 * @returns 0 on success, otherwise error
 */

int gr718b_clear_link_start(uint8_t port)
{
	uint32_t reg;
	uint32_t mask;


	if (gr718b_verify_port_physical(port))
		return -1;

	mask = (1UL << GR718B_PCTRL_LINK_START_BIT);
	reg = gr718b_get_rmap_pctrl_reg(port);

	return gr718b_rmw_reg(reg, 0x0, mask);
}


/**
 * @brief set a port control time code enable bit
 *
 * @param addr the physical address of the port to configure (1-19)
 *
 * @returns 0 on success, otherwise error
 */

int gr718b_set_time_code_enable(uint8_t port)
{
	uint32_t reg;
	uint32_t bits;


	if (gr718b_verify_port_physical(port))
		return -1;

	bits = (1UL << GR718B_PCTRL_TIME_CODE_ENABLE_BIT);
	reg = gr718b_get_rmap_pctrl_reg(port);

	/* data == mask */
	return gr718b_rmw_reg(reg, bits, bits);
}


/**
 * @brief clear a port control time code enable bit
 *
 * @param addr the physical address of the port to configure (1-19)
 *
 * @returns 0 on success, otherwise error
 */

int gr718b_clear_time_code_enable(uint8_t port)
{
	uint32_t reg;
	uint32_t mask;


	if (gr718b_verify_port_physical(port))
		return -1;

	mask = (1UL << GR718B_PCTRL_TIME_CODE_ENABLE_BIT);
	reg = gr718b_get_rmap_pctrl_reg(port);

	return gr718b_rmw_reg(reg, 0x0, mask);
}



/**
 * @brief initialise the gr718b control library
 *
 * @param addr the logical address of the initiator
 *
 * @param rmap_tx a function pointer to transmit an rmap command
 * @param rmap_rx function pointer to receive an rmap command
 *
 * @note rmap_tx is expected to return 0 on success
 *	 rmap_rx is expected to return the number of packet bytes
 *
 * @returns 0 on success, otherwise error
 */

int gr718b_rmap_init(uint8_t addr,
		     int32_t (*tx)(const void *hdr,  uint32_t hdr_size,
				   const uint8_t non_crc_bytes,
				   const void *data, uint32_t data_size),
		     uint32_t (*rx)(uint8_t *pkt))
{
	if (!tx)
		return -1;

	if (!rx)
		return -1;

	src_tla = addr;
	rmap_tx = tx;
	rmap_rx = rx;

	return 0;
}

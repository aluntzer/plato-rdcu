/**
 * @file   errors.h
 * @ingroup grspw2
 * @author Armin Luntzer (armin.luntzer@univie.ac.at),
 * @date   2015
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
 * @brief this extends errno.h definitions for more detailed error tracking
 *
 * The BCC errno.h implementation sets an offset after which custom error
 * number may be defined. We set up classes of errors for some of the
 * software in the IBSW package, each offset by 100 from each other.
 */

#ifndef ERRORS_H
#define ERRORS_H


#include <errno.h>

/*
 * start counting up from last errno value + 1
 */
#ifdef __ELASTERROR
#define ERR_BASE (__ELASTERROR)
#else
#define ERR_BASE 2000
#endif


/*
 *	detailed errno definitions go below
 */


/**
 * @def E_CPUS_PKT_SIZE_LIMIT
 *	attempted to push a packet larger than the transfer frame size of the
 *	AS250 protocol
 *
 * @def E_CPUS_FORWARD_WRITE
 *	could not forward the write position of the underlying circular buffer
 *
 * @def E_CPUS_PATTERN_WRITE
 *	could not write (in)valid pattern marker
 *
 * @def E_CPUS_SIZE_WRITE
 *	could not write of size of packet
 *
 * @def E_CPUS_PKT_READ
 *	error reading packet data from buffer
 *
 * @def E_CPUS_PKT_WRITE
 *	could not write packet data
 *
 * @def E_CPUS_FULL
 *	buffer full, could not accept packet data
 *
 * @def E_CPUS_PUSH_INVALID
 *	could not write invalid pattern
 *
 * @def E_CPUS_WRITE
 *	could not write packet to buffer
 */

/*
 *	circularly buffered PUS frame constructor errors
 */

#define ERR_CPUS_OFF	100
#define ERR_CPUS(x)   (x+ERR_BASE+ERR_CPUS_OFF)

#define E_CPUS_PKT_SIZE_LIMIT		ERR_CPUS(1)
#define E_CPUS_FORWARD_WRITE		ERR_CPUS(2)
#define E_CPUS_PATTERN_WRITE		ERR_CPUS(3)
/* error 4 removed */
#define E_CPUS_SIZE_WRITE		ERR_CPUS(5)
#define E_CPUS_PKT_READ			ERR_CPUS(6)
#define E_CPUS_PKT_WRITE		ERR_CPUS(7)
#define E_CPUS_FULL			ERR_CPUS(8)
#define E_CPUS_PUSH_INVALID		ERR_CPUS(9)
#define E_CPUS_WRITE			ERR_CPUS(10)
/* error 11 removed */
/* error 12 removed */



/**
 * @def E_PTRACK_PKT_SIZE_LIMIT
 *	packet size excceds buffer size
 *
 * @def E_PTRACK_PKT_WRITE
 *	could not write packet data
 *
 * @def E_PTRACK_SIZE_WRITE
 *	could not write of size of packet
 *
 * @def E_PTRACK_PKT_READ
 *	error while reading a packet (size mismatch)
 *
 * @def E_PTRACK_NOPKT
 *	there was no packet
 *
 * @def E_PTRACK_INVALID
 *	the packet tracker reference was not a valid pointer
 */

/*
 *	circularly buffered pus packet tracker
 */

#define ERR_PTRACK_OFF	200
#define ERR_PTRACK(x)   (x+ERR_BASE+ERR_PTRACK_OFF)

#define E_PTRACK_PKT_SIZE_LIMIT		ERR_PTRACK(1)
#define E_PTRACK_PKT_WRITE		ERR_PTRACK(2)
#define E_PTRACK_SIZE_WRITE		ERR_PTRACK(3)
#define E_PTRACK_PKT_READ		ERR_PTRACK(4)
#define E_PTRACK_NOPKT			ERR_PTRACK(5)
#define E_PTRACK_INVALID		ERR_PTRACK(6)


/**
 * @def E_BRM_MEM_ADDR_ALIGN
 *	the supplied memory block is not properly aligned
 *
 * @def E_BRM_INVALID_COREFREQ
 *	the specified 1553 core frequency is not valid
 *
 * @def E_BRM_INVALID_PKT_SIZE
 *	the size field of an alleged packet exceeded the allowed size
 *
 * @def E_BRM_INVALID_PKT_ID
 *	the packet's PUS id was invalid
 *
 * @def E_BRM_IRQ_RT_ILLCMD
 *	the bus controller wrote an illegal 1553 command
 *
 * @def E_BRM_IRQ_ILLOP
 *	the bus controller wrote an illegal 1553 operation
 *
 * @def E_BRM_IRQ_MERR
 *	a message error occured on the bus
 *
 * @def E_BRM_IRQ_DMAF
 *	a DMA fault occured
 *
 * @def E_BRM_IRQ_WRAPF
 *	a wrap fault occured
 *
 * @def E_BRM_IRQ_TAPF
 *	a terminal address parity fault occured
 *
 * @def E_BRM_IRQ_BITF
 *	a BIT fail occured
 *
 * @def E_BRM_IRQ_IXEQ0
 *	an Index Equal Zero occured
 *
 * @def E_BRM_CW_BAC_FLAG
 *	an block access was reported, but the block access flag was not set
 *
 * @def E_BRM_INVALID_TRANSFER_SIZE
 *	the specified transfer size did not match the total size of the packets
 */

/*
 *	1553BRM/AS250 errors
 */

#define ERR_BRM_OFF	300
#define ERR_BRM(x)   (x+ERR_BASE+ERR_BRM_OFF)

#define E_BRM_MEM_ADDR_ALIGN		ERR_BRM(1)
#define E_BRM_INVALID_COREFREQ		ERR_BRM(2)
#define E_BRM_INVALID_PKT_SIZE		ERR_BRM(3)
#define E_BRM_INVALID_PKT_ID		ERR_BRM(4)
#define E_BRM_IRQ_RT_ILLCMD		ERR_BRM(5)
#define E_BRM_IRQ_ILLOP			ERR_BRM(6)
#define E_BRM_IRQ_MERR			ERR_BRM(7)
#define E_BRM_IRQ_DMAF			ERR_BRM(8)
#define E_BRM_IRQ_WRAPF			ERR_BRM(9)
#define E_BRM_IRQ_TAPF			ERR_BRM(10)
#define E_BRM_IRQ_BITF			ERR_BRM(11)
#define E_BRM_IRQ_IXEQ0			ERR_BRM(12)
#define E_BRM_CW_BAC_FLAG		ERR_BRM(13)
#define E_BRM_INVALID_TRANSFER_SIZE	ERR_BRM(14)


/**
 * @def E_IRQ_QUEUE_BUSY
 *	a deferred interrupt could not be queued
 *
 * @def E_IRQ_EXCEEDS_IRL_SIZE
 *	the requested IRQ number exceeds the nominal number of interrupt lines
 *
 * @def E_IRQ_POOL_EMPTY
 *	all available ISR callback slots are used
 *
 * @def E_IRQ_DEREGISTER
 *	the removal of the specified ISR callback was unsuccessful
 */

/*
 *	irq dispatch errors
 */

#define ERR_IRQ_OFF	400
#define ERR_IRQ(x)   (x+ERR_BASE+ERR_IRQ_OFF)

#define E_IRQ_QUEUE_BUSY		ERR_IRQ(1)
#define E_IRQ_EXCEEDS_IRL_SIZE		ERR_IRQ(2)
#define E_IRQ_POOL_EMPTY		ERR_IRQ(3)
#define E_IRQ_DEREGISTER		ERR_IRQ(4)



/**
 * @def E_SPW_NO_RX_DESC_AVAIL
 *	there are no free RX descriptors available
 *
 * @def E_SPW_NO_TX_DESC_AVAIL
 *	there are no free TX descriptors available
 *
 * @def E_SPW_CLOCKS_INVALID
 *	the specified clock dividers are invalid
 *
 * @def E_SPW_INVALID_ADDR_ERROR
 *	an invalid address error occured
 *
 * @def E_SPW_PARITY_ERROR
 *	a parity error occured
 *
 * @def E_SPW_DISCONNECT_ERROR
 *	a disconnect error occured
 *
 * @def E_SPW_ESCAPE_ERROR
 *	an escape error occured
 *
 * @def E_SPW_CREDIT_ERROR
 *	a creadit error occured
 *
 * @def E_SPW_RX_AHB_ERROR
 *	a RX DMA error occured
 *
 * @def E_SPW_TX_AHB_ERROR
 *	a TX DMA error occured
 *
 * @def E_SPW_RX_DESC_TABLE_ALIGN
 *	the supplied RX descriptor table is incorrectly aligned
 *
 * @def E_SPW_TX_DESC_TABLE_ALIGN
 *	the supplied TX descriptor table is incorrectly aligned
 */

/*
 *	grspw2 errors
 */

#define ERR_SPW_OFF	500
#define ERR_SPW(x)   (x+ERR_BASE+ERR_SPW_OFF)

#define E_SPW_NO_RX_DESC_AVAIL		ERR_SPW(1)
#define E_SPW_NO_TX_DESC_AVAIL		ERR_SPW(2)
/* error 3 removed */
/* error 4 removed */
#define E_SPW_CLOCKS_INVALID		ERR_SPW(5)
#define E_SPW_INVALID_ADDR_ERROR	ERR_SPW(6)
#define E_SPW_PARITY_ERROR		ERR_SPW(7)
#define E_SPW_DISCONNECT_ERROR		ERR_SPW(8)
#define E_SPW_ESCAPE_ERROR		ERR_SPW(9)
#define E_SPW_CREDIT_ERROR		ERR_SPW(10)
#define E_SPW_RX_AHB_ERROR		ERR_SPW(11)
#define E_SPW_TX_AHB_ERROR		ERR_SPW(12)
#define E_SPW_RX_DESC_TABLE_ALIGN	ERR_SPW(13)
#define E_SPW_TX_DESC_TABLE_ALIGN	ERR_SPW(14)


/*
 *	timing errors
 */

/* timing errors removed */


/**
 * @def ERR_FLASH_BLOCKS_EXCEEDED
 *	the specified flash block exceeds the number of blocks per chip
 *
 * @def ERR_FLASH_PAGES_EXCEEDED
 *	the specified page exceeds the number of pages per block
 *
 * @def ERR_FLASH_PAGESIZE_EXCEEDED
 *	the specified page offset exceeds the size of a page
 *
 * @def ERR_FLASH_DISABLED
 *	the flash is diabled
 *
 * @def ERR_FLASH_READ_PAGE_EXCEEDED
 *	the specified read offset would have exceed the flash page size
 *
 * @def ERR_FLASH_BLOCK_INVALID
 *	a flash block was invalid
 *
 * @def ERR_FLASH_ADDR_EMPTY
 *	a read failed because the flash was marked empty at the given address
 *
 * @def ERR_FLASH_DATA_WRITE_ERROR
 *	a write to the data flash failed
 *
 * @def ERR_FLASH_EDAC_WRITE_ERROR
 *	a write to the eadc flash failed
 *
 * @def ERR_FLASH_DATA_ERASE_ERROR
 *	a data flash erase failed
 *
 * @def ERR_FLASH_EDAC_ERASE_ERROR
 *	an edac flash erase failed
 *
 * @def ERR_FLASH_WRITE_PAGE_EXCEEDED
 *	the current write would have exceeded the flash page size
 *
 * @def ERR_FLASH_EDAC_READ_ERROR
 *	the flash edac status could not be read
 */

/*
 *	flash errors
 */

#define ERR_FLASH_OFF	700

#define ERR_FLASH(x)   (x+ERR_BASE+ERR_FLASH_OFF)

#define ERR_FLASH_BLOCKS_EXCEEDED	ERR_FLASH(1)
#define ERR_FLASH_PAGES_EXCEEDED	ERR_FLASH(2)
#define ERR_FLASH_PAGESIZE_EXCEEDED	ERR_FLASH(3)
#define ERR_FLASH_DISABLED		ERR_FLASH(4)
#define ERR_FLASH_READ_PAGE_EXCEEDED	ERR_FLASH(5)
#define ERR_FLASH_BLOCK_INVALID		ERR_FLASH(6)
#define ERR_FLASH_ADDR_EMPTY		ERR_FLASH(7)
#define ERR_FLASH_DATA_WRITE_ERROR	ERR_FLASH(8)
#define ERR_FLASH_EDAC_WRITE_ERROR	ERR_FLASH(9)
#define ERR_FLASH_DATA_ERASE_ERROR	ERR_FLASH(10)
#define ERR_FLASH_EDAC_ERASE_ERROR	ERR_FLASH(11)
#define ERR_FLASH_WRITE_PAGE_EXCEEDED	ERR_FLASH(12)
#define ERR_FLASH_EDAC_READ_ERROR	ERR_FLASH(13)


/**
 * @def	ERR_DSU_CWP_INVALID
 *	the requested CPU window exceeds the valid range
 */

/*
 *	DSU errors
 */

#define ERR_DSU_OFF	800

#define ERR_DSU(x)   (x+ERR_BASE+ERR_DSU_OFF)

#define ERR_DSU_CWP_INVALID		ERR_DSU(1)




#endif

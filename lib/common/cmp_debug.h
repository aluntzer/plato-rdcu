/**
 * @file   cmp_debug.h
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2020
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
 * @brief compression/decompression debugging defines
 */

#ifndef CMP_DEBUG_H
#define CMP_DEBUG_H

#ifndef ICU_ASW
#endif

#if !defined(ICU_ASW) && (defined(DEBUG) || DEBUGLEVEL > 0)
	#include <stdio.h>
	__extension__
	#define debug_print(...) \
		do { fprintf(stderr, __VA_ARGS__); } while (0)
#else
	#define debug_print(...) \
		do {} while (0)
#endif


#endif /* CMP_DEBUG_H */

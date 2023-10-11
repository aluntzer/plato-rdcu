/**
 * @file   my_inttypes.h
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2021
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
 * @brief This is a simple inttypes.h implementation for the sparc platform,
 *	since the sparc-elf-gcc toolchain does not provide one.
 * @warning Does not fully implement the standard.
 */

#ifndef MY_INTTYPES_H
#define MY_INTTYPES_H

#ifndef __sparc__
#  include <inttypes.h>
#else

#ifndef PRId32
#  define PRId32 "ld"
#endif /*PRId32*/

#ifndef PRIi32
#  define PRIi32 "li"
#endif /*PRIi32*/

#ifndef PRIo32
#  define PRIo32 "lo"
#endif /*PRIo32*/

#ifndef PRIu32
#  define PRIu32 "lu"
#endif /* PRIu32 */

#ifndef PRIx32
#  define PRIx32 "lx"
#endif /*PRIx32*/

#ifndef PRIX32
#  define PRIX32 "lX"
#endif /*PRIX32*/

#endif /* __sparc__ */

#endif /* MY_INTTYPES_H */

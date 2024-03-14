/**
 * @file   vsnprintf.h
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2024
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
 * @brief Tiny vsnprintf implementation
 */


#ifndef VSNPRINTF_H
#define VSNPRINTF_H

int my_vsnprintf(char* buffer, size_t count, const char* format, va_list va);

#endif /* VSNPRINTF_H */

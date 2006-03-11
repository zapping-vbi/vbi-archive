/*
 *  Copyright (C) 2006 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: simd-consts.h,v 1.2 2006-03-11 13:11:59 mschimek Exp $ */

#include <inttypes.h>
#include "misc.h"

#define s8(n)  { n * 0x0101010101010101ULL, n * 0x0101010101010101ULL }
#define s16(n) { n * 0x0001000100010001ULL, n * 0x0001000100010001ULL }
#define s32(n) { n * 0x0000000100000001ULL, n * 0x0000000100000001ULL }

const int64_t __attribute__ ((aligned (16))) vsplat8_1[2]	= s8 (1);
const int64_t __attribute__ ((aligned (16))) vsplat8_m1[2]	= s8 (0xFF);
const int64_t __attribute__ ((aligned (16))) vsplat8_15[2]	= s8 (15);
const int64_t __attribute__ ((aligned (16))) vsplat8_127[2]	= s8 (127);
const int64_t __attribute__ ((aligned (16))) vsplatu8_F8[2]	= s8 (0xF8);
const int64_t __attribute__ ((aligned (16))) vsplatu8_FC[2]	= s8 (0xFC);
const int64_t __attribute__ ((aligned (16))) vsplat16_1[2]	= s16 (1);
const int64_t __attribute__ ((aligned (16))) vsplat16_2[2]	= s16 (2);
const int64_t __attribute__ ((aligned (16))) vsplat16_128[2]	= s16 (128);
const int64_t __attribute__ ((aligned (16))) vsplat16_255[2]	= s16 (255);
const int64_t __attribute__ ((aligned (16))) vsplat16_256[2]	= s16 (256);
const int64_t __attribute__ ((aligned (16))) vsplat16_m256[2]	= s16 (0xFF00);
const int64_t __attribute__ ((aligned (16))) vsplatu16_F8[2]	= s16 (0x00F8);
const int64_t __attribute__ ((aligned (16))) vsplat32_1[2]	= s32 (1);
const int64_t __attribute__ ((aligned (16))) vsplat32_2[2]	= s32 (2);

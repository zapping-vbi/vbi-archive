/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: tables.c,v 1.5 2002-06-12 04:00:16 mschimek Exp $ */

#include "mpeg.h"

/*
 *  ISO 13818-2 Table 6.4
 */
const double
frame_rate_value[16] =
{
	0,
	24000.0 / 1001, 24.0,
	25.0, 30000.0 / 1001, 30.0,
	50.0, 60000.0 / 1001, 60.0
};

/* ? */
const double
aspect_ratio_value[16] =
{
	0,      1.0,	0.6735,	0.7031,
	0.7615,	0.8055,	0.8437,	0.8935,
	0.9375,	0.9815,	1.0255,	1.0695,
	1.1250,	1.1575,	1.2015,	0
};

/*
 *  ISO 13818-2 6.3.11
 */
const unsigned char
default_intra_quant_matrix[8][8] =
{
	{  8, 16, 19, 22, 26, 27, 29, 34 },
	{ 16, 16, 22, 24, 27, 29, 34, 37 },
	{ 19, 22, 26, 27, 29, 34, 34, 38 },
	{ 22, 22, 26, 27, 29, 34, 37, 40 },
	{ 22, 26, 27, 29, 32, 35, 40, 48 },
	{ 26, 27, 29, 32, 35, 40, 48, 58 },
	{ 26, 27, 29, 34, 38, 46, 56, 69 },
	{ 27, 29, 35, 38, 46, 56, 69, 83 }
};

const unsigned char
default_inter_quant_matrix[8][8] =
{
        { 16, 16, 16, 16, 16, 16, 16, 16 },
        { 16, 16, 16, 16, 16, 16, 16, 16 },
        { 16, 16, 16, 16, 16, 16, 16, 16 },
        { 16, 16, 16, 16, 16, 16, 16, 16 },
        { 16, 16, 16, 16, 16, 16, 16, 16 },
        { 16, 16, 16, 16, 16, 16, 16, 16 },
        { 16, 16, 16, 16, 16, 16, 16, 16 },
        { 16, 16, 16, 16, 16, 16, 16, 16 },
};

/*
 *  ISO 13818-2 Table 7.6
 */
const unsigned char
quantiser_scale[2][32] =
{
	{ 0, 2, 4, 6, 8, 10, 12, 14,
	  16, 18, 20, 22, 24, 26, 28, 30,
	  32, 34, 36, 38, 40, 42, 44, 46,
	  48, 50, 52, 54, 56, 58, 60, 62 },

	{ 0, 1, 2, 3, 4, 5, 6, 7,
	  8, 10, 12, 14, 16, 18, 20, 22,
	  24, 28, 32, 36, 40, 44, 48, 52,
	  56, 64, 72, 80, 88, 96, 104, 112 }
};

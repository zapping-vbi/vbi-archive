/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  Based on code written by Tom G. Lane
 *  and released to public domain 11/22/93.
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

/* $Id: ieee.h,v 1.1 2000-07-05 18:09:34 mschimek Exp $ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define IEEE_PI 3.14159265358979323846

#define C0 cos(0.0 * IEEE_PI / 16.0) // 1.0
#define C1 cos(1.0 * IEEE_PI / 16.0) // 0.9808
#define C2 cos(2.0 * IEEE_PI / 16.0) // 0.9239
#define C3 cos(3.0 * IEEE_PI / 16.0) // 0.8315
#define C4 cos(4.0 * IEEE_PI / 16.0) // 0.7071
#define C5 cos(5.0 * IEEE_PI / 16.0) // 0.5556
#define C6 cos(6.0 * IEEE_PI / 16.0) // 0.3827
#define C7 cos(7.0 * IEEE_PI / 16.0) // 0.1951

#define S13 ((double)(1 << 13))
#define S14 ((double)(1 << 14))
#define S15 ((double)(1 << 15))
#define S16 ((double)(1 << 16))
#define S17 ((double)(1 << 17))
#define S18 ((double)(1 << 18))
#define S19 ((double)(1 << 19))

typedef void dct_func(short [8][8]);
typedef void qdct_func(int q, short [8][8]);

extern int ieee_round(double val);
extern void ieee_ref_fdct(short block[8][8]);
extern void ieee_ref_idct(short block[8][8]);
extern void mpeg_intra_quant(int q, short block[8][8]);
extern void mpeg_inter_quant(int q, short block[8][8]);
extern void mpeg1_intra_iquant(int q, short block[8][8]);
extern void mpeg1_inter_iquant(int q, short block[8][8]);
extern void mpeg2_intra_iquant(int q, short block[8][8]);
extern void mpeg2_inter_iquant(int q, short block[8][8]);
extern void ieee_randomize(short block[8][8], long minpix, long maxpix, long sign);
extern void rake_pattern(short block[8][8], long minpix, long maxpix, long sign);
extern void (* randomize)(short [8][8], long, long, long);
extern void q_fdct_test(qdct_func *fdct, qdct_func *quant, long minpix, long maxpix, long sign, int iterations, unsigned int quant_mask);
extern void q_idct_test(qdct_func *idct, qdct_func *quant, qdct_func *iquant, long minpix, long maxpix, long sign, int iterations, unsigned int quant_mask);
extern void ieee_idct_test(char *name, dct_func *idct, long minpix, long maxpix, long sign, int iterations);
extern void fdct_test(char *name, dct_func *fdct, long minpix, long maxpix, long sign, int iterations);
extern void ieee_1180(char *name, dct_func *idct);

#define elements(block) (sizeof(block) / sizeof((block)[0][0]))

#define swap(a, b)						\
do {								\
	__typeof__ (a) _t = (b);				\
	(b) = (a);						\
	(a) = _t;						\
} while (0)

#define mirror(block)						\
do {								\
	int _i, _j;						\
	for (_i = 0; _i < 7; _i++)				\
		for (_j = _i + 1; _j < 8; _j++)			\
			swap((block)[_i][_j], (block)[_j][_i]);	\
} while (0)

#define trans(block, n)						\
do {								\
	int _i;							\
	for (_i = 0; _i < elements(block); _i++)		\
		(block)[0][_i] += n;				\
} while (0)

#define copy(d, s)						\
do {								\
	int _i;							\
	for (_i = 0; _i < elements(d); _i++)			\
		(d)[0][_i] = (s)[0][_i];			\
} while (0)

#define clear(block)						\
do {								\
	int _i;							\
	for (_i = 0; _i < elements(block); _i++)		\
		(block)[0][_i] = 0.0;				\
} while (0)

#define dump(block)						\
do {								\
	int _i;							\
	int _j = sizeof((block)[0]) / sizeof((block)[0][0]);	\
	fprintf(stderr, #block ":\n");				\
	for (_i = 0; _i < elements(block); _i++)		\
		fprintf(stderr, "%11.4f%c",			\
			(double)(block)[0][_i],			\
			(_i % _j == _j - 1) ? '\n' : ' ');	\
	fprintf(stderr, "\n");					\
} while (0)

#define peak(block)						\
do {								\
	int _i;							\
	double _min = 1e30, _max = -1e30;			\
	for (_i = 0; _i < elements(block); _i++)		\
		if ((block)[0][_i] < _min)			\
			_min = (block)[0][_i];			\
		else if ((block)[0][_i] > _max)			\
			_max = (block)[0][_i];			\
	fprintf(stderr, #block ": %11.4f ... %11.4f\n",		\
		_min, _max);					\
} while (0)

#define maxabs(res, bl1, bl2)					\
do {								\
	int _i;							\
	for (_i = 0; _i < elements(res); _i++)			\
		(res)[0][_i] = MAX(fabs((bl1)[0][_i]),		\
			fabs((bl2)[0][_i]));			\
} while (0)

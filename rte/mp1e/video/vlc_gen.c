/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 2002 Michael H. Schimek
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

/* $Id: vlc_gen.c,v 1.3 2002-09-12 12:24:14 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include "vlc.h"

#define MPEG2 0

const int cbp_order[6] = { 5, 4, 3, 1, 2, 0 };

#define align(n)

/*
 *  ISO 13818-2 Figure 7-2, 7-3
 */
static const int
scan[2][8][8] =
{
	{
		{  0,  1,  5,  6, 14, 15, 27, 28 }, 
		{  2,  4,  7, 13, 16, 26, 29, 42 },
		{  3,  8, 12, 17, 25, 30, 41, 43 },
		{  9, 11, 18, 24, 31, 40, 44, 53 },
		{ 10, 19, 23, 32, 39, 45, 52, 54 },
		{ 20, 22, 33, 38, 46, 51, 55, 60 },
		{ 21, 34, 37, 47, 50, 56, 59, 61 },
		{ 35, 36, 48, 49, 57, 58, 62, 63 }
	}, {
		{  0,  4,  6, 20, 22, 36, 38, 52 },
		{  1,  5,  7, 21, 23, 37, 39, 53 },
		{  2,  8, 19, 24, 34, 40, 50, 54 },
		{  3,  9, 18, 25, 35, 41, 51, 55 },
		{ 10, 17, 26, 30, 42, 46, 56, 60 },
		{ 11, 16, 27, 31, 43, 47, 57, 61 },
		{ 12, 15, 28, 32, 44, 48, 58, 62 },
		{ 13, 14, 29, 33, 45, 49, 59, 63 }
	}
};

/*
 *  Variable Length Codes
 */

/*
 *  ISO 13818 Table B-1
 *  Variable length codes for macroblock_address_increment
 */
static const char *
macroblock_address_increment[33] =
{
	"1",
	"011",
	"010",
	"0011",
	"0010",
	"00011",
	"00010",
	"0000111",
	"0000110",
	"00001011",
	"00001010",
	"00001001",
	"00001000",
	"00000111",
	"00000110",
	"0000010111",
	"0000010110",
	"0000010101",
	"0000010100",
	"0000010011",
	"0000010010",
	"00000100011",
	"00000100010",
	"00000100001",
	"00000100000",
	"00000011111",
	"00000011110",
	"00000011101",
	"00000011100",
	"00000011011",
	"00000011010",
	"00000011001",
	"00000011000"
     /* "00000001000" macroblock_escape code */
};

/*
 *  ISO 13818-2 Table B-9
 *  Variable length codes for coded_block_pattern
 */
static const char *
coded_block_pattern[64] =
{
	"000000001", /* "This entry shall not be used with 4:2:0 chrominance structure" */
	"01011",
	"01001",		
	"001101",		
	"1101",		
	"0010111",	
	"0010011",	
	"00011111",	
	"1100",		
	"0010110",	
	"0010010",	
	"00011110",	
	"10011",		
	"00011011",
	"00010111",
	"00010011",
	"1011",		
	"0010101",	
	"0010001",	
	"00011101",	
	"10001",		
	"00011001",
	"00010101",
	"00010001",
	"001111",	
	"00001111",
	"00001101",
	"000000011",
	"01111",		
	"00001011",
	"00000111",
	"000000111",
	"1010",		
	"0010100",	
	"0010000",	
	"00011100",
	"001110",	
	"00001110",
	"00001100",
	"000000010",
	"10000",		
	"00011000",
	"00010100",
	"00010000",
	"01110",		
	"00001010",
	"00000110",
	"000000110",
	"10010",		
	"00011010",
	"00010110",
	"00010010",
	"01101",		
	"00001001",
	"00000101",
	"000000101",
	"01100",		
	"00001000",
	"00000100",
	"000000100",
	"111",		
	"01010",		
	"01000",		
	"001100"	
};

/*
 *  ISO 13818 Table B-10
 *  Variable length codes for motion_code (not including sign bit)
 */
static const char *
motion_code_vlc[17] =
{
	"1",		/* 0 */
	"01",		/* 1 */
	"001",
	"0001",
	"000011",
	"0000101",
	"0000100",
	"0000011",
	"000001011",
	"000001010",
	"000001001",
	"0000010001",
	"0000010000",
	"0000001111",
	"0000001110",
	"0000001101",	/* 15 */
	"0000001100"	/* 16 */
};

/*
 *  ISO 13818-2 Table B-12
 *  Variable length codes for dct_dc_size_luminance
 */
static const char *
dct_dc_size_luma[12] =
{
	"100",
	"00",
	"01",
	"101",
	"110",
	"1110",
	"11110",
	"111110",
	"1111110",
	"11111110",
	"111111110",
	"111111111"
};

/*
 *  ISO 13818-2 Table B-13
 *  Variable length codes for dct_dc_size_chrominance
 */
static const char *
dct_dc_size_chroma[12] =
{
	"00",
	"01",
	"10",
	"110",
	"1110",
	"11110",
	"111110",
	"1111110",
	"11111110",
	"111111110",
	"1111111110",
	"1111111111"
};

struct dct_coeff {
	const char *		code;
	const int		run, level;
};

/*
 *  ISO 13818-2 Table B-14
 *  DCT coefficients table zero (not including sign bit)
 */
static const struct dct_coeff
dct_coeff_zero[] =
{
      /* "10" End of Block */
    /* { "1", 0, 1 } "This code shall be used
	   for the first (DC) coefficient of a non-intra block" */
	{ "11", 0, 1 },
	{ "011", 1, 1 },
	{ "0100", 0, 2 },
	{ "0101", 2, 1 },
	{ "00101", 0, 3 },
	{ "00111", 3, 1 },
	{ "00110", 4, 1 },
	{ "000110", 1, 2 },
	{ "000111", 5, 1 },
	{ "000101", 6, 1 },
	{ "000100", 7, 1 },
	{ "0000110", 0, 4 },
	{ "0000100", 2, 2 },
	{ "0000111", 8, 1 },
	{ "0000101", 9, 1 },
       /* "000001" Escape code */
	{ "00100110", 0, 5 },
	{ "00100001", 0, 6 },
	{ "00100101", 1, 3 },
	{ "00100100", 3, 2 },
	{ "00100111", 10, 1 },
	{ "00100011", 11, 1 },
	{ "00100010", 12, 1 },
	{ "00100000", 13, 1 },
	{ "0000001010", 0, 7 },
	{ "0000001100", 1, 4 },
	{ "0000001011", 2, 3 },
	{ "0000001111", 4, 2 },
	{ "0000001001", 5, 2 },
	{ "0000001110", 14, 1 },
	{ "0000001101", 15, 1 },
	{ "0000001000", 16, 1 },
	{ "000000011101", 0, 8 },
	{ "000000011000", 0, 9 },
	{ "000000010011", 0, 10 },
	{ "000000010000", 0, 11 },
	{ "000000011011", 1, 5 },
	{ "000000010100", 2, 4 },
	{ "000000011100", 3, 3 },
	{ "000000010010", 4, 3 },
	{ "000000011110", 6, 2 },
	{ "000000010101", 7, 2 },
	{ "000000010001", 8, 2 },
	{ "000000011111", 17, 1 },
	{ "000000011010", 18, 1 },
	{ "000000011001", 19, 1 },
	{ "000000010111", 20, 1 },
	{ "000000010110", 21, 1 },
	{ "0000000011010", 0, 12 },
	{ "0000000011001", 0, 13 },
	{ "0000000011000", 0, 14 },
	{ "0000000010111", 0, 15 },
	{ "0000000010110", 1, 6 },
	{ "0000000010101", 1, 7 },
	{ "0000000010100", 2, 5 },
	{ "0000000010011", 3, 4 },
	{ "0000000010010", 5, 3 },
	{ "0000000010001", 9, 2 },
	{ "0000000010000", 10, 2 },
	{ "0000000011111", 22, 1 },
	{ "0000000011110", 23, 1 },
	{ "0000000011101", 24, 1 },
	{ "0000000011100", 25, 1 },
	{ "0000000011011", 26, 1 },
	{ "00000000011111", 0, 16 },
	{ "00000000011110", 0, 17 },
	{ "00000000011101", 0, 18 },
	{ "00000000011100", 0, 19 },
	{ "00000000011011", 0, 20 },
	{ "00000000011010", 0, 21 },
	{ "00000000011001", 0, 22 },
	{ "00000000011000", 0, 23 },
	{ "00000000010111", 0, 24 },
	{ "00000000010110", 0, 25 },
	{ "00000000010101", 0, 26 },
	{ "00000000010100", 0, 27 },
	{ "00000000010011", 0, 28 },
	{ "00000000010010", 0, 29 },
	{ "00000000010001", 0, 30 },
	{ "00000000010000", 0, 31 },
	{ "000000000011000", 0, 32 },
	{ "000000000010111", 0, 33 },
	{ "000000000010110", 0, 34 },
	{ "000000000010101", 0, 35 },
	{ "000000000010100", 0, 36 },
	{ "000000000010011", 0, 37 },
	{ "000000000010010", 0, 38 },
	{ "000000000010001", 0, 39 },
	{ "000000000010000", 0, 40 },
	{ "000000000011111", 1, 8 },
	{ "000000000011110", 1, 9 },
	{ "000000000011101", 1, 10 },
	{ "000000000011100", 1, 11 },
	{ "000000000011011", 1, 12 },
	{ "000000000011010", 1, 13 },
	{ "000000000011001", 1, 14 },
	{ "0000000000010011", 1, 15 },
	{ "0000000000010010", 1, 16 },
	{ "0000000000010001", 1, 17 },
	{ "0000000000010000", 1, 18 },
	{ "0000000000010100", 6, 3 },
	{ "0000000000011010", 11, 2 },
	{ "0000000000011001", 12, 2 },
	{ "0000000000011000", 13, 2 },
	{ "0000000000010111", 14, 2 },
	{ "0000000000010110", 15, 2 },
	{ "0000000000010101", 16, 2 },
	{ "0000000000011111", 27, 1 },
	{ "0000000000011110", 28, 1 },
	{ "0000000000011101", 29, 1 },
	{ "0000000000011100", 30, 1 },
	{ "0000000000011011", 31, 1 },
	{ NULL, -1, -1 }
};

/*
 *  ISO 13818-2 Table B-15
 *  DCT coefficients table one (not including sign bit)
 */
static const struct dct_coeff
dct_coeff_one[] =
{
       /* "0110" End of Block */
	{ "10", 0, 1 },
	{ "010", 1, 1 },
	{ "110", 0, 2 },
	{ "00101", 2, 1 },
	{ "0111", 0, 3 },
	{ "00111", 3, 1 },
	{ "000110", 4, 1 },
	{ "00110", 1, 2 },
	{ "000111", 5, 1 },
	{ "0000110", 6, 1 },
	{ "0000100", 7, 1 },
	{ "11100", 0, 4 },
	{ "0000111", 2, 2 },
	{ "0000101", 8, 1 },
	{ "1111000", 9, 1 },
       /* "000001" Escape code */
	{ "11101", 0, 5 },
	{ "000101", 0, 6 },
	{ "1111001", 1, 3 },
	{ "00100110", 3, 2 },
	{ "1111010", 10, 1 },
	{ "00100001", 11, 1 },
	{ "00100101", 12, 1 },
	{ "00100100", 13, 1 },
	{ "000100", 0, 7 },
	{ "00100111", 1, 4 },
	{ "11111100", 2, 3 },
	{ "11111101", 4, 2 },
	{ "000000100", 5, 2 },
	{ "000000101", 14, 1 },
	{ "000000111", 15, 1 },
	{ "0000001101", 16, 1 },
	{ "1111011", 0, 8 },
	{ "1111100", 0, 9 },
	{ "00100011", 0, 10 },
	{ "00100010", 0, 11 },
	{ "00100000", 1, 5 },
	{ "0000001100", 2, 4 },
	{ "000000011100", 3, 3 },
	{ "000000010010", 4, 3 },
	{ "000000011110", 6, 2 },
	{ "000000010101", 7, 2 },
	{ "000000010001", 8, 2 },
	{ "000000011111", 17, 1 },
	{ "000000011010", 18, 1 },
	{ "000000011001", 19, 1 },
	{ "000000010111", 20, 1 },
	{ "000000010110", 21, 1 },
	{ "11111010", 0, 12 },
	{ "11111011", 0, 13 },
	{ "11111110", 0, 14 },
	{ "11111111", 0, 15 },
	{ "0000000010110", 1, 6 },
	{ "0000000010101", 1, 7 },
	{ "0000000010100", 2, 5 },
	{ "0000000010011", 3, 4 },
	{ "0000000010010", 5, 3 },
	{ "0000000010001", 9, 2 },
	{ "0000000010000", 10, 2 },
	{ "0000000011111", 22, 1 },
	{ "0000000011110", 23, 1 },
	{ "0000000011101", 24, 1 },
	{ "0000000011100", 25, 1 },
	{ "0000000011011", 26, 1 },
	{ "00000000011111", 0, 16 },
	{ "00000000011110", 0, 17 },
	{ "00000000011101", 0, 18 },
	{ "00000000011100", 0, 19 },
	{ "00000000011011", 0, 20 },
	{ "00000000011010", 0, 21 },
	{ "00000000011001", 0, 22 },
	{ "00000000011000", 0, 23 },
	{ "00000000010111", 0, 24 },
	{ "00000000010110", 0, 25 },
	{ "00000000010101", 0, 26 },
	{ "00000000010100", 0, 27 },
	{ "00000000010011", 0, 28 },
	{ "00000000010010", 0, 29 },
	{ "00000000010001", 0, 30 },
	{ "00000000010000", 0, 31 },
	{ "000000000011000", 0, 32 },
	{ "000000000010111", 0, 33 },
	{ "000000000010110", 0, 34 },
	{ "000000000010101", 0, 35 },
	{ "000000000010100", 0, 36 },
	{ "000000000010011", 0, 37 },
	{ "000000000010010", 0, 38 },
	{ "000000000010001", 0, 39 },
	{ "000000000010000", 0, 40 },
	{ "000000000011111", 1, 8 },
	{ "000000000011110", 1, 9 },
	{ "000000000011101", 1, 10 },
	{ "000000000011100", 1, 11 },
	{ "000000000011011", 1, 12 },
	{ "000000000011010", 1, 13 },
	{ "000000000011001", 1, 14 },
	{ "0000000000010011", 1, 15 },
	{ "0000000000010010", 1, 16 },
	{ "0000000000010001", 1, 17 },
	{ "0000000000010000", 1, 18 },
	{ "0000000000010100", 6, 3 },
	{ "0000000000011010", 11, 2 },
	{ "0000000000011001", 12, 2 },
	{ "0000000000011000", 13, 2 },
	{ "0000000000010111", 14, 2 },
	{ "0000000000010110", 15, 2 },
	{ "0000000000010101", 16, 2 },
	{ "0000000000011111", 27, 1 },
	{ "0000000000011110", 28, 1 },
	{ "0000000000011101", 29, 1 },
	{ "0000000000011100", 30, 1 },
	{ "0000000000011011", 31, 1 },
	{ NULL, -1, -1 }
};

/*$$*/

/*
 *  ISO/IEC 13818-2 Table B-2
 *  Variable length codes for macroblock_type in I-pictures
 *
 *  '1' Intra
 *  '01 xxxxx' Intra, Quant
 */

/*
 *  ISO/IEC 13818-2 Table B-3
 *  Variable length codes for macroblock_type in P-pictures
 *
 *  '1' MC, Coded
 *  '01' No MC, Coded
 *  '001' MC, Not Coded
 *  '0001 1' Intra
 *  '0001 0' MC, Coded, Quant
 *  '0000 1' No MC, Coded, Quant
 *  '0000 01' Intra, Quant
 */

/*
 *  ISO/IEC 13818-2 Table B-4
 *  Variable length codes for macroblock_type in B-pictures
 */
const VLC4
mp1e_macroblock_type_b_nomc_quant[4] align(16) =
{
	{ 0x0020, 11, 0 }, /* '0000 01 xxxxx' (Intra, Quant) */
	{ 0x0183, 13, 2 }, /* '0000 11 xxxxx 11' (Fwd, Coded, Quant, MV (0, 0)) */
	{ 0x0103, 13, 2 }, /* '0000 10 xxxxx 11' (Bwd, Coded, Quant, MV (0, 0)) */
	{ 0x040F, 14, 4 }  /* '0001 0 xxxxx 11 11' (Interp, Coded, Quant, */
			   /*                       FMV (0, 0), BMV (0, 0)) */
};

const VLC2
mp1e_macroblock_type_b_nomc[4] align(8) =
{
	{ 0x03, 5 }, /* '0001 1' (Intra) */
	{ 0x0F, 6 }, /* '0011  11' (Fwd, Coded, MV (0, 0)) */
	{ 0x0F, 5 }, /* '011  11' (Bwd, Coded, MV (0, 0)) */
	{ 0x3F, 6 }, /* '11  11 11' (Interp, Coded, FMV (0, 0), BMV (0, 0)) */
};

const VLC2
mp1e_macroblock_type_b_nomc_notc[4] align(8) =
{
	{ 0, 0 },    /* Intra always coded */
	{ 0x0B, 6 }, /* '0010  11' (Fwd, Not Coded, MV (0, 0)) */
	{ 0x0B, 5 }, /* '010  11' (Bwd, Not Coded, MV (0, 0)) */
	{ 0x2F, 6 }, /* '10  11 11' (Interp, Not Coded, FMV (0, 0), BMV (0, 0)) */
};

const VLC2
mp1e_macroblock_type_b_quant[4] align(8) =
{
	{ 0x020, 11 }, /* '0000 01 xxxxx' (Intra, Quant) */
	{ 0x060, 11 }, /* '0000 11 xxxxx' (Fwd, Coded, Quant) */
	{ 0x040, 11 }, /* '0000 10 xxxxx' (Bwd, Coded, Quant) */
	{ 0x040, 10 }  /* '0001 0 xxxxx' (Interp, Coded, Quant) */
};

#if 0

/* Systematic VLCs (not looked up) */

const VLC2
mp1e_macroblock_type_b[4] align(8) =
{
	{ 0x03, 5 }, /* '0001 1' (Intra) */
	{ 0x03, 4 }, /* '0011' (Fwd, Coded) */
	{ 0x03, 3 }, /* '011' (Bwd, Coded) */
	{ 0x03, 2 }, /* '11' (Interp, Coded) */
};

const VLC2
mp1e_macroblock_type_b_notc[4] align(8) =
{
	{ 0, 0 },    /* Intra always coded */
	{ 0x02, 4 }, /* '0010' (Fwd, Not Coded) */
	{ 0x02, 3 }, /* '010' (Bwd, Not Coded) */
	{ 0x02, 2 }, /* '10' (Interp, Not Coded) */
};

#endif


/*$$*/


/*
 *  Translates bit string to vlc, returns length
 */
static int
vlc (const char *s, unsigned int *code)
{
	int i;

	*code = 0;

	for (i = 0; s[i]; i++)
		*code = (*code << 1) | (s[i] > '0');

	return strlen(s);
}

/*
 *  Finds dct coefficient vlc, not including sign bit
 *  (append 0 for positive level, 1 for negative level)
 */
static int
coeff_vlc (int table, int run, int level, unsigned int *vlcp)
{
	const struct dct_coeff *dcp;

	for (dcp = table ? dct_coeff_one : dct_coeff_zero; dcp->run >= 0; dcp++)
		if (dcp->run == run && dcp->level == level)
			return vlc (dcp->code, vlcp);

	return -1; /* No vlc for this run/length combination */
}

int
main (void)
{
	int i, j;
	unsigned int code;
	int dct_dc_size;
	int run, level, length;
	int f_code;
	int iscan[64];

	printf ("/* Generated file - do not edit */\n\n");


	/* Variable length codes for macroblock address increment */

	printf ("const VLC2\nmp1e_macroblock_address_increment[33]\talign(CACHE_LINE) =\n{\n");

	for (i = 0; i < 33; i++) {
		length = vlc (macroblock_address_increment[i], &code);
		assert (code <= UCHAR_MAX);
		printf ("\t{ %u, %d },\n", code, length);
	}

	printf ("};\n\n");


	/* Variable length codes for coded block pattern */

	printf ("const VLC2\nmp1e_coded_block_pattern[64]\talign(CACHE_LINE) =\n{\n");

	for (i = 0; i < 64; i++) {
		int j, k;

		for (j = k = 0; k < 6; k++)
			if (i & (1 << k))
				j |= 0x20 >> cbp_order[k]; /* (5 - k) */

		length = vlc (coded_block_pattern[j], &code);
		assert (code <= UCHAR_MAX);
		printf ("\t{ %u, %d },\n", code, length);
	}

	printf ("};\n\n");


	/* Variable length codes for motion vector component */

	printf ("const VLCM\nmp1e_motion_vector_component[480]\talign(CACHE_LINE) =\n{\n");

	for (f_code = F_CODE_MIN; f_code <= F_CODE_MAX; f_code++) {
		int r_size = f_code - 1;
		int f1 = (1 << r_size) - 1;

		for (i = 0; i < 16 << f_code; i++) {
			int motion_code, motion_residual;
			int delta = (i < (16 << r_size)) ? i : i - (16 << f_code);

			motion_code = (abs (delta) + f1) >> r_size;
			motion_residual = (abs (delta) + f1) & f1;

			length = vlc (motion_code_vlc[motion_code], &code);

			if (motion_code != 0) {
				code = code * 2 + (delta < 0); /* sign */
				length++;
			}

			if (f_code > 1 && motion_code != 0) {
				code = (code << r_size) + motion_residual;
				length += r_size;
			}

			assert (code < (1 << 12) && length < 16);

			printf ("\t{ %u, %d },\n", code, length);
#if 0
			fprintf (stderr, "MV %02x %-2d ", i, delta);

			for (j = length - 1; j >= 0; j--)
				fprintf (stderr, "%d", (code & (1 << j)) > 0);

			fprintf (stderr, "\n");
#endif
		}
	}

	printf ("};\n\n");


	run = 0;

	for (;;) {
		char buf[1024];

		fgets (buf, 1023, stdin);

		if (ferror (stdin)) {
			printf ("#error\n");
			exit (EXIT_FAILURE);
		} else if (feof (stdin))
			break;
		else if (strstr (buf, "$" "$"))
			run ^= 1;
		else if (run)
			printf("%s", buf);
	}


	/* Variable length codes for intra DC coefficient */

	printf ("const VLC8\nmp1e_dc_vlc_intra[%d][12]\talign(CACHE_LINE) =\n{\n", MPEG2 ? 5 : 3);

	for (i = 0; i < (MPEG2 ? 5 : 3); i++) {
		printf("\t{\n");

		for (dct_dc_size = 0; dct_dc_size < 12; dct_dc_size++) {
			switch (i) {
			case 0: /* Intra DC luma VLC */
				length = vlc (dct_dc_size_luma[dct_dc_size], &code);
				code <<= dct_dc_size;
				length += dct_dc_size;
				break;

			case 1: /* Intra DC luma VLC with EOB ('10' table B-14) of prev. block */
				length = vlc (dct_dc_size_luma[dct_dc_size], &code);
				code = ((0x2 << length) | code) << dct_dc_size;
				length += dct_dc_size + 2;
				break;

			case 2: /* Intra DC chroma VLC with EOB (B-14) of previous block */
				length = vlc (dct_dc_size_chroma[dct_dc_size], &code);
				code = ((0x2 << length) | code) << dct_dc_size;
				length += dct_dc_size + 2;
				break;

			case 3: /* Intra DC luma VLC with EOB ('0110' table B-15) of prev. block */
				length = vlc (dct_dc_size_luma[dct_dc_size], &code);
				code = ((0x6 << length) | code) << dct_dc_size;
				length += dct_dc_size + 4;
				break;

			case 4: /* Intra DC chroma VLC with EOB (B-15) of previous block */
				length = vlc (dct_dc_size_chroma[dct_dc_size], &code);
				code = ((0x6 << length) | code) << dct_dc_size;
				length += dct_dc_size + 4;
				break;

			default:
				assert (0);
			}

			printf ("\t\t{ 0x%x, %d },\n", code, length);
		}

		printf ("\t},\n");
	}

	printf ("};\n\n");


	/* Variable length codes for AC coefficients (table B-14, B-15) */

	for (j = 0; j <= MPEG2; j++) {
		int offs = 64 - 1 /* no level 0 */;

		printf ("const VLC2\nmp1e_ac_vlc_%s[]\talign(CACHE_LINE) =\n{\n\t",
			j ? "one" : "zero");

		for (run = 0; run < 64; run++) {
			for (level = 1; coeff_vlc (j, run, level, &code) > 0; level++)
				;

			assert (offs <= UCHAR_MAX);

			printf ("{ %u, %u }, ", offs, level);

			offs += level - 1 /* this item */ - 1 /* no level 0 */;
		}

		for (i = 64, run = 0; run < 64; run++) {
			for (level = 1; (length = coeff_vlc (j, run, level, &code)) > 0; level++, i++) {
				assert ((code << 1) <= UCHAR_MAX);

				if (level == 1)
					printf ("\n\t");

				printf ("{ %u, %u }, ", code << 1 /* sign 0 */, length + 1);
			}
		}

		printf ("\n};\n\n");
	}


	/* Forward zig-zag scanning pattern */

	for (j = 0; j <= 1; j++) {
		printf ("const uint8_t\nmp1e_iscan%u[8][8]\talign(CACHE_LINE) =\n{\n", j);

		for (i = 0; i < 64; i++) {
			iscan[(scan[j][0][i] - 1) & 63] = (i & 7) * 8 + (i >> 3);
		}

		for (i = 0; i < 64; i++) {
			if ((i % 8) == 0)
				printf ("\t{ ");
			printf ("%2u%s", iscan[i], (i % 8) < 7 ? ", " : " },\n");
		}

		printf ("};\n\n");
	}

	exit (EXIT_SUCCESS);

	return 0;
}

/*
 * DSP utils
 * Copyright (c) 2000, 2001 Gerard Lantau.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdlib.h>
#include <stdio.h>
#include "avcodec.h"
#include "dsputil.h"

void (*get_pixels)(DCTELEM *block, const UINT8 *pixels, int line_size);
void (*put_pixels_clamped)(const DCTELEM *block, UINT8 *pixels, int line_size);
void (*add_pixels_clamped)(const DCTELEM *block, UINT8 *pixels, int line_size);

op_pixels_abs_func pix_abs16x16;
op_pixels_abs_func pix_abs16x16_x2;
op_pixels_abs_func pix_abs16x16_y2;
op_pixels_abs_func pix_abs16x16_xy2;

static UINT8 cropTbl[256 + 2 * MAX_NEG_CROP];
UINT32 squareTbl[512];

void get_pixels_c(DCTELEM *block, const UINT8 *pixels, int line_size)
{
    DCTELEM *p;
    const UINT8 *pix;
    int i;

    /* read the pixels */
    p = block;
    pix = pixels;
    for(i=0;i<8;i++) {
        p[0] = pix[0];
        p[1] = pix[1];
        p[2] = pix[2];
        p[3] = pix[3];
        p[4] = pix[4];
        p[5] = pix[5];
        p[6] = pix[6];
        p[7] = pix[7];
        pix += line_size;
        p += 8;
    }
}

void put_pixels_clamped_c(const DCTELEM *block, UINT8 *pixels, int line_size)
{
    const DCTELEM *p;
    UINT8 *pix;
    int i;
    UINT8 *cm = cropTbl + MAX_NEG_CROP;
    
    /* read the pixels */
    p = block;
    pix = pixels;
    for(i=0;i<8;i++) {
        pix[0] = cm[p[0]];
        pix[1] = cm[p[1]];
        pix[2] = cm[p[2]];
        pix[3] = cm[p[3]];
        pix[4] = cm[p[4]];
        pix[5] = cm[p[5]];
        pix[6] = cm[p[6]];
        pix[7] = cm[p[7]];
        pix += line_size;
        p += 8;
    }
}

void add_pixels_clamped_c(const DCTELEM *block, UINT8 *pixels, int line_size)
{
    const DCTELEM *p;
    UINT8 *pix;
    int i;
    UINT8 *cm = cropTbl + MAX_NEG_CROP;
    
    /* read the pixels */
    p = block;
    pix = pixels;
    for(i=0;i<8;i++) {
        pix[0] = cm[pix[0] + p[0]];
        pix[1] = cm[pix[1] + p[1]];
        pix[2] = cm[pix[2] + p[2]];
        pix[3] = cm[pix[3] + p[3]];
        pix[4] = cm[pix[4] + p[4]];
        pix[5] = cm[pix[5] + p[5]];
        pix[6] = cm[pix[6] + p[6]];
        pix[7] = cm[pix[7] + p[7]];
        pix += line_size;
        p += 8;
    }
}

#define PIXOP(BTYPE, OPNAME, OP, INCR)                                                   \
                                                                                         \
static void OPNAME ## _pixels(BTYPE *block, const UINT8 *pixels, int line_size, int h)    \
{                                                                                        \
    BTYPE *p;                                                                            \
    const UINT8 *pix;                                                                    \
                                                                                         \
    p = block;                                                                           \
    pix = pixels;                                                                        \
    do {                                                                                 \
        OP(p[0], pix[0]);                                                                  \
        OP(p[1], pix[1]);                                                                  \
        OP(p[2], pix[2]);                                                                  \
        OP(p[3], pix[3]);                                                                  \
        OP(p[4], pix[4]);                                                                  \
        OP(p[5], pix[5]);                                                                  \
        OP(p[6], pix[6]);                                                                  \
        OP(p[7], pix[7]);                                                                  \
        pix += line_size;                                                                \
        p += INCR;                                                                       \
    } while (--h);;                                                                       \
}                                                                                        \
                                                                                         \
static void OPNAME ## _pixels_x2(BTYPE *block, const UINT8 *pixels, int line_size, int h)     \
{                                                                                        \
    BTYPE *p;                                                                          \
    const UINT8 *pix;                                                                    \
                                                                                         \
    p = block;                                                                           \
    pix = pixels;                                                                        \
    do {                                                                   \
        OP(p[0], avg2(pix[0], pix[1]));                                                    \
        OP(p[1], avg2(pix[1], pix[2]));                                                    \
        OP(p[2], avg2(pix[2], pix[3]));                                                    \
        OP(p[3], avg2(pix[3], pix[4]));                                                    \
        OP(p[4], avg2(pix[4], pix[5]));                                                    \
        OP(p[5], avg2(pix[5], pix[6]));                                                    \
        OP(p[6], avg2(pix[6], pix[7]));                                                    \
        OP(p[7], avg2(pix[7], pix[8]));                                                    \
        pix += line_size;                                                                \
        p += INCR;                                                                       \
    } while (--h);                                                                        \
}                                                                                        \
                                                                                         \
static void OPNAME ## _pixels_y2(BTYPE *block, const UINT8 *pixels, int line_size, int h)     \
{                                                                                        \
    BTYPE *p;                                                                          \
    const UINT8 *pix;                                                                    \
    const UINT8 *pix1;                                                                   \
                                                                                         \
    p = block;                                                                           \
    pix = pixels;                                                                        \
    pix1 = pixels + line_size;                                                           \
    do {                                                                                 \
        OP(p[0], avg2(pix[0], pix1[0]));                                                   \
        OP(p[1], avg2(pix[1], pix1[1]));                                                   \
        OP(p[2], avg2(pix[2], pix1[2]));                                                   \
        OP(p[3], avg2(pix[3], pix1[3]));                                                   \
        OP(p[4], avg2(pix[4], pix1[4]));                                                   \
        OP(p[5], avg2(pix[5], pix1[5]));                                                   \
        OP(p[6], avg2(pix[6], pix1[6]));                                                   \
        OP(p[7], avg2(pix[7], pix1[7]));                                                   \
        pix += line_size;                                                                \
        pix1 += line_size;                                                               \
        p += INCR;                                                                       \
    } while(--h);                                                                         \
}                                                                                        \
                                                                                         \
static void OPNAME ## _pixels_xy2(BTYPE *block, const UINT8 *pixels, int line_size, int h)    \
{                                                                                        \
    BTYPE *p;                                                                          \
    const UINT8 *pix;                                                                    \
    const UINT8 *pix1;                                                                   \
                                                                                         \
    p = block;                                                                           \
    pix = pixels;                                                                        \
    pix1 = pixels + line_size;                                                           \
    do {                                                                   \
        OP(p[0], avg4(pix[0], pix[1], pix1[0], pix1[1]));                                  \
        OP(p[1], avg4(pix[1], pix[2], pix1[1], pix1[2]));                                  \
        OP(p[2], avg4(pix[2], pix[3], pix1[2], pix1[3]));                                  \
        OP(p[3], avg4(pix[3], pix[4], pix1[3], pix1[4]));                                  \
        OP(p[4], avg4(pix[4], pix[5], pix1[4], pix1[5]));                                  \
        OP(p[5], avg4(pix[5], pix[6], pix1[5], pix1[6]));                                  \
        OP(p[6], avg4(pix[6], pix[7], pix1[6], pix1[7]));                                  \
        OP(p[7], avg4(pix[7], pix[8], pix1[7], pix1[8]));                                  \
        pix += line_size;                                                                \
        pix1 += line_size;                                                               \
        p += INCR;                                                                       \
    } while(--h);                                                                         \
}                                                                                        \
                                                                                         \
void (*OPNAME ## _pixels_tab[4])(BTYPE *block, const UINT8 *pixels, int line_size, int h) = { \
    OPNAME ## _pixels,                                                                   \
    OPNAME ## _pixels_x2,                                                                \
    OPNAME ## _pixels_y2,                                                                \
    OPNAME ## _pixels_xy2,                                                               \
};


/* rounding primitives */
#define avg2(a,b) ((a+b+1)>>1)
#define avg4(a,b,c,d) ((a+b+c+d+2)>>2)

#define op_put(a, b) a = b
#define op_avg(a, b) a = avg2(a, b)
#define op_sub(a, b) a -= b

PIXOP(UINT8, put, op_put, line_size)
PIXOP(UINT8, avg, op_avg, line_size)

PIXOP(DCTELEM, sub, op_sub, 8)

/* not rounding primitives */
#undef avg2
#undef avg4
#define avg2(a,b) ((a+b)>>1)
#define avg4(a,b,c,d) ((a+b+c+d+1)>>2)

PIXOP(UINT8, put_no_rnd, op_put, line_size)
PIXOP(UINT8, avg_no_rnd, op_avg, line_size)

/* motion estimation */

#undef avg2
#undef avg4
#define avg2(a,b) ((a+b+1)>>1)
#define avg4(a,b,c,d) ((a+b+c+d+2)>>2)

int pix_abs16x16_c(UINT8 *pix1, UINT8 *pix2, int line_size, int h)
{
    int s, i;

    s = 0;
    for(i=0;i<h;i++) {
        s += abs(pix1[0] - pix2[0]);
        s += abs(pix1[1] - pix2[1]);
        s += abs(pix1[2] - pix2[2]);
        s += abs(pix1[3] - pix2[3]);
        s += abs(pix1[4] - pix2[4]);
        s += abs(pix1[5] - pix2[5]);
        s += abs(pix1[6] - pix2[6]);
        s += abs(pix1[7] - pix2[7]);
        s += abs(pix1[8] - pix2[8]);
        s += abs(pix1[9] - pix2[9]);
        s += abs(pix1[10] - pix2[10]);
        s += abs(pix1[11] - pix2[11]);
        s += abs(pix1[12] - pix2[12]);
        s += abs(pix1[13] - pix2[13]);
        s += abs(pix1[14] - pix2[14]);
        s += abs(pix1[15] - pix2[15]);
        pix1 += line_size;
        pix2 += line_size;
    }
    return s;
}

int pix_abs16x16_x2_c(UINT8 *pix1, UINT8 *pix2, int line_size, int h)
{
    int s, i;

    s = 0;
    for(i=0;i<h;i++) {
        s += abs(pix1[0] - avg2(pix2[0], pix2[1]));
        s += abs(pix1[1] - avg2(pix2[1], pix2[2]));
        s += abs(pix1[2] - avg2(pix2[2], pix2[3]));
        s += abs(pix1[3] - avg2(pix2[3], pix2[4]));
        s += abs(pix1[4] - avg2(pix2[4], pix2[5]));
        s += abs(pix1[5] - avg2(pix2[5], pix2[6]));
        s += abs(pix1[6] - avg2(pix2[6], pix2[7]));
        s += abs(pix1[7] - avg2(pix2[7], pix2[8]));
        s += abs(pix1[8] - avg2(pix2[8], pix2[9]));
        s += abs(pix1[9] - avg2(pix2[9], pix2[10]));
        s += abs(pix1[10] - avg2(pix2[10], pix2[11]));
        s += abs(pix1[11] - avg2(pix2[11], pix2[12]));
        s += abs(pix1[12] - avg2(pix2[12], pix2[13]));
        s += abs(pix1[13] - avg2(pix2[13], pix2[14]));
        s += abs(pix1[14] - avg2(pix2[14], pix2[15]));
        s += abs(pix1[15] - avg2(pix2[15], pix2[16]));
        pix1 += line_size;
        pix2 += line_size;
    }
    return s;
}

int pix_abs16x16_y2_c(UINT8 *pix1, UINT8 *pix2, int line_size, int h)
{
    int s, i;
    UINT8 *pix3 = pix2 + line_size;

    s = 0;
    for(i=0;i<h;i++) {
        s += abs(pix1[0] - avg2(pix2[0], pix3[0]));
        s += abs(pix1[1] - avg2(pix2[1], pix3[1]));
        s += abs(pix1[2] - avg2(pix2[2], pix3[2]));
        s += abs(pix1[3] - avg2(pix2[3], pix3[3]));
        s += abs(pix1[4] - avg2(pix2[4], pix3[4]));
        s += abs(pix1[5] - avg2(pix2[5], pix3[5]));
        s += abs(pix1[6] - avg2(pix2[6], pix3[6]));
        s += abs(pix1[7] - avg2(pix2[7], pix3[7]));
        s += abs(pix1[8] - avg2(pix2[8], pix3[8]));
        s += abs(pix1[9] - avg2(pix2[9], pix3[9]));
        s += abs(pix1[10] - avg2(pix2[10], pix3[10]));
        s += abs(pix1[11] - avg2(pix2[11], pix3[11]));
        s += abs(pix1[12] - avg2(pix2[12], pix3[12]));
        s += abs(pix1[13] - avg2(pix2[13], pix3[13]));
        s += abs(pix1[14] - avg2(pix2[14], pix3[14]));
        s += abs(pix1[15] - avg2(pix2[15], pix3[15]));
        pix1 += line_size;
        pix2 += line_size;
        pix3 += line_size;
    }
    return s;
}

int pix_abs16x16_xy2_c(UINT8 *pix1, UINT8 *pix2, int line_size, int h)
{
    int s, i;
    UINT8 *pix3 = pix2 + line_size;

    s = 0;
    for(i=0;i<h;i++) {
        s += abs(pix1[0] - avg4(pix2[0], pix2[1], pix3[0], pix3[1]));
        s += abs(pix1[1] - avg4(pix2[1], pix2[2], pix3[1], pix3[2]));
        s += abs(pix1[2] - avg4(pix2[2], pix2[3], pix3[2], pix3[3]));
        s += abs(pix1[3] - avg4(pix2[3], pix2[4], pix3[3], pix3[4]));
        s += abs(pix1[4] - avg4(pix2[4], pix2[5], pix3[4], pix3[5]));
        s += abs(pix1[5] - avg4(pix2[5], pix2[6], pix3[5], pix3[6]));
        s += abs(pix1[6] - avg4(pix2[6], pix2[7], pix3[6], pix3[7]));
        s += abs(pix1[7] - avg4(pix2[7], pix2[8], pix3[7], pix3[8]));
        s += abs(pix1[8] - avg4(pix2[8], pix2[9], pix3[8], pix3[9]));
        s += abs(pix1[9] - avg4(pix2[9], pix2[10], pix3[9], pix3[10]));
        s += abs(pix1[10] - avg4(pix2[10], pix2[11], pix3[10], pix3[11]));
        s += abs(pix1[11] - avg4(pix2[11], pix2[12], pix3[11], pix3[12]));
        s += abs(pix1[12] - avg4(pix2[12], pix2[13], pix3[12], pix3[13]));
        s += abs(pix1[13] - avg4(pix2[13], pix2[14], pix3[13], pix3[14]));
        s += abs(pix1[14] - avg4(pix2[14], pix2[15], pix3[14], pix3[15]));
        s += abs(pix1[15] - avg4(pix2[15], pix2[16], pix3[15], pix3[16]));
        pix1 += line_size;
        pix2 += line_size;
        pix3 += line_size;
    }
    return s;
}

void dsputil_init(void)
{
    int i;

    for(i=0;i<256;i++) cropTbl[i + MAX_NEG_CROP] = i;
    for(i=0;i<MAX_NEG_CROP;i++) {
        cropTbl[i] = 0;
        cropTbl[i + MAX_NEG_CROP + 256] = 255;
    }

    for(i=0;i<512;i++) {
        squareTbl[i] = (i - 256) * (i - 256);
    }

    get_pixels = get_pixels_c;
    put_pixels_clamped = put_pixels_clamped_c;
    add_pixels_clamped = add_pixels_clamped_c;

    pix_abs16x16 = pix_abs16x16_c;
    pix_abs16x16_x2 = pix_abs16x16_x2_c;
    pix_abs16x16_y2 = pix_abs16x16_y2_c;
    pix_abs16x16_xy2 = pix_abs16x16_xy2_c;
    av_fdct = jpeg_fdct_ifast;

#ifdef HAVE_MMX
    dsputil_init_mmx();
#endif
}

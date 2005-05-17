/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_GreedyHM.c,v 1.1.2.2 2005-05-17 19:58:32 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2001 Tom Barry.  All rights reserved.
// Copyright (C) 2005 Michael H. Schimek
/////////////////////////////////////////////////////////////////////////////
//
//	This file is subject to the terms of the GNU General Public License as
//	published by the Free Software Foundation.  A copy of this license is
//	included with this software distribution in the file COPYING.  If you
//	do not have a copy, you may obtain a copy by writing to the Free
//	Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//	This software is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////
// Change Log
//
// Date          Developer             Changes
//
// 01 Feb 2001   Tom Barry	       New Greedy (High Motion)
//					 Deinterlace method
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.1.2.1  2005/05/05 09:46:00  mschimek
// *** empty log message ***
//
// Revision 1.1  2005/01/08 14:54:23  mschimek
// *** empty log message ***
//
// Revision 1.10  2001/11/25 04:33:37  trbarry
// Fix for TDeinterlace_Info. Also release UN-Filter code, 5-tap V & H
//   sharp/soft filters optimized to reverse excessive filtering (or EE?)
//
// Revision 1.9  2001/10/02 17:44:41  trbarry
// Changes to be compatible with the Avisynth filter version
//
// Revision 1.8  2001/08/19 06:26:38  trbarry
// Remove Greedy HM Low Motion Only option and files
//
// No longer needed
//
// Revision 1.7  2001/08/17 16:18:35  trbarry
// Minor GreedyH performance Enh.
// Only do pulldown calc when needed.
// Will become more needed in future when calc more expensive.
//
// Revision 1.6  2001/08/01 00:37:41  trbarry
// More chroma jitter fixes, tweak defaults
//
// Revision 1.5  2001/07/30 21:50:32  trbarry
// Use weave chroma for reduced chroma jitter. Fix DJR bug again.
// Turn off Greedy Pulldown default.
//
// Revision 1.4  2001/07/30 17:56:26  trbarry
// Add Greedy High Motion MMX, K6-II, K6-III, and Celeron support.
// Tweak defaults.
//
// Revision 1.3  2001/07/28 18:47:24  trbarry
// Fix Sharpness with Median Filter
// Increase Sharpness default to make obvious
// Adjust deinterlace defaults for less jitter
//
// Revision 1.2  2001/07/25 12:04:31  adcockj
// Moved Control stuff into DS_Control.h
// Added $Id and $Log to comment blocks as per standards
//
///////////////////////////////////////////////////////////////////////////*/

#include "windows.h"
#include "DI_GreedyHM.h"

#if !SIMD

/* Note - actual default values below may be set in DI_GreedyHSETTINGS */
long GreedyHMaxComb = 5;	 /* max comb we allow past clip */
long GreedyMotionThreshold = 25; /* ignore changes < this */
long GreedyMotionSense = 30;	 /* how rapidly to bob when > Threshold */
long GreedyGoodPullDownLvl = 83; /* Best Comb avg / Comb Avg must be < thes */
/* No Pulldown if field comb / Best avg comb > this */
long GreedyBadPullDownLvl = 88;
long GreedyHSharpnessAmt = 50;	 /* % H. sharpness to add or filter */
long GreedyVSharpnessAmt = 23;	 /* % V. sharpness to add or filter */
long GreedyMedianFilterAmt = 3;	 /* Don't filter if > this */
long GreedyLowMotionPdLvl = 9;	 /* Do PullDown on if motion < this */

BOOL GreedyUsePulldown = FALSE;			
BOOL GreedyUseInBetween = FALSE;
BOOL GreedyUseMedianFilter = FALSE;
BOOL GreedyUseVSharpness = FALSE;
BOOL GreedyUseHSharpness = FALSE;

/*  Input video data is first copied to the FieldStore array, possibly doing
    edge enhancement and median filtering. Field store is layed out to
    improve register usage and cache performace during further deinterlace
    processing.

    Hopefully we will gain enough using it to make up for the cost of
    filling it in column order. Note array transposed (1000 cols, 240 rows,
    1000 cols) */

uint8_t FieldStore[FSSIZE];

unsigned int FsPtr = 0;
unsigned int FsDelay = 1;

/* return FS subscripts depending on delay
   (L2P not returned as that is always L2 ^ 2). */
BOOL
SetFsPtrs			(int *			L1,
				 int *			L2,
				 int *			L3,
				 int *			CopySrc,
				 uint8_t **		CopyDest,
				 uint8_t **		WeaveDest,
				 TDeinterlaceInfo *	pInfo)
{
    if (FsDelay == 2) {
	if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
/* Assume here we are doing median filtering so we are delaying
   by 2 fields.  When we are doing Median filter we have to
   delay the display by 2 fields so at Time=5 we are displaying
   a screen for Time=3, For ODD fields we display an odd field
   and have the following, representing part of 1 column on the
   screen when Line=0, Time = 5, and W = the weave pixel we want
   to calc:

   Row  Fields (at Time=1..5)  Just got odd field 5, display odd frame 3
   ---  --------------------
         1  2  3  4  5
   -1    .  .  L1 .  x	Not really any -1 row but we pretend at first			
    0    . L2P W  L2 .	We create the W pixel somehow, FsPtrP will point L2
    1    .  .  L3 .  x	Odd Rows directly copied, FsPtrP2 will point to L3 */
	    *L2 = (FsPtr - 1) % 4;	/* the newest weave pixel */
	    /* Bottom curr pixel offset is prev odd pixel */
	    *L3 = FsPtr ^ 2;
	    *CopySrc = *L3;		/* Always copy from prev pixels */
	    /* top curr pixel offset, tricked on 1st line */
	    *L1 = *L3 - FSMAXCOLS;
	    *WeaveDest = pInfo->Overlay; /* where the weave pixel goes */
	    /* Dest for copy or vert filter pixel pixel */
	    *CopyDest = (uint8_t *) pInfo->Overlay + pInfo->OverlayPitch;
	} else {
/* Row  Fields (at Time=1..5)  Just got even frame 4, display even frame 2
   ---  --------------------
          1  2  3  4  5
    0     .  L1	.  x  .	 Even Rows are directly copied, FsPtrP2 will point L1
    1	 L2P W  L2 .  .	 We create the W pixel somehow, FsPtrP will point L2
    2     .  L3 .  x  .	 Even Rows directly copied for Even fields */
	    *L2 = (FsPtr - 1) % 4;	/* the newest weave pixel */
	    *L1 = FsPtr ^ 2;		/* top curr pixel subscript */
	    *CopySrc = *L1;		/* Always copy from prev pixels */
	    /* bottom curr pixel subscript, tricked on last line */
	    *L3 = *L1 + FSMAXCOLS;
	    /* Dest for weave pixel */
	    *WeaveDest = (uint8_t *) pInfo->Overlay + pInfo->OverlayPitch;
	    /* Dest for copy or vert filter pixel pixel */
	    *CopyDest = pInfo->Overlay;
	}
    } else {
	/* Assume FsDelay = 1; */

	if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
/* Assume here we are not doing median filtering so we are delaying
   only one field.  When we are not doing Median filter we have to
   delay the display by 1 fields so at Time=5 we are displaying
   a screen for Time=4, For ODD fields we display an even field
   and have the following, representing part of 1 column on the
   screen when Line=0, Time = 5, and W = the weave pixel we want
   to calc:

   Row  Fields (at Time=1..5)  Just got odd frame 5, display even frame 4
   ---  --------------------
           1  2  3  4  5
    0         x  .  L1 .   Even Rows are directly copied, FsPtrP will point L1
    1           L2P W  L2  We create the W pixel somehow, PsPtr will point L2
    2         x  .  L3 .   Even Rows directly copied for Odd fields
    			   Note L3 not on last line, L1 used twice there */
	    *L2 = FsPtr;		/* the newest weave pixel */
	    *L1 = (FsPtr - 1) % 4;	/* top curr pixel subscript */
	    *CopySrc = *L1;		/* Always copy from prev pixels */
	    /* bottom curr pixel subscript, tricked on last line */
	    *L3 = *L1 + FSMAXCOLS;
	    /* Dest for weave pixel */
	    *WeaveDest = (uint8_t *) pInfo->Overlay + pInfo->OverlayPitch;
	    /* Dest for copy or vert filter pixel pixel */
	    *CopyDest = pInfo->Overlay;
	} else {
/* Row  Fields (at Time=1..5)  Just got even frame 4, display odd frame 3
   ---  --------------------
	   1  2  3  4  5
   -1		 L1	Not really any -1 row but we pretend at first
    0        L2P W  L2	We create the W pixel somehow, PsPtr will point to L2
    1            L3	Odd Rows directly copied, FsPtrP will point to L3 */
	    *L2 = FsPtr;		/* the newest weave pixel */
	    /* Bottom curr pixel offset is prev odd pixel */
	    *L3 = (FsPtr - 1) % 4;
	    *CopySrc = *L3;		/* Always copy from prev pixels */
	    /* top curr pixel offset, tricked on 1st line */
	    *L1 = *L3 - FSMAXCOLS;
	    *WeaveDest = pInfo->Overlay;
	    /* Dest for copy or vert filter pixel pixel */
	    *CopyDest = (uint8_t *) pInfo->Overlay + pInfo->OverlayPitch;
	}
    }

    return TRUE;
}

#else /* SIMD */

typedef union {
    vu32		v[2];
    uint32_t		a[2 * sizeof (vu32) / sizeof (uint32_t)];
} pd_sum_union;

static always_inline void
pulldown_sum			(pd_sum_union *		sum,
				 vu8			prev,
				 vu8			new,
				 vu8			prev_next_row,
				 vu8			prev2)
{
#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW)
    v16 prev_y, new_y;
    v16 comb, contr, motion;

    prev_y = yuyv2yy (prev);
    new_y = yuyv2yy (new);

    comb = vabsdiffu8 (prev_y, new_y);
    contr = vabsdiffu8 (prev_y, yuyv2yy (prev_next_row));
    /* Adds high and low halves and
       combines to { contr, comb, contr, comb }. */
    comb = vadd16 (vunpacklo16 (comb, contr),
		   vunpackhi16 (comb, contr));
    /* Adds high and low halves to { x, x, contr, comb }. */
    comb = vadd16 (comb, vsru (comb, 32));
    /* Extends low half to v32 and sums to { contr, comb }. */
    sum->v[0] = vadd32 (sum->v[0], vunpacklo16 (comb, vzero16 ()));

    motion = vabsdiffu8 (new_y, yuyv2yy (prev2));
    motion = vadd16 (motion, vsru (motion, 32));
    /* Sums to { motion, motion }. */
    sum->v[1] = vadd32 (sum->v[1], vunpacklo16 (motion, vzero16 ()));

#elif SIMD == CPU_FEATURE_SSE
    v16 prev_y, new_y;
    v32 comb, contr, motion;

    prev_y = yuyv2yy (prev);
    new_y = yuyv2yy (new);

    /* XXX wasting cycles because half the bytes are zero... */
    comb = _mm_sad_pu8 (prev_y, new_y);
    contr = _mm_sad_pu8 (prev_y, yuyv2yy (prev_next_row));

    /* Pixels from previous field, same row & col. */
    motion = _mm_sad_pu8 (new_y, yuyv2yy (prev2));

    /* Sums to { contr, comb }, { 0, motion }. */
    sum->v[0] = vadd32 (sum->v[0], vunpacklo32 (comb, contr));
    sum->v[1] = vadd32 (sum->v[1], motion);

#elif SIMD & (CPU_FEATURE_SSE2 | CPU_FEATURE_SSE3)
    v16 prev_y, new_y;
    v32 comb, contr, motion;

    prev_y = yuyv2yy (prev);
    new_y = yuyv2yy (new);

    /* Yields { 0, comb, 0, comb }. */
    comb = _mm_sad_epu8 (prev_y, new_y);
    contr = _mm_sad_epu8 (prev_y, yuyv2yy (prev_next_row));

    /* Combine to { contr, comb, contr, comb }. */
    comb = vor (comb, vsl (contr, 32));
    /* Add high and low halves to { x, x, contr, comb }. */
    comb = vadd32 (comb, vsru (comb, 64));

    /* Yields { 0, motion, 0, motion }. */
    motion = _mm_sad_epu8 (new_y, yuyv2yy (prev2));
    /* { x, x, 0, motion } */
    motion = vadd32 (motion, vsru (motion, 64));

    /* Sums up to { 0, motion, contr, comb }, hopefully in a register. */
    sum->v[0] = vadd32 (sum->v[0], vunpacklo64 (comb, motion));

#elif SIMD == CPU_FEATURE_ALTIVEC
    /* Note AVec is big endian (0xY0UaY1Va) and
       we compare Y values only. */
    const vu8 sel1 = { 0x00, 0x02, 0x04, 0x06, 0x00, 0x02, 0x04, 0x06,
		       0x08, 0x0A, 0x0C, 0x0E, 0x08, 0x0A, 0x0C, 0x0E };
    const vu8 sel2 = { 0x00, 0x02, 0x04, 0x06, 0x10, 0x12, 0x14, 0x16,
		       0x08, 0x0A, 0x0C, 0x0E, 0x18, 0x1A, 0x1C, 0x1E };
    vu8 t1, t2;

    /* comb = absdiff (prev, new);
       contr = absdiff (prev, prev_next_row); */
    t1 = vec_perm (prev, prev, sel1);
    t2 = vec_perm (new, prev_next_row, sel2);
    t1 = vabsdiffu8 (t1, t2);

    /* This sums t[0 .. 3], t[4 ... 7], t[8 ... 11], t[12 ... 15]
       giving { comb, contr, comb, contr }, and does an additional
       vadd32() to sum->v[0]. */
    sum->v[0] = vec_sum4s (t1, sum->v[0]);

    t1 = vabsdiffu8 (new, prev2);
    t1 = (vu8) vsru16 ((vu16) t1, 8); /* throw away uv */
    /* Sums to { motion, motion, motion, motion }. */
    sum->v[1] = vec_sum4s (t1, sum->v[1]);
#endif
}

static always_inline vu8
median_filter			(vu8			prev,
				 vu8			old,
				 vu8			new)
{
	vu8 flt, min, max, diff;

	vminmaxu8 (&min, &max, old, new);
	flt = vsatu8 (prev, min, max);

	/* decide if we want to use the filtered value,
	   depending upon how much effect it has:
	   absdiff (prev, flt) <= GMFA ? flt : prev */
	diff = vabsdiffu8 (prev, flt);
	return vsel (vcmpleu8 (diff, (vu8) vsplat16 (GreedyMedianFilterAmt)),
		     flt, prev);
}

static always_inline void
loop_kernel			(uint8_t **		fs,
				 const uint8_t **	src,
				 pd_sum_union *		pd_sum,
				 unsigned int		FsPrev2,
				 unsigned int		FsPrev,
				 unsigned int		FsNewOld,
				 unsigned int		byte_width,
				 v16			QHA,
				 v16			QHB,
				 v16			QHC,
				 int			use_sharpness,
				 int			use_softness,
				 int			use_pulldown,
				 int			use_median_filter)
{
    if (use_sharpness) {
	int count;
	vu8 am, a0, a1;

	am = vzerou8 ();
	a0 = ((const vu8 *) *src)[0];

	for (count = byte_width / sizeof (vu8) - 1; count > 0; --count) {
	    vu8 old, new, prev2, l, r;
	    vu16 avg2, avg4, newy;

	    a1 = ((const vu8 *) *src)[1];

	last_column:
	    /* get avg of -2 & +2 pixels */
	    vshiftu2x (&l, &r, am, a0, a1, 2);
	    avg2 = vmullo16 (yuyv2yy (fast_vavgu8 (l, r)), QHB);

	    /* get avg of -4 & +4 pixels */
	    vshiftu2x (&l, &r, am, a0, a1, 4);
	    avg4 = vmullo16 (yuyv2yy (fast_vavgu8 (l, r)), QHC);

	    /* get ratio of center pixel and combine */
	    newy = vmullo16 (yuyv2yy (a0), QHA);

	    /* add in weighted average of Zj,Zl */
	    if (use_softness)
		newy = vaddsu16 (newy, vaddsu16 (avg2, avg4));
	    else
		newy = vsubsu16 (newy, vsubsu16 (avg2, avg4));

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW)
	    newy = vminu16i (vsru16 (newy, 6), 255);
#elif SIMD == CPU_FEATURE_ALTIVEC
	    newy = vec_min (vsru16 (newy, 6), vsplatu16_255);
#else
	    /* Should be vminu but vmin is ok. */
	    newy = vmin16 (vsru16 (newy, 6), vsplat16_255);
#endif
	    new = recombine_yuyv (newy, a0);

	    am = a0;
	    a0 = a1;

	    old = * (vu8 *)(*fs + FsNewOld);
	    /* save our sharp new value for next time */
	    * (vu8 *)(*fs + FsNewOld) = new;
	    prev2 = * (vu8 *)(*fs + FsPrev2);

	    if (use_pulldown)
		pulldown_sum (pd_sum,
			      * (vu8 *)(*fs + FsPrev), new,
			      * (vu8 *)(*fs + FsPrev + FSROWSIZE), prev2);

	    if (use_median_filter)
		* (vu8 *)(*fs + FsPrev2) = median_filter (prev2, old, new);

	    *fs += FSCOLSIZE;
	    *src += sizeof (vu8);
	}

	a1 = vzerou8 ();

	if (count >= 0)
	    goto last_column;
    } else {
	unsigned int count;

	for (count = byte_width / sizeof (vu8); count > 0; --count) {
	    vu8 old, new, prev2;

	    /* no sharpness, just get curr value */
	    new = * (const vu8 *) *src;
	    old = * (vu8 *)(*fs + FsNewOld);
	    /* save our sharp new value for next time */
	    * (vu8 *)(*fs + FsNewOld) = new;
	    prev2 = * (vu8 *)(*fs + FsPrev2);

	    if (use_pulldown)
		pulldown_sum (pd_sum,
			      * (vu8 *)(*fs + FsPrev), new,
			      * (vu8 *)(*fs + FsPrev + FSROWSIZE), prev2);

	    if (use_median_filter)
		* (vu8 *)(*fs + FsPrev2) = median_filter (prev2, old, new);

	    *fs += FSCOLSIZE;
	    *src += sizeof (vu8);
	}
    }
}

static always_inline BOOL
DI_GrUpdtFS_template		(TDeinterlaceInfo *	pInfo,
				 int			use_sharpness,
				 int			use_softness,
				 int			use_pulldown,
				 int			use_median_filter)
{
    pd_sum_union pd_sum;
    v16 QHA, QHB, QHC; 
    uint8_t *fs;
    const uint8_t *src;
    unsigned int FsPrev2;
    unsigned int FsPrev;
    unsigned int FsNewOld;
    unsigned int fs_padding;
    unsigned int src_padding;
    unsigned int byte_width;
    unsigned int height;
    unsigned int skip;
    unsigned int end;

    if (NULL == pInfo->PictureHistory[0]->pData)
	return FALSE;

    {
	int w, Q, Q2, denom, A, B;

	/* For the math, see the comments on the
	   PullDown_VSharp() function in the Pulldown code */

	/* note-adj down for overflow */
	if (GreedyHSharpnessAmt > 0) {
	    /* overflow, use 38%, 0<w<1000 */
	    w = 1000 - (GreedyHSharpnessAmt * 38 / 10);
	} else {
	    /* bias towards workable range */
	    w = 1000 - (GreedyHSharpnessAmt * 150 / 10);
	}
	
	Q = 500 * (1000 - w) / w; /* Q as 0 - 1K, max 16k */     
	Q2 = (Q * Q) / 1000; /* Q^2 as 0 - 1k */
	denom = (w * (1000 - 2 * Q2)) / 1000; /* [w (1-2q^2)] as 0 - 1k */

	A = 64000 / denom; /* A as 0 - 64 */
	QHA = vsplat16 (A); /* A as 0 - 64 */

	B = 128 * Q / denom; /* B as 0 - 64 */
	if (use_softness) {
	    int MB = -B;
	    QHB = vsplat16 (MB);
	} else {
	    QHB = vsplat16 (B);
	}

	{
	    int C = 64 - A + B; /* so A-B+C=64, unbiased weight */
	    QHC = vsplat16 (C);
	}
    }

    fs = FieldStore;
    src = pInfo->PictureHistory[0]->pData;

    /* MHS: FieldStore is organized as rows * (columns / gran)
       * fields * gran pixels, where gran = sizeof (vu8) / 2. */

    /* Offset to prev pixel (this line) to be median filtered */
    FsPrev2 = (FsPtr - 1) % 4 * sizeof (vu8);
    /* FieldStore elem holding pixels from prev field line */
    FsPrev = FsPtr * sizeof (vu8);
    FsPtr = (FsPtr + 1) % 4; /* NB this is a global */
    /* Offset to Oldest odd pixel, will be replaced */
    FsNewOld = FsPtr * sizeof (vu8);

    byte_width = pInfo->LineLength;

    fs_padding = FSROWSIZE - byte_width * (FSCOLSIZE / sizeof (vu8));
    src_padding = pInfo->InputPitch - byte_width;

    height = pInfo->FieldHeight;
    skip = 0;
    end = 0;

    if (use_pulldown) {
	skip = height / 4;
	end = height - skip;
    }

    for (;;) {
	/* Don't collect pulldown data during
	   first and last <skip> lines. */
	for (; height > end; --height) {
	    loop_kernel (&fs, &src,
			 NULL,
			 FsPrev2, FsPrev, FsNewOld,
			 byte_width, QHA, QHB, QHC,
			 use_sharpness, use_softness,
			 /* use_pulldown */ FALSE,
			 use_median_filter);

	    fs += fs_padding;
	    src += src_padding;
	}

	if (0 == height)
	    break;

	pd_sum.v[0] = vzerou32 ();
	pd_sum.v[1] = vzerou32 ();

	for (; height > skip; --height) {
	    loop_kernel (&fs, &src,
			 &pd_sum,
			 FsPrev2, FsPrev, FsNewOld,
			 byte_width, QHA, QHB, QHC,
			 use_sharpness, use_softness,
			 /* use_pulldown */ TRUE,
			 use_median_filter);

	    fs += fs_padding;
	    src += src_padding;
	}

	end = 0;
    }

    if (use_pulldown) {
	unsigned int scale;

	scale = (pInfo->FieldHeight - 2 * skip) * byte_width / 100;

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW)
	UpdatePulldown (pInfo,
			/* comb */     pd_sum.a[0] / scale,
			/* contrast */ pd_sum.a[1] / scale,
			/* motion */  (pd_sum.a[2] + pd_sum.a[3]) / scale);
#elif SIMD & (CPU_FEATURE_SSE | CPU_FEATURE_SSE2 | CPU_FEATURE_SSE3)
	UpdatePulldown (pInfo,
			/* comb */     pd_sum.a[0] / scale,
			/* contrast */ pd_sum.a[1] / scale,
			/* motion */   pd_sum.a[2] / scale);
#elif SIMD == CPU_FEATURE_ALTIVEC
	/* motion + motion + motion + motion */
	pd_sum.v[1] = (vu32) vec_sums ((v32) pd_sum.v[1], vzero32 ());
	UpdatePulldown (pInfo,
			/* comb */     (pd_sum.a[0] + pd_sum.a[2]) / scale,
			/* contrast */ (pd_sum.a[1] + pd_sum.a[3]) / scale,
			/* motion */    pd_sum.a[7] / scale);
#endif
    }

    vempty ();

    return TRUE;
}

#define DI_GrUpdtFS(suffix, mf, sh, pd, so)				\
static BOOL								\
DI_GrUpdtFS_ ## suffix		(TDeinterlaceInfo *	pInfo)		\
{									\
    return DI_GrUpdtFS_template (pInfo,					\
				 /* use_sharpness */ sh,		\
			         /* use_softness */ so,			\
			         /* use_pulldown */ pd,			\
			         /* use_median_filter */ mf);		\
}

DI_GrUpdtFS (NM_NE_NP,     0, 0, 0, 0)
DI_GrUpdtFS (NM_NE_P,      0, 0, 1, 0)
DI_GrUpdtFS (NM_E_NP,      0, 1, 0, 0)
DI_GrUpdtFS (NM_E_P,       0, 1, 1, 0)
DI_GrUpdtFS (M_NE_NP,      1, 0, 0, 0)
DI_GrUpdtFS (M_NE_P,       1, 0, 1, 0)
DI_GrUpdtFS (M_E_NP,       1, 1, 0, 0)
DI_GrUpdtFS (M_E_P,        1, 1, 1, 0)

DI_GrUpdtFS (NM_E_NP_Soft, 0, 1, 0, 1)
DI_GrUpdtFS (NM_E_P_Soft,  0, 1, 1, 1)
DI_GrUpdtFS (M_E_NP_Soft,  1, 1, 0, 1)
DI_GrUpdtFS (M_E_P_Soft,   1, 1, 1, 1)

static BOOL
UpdateFieldStore		(TDeinterlaceInfo *	pInfo)
{
    if (GreedyUsePulldown) {
	if (GreedyUseMedianFilter && GreedyMedianFilterAmt > 0) {
	    FsDelay = 2;

	    if (GreedyUseHSharpness && GreedyHSharpnessAmt) {
                if (GreedyHSharpnessAmt > 0) {
		    return DI_GrUpdtFS_M_E_P (pInfo);
                } else {
		    return DI_GrUpdtFS_M_E_P_Soft (pInfo);
                }
            } else {
		return DI_GrUpdtFS_M_NE_P (pInfo);
	    }
	} else {
	    FsDelay = 1;

	    if (GreedyUseHSharpness && GreedyHSharpnessAmt) {
                if (GreedyHSharpnessAmt > 0) {
		    return DI_GrUpdtFS_NM_E_P (pInfo);
                } else {
		    return DI_GrUpdtFS_NM_E_P_Soft (pInfo);
                }
            } else {
		return DI_GrUpdtFS_NM_NE_P (pInfo);
	    }
        }
    } else {
	if (GreedyUseMedianFilter && GreedyMedianFilterAmt > 0) {
	    FsDelay = 2;

	    if (GreedyUseHSharpness && GreedyHSharpnessAmt > 0) {
                if (GreedyHSharpnessAmt > 0) {
		    return DI_GrUpdtFS_M_E_NP (pInfo);
                } else {
		    return DI_GrUpdtFS_M_E_NP_Soft (pInfo);
                }
            } else {
		return DI_GrUpdtFS_M_NE_NP (pInfo);
	    }
	} else {
	    FsDelay = 1;

	    if (GreedyUseHSharpness && GreedyHSharpnessAmt > 0) {
                if (GreedyHSharpnessAmt > 0) {
		    return DI_GrUpdtFS_NM_E_NP (pInfo);
                } else {
		    return DI_GrUpdtFS_NM_E_NP_Soft (pInfo);
                }
            } else {
		return DI_GrUpdtFS_NM_NE_NP (pInfo);
	    }
        }
    }

    return FALSE;
}

/* Greedy High Motion Deinterlace, internal routine */
BOOL
SIMD_NAME (DI_GreedyHM)		(TDeinterlaceInfo *	pInfo)
{
    if (!UpdateFieldStore (pInfo)) {
	return FALSE;
    } else {
	if (SIMD_NAME (CanDoPulldown)(pInfo)) {
	    return TRUE;
	} else {
	    if (GreedyUseHSharpness && GreedyHSharpnessAmt > 0) {
		return SIMD_NAME (DI_GreedyHM_V)(pInfo);
	    } else {
		return SIMD_NAME (DI_GreedyHM_NV)(pInfo);
	    }
	}
    }

    return TRUE;
}

#endif /* SIMD */

/*
Local Variables:
c-basic-offset: 4
End:
 */

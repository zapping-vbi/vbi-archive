/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_GreedyHM.h,v 1.1.2.2 2005-05-17 19:58:32 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2001 Tom Barry.  All rights reserved.
// Copyright (c) 2005 Michael H. Schimek
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
// 01 Jul 2001   Tom Barry	       Added GreedyH Deinterlace method
//
/////////////////////////////////////////////////////////////////////////////
//
// This member contains the meat of the Greedy (High Motion) deinterlace
// method.  It is written to not be particularly dependend upon either
// DScaler or Windows.  It would be nice to keep it that way if possible
// as I'd like to also use it to port to other environments including
// maybe Linux, DirectShow filters, batch utilites, and maybe VirtualDub
// or TMPGEnc plug-ins.
//
// I'll add a bigger block of comments here from material I'll post on
// the list. Basically this was made from ideas used in the Blended Clip
// & Greedy (Low Motion) plug-in's.
//
// Then Edge Enhancement, Median Filtering, Vertical Filtering, Diagonal
// Jaggie Reduction (DJR ;-) ), n:n pulldown matching, and In-Between
// Frames were built on that.
//
// !!!  THIS REQUIRES A FAST SSE BOX (Celeron, Athlon, P-III, or P4. !!!
// It will just execute a copy of the old Greedy (Low Motion) if that is
// not present.
//
////////////////////////////////////////////////////////////////////////////*/

#include "DS_Deinterlace.h"

extern long GreedyHMaxComb;
extern long GreedyMotionThreshold;
extern long GreedyMotionSense;
extern long GreedyGoodPullDownLvl;
extern long GreedyBadPullDownLvl;
extern long GreedyHSharpnessAmt;
extern long GreedyVSharpnessAmt;
extern long GreedyMedianFilterAmt;
extern long GreedyLowMotionPdLvl;

extern BOOL GreedyUsePulldown;			
extern BOOL GreedyUseInBetween;
extern BOOL GreedyUseMedianFilter;
extern BOOL GreedyUseVSharpness;
extern BOOL GreedyUseHSharpness;

typedef struct {
    int Comb;		/* combs */
    int CombChoice;	/* val chosen by Greedy Choice */
    int Kontrast;	/* sum of all abs vertical diff in a field */
    int Motion;		/* sum of all abs vertical diff in a field */
    int Avg;		/* avg of last 10 combs (actually just a total) */
    int AvgChoice; /* avgs of last 10 chosen combs (actually just a total) */
    int Flags;	   /* a circlular history of last 20 Greedy choice flags */
    int Flags2;		/* various status flags, mostly for debugging */
} GR_PULLDOWN_INFO;

#define PD_VIDEO	(1 << 0) /* did video deinterlace for this frame */
#define PD_PULLDOWN	(1 << 1) /* did pulldown */
#define PD_BAD		(1 << 2) /* bad pulldown situation */
#define PD_LOW_MOTION	(1 << 3) /* did pulldown due to low motion */
#define PD_MERGED	(1 << 4) /* made an in between frame */
#define PD_32_PULLDOWN	(1 << 5) /* is 3:2 pulldown */
#define PD_22_PULLDOWN	(1 << 6) /* is 2:2 pulldown */
#define PD_ODD		(1 << 7) /* is Odd Field */

/* Allow space for max 288 rows/field, plus a spare, and
   max 896 sceen cols (should be a multiple of the cache line size). */
#define FSMAXROWS 289
#define FSMAXCOLS 896
/* Number of fields to buffer.
   Attention! this is hardcoded (grep FsPtr % 4). */
#define FSFIELDS 4
/* Bytes to skip for info for next col. */
#define FSCOLSIZE (FSFIELDS * sizeof (vu8))
/* Bytes to skip to get to info for 2nd row. */
#define FSROWSIZE (FSMAXCOLS * FSFIELDS)
/* Bytes in FieldStore array. */
#define FSSIZE (FSMAXROWS * FSROWSIZE)

extern uint8_t FieldStore[FSSIZE];

/* Current subscript (field) in FieldStore. */
extern unsigned int FsPtr;
/* Display is delayed by n fields (1,2,3). */
extern unsigned int FsDelay;

/* len of pulldown average, < len of queue */
#define PDAVGLEN 10

typedef void
LINE_COPY_FUNC			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		n_bytes);

extern int
UpdatePulldown(TDeinterlaceInfo *	pInfo,
	       int Comb, int Kontrast, int Motion);

SIMD_FN_PROTOS (LINE_COPY_FUNC, FieldStoreCopy);

SIMD_FN_PROTOS (DEINTERLACE_FUNC, DI_GreedyHF);
SIMD_FN_PROTOS (DEINTERLACE_FUNC, DI_GreedyHM);
SIMD_FN_PROTOS (DEINTERLACE_FUNC, DI_GreedyHM_V);
SIMD_FN_PROTOS (DEINTERLACE_FUNC, DI_GreedyHM_NV);
SIMD_FN_PROTOS (DEINTERLACE_FUNC, CanDoPulldown);

extern BOOL
SetFsPtrs			(int *			L1,
				 int *			L2,
				 int *			L3,
				 int *			CopySrc,
				 uint8_t **		CopyDest,
				 uint8_t **		WeaveDest,
				 TDeinterlaceInfo *	pInfo);

#if SIMD

/* Some common routines. */

static always_inline vu8
recombine_yuyv			(vu16			yy,
				 vu8			yuyv)
{
#if SIMD == CPU_FEATURE_ALTIVEC
    vu8 sel = { 0x01, 0x11, 0x03, 0x13,	0x05, 0x15, 0x07, 0x17,  
	        0x09, 0x19, 0x0B, 0x1B,	0x0D, 0x1D, 0x0F, 0x1F };  
    /* 0xY0UaY1Va */
    return vec_perm ((vu8) yy, yuyv, sel);
#else
    /* 0xVaY1UaY0 */
    return vor (yy, vand (yuyv, UVMask));
#endif
}

/* For debugging. */
#define USE_JAGGIE_REDUCTION 1
#define USE_GREEDY_CHOICE 1
#define USE_CLIP 1
#define USE_BOB_BLEND 1

typedef enum {
    STORE_WEAVE_L1,
    STORE_WEAVE_L3,
    STORE_L1WEAVE_L3WEAVE
} ghxc_mode;

/* MHS: This is a merge of GreedyDeLoop.asm which reads from FieldStore
   (hence src_incr) and the GreedyHF.c loop core which reads from
   PictureHistory.  *Dest stores in DI_GreedyHM_V() and DI_GreedyHM_NV()
   have been integrated. */
static always_inline void
GreedyHXCore			(uint8_t **		Dest,
				 const uint8_t **	L1,
				 const uint8_t **	L2,
				 const uint8_t **	L2P,
				 unsigned int		byte_width,
				 unsigned int		dst_bpl,
				 unsigned int		src_bpl,
				 unsigned int		src_incr,
				 vu8			MaxCombW,
				 vu8			MotionThresholdW,
				 v16			MotionSenseW,
				 ghxc_mode		mode)
{
    int count;
    vu8 am, a0, a1; /* previous, current, next column */

    /* For ease of reading, the comments below assume that
       we're operating on an odd field (i.e., that InfoIsOdd
       is true).  Assume the obvious for even lines. */

    am = vzerou8 (); /* no data left hand of first column */
    a0 = fast_vavgu8 (((const vu8 *) *L1)[0],
		      ((const vu8 *)(*L1 + src_bpl))[0]);

    for (count = byte_width / sizeof (vu8) - 1; count > 0; --count) {
	vu8 l1, l3, l2, l2p;
	vu8 weave, bob, motion;

	a1 = fast_vavgu8 (((const vu8 *) *L1)[1],
			  ((const vu8 *)(*L1 + src_bpl))[1]);

    last_column:
	/* DJR - Diagonal Jaggie Reduction
	   In the event that we are going to use an average (Bob)
	   pixel we do not want a jagged stair step effect.  To
	   combat this we avg in the 2 horizontally adjacent pixels
	   into the interpolated Bob mix.  This will do horizontal
	   smoothing for only the Bob'd pixels. */

	if (USE_JAGGIE_REDUCTION) {
	    vu8 l, c, r;

	    vshiftu2x (&l, &r, am, a0, a1, 2);

	    l = fast_vavgu8 (l, r);  /* avg of next and prev. pixel */
	    c = fast_vavgu8 (a0, l); /* avg of center with adjacent */

	    /* Don't do any more averaging than needed for MMX.
	       It hurts performance and causes rounding errors. */
	    if (SIMD != CPU_FEATURE_MMX) {
		l = fast_vavgu8 (l, c); /* 1/4 center, 3/4 adjacent */
		c = fast_vavgu8 (c, l); /* 3/8 center, 5/8 adjacent */
	    }

	    bob = c;
	} else {
	    bob = a0;
	}

	am = a0;
	a0 = a1;

	l2 = * (const vu8 *) *L2; /* L2 - the newest weave pixel value */
	*L2 += src_incr;
	
	l2p = * (const vu8 *) *L2P; /* L2P - the prev weave pixel */
	*L2P += src_incr;

	/* Greedy Choice
	   For a weave pixel candidate we choose whichever (preceding
	   or following) pixel that would yield the lowest comb factor.
	   This allows the possibilty of selecting choice pixels from 2
	   different field. */
	if (USE_GREEDY_CHOICE) {
	    /* use L2 or L2P depending upon which makes smaller comb:
	       absdiff (l2p, bob) <= absdiff (l2, bob) ? l2p : l2. */
	    weave = vsel ((vu8) vcmpleu8 (vabsdiffu8 (l2p, bob),
					  vabsdiffu8 (l2, bob)),
			  l2p, l2);
	} else {
	    weave = l2;
	}

	/* Let's measure movement, as how much the weave pixel has changed */
	motion = vabsdiffu8 (l2, l2p);

	/* MHS: We already had l1, l3 in the previous iteration, but
	   with few registers to save values reloading is faster. */
	l1 = * (const vu8 *) *L1;
	l3 = * (const vu8 *)(*L1 + src_bpl);

	/* Now lets clip our chosen value to be not outside of the range
	   of the high/low range L1-L3 by more than MaxComb.
	   This allows some comb but limits the damages and also allows more
	   detail than a boring oversmoothed clip. */
	if (USE_CLIP) {
	    vu8 min, max;

	    vminmaxu8 (&min, &max, l1, l3);

	    /* Allow the value to be above the high or below the low
	       by amt of MaxComb. */
	    max = vaddsu8 (max, MaxCombW);
	    min = vsubsu8 (min, MaxCombW);

	    weave = vsatu8 (weave, min, max); 
	}

	*L1 += src_incr;

	/* Blend weave pixel with bob pixel, depending on motion value.
	   The ratio of bob/weave will be dependend upon apparent damage
	   we expect from seeing large motion. */
	if (USE_BOB_BLEND) {
	    v16 weave_w, bob_w;
	    vu16 sum;

	    /* test Threshold, clear chroma bytes */
	    bob_w = (v16) vsubsu8 (motion, MotionThresholdW);

	    bob_w = (v16) vmullo16 (bob_w, MotionSenseW);

	    /* so the two sum to 256, weighted avg */
#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW)
	    bob_w = (v16) vminu16i ((v16) bob_w, 256);
#else
	    bob_w = vmin16 (bob_w, vsplat16_256);
#endif
	    weave_w = vsub16 (vsplat16_256, bob_w);

	    /* use more weave for less motion, more bob for large motion */
	    sum = vaddsu16 (vmullo16 (yuyv2yy (weave), weave_w),
			    vmullo16 (yuyv2yy (bob), bob_w));

	    if (Z_BYTE_ORDER == Z_LITTLE_ENDIAN) {
		/* 0xVaY1UaY0 */
		/* Div by 256 to get weighted avg and merge in
		   chroma from weave pixels. */
		weave = vor ((vu8) vsru16 (sum, 8),
			     vand (weave, (vu8) UVMask));
	    } else {
		/* 0xY0UaY1Va */
		weave = vsel ((vu8) YMask, (vu8) sum, weave);
	    }
	}

	switch (mode) {
	case STORE_WEAVE_L1:
	    vstorent (*Dest, dst_bpl, l1);
	    vstorent (*Dest, 0, weave);
	    break;

	case STORE_WEAVE_L3:
	    vstorent (*Dest, dst_bpl, l3);
	    vstorent (*Dest, 0, weave);
	    break;

	case STORE_L1WEAVE_L3WEAVE:
	    vstorent (*Dest, dst_bpl, fast_vavgu8 (l1, weave));
	    vstorent (*Dest, 0, fast_vavgu8 (l3, weave));
	    break;
	}

	*Dest += sizeof (vu8);
    }

    a1 = vzerou8 (); /* no data right of last column */

    if (count >= 0)
	goto last_column;
}

#endif /* SIMD */

/*
Local Variables:
c-basic-offset: 4
End:
 */

/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_TomsMoComp.c,v 1.1.2.3 2005-05-20 05:45:14 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 Tom Barry.  All rights reserved.
// Copyright (c) 2005 Michael H. Schimek
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.  If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
//
//  Also, this program is "Philanthropy-Ware".  That is, if you like it and 
//  feel the need to reward or inspire the author then please feel free (but
//  not obligated) to consider joining or donating to the Electronic Frontier
//  Foundation. This will help keep cyber space free of barbed wire and
//  bullsh*t. See www.eff.org for details
/////////////////////////////////////////////////////////////////////////////
// Change Log
//
// Date          Developer             Changes
//
// 04 May 2002   Tom Barry             Added TomsMoComp Deinterlace method
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.1.2.2  2005/05/17 19:58:32  mschimek
// *** empty log message ***
//
// Revision 1.1.2.1  2005/05/05 09:46:01  mschimek
// *** empty log message ***
//
// Revision 1.2  2005/04/08 15:18:57  michael
// *** empty log message ***
//
// Revision 1.1  2005/04/07 05:52:57  michael
// *** empty log message ***
//
// Revision 1.2  2005/02/05 22:18:17  mschimek
// Completed l18n.
//
// Revision 1.1  2005/01/08 14:33:23  mschimek
// TomsMoCompMethod, MoComp2Method, VideoWeaveMethod, VideoBobMethod,
// TwoFrameMethod, OldGameMethod, Greedy2FrameMethod, GreedyMethod,
// DI_GreedyHSettings: Localized.
//
// Revision 1.7  2003/06/17 12:46:29  adcockj
// Added Help for new deinterlace methods
//
// Revision 1.6  2003/03/25 10:13:10  laurentg
// Allow running of TomsMoComp SSE and 3DNOW methods with Toms'agreement
//
// Revision 1.5  2002/12/10 16:32:19  adcockj
// Fix StrangeBob for MMX
//
// Revision 1.4  2002/11/26 21:32:14  adcockj
// Made new strange bob method optional
//
// Revision 1.3  2002/07/08 18:16:43  adcockj
// final fixes fro alpha 3
//
// Revision 1.2  2002/07/08 17:44:58  adcockj
// Corrected Settings messages
//
// Revision 1.1  2002/07/07 20:07:24  trbarry
// First cut at TomsMoComp, motion compensated deinterlace
//
// Revision 1.0  2002/05/04 16:13:33  trbarry
//
///////////////////////////////////////////////////////////////////////////*/

#include "windows.h"
#include "DS_Deinterlace.h"

extern int SearchEffort2;
extern int UseStrangeBob2;

SIMD_FN_PROTOS (DEINTERLACE_FUNC, DeinterlaceTomsMoComp);

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW |			\
	    CPU_FEATURE_SSE | CPU_FEATURE_SSE2 | CPU_FEATURE_ALTIVEC)

#define USE_VERTICAL_FILTER 0
#define DONT_USE_STRANGE_BOB 0

#define DiffThres vsplatu8_15

static always_inline void
simple_bob			(uint8_t *		pDest,
				 const uint8_t *	pBob,
				 unsigned int		dst_bpl,
				 unsigned int		src_bpl)
{
    if (USE_VERTICAL_FILTER) {
	vu8 b, e, avg;

	b = vload (pBob, 0);
	e = vload (pBob, src_bpl);
	avg = fast_vavgu8 (b, e);			 /* halfway between */
	vstorent (pDest, 0, fast_vavgu8 (b, avg));	 /* 1/4 way */
	vstorent (pDest, dst_bpl, fast_vavgu8 (e, avg)); /* 3/4 way */
    } else {
	vu8 b, e;

	b = vload (pBob, 0);
	/* Copy line of previous field. */
	vstorent (pDest, 0, b);

	e = vload (pBob, src_bpl);
	/* Estimate line of this field. */
	vstorent (pDest, dst_bpl, fast_vavgu8 (b, e));
    }
}

static always_inline void
best_bob			(vu8 *			set,
				 vu8 *			pixels,
				 vu8 *			weight,
				 int			cont,
				 vu8			a,
				 vu8			b,
				 vu8			c,
				 vu8			d)
{
    vu8 diff1, diff2, mask;

    diff1 = vabsdiffu8 (a, b);
    diff2 = vabsdiffu8 (c, d);
    mask = (vu8) vandnot (vcmpleu8 (diff1, DiffThres),
			  vcmpleu8 (diff2, DiffThres));

    if (cont) {
	*set = vor (*set, mask);
	*pixels = vsel (mask, fast_vavgu8 (a, b), *pixels);
	*weight = vsel (mask, diff1, *weight);
    } else {
	*set = mask;
	*pixels = fast_vavgu8 (a, b);
        *weight = diff1;
    }
}

static always_inline void
MERGE4PIXavg			(vu8 *			pixels,
				 vu8 *			weight,
				 int			cont,
				 vu8			a,
				 vu8			b)
{
    if (cont) {
	vu8 diff, mask;

	diff = vabsdiffu8 (a, b);

#if SIMD & (CPU_FEATURE_SSE | CPU_FEATURE_SSE2 | CPU_FEATURE_ALTIVEC)
	/* AVEC: saves one, SSE: three instructions. */
	*weight = vminu8 (*weight, diff);
	mask = (vu8) vcmpeq8 (*weight, diff);
        *pixels = vsel (mask, vavgu8 (a, b), *pixels);
#else
	mask = (vu8) vcmpleu8 (diff, *weight);
        *pixels = vsel (mask, fast_vavgu8 (a, b), *pixels);
	*weight = vsel (mask, diff, *weight);
#endif
    } else {
	*weight = vabsdiffu8 (a, b);
	*pixels = fast_vavgu8 (a, b);
    }
}

static always_inline void
MERGE4PIXavgH			(vu8 *			pixels,
				 vu8 *			weight,
				 int			cont,
				 vu8			a,
				 vu8			b,
				 vu8			c,
				 vu8			d)
{
    MERGE4PIXavg (pixels, weight, cont,
		  fast_vavgu8 (a, c),
		  fast_vavgu8 (b, d));
}

/* Same average functions as above, except chroma is not considered.
   Two pair calls and unpair are faster than four regular calls
   and throwing away chroma results. */

static always_inline void
MERGE4PIXavgPair		(vu8 *			pixels,
				 vu8 *			weight,
				 int			cont,
				 vu8			a,
				 vu8			b,
				 vu8			c,
				 vu8			d)
{
#if SIMD == CPU_FEATURE_ALTIVEC
    const vu8 sel = { 0x00, 0x10, 0x02, 0x12, 0x04, 0x14, 0x06, 0x16,
		      0x08, 0x18, 0x0A, 0x1A, 0x0C, 0x1C, 0x0E, 0x1E };

    a = vec_perm (a, c, sel);
    b = vec_perm (b, d, sel);
#else
    a = vor (vand (a, vsplat16_255), vsl16 ((vu16) c, 8));
    b = vor (vand (b, vsplat16_255), vsl16 ((vu16) d, 8));
#endif

    MERGE4PIXavg (pixels, weight, cont, a, b);
}

static always_inline void
MERGE4PIXavgHPair		(vu8 *			pixels,
				 vu8 *			weight,
				 int			cont,
				 vu8			a,
				 vu8			b,
				 vu8			c,
				 vu8			d,
				 vu8			e,
				 vu8			f,
				 vu8			g,
				 vu8			h)
{
#if SIMD == CPU_FEATURE_ALTIVEC
    /* 0xY0UaY1Va */
    const vu8 sel = { 0x00, 0x10, 0x02, 0x12, 0x04, 0x14, 0x06, 0x16,
		      0x08, 0x18, 0x0A, 0x1A, 0x0C, 0x1C, 0x0E, 0x1E };

    a = vec_perm (a, e, sel);
    b = vec_perm (b, f, sel);
    c = vec_perm (c, g, sel);
    d = vec_perm (d, h, sel);
#else
    /* 0xVaY1UaY0 */
    a = vor (vand (a, vsplat16_255), vsl16 ((vu16) e, 8));
    b = vor (vand (b, vsplat16_255), vsl16 ((vu16) f, 8));
    c = vor (vand (c, vsplat16_255), vsl16 ((vu16) g, 8));
    d = vor (vand (d, vsplat16_255), vsl16 ((vu16) h, 8));
#endif

    MERGE4PIXavgH (pixels, weight, cont, a, b, c, d);
}

static always_inline void
Unpair				(vu8 *			pixels,
				 vu8 *			weight)
{
    vu8 t, mask;

#if SIMD == CPU_FEATURE_ALTIVEC
    /* 0xY0YaY1Yb */
    t = (vu8) vsl16 ((v16) *weight, 8);
    /* Saves one instruction over vcmpleu because AltiVec
       has no integer cmple. */
    *weight = vminu8 (*weight, t);
    mask = (vu8) vcmpeq8 (*weight, t);
    *weight = vor (*weight, (vu8) vsplat16_255);
    *pixels = vsel (mask, (vu8) vsl16 ((v16) *pixels, 8), *pixels);
#elif SIMD & (CPU_FEATURE_SSE | CPU_FEATURE_SSE2)
    /* 0xYbY1YaY0 */
    t = (vu8) vsru16 ((vu16) *weight, 8);
    /* Saves three instructions over a second vsel. */
    *weight = vminu8 (*weight, t);
    mask = vcmpeq8 (*weight, t);
    *weight = vor (*weight, (vu8) vsplat16_255);
    *pixels = vsel (mask, (vu8) vsru16 ((vu16) *pixels, 8), *pixels);
#else
    /* 0xYbY1YaY0 */
    t = (vu8) vsru16 ((vu16) *weight, 8);
    mask = (vu8) vcmpleu8 (t, *weight);
    *pixels = vsel (mask, (vu8) vsru16 ((vu16) *pixels, 8), *pixels);
    /* The vor masks out odd chroma. */
    *weight = vor (vsel (mask, t, *weight), vsplat16_m256);
#endif
}

#define SearchLoopOddA2(cont)						\
    uload2t (&cl1, &cc1, &cr1, pSrcP, src_bpl);				\
    uload2t (&cl2, &cc2, &cr2, pSrc, src_bpl);				\
    MERGE4PIXavgPair (&weave, &weave_d, cont,				\
		      cl1, cr2,						\
		      cr1, cl2);

#define SearchLoopOddA(cont)						\
    uload2t (&ul1, &uc1, &ur1, pSrcP, 0);				\
    uload2t (&dl2, &dc2, &dr2, pSrc, src_bpl * 2);			\
    MERGE4PIXavgPair (&weave, &weave_d, cont, ul1, dr2, ur1, dl2);	\
    uload2t (&dl1, &dc1, &dr1, pSrcP, src_bpl * 2);			\
    uload2t (&ul2, &uc2, &ur2, pSrc, 0);				\
    MERGE4PIXavgPair (&weave, &weave_d, 1, dl1, ur2, dr1, ul2);

#define SearchLoopOddAH2(cont)						\
    uload2t (&cl1, &cc1, &cr1, pSrcP, src_bpl);				\
    uload2t (&cl2, &cc2, &cr2, pSrc, src_bpl);				\
    MERGE4PIXavgHPair (&weave, &weave_d, cont,				\
		       cl1, cc1, cc2, cr2,				\
		       cr1, cc1, cc2, cl2);

/* 4 odd v half pels, 3 to left & right. */
#define SearchLoopOddA6(cont)						\
    uload6t (&ul1, &uc1, &ur1, pSrcP, 0);				\
    uload6t (&dl2, &dc2, &dr2, pSrc, src_bpl * 2);			\
    MERGE4PIXavgPair (&weave, &weave_d, cont, ul1, dr2, ur1, dl2);	\
    uload6t (&dl1, &dc1, &dr1, pSrcP, src_bpl * 2);			\
    uload6t (&ul2, &uc2, &ur2, pSrc, 0);				\
    MERGE4PIXavgPair (&weave, &weave_d, 1, dl1, ur2, dr1, ul2);		\
    uload6t (&cl1, &cc1, &cr1, pSrcP, src_bpl);				\
    uload6t (&cl2, &cc2, &cr2, pSrc, src_bpl);				\
    MERGE4PIXavgPair (&weave, &weave_d, 1, cl1, cr2, cr1, cl2);

/* Search averages of 2 pixels left and right. */
#define SearchLoopEdgeA(cont)						\
    uload4t (&ul1, &uc1, &ur1, pSrcP, 0);				\
    uload4t (&dl2, &dc2, &dr2, pSrc, src_bpl * 2);			\
    MERGE4PIXavg (&weave, &weave_d, cont, ul1, dr2);			\
    MERGE4PIXavg (&weave, &weave_d, 1, ur1, dl2);			\
    uload4t (&dl1, &dc1, &dr1, pSrcP, src_bpl * 2);			\
    uload4t (&ul2, &uc2, &ur2, pSrc, 0);				\
    MERGE4PIXavg (&weave, &weave_d, 1, dl1, ur2);			\
    MERGE4PIXavg (&weave, &weave_d, 1, dr1, ul2);			\
    uload4t (&cl1, &cc1, &cr1, pSrcP, src_bpl);				\
    uload4t (&cl2, &cc2, &cr2, pSrc, src_bpl);				\
    MERGE4PIXavg (&weave, &weave_d, 1, cl1, cr2);			\
    MERGE4PIXavg (&weave, &weave_d, 1, cr1, cl2);

/* Search vertical line and averages, -1,0,+1 */
/* MHS: In principle we could move invocations of this macro after OddA
   to save the loads, but I'm not sure the order was intentional to give
   vertical lines an advantage. Reloading may also avoid register spills. */
#define SearchLoopVA(cont)						\
    dc1 = vload (pSrcP, 2 * src_bpl);					\
    uc2 = vload (pSrc, 0);						\
    MERGE4PIXavg (&weave, &weave_d, cont, dc1, uc2);			\
    uc1 = vload (pSrcP, 0);						\
    dc2 = vload (pSrc, src_bpl * 2);					\
    MERGE4PIXavg (&weave, &weave_d, 1, uc1, dc2);

static always_inline BOOL
Search_Effort_template		(TDeinterlaceInfo *	pInfo,
				 const int		effort,
				 const int		use_strange_bob)
{
    uint8_t *pDest;
    const uint8_t *pSrc;
    const uint8_t *pSrcP;
    const uint8_t *pBob;
    const uint8_t *pBobP;
    unsigned int byte_width;
    unsigned int height;
    unsigned int dst_bpl;
    unsigned int src_bpl;
    unsigned int dst_padding;
    unsigned int src_padding1;
    unsigned int src_padding2;
       
    byte_width = pInfo->LineLength;
    height = pInfo->FieldHeight;

    dst_bpl = pInfo->OverlayPitch;
    src_bpl = pInfo->InputPitch;

    /*
      dest  src odd                          src even
      0     F3-0   copy F3-0                 F4-0  wcopy F3-0
      1	    F4-0  wcopy F3-0                 F3-0   copy F3-0      
      2     F3-1   copy F3-1                 F4-1  weave F1/3-0/1,F2/4-0/1/2
      3     F4-1  weave F1/3-1/2,F2/4-0/1/2  F3-1   copy F3-1
      4     F3-2   copy F3-2                 F4-2  weave F1/3-1/2,F2/4-1/2/3
      5     F4-2  weave F1/3-2/3,F2/4-1/2/3
      n-3				     F3     copy F3-(n/2-2)
      n-2   F3     copy F3-(n/2-1)           F4    wcopy F3-(n/2-1)
      n-1   F4    wcopy F3-(n/2-1)           F3     copy F3-(n/2-1)

      dest  src filter odd                   src filter even
      0     F3-0   copy F3-0                 F4-0  wcopy F3-0
      1	    F4-0  wcopy F3-0                 F3-0  weave F3-0/1      
      2     F3-1  weave F3-1/2               F4-1  weave F3-0/1
      3     F4-1  weave F3-1/2               F3-1  weave F3-1/2
      4     F3-2  weave F3-2/3               F4-2  weave F3-1/2
      5     F4-2  weave F3-2/3               
      n-3                                    F3    copy F3-(n/2-2)
      n-2   F3     copy F3-(n/2-1)           F4    wcopy F3-(n/2-1)
      n-1   F4    wcopy F3-(n/2-1)           F3    copy F3-(n/2-1)

      pDest  2     1     2     1
      pSrc   F4-0  F4-0  F4-0  F4-0 this field (-1, 0, +1)
      pBob   F3-1  F3-0  F3-1  F3-0 previous field, opposite parity (0, +1)
      pSrcP  F2-0  F2-0  F2-0  F2-0 previous field, same parity (-1, 0, +1)
      pBobP  F1-1  F1-0  F1-1  F1-0 previous field, opposite parity (0, +1)
    */

    pDest = (uint8_t *) pInfo->Overlay;

    /* pSrc, pSrcP point one field line above so we can
       easier calculate the address of current line -1, +0, +1 * bpl. */
    pSrc  = (const uint8_t *) pInfo->PictureHistory[0]->pData;
    pSrcP = (const uint8_t *) pInfo->PictureHistory[2]->pData;

    pBob  = (const uint8_t *) pInfo->PictureHistory[1]->pData;
    pBobP = (const uint8_t *) pInfo->PictureHistory[3]->pData;

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
	/* Copy first even and odd lines from first line of previous
	   field.  The loop copies the second and following pairs. */
	copy_line_pair (pDest, pBob, byte_width, dst_bpl);

	pDest += dst_bpl * 2;

	pBob  += src_bpl;
	pBobP += src_bpl;
    } else {
	/* Copy first even line, the loop starts in first odd line. */
	copy_line (pDest, pBob, byte_width);

	pDest += dst_bpl;
    }

    /* We write two frame lines in parallel, incrementing pDest
       except after the last column. */
    dst_padding = dst_bpl * 2 - byte_width + sizeof (vu8);

    /* We read one field line, incrementing the pointer
       except after the last column. */
    src_padding1 = src_bpl - byte_width + sizeof (vu8);

    /* We read one field line and skip over first and last column. */
    src_padding2 = src_padding1 + sizeof (vu8);

    pSrc += sizeof (vu8);  /* no access in first column */
    pSrcP += sizeof (vu8);
    pBobP += sizeof (vu8);

    /* All but first and last field line where we cannot read the
       line above and below. */
    for (height -= 2; height > 0; --height) {
	unsigned int count;

	/* Simple bob first column. */
	simple_bob (pDest, pBob, dst_bpl, src_bpl);

	pBob += sizeof (vu8);
	pDest += sizeof (vu8);

	/* All but first and last column where we cannot read the
	   pixels left and right. */
	for (count = byte_width / sizeof (vu8) - 2; count > 0; --count) {
	    vu8 ul1, uc1, ur1, cl1, cc1, cr1, dl1, dc1, dr1;
	    vu8 ul2, uc2, ur2, cl2, cc2, cr2, dl2, dc2, dr2;
	    vu8 bob, bob_d;
	    vu8 weave, weave_d;
	    vu8 Min_Vals, Max_Vals;
	    vu8 mm0, mm1;

	    /* First, get and save our possible Bob values.  Assume our
	       pixels are layed out as follows with x the calc'd bob
	       value and the other pixels are from the current field
	       j a b c k		current field
	           x			calculated line
	       m d e f n		current field
	    */

	    if (use_strange_bob) {
		vu8 a, b, c, d, e, f, j, k, m, n;
		vu8 set, diff_be, avg_be, mask, mm3, mm4;

		       /* -4  -2   0  +2  +4 +pBob +0 */
		uload24t (&j, &a, &b, &c, &k, pBob, 0);
		uload24t (&m, &d, &e, &f, &n, pBob, src_bpl);

		/* we calc the bob value luma value as:
		   if |j - n| < Thres && |a - m| > Thres
		     avg(j,n)
		   if |k - m| < Thres && |c - n| > Thres 
		     avg(k,m)
		   if |c - d| < Thres && |b - f| > Thres 
		     avg(c,d)
		   if |a - f| < Thres && |b - d| > Thres 
		     avg(a,f)
		   if |b - e| < Thres
		     avg(b,e)
		   pickup any thing not yet set with avg(b,e)
		*/

		best_bob (&set, &bob, &bob_d, 0, j, n, a, m);
		best_bob (&set, &bob, &bob_d, 1, k, m, c, n);
		best_bob (&set, &bob, &bob_d, 1, c, d, b, f);
		best_bob (&set, &bob, &bob_d, 1, a, f, b, d);

		/* mask out odd chroma */
		set = (vu8) yuyv2yy (set);
		bob = (vu8) yuyv2yy (bob);
		bob_d = (vu8) yuyv2yy (bob_d);

		diff_be = vabsdiffu8 (b, e);
		avg_be = fast_vavgu8 (b, e);
		mask = (vu8) vcmpleu8 (diff_be, DiffThres);
		set = vor (set, mask);
		bob = vsel (mask, avg_be, bob);
		bob_d = vsel (mask, diff_be, bob_d);

		/* bob in any leftovers */

		/* We will also calc here the max/min values to later
		   limit comb so the max excursion will not exceed the
		   Max_Comb constant */

		if (0 == effort) {
		    vu8 min, max;

		    vminmaxu8 (&min, &max, b, e);
		    bob = vsatu8 (bob, min, max);
		} else {
		    vu8 min, max, low_motion;

		    mm4 = vload (pBobP, 0);
		    mm3 = vload (pBobP, src_bpl);

		    mm4 = vabsdiffu8 (b, mm4);
		    mm3 = vabsdiffu8 (e, mm3);

		    /* top or bottom pixel moved most */
		    mm3 = vmaxu8 (mm3, mm4);
		    low_motion = (vu8) vcmpleu8 (mm3, DiffThres);

		    vminmaxu8 (&min, &max, b, e);
		    bob = vsatu8 (bob, min, max);

		    /* saturate if no surround motion */
		    Min_Vals = vsubsu8 (min, low_motion);
		    Max_Vals = vaddsu8 (max, low_motion);
		}

		mask = vor ((vu8) vcmpleu8 (diff_be, bob_d), vnot (set));
		bob = vsel (mask, avg_be, bob);
		bob_d = vsel (mask, diff_be, bob_d);
	    } else {
		vu8 a, b, c, d, e, f, j, k, m, n;

		/* WeirdBob */

		/* we calc the bob value as:
		   x2 = either avg(a,f), avg(c,d), avg(b,e), avg(j,n),
		   or avg(k,m) selected for the smallest of abs(a-f),
		   abs(c-d), abs(b-e), etc. */

		uload24t (&j, &a, &b, &c, &k, pBob, 0);
		uload24t (&m, &d, &e, &f, &n, pBob, src_bpl);

		MERGE4PIXavg (&bob, &bob_d, 0, a, f);
		MERGE4PIXavg (&bob, &bob_d, 1, c, d);

		/* we know chroma is worthless so far */
		bob_d = vor (bob_d, (vu8) UVMask);

		MERGE4PIXavg (&bob, &bob_d, 1, j, n);
		MERGE4PIXavg (&bob, &bob_d, 1, k, m);

		/* We will also calc here the max/min values to later
		   limit comb so the max excursion will not exceed
		   the Max_Comb constant */

		if (0 == effort) {
		    vu8 min, max;

		    vminmaxu8 (&min, &max, b, e);
		    bob = vsatu8 (bob, min, max);
		} else {
		    vu8 min, max, low_motion, mm3, mm4;

		    mm4 = vload (pBobP, 0);
		    mm3 = vload (pBobP, src_bpl);

		    mm4 = vabsdiffu8 (b, mm4);
		    mm3 = vabsdiffu8 (e, mm3);

		    /* top or bottom pixel moved most */
		    mm3 = vmaxu8 (mm3, mm4);
		    low_motion = (vu8) vcmpleu8 (mm3, DiffThres);

		    vminmaxu8 (&min, &max, b, e);
		    bob = vsatu8 (bob, min, max);

		    /* saturate if no surround motion */
		    Min_Vals = vsubsu8 (min, low_motion);
		    Max_Vals = vaddsu8 (max, low_motion);
		}

		MERGE4PIXavg (&bob, &bob_d, 1, b, e);
	    }

	    /* We will keep a slight bias to using the weave pixels
	       from the current location, by rating them by the min
	       distance from the Bob value instead of the avg distance
	       from that value. our best and only rating so far */

	    switch (effort) {
	    case 0:
		break;

	    case 1:
		cc1 = vload (pSrcP, src_bpl);
		cc2 = vload (pSrc, src_bpl);

		MERGE4PIXavg (&weave, &weave_d, /* cont */ FALSE, cc1, cc2);

		break;

	    case 2 ... 3:
		SearchLoopOddA2 (0);

		Unpair (&weave, &weave_d);

		/* for SearchLoop0A.inc cc1, cc2 from OddA2 */

		break;

	    case 4 ... 5:
		SearchLoopOddA2 (0);

		/* SearchLoopOddAH2.inc (cl,cc,cr left over from OddA2) */
		MERGE4PIXavgHPair (&weave, &weave_d, 1,
				   cl1, cc1, cc2, cr2,
				   cr1, cc1, cc2, cl2);

		Unpair (&weave, &weave_d);

		break;

	    case 6 ... 9: /* 3x3 search */
		SearchLoopOddA (0);

		Unpair (&weave, &weave_d);

		/* SearchLoopVA.inc (uc, dc left over from OddA) */
		MERGE4PIXavg (&weave, &weave_d, 1, dc1, uc2);
		MERGE4PIXavg (&weave, &weave_d, 1, uc1, dc2);

		/* for SearchLoop0A.inc */
		cc1 = vload (pSrcP, src_bpl);
		cc2 = vload (pSrc, src_bpl);

		break;

	    case 10 ... 11: /* Search 9 with 2 H-half pels added */
		/* MHS: Keep this order.  Not ideal for register allocation
		   but that's how the original code did it.  Same below. */
		SearchLoopOddA (0);
		SearchLoopOddAH2 (1);

		Unpair (&weave, &weave_d);

		/* SearchLoopVA.inc (uc, dc left over from OddA) */
		MERGE4PIXavg (&weave, &weave_d, 1, dc1, uc2);
		MERGE4PIXavg (&weave, &weave_d, 1, uc1, dc2);

		break;

	    case 12 ... 13: /* Search 11 with 2 V-half pels added */
		SearchLoopOddA (0);
		SearchLoopOddAH2 (1);

		Unpair (&weave, &weave_d);

		/* SearchLoopVAH.inc (uc, dc left over from OddA,
		   cc from OddAH2) */
		MERGE4PIXavgH (&weave, &weave_d, 1, dc1, cc1, cc2, uc2);
		MERGE4PIXavgH (&weave, &weave_d, 1, uc1, cc1, cc2, dc2);

		/* SearchLoopVA.inc */
		MERGE4PIXavg (&weave, &weave_d, 1, dc1, uc2);
		MERGE4PIXavg (&weave, &weave_d, 1, uc1, dc2);

		break;

	    case 14 ... 15: /* 5x3 */
		SearchLoopOddA (0);

		Unpair (&weave, &weave_d);

		SearchLoopEdgeA (1);

		SearchLoopVA (1);

		break;

	    case 16 ... 19: /* 5x3 + 4 half pels */
		SearchLoopOddAH2 (0);
		SearchLoopOddA (1);

		Unpair (&weave, &weave_d);

		/* SearchLoopVAH.inc (uc, dc left over from OddA,
		   cc from OddAH2) */
		MERGE4PIXavgH (&weave, &weave_d, 1, dc1, cc1, cc2, uc2);
		MERGE4PIXavgH (&weave, &weave_d, 1, uc1, cc1, cc2, dc2);

		/* SearchLoopVA.inc */
		/* MHS: Keep order. */
		MERGE4PIXavg (&weave, &weave_d, 1, dc1, uc2);
		MERGE4PIXavg (&weave, &weave_d, 1, uc1, dc2);

		break;

	    case 20 ... 21: /* Search a 7x3 area, no half pels */
		SearchLoopOddA6 (0);
		SearchLoopOddA (1);

		Unpair (&weave, &weave_d);

		SearchLoopEdgeA (1);

		SearchLoopVA (1);

		break;

	    default: /* Search a 9x3 area, no half pels */
	        /* odd addresses -- use only luma data */

		SearchLoopOddA6 (0);
		SearchLoopOddA (1);

		Unpair (&weave, &weave_d);

		/* even addresses -- use both luma and chroma from these */

		/* SearchLoopEdgeA8.inc */
		/* search averages of 4 pixels left and right */

		uload8t (&ul1, &uc1, &ur1, pSrcP, 0);
		uload8t (&dl2, &dc2, &dr2, pSrc, src_bpl * 2);
		MERGE4PIXavg (&weave, &weave_d, 1, ul1, dr2);
		MERGE4PIXavg (&weave, &weave_d, 1, ur1, dl2);

		uload8t (&ul2, &uc2, &ur2, pSrc, 0);
		uload8t (&dl1, &dc1, &dr1, pSrcP, src_bpl * 2);
		MERGE4PIXavg (&weave, &weave_d, 1, dl1, ur2);
		MERGE4PIXavg (&weave, &weave_d, 1, dr1, ul2);

		uload8t (&cl1, &cc1, &cr1, pSrcP, src_bpl);
		uload8t (&cl2, &cc2, &cr2, pSrc, src_bpl);
		MERGE4PIXavg (&weave, &weave_d, 1, cl1, cr2);
		MERGE4PIXavg (&weave, &weave_d, 1, cr1, cl2);

		SearchLoopEdgeA (1);

		SearchLoopVA (1);

		break;
	    }

	    if (0 == effort) {
	        mm0 = bob; /* just use the results of our weird bob */
	    } else {
		if (effort > 1) {
		    /* SearchLoop0A.inc - center in old and new */

		    /* bias toward no motion */
		    weave_d = vaddsu8 (weave_d, vsplatu8_1);
		    MERGE4PIXavg (&weave, &weave_d, /* cont */ TRUE, cc1, cc2);
		}

		/* JA 9/Dec/2002 failed experiment
		   but leave in placeholder for me to play about */
		if (DONT_USE_STRANGE_BOB) {
		    /* Use the best weave if diffs less than 10 as that
		       means the image is still or moving cleanly
		       if there is motion we will clip which will catch
		       anything */
		    mm0 = (vu8) vcmpleu8 (weave_d, vsplatu8i (4));
		    mm0 = vsel (mm0, weave, bob); // use weave, else bob
		} else {   
		    /* Use the better of bob or weave
		       10 is the most we care about */
		    bob_d = vminu8 (bob_d, vsplatu8i (10));
		    /* foregive that much from weave est? */
		    mm0 = vsubsu8 (weave_d, bob_d);
		    mm0 = (vu8) vcmpleu8 (mm0, vsplatu8i (4));
		    mm0 = vsel (mm0, weave, bob); /* use weave, else bob */
		}

		/* clip to catch the stray error */
		mm0 = vsatu8 (mm0, Min_Vals, Max_Vals);
	    }

	    if (USE_VERTICAL_FILTER) {
		mm1 = vload (pBob, 0);
		vstorent (pDest, 0, fast_vavgu8 (mm0, mm1));
		mm1 = vload (pBob, src_bpl);
		vstorent (pDest, dst_bpl, fast_vavgu8 (mm0, mm1));
	    } else {
		mm1 = vload (pBob, 0);
		/* Copy line of previous field. */
		vstorent (pDest, 0, mm1);
		/* Our value for the line of this field. */
		vstorent (pDest, dst_bpl, mm0);
	    }

	    pSrc += sizeof (vu8);
	    pSrcP += sizeof (vu8);
	    pBob += sizeof (vu8);
	    pBobP += sizeof (vu8);
	    pDest += sizeof (vu8);
	}

	pSrc += src_padding2;
	pSrcP += src_padding2;

	/* Simple bob last column. */
	simple_bob (pDest, pBob, dst_bpl, src_bpl);

	pBob += src_padding1;
	pBobP += src_padding2;

	pDest += dst_padding;
    }

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_EVEN) {
	/* Copy but-last odd dest line which isn't covered by the loop. */
	copy_line (pDest, pBob, byte_width);

	pBob += src_bpl;
	pDest += dst_bpl;
    }

    /* Copy the last even and odd dest line from
       the last line of the previous field. */
    copy_line_pair (pDest, pBob, byte_width, dst_bpl);

    vempty ();

    return TRUE;
}

typedef BOOL
Search_Effort_fn		(TDeinterlaceInfo *	pInfo);

#define Search_Effort_(effort)						\
static BOOL								\
Search_Effort_ ## effort	(TDeinterlaceInfo *	pInfo)		\
{									\
    return Search_Effort_template (pInfo, effort,			\
				   /* use_strange_bob */ FALSE);	\
}									\
static BOOL								\
Search_Effort_ ## effort ## _SB	(TDeinterlaceInfo *	pInfo)		\
{									\
    return Search_Effort_template (pInfo, effort,			\
				   /* use_strange_bob */ TRUE);		\
}

Search_Effort_ (0)
Search_Effort_ (1)
Search_Effort_ (3)
Search_Effort_ (5)
Search_Effort_ (9)
Search_Effort_ (11)
Search_Effort_ (13)
Search_Effort_ (15)
Search_Effort_ (19)
Search_Effort_ (21)
Search_Effort_ (27)

static Search_Effort_fn *
Search_Effort_table [23][2] = {
    { Search_Effort_0, Search_Effort_0_SB },
    { Search_Effort_1, Search_Effort_1_SB },
    { Search_Effort_3, Search_Effort_3_SB },
    { Search_Effort_3, Search_Effort_3_SB },
    { Search_Effort_5, Search_Effort_5_SB },
    { Search_Effort_5, Search_Effort_5_SB },
    { Search_Effort_9, Search_Effort_9_SB },
    { Search_Effort_9, Search_Effort_9_SB },
    { Search_Effort_9, Search_Effort_9_SB },
    { Search_Effort_9, Search_Effort_9_SB },
    { Search_Effort_11, Search_Effort_11_SB },
    { Search_Effort_11, Search_Effort_11_SB },
    { Search_Effort_13, Search_Effort_13_SB },
    { Search_Effort_13, Search_Effort_13_SB },
    { Search_Effort_15, Search_Effort_15_SB },
    { Search_Effort_15, Search_Effort_15_SB },
    { Search_Effort_19, Search_Effort_19_SB },
    { Search_Effort_19, Search_Effort_19_SB },
    { Search_Effort_19, Search_Effort_19_SB },
    { Search_Effort_19, Search_Effort_19_SB },
    { Search_Effort_21, Search_Effort_21_SB },
    { Search_Effort_21, Search_Effort_21_SB },
    { Search_Effort_27, Search_Effort_27_SB },
};

BOOL
SIMD_NAME (DeinterlaceTomsMoComp) (TDeinterlaceInfo *pInfo)
{
    unsigned int effort;

    if (SIMD == CPU_FEATURE_SSE2) {
	if ((INTPTR (pInfo->Overlay) |
	     INTPTR (pInfo->PictureHistory[0]->pData) |
	     INTPTR (pInfo->PictureHistory[1]->pData) |
	     INTPTR (pInfo->PictureHistory[2]->pData) |
	     INTPTR (pInfo->PictureHistory[3]->pData) |
	     (unsigned int) pInfo->OverlayPitch |
	     (unsigned int) pInfo->InputPitch |
	     (unsigned int) pInfo->LineLength) & 15)
	    return DeinterlaceTomsMoComp_SSE (pInfo);
    }

    effort = MIN ((unsigned int) SearchEffort2,
		  (unsigned int) N_ELEMENTS (Search_Effort_table) - 1);
    return Search_Effort_table[effort][!!UseStrangeBob2](pInfo);
}

#elif !SIMD

int SearchEffort2 = 3;
int UseStrangeBob2 = FALSE;

/*//////////////////////////////////////////////////////////////////////////
// Start of Settings related code
//////////////////////////////////////////////////////////////////////////*/
SETTING DI_TOMSMOCOMPSETTINGS[DI_TOMSMOCOMP_SETTING_LASTONE] =
{
    {
        N_("Search Effort"), SLIDER, 0, &SearchEffort2,
        5, 0, 255, 1, 1,
        NULL,
        "Deinterlace", "SearchEffort", NULL,
    },
    {
        N_("Use Strange Bob"), YESNO, 0, &UseStrangeBob2,
        0, 0, 1, 1, 1,
        NULL,
        "Deinterlace", "UseStrangeBob", NULL,
    },
};

const DEINTERLACE_METHOD TomsMoCompMethod =
{
    sizeof(DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    N_("Video (TomsMoComp)"), 
    "TomsMoComp",
    FALSE, 
    FALSE, 
    /* pfnAlgorithm */ NULL,
    50, 
    60,
    DI_TOMSMOCOMP_SETTING_LASTONE,
    DI_TOMSMOCOMPSETTINGS,
    INDEX_VIDEO_TOMSMOCOMP,
    NULL,
    NULL,
    NULL,
    NULL,
    4,	/* number fields needed */
    0,
    0,
    WM_DI_TOMSMOCOMP_GETVALUE - WM_APP,
    NULL,
    0,
    FALSE,
    FALSE,
    IDH_TOMSMOCOMP,
};

DEINTERLACE_METHOD *
DI_TomsMoComp_GetDeinterlacePluginInfo (void)
{
    DEINTERLACE_METHOD *m;

    m = malloc (sizeof (*m));
    *m = TomsMoCompMethod;

    m->pfnAlgorithm =
	SIMD_FN_SELECT (DeinterlaceTomsMoComp,
			CPU_FEATURE_MMX | CPU_FEATURE_3DNOW |
			CPU_FEATURE_SSE | CPU_FEATURE_SSE2 |
			CPU_FEATURE_ALTIVEC);

    return m;
}

#endif /* !SIMD */

/*
Local Variables:
c-basic-offset: 4
End:
 */

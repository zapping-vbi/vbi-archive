/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_MoComp2.c,v 1.3 2005-06-28 19:17:10 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2003 Tom Barry & John Adcock.  All rights reserved.
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
//  (From Tom Barry)
//  Also, this program is "Philanthropy-Ware".  That is, if you like it and 
//  feel the need to reward or inspire the author then please feel free (but
//  not obligated) to consider joining or donating to the Electronic Frontier
//  Foundation. This will help keep cyber space free of barbed wire and
//  bullsh*t.  See www.eff.org for details
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.2  2005/06/28 00:48:07  mschimek
// New file integrating all the code formerly in
// plugins/deinterlace/DI_MoComp2. Converted the MMX inline asm to vector
// intrinsics. Added support for 3DNow, SSE2, x86-64 and AltiVec. Cleaned
// up.
//
// Revision 1.1.2.5  2005/06/17 02:54:20  mschimek
// *** empty log message ***
//
// Revision 1.1.2.4  2005/05/31 02:40:34  mschimek
// *** empty log message ***
//
// Revision 1.1.2.3  2005/05/20 05:45:14  mschimek
// *** empty log message ***
//
// Revision 1.1.2.2  2005/05/17 19:58:32  mschimek
// *** empty log message ***
//
// Revision 1.1.2.1  2005/05/05 09:46:01  mschimek
// *** empty log message ***
//
// Revision 1.3  2005/02/05 22:18:27  mschimek
// Completed l18n.
//
// Revision 1.2  2005/01/31 07:03:38  mschimek
// Don\'t define size_t or we run into conflicts with system headers.
//
// Revision 1.1  2005/01/08 14:33:33  mschimek
// TomsMoCompMethod, MoComp2Method, VideoWeaveMethod, VideoBobMethod,
// TwoFrameMethod, OldGameMethod, Greedy2FrameMethod, GreedyMethod,
// DI_GreedyHSettings: Localized.
//
// Revision 1.3  2003/06/17 12:46:28  adcockj
// Added Help for new deinterlace methods
//
// Revision 1.2  2003/03/05 13:55:20  adcockj
// Allow SSE optimizations
//
// Revision 1.1  2003/01/02 13:15:00  adcockj
// Added new plug-ins ready for developement by copying TomsMoComp and Gamma
//
//
// Log: DI_TomsMoComp.c,v
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
#include <DS_Deinterlace.h>

SIMD_FN_PROTOS (DEINTERLACE_FUNC, DeinterlaceMoComp2);

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW |			\
	    CPU_FEATURE_SSE | CPU_FEATURE_SSE2 | CPU_FEATURE_ALTIVEC)

static void
simple_bob			(uint8_t *		pDest,
				 const uint8_t *	pBob,
				 unsigned long		dst_bpl,
				 unsigned long		src_bpl)
{
    vu8 b, e;

    b = vload (pBob, 0);
    /* Copy line of previous field. */
    vstorent (pDest, 0, b);

    e = vload (pBob, src_bpl);
    /* Estimate line of this field. */
    vstorent (pDest, dst_bpl, fast_vavgu8 (b, e));
}

BOOL
SIMD_NAME (DeinterlaceMoComp2)	(TDeinterlaceInfo *pInfo)
{
    uint8_t *Dest;
    const uint8_t *F4;
    const uint8_t *F3;
    const uint8_t *F2;
    const uint8_t *F1;
    unsigned int byte_width;
    unsigned int height;
    unsigned long dst_bpl;
    unsigned long src_bpl;
    unsigned long dst_padding;
    unsigned long src_padding1;
    unsigned long src_padding2;

    if (SIMD == CPU_FEATURE_SSE2) {
	if (((unsigned long) pInfo->Overlay |
	     (unsigned long) pInfo->PictureHistory[0]->pData |
	     (unsigned long) pInfo->PictureHistory[1]->pData |
	     (unsigned long) pInfo->PictureHistory[2]->pData |
	     (unsigned long) pInfo->PictureHistory[3]->pData |
	     (unsigned long) pInfo->OverlayPitch |
	     (unsigned long)  pInfo->InputPitch |
	     (unsigned long) pInfo->LineLength) & 15)
	    return DeinterlaceMoComp2_SSE (pInfo);
    }

    byte_width = pInfo->LineLength;

    dst_bpl = pInfo->OverlayPitch;
    src_bpl = pInfo->InputPitch;

    /*
	dest	src (F3) odd				src (F3) even
	0	F2.0 (F3.0*)				F3.0
	1     { F3.0					F2.0 (F3.0*)
	2     { weave.0 (F1|3.n|n+1 + F2.n+1 + F4.n+1)	F3.1		}
	3       F3.1		weave.1 (F1|3.n|n+1 + F2.n + F4.n)	}
	4	weave.1					F3.2
	h-3	F3.h/2-2				weave.h/2-2	
	h-2	F2.h/2-1 (F3.h/2-1*)			F3.h/2-1
	h-1	F3.h/2-1				F2.h/2-1 (F3.h/2-1*)

	F4	next field, opposite parity
	F3	this field
	F2	previous field, opposite parity
	F1	previous field, same parity
	h	frame height

	(*)	Actual data stored here. Why? Ask Tom. :-) 
    */

    Dest = (uint8_t *) pInfo->Overlay;

    F4 = (const uint8_t *) pInfo->PictureHistory[0]->pData;
    F3 = (const uint8_t *) pInfo->PictureHistory[1]->pData;
    F2 = (const uint8_t *) pInfo->PictureHistory[2]->pData;
    F1 = (const uint8_t *) pInfo->PictureHistory[3]->pData;

    if (pInfo->PictureHistory[1]->Flags & PICTURE_INTERLACED_ODD) {
	copy_line (Dest, F3, byte_width);
	Dest += dst_bpl;
    } else {
	copy_line_pair (Dest, F3, byte_width, dst_bpl);
	Dest += dst_bpl * 2;

	F3 += src_bpl;
	F1 += src_bpl;
    }

    /* We start in field row 1, no access in first column. */
    F4 += src_bpl + sizeof (vu8);
    F2 += src_bpl + sizeof (vu8);
    F1 += sizeof (vu8);

    /* We write two frame lines in parallel, incrementing Dest
       except after the last column. */
    dst_padding = dst_bpl * 2 - byte_width + sizeof (vu8);

    /* We read one field line, incrementing the pointer
       except after the last column. */
    src_padding1 = src_bpl - byte_width + sizeof (vu8);

    /* We read one field line and skip over first and last column. */
    src_padding2 = src_padding1 + sizeof (vu8);

    /* All but first and last field line where we cannot read the
       line above and below. */
    for (height = pInfo->FieldHeight - 2; height > 0; --height) {
	vu8 a, b, c, d, e, f;
	unsigned int count;

	/* Simple bob first column. */
	simple_bob (Dest, F3, dst_bpl, src_bpl);

	F3 += sizeof (vu8);
	Dest += sizeof (vu8);

	/* All but first and last column where we cannot read the
	   pixels left and right. */
	for (count = byte_width / sizeof (vu8) - 2; count > 0; --count) {
	    vu8 diff_af, diff_cd, diff_be;
	    vu8 avg_af, avg_cd, avg_be;
	    vu8 best, diag, bob, weave;
	    vu8 mm1, mm2, mm4, mm6, next, prev, above, below;
	    v32 tm, cm, bm;

	    /* StrangeBob */

	    /*///////////////////////////////////////////////////////////////
	    // This implements the algorithm described in:
	    // T. Chen, H.R. Wu; & Z.H. Yu, 
	    // "Efficient deinterlacing algorithm using edge-based line
	    // average interpolation", 
	    // Optical Engineering, Volume 39, Issue 8, (2000)
	    ///////////////////////////////////////////////////////////////*/

	    /* First, get and save our possible Bob values.
	       Assume our pixels are layed out as follows with x the
	       calc'd bob value and the other pixels are from the
	       current field.
	       a b c 	current field   (F3.n)
	         x	calculated line (F2.n)
	       d e f 	current field   (F3.n+1)
	    */
	    /* XXX could calculate a, d from b, e of previous loop
	       and integrate first 8/16 bytes into the loop. */
	    uload2t (&a, &b, &c, F3, 0);
	    uload2t (&d, &e, &f, F3, src_bpl);
	    F3 += sizeof (vu8);

	    diff_af = vabsdiffu8 (a, f);
	    avg_af = fast_vavgu8 (a, f);

	    diff_cd = vabsdiffu8 (c, d);
	    avg_cd = fast_vavgu8 (c, d);

	    /* Copy line of previous field. */
	    vstorent (Dest, 0, b);

	    mm4 = fast_vavgu8 (vabsdiffu8 (b, d), vabsdiffu8 (c, e));
	    mm6 = fast_vavgu8 (vabsdiffu8 (a, e), vabsdiffu8 (b, f));
	    mm1 = (vu8) vcmpleu8 (mm6, mm4);
	    best = vsel (mm1, diff_af, diff_cd);
	    diag = vsel (mm1, avg_af, avg_cd);

	    diff_be = vabsdiffu8 (b, e);
	    avg_be = fast_vavgu8 (b, e);

	    mm2 = (vu8) vcmpleu8 (diff_be, best);
	    /* we only want luma from diagonals */
	    mm2 = vor (mm2, (vu8) UVMask);
	    mm1 = (vu8) vcmpleu8 (diff_be, vsplatu8_15);
	    /* we let bob through always if diff is small */
	    mm1 = vor (mm1, mm2);

	    bob = vsel (mm1, avg_be, diag);

	    /* SimpleWeave */

	    next = vload (F4, 0);
	    F4 += sizeof (vu8);

	    prev = vload (F2, 0);
	    F2 += sizeof (vu8);

	    /* "movement" in the centre */
	    cm = (v32) vabsdiffu8 (prev, next);
	    weave = fast_vavgu8 (prev, next);

	    /* operate only on luma as we will always bob the chroma */
	    cm = (v32) yuyv2yy ((vu8) cm);

	    above = vload (F1, 0);
	    below = vload (F1, src_bpl);
	    F1 += sizeof (vu8);

	    /* "movement" in the top and bottom btw curr and prev frame */
	    tm = (v32) yuyv2yy (vabsdiffu8 (b, above));
	    bm = (v32) yuyv2yy (vabsdiffu8 (e, below));

	    /* 0xff where movement (xm > 15) in either of *two* pixels */
	    tm = (v32) vcmpnz32 ((v32) vsubsu8 ((vu8) tm, vsplatu8_15));
	    cm = (v32) vcmpnz32 ((v32) vsubsu8 ((vu8) cm, vsplatu8_15));
	    bm = (v32) vcmpnz32 ((v32) vsubsu8 ((vu8) bm, vsplatu8_15));

	    mm2 = (vu8) vand (tm, bm); /* top and bottom moving */
	    mm1 = vor ((vu8) cm, mm2); /* where we should bob */
	    mm1 = vor (mm1, (vu8) UVMask);

	    /* Our value for the line of this field. */
	    vstorent (Dest, dst_bpl, vsel (mm1, bob, weave));

	    Dest += sizeof (vu8);
	}

	/* Simple bob last column. */
	simple_bob (Dest, F3, dst_bpl, src_bpl);

	F4 += src_padding2;
	F3 += src_padding1;
	F2 += src_padding2;
	F1 += src_padding2;
	Dest += dst_padding;
    }

    /* Remaining lines. */

    if (pInfo->PictureHistory[1]->Flags & PICTURE_INTERLACED_ODD) {
	copy_line (Dest, F3, byte_width);
	Dest += dst_bpl;
	F3 += src_bpl;
    }

    copy_line_pair (Dest, F3, byte_width, dst_bpl);

    vempty ();

    return TRUE;
}

#elif !SIMD

static const DEINTERLACE_METHOD
MoComp2Method = {
    sizeof(DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    N_("Video (MoComp2)"), 
    "MoComp2",
    FALSE, 
    FALSE, 
    /* pfnAlgorithm */ NULL,
    50, 
    60,
    0,
    NULL,
    INDEX_VIDEO_MOCOMP2,
    NULL,
    NULL,
    NULL,
    NULL,
    4, /* number fields needed */
    0,
    0,
    0,
    NULL,
    0,
    FALSE,
    FALSE,
    IDH_MOCOMP2,
};

DEINTERLACE_METHOD *
DI_MoComp2_GetDeinterlacePluginInfo (void)
{
    DEINTERLACE_METHOD *m;
    DEINTERLACE_FUNC *f;

    m = NULL;

    f = SIMD_FN_SELECT (DeinterlaceMoComp2,
			CPU_FEATURE_MMX | CPU_FEATURE_3DNOW |
			CPU_FEATURE_SSE | CPU_FEATURE_SSE2 |
			CPU_FEATURE_ALTIVEC);

    if (f) {
	m = malloc (sizeof (*m));
	*m = MoComp2Method;

	m->pfnAlgorithm = f;
    }

    return m;
}

#endif /* !SIMD */

/*
Local Variables:
c-basic-offset: 4
End:
 */

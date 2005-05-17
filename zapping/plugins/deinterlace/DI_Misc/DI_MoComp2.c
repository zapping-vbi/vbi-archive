/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_MoComp2.c,v 1.1.2.2 2005-05-17 19:58:32 mschimek Exp $
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

static always_inline void
simple_bob			(uint8_t *		pDest,
				 const uint8_t *	pBob,
				 unsigned int		dst_bpl,
				 unsigned int		src_bpl)
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

    if (SIMD == CPU_FEATURE_SSE2) {
	if ((INTPTR (pInfo->Overlay) |
	     INTPTR (pInfo->PictureHistory[0]->pData) |
	     INTPTR (pInfo->PictureHistory[1]->pData) |
	     INTPTR (pInfo->PictureHistory[2]->pData) |
	     INTPTR (pInfo->PictureHistory[3]->pData) |
	     (unsigned int) pInfo->OverlayPitch |
	     (unsigned int) pInfo->InputPitch |
	     (unsigned int) pInfo->LineLength) & 15)
	    return DeinterlaceMoComp2_SSE (pInfo);
    }

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
      5     F4-2  weave F1/3-2/3,F2/4-1/2/3  F3-2   copy F3-2
      n-2   F3     copy F3-(n/2-1)           F4    wcopy F3-(n/2-1)
      n-1   F4    wcopy F3-(n/2-1)           F3     copy F3-(n/2-1)

      pDest  2     1     
      pSrc   F4-0  F4-0  this field (-1, 0, +1)
      pBob   F3-1  F3-0  previous field, opposite parity (0, +1)
      pSrcP  F2-0  F2-0  previous field, same parity (-1, 0, +1)
      pBobP  F1-1  F1-0  previous field, opposite parity (0, +1)
    */

    pDest = (uint8_t *) pInfo->Overlay;

    pSrc  = (const uint8_t *) pInfo->PictureHistory[0]->pData;
    pSrcP = (const uint8_t *) pInfo->PictureHistory[2]->pData;

    pBob  = (const uint8_t *) pInfo->PictureHistory[1]->pData;
    pBobP = (const uint8_t *) pInfo->PictureHistory[3]->pData;

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
	/* Copy first even and odd lines from first line of previous field.
	   The loop copies the second and following pairs. */
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

    pSrc  += src_bpl + sizeof (vu8);
    pSrcP += src_bpl + sizeof (vu8);

    pBobP += sizeof (vu8); /* no access in first column */

    /* All but first and last field line where we cannot read the
       line above and below. */
    for (height -= 2; height > 0; --height) {
	vu8 a, b, c, d, e, f;
	unsigned int count;

	/* Simple bob first column. */
	simple_bob (pDest, pBob, dst_bpl, src_bpl);

	pBob += sizeof (vu8);
	pDest += sizeof (vu8);

	/* All but first and last column where we cannot read the
	   pixels left and right. */
	for (count = byte_width / sizeof (vu8) - 2; count > 0; --count) {
	    vu8 af, cd, be;
	    vu8 avg_af, avg_cd, avg_be;
	    vu8 best, diag, bob, weave;
	    vu8 mm1, mm2, mm4, mm6;
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
	       a b c 	current field
	         x	calculated line
	       d e f 	current field
	    */
	    /* XXX could calculate a, d from b, e of previous loop
	       and integrate first 8/16 bytes into the loop. */
	    uload2t (&a, &b, &c, pBob, 0);
	    uload2t (&d, &e, &f, pBob, src_bpl);

	    af = vabsdiffu8 (a, f);
	    avg_af = fast_vavgu8 (a, f);

	    cd = vabsdiffu8 (c, d);
	    avg_cd = fast_vavgu8 (c, d);

	    /* Copy line of previous field. */
	    vstorent (pDest, 0, b);

	    mm4 = fast_vavgu8 (vabsdiffu8 (b, d), vabsdiffu8 (c, e));
	    mm6 = fast_vavgu8 (vabsdiffu8 (a, e), vabsdiffu8 (b, f));
	    mm1 = (vu8) vcmpleu8 (mm6, mm4);
	    best = vsel (mm1, af, cd);
	    diag = vsel (mm1, avg_af, avg_cd);

	    be = vabsdiffu8 (b, e);
	    avg_be = fast_vavgu8 (b, e);

	    mm2 = (vu8) vcmpleu8 (be, best);
	    /* we only want luma from diagonals */
	    mm2 = vor (mm2, (vu8) UVMask);
	    mm1 = (vu8) vcmpleu8 (be, vsplatu8_15);
	    /* we let bob through always if diff is small */
	    mm1 = vor (mm1, mm2);

	    bob = vsel (mm1, avg_be, diag);

	    /* SimpleWeave */

	    mm1 = * (const vu8 *) pSrcP;
	    mm2 = * (const vu8 *) pSrc;
	    /* "movement" in the centre */
	    cm = (v32) vabsdiffu8 (mm1, mm2);
	    weave = fast_vavgu8 (mm1, mm2);

	    /* operate only on luma as we will always bob the chroma */
	    cm = (v32) yuyv2yy ((vu8) cm);

	    mm1 = vload (pBobP, 0);
	    mm2 = vload (pBobP, src_bpl);

	    /* "movement" in the top */
	    tm = (v32) yuyv2yy (vabsdiffu8 (b, mm1));

	    /* "movement" in the bottom */
	    bm = (v32) yuyv2yy (vabsdiffu8 (e, mm2));

	    /* 0xff where movement (xm > 15) in either of *two* pixels */
	    tm = (v32) vcmpnz32 ((v32) vsubsu8 ((vu8) tm, vsplatu8_15));
	    cm = (v32) vcmpnz32 ((v32) vsubsu8 ((vu8) cm, vsplatu8_15));
	    bm = (v32) vcmpnz32 ((v32) vsubsu8 ((vu8) bm, vsplatu8_15));

	    mm2 = (vu8) vand (tm, bm); /* top and bottom moving */
	    mm1 = vor ((vu8) cm, mm2); /* where we should bob */
	    mm1 = vor (mm1, (vu8) UVMask);

	    /* Our value for the line of this field. */
	    vstorent (pDest, dst_bpl, vsel (mm1, bob, weave));

	    pSrc  += sizeof (vu8);
	    pSrcP += sizeof (vu8);
	    pBob  += sizeof (vu8);
	    pBobP += sizeof (vu8);
	    pDest += sizeof (vu8);
	}

	/* Simple bob last column. */
	simple_bob (pDest, pBob, dst_bpl, src_bpl);

	pSrc  += src_padding2;
	pSrcP += src_padding2;
	pBob  += src_padding1;
	pBobP += src_padding2;
	pDest += dst_padding;
    }

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_EVEN) {
	/* Copy but-last odd dest line which isn't covered by the loop. */
	copy_line (pDest, pBob, byte_width);

	pBob  += src_bpl;
	pDest += dst_bpl;
    }

    /* Copy the last even and odd line from
       the last line of the previous field. */
    copy_line_pair (pDest, pBob, byte_width, dst_bpl);

    vempty ();

    return TRUE;
}

#elif !SIMD

const DEINTERLACE_METHOD MoComp2Method =
{
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
    WM_DI_MOCOMP2_GETVALUE - WM_APP,
    NULL,
    0,
    FALSE,
    FALSE,
    IDH_MOCOMP2,
};

DEINTERLACE_METHOD* DI_MoComp2_GetDeinterlacePluginInfo(long CpuFeatureFlags)
{
    DEINTERLACE_METHOD *m;

    CpuFeatureFlags = CpuFeatureFlags;

    m = malloc (sizeof (*m));
    *m = MoComp2Method;

    m->pfnAlgorithm =
	SIMD_FN_SELECT (DeinterlaceMoComp2,
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

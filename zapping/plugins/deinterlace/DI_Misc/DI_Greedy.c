/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_Greedy.c,v 1.3.2.1 2005-05-05 09:46:01 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2001 Tom Barry.  All rights reserved.
// Copyright (C) 2005 Michael Schimek
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
/////////////////////////////////////////////////////////////////////////////
// Change Log
//
// Date          Developer             Changes
//
// 16 Jan 2001   Tom Barry             Added Greedy Deinterlace method
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.3  2005/03/30 21:27:58  mschimek
// Integrated and converted the MMX code to vector intrinsics.
//
// Revision 1.2  2005/02/05 22:20:03  mschimek
// Completed l18n.
//
// Revision 1.1  2005/01/08 14:35:11  mschimek
// TomsMoCompMethod, MoComp2Method, VideoWeaveMethod, VideoBobMethod,
// TwoFrameMethod, OldGameMethod, Greedy2FrameMethod, GreedyMethod,
// DI_GreedyHSettings: Localized.
//
// Revision 1.2  2004/11/15 23:03:19  michael
// *** empty log message ***
//
// Revision 1.1  2004/11/14 15:35:14  michael
// *** empty log message ***
//
// Revision 1.6  2002/06/18 19:46:07  adcockj
// Changed appliaction Messages to use WM_APP instead of WM_USER
//
// Revision 1.5  2002/06/13 12:10:24  adcockj
// Move to new Setings dialog for filers, video deint and advanced settings
//
// Revision 1.4  2001/07/13 16:13:33  adcockj
// Added CVS tags and removed tabs
//
///////////////////////////////////////////////////////////////////////////*/

#include "windows.h"
#include "DS_Deinterlace.h"

extern long GreedyMaxComb;

SIMD_FN_PROTOS (DEINTERLACE_FUNC, DeinterlaceGreedy);

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW |			\
	    CPU_FEATURE_SSE | CPU_FEATURE_SSE2 | CPU_FEATURE_ALTIVEC)

/* This is a simple lightweight DeInterlace method that uses little CPU time
// but gives very good results for low or intermedite motion.
// It defers frames by one field, but that does not seem to produce noticeable
// lip sync problems.
//
// The method used is to take either the older or newer weave pixel depending
// upon which give the smaller comb factor, and then clip to avoid large damage
// when wrong.
//
// I'd intended this to be part of a larger more elaborate method added to 
// Blended Clip but this give too good results for the CPU to ignore here.
*/

BOOL
SIMD_NAME (DeinterlaceGreedy)	(TDeinterlaceInfo *	pInfo)
{
    vu8 MaxComb;
    uint8_t *Dest;
    const uint8_t *L1;		/* ptr to Line1, of 3 */
    const uint8_t *L2;		/* ptr to Line2, the weave line */
    const uint8_t *L3;		/* ptr to Line3 */
    const uint8_t *LP2;		/* ptr to prev Line2 */
    unsigned int byte_width;
    unsigned int height;
    unsigned int dst_padding;
    unsigned int src_padding;
    unsigned int dst_bpl;
    unsigned int src_bpl;

    if (SIMD == CPU_FEATURE_SSE2) {
	if ((INTPTR (pInfo->Overlay) |
	     INTPTR (pInfo->PictureHistory[0]->pData) |
	     INTPTR (pInfo->PictureHistory[1]->pData) |
	     INTPTR (pInfo->PictureHistory[2]->pData) |
	     (unsigned int) pInfo->OverlayPitch |
	     (unsigned int) pInfo->InputPitch |
	     (unsigned int) pInfo->LineLength) & 15)
	    return DeinterlaceGreedy_SSE (pInfo);
    }

    /* How badly do we let it weave? 0-255 */
    MaxComb = vsplatu8 (GreedyMaxComb);

    byte_width = pInfo->LineLength;

    dst_bpl = pInfo->OverlayPitch;
    src_bpl = pInfo->InputPitch;

    Dest = pInfo->Overlay; /* DL1 if Odd or DL2 if Even */

    /* copy first even line no matter what, and the first odd line if we're
       processing an EVEN field. (note diff from other deint rtns.) */

    L1 = pInfo->PictureHistory[1]->pData;
    L2 = pInfo->PictureHistory[0]->pData;  
    L3 = L1 + src_bpl;
    LP2 = pInfo->PictureHistory[2]->pData;

    if (!(pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD)) {
	/* copy first even line */
	copy_line (Dest, L2, byte_width);
	Dest += dst_bpl;

	L2 += src_bpl;
	LP2 += src_bpl;
    }

    copy_line (Dest, L1, byte_width);
    Dest += dst_bpl;

    dst_padding = dst_bpl * 2 - byte_width;
    src_padding = src_bpl - byte_width;

    for (height = pInfo->FieldHeight; height > 0; --height) {
	unsigned int count;

	/* For ease of reading, the comments below assume that we're
	   operating on an odd field (i.e., that pInfo->IsOdd is true).
	   Assume the obvious for even lines. */

	for (count = byte_width / sizeof (vu8); count > 0; --count) {
	    vu8 l1, l2, l3, lp2, avg, mm4, mm5, best, min, max, sel;

	    l1 = * (const vu8 *) L1;
	    L1 += sizeof (vu8);
	    l3 = * (const vu8 *) L3;
	    L3 += sizeof (vu8);

	    vstorent (Dest, dst_bpl, l3);

            /* the average, for computing comb */
	    avg = fast_vavgu8 (l1, l3);

	    l2 = * (const vu8 *) L2;
	    L2 += sizeof (vu8);
	    lp2 = * (const vu8 *) LP2;
	    LP2 += sizeof (vu8);

	    /* get abs value of possible L2 and LP2 comb */
	    mm5 = vabsdiffu8 (l2, avg);
	    mm4 = vabsdiffu8 (lp2, avg);

	    /* use L2 or LP2 depending upon which makes smaller comb */
	    sel = (vu8) vcmpleu8 (mm4, mm5);
	    best = vsel (sel, lp2, l2);

	    /* Now lets clip our chosen value to be not outside of the range
	       of the high/low range L1-L3 by more than abs(L1-L3)
	       This allows some comb but limits the damages and also allows
	       more detail than a boring oversmoothed clip. */

	    vminmaxu8 (&min, &max, l1, l3);

	    /* allow the value to be above the high or below the low
	       by amt of MaxComb */
	    max = vaddsu8 (max, MaxComb);
	    min = vsubsu8 (min, MaxComb);

	    best = vsatu8 (best, min, max);
	    vstorent (Dest, 0, best);
	    Dest += sizeof (vu8);
	}

	L1 += src_padding;
	L2 += src_padding;
	L3 += src_padding;
	LP2 += src_padding;
	Dest += dst_padding;
    }

    /* Copy last odd line if we're processing an Odd field. */
    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
        copy_line (Dest, L2, byte_width);
    }

    vempty ();

    return TRUE;
}

#elif !SIMD

long GreedyMaxComb = 15;

/*//////////////////////////////////////////////////////////////////////////
// Start of Settings related code
//////////////////////////////////////////////////////////////////////////*/
SETTING DI_GreedySettings[DI_GREEDY_SETTING_LASTONE] =
{
    {
        N_("Greedy Max Comb"), SLIDER, 0, &GreedyMaxComb,
        15, 0, 255, 1, 1,
        NULL,
        "Deinterlace", "GreedyMaxComb", NULL,
    },
};

const DEINTERLACE_METHOD GreedyMethod =
{
    sizeof(DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    N_("Video (Greedy, Low Motion)"), 
    "Greedy",
    FALSE, 
    FALSE, 
    /* pfnAlgorithm */ NULL,
    50, 
    60,
    DI_GREEDY_SETTING_LASTONE,
    DI_GreedySettings,
    INDEX_VIDEO_GREEDY,
    NULL,
    NULL,
    NULL,
    NULL,
    3,
    0,
    0,
    WM_DI_GREEDY_GETVALUE - WM_APP,
    NULL,
    0,
    FALSE,
    FALSE,
    IDH_GREEDY,
};


DEINTERLACE_METHOD* DI_Greedy_GetDeinterlacePluginInfo(long CpuFeatureFlags)
{
    DEINTERLACE_METHOD *m;

    CpuFeatureFlags = CpuFeatureFlags;

    m = malloc (sizeof (*m));
    *m = GreedyMethod;

    m->pfnAlgorithm =
	SIMD_FN_SELECT (DeinterlaceGreedy,
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

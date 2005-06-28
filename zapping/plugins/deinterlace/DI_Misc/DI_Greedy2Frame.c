/*////////////////////////////////////////////////////////////////////////////
// $Id: DI_Greedy2Frame.c,v 1.4 2005-06-28 00:50:03 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000 John Adcock, Tom Barry, Steve Grimm  All rights reserved.
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
///////////////////////////////////////////////////////////////////////////////
// Change Log
//
// Date          Developer             Changes
//
// 08 Feb 2000   John Adcock           New Method
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.3.2.5  2005/06/17 02:54:20  mschimek
// *** empty log message ***
//
// Revision 1.3.2.4  2005/05/31 02:40:34  mschimek
// *** empty log message ***
//
// Revision 1.3.2.3  2005/05/20 05:45:14  mschimek
// *** empty log message ***
//
// Revision 1.3.2.2  2005/05/17 19:58:32  mschimek
// *** empty log message ***
//
// Revision 1.3.2.1  2005/05/05 09:46:01  mschimek
// *** empty log message ***
//
// Revision 1.3  2005/03/30 21:27:32  mschimek
// Integrated and converted the MMX code to vector intrinsics.
//
// Revision 1.2  2005/02/05 22:19:53  mschimek
// Completed l18n.
//
// Revision 1.1  2005/01/08 14:35:01  mschimek
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
// Revision 1.8  2002/06/18 19:46:07  adcockj
// Changed appliaction Messages to use WM_APP instead of WM_USER
//
// Revision 1.7  2002/06/13 12:10:24  adcockj
// Move to new Setings dialog for filers, video deint and advanced settings
//
// Revision 1.6  2001/07/13 16:13:33  adcockj
// Added CVS tags and removed tabs
//
///////////////////////////////////////////////////////////////////////////*/

#include "windows.h"
#include "DS_Deinterlace.h"

extern int GreedyTwoFrameThreshold;

SIMD_FN_PROTOS (DEINTERLACE_FUNC, DeinterlaceGreedy2Frame);

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW |			\
	    CPU_FEATURE_SSE | CPU_FEATURE_SSE2 | CPU_FEATURE_ALTIVEC)

/*/////////////////////////////////////////////////////////////////////////////
// Field 1 | Field 2 | Field 3 | Field 4 |
//   T0    |         |    T1   |         | 
//         |   M0    |         |    M1   | 
//   B0    |         |    B1   |         | 
*/

/* debugging feature
   output the value of mm4 at this point which is pink where we will weave
   and green were we are going to bob */
#define CHECK_BOBWEAVE 0

static always_inline v32
thresh_cmp			(vu8			a,
				 vu8			b,
				 v8			thresh,
				 v32			c)
{
    vu8 t;

    // XXX emulates vcmpgtu8, AVEC has this instruction
    t = vsr1u8 (vabsdiffu8 (a, b));
    t = (vu8) vcmpgt8 ((v8) t, thresh);

    t = vand (t, vsplatu8_127); // get rid of any sign bit
 
    // XXX can be replaced by logic ops.
    return vand ((v32) vcmpgt32 ((v32) t, vsplat32_1), c);
}

BOOL
SIMD_NAME (DeinterlaceGreedy2Frame) (TDeinterlaceInfo *pInfo)
{
    v8 qwGreedyTwoFrameThreshold;
    uint8_t *Dest;
    const uint8_t *T0;
    const uint8_t *T1;
    const uint8_t *M0;
    const uint8_t *M1;
    unsigned int byte_width;
    unsigned int height;
    unsigned long dst_padding;
    unsigned long src_padding;
    unsigned long dst_bpl;
    unsigned long src_bpl;

    if (SIMD == CPU_FEATURE_SSE2) {
	if ((INTPTR (pInfo->Overlay) |
	     INTPTR (pInfo->PictureHistory[0]->pData) |
	     INTPTR (pInfo->PictureHistory[1]->pData) |
	     INTPTR (pInfo->PictureHistory[2]->pData) |
	     INTPTR (pInfo->PictureHistory[3]->pData) |
	     (unsigned long) pInfo->OverlayPitch |
	     (unsigned long) pInfo->InputPitch |
	     (unsigned long) pInfo->LineLength) & 15)
	    return DeinterlaceGreedy2Frame_SSE (pInfo);
    }

    qwGreedyTwoFrameThreshold = vsplat8 (GreedyTwoFrameThreshold);

    byte_width = pInfo->LineLength;

    dst_bpl = pInfo->OverlayPitch;
    src_bpl = pInfo->InputPitch;

    Dest = pInfo->Overlay;

    M1 = pInfo->PictureHistory[0]->pData;
    T1 = pInfo->PictureHistory[1]->pData;
    M0 = pInfo->PictureHistory[2]->pData;
    T0 = pInfo->PictureHistory[3]->pData;

    if (!(pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD)) {
        copy_line (Dest, M1, byte_width);
        Dest += dst_bpl;

        M1 = (const uint8_t *) pInfo->PictureHistory[0]->pData + src_bpl;
        M0 = (const uint8_t *) pInfo->PictureHistory[2]->pData + src_bpl;
    }

    dst_padding = dst_bpl * 2 - byte_width;
    src_padding = src_bpl - byte_width;

    for (height = pInfo->FieldHeight - 1; height > 0; --height) {
        unsigned int count;

	for (count = byte_width / sizeof (vu8); count > 0; --count) {
	    vu8 m0, m1, t0, t1, b0, b1, avg, mm4;
	    v32 sum;

	    t1 = vload (T1, 0);
	    b1 = vload (T1, src_bpl);
	    T1 += sizeof (vu8);

	    /* Always use the most recent data verbatim.  By definition it's
	       correct (it'd be shown on an interlaced display) and our job is
	       to fill in the spaces between the new lines. */
	    vstorent (Dest, 0, t1);

	    avg = fast_vavgu8 (t1, b1);

	    m0 = vload (M0, 0);
	    M0 += sizeof (vu8);
	    m1 = vload (M1, 0);
	    M1 += sizeof (vu8);

	    /* if we have a good processor then make mm0 the average of M1
	       and M0 which should make weave look better when there is
	       small amounts of movement */
	    if (SIMD != CPU_FEATURE_MMX)
		m1 = vavgu8 (m1, m0);

	    /* if |M1-M0| > Threshold we want dword worth of twos */
	    sum = thresh_cmp (m1, m0, qwGreedyTwoFrameThreshold, vsplat32_2);

	    /* if |T1-T0| > Threshold we want dword worth of ones */
	    t0 = vload (T0, 0);
	    sum = vadd32 (sum, thresh_cmp (t1, t0, qwGreedyTwoFrameThreshold,
					   vsplat32_1));

	    /* if |B1-B0| > Threshold we want dword worth of ones */
	    b0 = vload (T0, src_bpl);
	    T0 += sizeof (vu8);
	    sum = vadd32 (sum, thresh_cmp (b1, b0, qwGreedyTwoFrameThreshold,
					   vsplat32_1));

	    mm4 = (vu8) vcmpgt32 (sum, vsplat32_2);

	    /* debugging feature
	       output the value of mm4 at this point which is pink
	       where we will weave and green were we are going to bob */
	    if (CHECK_BOBWEAVE)
		vstorent (Dest, dst_bpl, mm4);
	    else
		vstorent (Dest, dst_bpl, vsel (mm4, m1, avg));

	    Dest += sizeof (vu8);
	}

        M1 += src_padding;
        T1 += src_padding;
        M0 += src_padding;
        T0 += src_padding;
        Dest += dst_padding;
    }

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
	copy_line (Dest, T1, byte_width);
	Dest += dst_bpl;
	copy_line (Dest, M1, byte_width);
    } else {
	copy_line (Dest, T1, byte_width);
    }

    vempty ();

    return TRUE;
}

#elif !SIMD

int GreedyTwoFrameThreshold = 4;


/*//////////////////////////////////////////////////////////////////////////
// Start of Settings related code
//////////////////////////////////////////////////////////////////////////*/
static const SETTING
DI_Greedy2FrameSettings [] = {
    {
        N_("Greedy 2 Frame Luma Threshold"), SLIDER, 0,
	&GreedyTwoFrameThreshold,
        4, 0, 127, 1, 1,
        NULL,
        "Deinterlace", "GreedyTwoFrameThreshold", NULL,
    },
};

static const DEINTERLACE_METHOD
Greedy2FrameMethod = {
    sizeof (DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    N_("Greedy 2 Frame"), 
    "Greedy2", 
    FALSE, 
    FALSE, 
    /* pfnAlgorithm */ NULL,
    50, 
    60,
    N_ELEMENTS (DI_Greedy2FrameSettings),
    DI_Greedy2FrameSettings,
    INDEX_VIDEO_GREEDY2FRAME,
    NULL,
    NULL,
    NULL,
    NULL,
    4,
    0,
    0,
    0,
    NULL,
    0,
    FALSE,
    FALSE,
    IDH_GREEDY2
};

DEINTERLACE_METHOD *
DI_Greedy2Frame_GetDeinterlacePluginInfo (void)
{
    DEINTERLACE_METHOD *m;
    DEINTERLACE_FUNC *f;

    m = NULL;

    f = SIMD_FN_SELECT (DeinterlaceGreedy2Frame,
			CPU_FEATURE_MMX | CPU_FEATURE_3DNOW |
			CPU_FEATURE_SSE | CPU_FEATURE_SSE2 |
			CPU_FEATURE_ALTIVEC);

    if (f) {
	m = malloc (sizeof (*m));
	*m = Greedy2FrameMethod;

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

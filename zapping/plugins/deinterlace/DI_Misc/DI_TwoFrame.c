///////////////////////////////////////////////////////////////////////////////
// $Id: DI_TwoFrame.c,v 1.3 2005-03-30 21:26:54 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000 Steven Grimm.  All rights reserved.
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
// 30 Dec 2000   Mark Rejhon           Split into separate module
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.2  2005/02/05 22:19:14  mschimek
// Completed l18n.
//
// Revision 1.1  2005/01/08 14:34:04  mschimek
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
/////////////////////////////////////////////////////////////////////////////

#include "windows.h"
#include "DS_Deinterlace.h"

extern long TwoFrameTemporalTolerance;
extern long TwoFrameSpatialTolerance;

SIMD_PROTOS (DeinterlaceFieldTwoFrame);

#ifdef SIMD

///////////////////////////////////////////////////////////////////////////////
// Deinterlace the latest field, attempting to weave wherever it won't cause
// visible artifacts.
//
// The data from the most recently captured field is always copied to the
// overlay verbatim.  For the data from the previous field, the following
// algorithm is applied to each pixel.
//
// We use the following notation for the top, middle, and bottom pixels
// of concern:
//
// Field 1 | Field 2 | Field 3 | Field 4 |
//         |   T0    |         |   T1    | scanline we copied in last iteration
//   M0    |         |    M1   |         | interm. scanline from altern. field
//         |   B0    |         |   B1    | scanline we just copied
//
// We will weave M1 into the image if any of the following is true:
//   - M1 is similar to either B1 or T1.  This indicates that no weave
//     artifacts would be visible.  The SpatialTolerance setting controls
//     how far apart the luminances can be before pixels are considered
//     non-similar.
//   - T1 and B1 and M1 are old.  In that case any weave artifact that
//     appears isn't due to fast motion, since it was there in the previous
//     frame too.  By "old" I mean similar to their counterparts in the
//     previous frame; TemporalTolerance controls the maximum squared
//     luminance difference above which a pixel is considered "new".
//
// Pixels are processed 4 at a time using MMX instructions.
//
// SQUARING NOTE:
// We square luminance differences to amplify the effects of large
// differences and to avoid dealing with negative differences.  Unfortunately,
// we can't compare the square of difference directly against a threshold,
// thanks to the lack of an MMX unsigned compare instruction.  The
// problem is that if we had two pixels with luminance 0 and 255,
// the difference squared would be 65025, which is a negative
// 16-bit signed value and would thus compare less than a threshold.
// We get around this by dividing all the luminance values by two before
// squaring them; this results in an effective maximum luminance
// difference of 127, whose square (16129) is safely comparable.

static __inline__ v16
cmpsqdiff			(v16			a,
				 v16			b,
				 v16			thresh)
{
    v16 t;

#if SIMD == AVEC
    t = vsubs16 (a, b);
    return (v16) vcmpgtu16 ((vu16) vmullo16 (t, t), (vu16) thresh);
#else
    t = vsr16 (vsubs16 (a, b), 1);
    return (v16) vcmpgt16 ((v16) vmullo16 (t, t), thresh);
#endif
}

BOOL
SIMD_NAME (DeinterlaceFieldTwoFrame) (TDeinterlaceInfo *pInfo)
{
    v16 qwSpatialTolerance;
    v16 qwTemporalTolerance;
    uint8_t *Dest;
    const uint8_t *YVal0;
    const uint8_t *YVal1;
    const uint8_t *OVal0;
    const uint8_t *OVal1;
    unsigned int byte_width;
    unsigned int height;
    unsigned int dst_padding;
    unsigned int src_padding;
    unsigned int dst_bpl;
    unsigned int src_bpl;

    if (SIMD == SSE2) {
	if (((unsigned int) pInfo->Overlay |
	     (unsigned int) pInfo->PictureHistory[0]->pData |
	     (unsigned int) pInfo->PictureHistory[1]->pData |
	     (unsigned int) pInfo->PictureHistory[2]->pData |
	     (unsigned int) pInfo->PictureHistory[3]->pData |
	     (unsigned int) pInfo->OverlayPitch |
	     (unsigned int) pInfo->InputPitch |
	     (unsigned int) pInfo->LineLength) & 15)
	    return DeinterlaceFieldTwoFrame__SSE (pInfo);
    }

    if (SIMD == AVEC) {
	qwSpatialTolerance = vsplat16 (TwoFrameSpatialTolerance);
	qwTemporalTolerance = vsplat16 (TwoFrameTemporalTolerance);
    } else {
	// divide by 4 because of squaring behavior, see below
	qwSpatialTolerance = vsplat16 (TwoFrameSpatialTolerance / 4);
	qwTemporalTolerance = vsplat16 (TwoFrameTemporalTolerance / 4);
    }

    byte_width = pInfo->LineLength;

    dst_bpl = pInfo->OverlayPitch;
    src_bpl = pInfo->InputPitch;

    Dest = pInfo->Overlay;

    YVal0 = pInfo->PictureHistory[0]->pData;
    YVal1 = pInfo->PictureHistory[1]->pData;
    OVal0 = pInfo->PictureHistory[2]->pData;
    OVal1 = pInfo->PictureHistory[3]->pData;

    // copy first even line no matter what, and the first odd line if we're
    // processing an odd field.

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
        copy_line (Dest, YVal1, byte_width);
        Dest += dst_bpl;

        YVal1 += src_bpl;
        OVal1 += src_bpl;
    }

    copy_line (Dest, YVal0, byte_width);
    Dest += dst_bpl;

    dst_padding = dst_bpl * 2 - byte_width;
    src_padding = src_bpl - byte_width;

    for (height = pInfo->FieldHeight; height > 0; --height) {
        unsigned int count;

	for (count = byte_width / sizeof (vu8); count > 0; --count) {
	    vu8 T1, B1, M1, avg;
	    v16 lum_T0, lum_B0, lum_M0; 
	    v16 lum_T1, lum_B1, lum_M1;
	    v16 mm3, mm4, mm5, mm7;

	    T1 = * (const vu8 *) &YVal0[0];
	    B1 = * (const vu8 *) &YVal0[src_bpl];
	    YVal0 += sizeof (vu8);

	    // Always use the most recent data verbatim.
	    vstorent ((vu8 *)(Dest + dst_bpl), B1);

	    avg = fast_vavgu8 (T1, B1);

	    M1 = * (const vu8 *) YVal1;
	    YVal1 += sizeof (vu8);

	    lum_T1 = yuyv2yy (T1);
	    lum_B1 = yuyv2yy (B1);
	    lum_M1 = yuyv2yy (M1);

	    // Find out how different T1 and M1 are.
	    mm3 = cmpsqdiff (lum_T1, lum_M1, qwSpatialTolerance);

    	    // Find out how different B1 and M1 are.
	    mm4 = cmpsqdiff (lum_B1, lum_M1, qwSpatialTolerance);

	    // We care about cases where M1 isn't different from its
	    // neighbors T1 and B1.
	    mm3 = vnand (mm3, mm4);

	    lum_M0 = yuyv2yy (* (const vu8 *) OVal1);
	    OVal1 += sizeof (vu8);

	    // Find out whether M1 is new.  "New" means the square of
	    // the luminance difference between M1 and M0 is less than
	    // the temporal tolerance.
	    mm7 = cmpsqdiff (lum_M1, lum_M0, qwTemporalTolerance);

	    lum_T0 = yuyv2yy (* (const vu8 *) &OVal0[0]);
	    lum_B0 = yuyv2yy (* (const vu8 *) &OVal0[src_bpl]);
	    OVal0 += sizeof (vu8);

	    // Find out whether T1 is new.
	    mm4 = cmpsqdiff (lum_T1, lum_T0, qwTemporalTolerance);

	    // Find out whether B1 is new.
	    mm5 = cmpsqdiff (lum_B1, lum_B0, qwTemporalTolerance);

	    // We care about cases where M1 is old
	    // and either T1 or B1 is old.
	    mm4 = vnor (mm7, vor (mm4, mm5));

	    // Now figure out where we're going to weave (M1) and where
	    // we're going to bob (avg).  We'll weave if all pixels are
	    // old or M1 isn't different from both its neighbors.
	    vstorent ((vu8 *) Dest, vsel (avg, M1, (vu8) vor (mm4, mm3)));
	    Dest += sizeof (vu8);
	}

	YVal0 += src_padding;
        YVal1 += src_padding;
        OVal0 += src_padding;
        OVal1 += src_padding;
        Dest += dst_padding;
    }

    // Copy last odd line if we're processing an even field.
    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_EVEN) {
        copy_line (Dest, YVal1, byte_width);
    }

    vempty ();

    return TRUE;
}

#else /* !SIMD */

long TwoFrameTemporalTolerance = 300;
long TwoFrameSpatialTolerance = 600;

////////////////////////////////////////////////////////////////////////////
// Start of Settings related code
/////////////////////////////////////////////////////////////////////////////
SETTING DI_TwoFrameSettings[DI_TWOFRAME_SETTING_LASTONE] =
{
    {
        N_("2 Frame Spatial Tolerance"), SLIDER, 0, &TwoFrameSpatialTolerance,
        600, 0, 5000, 10, 1,
        NULL,
        "Deinterlace", "TwoFrameSpatialTolerance", NULL,
    },
    {
        N_("2 Frame Temporal Tolerance"), SLIDER, 0, &TwoFrameTemporalTolerance,
        300, 0, 5000, 10, 1,
        NULL,
        "Deinterlace", "TwoFrameTemporalTolerance", NULL,
    },
};

DEINTERLACE_METHOD TwoFrameMethod =
{
    sizeof(DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    N_("Video Deinterlace (2-Frame)"), 
    "2-Frame", 
    FALSE, 
    FALSE, 
    /* pfnAlgorithm */ NULL,
    50, 
    60,
    DI_TWOFRAME_SETTING_LASTONE,
    DI_TwoFrameSettings,
    INDEX_VIDEO_2FRAME,
    NULL,
    NULL,
    NULL,
    NULL,
    4,
    0,
    0,
    WM_DI_TWOFRAME_GETVALUE - WM_APP,
    NULL,
    0,
    FALSE,
    FALSE,
    IDH_2FRAME,
};

DEINTERLACE_METHOD* DI_TwoFrame_GetDeinterlacePluginInfo(long CpuFeatureFlags)
{
    TwoFrameMethod.pfnAlgorithm = SIMD_SELECT (DeinterlaceFieldTwoFrame);
    return &TwoFrameMethod;
}

#endif /* !SIMD */

/*
Local Variables:
c-basic-offset: 4
End:
 */

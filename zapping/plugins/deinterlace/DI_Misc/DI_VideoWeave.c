/////////////////////////////////////////////////////////////////////////////
// $Id: DI_VideoWeave.c,v 1.3 2005-03-30 21:26:06 mschimek Exp $
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
/////////////////////////////////////////////////////////////////////////////
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
// Revision 1.2  2005/02/05 22:18:54  mschimek
// Completed l18n.
//
// Revision 1.1  2005/01/08 14:33:42  mschimek
// TomsMoCompMethod, MoComp2Method, VideoWeaveMethod, VideoBobMethod,
// TwoFrameMethod, OldGameMethod, Greedy2FrameMethod, GreedyMethod,
// DI_GreedyHSettings: Localized.
//
// Revision 1.2  2004/11/15 23:03:19  michael
// *** empty log message ***
//
// Revision 1.1  2004/11/14 15:35:15  michael
// *** empty log message ***
//
// Revision 1.6  2002/06/18 19:46:08  adcockj
// Changed appliaction Messages to use WM_APP instead of WM_USER
//
// Revision 1.5  2002/06/13 12:10:25  adcockj
// Move to new Setings dialog for filers, video deint and advanced settings
//
// Revision 1.4  2001/07/13 16:13:33  adcockj
// Added CVS tags and removed tabs
//
/////////////////////////////////////////////////////////////////////////////

#include "windows.h"
#include "DS_Deinterlace.h"

extern long TemporalTolerance;
extern long SpatialTolerance;
extern long SimilarityThreshold;

SIMD_PROTOS (DeinterlaceFieldWeave);

#ifdef SIMD

BOOL
SIMD_NAME (DeinterlaceFieldWeave) (TDeinterlaceInfo *	pInfo)
{
    v16 qwSpatialTolerance;
    v16 qwTemporalTolerance;
    vu16 qwThreshold;
    uint8_t *Dest;
    const uint8_t *YVal1;
    const uint8_t *YVal2;
    const uint8_t *YVal3;
    const uint8_t *YVal4;
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
	     (unsigned int) pInfo->OverlayPitch |
	     (unsigned int) pInfo->InputPitch |
	     (unsigned int) pInfo->LineLength) & 15)
	    return DeinterlaceFieldBob__SSE (pInfo);
    }

    qwSpatialTolerance = vsplat16 (SpatialTolerance);
    qwTemporalTolerance = vsplat16 (TemporalTolerance);
    qwThreshold = vsplatu16 (SimilarityThreshold);

    byte_width = pInfo->LineLength;

    dst_bpl = pInfo->OverlayPitch;
    src_bpl = pInfo->InputPitch;

    Dest = pInfo->Overlay;

    YVal1 = pInfo->PictureHistory[1]->pData;
    YVal2 = pInfo->PictureHistory[0]->pData;
    YVal3 = YVal1 + src_bpl;
    YVal4 = (const uint8_t *) pInfo->PictureHistory[2]->pData + src_bpl;

    if (!(pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD)) {
        copy_line (Dest, YVal2, byte_width);
        Dest += dst_bpl;
        YVal2 += src_bpl;
    }

    copy_line (Dest, YVal1, byte_width);
    Dest += dst_bpl;

    dst_padding = dst_bpl * 2 - byte_width;
    src_padding = src_bpl - byte_width;

    for (height = pInfo->FieldHeight; height > 0; --height) {
        unsigned int count;

        // For ease of reading, the comments below assume that we're
	// operating on an odd field (i.e., that bIsOdd is true).  The
	// exact same processing is done when we operate on an even
	// field, but the roles of the odd and even fields are reversed.
        // It's just too cumbersome to explain the algorithm in terms
	// of "the next odd line if we're doing an odd field, or the
	// next even line if we're doing an even field" etc.  So
	// wherever you see "odd" or "even" below, keep in mind that
        // half the time this function is called, those words' meanings
	// will invert.

	for (count = byte_width / sizeof (vu8); count > 0; --count) {
	    vu8 E1, O, E2, avg;
	    v16 lum_E1, lum_O, lum_E2, mm5, lum_Oold;
	    vu16 mm3, mm7;

	    E1 = * (const vu8 *) YVal1;
	    YVal1 += sizeof (vu8);
	    lum_E1 = yuyv2yy (E1);

	    E2 = * (const vu8 *) YVal3;
	    YVal3 += sizeof (vu8);
	    lum_E2 = yuyv2yy (E2);

    	    // Copy the even scanline below this one to the overlay buffer,
	    // since we'll be adapting the current scanline to the even
	    // lines surrounding it.
	    vstorent ((vu8 *)(Dest + dst_bpl), E2);

	    // Average E1 and E2 for interpolated bobbing.
	    avg = fast_vavgu8 (E1, E2);

	    O = * (const vu8 *) YVal2;
	    YVal2 += sizeof (vu8);
	    lum_O = yuyv2yy (O);

	    // The meat of the work is done here.  We want to see
	    // whether this pixel is close in luminosity to ANY of:
	    // its top neighbor, its bottom neighbor, or its
	    // predecessor.  To do this without branching, we use MMX'
	    // saturation feature, which gives us Z(x) = x if x>=0,
	    // or 0 if x<0.
	    //
	    // The formula we're computing here is
	    // Z(ST - (E1 - O) ^ 2) + Z(ST - (E2 - O) ^ 2)
	    // + Z(TT - (Oold - O) ^ 2)
	    // where ST is spatial tolerance and TT is temporal tolerance.
	    // The idea is that if a pixel is similar to none of its
	    // neighbors, the resulting value will be pretty low, probably
	    // zero.  A high value therefore indicates that the pixel had
	    // a similar neighbor.  The pixel in the same position in the
	    // field before last (Oold) is considered a neighbor since we
	    // want to be able to display 1-pixel-high horizontal lines.

	    // ST - (E1 - O) ^ 2, or 0 if that's negative
	    mm5 = vsr16 (vsubs16 (lum_E1, lum_O), 1);
	    mm7 = vsubsu16 (qwSpatialTolerance, vmullo16 (mm5, mm5));

	    // ST - (E2 - O) ^ 2, or 0 if that's negative
	    mm5 = vsr16 (vsubs16 (lum_E2, lum_O), 1);
	    mm3 = vsubsu16 (qwSpatialTolerance, vmullo16 (mm5, mm5));

	    mm7 = vaddsu16 (mm7, mm3);

	    lum_Oold = yuyv2yy (* (const vu8 *) YVal4);
	    YVal4 += sizeof (vu8);

	    // TT - (Oold - O) ^ 2, or 0 if that's negative
	    mm5 = vsr16 (vsubs16 (lum_Oold, lum_O), 1);
	    mm3 = vsubsu16 (qwTemporalTolerance, vmullo16 (mm5, mm5));

	    mm7 = vaddsu16 (mm7, mm3);

	    // Now compare the similarity totals against our threshold.
	    // The pcmpgtw instruction will populate the target register
	    // with a bunch of mask bits, filling words where the
	    // comparison is true with 1s and ones where it's false with
	    // 0s.  A few ANDs and NOTs and an OR later, we have bobbed
	    // values for pixels under the similarity threshold and weaved
	    // ones for pixels over the threshold.

	    vstorent ((vu8 *) Dest,
		      vsel (avg, O, vcmpgt16 (mm7, qwThreshold)));
	    Dest += sizeof (vu8);
	}

        YVal1 += src_padding;
        YVal2 += src_padding;
        YVal3 += src_padding;
        YVal4 += src_padding;
        Dest += dst_padding;
    }

    // Copy last odd line if we're processing an odd field.
    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
        copy_line (Dest, YVal2, byte_width);
    }

    vempty ();

    return TRUE;
}

#else /* !SIMD */

long TemporalTolerance = 300;
long SpatialTolerance = 600;
long SimilarityThreshold = 25;

////////////////////////////////////////////////////////////////////////////
// Start of Settings related code
/////////////////////////////////////////////////////////////////////////////
SETTING DI_VideoWeaveSettings[DI_VIDEOWEAVE_SETTING_LASTONE] =
{
    {
        N_("Temporal Tolerance"), SLIDER, 0, &TemporalTolerance,
        300, 0, 5000, 10, 1,
        NULL,
        "Deinterlace", "TemporalTolerance", NULL,
    },
    {
        N_("Spatial Tolerance"), SLIDER, 0, &SpatialTolerance,
        600, 0, 5000, 10, 1,
        NULL,
        "Deinterlace", "SpatialTolerance", NULL,
    },
    {
        N_("Similarity Threshold"), SLIDER, 0, &SimilarityThreshold,
        25, 0, 255, 1, 1,
        NULL,
        "Deinterlace", "SimilarityThreshold", NULL,
    },
};

DEINTERLACE_METHOD VideoWeaveMethod =
{
    sizeof(DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    N_("Video Deinterlace (Weave)"), 
    "Weave", 
    FALSE, 
    FALSE, 
    /* pfnAlgorithm */ NULL,
    50, 
    60,
    DI_VIDEOWEAVE_SETTING_LASTONE,
    DI_VideoWeaveSettings,
    INDEX_VIDEO_WEAVE,
    NULL,
    NULL,
    NULL,
    NULL,
    3,
    0,
    0,
    WM_DI_VIDEOWEAVE_GETVALUE - WM_APP,
    NULL,
    0,
    FALSE,
    FALSE,
    IDH_VIDEOWEAVE,
};


DEINTERLACE_METHOD* DI_VideoWeave_GetDeinterlacePluginInfo(long CpuFeatureFlags)
{
    VideoWeaveMethod.pfnAlgorithm = SIMD_SELECT (DeinterlaceFieldWeave);
    return &VideoWeaveMethod;
}

#endif /* !SIMD */

/*
Local Variables:
c-basic-offset: 4
End:
 */

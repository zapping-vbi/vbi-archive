/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_VideoWeave.c,v 1.6 2006-04-12 01:43:04 mschimek Exp $
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
// Revision 1.5  2005/06/28 19:17:10  mschimek
// *** empty log message ***
//
// Revision 1.4  2005/06/28 00:48:17  mschimek
// Cleaned up.
// Replaced longs by ints for proper operation on LP64 machines. Code
// assumes option values cast to int.
//
// Revision 1.3.2.4  2005/06/17 02:54:20  mschimek
// *** empty log message ***
//
// Revision 1.3.2.3  2005/05/31 02:40:34  mschimek
// *** empty log message ***
//
// Revision 1.3.2.2  2005/05/20 05:45:14  mschimek
// *** empty log message ***
//
// Revision 1.3.2.1  2005/05/05 09:46:01  mschimek
// *** empty log message ***
//
// Revision 1.3  2005/03/30 21:26:06  mschimek
// Integrated and converted the MMX code to vector intrinsics.
//
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
///////////////////////////////////////////////////////////////////////////*/

#include "windows.h"
#include "DS_Deinterlace.h"

extern int TemporalTolerance;
extern int SpatialTolerance;
extern int SimilarityThreshold;

SIMD_FN_PROTOS (DEINTERLACE_FUNC, DeinterlaceFieldWeave);

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW |			\
	    CPU_FEATURE_SSE_INT | CPU_FEATURE_SSE2 | CPU_FEATURE_ALTIVEC)

BOOL
SIMD_NAME (DeinterlaceFieldWeave) (TDeinterlaceInfo *	pInfo)
{
    vu16 qwSpatialTolerance;
    vu16 qwTemporalTolerance;
    vu16 qwThreshold;
    uint8_t *Dest;
    const uint8_t *YVal1;
    const uint8_t *YVal2;
    const uint8_t *YVal3;
    const uint8_t *YVal4;
    unsigned int byte_width;
    unsigned int height;
    unsigned long dst_padding;
    unsigned long src_padding;
    unsigned long dst_bpl;
    unsigned long src_bpl;

    if (SIMD == CPU_FEATURE_SSE2) {
	if (((unsigned long) pInfo->Overlay |
	     (unsigned long) pInfo->PictureHistory[0]->pData |
	     (unsigned long) pInfo->PictureHistory[1]->pData |
	     (unsigned long) pInfo->PictureHistory[2]->pData |
	     (unsigned long) pInfo->OverlayPitch |
	     (unsigned long) pInfo->InputPitch |
	     (unsigned long) pInfo->LineLength) & 15)
	    return DeinterlaceFieldWeave_SSE (pInfo);
    }

    qwSpatialTolerance = vsplatu16 (SpatialTolerance);
    qwTemporalTolerance = vsplatu16 (TemporalTolerance);
    qwThreshold = vsplatu16 (SimilarityThreshold);

    byte_width = pInfo->LineLength;

    dst_bpl = pInfo->OverlayPitch;
    src_bpl = pInfo->InputPitch;

    Dest = pInfo->Overlay;

    YVal1 = pInfo->PictureHistory[1]->pData;
    YVal2 = pInfo->PictureHistory[0]->pData;
    YVal3 = YVal1 + src_bpl;
    YVal4 = (const uint8_t *) pInfo->PictureHistory[2]->pData + src_bpl;

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_EVEN) {
        copy_line (Dest, YVal2, byte_width);
        Dest += dst_bpl;
        YVal2 += src_bpl;
    }

    copy_line (Dest, YVal1, byte_width);
    Dest += dst_bpl;

    dst_padding = dst_bpl * 2 - byte_width;
    src_padding = src_bpl - byte_width;

    for (height = pInfo->FieldHeight - 1; height > 0; --height) {
        unsigned int count;

        /* For ease of reading, the comments below assume that we're
	// operating on an odd field (i.e., that bIsOdd is true).  The
	// exact same processing is done when we operate on an even
	// field, but the roles of the odd and even fields are reversed.
        // It's just too cumbersome to explain the algorithm in terms
	// of "the next odd line if we're doing an odd field, or the
	// next even line if we're doing an even field" etc.  So
	// wherever you see "odd" or "even" below, keep in mind that
        // half the time this function is called, those words' meanings
	// will invert.
	*/

	for (count = byte_width / sizeof (vu8); count > 0; --count) {
	    vu8 E1, O, E2, avg, mm0;
	    v16 lum_E1, lum_O, lum_E2, mm5, lum_Oold;
	    vu16 mm3, mm7;

	    E1 = vload (YVal1, 0);
	    YVal1 += sizeof (vu8);
	    lum_E1 = yuyv2yy (E1);

	    E2 = vload (YVal3, 0);
	    YVal3 += sizeof (vu8);
	    lum_E2 = yuyv2yy (E2);

    	    /* Copy the even scanline below this one to the overlay buffer,
	       since we'll be adapting the current scanline to the even
	       lines surrounding it. */
	    vstorent (Dest, dst_bpl, E2);

	    /* Average E1 and E2 for interpolated bobbing. */
	    avg = fast_vavgu8 (E1, E2);

	    O = vload (YVal2, 0);
	    YVal2 += sizeof (vu8);
	    lum_O = yuyv2yy (O);

	    /* The meat of the work is done here.  We want to see
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
	    */

	    /* ST - (E1 - O) ^ 2, or 0 if that's negative */
	    mm5 = vsr16 (vsubs16 (lum_E1, lum_O), 1);
	    mm7 = vmullo16 (mm5, mm5);
	    mm7 = vsubsu16 (qwSpatialTolerance, mm7);

	    /* ST - (E2 - O) ^ 2, or 0 if that's negative */
	    mm5 = vsr16 (vsubs16 (lum_E2, lum_O), 1);
	    mm3 = vmullo16 (mm5, mm5);
	    mm3 = vsubsu16 (qwSpatialTolerance, mm3);

	    mm7 = vaddsu16 (mm7, mm3);

	    lum_Oold = yuyv2yy (vload (YVal4, 0));
	    YVal4 += sizeof (vu8);

	    /* TT - (Oold - O) ^ 2, or 0 if that's negative */
	    mm5 = vsr16 (vsubs16 (lum_Oold, lum_O), 1);
	    mm3 = vmullo16 (mm5, mm5);
	    mm3 = vsubsu16 (qwTemporalTolerance, mm3);

	    mm7 = vaddsu16 (mm7, mm3);

	    /* Now compare the similarity totals against our threshold.
	    // The pcmpgtw instruction will populate the target register
	    // with a bunch of mask bits, filling words where the
	    // comparison is true with 1s and ones where it's false with
	    // 0s.  A few ANDs and NOTs and an OR later, we have bobbed
	    // values for pixels under the similarity threshold and weaved
	    // ones for pixels over the threshold.
	    */

	    mm0 = (vu8) vcmpgt16 (mm7, qwThreshold);
	    mm0 = vsel (mm0, O, avg); 
	    vstorent (Dest, 0, mm0);
	    Dest += sizeof (vu8);
	}

        YVal1 += src_padding;
        YVal2 += src_padding;
        YVal3 += src_padding;
        YVal4 += src_padding;
        Dest += dst_padding;
    }

    /* Copy last odd line if we're processing an odd field. */
    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
        copy_line (Dest, YVal2, byte_width);
    }

    vempty ();

    return TRUE;
}

#elif !SIMD

int TemporalTolerance = 300;
int SpatialTolerance = 600;
int SimilarityThreshold = 25;

/*//////////////////////////////////////////////////////////////////////////
// Start of Settings related code
//////////////////////////////////////////////////////////////////////////*/
static const SETTING
DI_VideoWeaveSettings [] = {
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

static const DEINTERLACE_METHOD
VideoWeaveMethod = {
    sizeof (DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    N_("Video Deinterlace (Weave)"), 
    "Weave", 
    FALSE, 
    FALSE, 
    /* pfnAlgorithm */ NULL,
    50, 
    60,
    N_ELEMENTS (DI_VideoWeaveSettings),
    DI_VideoWeaveSettings,
    INDEX_VIDEO_WEAVE,
    NULL,
    NULL,
    NULL,
    NULL,
    3,
    0,
    0,
    0,
    NULL,
    0,
    FALSE,
    FALSE,
    IDH_VIDEOWEAVE,
};

DEINTERLACE_METHOD *
DI_VideoWeave_GetDeinterlacePluginInfo (void)
{
    DEINTERLACE_METHOD *m;
    DEINTERLACE_FUNC *f;

    m = NULL;

    f =	SIMD_FN_SELECT (DeinterlaceFieldWeave,
			CPU_FEATURE_MMX | CPU_FEATURE_3DNOW |
			CPU_FEATURE_SSE_INT | CPU_FEATURE_SSE2 |
			CPU_FEATURE_ALTIVEC);

    if (f) {
	m = malloc (sizeof (*m));
	*m = VideoWeaveMethod;

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

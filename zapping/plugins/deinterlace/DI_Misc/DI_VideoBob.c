/////////////////////////////////////////////////////////////////////////////
// $Id: DI_VideoBob.c,v 1.3 2005-03-30 21:26:32 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000 John Adcock.  All rights reserved.
// Based on code from Virtual Dub Plug-in by Gunnar Thalin
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
// Revision 1.2  2005/02/05 22:19:04  mschimek
// Completed l18n.
//
// Revision 1.1  2005/01/08 14:33:51  mschimek
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
// Revision 1.7  2002/06/18 19:46:07  adcockj
// Changed appliaction Messages to use WM_APP instead of WM_USER
//
// Revision 1.6  2002/06/13 12:10:25  adcockj
// Move to new Setings dialog for filers, video deint and advanced settings
//
// Revision 1.5  2001/07/26 11:53:08  adcockj
// Fix for crashing in VideoBob
//
// Revision 1.4  2001/07/13 16:13:33  adcockj
// Added CVS tags and removed tabs
//
/////////////////////////////////////////////////////////////////////////////

#include "windows.h"
#include "DS_Deinterlace.h"

extern long EdgeDetect;
extern long JaggieThreshold;

SIMD_PROTOS (DeinterlaceFieldBob);

#ifdef SIMD

///////////////////////////////////////////////////////////////////////////////
// DeinterlaceFieldBob
//
// Deinterlaces a field with a tendency to bob rather than weave.  Best for
// high-motion scenes like sports.
//
// The algorithm for this was taken from the 
// Deinterlace - area based Vitual Dub Plug-in by
// Gunnar Thalin
///////////////////////////////////////////////////////////////////////////////

BOOL
SIMD_NAME (DeinterlaceFieldBob)	(TDeinterlaceInfo *	pInfo)
{
    v16 qwEdgeDetect;
    v16 qwThreshold;
    uint8_t *Dest;
    const uint8_t *YVal1;
    const uint8_t *YVal2;
    const uint8_t *YVal3;
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
	     (unsigned int) pInfo->OverlayPitch |
	     (unsigned int) pInfo->InputPitch |
	     (unsigned int) pInfo->LineLength) & 15)
	    return DeinterlaceFieldBob__SSE (pInfo);
    }

    qwEdgeDetect = vsplat16 (EdgeDetect);
    qwThreshold = vsplat16 (JaggieThreshold);

    byte_width = pInfo->LineLength;

    dst_bpl = pInfo->OverlayPitch;
    src_bpl = pInfo->InputPitch;

    Dest = pInfo->Overlay;

    YVal1 = pInfo->PictureHistory[0]->pData;
    YVal2 = pInfo->PictureHistory[1]->pData;
    YVal3 = YVal1 + src_bpl;

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
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
	    vu8 O1, E, O2, avg;
	    v16 lum_O1, lum_E, lum_O2, mm6, mm7;

	    O1 = * (const vu8 *) YVal1;
	    YVal1 += sizeof (vu8);
	    lum_O1 = yuyv2yy (O1);

	    O2 = * (const vu8 *) YVal3;
	    YVal3 += sizeof (vu8);
	    lum_O2 = yuyv2yy (O2);

	    // Always use the most recent data verbatim.
	    vstorent ((vu8 *)(Dest + dst_bpl), O2);

	    avg = fast_vavgu8 (O1, O2);

	    E = * (const vu8 *) YVal2;
	    YVal2 += sizeof (vu8);
	    lum_E = yuyv2yy (E);

	    // work out (O1 - E) * (O2 - E) / 2
	    // - EdgeDetect * ((O1 - O2) ^ 2 >> 12)
	    // (the shifts prevent overflow)

	    lum_O1 = vsr16 (lum_O1, 1);
	    lum_E = vsr16 (lum_E, 1);
	    lum_O2 = vsr16 (lum_O2, 1);
	    mm6 = (v16) vmullo16 (vsub16 (lum_O1, lum_E),
				  vsub16 (lum_O2, lum_E));
	    mm7 = vsub16 (lum_O1, lum_O2);
	    mm7 = vsr16 ((v16) vmullo16 (mm7, mm7), 12);
	    mm7 = (v16) vmullo16 (mm7, qwEdgeDetect);
	    mm6 = vsub16 (mm6, mm7);

	    vstorent ((vu8 *) Dest,
		      vsel (E, avg, (vu8) vcmpgt16 (mm6, qwThreshold)));
	    Dest += sizeof (vu8);
	}
                        
	YVal1 += src_padding;
        YVal2 += src_padding;
        YVal3 += src_padding;
        Dest += dst_padding;
    }

    // Copy last odd line if we're processing an even field.
    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_EVEN) {
        copy_line (Dest, YVal2, byte_width);
    }

    vempty ();

    return TRUE;
}

#else /* !SIMD */

long EdgeDetect = 625;
long JaggieThreshold = 73;

////////////////////////////////////////////////////////////////////////////
// Start of Settings related code
/////////////////////////////////////////////////////////////////////////////
SETTING DI_VideoBobSettings[DI_VIDEOBOB_SETTING_LASTONE] =
{
    {
        N_("Weave Edge Detect"), SLIDER, 0, &EdgeDetect,
        625, 0, 10000, 5, 1,
        NULL,
        "Deinterlace", "EdgeDetect", NULL,
    },
    {
        N_("Weave Jaggie Threshold"), SLIDER, 0, &JaggieThreshold,
        73, 0, 5000, 5, 1,
        NULL,
        "Deinterlace", "JaggieThreshold", NULL,
    },
};

DEINTERLACE_METHOD VideoBobMethod =
{
    sizeof(DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    N_("Video Deinterlace (Bob)"), 
    "Bob",
    FALSE, 
    FALSE, 
    /* pfnAlgorithm */ NULL,
    50, 
    60,
    DI_VIDEOBOB_SETTING_LASTONE,
    DI_VideoBobSettings,
    INDEX_VIDEO_BOB,
    NULL,
    NULL,
    NULL,
    NULL,
    2,
    0,
    0,
    WM_DI_VIDEOBOB_GETVALUE - WM_APP,
    NULL,
    0,
    FALSE,
    FALSE,
    IDH_VIDEOBOB,
};

DEINTERLACE_METHOD* DI_VideoBob_GetDeinterlacePluginInfo(long CpuFeatureFlags)
{
    VideoBobMethod.pfnAlgorithm = SIMD_SELECT (DeinterlaceFieldBob);
    return &VideoBobMethod;
}

#endif /* !SIMD */

/*
Local Variables:
c-basic-offset: 4
End:
 */

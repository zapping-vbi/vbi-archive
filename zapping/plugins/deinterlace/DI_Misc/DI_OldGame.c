/////////////////////////////////////////////////////////////////////////////
// $Id: DI_OldGame.c,v 1.3 2005-03-30 21:27:19 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2001 Lindsey Dubb.  All rights reserved.
// based on OddOnly and Temporal Noise DScaler Plugins
// (c) John Adcock & Steve Grimm
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
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.2  2005/02/05 22:19:34  mschimek
// Completed l18n.
//
// Revision 1.1  2005/01/08 14:34:43  mschimek
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
// Revision 1.6  2002/06/13 12:10:24  adcockj
// Move to new Setings dialog for filers, video deint and advanced settings
//
// Revision 1.5  2002/01/04 01:38:56  lindsey
// Turned off the debug flag
//
// Revision 1.4  2001/12/20 03:42:58  lindsey
// Corrected processor detection
// Miscellaneous reorganization
//
// Revision 1.3  2001/11/21 15:21:40  adcockj
// Renamed DEINTERLACE_INFO to TDeinterlaceInfo in line with standards
// Changed TDeinterlaceInfo structure to have history of pictures.
//
// Revision 1.2  2001/08/30 10:03:52  adcockj
// Slightly improved the color averaging
// Added a "composite mode" switch to force averaging when crosstalk is more important than blur.
// Commented the code
// Reorganized and edited to follow the coding guidelines
// Most importantly: Added a silly quote
// (Changes made on behalf of Lindsey Dubb)
//
// Revision 1.1  2001/07/30 08:25:22  adcockj
// Added Lindsey Dubb's method
//
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>		/* snprintf */
#include "windows.h"
#include "DS_Deinterlace.h"

// turn on display of the comb factor
#define OLDGAME_DEBUG 0

/////////////////////////////////////////////////////////////////////////////
/*
Up and down, up and down.  Pardon me, but my lunch wants to join the sea.
                                            - from Shining Force

This is the Old Game "deinterlacing" method.  More accurately, it's the Old
Game nondeinterlacing method, which circumvents the deinterlacing done in the
other algorithms.

Specifically:
It assumes that the image is half vertical resolution and progressive, and
therefore shouldn't be deinterlaced.  In addition, it averages when there
isn't any detected motion in order to clean up noise a bit. This works well
for games which run at less than a full 50 or 60 FPS.  It's downright
necessary if the game is connected via composite, since video games tend
to have awful chroma/luma crosstalk.

Ways this filter could be improved:
- The Playstation, Super Nintendo, Saturn, and Nintendo 64 all can switch
  between an interlaced and a progressive half vertical resolution mode.
  It would be nice to be able to automatically detect this and forward the
  deinterlacing on to a real deinterlacing algorithm.  But this would
  require DScaler to keep checking whether the vertical resolution has
  changed, which it currently doesn't.
- Potentially, you could also try to infer the horizontal resolution,
  deconvolve the image, and smooth it more optimally.  I really doubt
  it would be worth the effort.
- The difference between fields could be used in composite mode to infer
  motion.  But it often makes sense to average even when there is some
  30 FPS motion, so this wouldn't be easy.
*/
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Function prototypes
/////////////////////////////////////////////////////////////////////////////

extern DEINTERLACEPLUGINSETSTATUS*  gPfnSetStatus;
extern long                         gDisableMotionChecking;
extern long                         gMaxComb;

SIMD_PROTOS (OldGameFilter)

#ifdef SIMD

BOOL
SIMD_NAME (OldGameFilter)	(TDeinterlaceInfo *	pInfo)
{
    if (OLDGAME_DEBUG) {
        char OutputString[64];

        snprintf (OutputString, sizeof (OutputString),
		  "Motion %u", (unsigned int) pInfo->CombFactor);
        if (gPfnSetStatus != NULL) {
            gPfnSetStatus (OutputString);
        }
    }

    // If the field is significantly different from the previous one,
    // show the new frame unaltered.
    // This is just a tiny change on the evenOnly/oddOnly filters

    if (!pInfo->PictureHistory[0]) {
        return FALSE;
    }

    if (!pInfo->PictureHistory[1]
        || (gDisableMotionChecking == FALSE
	    && pInfo->CombFactor > gMaxComb)) {
        BYTE* pThisLine = pInfo->PictureHistory[0]->pData;
        DWORD LineTarget = 0;

        if (pThisLine == NULL) {
            return TRUE;
        }
        for (LineTarget = 0;
	     LineTarget < (DWORD) pInfo->FieldHeight; LineTarget++) {
            // copy latest field's rows to overlay,
	    // resulting in a half-height image.
            copy_line (pInfo->Overlay + LineTarget * pInfo->OverlayPitch,
		       pThisLine,
		       pInfo->LineLength);

            pThisLine += pInfo->InputPitch;
        }
    } else {
        BYTE*           pNewLines = pInfo->PictureHistory[0]->pData;
        BYTE*           pDestination = pInfo->Overlay;
        DWORD           LineTarget = 0;
        BYTE*           pOldLines = pInfo->PictureHistory[1]->pData;

	// If the field is very similar to the last one, average them.
	// This code is a cut down version of Steven Grimm's temporal
	// noise filter.  It does a really nice job on video via a
	// composite connector.

        if ((pNewLines == NULL) || (pOldLines == NULL)) {
            return TRUE;
        }

        for (LineTarget = 0;
	     LineTarget < (DWORD) pInfo->FieldHeight; ++LineTarget)
        {
	    uint8_t *Dest = (uint8_t *) pDestination;
	    const uint8_t *New = (const uint8_t *) pNewLines;
	    const uint8_t *Old = (const uint8_t *) pOldLines;
	    unsigned int count;

	    for (count = pInfo->LineLength / sizeof (*Dest);
		 count > 0; --count) {
		vstorent ((vu8 *) Dest, fast_vavgu8 (* (const vu8 *) New,
						     * (const vu8 *) Old));
		New += sizeof (vu8);
		Old += sizeof (vu8);
		Dest += sizeof (vu8);
	    }

            pDestination += pInfo->OverlayPitch;
            pNewLines += pInfo->InputPitch;
            pOldLines += pInfo->InputPitch;
        }
    }

    vempty ();

    return TRUE;
}

#else /* !SIMD */

/////////////////////////////////////////////////////////////////////////////
// Begin plugin globals
/////////////////////////////////////////////////////////////////////////////

// gMaxComb is compared to the comb factor to determine if the image has
// enough motion to force us to send the image to the screen unaltered
// instead of averaging witht he previous image.
long                         gMaxComb = 300;

// When using a composite connector, crosstalk is bad enough that it's
// (almost) always worth averaging.
long                         gDisableMotionChecking = FALSE;

// This is used to put up the comb factor for testing purposes.
DEINTERLACEPLUGINSETSTATUS*  gPfnSetStatus = NULL;

static DEINTERLACE_METHOD OldGameMethod;


static SETTING DI_OldGameSettings[DI_OLDGAME_SETTING_LASTONE] =
{
    {
        N_("Maximum motion"), SLIDER, 0, &gMaxComb,
        300, 1, 5000, 1, 1,
        NULL,
        "DI_OldGame", "maxComb", NULL,
    },
    {
        N_("Composite mode"), ONOFF, 0, &gDisableMotionChecking,
        FALSE, 0, 1, 1, 1,
        NULL,
        "DI_OldGame", "CompositeMode", NULL,
    }
};

static DEINTERLACE_METHOD OldGameMethod =
{
    sizeof(DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    N_("Old Game"), 
    NULL,     // It could make sense to use this with a resolution sensing adaptive filter
    TRUE, 
    FALSE, 
    NULL, 
    50, 
    60,
    DI_OLDGAME_SETTING_LASTONE,
    DI_OldGameSettings,
    INDEX_OLD_GAME,
    NULL,
#if OLDGAME_DEBUG
    OldGameDebugStart,
#else
    NULL,
#endif
    NULL,
    NULL,
    1,
    0,
    0,
    -1,
    NULL,
    0,
    FALSE,
    TRUE,
    IDH_OLD_GAME,
};

////////////////////////////////////////////////////////////////////////////
// Start of utility code
/////////////////////////////////////////////////////////////////////////////

#if OLDGAME_DEBUG
void __cdecl OldGameDebugStart(long NumPlugIns, DEINTERLACE_METHOD** OtherPlugins, DEINTERLACEPLUGINSETSTATUS* SetStatus)
{
    gPfnSetStatus = SetStatus;
}
#endif  // OLDGAME_DEBUG

DEINTERLACE_METHOD* DI_OldGame_GetDeinterlacePluginInfo(long CpuFeatureFlags)
{
    OldGameMethod.pfnAlgorithm = SIMD_SELECT (OldGameFilter);
    return &OldGameMethod;
}

#endif /* !SIMD */

/*
Local Variables:
c-basic-offset: 4
End:
 */

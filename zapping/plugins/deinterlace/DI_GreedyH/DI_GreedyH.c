/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_GreedyH.c,v 1.4 2005-07-29 17:39:29 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2001 Tom Barry.  All rights reserved.
// Copyright (C) 2005 Michael H. Schimek
/////////////////////////////////////////////////////////////////////////////
//
//	This file is subject to the terms of the GNU General Public License as
//	published by the Free Software Foundation.  A copy of this license is
//	included with this software distribution in the file COPYING.  If you
//	do not have a copy, you may obtain a copy by writing to the Free
//	Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//	This software is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////
// Change Log
//
// Date          Developer             Changes
//
// 16 Jan 2001   Tom Barry	       Added GreedyH Deinterlace method
//
/////////////////////////////////////////////////////////////////////////////
//
// GreedyH.c is basically just a wrapper for the new Greedy (High Motion)
// deinterlace method. This member handles all, or most of, the dependencies
// on Windows, or the DScaler environment. That includes the User Interface
// stuff to adjust parms or view the diagnostic pulldown trace.
//
// For details of the actual deinterlace algorithm, see member DI_GreedyHM.c.
//
// For details of the pulldown handling, see member DI_GreedyHMPulldown.c.
//
//////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.3  2005/06/28 00:47:12  mschimek
// Converted to vector intrinsics. Added support for 3DNow, SSE2, x86-64
// and AltiVec. Removed ununsed DScaler code. Cleaned up. All options work
// now.
//
// Revision 1.2.2.4  2005/06/17 02:54:20  mschimek
// *** empty log message ***
//
// Revision 1.2.2.3  2005/05/20 05:45:14  mschimek
// *** empty log message ***
//
// Revision 1.2.2.2  2005/05/17 19:58:32  mschimek
// *** empty log message ***
//
// Revision 1.2.2.1  2005/05/05 09:46:00  mschimek
// *** empty log message ***
//
// Revision 1.2  2005/02/05 22:21:06  mschimek
// Completed l18n.
//
// Revision 1.1  2005/01/08 14:35:29  mschimek
// TomsMoCompMethod, MoComp2Method, VideoWeaveMethod, VideoBobMethod,
// TwoFrameMethod, OldGameMethod, Greedy2FrameMethod, GreedyMethod,
// DI_GreedyHSettings: Localized.
//
// Revision 1.16  2002/06/18 19:46:07  adcockj
// Changed appliaction Messages to use WM_APP instead of WM_USER
//
// Revision 1.15  2002/06/13 12:10:24  adcockj
// Move to new Setings dialog for filers, video deint and advanced settings
//
// Revision 1.14  2001/11/25 04:33:37  trbarry
// Fix for TDeinterlace_Info. Also release UN-Filter code, 5-tap
// V & H sharp/soft filters optimized to reverse excessive filtering (or EE?)
//
// Revision 1.13  2001/11/21 15:21:40  adcockj
// Renamed DEINTERLACE_INFO to TDeinterlaceInfo in line with standards
// Changed TDeinterlaceInfo structure to have history of pictures.
//
// Revision 1.12  2001/11/02 10:46:09  adcockj
// Merge in code from Multiple card branch
//
// Revision 1.11  2001/08/21 09:39:12  adcockj
// Removed references to deleted file
//
// Revision 1.10  2001/08/19 06:26:38  trbarry
// Remove Greedy HM Low Motion Only option and files
//
// No longer needed
//
// Revision 1.9  2001/08/17 16:18:35  trbarry
// Minor GreedyH performance Enh.
// Only do pulldown calc when needed.
// Will become more needed in future when calc more expensive.
//
// Revision 1.10  2001/08/19 06:26:38  trbarry
// Remove Greedy HM Low Motion Only option and files
//
// No longer needed
//
// Revision 1.9  2001/08/17 16:18:35  trbarry
// Minor GreedyH performance Enh.
// Only do pulldown calc when needed.
// Will become more needed in future when calc more expensive.
//
// Revision 1.8  2001/08/04 06:46:56  trbarry
// Make Gui work with Large fonts,
// Improve Pulldown
//
// Revision 1.7  2001/08/01 00:37:41  trbarry
// More chroma jitter fixes, tweak defaults
//
// Revision 1.6  2001/07/30 21:50:32  trbarry
// Use weave chroma for reduced chroma jitter. Fix DJR bug again.
// Turn off Greedy Pulldown default.
//
// Revision 1.5  2001/07/30 17:56:26  trbarry
// Add Greedy High Motion MMX, K6-II, K6-III, and Celeron support.
// Tweak defaults.
//
// Revision 1.4  2001/07/28 18:47:24  trbarry
// Fix Sharpness with Median Filter
// Increase Sharpness default to make obvious
// Adjust deinterlace defaults for less jitter
//
// Revision 1.3  2001/07/26 02:42:10  trbarry
// Recognize Athlon CPU
//
// Revision 1.2  2001/07/25 12:04:31  adcockj
// Moved Control stuff into DS_Control.h
// Added $Id and $Log to comment blocks as per standards
//
///////////////////////////////////////////////////////////////////////////*/

#include "windows.h"
#include "DS_Deinterlace.h"
#include "DI_GreedyHM.h"

/*//////////////////////////////////////////////////////////////////////////
// Start of Settings related code
//////////////////////////////////////////////////////////////////////////*/

static BOOL
DeinterlaceGreedyH		(TDeinterlaceInfo *	pInfo)
{
    pInfo = pInfo;

#if defined (HAVE_ALTIVEC)
    if (cpu_features & CPU_FEATURE_ALTIVEC) {
	if (GreedyUseMedianFilter |
	    GreedyUsePulldown |
	    GreedyUseVSharpness |
	    GreedyUseHSharpness) {
	    return DI_GreedyHM_ALTIVEC (pInfo);
	} else {
	    return DI_GreedyHF_ALTIVEC (pInfo);
	}
    } else
#endif

#if defined (HAVE_SSE3)
    if (cpu_features & CPU_FEATURE_SSE3) {
	if (GreedyUseMedianFilter |
	    GreedyUsePulldown |
	    GreedyUseVSharpness |
	    GreedyUseHSharpness) {
	    return DI_GreedyHM_SSE3 (pInfo);
	} else {
	    return DI_GreedyHF_SSE3 (pInfo);
	}
    } else
#endif

#if defined (HAVE_SSE2)
    if (cpu_features & CPU_FEATURE_SSE2) {
	if (GreedyUseMedianFilter |
	    GreedyUsePulldown |
	    GreedyUseVSharpness |
	    GreedyUseHSharpness) {
	    return DI_GreedyHM_SSE2 (pInfo);
	} else {
	    return DI_GreedyHF_SSE2 (pInfo);
	}
    } else
#endif

#if defined (HAVE_SSE)
    if (cpu_features & CPU_FEATURE_SSE) {
	if (GreedyUseMedianFilter |
	    GreedyUsePulldown |
	    GreedyUseVSharpness |
	    GreedyUseHSharpness) {
	    return DI_GreedyHM_SSE (pInfo);
	} else {
	    return DI_GreedyHF_SSE (pInfo);
	}
    } else
#endif

    /* Test mode: enable expensive features for tests. */

#if defined (HAVE_3DNOW)
    if (cpu_features & CPU_FEATURE_3DNOW) {
	if (GreedyTestMode
	    && (GreedyUseMedianFilter |
		GreedyUsePulldown |
		GreedyUseVSharpness |
		GreedyUseHSharpness)) {
	    return DI_GreedyHM_3DNOW (pInfo);
	} else {
	    return DI_GreedyHF_3DNOW (pInfo);
	}
    } else
#endif

#if defined (HAVE_MMX)
    if (cpu_features & CPU_FEATURE_MMX) {
	if (GreedyTestMode
	    && (GreedyUseMedianFilter |
		GreedyUsePulldown |
		GreedyUseVSharpness |
		GreedyUseHSharpness)) {
	    return DI_GreedyHM_MMX (pInfo);
	} else {
	    return DI_GreedyHF_MMX (pInfo);
	}
    } else
#endif
	return FALSE;
}

static const SETTING
DI_GreedyHSettings [] = {
    {
	N_("Max Comb"), SLIDER, 0, /* szDisplayName, TYPE, orig val */
	&GreedyHMaxComb, 5, 0,	   /* *pValue, Default, Min */
	255, 1, 1,		   /* Max, Step, OSDDivider */
	NULL, "Deinterlace",	   /* **pszList, Ini Section */
	"GreedyMaxComb", NULL,	   /* Ini name, pfnOnChange */
    }, {
	N_("Motion Threshold"), SLIDER, 0,
	&GreedyMotionThreshold, 25, 0,
	255, 1, 1,
	NULL, "Deinterlace",
	"GreedyMotionThreshold", NULL,
    }, {
	N_("Motion Sense"), SLIDER, 0,
	&GreedyMotionSense, 30, 0,
	255, 1, 1,
	NULL, "Deinterlace",
	"GreedyMotionSense", NULL,
    }, {
	N_("Good PullDown Lvl"), SLIDER, 0,
	&GreedyGoodPullDownLvl, 83, 0,
	255, 1, 1,
	NULL, "Deinterlace",
	"GreedyGoodPullDownLvl", NULL,
    }, {
	N_("Bad PullDown Lvl"), SLIDER, 0,
	&GreedyBadPullDownLvl, 88, 0,
	255, 1, 1,
	NULL, "Deinterlace",
	"GreedyBadPullDownLvl", NULL,
    }, {
	N_("H. Sharpness"), SLIDER, 0,
	&GreedyHSharpnessAmt, 50, -100,
	100, 1, 1,
	NULL, "Deinterlace",
	"GreedyHSharpnessAmt", NULL,
    }, {
	N_("V. Sharpness"), SLIDER, 0,
	&GreedyVSharpnessAmt, 23, -100,
	100, 1, 1,
	NULL, "Deinterlace",
	"GreedyVHSharpnessAmt", NULL,
    }, {
	N_("Median Filter"), SLIDER, 0,
	&GreedyMedianFilterAmt, 5, 0,
	255, 1, 1,
	NULL, "Deinterlace",
	"GreedyMedianFilterAmt", NULL,
    }, {
	N_("High Comb Skip"), SLIDER, 0,
	&GreedyLowMotionPdLvl, 0, 0,
	100, 1, 1,
	NULL, "Deinterlace",
	"GreedyLowMotionPdLvl", NULL,
    }, {
	N_("Auto Pull-Down"), ONOFF, 0,
	&GreedyUsePulldown, FALSE, 0,
	1, 1, 1,
	NULL, "Deinterlace",
	"GreedyUsePulldown", NULL,
    }, {
	N_("In-Between Frames"), ONOFF, 0,
	&GreedyUseInBetween, FALSE, 0,
	1, 1, 1,
	NULL, "Deinterlace",
	"GreedyUseInBetween", NULL,
    }, {
	N_("Median Filter"), ONOFF, 0,
	&GreedyUseMedianFilter, FALSE, 0,
	1, 1, 1,
	NULL, "Deinterlace",
	"GreedyUseMedianFilter", NULL,
    }, {
	N_("V. Sharpness"), ONOFF, 0,
	&GreedyUseVSharpness, FALSE, 0,
	1, 1, 1,
	NULL, "Deinterlace",
	"GreedyUseVSharpness", NULL,
    }, {
	N_("H. Sharpness"), ONOFF, 0,
	&GreedyUseHSharpness, FALSE, 0,
	1, 1, 1,
	NULL, "Deinterlace",
	"GreedyUseHSharpness", NULL,
    }, {
	NULL, ONOFF, 0,
	&GreedyTestMode, FALSE, 0,
	1, 1, 1,
	NULL, "Deinterlace",
	"GreedyTestMode", NULL,
    },
};

static const DEINTERLACE_METHOD
GreedyHMethod = {
    sizeof (DEINTERLACE_METHOD),	/* size of this struct */
    DEINTERLACE_CURRENT_VERSION,	/* curr version compiled */
    N_("Video (Greedy, High Motion)"),  /* What to display when selected */
    "GreedyH",				/* Short name */
    FALSE,				/* Is 1/2 height? */
    FALSE,				/* Is film mode? */
    DeinterlaceGreedyH,			/* Pointer to Algorithm function */
    50,					/* flip frequency in 50Hz mode */
    60,					/* flip frequency in 60Hz mode */
    N_ELEMENTS (DI_GreedyHSettings),	/* number of settings */
    DI_GreedyHSettings,		/* ptr to start of Settings[nSettings] */
    INDEX_VIDEO_GREEDYH,	/* Index Number (pos. in menu) should map*/ 
    NULL,			/* to old enum value and d should be unique */
    0, 0, 0,
    3,		/* how many fields are required to run this plug-in */
    0,		/* Track number of mode Changes */
    0,		/* Track Time in mode */
    0,    /* the offset used by the external settings API */
    NULL, /* Dll module so that we can unload the dll cleanly at the end */
    0,    /* Menu Id used for this plug-in, 0 to auto allocate one */
    FALSE,	/* do we need FieldDiff filled in in info */
    FALSE,	/* do we need CombFactor filled in in info */
    IDH_GREEDYHM,
};

DEINTERLACE_METHOD *
DI_GreedyH_GetDeinterlacePluginInfo (void)
{
    DEINTERLACE_METHOD *m;
    DEINTERLACE_FUNC *f;

    m = NULL;
    f = NULL;

#if defined (HAVE_ALTIVEC)
    if (cpu_features & CPU_FEATURE_ALTIVEC)
	f = DeinterlaceGreedyH;
#endif
#if defined (HAVE_SSE3)
    if (cpu_features & CPU_FEATURE_SSE3)
	f = DeinterlaceGreedyH;
#endif
#if defined (HAVE_SSE2)
    if (cpu_features & CPU_FEATURE_SSE2)
	f = DeinterlaceGreedyH;
#endif
#if defined (HAVE_SSE)
    if (cpu_features & CPU_FEATURE_SSE)
	f = DeinterlaceGreedyH;
#endif
#if defined (HAVE_3DNOW)
    if (cpu_features & CPU_FEATURE_3DNOW)
	f = DeinterlaceGreedyH;
#endif
#if defined (HAVE_MMX)
    if (cpu_features & CPU_FEATURE_MMX)
       	f = DeinterlaceGreedyH;
#endif

    if (f) {
	m = malloc (sizeof (*m));
	*m = GreedyHMethod;

	m->pfnAlgorithm = f;
    }

    return m;
}

/*
Local Variables:
c-basic-offset: 4
End:
 */

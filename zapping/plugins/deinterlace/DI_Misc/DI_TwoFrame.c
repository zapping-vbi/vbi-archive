///////////////////////////////////////////////////////////////////////////////
// $Id: DI_TwoFrame.c,v 1.2 2005-02-05 22:19:14 mschimek Exp $
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
//Z #include "..\help\helpids.h"

long TwoFrameTemporalTolerance = 300;
long TwoFrameSpatialTolerance = 600;

#define IS_SSE 1
#include "DI_TwoFrame.asm"
#undef IS_SSE

#define IS_3DNOW 1
#include "DI_TwoFrame.asm"
#undef IS_3DNOW

#define IS_MMX 1
#include "DI_TwoFrame.asm"
#undef IS_MMX

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
    DeinterlaceFieldTwoFrame_MMX, 
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
    if (CpuFeatureFlags & FEATURE_SSE)
    {
        TwoFrameMethod.pfnAlgorithm = DeinterlaceFieldTwoFrame_SSE;
    }
    else if (CpuFeatureFlags & FEATURE_3DNOW)
    {
        TwoFrameMethod.pfnAlgorithm = DeinterlaceFieldTwoFrame_3DNOW;
    }
    else
    {
        TwoFrameMethod.pfnAlgorithm = DeinterlaceFieldTwoFrame_MMX;
    }
    return &TwoFrameMethod;
}

#if 0


BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

#endif /* 0 */

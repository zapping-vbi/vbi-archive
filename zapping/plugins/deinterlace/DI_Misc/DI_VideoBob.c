/////////////////////////////////////////////////////////////////////////////
// $Id: DI_VideoBob.c,v 1.1 2005-01-08 14:33:51 mschimek Exp $
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
//Z #include "..\help\helpids.h"

long EdgeDetect = 625;
long JaggieThreshold = 73;

#define IS_SSE 1
#include "DI_VideoBob.asm"
#undef IS_SSE

#define IS_3DNOW 1
#include "DI_VideoBob.asm"
#undef IS_3DNOW

#define IS_MMX 1
#include "DI_VideoBob.asm"
#undef IS_MMX

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
    "Video Deinterlace (Bob)", 
    "Bob",
    FALSE, 
    FALSE, 
    DeinterlaceFieldBob_MMX, 
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
    if (CpuFeatureFlags & FEATURE_SSE)
    {
        VideoBobMethod.pfnAlgorithm = DeinterlaceFieldBob_SSE;
    }
    else if (CpuFeatureFlags & FEATURE_3DNOW)
    {
        VideoBobMethod.pfnAlgorithm = DeinterlaceFieldBob_3DNOW;
    }
    else
    {
        VideoBobMethod.pfnAlgorithm = DeinterlaceFieldBob_MMX;
    }
    return &VideoBobMethod;
}

#if 0


BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

#endif /* 0 */

/////////////////////////////////////////////////////////////////////////////
// $Id: DI_Greedy.c,v 1.2 2005-02-05 22:20:03 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2001 Tom Barry.  All rights reserved.
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
/////////////////////////////////////////////////////////////////////////////

#include "windows.h"
#include "DS_Deinterlace.h"
//Z #include "..\help\helpids.h"

long GreedyMaxComb = 15;

#define IS_SSE 1
#include "DI_Greedy.asm"
#undef IS_SSE

#define IS_3DNOW 1
#include "DI_Greedy.asm"
#undef IS_3DNOW

#define IS_MMX 1
#include "DI_Greedy.asm"
#undef IS_MMX

////////////////////////////////////////////////////////////////////////////
// Start of Settings related code
/////////////////////////////////////////////////////////////////////////////
SETTING DI_GreedySettings[DI_GREEDY_SETTING_LASTONE] =
{
    {
        N_("Greedy Max Comb"), SLIDER, 0, &GreedyMaxComb,
        15, 0, 255, 1, 1,
        NULL,
        "Deinterlace", "GreedyMaxComb", NULL,
    },
};

DEINTERLACE_METHOD GreedyMethod =
{
    sizeof(DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    N_("Video (Greedy, Low Motion)"), 
    "Greedy",
    FALSE, 
    FALSE, 
    DeinterlaceGreedy_MMX, 
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
    if (CpuFeatureFlags & FEATURE_SSE)
    {
        GreedyMethod.pfnAlgorithm = DeinterlaceGreedy_SSE;
    }
    else if (CpuFeatureFlags & FEATURE_3DNOW)
    {
        GreedyMethod.pfnAlgorithm = DeinterlaceGreedy_3DNOW;
    }
    else
    {
        GreedyMethod.pfnAlgorithm = DeinterlaceGreedy_MMX;
    }
    return &GreedyMethod;
}

#if 0


BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

#endif /* 0 */

/////////////////////////////////////////////////////////////////////////////
// $Id: DI_EvenOnly.c,v 1.2 2005-02-05 22:20:18 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000 John Adcock.  All rights reserved.
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
// 04 Jan 2001   John Adcock           Split into separate module
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.1  2005/01/08 14:54:23  mschimek
// *** empty log message ***
//
// Revision 1.2  2004/11/15 23:03:19  michael
// *** empty log message ***
//
// Revision 1.1  2004/11/14 15:35:14  michael
// *** empty log message ***
//
// Revision 1.7  2002/06/13 12:10:24  adcockj
// Move to new Setings dialog for filers, video deint and advanced settings
//
// Revision 1.6  2001/11/22 13:32:03  adcockj
// Finished changes caused by changes to TDeinterlaceInfo - Compiles
//
// Revision 1.5  2001/11/21 15:21:40  adcockj
// Renamed DEINTERLACE_INFO to TDeinterlaceInfo in line with standards
// Changed TDeinterlaceInfo structure to have history of pictures.
//
// Revision 1.4  2001/07/13 16:13:33  adcockj
// Added CVS tags and removed tabs
//
/////////////////////////////////////////////////////////////////////////////

#include "windows.h"
#include "DS_Deinterlace.h"
//Z #include "..\help\helpids.h"

BOOL DeinterlaceEvenOnly(TDeinterlaceInfo* pInfo)
{
    int nLineTarget;
    BYTE* CurrentLine = pInfo->PictureHistory[0]->pData;

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_EVEN)
    {
        for (nLineTarget = 0; nLineTarget < pInfo->FieldHeight; nLineTarget++)
        {
            // copy latest field's rows to overlay, resulting in a half-height image.
            pInfo->pMemcpy(pInfo->Overlay + nLineTarget * pInfo->OverlayPitch,
                        CurrentLine,
                        pInfo->LineLength);
            CurrentLine += pInfo->InputPitch;
        }
        // need to clear up MMX registers
        _asm
        {
            emms
        }
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


DEINTERLACE_METHOD EvenOnlyMethod =
{
    sizeof(DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    N_("Even Scanlines Only"), 
    "Even",
    TRUE, 
    FALSE, 
    DeinterlaceEvenOnly, 
    25, 
    30,
    0,
    NULL,
    INDEX_EVEN_ONLY,
    NULL,
    NULL,
    NULL,
    NULL,
    1,
    0,
    0,
    -1,
    NULL,
    0,
    FALSE,
    FALSE,
    IDH_EVEN,
};


DEINTERLACE_METHOD* DI_EvenOnly_GetDeinterlacePluginInfo(long CpuFeatureFlags)
{
    return &EvenOnlyMethod;
}

#if 0


BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}



#endif /* 0 */

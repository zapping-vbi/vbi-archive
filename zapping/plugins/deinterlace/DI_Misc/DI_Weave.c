/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_Weave.c,v 1.2.2.3 2005-05-31 02:40:34 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000 John Adcock.  All rights reserved.
// Copyright (C) 2005 Michael Schimek
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
// Revision 1.2.2.2  2005/05/20 05:45:14  mschimek
// *** empty log message ***
//
// Revision 1.2.2.1  2005/05/05 09:46:01  mschimek
// *** empty log message ***
//
// Revision 1.2  2005/02/05 22:18:36  mschimek
// Completed l18n.
//
// Revision 1.1  2005/01/08 14:54:23  mschimek
// *** empty log message ***
//
// Revision 1.2  2004/11/15 23:03:19  michael
// *** empty log message ***
//
// Revision 1.1  2004/11/14 15:35:15  michael
// *** empty log message ***
//
// Revision 1.8  2002/06/13 12:10:25  adcockj
// Move to new Setings dialog for filers, video deint and advanced settings
//
// Revision 1.7  2001/11/22 13:32:04  adcockj
// Finished changes caused by changes to TDeinterlaceInfo - Compiles
//
// Revision 1.6  2001/11/21 15:21:41  adcockj
// Renamed DEINTERLACE_INFO to TDeinterlaceInfo in line with standards
// Changed TDeinterlaceInfo structure to have history of pictures.
//
// Revision 1.5  2001/07/13 16:13:33  adcockj
// Added CVS tags and removed tabs
//
///////////////////////////////////////////////////////////////////////////*/

#include "windows.h"
#include "DS_Deinterlace.h"

SIMD_FN_PROTOS (DEINTERLACE_FUNC, DeinterlaceWeave);

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_SSE |				\
	    CPU_FEATURE_SSE2 | CPU_FEATURE_ALTIVEC)

/*///////////////////////////////////////////////////////////////////////////
// Simple Weave.  Copies alternating scanlines from the most recent fields.
///////////////////////////////////////////////////////////////////////////*/
BOOL
SIMD_NAME (DeinterlaceWeave)	(TDeinterlaceInfo *	pInfo)
{
    int i;
    BYTE *lpOverlay = pInfo->Overlay;
    BYTE* CurrentOddLine;
    BYTE* CurrentEvenLine;
    DWORD Pitch = pInfo->InputPitch;

    if (SIMD == CPU_FEATURE_SSE2) {
	if ((INTPTR (pInfo->Overlay) |
	     INTPTR (pInfo->PictureHistory[0]->pData) |
	     INTPTR (pInfo->PictureHistory[1]->pData) |
	     (unsigned long) pInfo->OverlayPitch |
	     (unsigned long) pInfo->InputPitch |
	     (unsigned long) pInfo->LineLength) & 15)
	    return DeinterlaceWeave_SSE (pInfo);
    }

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
        CurrentOddLine = pInfo->PictureHistory[0]->pData;
        CurrentEvenLine = pInfo->PictureHistory[1]->pData;
    } else {
        CurrentOddLine = pInfo->PictureHistory[1]->pData;
        CurrentEvenLine = pInfo->PictureHistory[0]->pData;
    }
    
    for (i = 0; i < pInfo->FieldHeight; i++) {
        copy_line (lpOverlay, CurrentEvenLine, pInfo->LineLength);
        lpOverlay += pInfo->OverlayPitch;
        CurrentEvenLine += Pitch;

        copy_line (lpOverlay, CurrentOddLine, pInfo->LineLength);
        lpOverlay += pInfo->OverlayPitch;
        CurrentOddLine += Pitch;
    }

    vempty ();

    return TRUE;
}

#elif !SIMD

const DEINTERLACE_METHOD WeaveMethod =
{
    sizeof(DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    N_("Simple Weave"), 
    "Weave",
    FALSE, 
    FALSE, 
    /* pfnAlgorithm */ NULL,
    50, 
    60,
    0,
    NULL,
    INDEX_WEAVE,
    NULL,
    NULL,
    NULL,
    NULL,
    2,
    0,
    0,
    -1,
    NULL,
    0,
    FALSE,
    FALSE,
    IDH_WEAVE,
};

DEINTERLACE_METHOD *
DI_Weave_GetDeinterlacePluginInfo (void)
{
    DEINTERLACE_METHOD *m;

    m = malloc (sizeof (*m));
    *m = WeaveMethod;

    m->pfnAlgorithm =
	SIMD_FN_SELECT (DeinterlaceWeave,
			CPU_FEATURE_MMX | CPU_FEATURE_SSE |
			CPU_FEATURE_SSE2 | CPU_FEATURE_ALTIVEC);

    return m;
}

#endif /* !SIMD */

/*
Local Variables:
c-basic-offset: 4
End:
 */

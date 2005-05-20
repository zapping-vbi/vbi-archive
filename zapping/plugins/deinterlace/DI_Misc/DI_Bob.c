/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_Bob.c,v 1.2.2.2 2005-05-20 05:45:14 mschimek Exp $
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
// Revision 1.2.2.1  2005/05/05 09:46:01  mschimek
// *** empty log message ***
//
// Revision 1.2  2005/02/05 22:20:28  mschimek
// Completed l18n.
//
// Revision 1.1  2005/01/08 14:54:23  mschimek
// *** empty log message ***
//
// Revision 1.2  2004/11/15 23:03:19  michael
// *** empty log message ***
//
// Revision 1.1  2004/11/14 15:35:14  michael
// *** empty log message ***
//
// Revision 1.8  2002/06/13 12:10:24  adcockj
// Move to new Setings dialog for filers, video deint and advanced settings
//
// Revision 1.7  2001/11/23 19:33:14  adcockj
// Fixes to bob to make is less jittery
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
///////////////////////////////////////////////////////////////////////////*/

#include "windows.h"
#include "DS_Deinterlace.h"

SIMD_FN_PROTOS (DEINTERLACE_FUNC, DeinterlaceBob);

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW |			\
	    CPU_FEATURE_SSE | CPU_FEATURE_SSE2 | CPU_FEATURE_ALTIVEC)

/*///////////////////////////////////////////////////////////////////////////
// Simple Bob.  Copies the most recent field to the overlay, with each scanline
// copied twice.
///////////////////////////////////////////////////////////////////////////*/
BOOL
SIMD_NAME (DeinterlaceBob)	(TDeinterlaceInfo *	pInfo)
{
    int i;
    BYTE* lpOverlay = pInfo->Overlay;
    BYTE* CurrentLine = pInfo->PictureHistory[0]->pData;
    DWORD Pitch = pInfo->InputPitch;

    /* No recent data?  We can't do anything. */
    if (NULL == CurrentLine) {
        return FALSE;
    }

    if (SIMD == CPU_FEATURE_SSE2) {
	if ((INTPTR (pInfo->Overlay) |
	     INTPTR (pInfo->PictureHistory[0]->pData) |
	     (unsigned int) pInfo->OverlayPitch |
	     (unsigned int) pInfo->InputPitch |
	     (unsigned int) pInfo->LineLength) & 15)
	    return DeinterlaceBob_SSE (pInfo);
    }

    /* If field is odd we will offset it down
       1 line to avoid jitter  TRB 1/21/01 */
    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
        /* extra copy of first line */
        copy_line (lpOverlay, CurrentLine, pInfo->LineLength);
	lpOverlay += pInfo->OverlayPitch; /* and offset out output ptr */

	for (i = 0; i < pInfo->FieldHeight - 1; i++) {
	  copy_line_pair (lpOverlay, CurrentLine,
			  pInfo->LineLength, pInfo->OverlayPitch);
	  lpOverlay += 2 * pInfo->OverlayPitch;
	  CurrentLine += Pitch;
	}

	/* only 1 copy of last line */
	copy_line (lpOverlay, CurrentLine, pInfo->LineLength);
    } else {
        for (i = 0; i < pInfo->FieldHeight; i++) {
	    copy_line_pair (lpOverlay, CurrentLine,
			    pInfo->LineLength, pInfo->OverlayPitch);
	    lpOverlay += 2 * pInfo->OverlayPitch;
	    CurrentLine += Pitch;
	}
    }

    vempty ();

    return TRUE;
}

#elif !SIMD

const DEINTERLACE_METHOD BobMethod =
{
    sizeof(DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    N_("Simple Bob"), 
    NULL,
    FALSE, 
    FALSE, 
    /* pfnAlgorithm */ NULL,
    50, 
    60,
    0,
    NULL,
    INDEX_BOB,
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
    IDH_BOB,
};


DEINTERLACE_METHOD *
DI_Bob_GetDeinterlacePluginInfo (void)
{
    DEINTERLACE_METHOD *m;

    m = malloc (sizeof (*m));
    *m = BobMethod;

    m->pfnAlgorithm =
        SIMD_FN_SELECT (DeinterlaceBob,
			CPU_FEATURE_MMX | CPU_FEATURE_3DNOW |
			CPU_FEATURE_SSE | CPU_FEATURE_SSE2 |
			CPU_FEATURE_ALTIVEC);

    return m;
}

#endif /* !SIMD */

/*
Local Variables:
c-basic-offset: 4
End:
 */

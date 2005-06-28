/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_ScalerBob.c,v 1.4 2005-06-28 19:17:10 mschimek Exp $
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
// 04 Jan 2001   John Adcock           Split into separate module
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.3  2005/06/28 00:50:44  mschimek
// Added support for x86-64, AltiVec and a scalar version. Cleaned up.
//
// Revision 1.2.2.4  2005/06/17 02:54:20  mschimek
// *** empty log message ***
//
// Revision 1.2.2.3  2005/05/31 02:40:34  mschimek
// *** empty log message ***
//
// Revision 1.2.2.2  2005/05/20 05:45:14  mschimek
// *** empty log message ***
//
// Revision 1.2.2.1  2005/05/05 09:46:01  mschimek
// *** empty log message ***
//
// Revision 1.2  2005/02/05 22:19:24  mschimek
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
// Revision 1.6  2002/06/13 12:10:24  adcockj
// Move to new Setings dialog for filers, video deint and advanced settings
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

SIMD_FN_PROTOS (DEINTERLACE_FUNC, DeinterlaceScalerBob);

#if !SIMD || (SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_SSE |		\
		      CPU_FEATURE_SSE2 | CPU_FEATURE_ALTIVEC))

BOOL
SIMD_NAME (DeinterlaceScalerBob)(TDeinterlaceInfo *	pInfo)
{
    uint8_t *Dest;
    const uint8_t *Src;
    unsigned int height;

    if (SIMD == CPU_FEATURE_SSE2) {
	if (((unsigned long) pInfo->Overlay |
	     (unsigned long) pInfo->PictureHistory[0]->pData |
	     (unsigned long) pInfo->OverlayPitch |
	     (unsigned long) pInfo->InputPitch |
	     (unsigned long) pInfo->LineLength) & 15)
	    return DeinterlaceScalerBob_SSE (pInfo);
    }

    Dest = pInfo->Overlay;
    Src = pInfo->PictureHistory[0]->pData;

    for (height = pInfo->FieldHeight; height > 0; --height) {
        copy_line (Dest, Src, pInfo->LineLength);
	Dest += pInfo->OverlayPitch;
        Src += pInfo->InputPitch;
    }

    vempty ();

    return TRUE;
}

#endif

#if !SIMD

static const DEINTERLACE_METHOD
ScalerBobMethod = {
    sizeof(DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    N_("Scaler Bob"), 
    NULL,
    TRUE,
    FALSE,
    /* pfnAlgorithm */ NULL,
    50, 
    60,
    0,
    NULL,
    INDEX_SCALER_BOB,
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
    IDH_SCALER_BOB,
};

DEINTERLACE_METHOD *
DI_ScalerBob_GetDeinterlacePluginInfo (void)
{
    DEINTERLACE_METHOD *m;

    m = malloc (sizeof (*m));
    *m = ScalerBobMethod;

    m->pfnAlgorithm = SIMD_FN_SELECT (DeinterlaceScalerBob,
				      SCALAR |
				      CPU_FEATURE_MMX |
				      CPU_FEATURE_SSE |
				      CPU_FEATURE_SSE2 |
				      CPU_FEATURE_ALTIVEC);

    return m;
}

#endif /* !SIMD */

/*
Local Variables:
c-basic-offset: 4
End:
 */

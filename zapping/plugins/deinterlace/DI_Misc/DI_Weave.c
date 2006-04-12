/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_Weave.c,v 1.5 2006-04-12 01:42:56 mschimek Exp $
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
// Revision 1.4  2005/06/28 19:17:10  mschimek
// *** empty log message ***
//
// Revision 1.3  2005/06/28 00:50:22  mschimek
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

#if !SIMD || (SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_SSE_INT |		\
		      CPU_FEATURE_SSE2 | CPU_FEATURE_ALTIVEC))

/*///////////////////////////////////////////////////////////////////////////
// Simple Weave.  Copies alternating scanlines from the most recent fields.
///////////////////////////////////////////////////////////////////////////*/
BOOL
SIMD_NAME (DeinterlaceWeave)	(TDeinterlaceInfo *	pInfo)
{
    uint8_t *Dest;
    const uint8_t *F4;
    const uint8_t *F3;
    unsigned int byte_width;
    unsigned int height;
    unsigned long dst_bpl;
    unsigned long src_bpl;

    if (SIMD == CPU_FEATURE_SSE2) {
	if (((unsigned long) pInfo->Overlay |
	     (unsigned long) pInfo->PictureHistory[0]->pData |
	     (unsigned long) pInfo->PictureHistory[1]->pData |
	     (unsigned long) pInfo->OverlayPitch |
	     (unsigned long) pInfo->InputPitch |
	     (unsigned long) pInfo->LineLength) & 15)
	    return DeinterlaceWeave_SSE (pInfo);
    }

    byte_width = pInfo->LineLength;

    dst_bpl = pInfo->OverlayPitch;
    src_bpl = pInfo->InputPitch;

    /*
	dest	src odd		src even
	0	F3.0		F4.0
	1	F4.0		F3.0
	h-2	F3.h/2-1	F4.h/2-1
	h-1	F4.h/2-1	F3.h/2-1

	F4	this field
	F3	previous field, opposite parity
	h	frame height
    */

    Dest = (uint8_t *) pInfo->Overlay;

    F4 = (const uint8_t *) pInfo->PictureHistory[0]->pData;
    F3 = (const uint8_t *) pInfo->PictureHistory[1]->pData;

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
	SWAP (F3, F4);
    }

    for (height = pInfo->FieldHeight; height > 0; --height) {
        copy_line (Dest, F4, byte_width);
        Dest += dst_bpl;
	F4 += src_bpl;

        copy_line (Dest, F3, byte_width);
        Dest += dst_bpl;
	F3 += src_bpl;
    }

    vempty ();

    return TRUE;
}

#endif

#if !SIMD

static const DEINTERLACE_METHOD
WeaveMethod = {
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
			SCALAR | CPU_FEATURE_MMX | CPU_FEATURE_SSE_INT |
			CPU_FEATURE_SSE2 | CPU_FEATURE_ALTIVEC);

    return m;
}

#endif /* !SIMD */

/*
Local Variables:
c-basic-offset: 4
End:
 */

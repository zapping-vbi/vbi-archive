/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_GreedyHF.c,v 1.1.2.2 2005-05-31 02:40:34 mschimek Exp $
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
//
/////////////////////////////////////////////////////////////////////////////
// Change Log
//
// Date          Developer             Changes
//
// 01 Feb 2001   Tom Barry	       New Greedy (High Motion)
//					 Deinterlace method
//
// 29 Jul 2001   Tom Barry	       Move CPU dependent code to
//					 DI_GreedyHF.asm
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.1.2.1  2005/05/05 09:46:00  mschimek
// *** empty log message ***
//
// Revision 1.1  2005/01/08 14:54:23  mschimek
// *** empty log message ***
//
// Revision 1.6  2001/07/30 21:50:32  trbarry
// Use weave chroma for reduced chroma jitter. Fix DJR bug again.
// Turn off Greedy Pulldown default.
//
// Revision 1.5  2001/07/30 18:18:59  trbarry
// Fix new DJR bug
//
// Revision 1.4  2001/07/30 17:56:26  trbarry
// Add Greedy High Motion MMX, K6-II, K6-III, and Celeron support.
// Tweak defaults.
//
// Revision 1.3  2001/07/28 18:47:24  trbarry
// Fix Sharpness with Median Filter
// Increase Sharpness default to make obvious
// Adjust deinterlace defaults for less jitter
//
// Revision 1.2  2001/07/25 12:04:31  adcockj
// Moved Control stuff into DS_Control.h
// Added $Id and $Log to comment blocks as per standards
//
///////////////////////////////////////////////////////////////////////////*/

/* This is the first version of the Greedy High Motion Deinterlace method I
   wrote (and kept).  It doesn't have many of the fancier options but I left
   it in because it's faster. It runs with a field delay of 1 in a single pass
   with no call needed to UpdateFieldStore. It will be called if no special
   options are needed. The logic is somewhat different than the other rtns.
   TRB 7/2001

   This version has been modified to be compatible with other DScaler
   functions such as Auto Pulldown.  It will now automatically be call if none
   of the UI check boxes are check and also if we are not running on an SSE
   capable machine (Athlon, Duron, P-III, fast Celeron). */

#include "windows.h"
#include "DS_Deinterlace.h"
#include "DI_GreedyHM.h"

BOOL
SIMD_NAME (DI_GreedyHF)		(TDeinterlaceInfo *	pInfo)
{
    vu8 MaxCombW;
    vu8 MotionThresholdW;
    v16 MotionSenseW;
    uint8_t *Dest;
    const uint8_t *L1;
    const uint8_t *L2;
    const uint8_t *L2P;
    unsigned int byte_width;
    unsigned int height;
    unsigned long dst_bpl;
    unsigned long src_bpl;
    unsigned long dst_padding;
    unsigned long src_padding;

    MaxCombW = vsplatu8 (GreedyHMaxComb);

    /* In a saturated subtraction UVMask clears the u, v bytes. */
    MotionThresholdW =
	(vu8) vor ((v16) vsplat8 (GreedyMotionThreshold), UVMask);

    MotionSenseW = vsplat16 (GreedyMotionSense);

    byte_width = pInfo->LineLength;
    height = pInfo->FieldHeight;

    dst_bpl = pInfo->OverlayPitch;
    src_bpl = pInfo->InputPitch;

    Dest = (uint8_t *) pInfo->Overlay;

    /* copy first even line no matter what, and the first odd line if we're
       processing an EVEN field. (note diff from other deint rtns.) */

    L1 = (const uint8_t *) pInfo->PictureHistory[1]->pData;
    L2 = (const uint8_t *) pInfo->PictureHistory[0]->pData;  
    L2P = (const uint8_t *) pInfo->PictureHistory[2]->pData;

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
        /* copy first even line */
        copy_line (Dest, L1, byte_width);
        Dest += dst_bpl;
    } else {
        /* copy first even line */
        copy_line (Dest, L2, byte_width);
        Dest += dst_bpl;
        /* then first odd line */
        copy_line (Dest, L1, byte_width);
        Dest += dst_bpl;

        L2 += src_bpl;
        L2P += src_bpl;
    }

    dst_padding = dst_bpl * 2 - byte_width;
    src_padding = src_bpl - byte_width;

    for (height = pInfo->FieldHeight - 1; height > 0; --height) {
	GreedyHXCore (&Dest, &L1, &L2, &L2P,
		      byte_width, dst_bpl, src_bpl,
		      /* src_incr */ sizeof (vu8),
		      MaxCombW,
		      MotionThresholdW,
		      MotionSenseW,
		      STORE_WEAVE_L3);

	Dest += dst_padding;
        L1 += src_padding;
        L2 += src_padding;
        L2P += src_padding;
    }

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
        copy_line (Dest, L2, byte_width);
    }

    vempty ();

    return TRUE;
}

/*
Local Variables:
c-basic-offset: 4
End:
 */

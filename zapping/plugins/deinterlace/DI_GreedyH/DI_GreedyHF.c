/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_GreedyHF.c,v 1.2 2005-06-28 00:47:24 mschimek Exp $
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
// Revision 1.1.2.3  2005/06/17 02:54:20  mschimek
// *** empty log message ***
//
// Revision 1.1.2.2  2005/05/31 02:40:34  mschimek
// *** empty log message ***
//
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
    const uint8_t *F2;
    const uint8_t *F3;
    const uint8_t *F4;
    unsigned int byte_width;
    unsigned int height;
    unsigned long dst_bpl;
    unsigned long src_bpl;

    MaxCombW = vsplatu8 (GreedyHMaxComb);

    /* In a saturated subtraction UVMask (255) clears the u, v bytes. */
    MotionThresholdW =
	(vu8) vor ((v16) vsplat8 (GreedyMotionThreshold), UVMask);

    MotionSenseW = vsplat16 (GreedyMotionSense);

    byte_width = pInfo->LineLength;
    height = pInfo->FieldHeight;

    dst_bpl = pInfo->OverlayPitch;
    src_bpl = pInfo->InputPitch;

    /*
	dest	src odd					src even
	0	F3.0					F4.0
	1     {	weave.0 (F4.n + F2.n + F3.n + F3.n+1)	F3.0
	2     {	F3.1		weave.1 (F4.n + F2.n + F3.n-1 + F3.n)	}
	3	weave.1					F3.1		}
	4	F3.2					weave.2
	h-3	weave.h/2-2				F3.h/2-2
	h-2	F3.h/2-1				weave.h/2-1
	h-1	F4.h/2-1				F3.h/2-1

	F4	this field
	F3	previous field, opposite parity
	F2	previous field, same parity
	F1	previous field, opposite parity
	h	frame height
    */

    Dest = (uint8_t *) pInfo->Overlay;

    F4 = (const uint8_t *) pInfo->PictureHistory[0]->pData;  
    F3 = (const uint8_t *) pInfo->PictureHistory[1]->pData;
    F2 = (const uint8_t *) pInfo->PictureHistory[2]->pData;

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_EVEN) {
        copy_line (Dest, F4, byte_width);
        Dest += dst_bpl;

        F4 += src_bpl;
        F2 += src_bpl;
    }

    copy_line (Dest, F3, byte_width);
    Dest += dst_bpl;

    for (height = pInfo->FieldHeight - 1; height > 0; --height) {
	GreedyHXCore (Dest,
		      /* Curr */ F4,
		      /* Above */ F3,
		      /* Prev */ F2,
		      byte_width,
		      dst_bpl,
		      src_bpl,
		      /* src_incr */ sizeof (vu8),
		      MaxCombW,
		      MotionThresholdW,
		      MotionSenseW,
		      STORE_WEAVE_BELOW);

	Dest += dst_bpl * 2;
        F4 += src_bpl;
        F3 += src_bpl;
        F2 += src_bpl;
    }

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD) {
        copy_line (Dest, F4, byte_width);
    }

    vempty ();

    return TRUE;
}

/*
Local Variables:
c-basic-offset: 4
End:
 */

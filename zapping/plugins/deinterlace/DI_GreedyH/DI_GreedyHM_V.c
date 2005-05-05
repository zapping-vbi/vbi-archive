/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_GreedyHM_V.c,v 1.1.2.1 2005-05-05 09:46:00 mschimek Exp $
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
// 01 Jul 2001   Tom Barry	       Break out Greedy (High Motion)
//					 Deinterlace, w/Vert Filter
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.1  2005/01/08 14:54:23  mschimek
// *** empty log message ***
//
// Revision 1.5  2001/10/02 17:44:41  trbarry
// Changes to be compatible with the Avisynth filter version
//
// Revision 1.4  2001/08/17 19:30:55  trbarry
// Fix OBO error in GreedyH Vertical Filter
//
// Revision 1.3  2001/08/17 17:08:42  trbarry
// GreedyH performance enhancement:
//
// Unroll loop to support Write Combining in Vertical Filter
// (curiously this now peforms better than without V. Filter)
//
// Revision 1.2  2001/07/25 12:04:31  adcockj
// Moved Control stuff into DS_Control.h
// Added $Id and $Log to comment blocks as per standards
//
///////////////////////////////////////////////////////////////////////////*/

/* This version handles Greedy High Motion with/without Vertical Filtering */

#include "windows.h"
#include "DI_GreedyHM.h"

static __inline__ BOOL
DI_GreedyHM_V_template		(TDeinterlaceInfo *	pInfo,
				 ghxc_mode		mode)
{
    vu8 MaxCombW;
    vu8 MotionThresholdW;
    v16 MotionSenseW;
    uint8_t *WeaveDest;	/* dest for weave pixel */
    uint8_t *CopyDest;	/* other dest, copy or vertical filter */
    const uint8_t *pL2; /* ptr into FieldStore[L2] */
    const uint8_t *pFieldStoreEnd;
    int L1;	/* offset to FieldStore elem holding top known pixels */
    int L3;	/* offset to FieldStore elem holding bottom known pxl */
    int L2;	/* offset to FieldStore elem holding newest weave pixels */
    int CopySrc;
    unsigned int height;
    int dst_bpl;
    unsigned int dst_padding;
    unsigned int src_padding;

    MaxCombW = vsplat8 (GreedyHMaxComb);

    /* In a saturated subtraction UVMask clears the u, v bytes. */
    MotionThresholdW =
	(vu8) vor ((v16) vsplat8 (GreedyMotionThreshold), UVMask);

    MotionSenseW = vsplat16 (GreedyMotionSense);

    pFieldStoreEnd = (const uint8_t *) FieldStore
	+ pInfo->FieldHeight * FSROWSIZE;

    /* set up pointers, offsets */
    SetFsPtrs (&L1, &L2, &L3, &CopySrc, &CopyDest, &WeaveDest, pInfo);

    L2 &= 1; /* subscript to 1st of 2 possible weave pixels, our base addr */
    pL2 = (const uint8_t *) FieldStore + L2 * sizeof (vu8);
    L1 = (L1 - L2) * sizeof (vu8); /* now is signed offset from L2 */  
    L3 = (L3 - L2) * sizeof (vu8);

    height = pInfo->FieldHeight;

    if (WeaveDest == pInfo->Overlay) {
	/* on first line may just copy first and last */
	SIMD_NAME (FieldStoreCopy)(WeaveDest,
				   (const uint8_t *) FieldStore
				   + CopySrc * sizeof (vu8),
				   pInfo->LineLength);
	WeaveDest += pInfo->OverlayPitch * 2;
	/* CopyDest already OK */
	pL2 += FSROWSIZE;
	--height;
    }

    dst_bpl = CopyDest - WeaveDest;

    dst_padding = pInfo->OverlayPitch * 2 - pInfo->LineLength;
    src_padding = FSROWSIZE - pInfo->LineLength;

    for (; height > 0; --height) {
	const uint8_t *pL1, *pL2P;
	unsigned int l1o, l3o;

	l1o = L1;
	l3o = L3;

	if (pL2 + l1o < FieldStore)
	    l1o = l3o; /* first line */
	else if (pL2 + l3o >= pFieldStoreEnd)
	    l3o = l1o; /* last line */

	pL1 = pL2 + l1o;
	pL2P = pL2 + 2 * sizeof (vu8);

	GreedyHXCore (&WeaveDest,
		      &pL1, &pL2, &pL2P,
		      pInfo->LineLength,
		      dst_bpl,
		      /* src_bpl */ l3o - l1o,
		      /* src_incr */ FSCOLSIZE,
		      MaxCombW,
		      MotionThresholdW,
		      MotionSenseW,
		      mode);

	WeaveDest += dst_padding;
	pL2 += src_padding;
    }
   
    vempty ();

    return TRUE;
}

BOOL
SIMD_NAME (DI_GreedyHM_NV)	(TDeinterlaceInfo *	pInfo)
{
    /* No vertical averaging. */
    return DI_GreedyHM_V_template (pInfo, STORE_WEAVE_L1);
}

BOOL
SIMD_NAME (DI_GreedyHM_V)	(TDeinterlaceInfo *	pInfo)
{
    /* With vertical averaging. */
    return DI_GreedyHM_V_template (pInfo, STORE_L1WEAVE_L3WEAVE);
}

/*
Local Variables:
c-basic-offset: 4
End:
 */

/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_GreedyHM_V.c,v 1.2 2005-06-28 00:45:58 mschimek Exp $
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
// Revision 1.1.2.4  2005/06/17 02:54:20  mschimek
// *** empty log message ***
//
// Revision 1.1.2.3  2005/05/31 02:40:34  mschimek
// *** empty log message ***
//
// Revision 1.1.2.2  2005/05/17 19:58:32  mschimek
// *** empty log message ***
//
// Revision 1.1.2.1  2005/05/05 09:46:00  mschimek
// *** empty log message ***
//
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

#ifndef DI_GREEDYHM_V_ASSERT
#  define DI_GREEDYHM_V_ASSERT 0
#endif

static always_inline BOOL
DI_GreedyHM_V_template		(TDeinterlaceInfo *	pInfo,
				 ghxc_mode		mode)
{
    vu8 MaxCombW;
    vu8 MotionThresholdW;
    v16 MotionSenseW;
    uint8_t *WeaveDest;	/* dest for weave pixel */
    uint8_t *CopyDest;	/* other dest, copy or vertical filter */
    const uint8_t *pL2; /* ptr into FieldStore[L2] */
    const uint8_t *pFieldStoreBegin;
    const uint8_t *pFieldStoreEnd;
    long dL1;	/* offset to FieldStore elem holding top known pixels */
    long dL3;	/* offset to FieldStore elem holding bottom known pxl */
    long dL2;	/* offset to FieldStore elem holding newest weave pixels */
    long dCopySrc;
    unsigned int height;
    long dst_bpl;

    MaxCombW = vsplatu8 (GreedyHMaxComb);

    /* In a saturated subtraction UVMask clears the u, v bytes. */
    MotionThresholdW =
	(vu8) vor ((v16) vsplat8 (GreedyMotionThreshold), UVMask);

    MotionSenseW = vsplat16 (GreedyMotionSense);

    pFieldStoreBegin = FieldStore;
    pFieldStoreEnd = FieldStore	+ pInfo->FieldHeight * FS_BYTES_PER_ROW;

    /* set up pointers, offsets */
    SIMD_NAME (SetFsPtrs)(&dL1, &dL2, &dL3, &dCopySrc,
			  &CopyDest, &WeaveDest, pInfo);

    /* Subscript to 1st of 2 possible weave pixels, our base addr */
    dL2 &= 1 * sizeof (vu8);

    pL2 = pFieldStoreBegin + dL2;
    dL1 = dL1 - dL2; /* now is signed offset from pL2 */  
    dL3 = dL3 - dL2;

    height = pInfo->FieldHeight;

    if (WeaveDest == pInfo->Overlay) {
	/* on first line may just copy first and last */
	SIMD_NAME (FieldStoreCopy)(WeaveDest, pFieldStoreBegin + dCopySrc,
				   pInfo->LineLength);
	WeaveDest += 2 * pInfo->OverlayPitch;
	/* CopyDest already OK */
	pL2 += FS_BYTES_PER_ROW;
	--height;
    }

    dst_bpl = CopyDest - WeaveDest;

    for (; height > 0; --height) {
	const uint8_t *pL1, *pL2P;
	long l1o, l3o;

	l1o = dL1;
	l3o = dL3;

	if (pL2 + l1o < pFieldStoreBegin)
	    l1o = l3o; /* first line */
	if (pL2 + l3o >= pFieldStoreEnd)
	    l3o = l1o; /* last line */

	pL1 = pL2 + l1o;
	pL2P = pL2 + FsPrevFrame (0); /* Field 0/1 -> 2/3 */

	if (DI_GREEDYHM_V_ASSERT) {
		assert (pL1 >= pFieldStoreBegin && pL1 < pFieldStoreEnd);
		assert (pL2 >= pFieldStoreBegin && pL2 < pFieldStoreEnd);
		assert (pL2P >= pFieldStoreBegin && pL2P < pFieldStoreEnd);
		assert ((l3o - l1o) >= 0);
	}

	GreedyHXCore (WeaveDest, pL2, pL1, pL2P,
		      pInfo->LineLength,
		      dst_bpl,
		      /* src_bpl = FS_BYTES_PER_ROW or 0 */ l3o - l1o,
		      /* src_incr */ FS_FIELDS * sizeof (vu8),
		      MaxCombW,
		      MotionThresholdW,
		      MotionSenseW,
		      mode);

	WeaveDest += 2 * pInfo->OverlayPitch;
	pL2 += FS_BYTES_PER_ROW;
    }
   
    vempty ();

    return TRUE;
}

BOOL
SIMD_NAME (DI_GreedyHM_NV)	(TDeinterlaceInfo *	pInfo)
{
    /* No vertical averaging. */
    return DI_GreedyHM_V_template (pInfo, STORE_WEAVE_ABOVE);
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

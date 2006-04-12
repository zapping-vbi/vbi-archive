/*///////////////////////////////////////////////////////////////////////////
// $Id: DI_GreedyHMPulldown.c,v 1.4 2006-04-12 01:44:48 mschimek Exp $
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
/////////////////////////////////////////////////////////////////////////////
// Change Log
//
// Date          Developer             Changes
//
// 01 Jul 2001   Tom Barry	       New Greedy (High Motion)
//					 Deinterlace method
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.3  2005/06/28 00:46:10  mschimek
// Converted to vector intrinsics. Added support for 3DNow, SSE2, x86-64
// and AltiVec. Removed ununsed DScaler code. Cleaned up. All options work
// now.
//
// Revision 1.2.2.3  2005/05/31 02:40:34  mschimek
// *** empty log message ***
//
// Revision 1.2.2.2  2005/05/17 19:58:32  mschimek
// *** empty log message ***
//
// Revision 1.2.2.1  2005/05/05 09:46:00  mschimek
// *** empty log message ***
//
// Revision 1.2  2005/01/20 01:38:33  mschimek
// *** empty log message ***
//
// Revision 1.1  2005/01/08 14:54:23  mschimek
// *** empty log message ***
//
// Revision 1.5  2001/11/25 04:33:37  trbarry
// Fix for TDeinterlace_Info. Also release UN-Filter code,
// 5-tap V & H sharp/soft filters optimized to reverse excessive
// filtering (or EE?)
//
// Revision 1.4  2001/11/13 17:24:49  trbarry
// Misc GreedyHMA Avisynth related changes. Also fix bug in Vertical
// filter causing right hand garbage on screen.
//
// Revision 1.2  2001/07/25 12:04:31  adcockj
// Moved Control stuff into DS_Control.h
// Added $Id and $Log to comment blocks as per standards
//
///////////////////////////////////////////////////////////////////////////*/

#include "windows.h"
#include "DI_GreedyHM.h"

extern int HistPtr;
extern GR_PULLDOWN_INFO Hist[20];

#if !SIMD

int HistPtr = 0; /* where we are in Queue */
/* a short history of pulldown stats */
GR_PULLDOWN_INFO Hist[20];

int
UpdatePulldown(TDeinterlaceInfo *	pInfo,
	       int Comb, int Kontrast, int Motion)
{
    int Prev = (HistPtr+20-1) % 20;	/* prev entry ptr */
    /* Last elem still in average at prev */
    int Last = (Prev + 20+1-PDAVGLEN) % 20;

    int FlagMask = 0x000ffffe;      /* trunc to 20 bits, turn off low */

    /* note most values are updated delay 1, except input Comb */
    if (Comb < Hist[HistPtr].Comb) {
	Hist[HistPtr].CombChoice = Comb;
	Hist[HistPtr].Flags = ((Hist[Prev].Flags << 1) & FlagMask) | 1;
	if (Hist[HistPtr].Comb > 0 && Kontrast > 0) {
	    Hist[HistPtr].AvgChoice =
		100 - 100 * Hist[HistPtr].CombChoice / Hist[HistPtr].Comb;
        }
    } else {
	Hist[HistPtr].CombChoice = Hist[HistPtr].Comb;
	Hist[HistPtr].Flags = (Hist[Prev].Flags << 1) & FlagMask;
	if (Comb > 0 && Kontrast > 0) {
	    /*  Hist[HistPtr].AvgChoice =
		100 * (100 - 100 * Hist[HistPtr].CombChoice / Comb); */
	    Hist[HistPtr].AvgChoice =
		100 - 100 * Hist[HistPtr].CombChoice / Comb;
	}
    }

    Hist[HistPtr].Kontrast = Kontrast;	/* Kontrast calc'd in arrears */
    Hist[HistPtr].Motion = Motion;	/* Motion Calc'd in arrears */
    Hist[HistPtr].Avg = Hist[Prev].Avg
	+ Hist[HistPtr].AvgChoice - Hist[Last].AvgChoice;	

    HistPtr = (1+HistPtr) % 20;	/* bump for next time */
    /* only fill in Comb for curr val, rest is garbage */
    Hist[HistPtr].Comb = Comb;
    /* only fill in Comb for curr val, rest is garbage */
    Hist[HistPtr].Kontrast = 0;
    /* only fill in Comb for curr val, rest is garbage */
    Hist[HistPtr].Motion= 0;

    if (!(pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD)) {   
        Hist[HistPtr].Flags2 = PD_ODD;
    } else {
        Hist[HistPtr].Flags2 = 0;
    }

    return 0;
}

#else /* SIMD */

/* copy 1 line from Fieldstore to overlay buffer */
void
SIMD_NAME (FieldStoreCopy)	(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		n_bytes)
{
    vu8 m0, m1, m2, m3;

    for (; n_bytes & -(sizeof (vu8) * 4); n_bytes -= sizeof (vu8) * 4) {
	m0 = vload (src, FS_FIELDS * sizeof (vu8) * 0);
	m1 = vload (src, FS_FIELDS * sizeof (vu8) * 1);
	m2 = vload (src, FS_FIELDS * sizeof (vu8) * 2);
	m3 = vload (src, FS_FIELDS * sizeof (vu8) * 3);
	src += FS_FIELDS * sizeof (vu8) * 4;

	vstorent (dst, 0 * sizeof (vu8), m0);
	vstorent (dst, 1 * sizeof (vu8), m1);
	vstorent (dst, 2 * sizeof (vu8), m2);
	vstorent (dst, 3 * sizeof (vu8), m3);
	dst += 4 * sizeof (vu8);
    }

    /* Remaining bytes, usually not needed. */
    for (; n_bytes > 0; n_bytes -= sizeof (vu8)) {
	vstorent (dst, 0, vload (src, 0));
	src += FS_FIELDS * sizeof (vu8);
	dst += sizeof (vu8);
    }
}

/* copy 1 line from Fieldstore to overlay buffer,
   merging with succeeding line. */
static void
FieldStoreMerge			(uint8_t *		dst,
				 const uint8_t *	src1,
				 const uint8_t *	src2,
				 unsigned int		n_bytes)
{
    vu8 m0, m1, m2, m3;

    for (; n_bytes & -(sizeof (vu8) * 4); n_bytes -= sizeof (vu8) * 4) {
	m0 = vload (src1, FS_FIELDS * sizeof (vu8) * 0);
	m1 = vload (src1, FS_FIELDS * sizeof (vu8) * 1);
	m2 = vload (src1, FS_FIELDS * sizeof (vu8) * 2);
	m3 = vload (src1, FS_FIELDS * sizeof (vu8) * 3);
	src1 += FS_FIELDS * sizeof (vu8) * 4;

	m0 = fast_vavgu8 (m0, vload (src2, FS_FIELDS * sizeof (vu8) * 0));
	m1 = fast_vavgu8 (m1, vload (src2, FS_FIELDS * sizeof (vu8) * 1));
	m2 = fast_vavgu8 (m2, vload (src2, FS_FIELDS * sizeof (vu8) * 2));
	m3 = fast_vavgu8 (m3, vload (src2, FS_FIELDS * sizeof (vu8) * 3));
	src2 += FS_FIELDS * sizeof (vu8) * 4;

	vstorent (dst, 0 * sizeof (vu8), m0);
	vstorent (dst, 1 * sizeof (vu8), m1);
	vstorent (dst, 2 * sizeof (vu8), m2);
	vstorent (dst, 3 * sizeof (vu8), m3);
	dst += 4 * sizeof (vu8);
    }

    /* Remaining bytes, usually not needed. */
    for (; n_bytes > 0; n_bytes -= sizeof (vu8)) {
	m0 = fast_vavgu8 (vload (src1, 0), vload (src2, 0));
	src1 += FS_FIELDS * sizeof (vu8);
	src2 += FS_FIELDS * sizeof (vu8);

	vstorent (dst, 0, m0);
	dst += sizeof (vu8);
    }
}

/* copy 1 line from Fieldstore to overlay buffer,
   merging with succeeding line and succeeding frame. */
static void
FieldStoreMerge_V		(uint8_t *		dst,
				 const uint8_t *	src1,
				 const uint8_t *	src2,
				 unsigned int		n_bytes)
{
    /* next/prev. field of same parity */
    const unsigned int np = 2 * sizeof (vu8);
    vu8 m0, m1, m2, m3, m4, m5, m6, m7;

    for (; n_bytes & -(sizeof (vu8) * 4); n_bytes -= sizeof (vu8) * 4) {
	m0 = vload (src1, FS_FIELDS * sizeof (vu8) * 0);
	m1 = vload (src1, FS_FIELDS * sizeof (vu8) * 1);
	m2 = vload (src1, FS_FIELDS * sizeof (vu8) * 2);
	m3 = vload (src1, FS_FIELDS * sizeof (vu8) * 3);

	m4 = vload (src2, FS_FIELDS * sizeof (vu8) * 0);
	m5 = vload (src2, FS_FIELDS * sizeof (vu8) * 1);
	m6 = vload (src2, FS_FIELDS * sizeof (vu8) * 2);
	m7 = vload (src2, FS_FIELDS * sizeof (vu8) * 3);

	m0 = fast_vavgu8 (m0, vload (src1, FS_FIELDS * sizeof (vu8) * 0 + np));
	m1 = fast_vavgu8 (m1, vload (src1, FS_FIELDS * sizeof (vu8) * 1 + np));
	m2 = fast_vavgu8 (m2, vload (src1, FS_FIELDS * sizeof (vu8) * 2 + np));
	m3 = fast_vavgu8 (m3, vload (src1, FS_FIELDS * sizeof (vu8) * 3 + np));
	src1 += FS_FIELDS * sizeof (vu8) * 4;

	m4 = fast_vavgu8 (m4, vload (src2, FS_FIELDS * sizeof (vu8) * 0 + np));
	m5 = fast_vavgu8 (m5, vload (src2, FS_FIELDS * sizeof (vu8) * 1 + np));
	m6 = fast_vavgu8 (m6, vload (src2, FS_FIELDS * sizeof (vu8) * 2 + np));
	m7 = fast_vavgu8 (m7, vload (src2, FS_FIELDS * sizeof (vu8) * 3 + np));
	src2 += FS_FIELDS * sizeof (vu8) * 4;

	vstorent (dst, 0 * sizeof (vu8), fast_vavgu8 (m0, m4));
	vstorent (dst, 1 * sizeof (vu8), fast_vavgu8 (m1, m5));
	vstorent (dst, 2 * sizeof (vu8), fast_vavgu8 (m2, m6));
	vstorent (dst, 3 * sizeof (vu8), fast_vavgu8 (m3, m7));
	dst += 4 * sizeof (vu8);
    }

    /* Remaining bytes, usually not needed. */
    for (; n_bytes > 0; n_bytes -= sizeof (vu8)) {
	m0 = fast_vavgu8 (vload (src1, 0),
			  vload (src1, np));
	src1 += FS_FIELDS * sizeof (vu8);
	m4 = fast_vavgu8 (vload (src2, 0),
			  vload (src2, np));
	src2 += FS_FIELDS * sizeof (vu8);
	vstorent (dst, 0, fast_vavgu8 (m0, m4));
	dst += sizeof (vu8);
    }
}

static BOOL
PullDown_InBetween		(TDeinterlaceInfo *	pInfo)
{
    uint8_t *Dest;
    const uint8_t *pE;
    const uint8_t *pO;
    unsigned long EvenL;
    unsigned int height;

    Dest = pInfo->Overlay;

    /* EvenL = __min(FsPtr, FsPtrP2 = (FsPtr - 2) % 4); */
    /* OddL = __min(FsPtrP = (FsPtr - 1) % 4, FsPtrP3 = (FsPtr - 3) % 4); */
    EvenL = (FsPtr & 1) * sizeof (vu8);
    pE = (uint8_t *) FieldStore + EvenL;
    pO = (uint8_t *) FieldStore + (EvenL ^ (1 * sizeof (vu8)));

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD)
	SWAP (pE, pO);

    if (GreedyUseVSharpness) {
	for (height = pInfo->FieldHeight - 1; height > 0; --height) {
	    FieldStoreMerge_V (Dest, pE, pO, pInfo->LineLength);
	    Dest += pInfo->OverlayPitch;
	    pE += FS_BYTES_PER_ROW;

	    FieldStoreMerge_V (Dest, pO, pE, pInfo->LineLength);
	    Dest += pInfo->OverlayPitch;
	    pO += FS_BYTES_PER_ROW;
	}

	/* one more time but dup last line */
	FieldStoreMerge_V (Dest, pE, pO, pInfo->LineLength);
	Dest += pInfo->OverlayPitch;

	FieldStoreMerge_V (Dest, pE, pO, pInfo->LineLength);
    } else { /* not vertical filter */
	for (height = pInfo->FieldHeight; height > 0; --height) {
	    FieldStoreMerge (Dest, pE, pE + 2 * sizeof (vu8),
			     pInfo->LineLength);
	    Dest += pInfo->OverlayPitch;
	    pE += FS_BYTES_PER_ROW;

	    FieldStoreMerge (Dest, pO, pO + 2 * sizeof (vu8),
			     pInfo->LineLength);
	    Dest += pInfo->OverlayPitch;
	    pO += FS_BYTES_PER_ROW;
	}
    }

    vempty ();

    return TRUE;
}

/* Pulldown with vertical filter */
static BOOL
PullDown_V			(TDeinterlaceInfo *	pInfo,
				 BOOL			SelectL2)
{
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
    unsigned long dst_padding;
    unsigned long src_padding;

    pFieldStoreBegin = FieldStore;
    pFieldStoreEnd = pFieldStoreBegin + pInfo->FieldHeight * FS_BYTES_PER_ROW;

    /* set up pointers, offsets */
    SIMD_NAME (SetFsPtrs)(&dL1, &dL2, &dL3, &dCopySrc,
			  &CopyDest, &WeaveDest, pInfo);

    if (!SelectL2) {
	dL2 = FsPrevFrame (dL2);
    }

    pL2 = (const uint8_t *) FieldStore + dL2;
    dL1 -= dL2; /* now is signed offset from pL2 */  
    dL3 -= dL2;

    height = pInfo->FieldHeight;

    if (WeaveDest == pInfo->Overlay) {
	/* on first line may just copy first and last */
	SIMD_NAME (FieldStoreCopy)(pInfo->Overlay,
				   pFieldStoreBegin + dCopySrc,
				   pInfo->LineLength);
	WeaveDest += pInfo->OverlayPitch * 2;
	/* CopyDest already OK */
	pL2 += FS_BYTES_PER_ROW;
	--height;
    }

    dst_padding = pInfo->OverlayPitch * 2 - pInfo->LineLength;
    src_padding = FS_BYTES_PER_ROW - pInfo->LineLength * FS_FIELDS;

    for (; height > 0; --height) {
	unsigned int count;
	unsigned int l1o, l3o;

	l1o = dL1;
	l3o = dL3;

	if (pL2 + l1o < pFieldStoreBegin)
	    l1o = l3o; /* first line */
	if (pL2 + l3o >= pFieldStoreEnd)
	    l3o = l1o; /* last line */

	for (count = pInfo->LineLength / sizeof (vu8); count > 0; --count) {
	    vu8 l1, l2, l3;

	    l1 = vload (pL2, l1o);
	    l2 = vload (pL2, 0);
	    l3 = vload (pL2, l3o);

	    vstorent (CopyDest, 0, fast_vavgu8 (l1, l2));
	    vstorent (WeaveDest, 0, fast_vavgu8 (l2, l3));

	    CopyDest += sizeof (vu8);
	    WeaveDest += sizeof (vu8);
	    pL2 += FS_FIELDS * sizeof (vu8);
	}

	WeaveDest += dst_padding;
	CopyDest += dst_padding;
	pL2 += src_padding;
    }

    vempty ();

    return TRUE;
}

static always_inline vu16
min255_sru6_u16			(vu16			mm0)
{
#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW)
    /* Faster emulation of vminu16 when one arg is const. */
    return vminu16i (vsru16 (mm0, 6), 255);
#elif SIMD & (CPU_FEATURE_SSE_INT | CPU_FEATURE_SSE2 | CPU_FEATURE_SSE3)
    /* Has no vminu16 but vmin16 is safe here. */
    return (vu16) vmin16 (vsru16 (mm0, 6), vsplatu16_255);
#elif SIMD == CPU_FEATURE_ALTIVEC
    return vec_min (vsru16 (mm0, 6), vsplatu16_255);
#else
#  error Unknown SIMD ISA.
    return vzerou8 ();
#endif
}
		
/* see comments in PullDown_VSharp() above */
static void
PullDown_VSharp2		(uint8_t *		dst,
				 const uint8_t *	src1,
				 const uint8_t *	src2,
				 unsigned int		n_bytes,
				 v16			QA,
				 v16			QB,
				 v16			QC,
				 int			C)
{
    vu8 Zi, Zj, Zk, Zl, Zm;
    vu16 mm0, mm1, mm2;
    unsigned int count;

    if (0 != C) {
	for (count = n_bytes / sizeof (vu8); count > 0; --count) {
	    Zi = vload (src1, -FS_BYTES_PER_ROW);
	    Zj = vload (src2, 0);
	    Zk = vload (src1, 0);
	    Zl = vload (src2, +FS_BYTES_PER_ROW);
	    Zm = vload (src1, +FS_BYTES_PER_ROW);
	    src1 += FS_FIELDS * sizeof (vu8);
	    src2 += FS_FIELDS * sizeof (vu8);

	    mm0 = (vu16) vmullo16 (yuyv2yy (Zk), QA);
	    mm1 = (vu16) vmullo16 (yuyv2yy (fast_vavgu8 (Zj, Zl)), QB);
	    mm2 = (vu16) vmullo16 (yuyv2yy (fast_vavgu8 (Zi, Zm)), QC);
	    mm0 = vsubsu16 (mm0, vsubsu16 (mm1, mm2)); /* sub */
	    mm0 = min255_sru6_u16 (mm0);
	    vstorent (dst, 0, recombine_yuyv (mm0, Zk));
	    dst += sizeof (vu8);
	}
    } else {
	for (count = n_bytes / sizeof (vu8); count > 0; --count) {
	    Zj = vload (src2, 0);
	    Zk = vload (src1, 0);
	    Zl = vload (src2, +FS_BYTES_PER_ROW);
	    src1 += FS_FIELDS * sizeof (vu8);
	    src2 += FS_FIELDS * sizeof (vu8);

	    mm0 = (vu16) vmullo16 (yuyv2yy (Zk), QA);
	    mm1 = (vu16) vmullo16 (yuyv2yy (fast_vavgu8 (Zj, Zl)), QB);
	    mm0 = min255_sru6_u16 (vsubsu16 (mm0, mm1));
	    vstorent (dst, 0, recombine_yuyv (mm0, Zk));
	    dst += sizeof (vu8);
	}
    }

    vempty ();
}

/* see comments in PullDown_VSharp() above */
static void
PullDown_VSoft2			(uint8_t *		dst,
				 const uint8_t *	src1,
				 const uint8_t *	src2,
				 unsigned int		n_bytes,
				 v16			QA,
				 v16			QB,
				 v16			QC,
				 int			C)
{
    vu8 Zi, Zj, Zk, Zl, Zm;
    vu16 mm0, mm1, mm2;
    unsigned int count;

    if (0 != C) {
	for (count = n_bytes / sizeof (vu8); count > 0; --count) {
	    Zi = vload (src1, -FS_BYTES_PER_ROW);
	    Zj = vload (src2, 0);
	    Zk = vload (src1, 0);
	    Zl = vload (src2, +FS_BYTES_PER_ROW);
	    Zm = vload (src1, +FS_BYTES_PER_ROW);
	    src1 += FS_FIELDS * sizeof (vu8);
	    src2 += FS_FIELDS * sizeof (vu8);

	    mm0 = vmullo16 (yuyv2yy (Zk), QA);
	    mm1 = vmullo16 (yuyv2yy (fast_vavgu8 (Zj, Zl)), QB);
	    mm2 = vmullo16 (yuyv2yy (fast_vavgu8 (Zi, Zm)), QC);
	    mm0 = vaddsu16 (mm0, vaddsu16 (mm1, mm2)); /* add */
	    mm0 = min255_sru6_u16 (mm0);
	    vstorent (dst, 0, recombine_yuyv (mm0, Zk));
	    dst += sizeof (vu8);
	}
    } else {
	for (count = n_bytes / sizeof (vu8); count > 0; --count) {
	    Zj = vload (src2, 0);
	    Zk = vload (src1, 0);
	    Zl = vload (src2, +FS_BYTES_PER_ROW);
	    src1 += FS_FIELDS * sizeof (vu8);
	    src2 += FS_FIELDS * sizeof (vu8);

	    mm0 = vmullo16 (yuyv2yy (Zk), QA);
	    mm1 = vmullo16 (yuyv2yy (fast_vavgu8 (Zj, Zl)), QB);
	    mm0 = vaddsu16 (mm0, mm1);
	    mm0 = min255_sru6_u16 (mm0);
	    vstorent (dst, 0, recombine_yuyv (mm0, Zk));
	    dst += sizeof (vu8);
	}
    }

    vempty ();
}

/* Add new Vertical Edge Enhancement optimized
   to reverse previous vertical filtering */
/*
Assume that someone vertically filtered the video for interlace using a simple
center weighted moving average, creating new pixels Z[k] from old pixels X[k]
using a weighting factor w (0 < w < 1, w close to 1). Then we might be seeing:

1)	Z[k] = w X[k] + .5 (1-w) (X[k-1] + X[k+1])

useful abbrevs to avoid typing: 

Z[k-2] == Zi, X[k-2] == Xi
Z[k-1] == Zj, etc
Z[k] == Zk, X[k] == Xk
Z[k+1] == Zl, etc
Z[k+2] == Zm, etc

Q == .5 (1-w) / w, will need later         # Q = .5 w=.5


so:

2) Zk = w Xk + .5 (1-w) (Xj + Xl)           # Zk = .5 3 + .5 .5 (2 +2) = 2.5

But, if we wanted to solve for Xk

3) Xk =  [ Zk - .5 (1-w) (Xj + Xl) ] / w     # (2.5 - .25 (2+2))) / .5 = 3
	  
4) Xk =   Zk / w - Q (Xj + Xl)               # 2.5 / .5 - .5 (2+2) = 3

5) Xj =   Zj / w - Q (Xi + Xk)               # 2 / .5 - .5 (1+3) = 2 

6) Xl =   Zl / w - Q (Xk + Xm)               # 2 / .5 - .5 (3+1) = 2

and substituting for Xj and Xl in 4) from 5) and 6)

7) Xk =   Zk / w - Q [ Zj / w - Q (Xi + Xk) + Zl / w - Q (Xk + Xm) ]

	# 2.5/.5 - .5 [2/.5 - .5(1+3) + 2/.5 - .5(3+1)]
	# 5 - .5[4 - 2 + 4 - 2] = 5 - .5[4] = 3

rearranging a bit

8) Xk =  Zk / w - Q [ (Zj + Zl) / w  - Q (Xi + Xm + 2 Xk) ]
								
	# 2.5/.5 - .5 [(2+2)/.5 - .5 (1 + 1 + 2 3)]
	# 5 - .5 [8 - .5 (8)] = 5 - .5 [4] = 3
								
9) Xk =  Zk / w - Q (Zj + Zl) / w + Q^2 (Xi + Xm) + 2 Q^2 Xk

	# 2.5/.5 - .5 (2+2)/.5 + .25 (1+1) + 2 .25 3
	# 5 - 4 + .5 + 1.5 = 3

moving all Xk terms to the left

10) (1 - 2 Q^2) Xk = Zk / w - Q (Zj + Zl) / w + Q^2 (Xi + Xm)

	# (1 - 2 .25)3 = 2.5/.5 - .5(2+2)/.5 + .25(1+1)
	# (.5)3 = 5 - 4 + .5

11) Xk = [ Zk / w - Q (Zj + Zl) / w + Q^2 (Xi + Xm) ] / (1 - 2 Q^2)

	# [2.5/.5 - .5(2+2)/.5 + .25(1+1)] / (1 - 2 .25)
	# [5 - 4 + .5] / .5 = 1.5/.5 = 3

12) Xk = Zk - Q (Zj + Zl) + w Q^2 (Xi + Xm)    
	 ---------------------------------
	    	w (1 - 2 Q^2)  
				
	# [2.5 - .5(2+2) + .5 .25 (1+1)] / [.5 (1 - 2 .25)}
	# [2.5 - 2 + .25] / .5 (.5)
	# .75 / .25 = 3

but we'd like it in this form

13) Xk = A Zk - B avg(Zj,Zl) + C avg(Xi,Xm)

where:

14) A = 1 / [w (1 - 2 Q^2)]	# 1 / [.5 (1 - 2 .25)] = 1 / (.5 .5) = 4

15) B = 2 Q / [w (1 - 2 Q^2)]   # 2 .5 / [.5 (1 - 2 .25)] = 1 / .25 = 4

16( C = 2 Q^2 / [1 - 2 Q^2]	# 2 .25 / [1 - 2 .25] = .5 / .5 = 1

		# from 13)  3 =  4 2.5 - 4 2 + 1 1 = 10 - 8 + 1 = 3

Note we still don't know the orig values of Xi but we are out of
patience, CPU, and math ability so we just assume Xi=Zi for the
remaining terms. Hopefully  it will be close.

*/

static BOOL
PullDown_VSharp			(TDeinterlaceInfo *	pInfo,
				 BOOL			SelectL2)
{
    v16 QA;
    v16 QB;
    v16 QC;
    uint8_t *WeaveDest;	/* dest for weave pixel */
    uint8_t *CopyDest;	/* other dest, copy or vertical filter */
    const uint8_t *pFieldStore;
    const uint8_t *Src1;
    const uint8_t *Src2;
    long dL1; /* offset to FieldStore elem holding top known pixels */
    long dL3; /* offset to FieldStore elem holding bottom known pxl */
    long dL2; /* offset to FieldStore elem holding newest weave pixels */
    long dCopySrc;
    unsigned int height;
    unsigned long dst2_offs;
    unsigned long src2_offs;
    int w = (GreedyVSharpnessAmt > 0) /* note-adj down for overflow */
	/* overflow, use 38%, 0<w<1000  */
	? 1000 - (GreedyVSharpnessAmt * 38 / 10)
	/* bias towards workable range */
	: 1000 - (GreedyVSharpnessAmt * 150 / 10);
    int Q = 500 * (1000 - w) / w; /* Q as 0 - 1K, max 16k */        
    int Q2 = (Q*Q) / 1000;        /* Q^2 as 0 - 1k */
    int denom = (w * (1000 - 2 * Q2)) / 1000; /* [w (1-2q^2)] as 0 - 1k */    
    int A = 64000 / denom;        /* A as 0 - 64 */
    int B = 128 * Q / denom;      /* B as 0 - 64 */
    int C =  ((w * Q2) / (denom*500)) ; /* C as 0 - 64 */

    C = 64 - A + B;

    pFieldStore = FieldStore;

    /* set up pointers, offsets */
    SIMD_NAME (SetFsPtrs)(&dL1, &dL2, &dL3, &dCopySrc,
			  &CopyDest, &WeaveDest, pInfo);

    /* chk forward/backward Greedy Choice Flag for this field */
    if (!SelectL2) {
	dL2 = FsPrevFrame (dL2);
    }

    /* Pick up first 2 and last 2 lines */

    SIMD_NAME (FieldStoreCopy)(CopyDest, pFieldStore + dCopySrc,
			       pInfo->LineLength);
    SIMD_NAME (FieldStoreCopy)(WeaveDest, pFieldStore + dL2,
			       pInfo->LineLength);

    {
	unsigned int last_row;

	last_row = pInfo->FieldHeight - 1;
	dst2_offs = 2 * last_row * pInfo->OverlayPitch;
	src2_offs = last_row * FS_BYTES_PER_ROW;
    }

    SIMD_NAME (FieldStoreCopy)(CopyDest + dst2_offs,
			       pFieldStore + dCopySrc + src2_offs,
			       pInfo->LineLength);
    SIMD_NAME (FieldStoreCopy)(WeaveDest + dst2_offs,
			       pFieldStore + dL2 + src2_offs,
			       pInfo->LineLength);

    CopyDest += 2 * pInfo->OverlayPitch;
    WeaveDest += 2 * pInfo->OverlayPitch;

    if (CopyDest < WeaveDest) {
        Src2 = pFieldStore + dCopySrc + FS_BYTES_PER_ROW;
        Src1 = pFieldStore + dL2;
    } else {
        Src2 = pFieldStore + dL2 + FS_BYTES_PER_ROW;
        Src1 = pFieldStore + dCopySrc;
        CopyDest = WeaveDest;
    }

    QA = vsplat16 (A);
    QC = vsplat16 (C);

    if (B < 0) {
	int MB = -B;

	QB = vsplat16 (MB);

        for (height = pInfo->FieldHeight - 2; height > 0; --height) {
            PullDown_VSoft2 (CopyDest, Src1, Src2, pInfo->LineLength,
			     QA, QB, QC, C);
	    CopyDest += pInfo->OverlayPitch;
	    Src1 += FS_BYTES_PER_ROW;
    
            PullDown_VSoft2 (CopyDest, Src2, Src1, pInfo->LineLength,
			     QA, QB, QC, C);
	    CopyDest += pInfo->OverlayPitch;
	    Src2 += FS_BYTES_PER_ROW;
	}
    } else {
	QB = vsplat16 (B);

        for (height = pInfo->FieldHeight - 2; height > 0; --height) {
            PullDown_VSharp2 (CopyDest, Src1, Src2, pInfo->LineLength,
			      QA, QB, QC, C);
	    CopyDest += pInfo->OverlayPitch;
	    Src1 += FS_BYTES_PER_ROW;
    
            PullDown_VSharp2 (CopyDest, Src2, Src1, pInfo->LineLength,
			      QA, QB, QC, C);
	    CopyDest += pInfo->OverlayPitch;
	    Src2 += FS_BYTES_PER_ROW;
	}
    }

    vempty ();

    return TRUE;
}

static const int PdMask2 = 0x0F * 0x21;		/* 0b01111 x 2 */
static const int PdMask32 = 0x0F * 0x108421;	/* 0b01111 x 5 */
static const int PdMerge2 = 0x05 * 0x21;	/* 0b00101 x 2 */
static const int Pd32Pattern = 0x05 * 0x108421;	/* 0b00101 x 5 */
static const int PdMask2b = 0x1B * 0x21;	/* 0b11011 x 2 */
static const int PdMerge2b = 0x09 * 0x21;	/* 0b01001 x 2 */

static always_inline int
is_3_2_pattern			(int			flags)
{
    return (((Pd32Pattern >> 0) == (flags & (PdMask32 >> 0))) |
	    ((Pd32Pattern >> 1) == (flags & (PdMask32 >> 1))) |
	    ((Pd32Pattern >> 2) == (flags & (PdMask32 >> 2))) |
	    ((Pd32Pattern >> 3) == (flags & (PdMask32 >> 3))) |
	    ((Pd32Pattern >> 4) == (flags & (PdMask32 >> 4))));
}

static always_inline int
is_2_2_pattern			(int			flags)
{
    flags &= 0xFFFFF;
    return ((0x55555 == flags) |
	    (0xAAAAA == flags));
}

/* check if we should do pulldown, doit */
BOOL
SIMD_NAME (CanDoPulldown)	(TDeinterlaceInfo *	pInfo)

{
    int hPtr = (HistPtr + 20 - FsDelay) % 20; /* curr HistPtr adj for delay*/
    int hPtrb = (HistPtr + 20 - 1) % 20;      /* curr HistPtr delay 1 */
    int FlagsW = Hist[hPtrb].Flags;    

    if (!GreedyUsePulldown
	|| Hist[hPtr].AvgChoice == 0
	|| Hist[hPtr].Avg == 0)	{
        Hist[hPtr].Flags2 |= PD_VIDEO;	/* flag did video, not pulldown */
	return FALSE;
    }

    /* If Greedy Matching doesn't help enough or this
       field comb raises avg too much then no good. */
    if (Hist[hPtr].Motion < GreedyLowMotionPdLvl * 10) {
	/* flag did pulldown due to low motion */
	Hist[hPtr].Flags2 |= PD_LOW_MOTION;
    } else if (is_3_2_pattern (FlagsW)) {
	/* flag found 20 bit 3:2 pulldown pattern */
        Hist[hPtr].Flags2 |= PD_32_PULLDOWN;
    } else if (0 && is_2_2_pattern (FlagsW)) {
	/* temp removed until it works better */
	/* flag found 20 bit 2:2 pulldown pattern */
        Hist[hPtr].Flags2 |= PD_22_PULLDOWN;
    } else if (1000 * Hist[hPtr].Avg
	       < GreedyGoodPullDownLvl * PDAVGLEN * Hist[hPtr].Comb) {
	/* flag did video, not pulldown */
        Hist[hPtr].Flags2 |= PD_VIDEO;
	return FALSE;
    }

    /* if (Hist[hPtr].CombChoice * PDAVGLEN * 100
           > Hist[hPtr].AvgChoice * GreedyBadPullDownLvl) */
    if (Hist[hPtr].CombChoice * 100
	> Hist[hPtr].Kontrast * GreedyBadPullDownLvl) {
	/* flag bad pulldown, did video */
	Hist[hPtr].Flags2 |= PD_BAD | PD_VIDEO;
	return FALSE;
    }

    /* We can do some sort of pulldown */
    Hist[hPtr].Flags2 |= PD_PULLDOWN;

    /* trb 11/15/01 add new Inverse Filtering */
    if (GreedyUseVSharpness && GreedyVSharpnessAmt) {
	/* Heck with it. Do old cheaper way for -100, gimmick */
	if (GreedyVSharpnessAmt == -100) {
	    return PullDown_V (pInfo, Hist[hPtr].Flags & 1);
	} else {
            return PullDown_VSharp (pInfo, Hist[hPtr].Flags & 1);
        }
    }

    if (GreedyUseInBetween) {
	/* for FsDelay == 2 do inbetween for (delay 1)
	   flags 01x0101x01 pattern only */
        if (FsDelay == 2) {
            if ((Hist[hPtrb].Flags & PdMask2b) == PdMerge2b) {
		Hist[hPtr].Flags2 |= PD_MERGED;
		return PullDown_InBetween (pInfo);
            }
        } else {
	    /* for FsDelay == 1 do inbetween for
	       flags x0101x0101 pattern only */
            if ((Hist[hPtr].Flags & PdMask2) == PdMerge2) {
		Hist[hPtr].Flags2 |= PD_MERGED;
		return PullDown_InBetween (pInfo);
            }
        }
    }

    /* OK, do simple pulldown */

    {	
	uint8_t *WeaveDest;	/* dest for weave pixel */
	uint8_t *CopyDest;	/* other dest, copy or vertical filter */
	const uint8_t *pL2;	/* ptr into FieldStore[L2] */
	const uint8_t *pCS;	/* ptr into FieldStore[CopySrc] */
	long dL1; /* offset to FieldStore elem holding top known pixels */
	long dL3; /* offset to FieldStore elem holding bottom known pxl */
	long dL2; /* offset to FieldStore elem holding newest weave pixels */
	long dCopySrc;
	unsigned int height;
	unsigned long dst_padding;
	unsigned long src_padding;

	/* set up pointers, offsets */
	SIMD_NAME (SetFsPtrs)(&dL1, &dL2, &dL3, &dCopySrc,
			      &CopyDest, &WeaveDest, pInfo);

	/* chk forward/backward Greedy Choice Flag for this field */
	if (!(Hist[hPtr].Flags & 1)) {
	    dL2 = FsPrevFrame (dL2);
	}

	pL2 = (const uint8_t *) FieldStore + dL2;
	pCS = (const uint8_t *) FieldStore + dCopySrc;

	dst_padding = 2 * pInfo->OverlayPitch;
	src_padding = FS_BYTES_PER_ROW;

	for (height = pInfo->FieldHeight; height > 0; --height) {
	    SIMD_NAME (FieldStoreCopy)(CopyDest, pCS, pInfo->LineLength);
	    CopyDest += dst_padding;
	    pCS += src_padding;

	    SIMD_NAME (FieldStoreCopy)(WeaveDest, pL2, pInfo->LineLength);
	    WeaveDest += dst_padding;
	    pL2 += src_padding;
	}
    }

    vempty ();

    return TRUE;
}

#endif /* SIMD */

/*
Local Variables:
c-basic-offset: 4
End:
 */

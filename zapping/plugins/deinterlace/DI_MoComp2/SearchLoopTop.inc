/////////////////////////////////////////////////////////////////////////////
// $Id: SearchLoopTop.inc,v 1.1 2005-01-08 14:54:24 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2003 Tom Barry & John Adcock.  All rights reserved.
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
//
//  (From Tom Barry)
//  Also, this program is "Philanthropy-Ware".  That is, if you like it and 
//  feel the need to reward or inspire the author then please feel free (but
//  not obligated) to consider joining or donating to the Electronic Frontier
//  Foundation. This will help keep cyber space free of barbed wire and bullsh*t.  
//  See www.eff.org for details
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.2  2003/02/11 15:41:00  adcockj
// New implementation of StrangeBob based off paper
// Tidy up sources
//
// Revision 1.1  2003/01/02 13:15:01  adcockj
// Added new plug-ins ready for developement by copying TomsMoComp and Gamma
//
/////////////////////////////////////////////////////////////////////////////

__int64 Save1;
__int64 Save2;
BYTE*	pDest;
const BYTE*	pSrcP;
const BYTE*	pSrc;
const BYTE*	pBob;
const BYTE*	pBobP;

int	    src_pitch2 = src_pitch;			// even & odd lines are not interleaved in DScaler

int	    dst_pitch2 = 2 * dst_pitch;
int     y;

int     Last8 = (rowsize-8);			// ofs to last 8 bytes in row

int		SaveMotion = 0;
int		dst_pitchw = dst_pitch; // local stor so asm can ref
	pSrc  = pWeaveSrc;			// points 1 weave line above
	pSrcP = pWeaveSrcP;			// " 
	pDest = pWeaveDest + dst_pitch2;

	if (TopFirst)
	{
		pBob = pCopySrc + src_pitch2;      // remember one weave line just copied previously
		pBobP = pCopySrcP + src_pitch2;
	}
	else
	{
		pBob =  pCopySrc;
		pBobP =  pCopySrcP;
	}

	for (y=1; y < FldHeight-1; y++)	
	{
_saved_regs;

		// pretend it's indented -->>
_asm_begin
"		## Loop general reg usage\n"
"		##\n"
"		## eax - pBobP, then pDest \n"
"		## ebx - pBob\n"
"		## ecx - src_pitch2\n"
"		## edx - current offset\n"
"		## edi - prev weave pixels, 1 line up\n"
"		## esi - next weave pixels, 1 line up\n"
"\n"
"\n"
"\n"
"\n"
"		## simple bob first 8 bytes\n"
"		mov		ebx, %[pBob]\n"
"		mov		ecx, %[src_pitch2]\n"
"\n"
"		movq	mm0, qword ptr[ebx]\n"
"##		pavgb	mm0, qword ptr[ebx+ecx]\n"
		V_PAVGB (mm0, qword ptr[ebx+ecx], mm2, ShiftMask)
"		mov		edi, %[pDest]\n"
		V_MOVNTQ	(qword ptr[edi], mm0)
"\n"
"		## simple bob last 8 bytes\n"
"		mov		edx, %[Last8]\n"
"		lea		esi, [ebx+edx]\n"
"		movq	mm0, qword ptr[esi]\n"
"##		pavgb	mm0, qword ptr[esi+ecx]\n"
		V_PAVGB	(mm0, qword ptr[esi+ecx], mm2, ShiftMask)
		V_MOVNTQ	(qword ptr[edi+edx], mm0)
"\n"
"		## now loop and get the middle qwords\n"
"		mov		esi, %[pSrc]\n"
"		mov		edi, %[pSrcP]\n"
"		mov		edx, 8				## curr offset into all lines\n"
"\n"
"LoopQ:	\n"
"		mov		eax, %[pBobP]\n"
"		add		edi, 8		\n"
"		add		esi, 8		\n"
"		add		ebx, 8\n"
"		add		eax, edx\n"

#include "StrangeBob.inc"

/////////////////////////////////////////////////////////////////////////////
// $Id: StrangeBob.inc,v 1.1 2005-01-08 14:54:24 mschimek Exp $
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
// Revision 1.4  2003/05/30 10:05:49  adcockj
// Improvements to diagonal selection
//
// Revision 1.3  2003/05/26 15:54:36  adcockj
// Improved Performance of motion detection
// Fixed bug with pBobP
//
// Revision 1.2  2003/02/11 15:41:05  adcockj
// New implementation of StrangeBob based off paper
// Tidy up sources
//
// Revision 1.1  2003/01/02 13:15:01  adcockj
// Added new plug-ins ready for developement by copying TomsMoComp and Gamma
//
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// This file contains an implementation of the alogorithm described in:
// T. Chen, H.R. Wu; & Z.H. Yu, 
// "Efficient deinterlacing algorithm using edge-based line average interpolation", 
// Optical Engineering, Volume 39, Issue 8, (2000)
/////////////////////////////////////////////////////////////////////////////
"\n"
"	\n"
"	## First, get and save our possible Bob values\n"
"	## Assume our pixels are layed out as follows with x the calc'd bob value\n"
"	## and the other pixels are from the current field\n"
"	##  \n"
"	##		   a b c 		current field\n"
"	##           x			calculated line\n"
"	##         d e f 		current field\n"
"	##\n"
"\n"
"	movq    mm1, qword ptr[ebx-2]\n"
"	movq    mm2, qword ptr[ebx]\n"
"	movq    mm3, qword ptr[ebx + 2]\n"
"\n"
"	movq    mm4, qword ptr[ebx+ecx - 2]\n"
"	movq    mm5, qword ptr[ebx+ecx]\n"
"	movq    mm6, qword ptr[ebx+ecx + 2]\n"
"\n"
"    ## mm1 = a\n"
"    ## mm2 = b\n"
"    ## mm3 = c\n"
"    ## mm4 = d\n"
"    ## mm5 = e\n"
"    ## mm6 = f\n"
"\n"
"    movq mm7, mm1\n"
    V_PAVGB(mm7, mm6, mm0, ShiftMask)
"    movq %[Save1], mm7\n"
"    \n"
"    movq mm7, mm3\n"
    V_PAVGB(mm7, mm4, mm0, ShiftMask)
"    movq %[Save2], mm7\n"
"\n"
"    movq mm7, mm1\n"
    PABS(mm1, mm6, mm0)
    PABS(mm6, mm2, mm0)
    PABS(mm7, mm5, mm0)
    V_PAVGB(mm6, mm7, mm0, ShiftMask)
"\n"
"    movq mm7, mm3\n"
    PABS(mm3, mm4, mm0)
    PABS(mm4, mm2, mm0)
    PABS(mm7, mm5, mm0)
    V_PAVGB(mm4, mm7, mm0, ShiftMask)
"\n"
"    movq mm7, mm2\n"
    PABS(mm2, mm5, mm0)
    V_PAVGB(mm5, mm7, mm0, ShiftMask)
"\n"
"    ## at this point the  following registers are set\n"
"    ## mm1 = |a - f|\n"
"    ## mm2 = |b - e|\n"
"    ## mm3 = |c - d|\n"
"    ## mm4 = avg(|b - d|, |c - e|)\n"
"    ## mm5 = avg(b, e)\n"
"    ## mm6 = avg(|a - e|, |b - f|)\n"
"    ## Save1 = avg(a, f)\n"
"    ## Save2 = avg(c, d)\n"
"    \n"
"	pxor	mm7, mm7			\n"
"	pxor	mm0, mm0			\n"
"	psubusb mm4, mm6			## nonzero where mm4 > mm6 \n"
"	pcmpeqb mm4, mm0			## now ff where mm4 <= mm6\n"
"    movq    mm6, mm4\n"
"    pcmpeqb mm6, mm0            ## now ff where mm6 < mm4\n"
"\n"
"\n"
"    ## make the corresponding one unsuitable\n"
"    paddusb mm1, mm4\n"
"    paddusb mm3, mm6\n"
"\n"
"    movq mm4, mm1\n"
"    movq mm6, mm3\n"
"    psubusb mm1, mm3            ## nonzero  where (|a - f| > |c - d|)\n"
"	pcmpeqb mm1, mm0			## now ff where (|a - f| < |c - d|)\n"
"    movq mm7, mm1\n"
    COMBINE(mm4, mm6, mm1)
"    movq mm6, %[Save1]\n"
"    movq mm3, %[Save2]\n"
    COMBINE(mm6, mm3, mm7)     
"\n"
"    ## so now we should have mm4 as current best diff and mm6 as best average\n"
"    ## of the diagonals\n"
"    movq    mm1, mm2           ## save |b - e|\n"
"    psubusb mm2, mm4           ## nonzero  where (|b - e| >= Best)\n"
"	pcmpeqb mm2, mm0		   ## now ff where (|b - e| < Best)\n"
"    por mm2, UVMask            ## we only want luma from diagonals\n"
"    psubusb mm1, MOVE          ## nonzero where |b - e| > MOVE is Bob is visible\n"
"	pcmpeqb mm1, mm0		   ## now ff where (|b - e| <= MOVE)\n"
"    por mm2, mm1               ## we'll let bob through always if the diff is small\n"
"\n"
    COMBINE(mm5, mm6, mm2)     
"    movq mm6, mm5\n"

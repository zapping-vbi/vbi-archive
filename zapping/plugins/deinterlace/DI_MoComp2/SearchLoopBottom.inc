    /////////////////////////////////////////////////////////////////////////////
// $Id: SearchLoopBottom.inc,v 1.1 2005-01-08 14:54:24 mschimek Exp $
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
// Revision 1.4  2003/06/26 09:07:19  adcockj
// Corrected comments
//
// Revision 1.3  2003/05/26 15:54:36  adcockj
// Improved Performance of motion detection
// Fixed bug with pBobP
//
// Revision 1.2  2003/02/11 15:40:54  adcockj
// New implementation of StrangeBob based off paper
// Tidy up sources
//
// Revision 1.1  2003/01/02 13:15:01  adcockj
// Added new plug-ins ready for developement by copying TomsMoComp and Gamma
//
/////////////////////////////////////////////////////////////////////////////

"    ## these should be the values held as we go in\n"
"    ## mm0 = 0\n"
"    ## mm1 = weave pixels\n"
"    ## mm3 = \"movement\" in the centre\n"
"    ## mm6 = Bob pixels          \n"
"\n"
"    ## operate only on luma as we will always bob the chroma\n"
"    pand    mm3, YMask\n"
"\n"
"    movq	mm2, qword ptr[ebx]\n"
"    movq	mm4, qword ptr[eax]\n"
"\n"
    PABS(mm2, mm4, mm5)
"    pand    mm2, YMask\n"
"\n"
"    movq	mm4, qword ptr[ebx + ecx]\n"
"    movq	mm7, qword ptr[eax + ecx]\n"
"\n"
    PABS(mm4, mm7, mm5)
"    pand    mm4, YMask\n"
"    \n"
"    ## mm0 = 0\n"
"    ## mm1 = weave pixels\n"
"    ## mm2 = \"movement\" in the top\n"
"    ## mm3 = \"movement\" in the centre\n"
"    ## mm4 = \"movement\" in the bottom\n"
"    ## mm6 = Bob pixels          \n"
"\n"
"    psubusb mm2, MOVE           ## non-zero where mm2 > MOVE i.e. Movement\n"
"    pcmpeqd mm2, mm0            ## FFFF where the luma has no movement in two pixels\n"
"    pcmpeqd mm2, mm0            ## all ff where movement in either of the two pixels\n"
"\n"
"    psubusb mm3, MOVE           ## non-zero where mm3 > TENS i.e. Movement\n"
"    pcmpeqd mm3, mm0            ## FFFF where the luma has no movement in two pixels\n"
"    pcmpeqd mm3, mm0            ## all ff where movement in either of the two pixels\n"
"\n"
"    psubusb mm4, MOVE           ## non-zero where mm4 > TENS i.e. Movement\n"
"    pcmpeqd mm4, mm0            ## FFFF where the luma has no movement in two pixels\n"
"    pcmpeqd mm4, mm0            ## all ff where movement in either of the two pixels\n"
"\n"
"    pand  mm2, mm4              ## top and bottom moving\n"
"    por  mm3, mm2               ## where we should bob\n"
"\n"
"    por mm3, UVMask\n"
"\n"
    COMBINE(mm6, mm1, mm3)
"\n"
"    mov     eax, %[pDest]\n"
"\n"
    V_MOVNTQ (qword ptr[eax+edx], mm6)
"\n"
"    lea     edx, [edx+8]        ## bump offset pointer\n"
"    cmp     edx, %[Last8]          ## done with line?\n"
"    jb      LoopQ               ## y\n"
"    \n"
_asm_end,
_m(pBob), _m(src_pitch2), _m(pDest), _m(pSrc),
_m(pSrcP), _m(pBobP), _m(Save1), _m(Save2), _m(Last8),
/* Static consts gcc must not optimize away. */
_m(YMask), _m(UVMask), _m(ShiftMask), _m(MOVE)
: "eax", "ecx", "edx", "esi", "edi");

    // adjust for next line
    pSrc  += src_pitch2;
    pSrcP += src_pitch2;
    pDest += dst_pitch2;
    pBob += src_pitch2;
    pBobP += src_pitch2;
}

/////////////////////////////////////////////////////////////////////////////
// $Id: DI_GreedyDeLoop.asm,v 1.1 2005-01-08 14:54:23 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2001 Tom Barry  All rights reserved.
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
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.3  2001/08/01 00:37:41  trbarry
// More chroma jitter fixes, tweak defaults
//
// Revision 1.2  2001/07/25 12:04:31  adcockj
// Moved Control stuff into DS_Control.h
// Added $Id and $Log to comment blocks as per standards
//
/////////////////////////////////////////////////////////////////////////////

// This file contains the body of the Greedy High Motion Deinterlace loop
// It may be included in the body of the real loop multiple times for performance
// as may graphics cards still like at least 32 bytes written at a time.

// The following 4 values should be defined before using this include but may be removed for debugging:
// #define USE_JAGGIE_REDUCTION
// #define USE_GREEDY_CHOICE
// #define USE_CLIP
// #define USE_BOB_BLEND

// The Value FSOFFS must be defined before including this header.
//
// On exit mm2 will contain the value of the calculated weave pixel, not yet stored.
// It is also expected that mm1 and mm3 will still contain the vertically adjacent pixels
// which may be needed for the vertical filter.
"		movq	mm0, qword ptr[esi+16+" _strf(FSOFFS) "]  ## L2 - the other one \n"
"		movq	mm1, qword ptr[esi+eax+" _strf(FSOFFS) "]	## L1\n"
"		movq	mm2, qword ptr[esi+" _strf(FSOFFS) "]		## L2 \n"
"		movq	mm3, qword ptr[esi+ebx+" _strf(FSOFFS) "] ## L3\n"
"\n"
"## calc simple interp value in case we need it			\n"
"		movq	mm6, mm1				## L1 - get simple single pixel interp\n"
"		pavgb   mm6, mm3\n"
"\n"
"## DJR - Diagonal Jaggie Reduction\n"
"## In the event that we are going to use an average (Bob) pixel we do not want a jagged\n"
"## stair step effect.  To combat this we will average in the 2 horizontally adjacent\n"
"## pixels into the interp Bob mix.\n"
"\n"
#ifdef USE_JAGGIE_REDUCTION				// always used but can turn off for test
"		movq    mm4, %[LastAvg]			## the bob value from prev qword in row\n"
"		movq	%[LastAvg], mm6			## save for next pass\n"
"		psrlq   mm4, 48					## right justify 1 pixel\n"
"		movq	mm7, mm6				## copy of simple bob pixel\n"
"		psllq   mm7, 16                 ## left justify 3 pixels\n"
"		por     mm4, mm7				## and combine\n"
"		\n"
"		movq	mm5, qword ptr[esi+eax+" _strf(FSCOLSIZE+FSOFFS) "] ## next horiz qword from L1\n"
"		pavgb   mm5, qword ptr[esi+ebx+" _strf(FSCOLSIZE+FSOFFS) "] ## next horiz qword from L3\n"
"		psllq	mm5, 48					## left just 1 pixel\n"
"		movq	mm7, mm6                ## another copy of simple bob pixel\n"
"		psrlq   mm7, 16					## right just 3 pixels\n"
"		por		mm5, mm7				## combine\n"
"		pavgb	mm4, mm5				## avg of forward and prev by 1 pixel\n"
"		pavgb	mm6, mm4				## avg of center with adjacent\n"
"        pavgb	mm4, mm6				## 1/4 center, 3/4 adjacent\n"
"    	pavgb	mm6, mm4				## 3/8 center, 5/8 adjacent\n"
"\n"
#endif										// end of jaggie reduction code
"\n"
"## Greedy Choice\n"
"## For a weave pixel candidate we choose whichever (preceding or following) pixel that would\n"
"## yield the lowest comb factor. This allows the possibilty of selecting choice pixels from 2\n"
"## different field.\n"
#ifdef USE_GREEDY_CHOICE				// always use but can turn off for test
"## get abs value of possible L2 comb\n"
"		movq    mm4, mm6				## work copy of interp val\n"
"		movq	mm7, mm2				## L2\n"
"		psubusb mm7, mm4				## L2 - avg\n"
"		movq	mm5, mm4				## avg\n"
"		psubusb mm5, mm2				## avg - L2\n"
"		por		mm5, mm7				## abs(avg-L2)\n"
"\n"
"## get abs value of possible LP2 comb\n"
"		movq	mm7, mm0				## LP2\n"
"		psubusb mm7, mm4				## LP2 - avg\n"
"		psubusb	mm4, mm0				## avg - LP2\n"
"		por		mm4, mm7				## abs(avg-LP2)\n"
"\n"
"## use L2 or LP2 depending upon which makes smaller comb\n"
"		psubusb mm4, mm5				## see if it goes to zero\n"
"		psubusb mm5, mm5				## 0\n"
"		pcmpeqb mm4, mm5				## if (mm4=0) then FF else 0\n"
"		pcmpeqb mm5, mm4				## opposite of mm4\n"
"\n"
"## if Comb(LP2) <= Comb(L2) then mm4=ff, mm5=0 else mm4=0, mm5 = 55\n"
"		pand	mm5, mm2				## use L2 if mm5 == ff, else 0\n"
"		pand	mm4, mm0				## use LP2 if mm4 = ff, else 0\n"
"		por		mm4, mm5				## may the best win\n"
#else
"		movq	mm4, mm2				## just use L2P for Weave pixel\n"
#endif										// end of Greedy choice code
"\n"
"\n"
"## Inventory: at this point we have the following values:\n"
"## mm0 = L2\n"
"## mm1 = L1\n"
"## mm2 = LP2\n"
"## mm3 = L3\n"
"## mm4 = the best of L2,LP2 weave pixel, based upon comb \n"
"## mm6 = the avg interpolated value, if we need to use it\n"
"\n"
"## Let's measure movement, as how much the weave pixel has changed\n"
"		movq	mm7, mm2\n"
"		psubusb mm2, mm0\n"
"		psubusb mm0, mm7\n"
"		por		mm0, mm2				## abs value of change, used later\n"
"\n"
#ifdef USE_CLIP							// always use but can turn off for test
"## Now lets clip our chosen weave pixel value to be not outside of the range\n"
"## of the high/low range L1-L3 by more than MaxComb.\n"
"## This allows some comb but limits the damages and also allows more\n"
"## detail than a boring oversmoothed clip.\n"
"		movq	mm2, mm1				## copy L1\n"
"		pmaxub	mm2, mm3                ## now = Max(L1,L3)\n"
"		movq	mm5, mm1				## copy L1\n"
"		pminub	mm5, mm3				## now = Min(L1,L3)\n"
"## allow the value to be above the high or below the low by amt of MaxComb\n"
"		psubusb mm5, %[MaxCombW]			## lower min by diff\n"
"		paddusb	mm2, %[MaxCombW]			## increase max by diff\n"
"		pmaxub	mm4, mm5				## now = Max(best,Min(L1,L3)\n"
"		pminub	mm4, mm2 				## now = Min( Max(best, Min(L1,L3), L2 )=L2 clipped\n"
#endif										// end of clip code
"        movq    mm2, mm4                ## save copy of clipped val for luma\n"
"\n"
#ifdef USE_BOB_BLEND					// always use but can turn off for test
"## the ratio of bob/weave will be dependend upon apparent damage we expect\n"
"## from seeing large motion. \n"
"		psubusb mm0, %[MotionThresholdW]  ## test motion Threshold, clear chroma\n"
"		pmullw  mm0, %[MotionSenseW] ## mul by user factor, keep low 16 bits\n"
"		movq    mm7, %[QW256]\n"
"		pminsw  mm0, mm7				## max = 256\n"
"		psubusw mm7, mm0				## so the 2 sum to 256, weighted avg\n"
"		pand	mm4, %[YMaskW]				## keep only luma from clipped weave value\n"
"		pmullw  mm4, mm7				## use more weave for less motion\n"
"		pand	mm6, %[YMaskW]				## keep only luma from interp bob DJR value\n"
"		pmullw  mm6, mm0				## use more bob for large motion\n"
"		paddusw mm4, mm6				## combine\n"
"		psrlw   mm4, 8					## div by 256 to get weighted avg	\n"
#endif										// end of motion sensitive bob blend
"\n"
"## chroma comes from our clipped weave value - gives more chroma res & lower chroma jitter\n"
"		pand    mm2, %[UVMask]             ## get only chroma\n"
"		por		mm4, mm2				## and combine\n"

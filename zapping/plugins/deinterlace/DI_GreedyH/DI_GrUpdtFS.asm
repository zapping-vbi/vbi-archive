/////////////////////////////////////////////////////////////////////////////
// $Id: DI_GrUpdtFS.asm,v 1.1 2005-01-08 14:54:23 mschimek Exp $
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
// Revision 1.5  2001/11/25 04:33:37  trbarry
// Fix for TDeinterlace_Info. Also release UN-Filter code, 5-tap V & H sharp/soft filters optimized to reverse excessive filtering (or EE?)
//
// Revision 1.4  2001/08/17 16:18:35  trbarry
// Minor GreedyH performance Enh.
// Only do pulldown calc when needed.
// Will become more needed in future when calc more expensive.
//
// Revision 1.3  2001/08/04 06:46:57  trbarry
// Make Gui work with Large fonts,
// Improve Pulldown
//
// Revision 1.2  2001/07/25 12:04:31  adcockj
// Moved Control stuff into DS_Control.h
// Added $Id and $Log to comment blocks as per standards
//
/////////////////////////////////////////////////////////////////////////////



// The following 2 values may be defined before using this include.
// FUNC_NAME must be defined.
// #define USE_SHARPNESS		
// #define USE_MEDIAN_FILTER
// #define FUNC_NAME DI_GrUpdtFS_NM_NE_P
BOOL FUNC_NAME()
{
_saved_regs;
#include "DI_GreedyHM2.h"
	__int64* pFieldStore;		// ptr into FieldStore qwords
    __int64 lastmm3 = 0;        // >>> for debug only
//>>>	short **pLinesW = pLines;	// current input lines, local storage is faster
	BYTE *pLinesW = pLines;	// current input lines, local storage is faster
	int LineCtr = FieldHeight;	// number of lines to do
    int SkipCt = FieldHeight / 4;   // skip this many at top and bottom
    int FirstLine = LineCtr - SkipCt+1;  // don't use top n lines in totals, re-clear totals here
    int LastLine = SkipCt+1;    // don't use last n lines in totals, save totals here
	int	LoopCtr;				// number of qwords in line - 1
	int	LoopCtrW;				// number of qwords in line - 1
	int FsPrev;					// FieldStore elem holding pixels from prev field line
	int FsPrev2;				// Offset to prev pixel (this line) to be median filtered
	int FsNewOld;				// FieldStore elem holding oldest then newest
	int Motion = 0;				// our scaled motion total	
	int CombSum = 0;			// our scaled comb total
	int ContrSum = 0;			// our scaled contrast total
	int CombScale;				// multiplier to keep comb as 100 * avg/pixel	

#ifdef USE_SHARPNESS
// For the math, see the comments on the PullDown_VSharp() function in the Pulldown code
    int w = (GreedyHSharpnessAmt > 0)                 // note-adj down for overflow
            ? 1000 - (GreedyHSharpnessAmt * 38 / 10)  // overflow, use 38%, 0<w<1000                                             
            : 1000 - (GreedyHSharpnessAmt * 150 / 10); // bias towards workable range 
    int Q = 500 * (1000 - w) / w;                      // Q as 0 - 1K, max 16k        
    int Q2 = (Q*Q) / 1000;                             // Q^2 as 0 - 1k
    int denom = (w * (1000 - 2 * Q2)) / 1000;          // [w (1-2q^2)] as 0 - 1k    
    int A = 64000 / denom;                             // A as 0 - 64
    int B = 128 * Q / denom;                           // B as 0 - 64
    int C = 64 - A + B;                                // so A-B+C=64, unbiased weight
    __int64 i;
    i = A;                          
    QHA = i << 48 | i << 32 | i << 16 | i;

#ifdef REALLY_USE_SOFTNESS
    i = -B;
    QHB = i << 48 | i << 32 | i << 16 | i;
#else
    i = B;
    QHB = i << 48 | i << 32 | i << 16 | i;
#endif

    i = C;
    QHC = i << 48 | i << 32 | i << 16 | i;
#endif

	if (pLines == NULL)
		return FALSE;

// perc and adjust our global FieldStore subscripts
	FsPtrP3 = FsPtrP2;			// now is subscript of oldest field 
	FsPtrP2 = FsPtrP;			// now is subscript of prev field same parity
	FsPtrP = FsPtr;				// now is subscript of prev field
	FsPtr = (1+FsPtr) % 4;      // bump to nex

	FsNewOld = FsPtr * 8;		// Offset to Oldest odd pixel, will be replaced
	FsPrev = FsPtrP * 8;		// FieldStore elem holding pixels from prev field line
	FsPrev2 = FsPtrP2 * 8;		// Offset to prev pixel (this line) to be median filtered

	LineCtr = FieldHeight;		// number lines to do

	CombScale = (FieldHeight - 2 * SkipCt) * LineLength / 100;  // Divide totals by this later
	pFieldStore = & FieldStore[0];		// starting ptr into FieldStore
	
	LoopCtr = LineLength / 8 - 1;		// do 8 bytes at a time, adjusted
	LoopCtrW = LoopCtr;
	
_asm_begin
_save(eax)
_save(ecx)
_save(edx)
_save(esi)
_save(edi)
"		mov		esi, %[pLinesW]				## get ptr to line ptrs\n"
"		mov		edi, %[pFieldStore]## addr of our 1st qword in FieldStore\n"
"		mov		ecx, %[FsNewOld]   ## where to find oldest,save new pixel\n"
"		mov		edx, %[FsPrev2]	## The prev possible weave pixel\n"
"		pxor	mm3, mm3					## clear comb & Kontrast totals\n"
"		xor		ebx, ebx					## clear motion totol\n"
"\n"
"		.align 8\n"
"1:	# LineLoop:\n"
"		mov		eax, %[FsPrev]		## offset to pixels from prev line 	\n"
"\n"
#ifdef USE_SHARPNESS
"## If we are using edge enhancement then the asm loop will expect to be entered\n"
"## with mm0,mm1,mm2 holding the left, middle, and right pixel qwords\n"
"## On the last pass we will enter the loop at QwordLoop2 to avoid pixels off the end of the line.\n"
"		movq	mm1, qword ptr[esi]		## curr qword\n"
"		movq	mm0, mm1				## also pretend is left pixels\n"
"\n"
"		.align 8\n"
"2:	# QwordLoop:\n"
"		movq	mm2, qword ptr[esi+8]	## pixels to the right, for edge enh.\n"
"3:	# QwordLoop2:\n"
"## get avg of -2 & +2 pixels\n"
"        movq    mm5, mm0                ## save copy before trashing \n"
"		movq	mm7, mm1				## work copy of curr pixel val\n"
"		psrlq   mm0, 48					## right justify 1 pixel from qword to left\n"
"		psllq   mm7, 16                 ## left justify 3 pixels\n"
"		por     mm0, mm7				## and combine\n"
"		\n"
"		movq	mm6, mm2				## copy of right qword pixel val\n"
"		psllq	mm6, 48					## left just 1 pixel from qword to right\n"
"		movq	mm7, mm1                ## another copy of L2N current\n"
"		psrlq   mm7, 16					## right just 3 pixels\n"
"		por		mm6, mm7				## combine\n"
"		pavgb	mm0, mm6				## avg of forward and prev by 1 pixel\n"
"        pand    mm0, %[YMask]\n"
"        pmullw  mm0, %[QHB]                ## proper ratio to use\n"
"                    \n"
"## get avg of -2 & +2 pixels and subtract\n"
"		movq	mm7, mm1				## work copy of curr pixel val\n"
"		psrlq   mm5, 32					## right justify 2 pixels from qword to left\n"
"		psllq   mm7, 32                 ## left justify 2 pixels\n"
"		por     mm5, mm7				## and combine\n"
"		\n"
"		movq	mm6, mm2				## copy of right qword pixel val\n"
"		psllq	mm6, 32					## left just 2 pixels from qword to right\n"
"		movq	mm7, mm1                ## another copy of L2N current\n"
"		psrlq   mm7, 32					## right just 2 pixels\n"
"		por		mm6, mm7				## combine\n"
"		pavgb	mm5, mm6				## avg of forward and prev by 1 pixel\n"
"        pand    mm5, %[YMask]\n"
"        pmullw  mm5, %[QHC]                ## proper ratio to use\n"
"\n"
"## get ratio of center pixel and combine\n"
"        movq    mm7, mm1\n"
"        pand    mm7, %[YMask]              ## only luma\n"
"        pmullw  mm7, %[QHA]                ## weight it\n"
#ifdef REALLY_USE_SOFTNESS
"        paddusw mm7, mm5                ## add in weighted average of Zj,Zl               \n"
"        paddusw mm7, mm0                ## adjust\n"
#else
"        psubusw mm0, mm5                ## add in weighted average of Zj,Zl               \n"
"        psubusw mm7, mm0                ## adjust\n"
#endif                               
"        psrlw   mm7, 6				    ## should be our luma answers\n"
"        pminsw  mm7, %[YMask]              ## avoid overflow\n"
"		movq    mm0, mm1				## migrate for next pass through loop\n"
"        pand    mm1, %[UVMask]             ## get chroma from here\n"
"        por     mm7, mm1                ## combine luma and chroma\n"
"\n"
"		movq	mm4, qword ptr[edi+eax]	## prefetch FsPrev, need later\n"
"		movq    mm1, mm2				## migrate for next pass through loop\n"
"                    \n"
/* >>> save old way
"## do edge enhancement. \n"
"		movq	mm7, mm1				## work copy of curr pixel val\n"
"		psrlq   mm0, 48					## right justify 1 pixel from qword to left\n"
"		psllq   mm7, 16                 ## left justify 3 pixels\n"
"		por     mm0, mm7				## and combine\n"
"		\n"
"		movq	mm6, mm2				## copy of right qword pixel val\n"
"		psllq	mm6, 48					## left just 1 pixel from qword to right\n"
"		movq	mm7, mm1                ## another copy of L2N current\n"
"		psrlq   mm7, 16					## right just 3 pixels\n"
"		por		mm6, mm7				## combine\n"
"		pavgb	mm0, mm6				## avg of forward and prev by 1 pixel\n"
"\n"
"## we handle the possible plus and minus sharpness adjustments separately\n"
"		movq    mm7, mm1				## another copy of L2N\n"
"		psubusb mm7, mm0				## curr - surround\n"
"		pand	mm7, YMask\n"
"		pmullw  mm7, HSharpnessAmt          ## mult by sharpness factor\n"
"		psrlw   mm7, 8					## now have diff*HSharpnessAmt/256 ratio			\n"
"\n"
"		psubusb mm0, mm1                ## surround - curr\n"
"		pand	mm0, YMask\n"
"		pmullw  mm0, HSharpnessAmt          ## mult by sharpness factor\n"
"		psrlw   mm0, 8					## now have diff*HSharpnessAmt/256 ratio			\n"
"\n"
"		paddusb mm7, mm1				## edge enhancement up\n"
"		psubusb mm7, mm0                ## edge enhancement down, mm7 now our sharpened value\n"
"		movq	mm4, qword ptr[edi+eax]	## prefetch FsPrev, need later\n"
"		movq    mm0, mm1				## migrate for next pass through loop\n"
"		movq    mm1, mm2				## migrate for next pass through loop\n"
>>>> old way */
#else
"\n"
"## If we are not using edge enhancement we just need the current value in mm1\n"
"2:	# QwordLoop:\n"
"		movq    mm1, qword ptr[esi]		## no sharpness, just get curr value\n"
"		movq	mm4, qword ptr[edi+eax]	## prefetch FsPrev, need later\n"
"		movq	mm7, mm1				## work copy of curr pixel val\n"
#endif										// end of sharpness code
"\n"
"		movq    mm2, qword ptr[edi+ecx]  ## FsNewOld, fetch before store new pixel\n"
"		movq    qword ptr[edi+ecx], mm7 ## save our sharp new value for next time\n"
"	\n"
#ifdef USE_PULLDOWN	
"## Now is a good time to calc comb, contrast, and motion\n"
"##>>> need to optimize this again >>>\n"
"        pand    mm4, %[YMask]\n"
"		movq	mm5, mm4				## work copy of FsPrev\n"
"        movq    mm6, mm7\n"
"        pand    mm6, %[YMask]\n"
"		psadbw  mm4, mm6				## sum of abs differences is comb\n"
"        \n"
"        movq    mm6, %[YMask]\n"
"		pand    mm6, qword ptr[edi+eax+" _strf(FSROWSIZE) "]\n"
"		psadbw  mm5, mm6               	## sum of abs differences is contrast\n"
"		punpckldq mm4, mm5				## move mm5 to high dword of mm4\n"
"		paddd   mm3, mm4				## and accum result(s)\n"
"\n"
"		movq	mm5, qword ptr[edi+edx] ## pixels from previous field, same row & col\n"
"        pand    mm5, %[YMask]\n"
"		movq	mm6, mm7\n"
"        pand    mm6, %[YMask]\n"
"		psadbw  mm6, mm5				## sum of abs differences is motion\n"
"		movd	mm4, ebx				## our motion total\n"
"		paddd   mm4, mm6				## accum \n"
"		movd	ebx, mm4				## update our motion total\n"
#endif
"\n"
#ifdef USE_MEDIAN_FILTER
"\n"
"## apply median filter to prev pixels to (from FsPrev2) qword and save\n"
"## in:	mm7 = new pixels		\n"
"##		mm5 = prev pixels\n"
"##		mm2 = old pixels\n"
"\n"
"		movq	mm5, qword ptr[edi+edx] ## pixels from previous field, same row & col\n"
"		movq	mm6, mm7				## work copy of new pixels\n"
"		pminub	mm6, mm2				## Lowest of new and old\n"
"		pmaxub	mm7, mm2				## Highest of new and old\n"
"		pminub	mm7, mm5				## no higher than highest\n"
"		pmaxub	mm7, mm6                ## no lower than lowest\n"
"\n"
"## decide if we want to use the filtered value, depending upon how much effect it has\n"
"		movq    mm6, mm7\n"
"		psubusb mm6, mm5				## how different is the filtered val\n"
"		movq    mm4, mm5\n"
"		psubusb mm4, mm7				## how different is the filtered val\n"
"		por     mm6, mm4				## the abs diff caused by filter\n"
"\n"
"		psubusb mm6, %[MedianFilterAmt]    ## bigger than max filter?\n"
"		pxor    mm4, mm4\n"
"		pcmpeqb mm6, mm4				## will be FFF.. if we should filter small change\n"
"		pand    mm7, mm6				## so use filtered val\n"
"		pcmpeqb mm6, mm4				## will be FFF.. if we shouldn't filter\n"
"		pand	mm5, mm6				## so use unfiltered val\n"
"		por		mm7, mm5				## combine\n"
"		movq	qword ptr[edi+edx], mm7	## save maybe filtered val for later\n"
"\n"
#endif									// end of median filter code
"\n"
"## bump ptrs and loop for next qword in row\n"
"		lea		edi,[edi+" _strf(FSCOLSIZE) "]\n"
"		lea		esi,[esi+8]			\n"
"		dec		%[LoopCtr]\n"
"\n"
#ifdef USE_SHARPNESS
"		jg		2b # QwordLoop				## if we are not at the end of the row\n"
"		movq    mm2, mm1				## if on last qword use same qword again\n"
"		jz		3b # QwordLoop2				## fall thru only if neg\n"
#else
"		jnl		2b # QwordLoop			\n"
#endif
"\n"
"## Ok, done with one line\n"
"\n"
#ifdef USE_PULLDOWN
"        mov     eax, %[LineCtr]\n"
"        cmp     eax, %[FirstLine]          ## ignore some lines, clear totals here?\n"
"        jnz     2f # NotFirst                ## no\n"
"        pxor    mm3, mm3                ## clear Comb, Kontras\n"
"        xor     ebx, ebx                ## clear motion\n"
"\n"
"2:	# NotFirst:\n"
"        cmp     eax, %[LastLine]           ## ignore some lines, save totals early?\n"
"        jnz     3f # NotLast                 ## no\n"
"		mov		%[Motion], ebx				## Save our Motion total now \n"
"		movd    %[CombSum], mm3			## Save our comb total\n"
"		psrlq	mm3, 32					## shift our Kontrast total\n"
"		movd    %[ContrSum], mm3			## save that too\n"
"\n"
"3:	# NotLast:\n"
#endif
"\n"
"		movq    qword ptr[edi], mm1     ## jaggie reduction needs one to right later\n"
"		mov		eax, %[LoopCtrW]\n"
"		mov     %[LoopCtr], eax            ## reset ctr\n"
"\n"
"		mov		edi, %[pFieldStore]		## addr of our 1st qword in FieldStore\n"
"		lea     edi, [edi+" _strf(FSROWSIZE) "]    ## bump to next row\n"
"		mov		%[pFieldStore], edi		## addr of our 1st qword in FieldStore for line\n"
"\n"
"\n"
"		mov     esi, %[pLinesW]			## ptr to curr line beging\n"
"##>>		lea     esi, [esi+4]			## but we want the next one\n"
"        add     esi, %[InpPitch]           ## but we want the next one\n"
"		mov		%[pLinesW], esi			## update for next loop\n"
"\n"
"		dec		%[LineCtr]\n"
"		jnz		1b # LineLoop				## if not to last line yet\n"
"\n"
"		emms\n"
_restore(edi)
_restore(esi)
_restore(edx)
_restore(ecx)
_restore(eax)
_asm_end,
_m(LineCtr), _m(pLinesW), _m(pFieldStore), _m(FsNewOld), _m(FsPrev2),
_m(FsPrev), _m(YMask), _m(QHB), _m(QHC), _m(QHA), _m(UVMask), _m(MedianFilterAmt),
_m(LoopCtr), _m(FirstLine), _m(LastLine), _m(Motion), _m(CombSum), _m(ContrSum),
_m(LoopCtrW), _m(InpPitch));

#ifdef USE_PULLDOWN
	UpdatePulldown(CombSum / CombScale, ContrSum / CombScale,
		Motion / CombScale);  // go update our pulldown status for new field
#endif

	return TRUE;
}	

/////////////////////////////////////////////////////////////////////////////
// $Id: DI_GreedyHF.asm,v 1.1 2005-01-08 14:54:23 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2001 Tom Barry.  All rights reserved.
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
// 01 Feb 2001   Tom Barry		       New Greedy (High Motion)Deinterlace method
// 29 Jul 2001   Tom Barry             Add 3DNOW, MMX support, create .asm mem 
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.3  2001/11/25 04:33:37  trbarry
// Fix for TDeinterlace_Info. Also release UN-Filter code, 5-tap V & H sharp/soft filters optimized to reverse excessive filtering (or EE?)
//
// Revision 1.2  2001/07/31 13:34:46  trbarry
// Add missing end of line range check
//
// Revision 1.1  2001/07/30 21:50:32  trbarry
// Use weave chroma for reduced chroma jitter. Fix DJR bug again.
// Turn off Greedy Pulldown default.
//
// Revision 1.5  2001/07/30 18:18:59  trbarry
// Fix new DJR bug
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
/////////////////////////////////////////////////////////////////////////////

// FUNCT_NAME must be defined before include
BOOL FUNCT_NAME(TDeinterlaceInfo* pInfo)
{
	#include "DI_GreedyHM2.h"
    int Line;
	int	LoopCtr;
    DWORD Pitch = pInfo->InputPitch;

/*>>>>	short* L1;					// ptr to Line1, of 3
	short* L2;					// ptr to Line2, the weave line
	short* L3;					// ptr to Line3
	short* L2P;					// ptr to prev Line2
>>>>*/
	BYTE* L1;					// ptr to Line1, of 3
	BYTE* L2;					// ptr to Line2, the weave line
	BYTE* L3;					// ptr to Line3

	BYTE* L2P;					// ptr to prev Line2
    BYTE* Dest = pInfo->Overlay;

	__int64 QW256B;
	__int64 LastAvg=0;			//interp value from left qword
	__int64 i;

    i = 0xffffffff - 256;
    QW256B =  i << 48 |  i << 32 | i << 16 | i;  // save a couple instr on PMINSW instruct.

//>>>	if (pOddLines == NULL || pEvenLines == NULL || pPrevLines == NULL)
//>>>		return FALSE;

	// copy first even line no matter what, and the first odd line if we're
	// processing an EVEN field. (note diff from other deint rtns.)
	
/* >>>    pMemcpy(lpCurOverlay, pEvenLines[0], LineLength);	// DL0
	if (!InfoIsOdd)
		pMemcpy(lpCurOverlay + OverlayPitch, pOddLines[0], LineLength);  // DL1
	for (Line = 0; Line < (FieldHeight - 1); ++Line)
	{
		LoopCtr = LineLength / 8 - 1;				// there are LineLength / 8 qwords per line
                                                    // but do 1 less, adj at end of loop
		if (InfoIsOdd)
		{
			L1 = pEvenLines[Line];		
			L2 = pOddLines[Line];	
			L3 = pEvenLines[Line + 1];	
			L2P = pPrevLines[Line];			// prev Odd lines
			Dest = lpCurOverlay + (Line * 2 + 1) * OverlayPitch;	// DL1
		}
		else
		{
			L1 = pOddLines[Line] ;		
			L2 = pEvenLines[Line + 1];		
			L3 = pOddLines[Line + 1];   
			L2P = pPrevLines[Line + 1];			// prev even lines
			Dest = lpCurOverlay + (Line * 2 + 2) * OverlayPitch;	// DL2
		}
		pMemcpy(Dest + OverlayPitch, L3, LineLength);
>>>*/
//>>> new way
//>>>    if(pInfo->PictureHistory[0]->Flags | PICTURE_INTERLACED_ODD)
    if(InfoIsOdd)
    {
        L1 = pInfo->PictureHistory[1]->pData;
        L2 = pInfo->PictureHistory[0]->pData;  
        L3 = L1 + Pitch;   
        L2P = pInfo->PictureHistory[2]->pData;

        // copy first even line
        pInfo->pMemcpy(Dest, L1, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;
    }
    else
    {
        L1 = pInfo->PictureHistory[1]->pData;
        L2 = pInfo->PictureHistory[0]->pData + Pitch;  
        L3 = L1 + Pitch;   
        L2P = pInfo->PictureHistory[2]->pData + Pitch;

        // copy first even line
        pInfo->pMemcpy(Dest, pInfo->PictureHistory[0]->pData, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;
        // then first odd line
        pInfo->pMemcpy(Dest, L1, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;
    }

    for (Line = 0; Line < (pInfo->FieldHeight - 1); ++Line)
    {
_saved_regs;
		LoopCtr = LineLength / 8 - 1;				// there are LineLength / 8 qwords per line
                                                    // but do 1 less, adj at end of loop

//>>> end new way

// For ease of reading, the comments below assume that we're operating on an odd
// field (i.e., that InfoIsOdd is true).  Assume the obvious for even lines..

_asm_begin
_save(eax)
_save(ecx)
_save(edx)
_save(esi)
_save(edi)
"			mov %[LastAvg], 0       ## init easy way\n"
"			mov eax, %[L1]		\n"
"            lea ebx, [eax+8]                ## next qword needed by DJR\n"
"			mov ecx, %[L3]		\n"
"            sub ecx, eax                    ## carry L3 addr as an offset\n"
"            mov edx, %[L2P]		\n"
"            mov esi, %[L2]		\n"
"			mov edi, %[Dest]       ## DL1 if Odd or DL2 if Even 	\n"
"\n"
".align 8\n"
"1:	# DoNext8Bytes:			\n"
"\n"
"			movq	mm0, qword ptr[esi]	    ## L2 - the newest weave pixel value \n"
"			movq	mm1, qword ptr[eax]		## L1 - the top pixel\n"
"			movq	mm2, qword ptr[edx]		## L2P - the prev weave pixel \n"
"			movq	mm3, qword ptr[eax+ecx] ## L3, next odd row\n"
"			movq	mm6, mm1				## L1 - get simple single pixel interp\n"
"##			pavgb   mm6, mm3                ## use macro below\n"
            V_PAVGB (mm6, mm3, mm4, ShiftMask)
"\n"
"## DJR - Diagonal Jaggie Reduction\n"
"## In the event that we are going to use an average (Bob) pixel we do not want a jagged\n"
"## stair step effect.  To combat this we avg in the 2 horizontally adjacen pixels into the\n"
"## interpolated Bob mix. This will do horizontal smoothing for only the Bob'd pixels.\n"
"\n"
"			movq    mm4, %[LastAvg]			## the bob value from prev qword in row\n"
"			movq	%[LastAvg], mm6			## save for next pass\n"
"			psrlq   mm4, 48					## right justify 1 pixel\n"
"			movq	mm7, mm6				## copy of simple bob pixel\n"
"			psllq   mm7, 16                 ## left justify 3 pixels\n"
"			por     mm4, mm7				## and combine\n"
"			\n"
"			movq	mm5, qword ptr[ebx] ## next horiz qword from L1\n"
"##			pavgb   mm5, qword ptr[ebx+ecx] ## next horiz qword from L3, use macro below\n"
            V_PAVGB (mm5, qword ptr[ebx+ecx], mm7, ShiftMask)
"			psllq	mm5, 48					## left just 1 pixel\n"
"			movq	mm7, mm6                ## another copy of simple bob pixel\n"
"			psrlq   mm7, 16					## right just 3 pixels\n"
"			por		mm5, mm7				## combine\n"
"##			pavgb	mm4, mm5				## avg of forward and prev by 1 pixel, use macro\n"
            V_PAVGB (mm4, mm5, mm5, ShiftMask)   // mm5 gets modified if MMX
"##			pavgb	mm6, mm4				## avg of center and surround interp vals, use macro\n"
            V_PAVGB (mm6, mm4, mm7, ShiftMask)
"\n"
"## Don't do any more averaging than needed for mmx. It hurts performance and causes rounding errors.\n"
#ifndef IS_MMX
"##          pavgb	mm4, mm6				## 1/4 center, 3/4 adjacent\n"
            V_PAVGB (mm4, mm6, mm7, ShiftMask)
"##    		pavgb	mm6, mm4				## 3/8 center, 5/8 adjacent\n"
            V_PAVGB (mm6, mm4, mm7, ShiftMask)
#endif
"\n"
"## get abs value of possible L2 comb\n"
"			movq    mm4, mm6				## work copy of interp val\n"
"			movq	mm7, mm2				## L2\n"
"			psubusb mm7, mm4				## L2 - avg\n"
"			movq	mm5, mm4				## avg\n"
"			psubusb mm5, mm2				## avg - L2\n"
"			por		mm5, mm7				## abs(avg-L2)\n"
"\n"
"## get abs value of possible L2P comb\n"
"			movq	mm7, mm0				## L2P\n"
"			psubusb mm7, mm4				## L2P - avg\n"
"			psubusb	mm4, mm0				## avg - L2P\n"
"			por		mm4, mm7				## abs(avg-L2P)\n"
"\n"
"## use L2 or L2P depending upon which makes smaller comb\n"
"			psubusb mm4, mm5				## see if it goes to zero\n"
"			psubusb mm5, mm5				## 0\n"
"			pcmpeqb mm4, mm5				## if (mm4=0) then FF else 0\n"
"			pcmpeqb mm5, mm4				## opposite of mm4\n"
"\n"
"## if Comb(L2P) <= Comb(L2) then mm4=ff, mm5=0 else mm4=0, mm5 = 55\n"
"			pand	mm5, mm2				## use L2 if mm5 == ff, else 0\n"
"			pand	mm4, mm0				## use L2P if mm4 = ff, else 0\n"
"			por		mm4, mm5				## may the best win\n"
"\n"
"## Inventory: at this point we have the following values:\n"
"## mm0 = L2P (or L2)\n"
"## mm1 = L1\n"
"## mm2 = L2 (or L2P)\n"
"## mm3 = L3\n"
"## mm4 = the best of L2,L2P weave pixel, base upon comb \n"
"## mm6 = the avg interpolated value, if we need to use it\n"
"\n"
"## Let's measure movement, as how much the weave pixel has changed\n"
"			movq	mm7, mm2\n"
"			psubusb mm2, mm0\n"
"			psubusb mm0, mm7\n"
"			por		mm0, mm2				## abs value of change, used later\n"
"\n"
"## Now lets clip our chosen value to be not outside of the range\n"
"## of the high/low range L1-L3 by more than MaxComb.\n"
"## This allows some comb but limits the damages and also allows more\n"
"## detail than a boring oversmoothed clip.\n"
"			movq	mm2, mm1				## copy L1\n"
"##			pmaxub	mm2, mm3                ## use macro\n"
			V_PMAXUB (mm2, mm3)             // now = Max(L1,L3)
"			movq	mm5, mm1				## copy L1\n"
"##			pminub	mm5, mm3				## now = Min(L1,L3), use macro\n"
            V_PMINUB (mm5, mm3, mm7)
"## allow the value to be above the high or below the low by amt of MaxComb\n"
"			psubusb mm5, %[MaxCombW]			## lower min by diff\n"
"			paddusb	mm2, %[MaxCombW]			## increase max by diff\n"
"##			pmaxub	mm4, mm5				## now = Max(best,Min(L1,L3) use macro\n"
            V_PMAXUB (mm4, mm5)
"##			pminub	mm4, mm2 				## now = Min( Max(best, Min(L1,L3), L2 )=L2 clipped\n"
            V_PMINUB (mm4, mm2, mm7)
"\n"
"## Blend weave pixel with bob pixel, depending on motion val in mm0			\n"
"			psubusb mm0, %[MotionThresholdW]   ## test Threshold, clear chroma change >>>??\n"
"			pmullw  mm0, %[MotionSenseW]  ## mul by user factor, keep low 16 bits\n"
"			movq    mm7, %[QW256]\n"
#ifdef IS_SSE
"			pminsw  mm0, mm7				## max = 256  \n"
#else
"            paddusw mm0, %[QW256B]              ## add, may sat at fff..\n"
"            psubusw mm0, %[QW256B]              ## now = Min(L1,256)\n"
#endif
"			psubusw mm7, mm0				## so the 2 sum to 256, weighted avg\n"
"            movq    mm2, mm4                ## save weave chroma info before trashing\n"
"            pand	mm4, %[YMask]				## keep only luma from calc'd value\n"
"			pmullw  mm4, mm7				## use more weave for less motion\n"
"            pand	mm6, %[YMask]				## keep only luma from calc'd value\n"
"			pmullw  mm6, mm0				## use more bob for large motion\n"
"			paddusw mm4, mm6				## combine\n"
"			psrlw   mm4, 8					## div by 256 to get weighted avg	\n"
"\n"
"## chroma comes from weave pixel\n"
"            pand    mm2, %[UVMask]             ## keep chroma\n"
"			por		mm2, mm4				## and combine\n"
"\n"
            V_MOVNTQ (qword ptr[edi], mm2)  // move in our clipped best, use macro
"\n"
"## bump ptrs and loop\n"
"			lea		eax,[eax+8]				\n"
"			lea		ebx,[ebx+8]				\n"
"			lea		edx,[edx+8]\n"
"			lea		edi,[edi+8]			\n"
"			lea		esi,[esi+8]\n"
"			dec		%[LoopCtr]\n"
"			jg		1b # DoNext8Bytes            ## loop if not to last line\n"
"                                            ## note P-III default assumes backward branches taken\n"
"            jl      2f # LoopDone                ## done\n"
"            mov     ebx, eax                ## sharpness lookahead 1 byte only, be wrong on 1\n"
"            jmp     1b # DoNext8Bytes\n"
"\n"
"2:	# LoopDone:\n"
_restore(edi)
_restore(esi)
_restore(edx)
_restore(ecx)
_restore(eax)
_asm_end,
_m_nth(LastAvg, 6), _m(L1), _m(L2), _m(L3), _m(L2P),
_m(Dest), _m(ShiftMask), _m(LastAvg), _m(MaxCombW), _m(MotionThresholdW),
_m(MotionSenseW), _m(QW256), _m(QW256B), _m(YMask), _m(UVMask), _m(LoopCtr)) ;

        Dest += pInfo->OverlayPitch;
        pInfo->pMemcpy(Dest, L3, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;

        L1 += Pitch;
        L2 += Pitch;  
        L3 += Pitch;   
        L2P += Pitch;

	}

/* >>>>>>>>>>>>>
	// Copy last odd line if we're processing an Odd field.
	if (InfoIsOdd)
	{
		pMemcpy(lpCurOverlay + (FrameHeight - 1) * OverlayPitch,
				  pOddLines[FieldHeight - 1],
				  LineLength);
	}
*/
    // Copy last odd line if we're processing an Odd field.
//>>>    if(pInfo->PictureHistory[0]->Flags | PICTURE_INTERLACED_ODD)
    if (InfoIsOdd)
    {
        pInfo->pMemcpy(Dest,
                  L2,
                  pInfo->LineLength);
    }

    // clear out the MMX registers ready for doing floating point
    // again
    _asm
    {
        emms
    }
	return TRUE;
}

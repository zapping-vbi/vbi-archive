/////////////////////////////////////////////////////////////////////////////
// DI_GreedyH.asm
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000 Tom Barry  All rights reserved.
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
/*
#if defined(IS_SSE)
#define MAINLOOP_LABEL DoNext8Bytes_SSE
#elif defined(IS_3DNOW)
#define MAINLOOP_LABEL DoNext8Bytes_3DNow
#else
#define MAINLOOP_LABEL DoNext8Bytes_MMX
#endif
*/

// This is a simple lightweight DeInterlace method that uses little CPU time
// but gives very good results for low or intermedite motion.
// It defers frames by one field, but that does not seem to produce noticeable
// lip sync problems.
//
// The method used is to take either the older or newer weave pixel depending
// upon which give the smaller comb factor, and then clip to avoid large damage
// when wrong.
//
// I'd intended this to be part of a larger more elaborate method added to 
// Blended Clip but this give too good results for the CPU to ignore here.

#if defined(IS_SSE)
BOOL DeinterlaceGreedyH_SSE(DEINTERLACE_INFO *info)
#elif defined(IS_3DNOW)
BOOL DeinterlaceGreedyH_3DNOW(DEINTERLACE_INFO *info)
#else
BOOL DeinterlaceGreedyH_MMX(DEINTERLACE_INFO *info)
#endif
{
	int Line;
	int	LoopCtr;
	short* L1;					// ptr to Line1, of 3
	short* L2;					// ptr to Line2, the weave line
	short* L3;					// ptr to Line3
	short* LP2;					// ptr to prev Line2
	BYTE* Dest;
	BYTE *lpCurOverlay = info->Overlay;
	short **pOddLines = info->OddLines[0];
	short **pEvenLines = info->EvenLines[0];
	short **pPrevLines = info->IsOdd ? info->OddLines[1] : info->EvenLines[1];
#ifdef IS_MMX
	const __int64 ShiftMask = 0xfefffefffefffeffLL;	// to avoid shifting chroma to luma
#endif
	__int64 MaxComb;
	__int64 i;

	i = GreedyMaxComb;			// How badly do we let it weave? 0-255
	MaxComb = i << 56 | i << 48 | i << 40 | i << 32 | i << 24 | i << 16 | i << 8 | i;    
	

	if (pOddLines == NULL || pEvenLines == NULL || pPrevLines == NULL)
		return FALSE;

	// copy first even line no matter what, and the first odd line if we're
	// processing an EVEN field. (note diff from other deint rtns.)
	info->pMemcpy(lpCurOverlay, pEvenLines[0], info->LineLength);	// DL0
	if (!info->IsOdd)
		info->pMemcpy(lpCurOverlay + info->OverlayPitch, pOddLines[0], info->LineLength);  // DL1
	for (Line = 0; Line < (info->FieldHeight - 1); ++Line)
	{
_saved_regs;
		LoopCtr = info->LineLength / 8;				// there are LineLength / 8 qwords per line

		if (info->IsOdd)
		{
			L1 = pEvenLines[Line];		
			L2 = pOddLines[Line];	
			L3 = pEvenLines[Line + 1];	
			LP2 = pPrevLines[Line];			// prev Odd lines
			Dest = lpCurOverlay + (Line * 2 + 1) * info->OverlayPitch;	// DL1
		}
		else
		{
			L1 = pOddLines[Line] ;		
			L2 = pEvenLines[Line + 1];		
			L3 = pOddLines[Line + 1];   
			LP2 = pPrevLines[Line + 1];			// prev even lines
			Dest = lpCurOverlay + (Line * 2 + 2) * info->OverlayPitch;	// DL2
		}
		info->pMemcpy(Dest + info->OverlayPitch, L3, info->LineLength);

// For ease of reading, the comments below assume that we're operating on an odd
// field (i.e., that info->IsOdd is true).  Assume the obvious for even lines..

_asm_begin
"			mov eax, %[L1]		\n"
"			mov ebx, %[L2]		\n"
"			mov edx, %[L3]		\n"
"			mov esi, %[LP2]		\n"
"			mov edi, %[Dest]       ## DL1 if Odd or DL2 if Even \n"
"			\n"
".align 8\n"
_strf(MAINLOOP_LABEL) ":			\n"
"			movq	mm1, qword ptr[eax]		## L1\n"
"			movq	mm2, qword ptr[ebx]		## L2\n"
"			movq	mm3, qword ptr[edx]		## L3\n"
"			movq	mm0, qword ptr[esi]     ## LP2\n"
"\n"
"            ## average L1 and L3 leave result in mm4\n"
"            movq	mm4, mm1				## L1\n"
#if defined(IS_SSE)
"		    pavgb mm4, mm3\n"
#elif defined(IS_3DNOW)
"		    pavgusb mm4, mm3\n"
#else
"			pand	mm4, %[ShiftMask]			## "\n"
"			psrlw	mm4, 1\n"
"			movq	mm5, mm3				## L3\n"
"			pand	mm5, %[ShiftMask]			## "\n"
"			psrlw   mm5, 1\n"
"			paddb   mm4, mm5				## the average, for computing comb\n"
#endif
"\n"
"## get abs value of possible L2 comb\n"
"			movq	mm7, mm2				## L2\n"
"			psubusb mm7, mm4				## L2 - avg\n"
"			movq	mm5, mm4				## avg\n"
"			psubusb mm5, mm2				## avg - L2\n"
"			por		mm5, mm7				## abs(avg-L2)\n"
"			movq    mm6, mm4                ## copy of avg for later\n"
"\n"
"## get abs value of possible LP2 comb\n"
"			movq	mm7, mm0				## LP2\n"
"			psubusb mm7, mm4				## LP2 - avg\n"
"			psubusb	mm4, mm0				## avg - LP2\n"
"			por		mm4, mm7				## abs(avg-LP2)\n"
"\n"
"## use L2 or LP2 depending upon which makes smaller comb\n"
"			psubusb mm4, mm5				## see if it goes to zero\n"
"			psubusb mm5, mm5				## 0\n"
"			pcmpeqb mm4, mm5				## if (mm4=0) then FF else 0\n"
"			pcmpeqb mm5, mm4				## opposite of mm4\n"
"\n"
"## if Comb(LP2) <= Comb(L2) then mm4=ff, mm5=0 else mm4=0, mm5 = 55\n"
"			pand	mm5, mm2				## use L2 if mm5 == ff, else 0\n"
"			pand	mm4, mm0				## use LP2 if mm4 = ff, else 0\n"
"			por		mm4, mm5				## may the best win\n"
"\n"
"## Now lets clip our chosen value to be not outside of the range\n"
"## of the high/low range L1-L3 by more than abs(L1-L3)\n"
"## This allows some comb but limits the damages and also allows more\n"
"## detail than a boring oversmoothed clip.\n"
"\n"
"			movq	mm2, mm1				## copy L1\n"
"			psubusb mm2, mm3				## - L3, with saturation\n"
"			paddusb mm2, mm3                ## now = Max(L1,L3)\n"
"\n"
"			pcmpeqb mm7, mm7				## all ffffffff\n"
"			psubusb mm7, mm1				## - L1 \n"
"			paddusb mm3, mm7				## add, may sat at fff..\n"
"			psubusb mm3, mm7				## now = Min(L1,L3)\n"
"\n"
"## allow the value to be above the high or below the low by amt of MaxComb\n"
"			paddusb	mm2, %[MaxComb]			## increase max by diff\n"
"			psubusb mm3, %[MaxComb]			## lower min by diff\n"
"\n"
"			psubusb mm4, mm3				## best - Min\n"
"			paddusb mm4, mm3				## now = Max(best,Min(L1,L3)\n"
"\n"
"			pcmpeqb mm7, mm7				## all ffffffff\n"
"			psubusb mm7, mm4				## - Max(best,Min(best,L3) \n"
"			paddusb mm2, mm7				## add may sat at FFF..\n"
"			psubusb mm2, mm7				## now = Min( Max(best, Min(L1,L3), L2 )=L2 clipped\n"
"\n"
#ifdef IS_SSE
"		    movntq qword ptr[edi], mm2      ## move in our clipped best\n"
#else
"		    movq qword ptr[edi], mm2        ## move in our clipped best\n"
#endif
"\n"
"## bump ptrs and loop\n"
"			lea		eax,[eax+8]				\n"
"			lea		ebx,[ebx+8]\n"
"			lea		edx,[edx+8]\n"
"			lea		edi,[edi+8]			\n"
"			lea		esi,[esi+8]\n"
"			dec		%[LoopCtr]\n"
"			jnz		" _strf(MAINLOOP_LABEL) "\n"
_asm_end,
_m(L1), _m(L2), _m(L3), _m(LP2), _m(Dest), _m(ShiftMask), _m(MaxComb), _m(LoopCtr)
: "eax", "edx", "esi", "edi");

	// Copy last odd line if we're processing an Odd field.
	if (info->IsOdd)
	{
		info->pMemcpy(lpCurOverlay + (info->FrameHeight - 1) * info->OverlayPitch,
				  pOddLines[info->FieldHeight - 1],
				  info->LineLength);
	}

    // clear out the MMX registers ready for doing floating point
    // again
    _asm
    {
        emms
    }
	return TRUE;
}

// #undef MAINLOOP_LABEL

/////////////////////////////////////////////////////////////////////////////
// $Id: DI_GreedyHMPulldown.c,v 1.2 2005-01-20 01:38:33 mschimek Exp $
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
/////////////////////////////////////////////////////////////////////////////
// Change Log
//
// Date          Developer             Changes
//
// 01 Jul 2001   Tom Barry		       New Greedy (High Motion)Deinterlace method
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.1  2005/01/08 14:54:23  mschimek
// *** empty log message ***
//
// Revision 1.5  2001/11/25 04:33:37  trbarry
// Fix for TDeinterlace_Info. Also release UN-Filter code, 5-tap V & H sharp/soft filters optimized to reverse excessive filtering (or EE?)
//
// Revision 1.4  2001/11/13 17:24:49  trbarry
// Misc GreedyHMA Avisynth related changes. Also fix bug in Vertical filter causing right hand garbage on screen.
//
// Revision 1.2  2001/07/25 12:04:31  adcockj
// Moved Control stuff into DS_Control.h
// Added $Id and $Log to comment blocks as per standards
//
/////////////////////////////////////////////////////////////////////////////

#include "windows.h"
//>>>>>>>>#include "DS_Deinterlace.h"
#include "DI_GreedyHM.h"
BOOL PullDown_V(BOOL SelectL2);
BOOL PullDown_VSharp(BOOL SelectL2);
BOOL PullDown_VSharp2(BYTE* dest, __int64* Source1, __int64* Dest, int LinLength);
BOOL PullDown_VSoft2(BYTE* dest, __int64* Source1, __int64* Dest, int LinLength);
BOOL PullDown_InBetween();
BOOL FieldStoreMerge(BYTE * dest, __int64 * src, int clen);
BOOL FieldStoreCopy_V(BYTE * dest, __int64 * src, __int64 * src2, int clen);
BOOL FieldStoreMerge_V(BYTE * dest, __int64 * src, __int64 * src2, int clen);

int HistPtr = 0;					// where we are in Queue
static GR_PULLDOWN_INFO Hist[20] = {0};	// a short history of pulldown stats

int UpdatePulldown(int Comb, int Kontrast, int Motion)
{
	int Prev = (HistPtr+20-1) % 20;	// prev entry ptr
	int Last = (Prev + 20+1-PDAVGLEN) % 20;	// Last elem still in average at prev
	int FlagMask = 0x000ffffe;      // trunc to 20 bits, turn off low          

    // note most values are updated delay 1, except input Comb
	if (Comb < Hist[HistPtr].Comb)
	{
		Hist[HistPtr].CombChoice = Comb;
		Hist[HistPtr].Flags = ((Hist[Prev].Flags << 1) & FlagMask) | 1;
		if (Hist[HistPtr].Comb > 0 && Kontrast > 0)
		{
    		Hist[HistPtr].AvgChoice =  100 - 100 * Hist[HistPtr].CombChoice / Hist[HistPtr].Comb;
        }
    }
	else
	{
		Hist[HistPtr].CombChoice = Hist[HistPtr].Comb;
		Hist[HistPtr].Flags = (Hist[Prev].Flags << 1) & FlagMask;
		if (Comb > 0 && Kontrast > 0)
		{
//		    Hist[HistPtr].AvgChoice = 100 * (100 - 100 * Hist[HistPtr].CombChoice / Comb);
		    Hist[HistPtr].AvgChoice = 100 - 100 * Hist[HistPtr].CombChoice / Comb;
		}
	}
	Hist[HistPtr].Kontrast = Kontrast;	// Kontrast calc'd in arrears
	Hist[HistPtr].Motion = Motion;		// Motion Calc'd in arrears
	Hist[HistPtr].Avg = Hist[Prev].Avg + Hist[HistPtr].AvgChoice - Hist[Last].AvgChoice;	

	HistPtr = (1+HistPtr) % 20;			// bump for next time
	Hist[HistPtr].Comb = Comb;			// only fill in Comb for curr val, rest is garbage
	Hist[HistPtr].Kontrast = 0;			// only fill in Comb for curr val, rest is garbage
	Hist[HistPtr].Motion= 0;			// only fill in Comb for curr val, rest is garbage
    if (!InfoIsOdd)
    {   
        Hist[HistPtr].Flags2 = PD_ODD;
    }
    else
    {
        Hist[HistPtr].Flags2 = 0;
    }
    return 0;
}

int CheckPD()	
{		
	int hPtr = (HistPtr + 20 - FsDelay) % 20;
	if ( Hist[hPtr].Flags2 == 0)
	{
		return FALSE;
	}
	return FALSE;
}

BOOL CanDoPulldown()					// check if we should do pulldown, doit
{
	int line;				// number of lines
	int L1;						// FieldStore elem holding top known pixels-unneeded
	int L3;						// FieldStore elem holding bottom known pxl-unneeded
	int L2;						// FieldStore elem holding newest weave pixels
	int L2P;					// FieldStore elem holding prev weave pixels
	const int PdMask1 = 0xf;    // 0b01111
	const int PdMask2 = PdMask1 << 5 | PdMask1;     // 0b01111,01111
	const int PdMask32 = PdMask1 << 20 | PdMask2 << 10 | PdMask2;    // 0b0111101111011110111101111
    const int Pd20Bits = 0x000fffff;     // 20 1 bits 
    const int PdMerge1 = 5;		// 0b00101
	const int PdMerge2 = PdMerge1 << 5 | PdMerge1;     // 0b00101,00101
    const int Pd32Pattern = PdMerge1 << 20 | PdMerge2 << 10 | PdMerge2; // 0b00101.. 5 times
    const int Pd22Pattern = 0x00055555; // 2:2 pattern = 0b01010101..
    const int Pd22PatternB = 0x000aaaaa; // 2:2 pattern = 0b10101010..
    
	const int PdMask1b = 0x1b;    // 0b11011
	const int PdMask2b = PdMask1b << 5 | PdMask1b;     // 0b11011, 0b11011
	const int PdMerge1b = 9;		// 0b01001
	const int PdMerge2b = PdMerge1b << 5 | PdMerge1b;     // 0b01x01,01x01
	BYTE* WeaveDest;			// dest for weave pixel
	BYTE* CopyDest;				// other dest, copy or vertical filter
	int CopySrc;                // where to copy from
	int hPtr = (HistPtr + 20 - FsDelay) % 20;     // curr HistPtr adj for delay
	int hPtrb = (HistPtr + 20 - 1) % 20;          // curr HistPtr delay 1
    int FlagsW = Hist[hPtrb].Flags;    

    if (! GreedyUsePulldown || Hist[hPtr].AvgChoice == 0 || Hist[hPtr].Avg == 0)
	{
        Hist[hPtr].Flags2 |= PD_VIDEO;	// flag did video, not pulldown
		return FALSE;
	}

	// If Greedy Matching doesn't help enough or this field comb raises avg too much
	// then no good.
    if (Hist[hPtr].Motion < GreedyLowMotionPdLvl*10)
//    if (100 * Hist[hPtr].Motion < GreedyLowMotionPdLvl * Hist[hPtr].Kontrast )
    {
         Hist[hPtr].Flags2 |= PD_LOW_MOTION;	// flag did pulldown due to low motion
    }

    else if ( ( ((FlagsW ^ Pd32Pattern) & PdMask32 & Pd20Bits) == 0)
            | ( ((FlagsW ^ (Pd32Pattern >> 1) ) & (PdMask32 >> 1) & Pd20Bits) == 0)
            | ( ((FlagsW ^ (Pd32Pattern >> 2) ) & (PdMask32 >> 2) & Pd20Bits) == 0)
            | ( ((FlagsW ^ (Pd32Pattern >> 3) ) & (PdMask32 >> 3) & Pd20Bits) == 0)
            | ( ((FlagsW ^ (Pd32Pattern >> 4) ) & (PdMask32 >> 4) & Pd20Bits) == 0) )
    {
        Hist[hPtr].Flags2 |= PD_32_PULLDOWN;	// flag found 20 bit 3:2 pulldown pattern
    }

/* >>> temp remove until it works better
    else if ( ( ((FlagsW ^ Pd22Pattern) & Pd20Bits) == 0 )
            | ( ((FlagsW ^ Pd22PatternB) & Pd20Bits) == 0 ) )
    {
        Hist[hPtr].Flags2 |= PD_22_PULLDOWN;	// flag found 20 bit 2:2 pulldown pattern
    }
*/

    else if (1000 * Hist[hPtr].Avg  < GreedyGoodPullDownLvl * PDAVGLEN * Hist[hPtr].Comb)
	{
        Hist[hPtr].Flags2 |= PD_VIDEO;	// flag did video, not pulldown
	    return FALSE;
	}

//	if (Hist[hPtr].CombChoice * PDAVGLEN * 100 > Hist[hPtr].AvgChoice * GreedyBadPullDownLvl )
	if (Hist[hPtr].CombChoice * 100 > Hist[hPtr].Kontrast *  GreedyBadPullDownLvl )
	{
		Hist[hPtr].Flags2 |= PD_BAD | PD_VIDEO;	// flag bad pulldown, did video
		return FALSE;
	}

	// We can do some sort of pulldown
	Hist[hPtr].Flags2 |= PD_PULLDOWN;

    // trb 11/15/01 add new Inverse Filtering
    if (GreedyUseVSharpness && GreedyVSharpnessAmt)
	{
	    // Heck with it. Do old cheaper way for -100, gimmick
	    if (GreedyVSharpnessAmt == -100)
	    {
		    return PullDown_V(Hist[hPtr].Flags & 1);
	    }
        else
        {
            return PullDown_VSharp(Hist[hPtr].Flags & 1);
        }
	}

	if (GreedyUseInBetween) 
    {
        if (FsDelay == 2)   // for FsDelay == 2 do inbetween for (delay 1) flags 01x0101x01 pattern only
        {
            if ( (Hist[hPtrb].Flags & PdMask2b) == PdMerge2b )
            {
	            Hist[hPtr].Flags2 |= PD_MERGED;
	            return PullDown_InBetween();
            }
        }
        
        else    // for FsDelay == 1 do inbetween for flags x0101x0101 pattern only
        {
            if ( (Hist[hPtr].Flags & PdMask2) == PdMerge2 )  
            {
	            Hist[hPtr].Flags2 |= PD_MERGED;
	            return PullDown_InBetween();
            }
        }
    }


	// OK, do simple pulldown
	
	// set up pointers, offsets
	SetFsPtrs(&L1, &L2, &L2P, &L3, &CopySrc, &CopyDest, &WeaveDest);
	// chk forward/backward Greedy Choice Flag for this field
	if (!(Hist[hPtr].Flags & 1))   // temp
	{
   		L2 = L2P;
	}
 //   L2=L1; //>>> for test only
//    CopySrc=L2; //>>>
//      L2=CopySrc; //>>>
    for (line = 0; line < (FieldHeight); ++line)
	{
		FieldStoreCopy(CopyDest, &FieldStore[CopySrc], LineLength);
		CopyDest += 2 * OverlayPitch;
		CopySrc += FSMAXCOLS;

		FieldStoreCopy(WeaveDest, &FieldStore[L2], LineLength);
		WeaveDest += 2 * OverlayPitch;
		L2 += FSMAXCOLS;
	}
	return TRUE;
}

// Return sorted last N Hist Records - note current entry never complete yet
BOOL PullDown_InBetween()					
{
	int	line;
	int EvenL = __min(FsPtrP, FsPtrP3);	    // where to get even lines
	int OddL = __min(FsPtr, FsPtrP2);		//  " odd lines
	BYTE* Dest = lpCurOverlay;
	if (InfoIsOdd)
	{
	    EvenL = __min(FsPtrP, FsPtrP3);	// first copy ptr
	    OddL = __min(FsPtr, FsPtrP2);		// first weave ptr
	}
	else
	{
	    EvenL = __min(FsPtr, FsPtrP2);	// first copy ptr
	    OddL = __min(FsPtrP, FsPtrP3);		// first weave ptr
	}
    if (GreedyUseVSharpness)
    {
        for (line = 0; line < (FieldHeight - 1); ++line)
	    {

		    FieldStoreMerge_V(Dest, &FieldStore[EvenL], &FieldStore[OddL], LineLength);
		    Dest += OverlayPitch;
		    EvenL += FSMAXCOLS;

		    FieldStoreMerge_V(Dest, &FieldStore[OddL], &FieldStore[EvenL], LineLength);
		    Dest += OverlayPitch;
		    OddL += FSMAXCOLS;
	    }
        // one more time but dup last line
        FieldStoreMerge_V(Dest, &FieldStore[EvenL], &FieldStore[OddL], LineLength);
		Dest += OverlayPitch;
        FieldStoreMerge_V(Dest, &FieldStore[EvenL], &FieldStore[OddL], LineLength);

    }
    else    // not vertical filter
    {
       	for (line = 0; line < FieldHeight; ++line)
	    {

		    FieldStoreMerge(Dest, &FieldStore[EvenL], LineLength);
		    Dest += OverlayPitch;
		    EvenL += FSMAXCOLS;

		    FieldStoreMerge(Dest, &FieldStore[OddL], LineLength);
		    Dest += OverlayPitch;
		    OddL += FSMAXCOLS;
	    }
    }
return TRUE;
}

// copy 1 line from Fieldstore to overlay buffer, mult of 32 bytes, merging with succeeding line.
BOOL FieldStoreCopy_V(BYTE * dest, __int64 * src, __int64 * src2, int clen)
{
_saved_regs;
	int ct = clen / 32;
_asm_begin
"		mov		esi, %[src]                    ## a source line\n"
"		mov		ebx, %[src2]                   ## the other source line to merge\n"
"        sub     ebx, esi                    ## carry as offset\n"
"		mov		edi, %[dest]					## new output line dest\n"
"		mov		ecx, %[ct]\n"
"\n"
"1: # cloop:	\n"
"		movq	mm0, qword ptr[esi]\n"
"		movq	mm1, qword ptr[esi+" _strf(FSCOLSIZE) "]\n"
"		movq	mm2, qword ptr[esi+" _strf(FSCOLSIZE) "*2]\n"
"		movq	mm3, qword ptr[esi+" _strf(FSCOLSIZE) "*3]\n"
"\n"
"		pavgb	mm0, qword ptr[esi+ebx + 16]		## next frame 2 qwords removed\n"
"		pavgb	mm1, qword ptr[esi+ebx + " _strf(FSCOLSIZE) " + 16]\n"
"		pavgb	mm2, qword ptr[esi+ebx + " _strf(FSCOLSIZE) "*2 + 16]\n"
"		pavgb	mm3, qword ptr[esi+ebx + " _strf(FSCOLSIZE) "*3 + 16]\n"
"		\n"
"		movntq	qword ptr[edi], mm0\n"
"		movntq	qword ptr[edi+8], mm1\n"
"		movntq	qword ptr[edi+16], mm2\n"
"		movntq	qword ptr[edi+24], mm3\n"
"		lea		esi, [esi+" _strf(FSCOLSIZE) "*4]\n"
"		lea		edi, [edi+32]\n"
"		loop	1b # cloop						## go do next 4 qwords\n"
"		sfence\n"
"		emms\n"
_asm_end,
_m(src), _m(src2), _m(dest), _m(ct)
: "ecx", "esi", "edi");
	return TRUE;
}


// copy 1 line from Fieldstore to overlay buffer, mult of 32 bytes, merging with succeeding line and 
// succeeding frame.
BOOL FieldStoreMerge_V(BYTE * dest, __int64 * src, __int64 * src2, int clen)
{
_saved_regs;
	int ct = clen / 32;
_asm_begin
"		mov		esi, %[src]                    ## a source line\n"
"		mov		ebx, %[src2]                   ## the other source line to merge\n"
"        sub     ebx, esi                    ## carry as offset\n"
"		mov		edi, %[dest]					## new output line dest\n"
"		mov		ecx, %[ct]\n"
"\n"
"1: # cloop:	\n"
"		movq	mm0, qword ptr[esi]             ## 4 qwords from current row and field\n"
"		movq	mm1, qword ptr[esi + " _strf(FSCOLSIZE) "]\n"
"		movq	mm2, qword ptr[esi + " _strf(FSCOLSIZE) " * 2]\n"
"		movq	mm3, qword ptr[esi + " _strf(FSCOLSIZE) " * 3]\n"
"\n"
"		movq	mm4, qword ptr[esi + ebx]		## 4 more of same from following line\n"
"		movq	mm5, qword ptr[esi + ebx + " _strf(FSCOLSIZE) "]\n"
"		movq	mm6, qword ptr[esi + ebx + " _strf(FSCOLSIZE) " * 2]\n"
"		movq	mm7, qword ptr[esi + ebx + " _strf(FSCOLSIZE) " * 3]\n"
"\n"
"        pavgb	mm0, qword ptr[esi + 16]		## current row, next frame 2 qwords removed\n"
"		pavgb	mm1, qword ptr[esi + " _strf(FSCOLSIZE) " + 16]\n"
"		pavgb	mm2, qword ptr[esi + " _strf(FSCOLSIZE) " * 2 + 16]\n"
"		pavgb	mm3, qword ptr[esi + " _strf(FSCOLSIZE) " * 3 + 16]\n"
"		\n"
"        pavgb	mm4, qword ptr[esi + ebx + 16]		## next row, next frame 2 qwords removed\n"
"		pavgb	mm5, qword ptr[esi + ebx + " _strf(FSCOLSIZE) " + 16]\n"
"		pavgb	mm6, qword ptr[esi + ebx + " _strf(FSCOLSIZE) " * 2 + 16]\n"
"		pavgb	mm7, qword ptr[esi + ebx + " _strf(FSCOLSIZE) " * 3 + 16]\n"
"\n"
"        pavgb	mm0, mm4\n"
"		pavgb	mm1, mm5\n"
"		pavgb	mm2, mm6\n"
"		pavgb	mm3, mm7\n"
"		\n"
"		movntq	qword ptr[edi], mm0\n"
"		movntq	qword ptr[edi + 8], mm1\n"
"		movntq	qword ptr[edi + 16], mm2\n"
"		movntq	qword ptr[edi + 24], mm3\n"
"\n"
"		lea		esi, [esi + " _strf(FSCOLSIZE) " * 4]\n"
"		lea		edi, [edi + 32]\n"
"		loop	1b # cloop						## go do next 4 qwords\n"
"		sfence\n"
"		emms\n"
_asm_end,
_m(src), _m(src2), _m(dest), _m(ct)
: "ecx", "esi", "edi");
	return TRUE;
}


// copy 1 line from Fieldstore to overlay buffer, mult of 32 bytes, merging 2 frames
// Only the lower of the 2 src addr is passed, other assumed
BOOL FieldStoreMerge(BYTE * dest, __int64 * src, int clen)
{
_saved_regs;
	int ct = clen / 32;
_asm_begin
"		mov		esi, %[src]\n"
"		mov		edi, %[dest]					## new output line dest\n"
"		mov		ecx, %[ct]\n"
"\n"
"1: # cloop:	\n"
"		movq	mm0, qword ptr[esi]\n"
"		movq	mm1, qword ptr[esi+" _strf(FSCOLSIZE) "]\n"
"		movq	mm2, qword ptr[esi+" _strf(FSCOLSIZE) "*2]\n"
"		movq	mm3, qword ptr[esi+" _strf(FSCOLSIZE) "*3]\n"
"\n"
"		pavgb	mm0, qword ptr[esi + 16]		## next frame 2 qwords removed\n"
"		pavgb	mm1, qword ptr[esi+" _strf(FSCOLSIZE) " + 16]\n"
"		pavgb	mm2, qword ptr[esi+" _strf(FSCOLSIZE) "*2 + 16]\n"
"		pavgb	mm3, qword ptr[esi+" _strf(FSCOLSIZE) "*3 + 16]\n"
"		\n"
"		movntq	qword ptr[edi], mm0\n"
"		movntq	qword ptr[edi+8], mm1\n"
"		movntq	qword ptr[edi+16], mm2\n"
"		movntq	qword ptr[edi+24], mm3\n"
"		lea		esi, [esi+" _strf(FSCOLSIZE) "*4]\n"
"		lea		edi, [edi+32]\n"
"		loop	1b # cloop						## go do next 4 qwords\n"
"		sfence\n"
"		emms\n"
_asm_end,
_m(src), _m(dest), _m(ct)
: "ecx", "esi", "edi");
	return TRUE;
}

// Return sorted last N Hist Records - note current entry never complete yet
BOOL GetHistData(GR_PULLDOWN_INFO * OHist, int ct)					
{
	int i;
	int j= (HistPtr+20-ct) % 20;
	for (i = 0; i < ct; ++i)
	{
		*OHist = Hist[j];
		OHist++;
		j = (1+j) % 20;
	}
	return TRUE;
}


// Pulldown with vertical filter
BOOL PullDown_V(BOOL SelectL2)
{
	int line;				// number of lines
	int	LoopCtr;				// number of qwords in line - 1
	int	LoopCtrW;				// number of qwords in line - 1

	int L1;						// offset to FieldStore elem holding top known pixels
	int L3;						// offset to FieldStore elem holding bottom known pxl
	int L2;						// offset to FieldStore elem holding newest weave pixels
	int L2P;					// offset to FieldStore elem holding prev weave pixels
	__int64* pFieldStore;		// ptr into FieldStore qwords
	__int64* pFieldStoreEnd;	// ptr to Last FieldStore qword
	__int64* pL2;				// ptr into FieldStore[L2] 
	BYTE* WeaveDest;					// dest for weave pixel
	BYTE* CopyDest;				// other dest, copy or vertical filter
	int CopySrc;

	int DestIncr = 2 * OverlayPitch;  // we go throug overlay buffer 2 lines per

	// set up pointers, offsets
	SetFsPtrs(&L1, &L2, &L2P, &L3, &CopySrc, &CopyDest, &WeaveDest);
	if (!SelectL2)
	{
		L2 = L2P;
	}
	L1 = (L1 - L2) * 8;					// now is signed offset from L2  
	L3 = (L3 - L2) * 8;					// now is signed offset from L2  
	pFieldStore = & FieldStore[0];		// starting ptr into FieldStore[L2]
	pFieldStoreEnd = & FieldStore[FieldHeight * FSCOLCT];		// ending ptr into FieldStore[L2]
	pL2 = & FieldStore[L2];				// starting ptr into FieldStore[L2]
	LoopCtrW = LineLength / 8;		    // do 8 bytes at a time, adjusted

	for (line = 0; line < (FieldHeight); ++line)
	{
		LoopCtr = LoopCtrW;				// actually qword counter
		if (WeaveDest == lpCurOverlay)    // on first line may just copy first and last
		{
			FieldStoreCopy(lpCurOverlay, &FieldStore[CopySrc], LineLength);
			WeaveDest += DestIncr;		// bump for next, CopyDest already OK
			pL2 = & FieldStore[L2 + FSCOLCT];
		}
		else
		{			
_saved_regs;

_asm_begin
"		mov		edi, %[WeaveDest]				## get ptr to line ptrs	\n"
"		mov		esi, %[pL2]		## addr of our 1st qword in FieldStore\n"
"		mov		eax, %[L1]						## offset to top 	\n"
"		mov		ebx, %[L3]						## offset to top comb 	\n"
"		lea     edx, [esi+eax]				## where L1 would point\n"
"		cmp		edx, %[pFieldStore]			## before begin of fieldstore?\n"
"		jnb		L1OK						## n, ok\n"
"		mov		eax, ebx					## else use this for top pixel vals\n"
"L1OK:\n"
"		lea     edx, [esi+ebx]				## where L2 would point\n"
"		cmp		edx, %[pFieldStoreEnd]			## after end of fieldstore?\n"
"		jb		L3OK						## n, ok\n"
"		mov		ebx, eax					## else use this bottom pixel vals\n"
"\n"
"L3OK:		\n"
"		mov		edx, %[CopyDest]\n"
"\n"
"		.align 8\n"
"QwordLoop:\n"
"\n"
#define FSOFFS 0 * FSCOLSIZE				// following include needs an offset
"\n"
"## The Value FSOFFS must be defined before including this header.\n"
"##\n"
"## On exit mm2 will contain the value of the calculated weave pixel, not yet stored.\n"
"## It is also expected that mm1 and mm3 will still contain the vertically adjacent pixels\n"
"## which may be needed for the vertical filter.\n"
"		movq	mm1, qword ptr[esi+eax+" _strf(FSOFFS) "]	## L1\n"
"		movq	mm2, qword ptr[esi+" _strf(FSOFFS) "]		## L2 \n"
"		movq	mm3, qword ptr[esi+ebx+" _strf(FSOFFS) "] ## L3\n"
"\n"
"		pavgb   mm1, mm2\n"
"		movntq	qword ptr[edx], mm1		## top & middle pixels avg\n"
"		pavgb   mm3, mm2\n"
"		movntq	qword ptr[edi], mm3		## middle & bottom\n"
"\n"
"		## bump ptrs and loop for next qword\n"
"		lea		edx,[edx+8]				## bump CopyDest\n"
"		lea		edi,[edi+8]				## bump WeaveDest\n"
"		lea		esi,[esi+" _strf(FSCOLSIZE) "]			\n"
"		dec		%[LoopCtr]\n"
"		jg		QwordLoop			\n"
"\n"
"## Ok, done with one line\n"
"		mov		esi, %[pL2]				## addr of our 1st qword in FieldStore\n"
"		lea     esi, [esi+" _strf(FSROWSIZE) "]    ## bump to next row\n"
"		mov		%[pL2], esi				## addr of our 1st qword in FieldStore for line\n"
"		mov     edi, %[WeaveDest]			## ptr to curr overlay buff line start\n"
"		add     edi, %[DestIncr]			## but we want to skip 1\n"
"		mov		%[WeaveDest], edi			## update for next loop\n"
"		mov     edx, %[CopyDest]			## ptr to curr overlay buff line start\n"
"		add     edx, %[DestIncr]			## but we want to skip 1\n"
"		mov		%[CopyDest], edx			## update for next loop\n"
"		sfence\n"
"		emms\n"
_asm_end,
_m(WeaveDest), _m(pL2), _m(L1), _m(L3), _m(pFieldStore), _m(pFieldStoreEnd),
_m(CopyDest), _m(LoopCtr), _m(DestIncr)
: "eax", "ecx", "edx", "esi", "edi");
		}		// should undent here but I can't read it
	}

  return TRUE;
}	
		
// Add new Vertical Edge Enhancement optimized to reverse previous vertical filtering
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

14) A = 1 / [w (1 - 2 Q^2)]		# 1 / [.5 (1 - 2 .25)] = 1 / (.5 .5) = 4

15) B = 2 Q / [w (1 - 2 Q^2)]   # 2 .5 / [.5 (1 - 2 .25)] = 1 / .25 = 4

16( C = 2 Q^2 / [1 - 2 Q^2]		# 2 .25 / [1 - 2 .25] = .5 / .5 = 1

		# from 13)  3 =  4 2.5 - 4 2 + 1 1 = 10 - 8 + 1 = 3

Note we still don't know the orig values of Xi but we are out of patience, CPU,
and math ability so we just assume Xi=Zi for the remaining terms. Hopefully 
it will be close.

*/
    __int64 QA;
    __int64 QB;
    __int64 QC; 
    
BOOL PullDown_VSharp(BOOL SelectL2)
{
    int w = (GreedyVSharpnessAmt > 0)                 // note-adj down for overflow
            ? 1000 - (GreedyVSharpnessAmt * 38 / 10)  // overflow, use 38%, 0<w<1000 
                                                      
            : 1000 - (GreedyVSharpnessAmt * 150 / 10); // bias towards workable range 
    int Q = 500 * (1000 - w) / w;                      // Q as 0 - 1K, max 16k        
    int Q2 = (Q*Q) / 1000;                             // Q^2 as 0 - 1k
    int denom = (w * (1000 - 2 * Q2)) / 1000;          // [w (1-2q^2)] as 0 - 1k    
    int A = 64000 / denom;                             // A as 0 - 64
    int B = 128 * Q / denom;                           // B as 0 - 64
    int C =  ((w * Q2) / (denom*500)) ;                // C as 0 - 64
    __int64 i;
    
//
	int line;				// number of lines

	int L1;						// offset to FieldStore elem holding top known pixels
	int L3;						// offset to FieldStore elem holding bottom known pxl
	int L2;						// offset to FieldStore elem holding newest weave pixels
	int L2P;					// offset to FieldStore elem holding prev weave pixels
	BYTE* WeaveDest;					// dest for weave pixel
	BYTE* CopyDest;				// other dest, copy or vertical filter
	int CopySrc;
    int Src1 = 0;
    int Src2 = 0;

	int DestIncr = 2 * OverlayPitch;  // we go throug overlay buffer 2 lines per
 //   C =  C = 64 - A + B + 0 *(C =  ((w * Q2) / denom) >> 8);                          // works better, unbiased avg

    
    if ((A-B+C-64 > 1) || (A-B+C-64 < -1))
    {
        C = 64 - A + B;
    }
    C = 64 - A + B;
    // for too large plus minus Sharpness
    if (A < 32 || A > 64 || B > 0 || B < -64 || C < 0 || C > 64 || w < 0 )
    {
        A = A;
    }
    if (C !=0)
    {
        C=C;
    }
    // for too large plus Sharpness
    if (A > 127 || B > 127 || C > 127 || A < 64 || B < 0 || C < 0)
    {
        C = C;  // for setting stops
    }
    

    // set up pointers, offsets
	SetFsPtrs(&L1, &L2, &L2P, &L3, &CopySrc, &CopyDest, &WeaveDest);
	// chk forward/backward Greedy Choice Flag for this field
	if (!SelectL2) 
	{
   		L2 = L2P;
	}

    // Pick up first 2 and last 2 lines
	FieldStoreCopy(CopyDest, &FieldStore[CopySrc], LineLength);
    FieldStoreCopy(WeaveDest, &FieldStore[L2], LineLength);
	FieldStoreCopy(CopyDest+2*(FieldHeight-1)*OverlayPitch, 
        &FieldStore[CopySrc+FSMAXCOLS*(FieldHeight-1)], LineLength);
    FieldStoreCopy(WeaveDest+2*(FieldHeight-1)*OverlayPitch,
        &FieldStore[L2+FSMAXCOLS*(FieldHeight-1)], LineLength);
	CopyDest += 2 * OverlayPitch;
	WeaveDest += 2 * OverlayPitch;


    if (CopyDest < WeaveDest)
    {
        Src2 = CopySrc + FSMAXCOLS;
        Src1 = L2; // + FSMAXCOLS;
    }
    else
    {
        Src2 = L2 + FSMAXCOLS;
        Src1 = CopySrc; // + FSMAXCOLS;
        CopyDest = WeaveDest;
    }

    i = A;                          
    QA = i << 48 | i << 32 | i << 16 | i;
    i = C;
    QC = i << 48 | i << 32 | i << 16 | i;
    if (B < 0)
    {
        i = -B;
        QB = i << 48 | i << 32 | i << 16 | i;

        for (line = 1; line < FieldHeight-1; ++line)
	    {
            PullDown_VSoft2(CopyDest, &FieldStore[Src1],
                &FieldStore[Src2], LineLength);
		    Src1 += FSMAXCOLS;
		    CopyDest += OverlayPitch;
    
            PullDown_VSoft2(CopyDest, &FieldStore[Src2],
                &FieldStore[Src1], LineLength);
		    Src2 += FSMAXCOLS;
		    CopyDest += OverlayPitch;
	    }
    }
    else
    {    
        i = B;
        QB = i << 48 | i << 32 | i << 16 | i;

        for (line = 1; line < FieldHeight-1; ++line)
	    {
            PullDown_VSharp2(CopyDest, &FieldStore[Src1],
                &FieldStore[Src2], LineLength);
		    Src1 += FSMAXCOLS;
		    CopyDest += OverlayPitch;
    
            PullDown_VSharp2(CopyDest, &FieldStore[Src2],
                &FieldStore[Src1], LineLength);
		    Src2 += FSMAXCOLS;
		    CopyDest += OverlayPitch;
	    }
    }
    return TRUE;

  return TRUE;
}

// see comments in PullDown_VSharp() above	
BOOL PullDown_VSharp2(BYTE* Dest, __int64* Source1, __int64* Source2, int LinLength)
{
_saved_regs;
    const __int64 YMask		= 0x00ff00ff00ff00ffLL;	// to keep only luma
    const __int64 UVMask    = 0xff00ff00ff00ff00LL;	// to keep only chroma

	int ct = LinLength / 8;
_asm_begin
_save(ecx)
_save(edx)
_save(esi)
_save(edi)
"		mov		esi, %[Source2]                ## the source line\n"
"        mov     eax, %[Source1]                ## the preceeding source line\n"
"        sub     eax, esi\n"
"		mov		edi, %[Dest]					## new output line dest\n"
"		mov		ecx, %[ct]\n"
"        movq    mm7, %[YMask]                  ## useful constant\n"
"        cmp     %[Int_QC],0              ## was 3rd parm 0?\n"
"        je      5f # cloopEasy                   ## yes, do faster way\n"
"\n"
"1: # cloop:	\n"
"        movq    mm2, qword ptr[esi-" _strf(FSROWSIZE) "]   ## Zi\n"
"        movq    mm1, qword ptr[esi+eax]         ## Zj\n"
"		movq	mm0, qword ptr[esi]             ## Zk \n"
"        pavgb   mm1, qword ptr[esi+eax+" _strf(FSROWSIZE) "]   ## avg(Zj,Zl)\n"
"        pavgb   mm2, qword ptr[esi+" _strf(FSROWSIZE) "]   ## avg(Zi,Zm)\n"
"\n"
"        movq    mm3, mm0                    ## save copy of center line, Zk with chroma\n"
"##        pand    mm3, %[UVMask]                 ## keep only chroma \n"
"\n"
"        pand    mm0, mm7                  ## only luma\n"
"		pmullw  mm0, %[QA]                     ## mult by weighting factor, A * Zk  \n"
"\n"
"        pand    mm1, mm7\n"
"		pmullw  mm1, %[QB]                     ## mult by weighting factor, B * avg(Zj, Zl)\n"
"        \n"
"        pand    mm2, mm7\n"
"		pmullw  mm2, %[QC]                     ## mult by weighting factor\n"
"\n"
"        psubusw mm1, mm2                    ## add in weighted average of Zj,Zl \n"
"        psubusw mm0, mm1                    ## sub weighted average of Zj,Zl \n"
"        psrlw   mm0, 6					    ## should be our luma answers\n"
"        pminsw  mm0, mm7                  ## avoid overflow\n"
"        pand    mm3, %[UVMask]                 ## get chroma from here\n"
"        por     mm0, mm3                    ## put chroma back, add 1*luma\n"
"\n"
"        movntq	qword ptr[edi], mm0\n"
"		lea		esi, [esi+" _strf(FSCOLSIZE) "]\n"
"		lea		edi, [edi+8]\n"
"		loop	1b # cloop						## go do next 4 qwords\n"
"		sfence\n"
"        jmp     6f # done\n"
"\n"
"5: # cloopEasy:                                  ## easy way only 2 vals	\n"
"        movq    mm1, qword ptr[esi+eax]         ## Zj\n"
"		movq	mm0, qword ptr[esi]             ## Zk \n"
"        pavgb   mm1, qword ptr[esi+eax+" _strf(FSROWSIZE) "]   ## avg(Zj,Zl)\n"
"\n"
"        movq    mm3, mm0                    ## save copy of center line, Zk with chroma\n"
"\n"
"        pand    mm0, mm7                  ## only luma\n"
"		pmullw  mm0, %[QA]                     ## mult by weighting factor, A * Zk  \n"
"\n"
"        pand    mm1, mm7\n"
"		pmullw  mm1, %[QB]                     ## mult by weighting factor, B * avg(Zj, Zl)\n"
"     \n"
"        psubusw mm0, mm1                    ## sub weighted average of Zj,Zl \n"
"        psrlw   mm0, 6					    ## should be our luma answers\n"
"        pminsw  mm0, mm7                  ## avoid overflow\n"
"        pand    mm3, %[UVMask]                 ## get chroma from here\n"
"        por     mm0, mm3                    ## put chroma back, add 1*luma\n"
"\n"
"        movntq	qword ptr[edi], mm0\n"
"		lea		esi, [esi+" _strf(FSCOLSIZE) "]\n"
"		lea		edi, [edi+8]\n"
"		loop	5b # cloopEasy					## go do next 4 qwords\n"
"\n"
"6: # done:\n"
"        sfence\n"
"\n"
"        emms\n"
_restore(edi)
_restore(esi)
_restore(ecx)
_restore(eax)
_asm_end,
_m(Source2), _m(Source1), _m(Dest), _m(ct), _m(YMask), _m(UVMask),
_m(QA), _m(QB), _m(QC), _m_int(QC));
	return TRUE;
}

// see comments in PullDown_VSharp() above	
BOOL PullDown_VSoft2(BYTE* Dest, __int64* Source1, __int64* Source2, int LinLength)
{
_saved_regs;
    const __int64 YMask		= 0x00ff00ff00ff00ffLL;	// to keep only luma
    const __int64 UVMask    = 0xff00ff00ff00ff00LL;	// to keep only chroma

	int ct = LinLength / 8;
_asm_begin
_save(eax)
_save(ecx)
_save(edx)
_save(esi)
_save(edi)
"		mov		esi, %[Source2]                ## the source line\n"
"        mov     eax, %[Source1]                ## the preceeding source line\n"
"        sub     eax, esi\n"
"		mov		edi, %[Dest]					## new output line dest\n"
"		mov		ecx, %[ct]\n"
"        movq    mm7, %[YMask]                  ## useful constant\n"
"        cmp     %[Int_QC],0              ## was 3rd parm 0?\n"
"        je      5f # cloopEasy                   ## yes, do faster way\n"
"\n"
"1: # cloop:	\n"
"        movq    mm2, qword ptr[esi-" _strf(FSROWSIZE) "]   ## Zi\n"
"        movq    mm1, qword ptr[esi+eax]         ## Zj\n"
"		movq	mm0, qword ptr[esi]             ## Zk \n"
"        pavgb   mm1, qword ptr[esi+eax+" _strf(FSROWSIZE) "]   ## avg(Zj,Zl)\n"
"        pavgb   mm2, qword ptr[esi+" _strf(FSROWSIZE) "]   ## avg(Zi,Zm)\n"
"\n"
"        movq    mm3, mm0                    ## save copy of center line, Zk with chroma\n"
"##        pand    mm3, %[UVMask]                 ## keep only chroma \n"
"\n"
"        pand    mm0, mm7                  ## only luma\n"
"		pmullw  mm0, %[QA]                     ## mult by weighting factor, A * Zk  \n"
"\n"
"        pand    mm1, mm7\n"
"		pmullw  mm1, %[QB]                     ## mult by weighting factor, B * avg(Zj, Zl)\n"
"        \n"
"        pand    mm2, mm7\n"
"		pmullw  mm2, %[QC]                     ## mult by weighting factor\n"
"\n"
"        paddusw mm0, mm2                    ## add in weighted average of Zj,Zl \n"
"        paddusw mm0, mm1                    ## sub weighted average of Zj,Zl \n"
"        psrlw   mm0, 6					    ## should be our luma answers\n"
"        pminsw  mm0, mm7                  ## avoid overflow\n"
"        pand    mm3, %[UVMask]                 ## get chroma from here\n"
"        por     mm0, mm3                    ## put chroma back, add 1*luma\n"
"\n"
"        movntq	qword ptr[edi], mm0\n"
"		lea		esi, [esi+" _strf(FSCOLSIZE) "]\n"
"		lea		edi, [edi+8]\n"
"		loop	1b # cloop						## go do next 4 qwords\n"
"		sfence\n"
"        jmp     6f # done\n"
"\n"
"5: # cloopEasy:                                  ## easy way only 2 vals	\n"
"        movq    mm1, qword ptr[esi+eax]         ## Zj\n"
"		movq	mm0, qword ptr[esi]             ## Zk \n"
"        pavgb   mm1, qword ptr[esi+eax+" _strf(FSROWSIZE) "]   ## avg(Zj,Zl)\n"
"\n"
"        movq    mm3, mm0                    ## save copy of center line, Zk with chroma\n"
"\n"
"        pand    mm0, mm7                  ## only luma\n"
"		pmullw  mm0, %[QA]                     ## mult by weighting factor, A * Zk  \n"
"\n"
"        pand    mm1, mm7\n"
"		pmullw  mm1, %[QB]                     ## mult by weighting factor, B * avg(Zj, Zl)\n"
"     \n"
"        paddusw mm0, mm1                    ## sub weighted average of Zj,Zl \n"
"        psrlw   mm0, 6					    ## should be our luma answers\n"
"        pminsw  mm0, mm7                  ## avoid overflow\n"
"        pand    mm3, %[UVMask]                 ## get chroma from here\n"
"        por     mm0, mm3                    ## put chroma back, add 1*luma\n"
"\n"
"        movntq	qword ptr[edi], mm0\n"
"		lea		esi, [esi+" _strf(FSCOLSIZE) "]\n"
"		lea		edi, [edi+8]\n"
"		loop	5b # cloopEasy					## go do next 4 qwords\n"
"\n"
"6: # done:\n"
"        sfence\n"
"\n"
"        emms\n"
_restore(edi)
_restore(esi)
_restore(edx)
_restore(ecx)
_restore(eax)
_asm_end,
_m(Source2), _m(Source1), _m(Dest), _m(ct), _m(YMask), _m(UVMask),
_m(QA), _m(QB), _m(QC), _m_int(QC));

	return TRUE;
}

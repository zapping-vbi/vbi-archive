/////////////////////////////////////////////////////////////////////////////
// $Id: DI_Greedy2Frame.asm,v 1.1 2005-01-08 14:54:23 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000 John Adcock, Tom Barry, Steve Grimm  All rights reserved.
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
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.8  2001/11/23 17:18:54  adcockj
// Fixed silly and/or confusion
//
// Revision 1.7  2001/11/22 22:27:00  adcockj
// Bug Fixes
//
// Revision 1.6  2001/11/21 15:21:40  adcockj
// Renamed DEINTERLACE_INFO to TDeinterlaceInfo in line with standards
// Changed TDeinterlaceInfo structure to have history of pictures.
//
// Revision 1.5  2001/07/31 06:48:33  adcockj
// Fixed index bug spotted by Peter Gubanov
//
// Revision 1.4  2001/07/13 16:13:33  adcockj
// Added CVS tags and removed tabs
//
/////////////////////////////////////////////////////////////////////////////

// This is the implementation of the Greedy 2-frame deinterlace algorithm described in
// DI_Greedy2Frame.c.  It's in a separate file so we can compile variants for different
// CPU types; most of the code is the same in the different variants.

#if defined(IS_SSE)
#define MAINLOOP_LABEL DoNext8Bytes_SSE
#elif defined(IS_3DNOW)
#define MAINLOOP_LABEL DoNext8Bytes_3DNow
#else
#define MAINLOOP_LABEL DoNext8Bytes_MMX
#endif

///////////////////////////////////////////////////////////////////////////////
// Field 1 | Field 2 | Field 3 | Field 4 |
//   T0    |         |    T1   |         | 
//         |   M0    |         |    M1   | 
//   B0    |         |    B1   |         | 
//


// debugging feature
// output the value of mm4 at this point which is pink where we will weave
// and green were we are going to bob
// uncomment next line to see this
//#define CHECK_BOBWEAVE

#if defined(IS_SSE)
BOOL DeinterlaceGreedy2Frame_SSE(TDeinterlaceInfo* pInfo)
#elif defined(IS_3DNOW)
BOOL DeinterlaceGreedy2Frame_3DNOW(TDeinterlaceInfo* pInfo)
#else
BOOL DeinterlaceGreedy2Frame_MMX(TDeinterlaceInfo* pInfo)
#endif
{
    int Line;
    BYTE* M1;
    BYTE* M0;
    BYTE* T0;
    BYTE* T1;
    BYTE* B1;
    BYTE* B0;
	BYTE* B0UseInAsm;
    DWORD OldSI;
    DWORD OldSP;
    BYTE* Dest = pInfo->Overlay;
    BYTE* Dest2;
    DWORD Pitch = pInfo->InputPitch;
    DWORD LineLength = pInfo->LineLength;

    const __int64 YMask    = 0x00ff00ff00ff00ffLL;

    __int64 qwGreedyTwoFrameThreshold;
    const __int64 Mask = 0x7f7f7f7f7f7f7f7fLL;
    const __int64 DwordOne = 0x0000000100000001LL;    
    const __int64 DwordTwo = 0x0000000200000002LL;    

    qwGreedyTwoFrameThreshold = GreedyTwoFrameThreshold;
    qwGreedyTwoFrameThreshold += (GreedyTwoFrameThreshold2 << 8);
    qwGreedyTwoFrameThreshold += (qwGreedyTwoFrameThreshold << 48) +
                                (qwGreedyTwoFrameThreshold << 32) + 
                                (qwGreedyTwoFrameThreshold << 16);


    if(pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD)
    {
        M1 = pInfo->PictureHistory[0]->pData;
        T1 = pInfo->PictureHistory[1]->pData;
        B1 = T1 + Pitch;
        M0 = pInfo->PictureHistory[2]->pData;
        T0 = pInfo->PictureHistory[3]->pData;
        B0 = T0 + Pitch;
    }
    else
    {
        M1 = pInfo->PictureHistory[0]->pData + Pitch;
        T1 = pInfo->PictureHistory[1]->pData;
        B1 = T1 + Pitch;
        M0 = pInfo->PictureHistory[2]->pData + Pitch;
        T0 = pInfo->PictureHistory[3]->pData;
        B0 = T0 + Pitch;

        pInfo->pMemcpy(Dest, pInfo->PictureHistory[0]->pData, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;
    }

    for (Line = 0; Line < pInfo->FieldHeight - 1; ++Line)
    {
_saved_regs;
        // Always use the most recent data verbatim.  By definition it's correct (it'd
        // be shown on an interlaced display) and our job is to fill in the spaces
        // between the new lines.
        pInfo->pMemcpy(Dest, T1, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;
        Dest2 = Dest;

		B0UseInAsm = B0;
_asm_begin
"            ## We'll be using a couple registers that have meaning in the C code, so\n"
"            ## save them.\n"
"            mov %[OldSI], esi\n"
"            mov %[OldSP], esp\n"
"\n"
"            ## Figure out what to do with the scanline above the one we just copied.\n"
"            ## See above for a description of the algorithm.\n"
"\n"
"            mov ecx, %[LineLength]\n"
"            mov eax, %[T1]     \n"
"            mov ebx, %[M1]     \n"
"            mov edx, %[B1]     \n"
"            mov esi, %[M0]     \n"
"            mov esp, %[T0]\n"
"            shr ecx, 3                      ## there are LineLength / 8 qwords\n"
"            movq    mm6, %[Mask]\n"
"\n"
".align 8\n"
_strf(MAINLOOP_LABEL) ":\n"
"\n"
"            mov edi, %[B0UseInAsm]\n"
"            movq    mm1, qword ptr[eax]     ## T1\n"
"            movq    mm0, qword ptr[ebx]     ## M1\n"
"            movq    mm3, qword ptr[edx]     ## B1\n"
"            movq    mm2, qword ptr[esi]     ## M0\n"
"\n"
"            ## Average T1 and B1 so we can do interpolated bobbing if we bob onto T1.\n"
"            movq mm7, mm3                   ## mm7 = B1\n"
"\n"
#if defined(IS_SSE)
"            pavgb mm7, mm1\n"
#elif defined(IS_3DNOW)
"            pavgusb mm7, mm1\n"
#else
"            movq mm5, mm1                   ## mm5 = T1\n"
"            psrlw mm7, 1                    ## mm7 = B1 / 2\n"
"            pand mm7, mm6                   ## mask off lower bits\n"
"            psrlw mm5, 1                    ## mm5 = T1 / 2\n"
"            pand mm5, mm6                   ## mask off lower bits\n"
"            paddw mm7, mm5                  ## mm7 = (T1 + B1) / 2\n"
#endif
"\n"
"            ## calculate |M1-M0| put result in mm4 need to keep mm0 intact\n"
"            ## if we have a good processor then make mm0 the average of M1 and M0\n"
"            ## which should make weave look better when there is small amounts of\n"
"            ## movement\n"
#if defined(IS_SSE)
"            movq    mm4, mm0\n"
"            movq    mm5, mm2\n"
"            psubusb mm4, mm2\n"
"            psubusb mm5, mm0\n"
"            por     mm4, mm5\n"
"            psrlw   mm4, 1\n"
"            pavgb mm0, mm2\n"
"            pand    mm4, mm6\n"
#elif defined(IS_3DNOW)
"            movq    mm4, mm0\n"
"            movq    mm5, mm2\n"
"            psubusb mm4, mm2\n"
"            psubusb mm5, mm0\n"
"            por     mm4, mm5\n"
"            psrlw   mm4, 1\n"
"            pavgusb mm0, mm2\n"
"            pand    mm4, mm6\n"
#else
"            movq    mm4, mm0\n"
"            psubusb mm4, mm2\n"
"            psubusb mm2, mm0\n"
"            por     mm4, mm2\n"
"            psrlw   mm4, 1\n"
"            pand    mm4, mm6\n"
#endif
"\n"
"\n"
"            ## if |M1-M0| > Threshold we want dword worth of twos\n"
"            pcmpgtb mm4, %[qwGreedyTwoFrameThreshold]\n"
"            pand    mm4, %[Mask]               ## get rid of any sign bit\n"
"            pcmpgtd mm4, %[DwordOne]           ## do we want to bob\n"
"            pandn   mm4, %[DwordTwo]\n"
"\n"
"            movq    mm2, qword ptr[esp]     ## mm2 = T0\n"
"\n"
"            ## calculate |T1-T0| put result in mm5\n"
"            movq    mm5, mm2\n"
"            psubusb mm5, mm1\n"
"            psubusb mm1, mm2\n"
"            por     mm5, mm1\n"
"            psrlw   mm5, 1\n"
"            pand    mm5, mm6\n"
"\n"
"            ## if |T1-T0| > Threshold we want dword worth of ones\n"
"            pcmpgtb mm5, %[qwGreedyTwoFrameThreshold]\n"
"            pand    mm5, mm6                ## get rid of any sign bit\n"
"            pcmpgtd mm5, %[DwordOne]           \n"
"            pandn   mm5, %[DwordOne]\n"
"            paddd mm4, mm5\n"
"\n"
"            movq    mm2, qword ptr[edi]     ## B0\n"
"\n"
"            ## calculate |B1-B0| put result in mm5\n"
"            movq    mm5, mm2\n"
"            psubusb mm5, mm3\n"
"            psubusb mm3, mm2\n"
"            por     mm5, mm3\n"
"            psrlw   mm5, 1\n"
"            pand    mm5, mm6\n"
"\n"
"            ## if |B1-B0| > Threshold we want dword worth of ones\n"
"            pcmpgtb mm5, %[qwGreedyTwoFrameThreshold]\n"
"            pand    mm5, mm6                ## get rid of any sign bit\n"
"            pcmpgtd mm5, %[DwordOne]\n"
"            pandn   mm5, %[DwordOne]\n"
"            paddd mm4, mm5\n"
"\n"
"            ## Get the dest pointer.\n"
"            add edi, 8\n"
"            mov %[B0UseInAsm], edi\n"
"            mov edi, %[Dest2]\n"
"\n"
"            pcmpgtd mm4, %[DwordTwo]\n"
"\n"
"## debugging feature\n"
"## output the value of mm4 at this point which is pink where we will weave\n"
"## and green were we are going to bob\n"
#ifdef CHECK_BOBWEAVE
#ifdef IS_SSE
"            movntq qword ptr[edi], mm4\n"
#else
"            movq qword ptr[edi], mm4\n"
#endif
#else
"\n"
"            movq mm5, mm4\n"
"             ## mm4 now is 1 where we want to weave and 0 where we want to bob\n"
"            pand    mm4, mm0                \n"
"            pandn   mm5, mm7                \n"
"            por     mm4, mm5                \n"
#ifdef IS_SSE
"            movntq qword ptr[edi], mm4\n"
#else
"            movq qword ptr[edi], mm4\n"
#endif
#endif
"            ## Advance to the next set of pixels.\n"
"            add edi, 8\n"
"            add eax, 8\n"
"            add ebx, 8\n"
"            add edx, 8\n"
"            mov %[Dest2], edi\n"
"            add esi, 8\n"
"            add esp, 8\n"
"            dec ecx\n"
"            jne " _strf(MAINLOOP_LABEL) "\n"
"\n"
"            mov esi, %[OldSI]\n"
"            mov esp, %[OldSP]\n"
_asm_end,
_m(OldSI), _m(OldSP), _m(LineLength), _m(T1), _m(M1), _m(B1),
_m(M0), _m(T0), _m(Mask), _m(B0UseInAsm), _m(qwGreedyTwoFrameThreshold),
_m(DwordOne), _m(DwordTwo), _m(Dest2)
: "eax", "ecx", "edx", "esi", "edi");

        Dest += pInfo->OverlayPitch;

        M1 += Pitch;
        T1 += Pitch;
        B1 += Pitch;
        M0 += Pitch;
        T0 += Pitch;
        B0 += Pitch;
    }

    if(pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD)
    {
        pInfo->pMemcpy(Dest, T1, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;
        pInfo->pMemcpy(Dest, M1, pInfo->LineLength);
    }
    else
    {
        pInfo->pMemcpy(Dest, T1, pInfo->LineLength); 
    }
    
    // clear out the MMX registers ready for doing floating point
    // again
    _asm
    {
        emms
    }

    return TRUE;
}


#undef MAINLOOP_LABEL

/////////////////////////////////////////////////////////////////////////////
// $Id: DI_VideoBob.asm,v 1.1 2005-01-08 14:54:23 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000 John Adcock.  All rights reserved.
// Based on code from Virtual Dub Plug-in by Gunnar Thalin
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
// Revision 1.4  2001/11/23 17:18:54  adcockj
// Fixed silly and/or confusion
//
// Revision 1.3  2001/11/21 15:21:41  adcockj
// Renamed DEINTERLACE_INFO to TDeinterlaceInfo in line with standards
// Changed TDeinterlaceInfo structure to have history of pictures.
//
// Revision 1.2  2001/07/13 16:13:33  adcockj
// Added CVS tags and removed tabs
//
/////////////////////////////////////////////////////////////////////////////

#if defined(IS_SSE)
#define MAINLOOP_LABEL DoNext8Bytes_SSE
#elif defined(IS_3DNOW)
#define MAINLOOP_LABEL DoNext8Bytes_3DNow
#else
#define MAINLOOP_LABEL DoNext8Bytes_MMX
#endif

///////////////////////////////////////////////////////////////////////////////
// DeinterlaceFieldBob
//
// Deinterlaces a field with a tendency to bob rather than weave.  Best for
// high-motion scenes like sports.
//
// The algorithm for this was taken from the 
// Deinterlace - area based Vitual Dub Plug-in by
// Gunnar Thalin
///////////////////////////////////////////////////////////////////////////////
#if defined(IS_SSE)
BOOL DeinterlaceFieldBob_SSE(TDeinterlaceInfo* pInfo)
#elif defined(IS_3DNOW)
BOOL DeinterlaceFieldBob_3DNOW(TDeinterlaceInfo* pInfo)
#else
BOOL DeinterlaceFieldBob_MMX(TDeinterlaceInfo* pInfo)
#endif
{
    int Line;
    BYTE* YVal1;
    BYTE* YVal2;
    BYTE* YVal3;
    BYTE* Dest = pInfo->Overlay;
    DWORD LineLength = pInfo->LineLength;
    DWORD Pitch = pInfo->InputPitch;
    
    __int64 qwEdgeDetect;
    __int64 qwThreshold;
#ifdef IS_MMX
    const __int64 Mask = 0xfefefefefefefefeLL;
#endif
    const __int64 YMask    = 0x00ff00ff00ff00ffLL;

    qwEdgeDetect = EdgeDetect;
    qwEdgeDetect += (qwEdgeDetect << 48) + (qwEdgeDetect << 32) + (qwEdgeDetect << 16);
    qwThreshold = JaggieThreshold;
    qwThreshold += (qwThreshold << 48) + (qwThreshold << 32) + (qwThreshold << 16);


    if(pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD)
    {
        YVal1 = pInfo->PictureHistory[0]->pData;
        YVal2 = pInfo->PictureHistory[1]->pData + Pitch;
        YVal3 = YVal1 + Pitch;

        pInfo->pMemcpy(Dest, pInfo->PictureHistory[1]->pData, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;
        
        pInfo->pMemcpy(Dest, YVal1, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;
    }
    else
    {
        YVal1 = pInfo->PictureHistory[0]->pData;
        YVal2 = pInfo->PictureHistory[1]->pData;
        YVal3 = YVal1 + Pitch;

        pInfo->pMemcpy(Dest, YVal1, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;
    }

    for (Line = 0; Line < pInfo->FieldHeight - 1; ++Line)
    {
_saved_regs;
        // For ease of reading, the comments below assume that we're operating on an odd
        // field (i.e., that bIsOdd is true).  The exact same processing is done when we
        // operate on an even field, but the roles of the odd and even fields are reversed.
        // It's just too cumbersome to explain the algorithm in terms of "the next odd
        // line if we're doing an odd field, or the next even line if we're doing an
        // even field" etc.  So wherever you see "odd" or "even" below, keep in mind that
        // half the time this function is called, those words' meanings will invert.

_asm_begin
"            mov ecx, %[LineLength]\n"
"            mov eax, %[YVal1]\n"
"            mov ebx, %[YVal2]\n"
"            mov edx, %[YVal3]\n"
"            mov edi, %[Dest]\n"
"            shr ecx, 3       ## there are LineLength / 8 qwords\n"
"\n"
".align 8\n"
_strf(MAINLOOP_LABEL) ":         \n"
"            movq mm0, qword ptr[eax] \n"
"            movq mm1, qword ptr[ebx] \n"
"            movq mm2, qword ptr[edx]\n"
"\n"
"            ## get intensities in mm3 - 4\n"
"            movq mm3, mm0\n"
"            movq mm4, mm1\n"
"            movq mm5, mm2\n"
"\n"
"            pand mm3, %[YMask]\n"
"            pand mm4, %[YMask]\n"
"            pand mm5, %[YMask]\n"
"\n"
"            ## get average in mm0\n"
#if defined(IS_SSE)
"            pavgb mm0, mm2\n"
#elif defined(IS_3DNOW)
"            pavgusb mm0, mm2\n"
#else
"            pand  mm0, %[Mask]\n"
"            pand  mm2, %[Mask]\n"
"            psrlw mm0, 01\n"
"            psrlw mm2, 01\n"
"            paddw mm0, mm2\n"
#endif
"\n"
"            ## work out (O1 - E) * (O2 - E) / 2 - EdgeDetect * (O1 - O2) ^ 2 >> 12\n"
"            ## result will be in mm6\n"
"\n"
"            psrlw mm3, 01\n"
"            psrlw mm4, 01\n"
"            psrlw mm5, 01\n"
"\n"
"            movq mm6, mm3\n"
"            psubw mm6, mm4  ##mm6 = O1 - E\n"
"\n"
"            movq mm7, mm5\n"
"            psubw mm7, mm4  ##mm7 = O2 - E\n"
"\n"
"            pmullw mm6, mm7     ## mm0 = (O1 - E) * (O2 - E)\n"
"\n"
"            movq mm7, mm3\n"
"            psubw mm7, mm5      ## mm7 = (O1 - O2)\n"
"            pmullw mm7, mm7     ## mm7 = (O1 - O2) ^ 2\n"
"            psrlw mm7, 12       ## mm7 = (O1 - O2) ^ 2 >> 12\n"
"            pmullw mm7, %[qwEdgeDetect]        ## mm7  = EdgeDetect * (O1 - O2) ^ 2 >> 12\n"
"\n"
"            psubw mm6, mm7      ## mm6 is what we want\n"
"\n"
"            pcmpgtw mm6, %[qwThreshold]\n"
"\n"
"            movq mm7, mm6\n"
"\n"
"            pand mm0, mm6\n"
"\n"
"            pandn mm7, mm1\n"
"\n"
"            por mm7, mm0\n"
"\n"
#ifdef IS_SSE
"            movntq qword ptr[edi], mm7\n"
#else
"            movq qword ptr[edi], mm7\n"
#endif
"\n"
"            add eax, 8\n"
"            add ebx, 8\n"
"            add edx, 8\n"
"            add edi, 8\n"
"            dec ecx\n"
"            jne " _strf(MAINLOOP_LABEL) "\n"
_asm_end,
_m(LineLength), _m(YVal1), _m(YVal2), _m(YVal3), _m(Dest), _m(YMask),
#ifdef IS_MMX
	_m(Mask),
#endif
_m(qwEdgeDetect), _m(qwThreshold)
: "eax", "ecx", "edx", "edi");

        Dest += pInfo->OverlayPitch;

        // Always use the most recent data verbatim.
        pInfo->pMemcpy(Dest, YVal3, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;

        YVal1 += Pitch;
        YVal2 += Pitch;
        YVal3 += Pitch;
    }

    // Copy last odd line if we're processing an even field.
    if(pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_EVEN)
    {
        pInfo->pMemcpy(Dest, YVal2, pInfo->LineLength);
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


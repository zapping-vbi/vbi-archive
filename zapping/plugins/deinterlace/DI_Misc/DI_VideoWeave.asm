/////////////////////////////////////////////////////////////////////////////
// $Id: DI_VideoWeave.asm,v 1.1 2005-01-08 14:54:23 mschimek Exp $
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
// Deinterlace the latest field, with a tendency to weave rather than bob.
// Good for high detail on low-movement scenes.
//
// The algorithm is described in comments below.
//
#if defined(IS_SSE)
BOOL DeinterlaceFieldWeave_SSE(TDeinterlaceInfo* pInfo)
#elif defined(IS_3DNOW)
BOOL DeinterlaceFieldWeave_3DNOW(TDeinterlaceInfo* pInfo)
#else
BOOL DeinterlaceFieldWeave_MMX(TDeinterlaceInfo* pInfo)
#endif
{
    int Line;
    BYTE* YVal1;
    BYTE* YVal2;
    BYTE* YVal3;
    BYTE* YVal4;
    BYTE* OldStack;
    BYTE* Dest = pInfo->Overlay;
    DWORD LineLength = pInfo->LineLength;
    DWORD Pitch = pInfo->InputPitch;

    const __int64 YMask    = 0x00ff00ff00ff00ffLL;

    __int64 qwSpatialTolerance;
    __int64 qwTemporalTolerance;
    __int64 qwThreshold;
#ifdef IS_MMX
    const __int64 Mask = 0xfefefefefefefefeLL;
#endif

    // Since the code uses MMX to process 4 pixels at a time, we need our constants
    // to be represented 4 times per quadword.
    qwSpatialTolerance = SpatialTolerance;
    qwSpatialTolerance += (qwSpatialTolerance << 48) + (qwSpatialTolerance << 32) + (qwSpatialTolerance << 16);
    qwTemporalTolerance = TemporalTolerance;
    qwTemporalTolerance += (qwTemporalTolerance << 48) + (qwTemporalTolerance << 32) + (qwTemporalTolerance << 16);
    qwThreshold = SimilarityThreshold;
    qwThreshold += (qwThreshold << 48) + (qwThreshold << 32) + (qwThreshold << 16);


    if(pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD)
    {
        YVal1 = pInfo->PictureHistory[1]->pData;
        YVal2 = pInfo->PictureHistory[0]->pData;
        YVal3 = YVal1 + Pitch;
        YVal4 = pInfo->PictureHistory[2]->pData + Pitch;

        pInfo->pMemcpy(Dest, YVal1, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;
    }
    else
    {
        YVal1 = pInfo->PictureHistory[1]->pData;
        YVal2 = pInfo->PictureHistory[0]->pData + Pitch;
        YVal3 = YVal1 + Pitch;
        YVal4 = pInfo->PictureHistory[2]->pData + Pitch;

        pInfo->pMemcpy(Dest, pInfo->PictureHistory[0]->pData, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;

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
"            mov %[OldStack], esi\n"
"\n"
"            mov ecx, %[LineLength]\n"
"            mov eax, %[YVal1]\n"
"            mov ebx, %[YVal2]\n"
"            mov edx, %[YVal3]\n"
"            mov esi, %[YVal4]\n"
"            mov edi, %[Dest]\n"
"            shr ecx, 3       ## there are LineLength / 8 qwords\n"
"\n"
".align 8\n"
_strf(MAINLOOP_LABEL) ":         \n"
"            movq mm0, qword ptr[eax]        ## mm0 = E1\n"
"            movq mm1, qword ptr[ebx]        ## mm1 = O\n"
"            movq mm2, qword ptr[edx]        ## mm2 = E2\n"
"\n"
"            movq mm3, mm0                   ## mm3 = intensity(E1)\n"
"            movq mm4, mm1                   ## mm4 = intensity(O)\n"
"            movq mm6, mm2                   ## mm6 = intensity(E2)\n"
"            pand mm3, %[YMask]\n"
"            pand mm4, %[YMask]\n"
"            pand mm6, %[YMask]\n"
"\n"
"\n"
"            ## Average E1 and E2 for interpolated bobbing.\n"
"            ## leave result in mm0\n"
#if defined(IS_SSE)
"            pavgb mm0, mm2\n"
#elif defined(IS_3DNOW)
"            pavgusb mm0, mm2\n"
#else
"            pand mm0, %[Mask]                  ## mm0 = E1 with lower chroma bit stripped off\n"
"            pand mm2, %[Mask]                  ## mm2 = E2 with lower chroma bit stripped off\n"
"            psrlw mm0, 1                    ## mm0 = E1 / 2\n"
"            psrlw mm2, 1                    ## mm2 = E2 / 2\n"
"            paddb mm0, mm2                  ## mm2 = (E1 + E2) / 2\n"
#endif
"\n"
"            ## The meat of the work is done here.  We want to see whether this pixel is\n"
"            ## close in luminosity to ANY of: its top neighbor, its bottom neighbor,\n"
"            ## or its predecessor.  To do this without branching, we use MMX's\n"
"            ## saturation feature, which gives us Z(x) = x if x>=0, or 0 if x<0.\n"
"            ##\n"
"            ## The formula we're computing here is\n"
"            ##      Z(ST - (E1 - O) ^ 2) + Z(ST - (E2 - O) ^ 2) + Z(TT - (Oold - O) ^ 2)\n"
"            ## where ST is spatial tolerance and TT is temporal tolerance.  The idea\n"
"            ## is that if a pixel is similar to none of its neighbors, the resulting\n"
"            ## value will be pretty low, probably zero.  A high value therefore indicates\n"
"            ## that the pixel had a similar neighbor.  The pixel in the same position\n"
"            ## in the field before last (Oold) is considered a neighbor since we want\n"
"            ## to be able to display 1-pixel-high horizontal lines.\n"
"\n"
"            movq mm7, %[qwSpatialTolerance]\n"
"            movq mm5, mm3                   ## mm5 = E1\n"
"            psubsw mm5, mm4                 ## mm5 = E1 - O\n"
"            psraw mm5, 1\n"
"            pmullw mm5, mm5                 ## mm5 = (E1 - O) ^ 2\n"
"            psubusw mm7, mm5                ## mm7 = ST - (E1 - O) ^ 2, or 0 if that's negative\n"
"\n"
"            movq mm3, %[qwSpatialTolerance]\n"
"            movq mm5, mm6                   ## mm5 = E2\n"
"            psubsw mm5, mm4                 ## mm5 = E2 - O\n"
"            psraw mm5, 1\n"
"            pmullw mm5, mm5                 ## mm5 = (E2 - O) ^ 2\n"
"            psubusw mm3, mm5                ## mm0 = ST - (E2 - O) ^ 2, or 0 if that's negative\n"
"            paddusw mm7, mm3                ## mm7 = (ST - (E1 - O) ^ 2) + (ST - (E2 - O) ^ 2)\n"
"\n"
"            movq mm3, %[qwTemporalTolerance]\n"
"            movq mm5, qword ptr[esi]        ## mm5 = Oold\n"
"            pand mm5, %[YMask]\n"
"            psubsw mm5, mm4                 ## mm5 = Oold - O\n"
"            psraw mm5, 1 ## XXX\n"
"            pmullw mm5, mm5                 ## mm5 = (Oold - O) ^ 2\n"
"            psubusw mm3, mm5                ## mm0 = TT - (Oold - O) ^ 2, or 0 if that's negative\n"
"            paddusw mm7, mm3                ## mm7 = our magic number\n"
"\n"
"            ## Now compare the similarity totals against our threshold.  The pcmpgtw\n"
"            ## instruction will populate the target register with a bunch of mask bits,\n"
"            ## filling words where the comparison is true with 1s and ones where it's\n"
"            ## false with 0s.  A few ANDs and NOTs and an OR later, we have bobbed\n"
"            ## values for pixels under the similarity threshold and weaved ones for\n"
"            ## pixels over the threshold.\n"
"\n"
"            pcmpgtw mm7, %[qwThreshold]        ## mm7 = 0xffff where we're greater than the threshold, 0 elsewhere\n"
"            movq mm6, mm7                   ## mm6 = 0xffff where we're greater than the threshold, 0 elsewhere\n"
"            pand mm7, mm1                   ## mm7 = weaved data where we're greater than the threshold, 0 elsewhere\n"
"            pandn mm6, mm0                  ## mm6 = bobbed data where we're not greater than the threshold, 0 elsewhere\n"
"            por mm7, mm6                    ## mm7 = bobbed and weaved data\n"
"\n"
#ifdef IS_SSE
"            movntq qword ptr[edi], mm7\n"
#else
"            movq qword ptr[edi], mm7\n"
#endif
"            add eax, 8\n"
"            add ebx, 8\n"
"            add edx, 8\n"
"            add edi, 8\n"
"            add esi, 8\n"
"            dec ecx\n"
"            jne " _strf(MAINLOOP_LABEL) "\n"
"\n"
"            mov esi, %[OldStack]\n"
_asm_end,
_m(OldStack), _m(LineLength), _m(YVal1), _m(YVal2), _m(YVal3), _m(YVal4),
_m(Dest), _m(YMask),
#ifdef IS_MMX
	_m(Mask),
#endif
_m(qwSpatialTolerance), _m(qwTemporalTolerance), _m(qwThreshold)
: "eax", "ecx", "edx", "esi", "edi");

        Dest += pInfo->OverlayPitch;
        // Copy the even scanline below this one to the overlay buffer, since we'll be
        // adapting the current scanline to the even lines surrounding it.  The scanline
        // above has already been copied by the previous pass through the loop.
        pInfo->pMemcpy(Dest, YVal3, LineLength);
        Dest += pInfo->OverlayPitch;

        YVal1 += Pitch;
        YVal2 += Pitch;
        YVal3 += Pitch;
        YVal4 += Pitch;
    }

    // Copy last odd line if we're processing an odd field.
    if(pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD)
    {
        pInfo->pMemcpy(Dest, YVal2, LineLength);
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

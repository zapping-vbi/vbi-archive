/////////////////////////////////////////////////////////////////////////////
// $Id: DI_TwoFrame.asm,v 1.1 2005-01-08 14:54:23 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000 Steven Grimm.  All rights reserved.
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
// Revision 1.6  2001/11/23 17:18:54  adcockj
// Fixed silly and/or confusion
//
// Revision 1.5  2001/11/22 22:29:25  adcockj
// Bug Fix
//
// Revision 1.4  2001/11/22 22:27:00  adcockj
// Bug Fixes
//
// Revision 1.3  2001/11/21 15:21:41  adcockj
// Renamed DEINTERLACE_INFO to TDeinterlaceInfo in line with standards
// Changed TDeinterlaceInfo structure to have history of pictures.
//
// Revision 1.2  2001/07/13 16:13:33  adcockj
// Added CVS tags and removed tabs
//
/////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Deinterlace the latest field, attempting to weave wherever it won't cause
// visible artifacts.
//
// The data from the most recently captured field is always copied to the overlay
// verbatim.  For the data from the previous field, the following algorithm is
// applied to each pixel.
//
// We use the following notation for the top, middle, and bottom pixels
// of concern:
//
// Field 1 | Field 2 | Field 3 | Field 4 |
//         |   T0    |         |   T1    | scanline we copied in last iteration
//   M0    |         |    M1   |         | intermediate scanline from alternate field
//         |   B0    |         |   B1    | scanline we just copied
//
// We will weave M1 into the image if any of the following is true:
//   - M1 is similar to either B1 or T1.  This indicates that no weave
//     artifacts would be visible.  The SpatialTolerance setting controls
//     how far apart the luminances can be before pixels are considered
//     non-similar.
//   - T1 and B1 and M1 are old.  In that case any weave artifact that
//     appears isn't due to fast motion, since it was there in the previous
//     frame too.  By "old" I mean similar to their counterparts in the
//     previous frame; TemporalTolerance controls the maximum squared
//     luminance difference above which a pixel is considered "new".
//
// Pixels are processed 4 at a time using MMX instructions.
//
// SQUARING NOTE:
// We square luminance differences to amplify the effects of large
// differences and to avoid dealing with negative differences.  Unfortunately,
// we can't compare the square of difference directly against a threshold,
// thanks to the lack of an MMX unsigned compare instruction.  The
// problem is that if we had two pixels with luminance 0 and 255,
// the difference squared would be 65025, which is a negative
// 16-bit signed value and would thus compare less than a threshold.
// We get around this by dividing all the luminance values by two before
// squaring them; this results in an effective maximum luminance
// difference of 127, whose square (16129) is safely comparable.


#if defined(IS_SSE)
#define MAINLOOP_LABEL DoNext8Bytes_SSE
#elif defined(IS_3DNOW)
#define MAINLOOP_LABEL DoNext8Bytes_3DNow
#else
#define MAINLOOP_LABEL DoNext8Bytes_MMX
#endif


#if defined(IS_SSE)
BOOL DeinterlaceFieldTwoFrame_SSE(TDeinterlaceInfo* pInfo)
#elif defined(IS_3DNOW)
BOOL DeinterlaceFieldTwoFrame_3DNOW(TDeinterlaceInfo* pInfo)
#else
BOOL DeinterlaceFieldTwoFrame_MMX(TDeinterlaceInfo* pInfo)
#endif
{
    int Line;
    BYTE* YVal0;
    BYTE* YVal1;
    BYTE* YVal2;
    BYTE* OVal0;
    BYTE* OVal1;
    BYTE* OVal2;
    DWORD OldSI;
    DWORD OldSP;
    BYTE* Dest = pInfo->Overlay;
    DWORD Pitch = pInfo->InputPitch;
    DWORD LineLength = pInfo->LineLength;

    const __int64 YMask    = 0x00ff00ff00ff00ffLL;

    __int64 qwSpatialTolerance;
    __int64 qwTemporalTolerance;
    __int64 qwAllOnes = 0xffffffffffffffffLL;
    __int64 qwBobbedPixels;
    const __int64 Mask = 0x7f7f7f7f7f7f7f7fLL;

    qwSpatialTolerance = TwoFrameSpatialTolerance / 4;      // divide by 4 because of squaring behavior, see below
    qwSpatialTolerance += (qwSpatialTolerance << 48) + (qwSpatialTolerance << 32) + (qwSpatialTolerance << 16);
    qwTemporalTolerance = TwoFrameTemporalTolerance / 4;
    qwTemporalTolerance += (qwTemporalTolerance << 48) + (qwTemporalTolerance << 32) + (qwTemporalTolerance << 16);

    // copy first even line no matter what, and the first odd line if we're
    // processing an odd field.

    if(pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD)
    {
        YVal0 = pInfo->PictureHistory[0]->pData;
        YVal1 = pInfo->PictureHistory[1]->pData + Pitch;
        YVal2 = YVal0 + Pitch;
        OVal0 = pInfo->PictureHistory[2]->pData;
        OVal1 = pInfo->PictureHistory[3]->pData + Pitch;
        OVal2 = OVal0 + Pitch;

        pInfo->pMemcpy(Dest, pInfo->PictureHistory[1]->pData, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;
        
        pInfo->pMemcpy(Dest, YVal0, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;
    }
    else
    {
        YVal0 = pInfo->PictureHistory[0]->pData;
        YVal1 = pInfo->PictureHistory[1]->pData;
        YVal2 = YVal0 + Pitch;
        OVal0 = pInfo->PictureHistory[2]->pData;
        OVal1 = pInfo->PictureHistory[3]->pData;
        OVal2 = OVal0 + Pitch;

        pInfo->pMemcpy(Dest, YVal0, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;
    }

    for (Line = 0; Line < pInfo->FieldHeight - 1; ++Line)
    {
_saved_regs;
		BYTE* Dest2 = Dest;
		BYTE* OVal2UseInAsm = OVal2;
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
"            mov eax, %[YVal0]      ## eax = T1\n"
"            mov ebx, %[YVal1]      ## ebx = M1\n"
"            mov esp, %[YVal2]      ## esp = B1\n"
"            mov edx, %[OVal0]      ## edx = T0\n"
"            mov esi, %[OVal1]      ## esi = M0\n"
"            shr ecx, 3                      ## there are LineLength / 8 qwords\n"
"\n"
".align 8\n"
_strf(MAINLOOP_LABEL) ":\n"
"\n"
"            mov edi, %[OVal2UseInAsm]      ## edi = B0\n"
"            movq mm0, qword ptr[eax]        ## mm0 = T1\n"
"            movq mm1, qword ptr[esp]        ## mm1 = B1\n"
"            movq mm2, qword ptr[ebx]        ## mm2 = M1\n"
"\n"
"            ## Average T1 and B1 so we can do interpolated bobbing if we bob onto T1.\n"
"            movq mm7, mm1                   ## mm7 = B1\n"
"\n"
#if defined(IS_SSE)
"            pavgb mm7, mm0\n"
#elif defined(IS_3DNOW)
"            pavgusb mm7, mm0\n"
#else
"            movq mm5, mm0                   ## mm5 = T1\n"
"            psrlw mm7, 1                    ## mm7 = B1 / 2\n"
"            pand mm7, %[Mask]                  ## mask off lower bits\n"
"            psrlw mm5, 1                    ## mm5 = T1 / 2\n"
"            pand mm5, %[Mask]                  ## mask off lower bits\n"
"            paddw mm7, mm5                  ## mm7 = (T1 + B1) / 2\n"
#endif
"            movq %[qwBobbedPixels], mm7\n"
"\n"
"            ## Now that we've averaged them, we no longer care about the chroma\n"
"            ## values of T1 and B1 (all our comparisons are luminance-only).\n"
"            pand mm0, %[YMask]                 ## mm0 = luminance(T1)\n"
"            pand mm1, %[YMask]                 ## mm1 = luminance(B1)\n"
"\n"
"            ## Find out whether M1 is new.  \"New\" means the square of the\n"
"            ## luminance difference between M1 and M0 is less than the temporal\n"
"            ## tolerance.\n"
"            ##\n"
"            movq mm7, mm2                   ## mm7 = M1\n"
"            movq mm4, qword ptr[esi]        ## mm4 = M0\n"
"            pand mm7, %[YMask]                 ## mm7 = luminance(M1)\n"
"            movq mm6, mm7                   ## mm6 = luminance(M1)     used below\n"
"            pand mm4, %[YMask]                 ## mm4 = luminance(M0)\n"
"            psubsw mm7, mm4                 ## mm7 = M1 - M0\n"
"            psraw mm7, 1                    ## mm7 = M1 - M0 (see SQUARING NOTE above)\n"
"            pmullw mm7, mm7                 ## mm7 = (M1 - M0) ^ 2\n"
"            pcmpgtw mm7, %[qwTemporalTolerance] ## mm7 = 0xffff where (M1 - M0) ^ 2 > threshold, 0x0000 otherwise\n"
"\n"
"            ## Find out how different T1 and M1 are.\n"
"            movq mm3, mm0                   ## mm3 = T1\n"
"            psubsw mm3, mm6                 ## mm3 = T1 - M1\n"
"            psraw mm3, 1                    ## mm3 = T1 - M1 (see SQUARING NOTE above)\n"
"            pmullw mm3, mm3                 ## mm3 = (T1 - M1) ^ 2\n"
"            pcmpgtw mm3, %[qwSpatialTolerance] ## mm3 = 0xffff where (T1 - M1) ^ 2 > threshold, 0x0000 otherwise\n"
"\n"
"            ## Find out how different B1 and M1 are.\n"
"            movq mm4, mm1                   ## mm4 = B1\n"
"            psubsw mm4, mm6                 ## mm4 = B1 - M1\n"
"            psraw mm4, 1                    ## mm4 = B1 - M1 (see SQUARING NOTE above)\n"
"            pmullw mm4, mm4                 ## mm4 = (B1 - M1) ^ 2\n"
"            pcmpgtw mm4, %[qwSpatialTolerance] ## mm4 = 0xffff where (B1 - M1) ^ 2 > threshold, 0x0000 otherwise\n"
"\n"
"            ## We care about cases where M1 is different from both T1 and B1.\n"
"            pand mm3, mm4                   ## mm3 = 0xffff where M1 is different from T1 and B1, 0x0000 otherwise\n"
"\n"
"            ## Find out whether T1 is new.\n"
"            movq mm4, mm0                   ## mm4 = T1\n"
"            movq mm5, qword ptr[edx]        ## mm5 = T0\n"
"            pand mm5, %[YMask]                 ## mm5 = luminance(T0)\n"
"            psubsw mm4, mm5                 ## mm4 = T1 - T0\n"
"            psraw mm4, 1                    ## mm4 = T1 - T0 (see SQUARING NOTE above)\n"
"            pmullw mm4, mm4                 ## mm4 = (T1 - T0) ^ 2 / 4\n"
"            pcmpgtw mm4, %[qwTemporalTolerance] ## mm4 = 0xffff where (T1 - T0) ^ 2 > threshold, 0x0000 otherwise\n"
"\n"
"            ## Find out whether B1 is new.\n"
"            movq mm5, mm1                   ## mm5 = B1\n"
"            movq mm6, qword ptr[edi]        ## mm6 = B0\n"
"            pand mm6, %[YMask]                 ## mm6 = luminance(B0)\n"
"            psubsw mm5, mm6                 ## mm5 = B1 - B0\n"
"            psraw mm5, 1                    ## mm5 = B1 - B0 (see SQUARING NOTE above)\n"
"            pmullw mm5, mm5                 ## mm5 = (B1 - B0) ^ 2\n"
"            pcmpgtw mm5, %[qwTemporalTolerance] ## mm5 = 0xffff where (B1 - B0) ^ 2 > threshold, 0x0000 otherwise\n"
"\n"
"            ## We care about cases where M1 is old and either T1 or B1 is old.\n"
"            por mm4, mm5                    ## mm4 = 0xffff where T1 or B1 is new\n"
"            por mm4, mm7                    ## mm4 = 0xffff where T1 or B1 or M1 is new\n"
"            movq mm6, %[qwAllOnes]             ## mm6 = 0xffffffffffffffff\n"
"            pxor mm4, mm6                   ## mm4 = 0xffff where T1 and B1 and M1 are old\n"
"\n"
"            ## Pick up the interpolated (T1+B1)/2 pixels.\n"
"            movq mm1, %[qwBobbedPixels]        ## mm1 = (T1 + B1) / 2\n"
"\n"
"            ## At this point:\n"
"            ##  mm1 = (T1+B1)/2\n"
"            ##  mm2 = M1\n"
"            ##  mm3 = mask, 0xffff where M1 is different from both T1 and B1\n"
"            ##  mm4 = mask, 0xffff where T1 and B1 and M1 are old\n"
"            ##  mm6 = 0xffffffffffffffff\n"
"            ##\n"
"            ## Now figure out where we're going to weave and where we're going to bob.\n"
"            ## We'll weave if all pixels are old or M1 isn't different from both its\n"
"            ## neighbors.\n"
"            pxor mm3, mm6                   ## mm3 = 0xffff where M1 is the same as either T1 or B1\n"
"            por mm3, mm4                    ## mm3 = 0xffff where M1 and T1 and B1 are old or M1 = T1 or B1\n"
"            pand mm2, mm3                   ## mm2 = woven data where T1 or B1 isn't new or they're different\n"
"            pandn mm3, mm1                  ## mm3 = bobbed data where T1 or B1 is new and they're similar\n"
"            por mm3, mm2                    ## mm3 = finished pixels\n"
"\n"
"            ## Shuffle some registers around since there aren't enough of them\n"
"            ## to hold all our pointers at once.\n"
"            add edi, 8\n"
"            mov %[OVal2UseInAsm], edi\n"
"            mov edi, %[Dest2]\n"
"\n"
"           ## Put the pixels in place.\n"
#ifdef IS_SSE
"            movntq qword ptr[edi], mm3\n"
#else
"            movq qword ptr[edi], mm3\n"
#endif
"\n"
"            ## Advance to the next set of pixels.\n"
"            add eax, 8\n"
"            add ebx, 8\n"
"            add edx, 8\n"
"            add esi, 8\n"
"            add esp, 8\n"
"            add edi, 8\n"
"            mov %[Dest2], edi\n"
"            dec ecx\n"
"            jne " _strf(MAINLOOP_LABEL) "\n"
"\n"
"            mov esi, %[OldSI]\n"
"            mov esp, %[OldSP]\n"
_asm_end,
_m(OldSI), _m(OldSP), _m(LineLength), _m(YVal0), _m(YVal1), _m(YVal2),
_m(OVal0), _m(OVal1), _m(OVal2UseInAsm), _m(Mask), _m(qwBobbedPixels),
_m(YMask), _m(qwTemporalTolerance), _m(qwSpatialTolerance), _m(qwAllOnes),
_m(Dest2)
: "eax", "ecx", "edx", "edi");

        Dest += pInfo->OverlayPitch;

        // Always use the most recent data verbatim.  By definition it's correct (it'd
        // be shown on an interlaced display) and our job is to fill in the spaces
        // between the new lines.
        pInfo->pMemcpy(Dest, YVal2, pInfo->LineLength);
        Dest += pInfo->OverlayPitch;

        YVal0 += Pitch;
        YVal1 += Pitch;
        YVal2 += Pitch;
        OVal0 += Pitch;
        OVal1 += Pitch;
        OVal2 += Pitch;

    }

    // Copy last odd line if we're processing an even field.
    if(pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_EVEN)
    {
        pInfo->pMemcpy(Dest, YVal1, pInfo->LineLength);
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

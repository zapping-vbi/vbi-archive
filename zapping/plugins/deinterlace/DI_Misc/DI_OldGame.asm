/////////////////////////////////////////////////////////////////////////////
// $Id: DI_OldGame.asm,v 1.1 2005-01-08 14:54:23 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2001 Lindsey Dubb.  All rights reserved.
// based on OddOnly and Temporal Noise DScaler Plugins
// (c) John Adcock & Steve Grimm
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
// Revision 1.5  2001/12/20 03:44:07  lindsey
// Improved averaging for MMX machines
// Miscellaneous reorganization
//
// Revision 1.4  2001/11/22 13:32:04  adcockj
// Finished changes caused by changes to TDeinterlaceInfo - Compiles
//
// Revision 1.3  2001/11/21 15:21:40  adcockj
// Renamed DEINTERLACE_INFO to TDeinterlaceInfo in line with standards
// Changed TDeinterlaceInfo structure to have history of pictures.
//
// Revision 1.2  2001/08/30 10:03:51  adcockj
// Slightly improved the color averaging
// Added a "composite mode" switch to force averaging when crosstalk is more important than blur.
// Commented the code
// Reorganized and edited to follow the coding guidelines
// Most importantly: Added a silly quote
// (Changes made on behalf of Lindsey Dubb)
//
// Revision 1.1  2001/07/30 08:25:22  adcockj
// Added Lindsey Dubb's method
//
/////////////////////////////////////////////////////////////////////////////

// Processor specific averaging:
// Set destMM to average of destMM and sourceMM
// Note that this is a somewhat unconventional averaging function: It rounds toward
// the first operand if it is (even and larger) or (odd and smaller).  This is faster
// and just as effective here as "round toward even."
// Explanation of the MMX version: 1 is added to the source pixel if it is odd (and != 255)
// Then half the (adjusted) source pixel (rounding down -- which is effectively the same as
// rounding the unadjusted pixel up unless source == 255) is added to half the destination
// pixel (also rounding down). This gives the same result as the much faster and less 
// complicated versions for other processors
//.Yes, shiftMask and noLowBitsMask could be the same, but this is a little easier to
// follow.

// tempMM is changed 

#undef AVERAGE
#if defined(IS_SSE)
#define AVERAGE(destMM, sourceMM, tempMM, shiftMask, noLowBitsMask) \
"    pand " #destMM ", %[" #noLowBitsMask "]\n" \
"    pavgb " #destMM ", " #sourceMM "\n"
#elif defined(IS_3DNOW)
#define AVERAGE(destMM, sourceMM, tempMM, shiftMask, noLowBitsMask) \
"    pand " #destMM ", %[" #noLowBitsMask "]\n" \
"    pavgusb " #destMM ", " #sourceMM "\n"
#else
#define AVERAGE(destMM, sourceMM, tempMM, shiftMask, noLowBitsMask) \
"    movq " #tempMM ", %[" #noLowBitsMask "]\n" \
"    pandn " #tempMM ", " #sourceMM "\n" \
"    paddusb " #tempMM ", " #sourceMM "\n" \
"    pand " #tempMM ", %[" #shiftMask "]\n" \
"    psrlw " #tempMM ", 1 \n" \
"    pand " #destMM ", %[" #shiftMask "]\n" \
"    psrlw " #destMM ", 1 \n" \
"    paddusb " #destMM ", " #tempMM "\n"
#endif // processor specific averaging routine

#if defined(IS_SSE)
#define MAINLOOP_LABEL DoNext8Bytes_SSE
#elif defined(IS_3DNOW)
#define MAINLOOP_LABEL DoNext8Bytes_3DNow
#else
#define MAINLOOP_LABEL DoNext8Bytes_MMX
#endif

// Hidden in the preprocessor stuff below is the actual routine

#if defined(IS_SSE)
long OldGameFilter_SSE(TDeinterlaceInfo* pInfo)
#elif defined(IS_3DNOW)
long OldGameFilter_3DNOW(TDeinterlaceInfo* pInfo)
#else
long OldGameFilter_MMX(TDeinterlaceInfo* pInfo)
#endif
{
#ifdef OLDGAME_DEBUG
    {
        char    OutputString[64];
        wsprintf(OutputString, "Motion %u", pInfo->CombFactor);
        if (gPfnSetStatus != NULL)
        {
            gPfnSetStatus(OutputString);
        }
    }
#endif // Debug output
    // If the field is significantly different from the previous one,
    // show the new frame unaltered.
    // This is just a tiny change on the evenOnly/oddOnly filters

    if (!pInfo->PictureHistory[0])
    {
        return FALSE;
    }

    if (
        (!pInfo->PictureHistory[1])
        || ((gDisableMotionChecking == FALSE) && (pInfo->CombFactor > gMaxComb))
    ) {
        BYTE* pThisLine = pInfo->PictureHistory[0]->pData;
        DWORD LineTarget = 0;

        if (pThisLine == NULL)
        {
            return TRUE;
        }
        for (LineTarget = 0; LineTarget < (DWORD)pInfo->FieldHeight; LineTarget++)
        {
            // copy latest field's rows to overlay, resulting in a half-height image.
            pInfo->pMemcpy(pInfo->Overlay + LineTarget * pInfo->OverlayPitch,
                        pThisLine,
                        pInfo->LineLength);

            pThisLine += pInfo->InputPitch;
        }
    }
    // If the field is very similar to the last one, average them.
    // This code is a cut down version of Steven Grimm's temporal noise filter.
    // It does a really nice job on video via a composite connector.
    else
    {
        BYTE*           pNewLines = pInfo->PictureHistory[0]->pData;
        const DWORD     Cycles = ((DWORD)pInfo->LineLength) / 8;
        const __int64   qwShiftMask = 0xFEFFFEFFFEFFFEFFLL;
        const __int64   qwNoLowBitsMask = 0xFEFEFEFEFEFEFEFELL;
        BYTE*           pDestination = pInfo->Overlay;
        DWORD           LineTarget = 0;
        BYTE*           pOldLines = pInfo->PictureHistory[1]->pData;

        if ((pNewLines == NULL) || (pOldLines == NULL))
        {
            return TRUE;
        }

        for (LineTarget = 0; LineTarget < (DWORD)pInfo->FieldHeight; ++LineTarget)
        {
_saved_regs;
_asm_begin
"                mov esi, %[pDestination]           ## Pointers are incremented at the bottom of the loop\n"
"                mov ecx, %[Cycles]\n"
"                mov eax, %[pNewLines]\n"
"                mov ebx, %[pOldLines]\n"
"\n"
_strf(MAINLOOP_LABEL) ":\n"
"\n"
"                movq mm2, qword ptr[eax]        ## mm2 = NewPixel\n"
"                movq mm1, qword ptr[ebx]        ## mm1 = OldPixel\n"
"\n"
"                ## Now determine the weighted averages of the old and new pixel values.\n"
"                ## Since the frames are likely to be similar for only a short time, use\n"
"                ## a more even weighting than employed in the temporal nose filter\n"
                AVERAGE(mm2, mm1, mm7, qwShiftMask, qwNoLowBitsMask)
"\n"
"                movq qword ptr[esi], mm2        ## Output to the overlay buffer\n"
"\n"
"                add eax, 8\n"
"                add ebx, 8\n"
"                add esi, 8                      ## Move the output pointer\n"
"                loop " _strf(MAINLOOP_LABEL) "\n"
_asm_end,
_m(pDestination), _m(Cycles), _m(pNewLines), _m(pOldLines),
_m(qwShiftMask), _m(qwNoLowBitsMask)
: "eax", "ecx", "esi");

            pDestination += pInfo->OverlayPitch;
            pNewLines += pInfo->InputPitch;
            pOldLines += pInfo->InputPitch;
        }
    }
    _asm
    {
        emms
    }
    return TRUE;
}

#undef MAINLOOP_LABEL

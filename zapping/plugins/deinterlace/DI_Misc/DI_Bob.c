/////////////////////////////////////////////////////////////////////////////
// $Id: DI_Bob.c,v 1.1 2005-01-08 14:54:23 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000 John Adcock.  All rights reserved.
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
// Change Log
//
// Date          Developer             Changes
//
// 30 Dec 2000   Mark Rejhon           Split into separate module
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.2  2004/11/15 23:03:19  michael
// *** empty log message ***
//
// Revision 1.1  2004/11/14 15:35:14  michael
// *** empty log message ***
//
// Revision 1.8  2002/06/13 12:10:24  adcockj
// Move to new Setings dialog for filers, video deint and advanced settings
//
// Revision 1.7  2001/11/23 19:33:14  adcockj
// Fixes to bob to make is less jittery
//
// Revision 1.6  2001/11/22 13:32:03  adcockj
// Finished changes caused by changes to TDeinterlaceInfo - Compiles
//
// Revision 1.5  2001/11/21 15:21:40  adcockj
// Renamed DEINTERLACE_INFO to TDeinterlaceInfo in line with standards
// Changed TDeinterlaceInfo structure to have history of pictures.
//
// Revision 1.4  2001/07/13 16:13:33  adcockj
// Added CVS tags and removed tabs
//
/////////////////////////////////////////////////////////////////////////////

#include "windows.h"
#include "DS_Deinterlace.h"
//Z #include "..\help\helpids.h"

/////////////////////////////////////////////////////////////////////////////
// Copies memory to two locations using MMX registers for speed.
void memcpyBOBMMX(void *Dest1, void *Dest2, void *Src, size_t nBytes)
{
 _saved_regs;
 _asm_begin
"        mov     esi, %[Src]\n"
"        mov     edi, %[Dest1]\n"
"        mov     ebx, %[Dest2]\n"
"        mov     ecx, %[nBytes]\n"
"        shr     ecx, 6                      ## nBytes / 64\n"
".align 8\n"
"1: # CopyLoop:\n"
"        movq    mm0, qword ptr[esi]\n"
"        movq    mm1, qword ptr[esi+8*1]\n"
"        movq    mm2, qword ptr[esi+8*2]\n"
"        movq    mm3, qword ptr[esi+8*3]\n"
"        movq    mm4, qword ptr[esi+8*4]\n"
"        movq    mm5, qword ptr[esi+8*5]\n"
"        movq    mm6, qword ptr[esi+8*6]\n"
"        movq    mm7, qword ptr[esi+8*7]\n"
"        movq    qword ptr[edi], mm0\n"
"        movq    qword ptr[edi+8*1], mm1\n"
"        movq    qword ptr[edi+8*2], mm2\n"
"        movq    qword ptr[edi+8*3], mm3\n"
"        movq    qword ptr[edi+8*4], mm4\n"
"        movq    qword ptr[edi+8*5], mm5\n"
"        movq    qword ptr[edi+8*6], mm6\n"
"        movq    qword ptr[edi+8*7], mm7\n"
"        movq    qword ptr[ebx], mm0\n"
"        movq    qword ptr[ebx+8*1], mm1\n"
"        movq    qword ptr[ebx+8*2], mm2\n"
"        movq    qword ptr[ebx+8*3], mm3\n"
"        movq    qword ptr[ebx+8*4], mm4\n"
"        movq    qword ptr[ebx+8*5], mm5\n"
"        movq    qword ptr[ebx+8*6], mm6\n"
"        movq    qword ptr[ebx+8*7], mm7\n"
"        add     esi, 64\n"
"        add     edi, 64\n"
"        add     ebx, 64\n"
"        dec ecx\n"
"        jne 1b # CopyLoop\n"
"\n"
"        mov     ecx, %[nBytes]\n"
"        and     ecx, 63\n"
"        cmp     ecx, 0\n"
"        je 3f # EndCopyLoop\n"
".align 8\n"
"2: # CopyLoop2:\n"
"        mov dl, byte ptr[esi] \n"
"        mov byte ptr[edi], dl\n"
"        mov byte ptr[ebx], dl\n"
"        inc esi\n"
"        inc edi\n"
"        inc ebx\n"
"        dec ecx\n"
"        jne 2b # CopyLoop2\n"
"3: # EndCopyLoop:\n"
   _asm_end,
   _m(Src), _m(Dest1), _m(Dest2), _m(nBytes)
   : "ecx", "esi", "edi");
}

/////////////////////////////////////////////////////////////////////////////
// Copies memory to two locations using MMX registers for speed.
void memcpyBOBSSE(void *Dest1, void *Dest2, void *Src, size_t nBytes)
{
_saved_regs;
_asm_begin
"        mov     esi, %[Src]\n"
"        mov     edi, %[Dest1]\n"
"        mov     ebx, %[Dest2]\n"
"        mov     ecx, %[nBytes]\n"
"        shr     ecx, 7                      ## nBytes / 128\n"
".align 8\n"
"1: # CopyLoop:\n"
//Z s/xmmword/qword b/c xmmword unknown
"        movaps  xmm0, qword ptr[esi]\n"
"        movaps  xmm1, qword ptr[esi+16*1]\n"
"        movaps  xmm2, qword ptr[esi+16*2]\n"
"        movaps  xmm3, qword ptr[esi+16*3]\n"
"        movaps  xmm4, qword ptr[esi+16*4]\n"
"        movaps  xmm5, qword ptr[esi+16*5]\n"
"        movaps  xmm6, qword ptr[esi+16*6]\n"
"        movaps  xmm7, qword ptr[esi+16*7]\n"
"        movntps qword ptr[edi], xmm0\n"
"        movntps qword ptr[edi+16*1], xmm1\n"
"        movntps qword ptr[edi+16*2], xmm2\n"
"        movntps qword ptr[edi+16*3], xmm3\n"
"        movntps qword ptr[edi+16*4], xmm4\n"
"        movntps qword ptr[edi+16*5], xmm5\n"
"        movntps qword ptr[edi+16*6], xmm6\n"
"        movntps qword ptr[edi+16*7], xmm7\n"
"        movntps qword ptr[ebx], xmm0\n"
"        movntps qword ptr[ebx+16*1], xmm1\n"
"        movntps qword ptr[ebx+16*2], xmm2\n"
"        movntps qword ptr[ebx+16*3], xmm3\n"
"        movntps qword ptr[ebx+16*4], xmm4\n"
"        movntps qword ptr[ebx+16*5], xmm5\n"
"        movntps qword ptr[ebx+16*6], xmm6\n"
"        movntps qword ptr[ebx+16*7], xmm7\n"
"        add     esi, 128\n"
"        add     edi, 128\n"
"        add     ebx, 128\n"
"        dec ecx\n"
"        jne 1b # CopyLoop\n"
"\n"
"        mov     ecx, %[nBytes]\n"
"        and     ecx, 127\n"
"        cmp     ecx, 0\n"
"        je 3f # EndCopyLoop\n"
".align 8\n"
"2: # CopyLoop2:\n"
"        mov dl, byte ptr[esi] \n"
"        mov byte ptr[edi], dl\n"
"        mov byte ptr[ebx], dl\n"
"        inc esi\n"
"        inc edi\n"
"        inc ebx\n"
"        dec ecx\n"
"        jne 2b # CopyLoop2\n"
"3: # EndCopyLoop:\n"
_asm_end,
  _m(Src), _m(Dest1), _m(Dest2), _m(nBytes)
    : "ecx", "esi", "edi");
}

/////////////////////////////////////////////////////////////////////////////
// Simple Bob.  Copies the most recent field to the overlay, with each scanline
// copied twice.
/////////////////////////////////////////////////////////////////////////////
BOOL DeinterlaceBob(TDeinterlaceInfo* pInfo)
{
    int i;
    BYTE* lpOverlay = pInfo->Overlay;
    BYTE* CurrentLine = pInfo->PictureHistory[0]->pData;
    DWORD Pitch = pInfo->InputPitch;

    // No recent data?  We can't do anything.
    if (CurrentLine == NULL)
    {
        return FALSE;
    }
    
    // If field is odd we will offset it down 1 line to avoid jitter  TRB 1/21/01
    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD)
    {
        if (pInfo->CpuFeatureFlags & FEATURE_SSE)
        {
            pInfo->pMemcpy(lpOverlay, CurrentLine, pInfo->LineLength);   // extra copy of first line
            lpOverlay += pInfo->OverlayPitch;                            // and offset out output ptr
            for (i = 0; i < pInfo->FieldHeight - 1; i++)
            {
                memcpyBOBSSE(lpOverlay, lpOverlay + pInfo->OverlayPitch,
                    CurrentLine, pInfo->LineLength);
                lpOverlay += 2 * pInfo->OverlayPitch;
                CurrentLine += Pitch;
            }
            pInfo->pMemcpy(lpOverlay, CurrentLine, pInfo->LineLength);   // only 1 copy of last line
        }
        else
        {
            pInfo->pMemcpy(lpOverlay, CurrentLine, pInfo->LineLength);   // extra copy of first line
            lpOverlay += pInfo->OverlayPitch;                    // and offset out output ptr
            for (i = 0; i < pInfo->FieldHeight - 1; i++)
            {
                memcpyBOBMMX(lpOverlay, lpOverlay + pInfo->OverlayPitch,
                    CurrentLine, pInfo->LineLength);
                lpOverlay += 2 * pInfo->OverlayPitch;
                CurrentLine += Pitch;
            }
            pInfo->pMemcpy(lpOverlay, CurrentLine, pInfo->LineLength);   // only 1 copy of last line
        }
    }   
    else
    {
        if (pInfo->CpuFeatureFlags & FEATURE_SSE)
        {
            for (i = 0; i < pInfo->FieldHeight; i++)
            {
                memcpyBOBSSE(lpOverlay, lpOverlay + pInfo->OverlayPitch,
                    CurrentLine, pInfo->LineLength);
                lpOverlay += 2 * pInfo->OverlayPitch;
                CurrentLine += Pitch;
            }
        }
        else
        {
            for (i = 0; i < pInfo->FieldHeight; i++)
            {
                memcpyBOBMMX(lpOverlay, lpOverlay + pInfo->OverlayPitch,
                    CurrentLine, pInfo->LineLength);
                lpOverlay += 2 * pInfo->OverlayPitch;
                CurrentLine += Pitch;
            }
        }
    }
    // need to clear up MMX registers
    _asm
    {
        emms
    }
    return TRUE;
}

DEINTERLACE_METHOD BobMethod =
{
    sizeof(DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    "Simple Bob", 
    NULL,
    FALSE, 
    FALSE, 
    DeinterlaceBob, 
    50, 
    60,
    0,
    NULL,
    INDEX_BOB,
    NULL,
    NULL,
    NULL,
    NULL,
    1,
    0,
    0,
    -1,
    NULL,
    0,
    FALSE,
    FALSE,
    IDH_BOB,
};


DEINTERLACE_METHOD* DI_Bob_GetDeinterlacePluginInfo(long CpuFeatureFlags)
{
    return &BobMethod;
}

#if 0


BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

#endif /* 0 */

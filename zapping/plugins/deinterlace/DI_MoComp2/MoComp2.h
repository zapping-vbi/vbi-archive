/////////////////////////////////////////////////////////////////////////////
// $Id: MoComp2.h,v 1.1 2005-01-08 14:54:24 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2003 Tom Barry & John Adcock.  All rights reserved.
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
//
//  (From Tom Barry)
//  Also, this program is "Philanthropy-Ware".  That is, if you like it and 
//  feel the need to reward or inspire the author then please feel free (but
//  not obligated) to consider joining or donating to the Electronic Frontier
//  Foundation. This will help keep cyber space free of barbed wire and bullsh*t.  
//  See www.eff.org for details
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.3  2003/05/26 15:54:36  adcockj
// Improved Performance of motion detection
// Fixed bug with pBobP
//
// Revision 1.2  2003/02/11 15:40:50  adcockj
// New implementation of StrangeBob based off paper
// Tidy up sources
//
// Revision 1.1  2003/01/02 13:15:01  adcockj
// Added new plug-ins ready for developement by copying TomsMoComp and Gamma
//
/////////////////////////////////////////////////////////////////////////////

#include <malloc.h>
#include <string.h>
//Z #include <math.h>
#include <malloc.h>

#define BOOL unsigned int
#define BYTE unsigned char

BOOL DeinterlaceMoComp2_SSE(TDeinterlaceInfo* pInfo);
BOOL DeinterlaceMoComp2_MMX(TDeinterlaceInfo* pInfo);
BOOL DeinterlaceMoComp2_3DNOW(TDeinterlaceInfo* pInfo);
#define MyMemCopy pMyMemcpy
extern long SearchEffort;

static int  Fieldcopy(void *dest, const void *src, size_t count, 
			int rows, int dst_pitch, int src_pitch);
static void  DoMoComp2_SimpleWeave();

//Z static __int64 Save1; 
//Z static __int64 Save2; 

static const __int64 MOVE  = 0x0f0f0f0f0f0f0f0fLL; 

static const __int64 YMask = 0x00ff00ff00ff00ffLL; // keeps only luma
static const __int64 UVMask =  0xff00ff00ff00ff00LL; // keeps only chroma

static const __int64 ShiftMask = 0xfefffefffefffeffLL;	// to avoid shifting chroma to luma


// Define a few macros for CPU dependent instructions. 
// I suspect I don't really understand how the C macro preprocessor works but
// this seems to get the job done.          // TRB 7/01

// BEFORE USING THESE YOU MUST SET:

// #define SSE_TYPE SSE            (or MMX or 3DNOW)

// some macros for pavgb instruction
//      V_PAVGB(mmr1, mmr2, mmr work register, smask) mmr2 may = mmrw if you can trash it


#define V_PAVGB_MMX(mmr1,mmr2,mmrw,smask) \
"	movq " #mmrw "," #mmr2 "\n" \
"	pand " #mmrw "," #smask "\n" \
"	psrlw " #mmrw ",1\n" \
"	pand " #mmr1 "," #smask "\n" \
"	psrlw " #mmr1 ",1\n" \
"	paddusb " #mmr1 "," #mmrw "\n"

#define V_PAVGB_SSE(mmr1,mmr2,mmrw,smask) " pavgb " #mmr1 "," #mmr2 "\n"
#define V_PAVGB_3DNOW(mmr1,mmr2,mmrw,smask) " pavgusb " #mmr1 "," #mmr2 "\n"
#define V_PAVGB(mmr1,mmr2,mmrw,smask) V_PAVGB2(mmr1,mmr2,mmrw,smask,SSE_TYPE) 
#define V_PAVGB2(mmr1,mmr2,mmrw,smask,ssetyp) V_PAVGB3(mmr1,mmr2,mmrw,smask,ssetyp) 
#define V_PAVGB3(mmr1,mmr2,mmrw,smask,ssetyp) V_PAVGB_##ssetyp(mmr1,mmr2,mmrw,smask) 

// some macros for pmaxub instruction
//      V_PMAXUB(mmr1, mmr2)    
#define V_PMAXUB_MMX(mmr1,mmr2) \
"    psubusb " #mmr1 "," #mmr2 "\n" \
"    paddusb " #mmr1 "," #mmr2 "\n"

#define V_PMAXUB_SSE(mmr1,mmr2) " pmaxub " #mmr1 "," #mmr2 "\n"
#define V_PMAXUB_3DNOW(mmr1,mmr2) V_PMAXUB_MMX(mmr1,mmr2)  // use MMX version
#define V_PMAXUB(mmr1,mmr2) V_PMAXUB2(mmr1,mmr2,SSE_TYPE) 
#define V_PMAXUB2(mmr1,mmr2,ssetyp) V_PMAXUB3(mmr1,mmr2,ssetyp) 
#define V_PMAXUB3(mmr1,mmr2,ssetyp) V_PMAXUB_##ssetyp(mmr1,mmr2) 

// some macros for pminub instruction
//      V_PMINUB(mmr1, mmr2, mmr work register)     mmr2 may NOT = mmrw
#define V_PMINUB_MMX(mmr1,mmr2,mmrw) \
"    pcmpeqb " #mmrw "," #mmrw "\n" \
"    psubusb " #mmrw "," #mmr2 "\n" \
"    paddusb " #mmr1 "," #mmrw "\n" \
"    psubusb " #mmr1 "," #mmrw "\n"

#define V_PMINUB_SSE(mmr1,mmr2,mmrw) " pminub " #mmr1 "," #mmr2 "\n"
#define V_PMINUB_3DNOW(mmr1,mmr2,mmrw) V_PMINUB_MMX(mmr1,mmr2,mmrw)  // use MMX version
#define V_PMINUB(mmr1,mmr2,mmrw) V_PMINUB2(mmr1,mmr2,mmrw,SSE_TYPE) 
#define V_PMINUB2(mmr1,mmr2,mmrw,ssetyp) V_PMINUB3(mmr1,mmr2,mmrw,ssetyp) 
#define V_PMINUB3(mmr1,mmr2,mmrw,ssetyp) V_PMINUB_##ssetyp(mmr1,mmr2,mmrw) 

// some macros for movntq instruction
//      V_MOVNTQ(mmr1, mmr2) 
#define V_MOVNTQ_MMX(mmr1,mmr2) " movq " #mmr1 "," #mmr2 "\n"
#define V_MOVNTQ_3DNOW(mmr1,mmr2) " movq " #mmr1 "," #mmr2 "\n"
#define V_MOVNTQ_SSE(mmr1,mmr2) " movntq " #mmr1 "," #mmr2 "\n"
#define V_MOVNTQ(mmr1,mmr2) V_MOVNTQ2(mmr1,mmr2,SSE_TYPE) 
#define V_MOVNTQ2(mmr1,mmr2,ssetyp) V_MOVNTQ3(mmr1,mmr2,ssetyp) 
#define V_MOVNTQ3(mmr1,mmr2,ssetyp) V_MOVNTQ_##ssetyp(mmr1,mmr2)

// end of macros

// macro load a field from this object
#if 0
#define thisLoad(reg, intfield) \
		__asm {	\
		__asm mov	reg, this \
		__asm mov   reg, dword ptr [reg].intfield \
		}
#endif

// sorts registers a & b
#define SORT(a,b,temp1) \
"   movq " #temp1 "," #a "\n" \
"   pminub " #a "," #b "\n" \
"   pmaxub " #b "," #temp1 "\n"

#define PABS(a,b,temp1) \
"   movq " #temp1 "," #b "\n" \
"   psubusb " #temp1 "," #a "\n" \
"   psubusb " #a "," #b "\n" \
"   por " #a "," #temp1 "\n"

// a = a where mask, b otherwise; mask = -1
#define COMBINE(a,b,mask) \
"	pand " #a "," #mask "\n" \
"	pandn " #mask "," #b "\n" \
"	por " #a "," #mask "\n"

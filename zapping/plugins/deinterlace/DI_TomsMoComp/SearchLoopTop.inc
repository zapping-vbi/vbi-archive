

__declspec(align(16)) __int64 Min_Vals[2];
__declspec(align(16)) __int64 Max_Vals[2];

BYTE*	pDest;
const BYTE*	pSrcP;
const BYTE*	pSrc;
const BYTE*	pBob;
const BYTE*	pBobP;

#if defined(DBL_RESIZE) || defined(USE_FOR_DSCALER)
int	    src_pitch2 = src_pitch;			// even & odd lines are not interleaved in DScaler
#else
int	    src_pitch2 = 2 * src_pitch;		// even & odd lines are interleaved in Avisynth
#endif

int	    dst_pitch2 = 2 * dst_pitch;
int     y;

#ifdef IS_SSE2
int     Last8 = (rowsize-16);			// ofs to last 16 bytes in row for SSE2
#else
int     Last8 = (rowsize-8);			// ofs to last 8 bytes in row
#endif

int		SaveMotion = 0;
int		dst_pitchw = dst_pitch; // local stor so asm can ref
	pSrc  = pWeaveSrc;			// points 1 weave line above
	pSrcP = pWeaveSrcP;			// " 

#ifdef DBL_RESIZE
	        
#ifdef USE_VERTICAL_FILTER
	pDest = pWeaveDest + dst_pitch2;
#else
	pDest = pWeaveDest + 3*dst_pitch;
#endif

#else

#ifdef USE_VERTICAL_FILTER
	pDest = pWeaveDest + dst_pitch;
#else
	pDest = pWeaveDest + dst_pitch2;
#endif

#endif

	if (TopFirst)
	{
		pBob = pCopySrc + src_pitch2;      // remember one weave line just copied previously
		pBobP = pCopySrcP + src_pitch2;
	}
	else
	{
		pBob =  pCopySrc;
		pBobP =  pCopySrcP;
	}

	for (y=1; y < FldHeight-1; y++)	
	{
_saved_regs;

		// pretend it's indented -->>
_asm_begin
"		## Loop general reg usage\n"
"		##\n"
"		## eax - pBobP, then pDest \n"
"		## ebx - pBob\n"
"		## ecx - src_pitch2\n"
"		## edx - current offset\n"
"		## edi - prev weave pixels, 1 line up\n"
"		## esi - next weave pixels, 1 line up\n"
"\n"
"\n"
"\n"
"\n"
#ifdef IS_SSE2
"\n"
"## sse2 code deleted for now\n"
"\n"
#else
"		## simple bob first 8 bytes\n"
"		mov		ebx, %[pBob]\n"
"		mov		ecx, %[src_pitch2]\n"
"\n"
#ifdef USE_VERTICAL_FILTER
"		movq	mm0, qword ptr[ebx]\n"
"		movq	mm1, qword ptr[ebx+ecx]\n"
"		movq	mm2, mm0\n"
		V_PAVGB (mm2, mm1, mm3, ShiftMask)		## halfway between
		V_PAVGB (mm0, mm2, mm3, ShiftMask)		## 1/4 way
		V_PAVGB (mm1, mm2, mm3, ShiftMask)		## 3/4 way
"		mov		edi, %[pDest]\n"
"		mov		eax, %[dst_pitchw]\n"
		V_MOVNTQ	(qword ptr[edi], mm0)
		V_MOVNTQ	(qword ptr[edi+eax], mm1)
"\n"
"		## simple bob last 8 bytes\n"
"		mov		edx, %[Last8]\n"
"		lea		esi, [ebx+edx]\n"
"		movq	mm0, qword ptr[esi]\n"
"		movq	mm1, qword ptr[esi+ecx]\n"
"		movq	mm2, mm0\n"
		V_PAVGB (mm2, mm1, mm3, ShiftMask)		// halfway between
		V_PAVGB (mm0, mm2, mm3, ShiftMask)		// 1/4 way
		V_PAVGB (mm1, mm2, mm3, ShiftMask)		// 3/4 way
"		add		edi, edx						## last 8 bytes of dest\n"
		V_MOVNTQ	(qword ptr[edi], mm0)
		V_MOVNTQ	(qword ptr[edi+eax], mm1)
"\n"
#else
"		movq	mm0, qword ptr[ebx]\n"
"##		pavgb	mm0, qword ptr[ebx+ecx]\n"
		V_PAVGB (mm0, qword ptr[ebx+ecx], mm2, ShiftMask)
"		mov		edi, %[pDest]\n"
		V_MOVNTQ	(qword ptr[edi], mm0)
"\n"
"		## simple bob last 8 bytes\n"
"		mov		edx, %[Last8]\n"
"		lea		esi, [ebx+edx]\n"
"		movq	mm0, qword ptr[esi]\n"
"##		pavgb	mm0, qword ptr[esi+ecx]\n"
		V_PAVGB	(mm0, qword ptr[esi+ecx], mm2, ShiftMask)
		V_MOVNTQ	(qword ptr[edi+edx], mm0)
#endif
"		## now loop and get the middle qwords\n"
"		mov		esi, %[pSrc]\n"
"		mov		edi, %[pSrcP]\n"
"		mov		edx, 8				## curr offset into all lines\n"
"\n"
"1: # LoopQ:	\n"
"		mov		eax, %[pBobP]\n"
"		add		edi, 8		\n"
"		add		esi, 8		\n"
"		add		ebx, 8\n"
"		add		eax, edx\n"


#ifdef USE_STRANGE_BOB
#include "StrangeBob.inc"
#else
#include "WierdBob.inc"
#endif

		// For non-SSE2:
		// through out most of the rest of this loop we will maintain
		//	mm4		our min bob value
		//	mm5		best weave pixels so far
		//  mm6		our max Bob value 
		//	mm7		best weighted pixel ratings so far

		// We will keep a slight bias to using the weave pixels
		// from the current location, by rating them by the min distance
		// from the Bob value instead of the avg distance from that value.
		// our best and only rating so far
		
"		pcmpeqb	mm7, mm7			## ffff, say we didn't find anything good yet\n"

#endif


#include <malloc.h>
#include <string.h>
//Z #include <math.h>
#include <malloc.h>

#define BOOL unsigned int
#define BYTE unsigned char

#ifdef USE_FOR_DSCALER
//#define USE_VERTICAL_FILTER 

BOOL DeinterlaceTomsMoComp_SSE(TDeinterlaceInfo* pInfo);
//BOOL DeinterlaceTomsMoComp_SSE2(TDeinterlaceInfo* pInfo);
BOOL DeinterlaceTomsMoComp_MMX(TDeinterlaceInfo* pInfo);
BOOL DeinterlaceTomsMoComp_3DNOW(TDeinterlaceInfo* pInfo);
#define MyMemCopy pMyMemcpy
extern long SearchEffort;
extern BOOL UseStrangeBob;

#else
//#define USE_VERTICAL_FILTER 

#define MyMemCopy memcpy


  //static int  Do_Dbl_Resize();
//	int __stdcall Search_Effort_21();

static int  Do_Dbl_Resize();

#endif

static int  Fieldcopy(void *dest, const void *src, size_t count, 
			int rows, int dst_pitch, int src_pitch);
static int  Search_Effort_0();
static int  Search_Effort_1();
static int  Search_Effort_3();
static int  Search_Effort_5();
static int  Search_Effort_9();
static int  Search_Effort_11();
static int  Search_Effort_13();
static int  Search_Effort_15();
static int  Search_Effort_19();
static int  Search_Effort_21();
static int  Search_Effort_Max();

static int  Search_Effort_0_SB();
static int  Search_Effort_1_SB();
static int  Search_Effort_3_SB();
static int  Search_Effort_5_SB();
static int  Search_Effort_9_SB();
static int  Search_Effort_11_SB();
static int  Search_Effort_13_SB();
static int  Search_Effort_15_SB();
static int  Search_Effort_19_SB();
static int  Search_Effort_21_SB();
static int  Search_Effort_Max_SB();


__declspec(align(128))
static __int64 BobDiffW[2]; 

__declspec(align(16))
static __int64 BobVal[2]; 

//Z __declspec(align(16))
//Z static __int64 Min_Vals[2]; 
//Z 
//Z __declspec(align(16))
//Z static __int64 Max_Vals[2]; 
//Z 
__declspec(align(16))
static const __int64 Max_Mov[2] =  {0x0404040404040404LL,0x0404040404040404LL}; 
//static const __int64 Max_Mov[2] =  {0x0f0f0f0f0f0f0f0fLL,0x0f0f0f0f0f0f0f0fLL}; 
//static const __int64 Max_Mov[2] =  {0x0808080808080808LL,0x0808080808080808LL}; 

__declspec(align(16))
static const __int64 DiffThres[2] =  {0x0f0f0f0f0f0f0f0fLL,0x0f0f0f0f0f0f0f0fLL}; 

__declspec(align(16))
static const __int64 YMask[2] = {0x00ff00ff00ff00ffLL,0x00ff00ff00ff00ffLL}; // keeps only luma

__declspec(align(16))
static const __int64 UVMask[2] =   {0xff00ff00ff00ff00LL,0xff00ff00ff00ff00LL}; // keeps only chroma

__declspec(align(16))
static const __int64 TENS[2]     = {0x0a0a0a0a0a0a0a0aLL,0x0a0a0a0a0a0a0a0aLL}; 

__declspec(align(16))
static const __int64 FOURS[2] =    {0x0404040404040404LL,0x0404040404040404LL}; 

__declspec(align(16))
static const __int64 ONES[2] =     {0x0101010101010101LL,0x0101010101010101LL}; 

__declspec(align(16))
static const __int64 Max_Comb[2] = {0x0202020202020202LL,0x0202020202020202LL}; 

__declspec(align(16))
static const __int64 WHITE[2] =    {0x7fff0fff7fff0fffLL,0x7fff0fff7fff0fffLL};

static const __int64 ShiftMask = 0xfefffefffefffeffLL;	// to avoid shifting chroma to luma

static __int64 swork = 0;

/*
// A bunch of things that may need 16 byte alignment

__declspec(align(16))
static const __int64 FIFTEENS[2] = {0x0F0F0F0F0F0F0F0FLL,0x0F0F0F0F0F0F0F0FLL}; 

__declspec(align(16))
static const __int64 TWENTIES[2] = {0x1414141414141414LL,0x1414141414141414LL}; 

__declspec(align(16))
static const __int64 SIXES[2] =    {0x0606060606060606LL,0x0606060606060606LL}; 

__declspec(align(16))
static const __int64 FIVES[2] =    {0x0505050505050505LL,0x0505050505050505LL}; 

__declspec(align(16))
static const __int64 THREES[2] =   {0x0303030303030303LL,0x0303030303030303LL}; 

__declspec(align(16))
static const __int64 TWOS[2] =     {0x0202020202020202LL,0x0202020202020202LL}; 

*/


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

#if 0

// macro load a field from this object
#define thisLoad(reg, intfield) \
		__asm {	\
		__asm mov	reg, this \
		__asm mov   reg, dword ptr [reg].intfield \
		}


#endif /* 0 */

#ifdef IS_SSE2

#define MERGE4PIXavg(PADDR1, PADDR2) \
"	movdqu xmm0, [" _strf(PADDR1) "] #/* our 4 pixels */ \n" \
"	movdqu xmm1, [" _strf(PADDR2) "] #/* our pixel2 value */ \n" \
"	movdqa xmm2, xmm0			#/* another copy of our pixel1 value */ \n" \
"	movdqa xmm3, xmm1			#/* another copy of our pixel1 value */ \n" \
"	psubusb xmm2, xmm1 \n" \
"	psubusb xmm3, xmm0 \n" \
"	por xmm2,xmm3 \n" \
"	pavgb xmm0, xmm1			#/* avg of 2 pixels */ \n" \
"	movdqa xmm3, xmm2			#/* another copy of our our weights */ \n" \
"	pxor	 xmm1, xmm1 \n" \
"	psubusb xmm3, xmm7			#/* nonzero where old weights lower, else 0 */ \n" \
"	pcmpeqb xmm3, xmm1			#/* now ff where new better, else 00	*/ \n" \
"	pcmpeqb xmm1, xmm3		#/* here ff where old better, else 00 */ \n" \
"	pand	xmm0, xmm3			#/* keep only better new pixels */ \n" \
"	pand  xmm2, xmm3			#/* and weights */ \n" \
"	pand	xmm5, xmm1			#/* keep only better old pixels */ \n" \
"	pand  xmm7, xmm1 \n" \
"	por	xmm5, xmm0			#/* and merge new & old vals */ \n" \
"	por	xmm7, xmm2 \n"


#define MERGE4PIXavgH(PADDR1A, PADDR1B, PADDR2A, PADDR2B) \
"	movdqu xmm0, [" _strf(PADDR1A) "] #/* our 4 pixels */ \n" \
"	movdqu xmm1, [" _strf(PADDR2A) "] #/* our pixel2 value */ \n" \
"	movdqu xmm2, [" _strf(PADDR1B) "] #/* our 4 pixels */ \n" \
"	movdqu xmm3, [" _strf(PADDR2B) "] #/* our pixel2 value */ \n" \
"	pavgb	 xmm0, xmm2 \n" \
"	pavgb	 xmm1, xmm3 \n" \
"	movdqa xmm2, xmm0			#/* another copy of our pixel1 value */ \n" \
"	movdqa xmm3, xmm1			#/* another copy of our pixel1 value */ \n" \
"	psubusb xmm2, xmm1 \n" \
"	psubusb xmm3, xmm0 \n" \
"	por xmm2,xmm3 \n" \
"	pavgb xmm0, xmm1			#/* avg of 2 pixels */ \n" \
"	movdqa xmm3, xmm2			#/* another copy of our our weights */ \n" \
"	pxor	 xmm1, xmm1 \n" \
"	psubusb xmm3, xmm7			#/* nonzero where old weights lower, else 0 */ \n" \
"	pcmpeqb xmm3, xmm1			#/* now ff where new better, else 00	*/ \n" \
"	pcmpeqb xmm1, xmm3		#/* here ff where old better, else 00 */ \n" \
"	pand	xmm0, xmm3			#/* keep only better new pixels */ \n" \
"	pand  xmm2, xmm3			#/* and weights */ \n" \
"	pand	xmm5, xmm1			#/* keep only better old pixels */ \n" \
"	pand  xmm7, xmm1 \n" \
"	por	xmm5, xmm0			#/* and merge new & old vals */ \n" \
"	por	xmm7, xmm2 \n" \

#define RESET_CHROMA " por xmm7, UVMask \n"

#else

#define MERGE4PIXavg(PADDR1, PADDR2) \
"	movq mm0, [" _strf(PADDR1) "] #/* our 4 pixels */ \n" \
"	movq mm1, [" _strf(PADDR2) "] #/* our pixel2 value */ \n" \
"	movq mm2, mm0			#/* another copy of our pixel1 value */ \n" \
"	movq mm3, mm1			#/* another copy of our pixel1 value */ \n" \
"	psubusb mm2, mm1 \n" \
"	psubusb mm3, mm0 \n" \
"	por mm2,mm3 \n" \
	V_PAVGB (mm0, mm1, mm3, ShiftMask) /* avg of 2 pixels */ \
"	movq mm3, mm2			#/* another copy of our our weights */ \n" \
"	pxor	 mm1, mm1 \n" \
"	psubusb mm3, mm7			#/* nonzero where old weights lower, else 0 */ \n" \
"	pcmpeqb mm3, mm1			#/* now ff where new better, else 00	*/ \n" \
"	pcmpeqb mm1, mm3		#/* here ff where old better, else 00 */ \n" \
"	pand	mm0, mm3			#/* keep only better new pixels */ \n" \
"	pand  mm2, mm3			#/* and weights */ \n" \
"	pand	mm5, mm1			#/* keep only better old pixels */ \n" \
"	pand  mm7, mm1 \n" \
"	por	mm5, mm0			#/* and merge new & old vals */ \n" \
"	por	mm7, mm2 \n" \


#define MERGE4PIXavgH(PADDR1A, PADDR1B, PADDR2A, PADDR2B) \
"	movq mm0, [" _strf(PADDR1A) "] #/* our 4 pixels */ \n" \
"	movq mm1, [" _strf(PADDR2A) "] #/* our pixel2 value */ \n" \
"	movq mm2, [" _strf(PADDR1B) "] #/* our 4 pixels */ \n" \
"	movq mm3, [" _strf(PADDR2B) "] #/* our pixel2 value */ \n" \
	V_PAVGB(mm0, mm2, mm2, ShiftMask) \
	V_PAVGB(mm1, mm3, mm3, ShiftMask) \
"	movq mm2, mm0			#/* another copy of our pixel1 value */ \n" \
"	movq mm3, mm1			#/* another copy of our pixel1 value */ \n" \
"	psubusb mm2, mm1 \n" \
"	psubusb mm3, mm0 \n" \
"	por mm2,mm3 \n" \
	V_PAVGB(mm0, mm1, mm3, ShiftMask)	/* avg of 2 pixels */ \
"	movq mm3, mm2			#/* another copy of our our weights */ \n" \
"	pxor	 mm1, mm1 \n" \
"	psubusb mm3, mm7			#/* nonzero where old weights lower, else 0 */ \n" \
"	pcmpeqb mm3, mm1			#/* now ff where new better, else 00	*/ \n" \
"	pcmpeqb mm1, mm3		#/* here ff where old better, else 00 */ \n" \
"	pand	mm0, mm3			#/* keep only better new pixels */ \n" \
"	pand  mm2, mm3			#/* and weights */ \n" \
"	pand	mm5, mm1			#/* keep only better old pixels */ \n" \
"	pand  mm7, mm1 \n" \
"	por	mm5, mm0			#/* and merge new & old vals */ \n" \
"	por	mm7, mm2 \n"


#define RESET_CHROMA " por mm7, UVMask \n"

#endif

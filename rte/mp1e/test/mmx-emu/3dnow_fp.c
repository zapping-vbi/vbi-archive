/*
 *  Emulation of the "3DNow!" floating point functions
 *
 *  Copyright (C) 1998 Oliver Delise <delise@online-club.de>
 *  Modified Michael H. Schimek, 2001.
 */

#include <stdio.h>
#include <math.h>
#include "mmx-emu.h"
#include "macros.h"

#define MMX_EMU_3dn_CON_MACRO(NAME, STYPE, DTYPE)			\
void NAME (void *src, void *dest) {					\
	DTYPE *d = (DTYPE *) dest;					\
	STYPE *s = (STYPE *) src;					\
									\
	d[0] = (DTYPE) s[0];						\
	d[1] = (DTYPE) s[1];						\
									\
	mmx_printf(#NAME " called\n");					\
}

// NB [syl]: does the C cast do exactly what is expected ?

MMX_EMU_3dn_CON_MACRO(pi2fd, int32_t, float)
MMX_EMU_3dn_CON_MACRO(pf2id, float, int32_t)

#define MMX_EMU_3dn_ARI_MACRO(NAME, SYMB, OPS, OPD)			\
void NAME (void *src, void *dest) {					\
	float *d = (float *) dest;					\
	float *s = (float *) src;					\
									\
	d[0] = OPD[0] SYMB OPS[0];					\
	d[1] = OPD[1] SYMB OPS[1];					\
									\
	mmx_printf(#NAME " called\n");					\
}

MMX_EMU_3dn_ARI_MACRO(pfadd,  +, s, d)
MMX_EMU_3dn_ARI_MACRO(pfmul,  *, s, d)
MMX_EMU_3dn_ARI_MACRO(pfsub,  -, s, d)
MMX_EMU_3dn_ARI_MACRO(pfsubr, -, d ,s)

#define MMX_EMU_3dn_ARI_minmax_MACRO(NAME, SYMB)			\
void NAME (void *src, void *dest) {					\
	float *d = (float *) dest;					\
	float *s = (float *) src;					\
									\
	if (s[0] SYMB d[0]) d[0] = s[0];				\
	if (s[1] SYMB d[1]) d[1] = s[1];				\
									\
	mmx_printf(#NAME " called\n");					\
}

MMX_EMU_3dn_ARI_minmax_MACRO(pfmin, <)
MMX_EMU_3dn_ARI_minmax_MACRO(pfmax, >)

#define MMX_EMU_3dn_ARI_integer_MACRO(NAME, TYPE, LOOP, OP, OFFSET, SHIFT) \
void NAME (void *src, void *dest) {					\
	TYPE *d = (TYPE *) dest;					\
	TYPE *s = (TYPE *) src;						\
	int i;								\
									\
	for (i=0; i < LOOP; i++){					\
		d[i] = ((d[i] OP s[i]) + OFFSET) >> SHIFT;		\
	}								\
									\
	mmx_printf(#NAME " called\n");					\
}

MMX_EMU_3dn_ARI_integer_MACRO(pfmulhrw, short, 4, *, 0x8000, 16)

void
pfacc(void *src, void *dest)
{
	float *d = (float *) dest;
	float *s = (float *) src;

	d[0] = d[0] + d[1];
	d[1] = s[0] + s[1];

	mmx_printf("pfacc called\n");
}

#define MMX_EMU_3dn_CMP_MACRO(NAME, SYMB)				\
void NAME (void *src, void *dest) {					\
	int  *dd = (int *) dest;					\
	float *d = (float *) dest;					\
	float *s = (float *) src;					\
									\
	dd[0] = (d[0] SYMB s[0]) ? -1 : 0;				\
	dd[1] = (d[1] SYMB s[1]) ? -1 : 0;				\
									\
	mmx_printf(#NAME " called\n");					\
}

MMX_EMU_3dn_CMP_MACRO(pfcmpeq, ==)
MMX_EMU_3dn_CMP_MACRO(pfcmpge, >=)
MMX_EMU_3dn_CMP_MACRO(pfcmpgt, >)

// Our emulated approximation is already the good result :-),
// so the Newton-Raphson steps are empty (only a check).
// Maybe better do pfrcpit1 = pfrcpit2 = pfrcp.
void
pfrcp(void *src, void *dest)
{
	float *s = (float *) src;
	float *d = (float *) dest;

	d[0] = 1/s[0];
	d[1] = 1/s[1];

	mmx_printf("pfrcp called\n");
}

void
pfrcpit1(void *src, void *dest)
{
	float *s = (float *) src;
	float *d = (float *) dest;

	if ((d[0] != 1/s[0]) || (d[1] != 1/s[1]))
		printf("mmx-emu: warning, maybe using pfrcpit1 "
			"not the way we thought.\n");

// NB: must we do dest = src ?
	mmx_printf("pfrcpit1 called\n");
}

void
pfrcpit2(void *src, void *dest)
{
	float *s = (float *) src;
	float *d = (float *) dest;

	if ((d[0] != 1/s[0]) || (d[1] != 1/s[1]))
		printf("mmx-emu: warning, maybe using pfrcpit2 "
			"not the way we thought.\n");
// NB: must we do dest = src ?

	mmx_printf("pfrcpit2 called\n");
}

void
pfrsqrt(void *src, void *dest)
{
	float *s = (float *) src;
	float *d = (float *) dest;

	d[0] = 1/sqrt((double) fabs(s[0]));
	if (s[0] < 0)  d[0] = -d[0];
	d[1] = d[0];

	mmx_printf("pfrsqrt called\n");
}

void
pfrsqit1(void *src, void *dest)
{
	float *s = (float *) src;
	float *d = (float *) dest;
	float test = 1/sqrt((double) fabs(s[0]));

	if (s[0] < 0)  test = -test;
	if ((d[0] != test) || (d[1] != test))
		printf("mmx-emu: warning, maybe using pfrsqit1 "
			"not the way we thought.\n");
// NB: must we do dest = src ?

	mmx_printf("pfrsqit1 called\n");
}

/* AMD Athlon extensions */

MMX_EMU_NI_MACRO(pi2fw)
MMX_EMU_NI_MACRO(pf2iw)
MMX_EMU_NI_MACRO(pfnacc)
MMX_EMU_NI_MACRO(pfpnacc)

/*
 *  Emulation of the Cyrix MMX functions
 *
 *  (C) <Vincent.Renardias@waw.com>, 1998, LGPL.
 *  Modified Michael H. Schimek, 2001.
 */

#include "mmx-emu.h"
#include "macros.h"

// Note the comment in Cyrix' doc: "M2 hardware versions before v1.3
// interpret values as signed bytes on this instruction".

MMX_EMU_UOP_MACRO(paveb, uint8_t, 8, ((int) d[i] + (int) s[i]) >> 1)

void pdistib (void *src, void *dest, sigcontext_t *context)
{
	uint8_t *impl = (uint8_t *) IMPL_REG(dest);
	uint8_t *d = (uint8_t *) dest;
	uint8_t *s = (uint8_t *) src;
	int i;

	for (i = 0; i < 8; i++)
		impl[i] = saturate_uint8_t((int) impl[i]
			+ nbabs((int) d[i] - (int) s[i]));
	DD;
}

MMX_EMU_UOP_MACRO(pmagw, int16_t, 4, (nbabs(s[i]) > nbabs(d[i])) ? s[i] : d[i])

MMX_EMU_UOP_MACRO(cyrix_pmulhrw, int16_t, 4, ((int) d[i] * s[i] + 0x4000L) >> 15)

#define MMX_EMU_CYRIX_MULT_MACRO(NAME,OPER)				\
void NAME (void *src, void *dest, sigcontext_t *context) {		\
	int16_t *impl = (int16_t *) IMPL_REG(dest);			\
	int16_t *d = (int16_t *) dest;					\
	int16_t *s = (int16_t *) src;					\
	int i;								\
									\
	for (i = 0; i < 4; i++)						\
		impl[i] OPER ((int) d[i]				\
			* (int) s[i] + 0x4000L) >> 15;			\
	DD;								\
}

MMX_EMU_CYRIX_MULT_MACRO(pmulhriw,  =)
MMX_EMU_CYRIX_MULT_MACRO(pmachriw, +=)

#define MMX_EMU_CYRIX_CMP_MACRO(NAME, SYMB)				\
void NAME (void *src, void *dest, sigcontext_t *context) {		\
	int8_t *impl = (int8_t *) IMPL_REG(dest);			\
	int8_t *d = (int8_t *) dest;					\
	int8_t *s = (int8_t *) src;					\
	int i;								\
									\
	for (i = 0; i < 8; i++)						\
		if (impl[i] SYMB 0)					\
			d[i] = s[i];					\
	DD;								\
}

MMX_EMU_CYRIX_CMP_MACRO(pmvzb, ==)
MMX_EMU_CYRIX_CMP_MACRO(pmvnzb, !=)
MMX_EMU_CYRIX_CMP_MACRO(pmvlzb, <)
MMX_EMU_CYRIX_CMP_MACRO(pmvgezb, >=)

#define MMX_EMU_CYRIX_SAT_MACRO(NAME, TYPE, LOOP, SYMB)			\
void NAME (void *src, void *dest, sigcontext_t *context) {		\
        TYPE *impl = (TYPE *) IMPL_REG(dest);				\
        TYPE *d = (TYPE *) dest;					\
        TYPE *s = (TYPE *) src;						\
        int i;								\
									\
        for (i=0; i < LOOP; i++)					\
		impl[i] = saturate_##TYPE((int) d[i] SYMB (int) s[i]);	\
	DD;								\
}

MMX_EMU_CYRIX_SAT_MACRO(paddsiw, int16_t, 4, +)
MMX_EMU_CYRIX_SAT_MACRO(psubsiw, int16_t, 4, -)

/*
 *  Emulation of the SSE integer functions
 *
 *  Copyright (C) 2001 Michael H. Schimek
 */

#include "mmx-emu.h"
#include "macros.h"

void
maskmovq(void *src, void *dest, sigcontext_t *context)
{
	uint8_t *dd = (uint8_t *) context->edi;
	uint8_t *s = (uint8_t *) src; /* mmx (reg) */
	int8_t *d = (int8_t *) dest; /* mmx mask (r/m) */
	int i;

	for (i = 0; i < 8; i++)
		if (d[i] < 0)
			dd[i] = s[i];
	DD;
}

MMX_EMU_STORE_MACRO(movntq, uint64_t, 1);

MMX_EMU_UOP_MACRO(pavgb, uint8_t, 8, ((int) d[i] + (int) s[i] + 1) >> 1)
MMX_EMU_UOP_MACRO(pavgw, uint16_t, 4, ((int) d[i] + (int) s[i] + 1) >> 1)

void
pextrw(void *src, void *dest, int count)
{
	uint32_t *d = (uint32_t *) dest; /* gp reg (r/m) */
	uint16_t *s = (uint16_t *) src; /* mmx (reg) */

	*d = s[count & 3];

	DD;
}

void
pinsrw(void *src, void *dest, int count)
{
	uint16_t *d = (uint16_t *) dest; /* mmx */
	uint16_t *s = (uint16_t *) src; /* gp reg or memory */

	d[count & 3] = *s;

	DD;
}

MMX_EMU_UOP_MACRO(pmaxsw, int16_t, 4, MAX(d[i], s[i]))
MMX_EMU_UOP_MACRO(pmaxub, uint8_t, 8, MAX(d[i], s[i]))
MMX_EMU_UOP_MACRO(pminsw, int16_t, 4, MIN(d[i], s[i]))
MMX_EMU_UOP_MACRO(pminub, uint8_t, 8, MIN(d[i], s[i]))

void
pmovmskb(void *src, void *dest)
{
	uint32_t *d = (uint32_t *) dest; /* gp reg (r/m) */
	uint8_t *s = (uint8_t *) src; /* mmx (reg) */
	int n = 0, i;

	for (i = 0; i < 8; i++)
		n |= (s[i] & 0x80) << i;

	*d = n >> 7;

	DD;
}

MMX_EMU_UOP_MACRO(pmulhuw, uint16_t, 4, ((unsigned int) d[i] * s[i]) >> 16)

void
psadbw(void *src, void *dest)
{
	uint8_t *d = (uint8_t *) dest;
	uint8_t *s = (uint8_t *) src;
	int n, m = 0, i;

	for (i = 0; i < 8; i++) {
		n = d[i] - s[i];
		m += (n < 0) ? -n : n;
	}

	* (uint64_t *) d = m;

	DD;
}

void
pshufw(void *src, void *dest, int order)
{
	uint16_t *d = (uint16_t *) dest;
	uint16_t *s = (uint16_t *) src;
	uint16_t t[4];
	int i;

	for (i = 0; i < 4; i++)
		t[i] = s[(order >> (i * 2)) & 3];

	* (uint64_t *) d = * (uint64_t *) &t;

	DD;
}

MMX_EMU_NOP_MACRO(prefetcht0)
MMX_EMU_NOP_MACRO(prefetcht1)
MMX_EMU_NOP_MACRO(prefetcht2)
MMX_EMU_NOP_MACRO(prefetchnta)

void
sse_prefetch(void *src, void *dest, int reg)
{
	switch (reg) {
	case 0:
		prefetchnta(src, dest);
		break;

	case 1:
		prefetcht0(src, dest);
		break;

	case 2:
		prefetcht1(src, dest);
		break;

	case 3:
		prefetcht2(src, dest);
		break;

	default:
		FAIL("invalid prefetch instruction %d", reg);
	}
}

void
grp0FAE(void *src, void *dest, int foo)
{
	FAIL("0FAEnn not implemented");
}

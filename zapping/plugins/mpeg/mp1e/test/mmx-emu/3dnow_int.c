/*
 *  Emulation of the "3DNow!" integer functions
 *
 *  (C) Oliver Delise <delise@online-club.de>, 1998, LGPL
 *  Copyright (C) 2001 Michael H. Schimek
 */

#include "mmx-emu.h"
#include "macros.h"

MMX_EMU_UOP_MACRO(pavgusb, uint8_t, 8, \
	((int) d[i] + (int) s[i] + 1) >> 1)

MMX_EMU_UOP_MACRO(_3dnow_pmulhrw, int16_t, 4, \
	((int) d[i] * s[i] + 0x8000L) >> 16)

void
pswapd(void *src, void *dest)
{
	uint32_t *d = (uint32_t *) dest;
	uint32_t *s = (uint32_t *) src;
	uint32_t temp;

	temp = s[0];
	d[0] = s[1];
	d[1] = temp;

	DD;
}

void
femms(void *src, void *dest, sigcontext_t *context)
{
	context->fpstate->tag = 0xffffffff;

	DD;
}

MMX_EMU_NOP_MACRO(_3dnow_prefetch)

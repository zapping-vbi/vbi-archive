/*
 *  Emulation of the stock MMX functions
 *
 *  Copyright (C) Vincent.Renardias@waw.com, 1998, LGPL.
 *  Copyright (C) Sylvain Pion, July 1998.
 *  Copyright (C) 2001 Michael H. Schimek
 */

#include "mmx-emu.h"
#include "macros.h"

MMX_EMU_STORE_MACRO(movd2mem, uint32_t, 1)
MMX_EMU_STORE_MACRO(movq2mem, uint64_t, 1)

MMX_EMU_UOP_MACRO(movd2mmx, uint32_t, 2, (i == 0) ? s[i] : 0)
MMX_EMU_UOP_MACRO(movq2mmx, uint64_t, 1, s[i])

MMX_EMU_UOP_MACRO(paddb, int8_t,  8, d[i] + s[i])
MMX_EMU_UOP_MACRO(paddw, int16_t, 4, d[i] + s[i])
MMX_EMU_UOP_MACRO(paddd, int32_t, 2, d[i] + s[i])

MMX_EMU_UOP_MACRO(psubb, int8_t,  8, d[i] - s[i])
MMX_EMU_UOP_MACRO(psubw, int16_t, 4, d[i] - s[i])
MMX_EMU_UOP_MACRO(psubd, int32_t, 2, d[i] - s[i])

MMX_EMU_SAT_MACRO(paddsb, int8_t, 8, +, int8_t)
MMX_EMU_SAT_MACRO(paddsw, int16_t, 4, +, int16_t)
MMX_EMU_SAT_MACRO(paddusb, uint8_t, 8, +, uint8_t)
MMX_EMU_SAT_MACRO(paddusw, uint16_t, 4, +, uint16_t)

MMX_EMU_SAT_MACRO(psubsb, int8_t, 8, -, int8_t)
MMX_EMU_SAT_MACRO(psubsw, int16_t, 4, -, int16_t)
MMX_EMU_SAT_MACRO(psubusb, uint8_t, 8, -, uint8_t)
MMX_EMU_SAT_MACRO(psubusw, uint16_t, 4, -, uint16_t)

MMX_EMU_UOP_MACRO(pmullw, int16_t, 4, ((int) d[i] * s[i]) & 0xFFFF)
MMX_EMU_UOP_MACRO(pmulhw, int16_t, 4, ((int) d[i] * s[i]) >> 16)

void
pmaddwd(void *src, void *dest) 
{
	int32_t *dd = (int32_t *) dest;
	int16_t *d = (int16_t *) dest;
	int16_t *s = (int16_t *) src;

	dd[0] = (int32_t) s[0] * (int32_t) d[0]
	      + (int32_t) s[1] * (int32_t) d[1];		
	dd[1] = (int32_t) s[2] * (int32_t) d[2]
	      + (int32_t) s[3] * (int32_t) d[3];		

	DD; 
}

MMX_EMU_UOP_MACRO(pand,  uint64_t, 1, d[i] & s[i])
MMX_EMU_UOP_MACRO(por,   uint64_t, 1, d[i] | s[i])
MMX_EMU_UOP_MACRO(pxor,  uint64_t, 1, d[i] ^ s[i])
MMX_EMU_UOP_MACRO(pandn, uint64_t, 1, (d[i] ^ (uint64_t) -1) & s[i])

MMX_EMU_UOP_MACRO(pcmpeqb, int8_t,  8, (d[i] == s[i]) ? -1 : 0)
MMX_EMU_UOP_MACRO(pcmpeqw, int16_t, 4, (d[i] == s[i]) ? -1 : 0)
MMX_EMU_UOP_MACRO(pcmpeqd, int32_t, 2, (d[i] == s[i]) ? -1 : 0)

MMX_EMU_UOP_MACRO(pcmpgtb, int8_t,  8, (d[i] > s[i]) ? -1 : 0)
MMX_EMU_UOP_MACRO(pcmpgtw, int16_t, 4, (d[i] > s[i]) ? -1 : 0)
MMX_EMU_UOP_MACRO(pcmpgtd, int32_t, 2, (d[i] > s[i]) ? -1 : 0)

MMX_EMU_SHIFT_MACRO(psllw, uint16_t, 4, <<, 0)
MMX_EMU_SHIFT_MACRO(pslld, uint32_t, 2, <<, 0)
MMX_EMU_SHIFT_MACRO(psllq, uint64_t, 1, <<, 0)

MMX_EMU_SHIFT_MACRO(psrlw, uint16_t, 4, >>, 0)
MMX_EMU_SHIFT_MACRO(psrld, uint32_t, 2, >>, 0)
MMX_EMU_SHIFT_MACRO(psrlq, uint64_t, 1, >>, 0)

MMX_EMU_SHIFT_MACRO(psraw, int16_t, 4, >>, d[i] >> 15)
MMX_EMU_SHIFT_MACRO(psrad, int32_t, 2, >>, d[i] >> 31)

void
pshimw(void *src, void *dest, int reg)
{
	switch (reg) {
	case 2:
		psrlw(src,dest);
		break;

	case 4:
		psraw(src,dest);
		break;

	case 6:
		psllw(src,dest);
		break;

	default:
		FAIL("invalid pshimw instruction %d", reg);
	}
}

void
pshimd(void *src, void *dest, int reg)
{
	switch (reg) {
	case 2:
		psrld(src,dest);
		break;

	case 4:
		psrad(src,dest);
		break;

	case 6:
		pslld(src,dest);
		break;

	default:
		FAIL("invalid pshimd instruction %d", reg);
	}
}

void
pshimq(void *src, void *dest, int reg)
{
	switch (reg) {
	case 2:
		psrlq(src,dest);
		break;

	case 6:
		psllq(src,dest);
		break;

	default:
		FAIL("invalid pshimq instruction %d", reg);
	}
}

MMX_EMU_UNPCKL_MACRO(punpcklbw, int8_t, 4)
MMX_EMU_UNPCKL_MACRO(punpcklwd, int16_t, 2)
MMX_EMU_UNPCKL_MACRO(punpckldq, int32_t, 1)

MMX_EMU_UNPCKH_MACRO(punpckhbw, int8_t, 4)
MMX_EMU_UNPCKH_MACRO(punpckhwd, int16_t, 2)
MMX_EMU_UNPCKH_MACRO(punpckhdq, int32_t, 1)

void
packsswb(void *src, void *dest)
{
	int16_t *dd = (int16_t *) dest;
	int16_t *ss = (int16_t *) src;
	int8_t  *d  = (int8_t *)  dest;
	int8_t  *s  = (int8_t *)  src;

/* I find this one more readable, but maybe it's slower.

	int i;

	for (i=0; i<4; i++) {
		if (dd[i] > 0x7f)       d[i] = 0x7f;
		else if (dd[i] < -0x80) d[i] = -0x80;
	}

	for (i=0; i<4; i++) {
		if (ss[i] > 0x7f)       d[i+4] = 0x7f;
		else if (ss[i] < -0x80) {
					d[i+4] = -0x80; }
		else 			d[i+4] = s[2*i];
	}
*/
	d[0] = ((dd[0]+0x80) & 0xff00) ? 0x80-!(d[1]>>8) : d[0];
	d[1] = ((dd[1]+0x80) & 0xff00) ? 0x80-!(d[3]>>8) : d[2];
	d[2] = ((dd[2]+0x80) & 0xff00) ? 0x80-!(d[5]>>8) : d[4];
	d[3] = ((dd[3]+0x80) & 0xff00) ? 0x80-!(d[7]>>8) : d[6]; 
	d[4] = ((ss[0]+0x80) & 0xff00) ? 0x80-!(s[1]>>8) : s[0];
	d[5] = ((ss[1]+0x80) & 0xff00) ? 0x80-!(s[3]>>8) : s[2];
	d[6] = ((ss[2]+0x80) & 0xff00) ? 0x80-!(s[5]>>8) : s[4];
	d[7] = ((ss[3]+0x80) & 0xff00) ? 0x80-!(s[7]>>8) : s[6]; 

	DD;
}

void
packuswb(void *src, void *dest)
{ 
	int8_t *d = (int8_t *) dest;
	int8_t *s = (int8_t *) src;
	
	d[0] = (d[1]) ? -!(d[1]>>8) : d[0];
	d[1] = (d[3]) ? -!(d[3]>>8) : d[2];
	d[2] = (d[5]) ? -!(d[5]>>8) : d[4];
	d[3] = (d[7]) ? -!(d[7]>>8) : d[6];
	d[4] = (s[1]) ? -!(s[1]>>8) : s[0];
	d[5] = (s[3]) ? -!(s[3]>>8) : s[2];
	d[6] = (s[5]) ? -!(s[5]>>8) : s[4];
	d[7] = (s[7]) ? -!(s[7]>>8) : s[6];

	DD;
}

void
packssdw(void *src, void *dest)
{
	int32_t *dd = (int32_t *) dest;
	int32_t *ss = (int32_t *) src;
	int16_t *d  = (int16_t *) dest;
	int16_t *s  = (int16_t *) src;

	d[0] = ((dd[0]+0x8000) & 0xffff0000) ? 0x8000-!(d[1]>>16) : d[0];
	d[1] = ((dd[1]+0x8000) & 0xffff0000) ? 0x8000-!(d[3]>>16) : d[2];
	d[2] = ((ss[0]+0x8000) & 0xffff0000) ? 0x8000-!(s[1]>>16) : s[0];
	d[3] = ((ss[1]+0x8000) & 0xffff0000) ? 0x8000-!(s[3]>>16) : s[2];

	DD;
}

void
emms(void *src, void *dest, sigcontext_t *context)
{
	context->fpstate->tag = 0xffffffff;

	DD;
}

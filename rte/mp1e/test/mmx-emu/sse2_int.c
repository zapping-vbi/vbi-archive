/*
 *  Emulation of the SSE2 integer functions
 *
 *  Copyright (C) 2001 Michael H. Schimek
 */

#include "mmx-emu.h"
#include "macros.h"

/* Stock MMX functions extended to 128 bit */

/* movd xmm2mem same as movd mmx2mem */
/* movq xmm2mem same as movq mmx2mem */

MMX_EMU_UOP_MACRO(movd2xmm, uint32_t, 4, (i == 0) ? s[i] : 0)
MMX_EMU_UOP_MACRO(movq2xmm, uint64_t, 2, (i == 0) ? s[i] : 0)

MMX_EMU_UOP_MACRO(xpaddb, int8_t, 16, d[i] + s[i])
MMX_EMU_UOP_MACRO(xpaddw, int16_t, 8, d[i] + s[i])
MMX_EMU_UOP_MACRO(xpaddd, int32_t, 4, d[i] + s[i])

MMX_EMU_UOP_MACRO(xpsubb, int8_t, 16, d[i] - s[i])
MMX_EMU_UOP_MACRO(xpsubw, int16_t, 8, d[i] - s[i])
MMX_EMU_UOP_MACRO(xpsubd, int32_t, 4, d[i] - s[i])

MMX_EMU_SAT_MACRO(xpaddsb, int8_t, 16, +, int8_t)
MMX_EMU_SAT_MACRO(xpaddsw, int16_t, 8, +, int16_t)
MMX_EMU_SAT_MACRO(xpaddusb, uint8_t, 16, +, uint8_t)
MMX_EMU_SAT_MACRO(xpaddusw, uint16_t, 8, +, uint16_t)

MMX_EMU_SAT_MACRO(xpsubsb, int8_t, 16, -, int8_t)
MMX_EMU_SAT_MACRO(xpsubsw, int16_t, 8, -, int16_t)
MMX_EMU_SAT_MACRO(xpsubusb, uint8_t, 16, -, uint8_t)
MMX_EMU_SAT_MACRO(xpsubusw, uint16_t, 8, -, uint16_t)

MMX_EMU_UOP_MACRO(xpmullw, int16_t, 8, ((int) d[i] * s[i]) & 0xFFFF)
MMX_EMU_UOP_MACRO(xpmulhw, int16_t, 8, ((int) d[i] * s[i]) >> 16)

void
xpmaddwd(void *src, void *dest) 
{
	int32_t *dd = (int32_t *) dest;
	int16_t *d = (int16_t *) dest;
	int16_t *s = (int16_t *) src;

	dd[0] = (int32_t) s[0] * (int32_t) d[0]
	      + (int32_t) s[1] * (int32_t) d[1];
	dd[1] = (int32_t) s[2] * (int32_t) d[2]
	      + (int32_t) s[3] * (int32_t) d[3];
	dd[2] = (int32_t) s[4] * (int32_t) d[4]
	      + (int32_t) s[5] * (int32_t) d[5];
	dd[3] = (int32_t) s[6] * (int32_t) d[6]
	      + (int32_t) s[7] * (int32_t) d[7];

	DD; 
}

MMX_EMU_UOP_MACRO(xpand,  uint64_t, 2, d[i] & s[i])
MMX_EMU_UOP_MACRO(xpor,   uint64_t, 2, d[i] | s[i])
MMX_EMU_UOP_MACRO(xpxor,  uint64_t, 2, d[i] ^ s[i])
MMX_EMU_UOP_MACRO(xpandn, uint64_t, 2, (d[i] ^ (uint64_t) -1) & s[i])

MMX_EMU_UOP_MACRO(xpcmpeqb, int8_t, 16, (d[i] == s[i]) ? -1 : 0)
MMX_EMU_UOP_MACRO(xpcmpeqw, int16_t, 8, (d[i] == s[i]) ? -1 : 0)
MMX_EMU_UOP_MACRO(xpcmpeqd, int32_t, 4, (d[i] == s[i]) ? -1 : 0)

MMX_EMU_UOP_MACRO(xpcmpgtb, int8_t, 16, (d[i] > s[i]) ? -1 : 0)
MMX_EMU_UOP_MACRO(xpcmpgtw, int16_t, 8, (d[i] > s[i]) ? -1 : 0)
MMX_EMU_UOP_MACRO(xpcmpgtd, int32_t, 4, (d[i] > s[i]) ? -1 : 0)

MMX_EMU_SHIFT_MACRO(xpsllw, uint16_t, 8, <<, 0)
MMX_EMU_SHIFT_MACRO(xpslld, uint32_t, 4, <<, 0)
MMX_EMU_SHIFT_MACRO(xpsllq, uint64_t, 2, <<, 0)

MMX_EMU_SHIFT_MACRO(xpsrlw, uint16_t, 8, >>, 0)
MMX_EMU_SHIFT_MACRO(xpsrld, uint32_t, 4, >>, 0)
MMX_EMU_SHIFT_MACRO(xpsrlq, uint64_t, 2, >>, 0)

MMX_EMU_SHIFT_MACRO(xpsraw, int16_t, 8, >>, d[i] >> 15)
MMX_EMU_SHIFT_MACRO(xpsrad, int32_t, 4, >>, d[i] >> 31)

void
xpshimw(void *src, void *dest, int reg)
{
	switch (reg) {
	case 2:
		xpsrlw(src,dest);
		break;

	case 4:
		xpsraw(src,dest);
		break;

	case 6:
		xpsllw(src,dest);
		break;

	default:
		FAIL("invalid xpshimw instruction %d", reg);
	}
}

void
xpshimd(void *src, void *dest, int reg)
{
	switch (reg) {
	case 2:
		xpsrld(src,dest);
		break;

	case 4:
		xpsrad(src,dest);
		break;

	case 6:
		xpslld(src,dest);
		break;

	default:
		FAIL("invalid xpshimd instruction %d", reg);
	}
}

void
xpshimq(void *src, void *dest, int reg)
{
	switch (reg) {
	case 2:
		xpsrlq(src,dest);
		break;

	case 6:
		xpsllq(src,dest);
		break;

	default:
		FAIL("invalid xpshimq instruction %d", reg);
	}
}

/* NB. 64 to 128, not upper/lower qword */

MMX_EMU_UNPCKL_MACRO(xpunpcklbw, int8_t, 8)
MMX_EMU_UNPCKL_MACRO(xpunpcklwd, int16_t, 4)
MMX_EMU_UNPCKL_MACRO(xpunpckldq, int32_t, 2)

MMX_EMU_UNPCKH_MACRO(xpunpckhbw, int8_t, 8)
MMX_EMU_UNPCKH_MACRO(xpunpckhwd, int16_t, 4)
MMX_EMU_UNPCKH_MACRO(xpunpckhdq, int32_t, 2)

void
xpacksswb(void *src, void *dest)
{
	int16_t *dd = (int16_t *) dest;
	int16_t *ss = (int16_t *) src;
	int8_t  *d  = (int8_t *)  dest;
	int8_t  *s  = (int8_t *)  src;
	int i;

	for (i = 0; i < 8; i++)
		d[i]  = ((dd[i] + 0x80) & 0xff00) ?
			0x80 - !(d[i * 2 + 1] >> 8) : d[i * 2 + 0];
	for (i = 0; i < 8; i++)
		d[i + 8]  = ((ss[i] + 0x80) & 0xff00) ?
			0x80 - !(s[i * 2 + 1] >> 8) : s[i * 2 + 0];
	DD;
}

void
xpackuswb(void *src, void *dest)
{ 
	int8_t *d = (int8_t *) dest;
	int8_t *s = (int8_t *) src;
	int i;

	for (i = 0; i < 8; i++)
		d[i] = (d[i * 2 + 1]) ?
			-!(d[i * 2 + 1] >> 8) : d[i * 2];
	for (i = 0; i < 8; i++)
		d[i + 8] = (s[i * 2 + 1]) ?
			-!(s[i * 2 + 1] >> 8) : s[i * 2];
	DD;
}

void
xpackssdw(void *src, void *dest)
{
	int32_t *dd = (int32_t *) dest;
	int32_t *ss = (int32_t *) src;
	int16_t *d  = (int16_t *) dest;
	int16_t *s  = (int16_t *) src;

	d[0] = ((dd[0]+0x8000) & 0xffff0000) ? 0x8000-!(d[1]>>16) : d[0];
	d[1] = ((dd[1]+0x8000) & 0xffff0000) ? 0x8000-!(d[3]>>16) : d[2];
	d[2] = ((dd[2]+0x8000) & 0xffff0000) ? 0x8000-!(d[5]>>16) : d[4];
	d[3] = ((dd[3]+0x8000) & 0xffff0000) ? 0x8000-!(d[7]>>16) : d[6];
	d[4] = ((ss[0]+0x8000) & 0xffff0000) ? 0x8000-!(s[1]>>16) : s[0];
	d[5] = ((ss[1]+0x8000) & 0xffff0000) ? 0x8000-!(s[3]>>16) : s[2];
	d[6] = ((ss[2]+0x8000) & 0xffff0000) ? 0x8000-!(s[5]>>16) : s[4];
	d[7] = ((ss[3]+0x8000) & 0xffff0000) ? 0x8000-!(s[7]>>16) : s[6];

	DD;
}

/* SSE integer functions extended to 128 bit */

void
maskmovdqu(void *src, void *dest, sigcontext_t *context)
{
	uint8_t *dd = (uint8_t *) context->edi;
	uint8_t *s = (uint8_t *) src; /* xmm (reg) */
	int8_t *d = (int8_t *) dest; /* xmm mask (r/m) */
	int i;

	for (i = 0; i < 16; i++)
		if (d[i] < 0)
			dd[i] = s[i];
	DD;
}

MMX_EMU_STORE_MACRO(movntdq, uint64_t, 2);

MMX_EMU_UOP_MACRO(xpavgb, uint8_t, 16, ((int) d[i] + (int) s[i] + 1) >> 1)
MMX_EMU_UOP_MACRO(xpavgw, uint16_t, 8, ((int) d[i] + (int) s[i] + 1) >> 1)

void
xpextrw(void *src, void *dest, int count)
{
	uint32_t *d = (uint32_t *) dest; /* gp reg (r/m) */
	uint16_t *s = (uint16_t *) src; /* xmm (reg) */

	*d = s[count & 7];

	DD;
}

void
xpinsrw(void *src, void *dest, int count)
{
	uint16_t *d = (uint16_t *) dest; /* xmm */
	uint16_t *s = (uint16_t *) src; /* gp reg or memory */

	d[count & 7] = *s;

	DD;
}

MMX_EMU_UOP_MACRO(xpmaxsw, int16_t,  8, MAX(d[i], s[i]))
MMX_EMU_UOP_MACRO(xpmaxub, uint8_t, 16, MAX(d[i], s[i]))
MMX_EMU_UOP_MACRO(xpminsw, int16_t,  8, MIN(d[i], s[i]))
MMX_EMU_UOP_MACRO(xpminub, uint8_t, 16, MIN(d[i], s[i]))

void
xpmovmskb(void *src, void *dest)
{
	uint32_t *d = (uint32_t *) dest; /* gp reg (r/m) */
	uint8_t *s = (uint8_t *) src; /* xmm (reg) */
	int n = 0, i;

	for (i = 0; i < 16; i++)
		n |= (s[i] & 0x80) << i;

	*d = n >> 7;

	DD;
}

MMX_EMU_UOP_MACRO(xpmulhuw, uint16_t, 4, ((unsigned int) d[i] * s[i]) >> 16)

void
xpsadbw(void *src, void *dest)
{
	uint8_t *d = (uint8_t *) dest;
	uint8_t *s = (uint8_t *) src;
	int n, m, i;

	for (i = m = 0; i < 8; i++) {
		n = d[i + 0] - s[i + 0];
		m += (n < 0) ? -n : n;
	}

	((uint64_t *) d)[0] = m;

	for (i = m = 0; i < 8; i++) {
		n = d[i + 8] - s[i + 8];
		m += (n < 0) ? -n : n;
	}

	((uint64_t *) d)[1] = m;

	DD;
}

/* SSE2 integer functions */

MMX_EMU_NI_MACRO(paddq)
MMX_EMU_NI_MACRO(psubq)
MMX_EMU_NI_MACRO(xpaddq)
MMX_EMU_NI_MACRO(xpsubq)
MMX_EMU_NI_MACRO(pmuludq)
MMX_EMU_NI_MACRO(pshuflw)
MMX_EMU_NI_MACRO(pshufhw)
MMX_EMU_NI_MACRO(pshufd)
MMX_EMU_NI_MACRO(pslldq)
MMX_EMU_NI_MACRO(psrldq)
MMX_EMU_NI_MACRO(punpckhqdq)
MMX_EMU_NI_MACRO(punpcklqdq)
MMX_EMU_NI_MACRO(movmm2dq)
MMX_EMU_NI_MACRO(movdq2mm)

/*
 *  Opcode and operand decoding.
 *
 *  Copyright (C) 1998 Sylvain Pion
 *  Copyright (C) 2001 Michael H. Schimek
 */

#include "mmx-emu.h"

#include "mmx.h"
#include "cyrix.h"
#include "3dnow_fp.h"
#include "3dnow_int.h"
#include "sse_fp.h"
#include "sse_int.h"
#include "sse2_fp.h"
#include "sse2_int.h"

// FIXME: Those global vars are bad if we emulate a multi-threaded program...
// {mhs} two gone, one added :-/
// int pshi_diff; // Used temporarily to differentiate pshi*.
// sigcontext_t * context;
static unsigned char xmm_reg[8][16]; /* should use SSE context where available */


/* Two-byte opcodes 0x0Fnn */

static ifunc *
instr_0F[256] = {
/* 0 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 /* ud2 */,
		0,	    _3dnow_prefetch,	femms,		(void *) -1 /* 3dnow "prefix" */,
/* 1 */		movups,		movups,		movlps2mem,	movlps2xmm,
		unpcklps,	unpckhps,	movhps2mem,	movhps2xmm,
		sse_prefetch,	0,		0,		0,
		0,		0,		0,		0,
/* 2 */		0, 0, 0, 0, 0, 0, 0, 0,
		movaps2mem,	movaps2xmm,	cvtpi2ps,	movntps,
		cvttps2pi,	cvtps2pi,	ucomiss,	comiss,
/* 3 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 4 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 5 */		movmskps,	sqrtps,		rsqrtps,	rcpps,
		andps,		andnps,		orps,		xorps,
		addps,		mulps,		0,		0,
		subps,		minps,		divps,		maxps,
/* 6 */		punpcklbw,	punpcklwd,	punpckldq,	packsswb,
		pcmpgtb,	pcmpgtw,	pcmpgtd,	packuswb,
		punpckhbw,	punpckhwd,	punpckhdq,	packssdw,
		0,		0,		movd2mmx,	movq2mmx,
/* 7 */		pshufw,		pshimw,		pshimd,		pshimq,
		pcmpeqb,	pcmpeqw,	pcmpeqd,	emms,
		0,		0,		0,		0,
		0,		0,		movd2mem,	movq2mem,
/* 8 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 9 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* A */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,		0,		grp0FAE,	0,
/* B */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* C */		0,		0,		cmpps,		0,
		pinsrw,		pextrw,		shufps,		0,
		0, 0, 0, 0, 0, 0, 0, 0,
/* D */		0,		psrlw,		psrld,		psrlq,
		0,		pmullw,		0,		pmovmskb,
		psubusb,	psubusw,	pminub,		pand,
		paddusb,	paddusw,	pmaxub,		pandn,
/* E */		pavgb,		psraw,		psrad,		pavgw,
		pmulhuw,	pmulhw,		0,		movntq,
		psubsb,		psubsw,		pminsw,		por,
		paddsb,		paddsw,		pmaxsw,		pxor,
/* F */		0,		psllw,		pslld,		psllq,
		0,		pmaddwd,	psadbw,		maskmovq,
		psubb,		psubw,		psubd,		psubq,
		paddb,		paddw,		paddd,		paddq
};

/* Cyrix MMX extensions 0x0F5n */

static ifunc *
instr_cyrix[16] = {
		paveb,		paddsiw,	pmagw,		0,
		pdistib,	psubsiw,	0,		0,
		pmvzb,		cyrix_pmulhrw,	pmvnzb,		pmvlzb,
		pmvgezb,	pmulhriw,	pmachriw,	0
};

/* "3DNow!" extensions 0x0F0F/.../nn */

static ifunc *
instr_0F0F[256] = {
/* 0 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		pi2fw,		pi2fd,		0,		0,
/* 1 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		pf2iw,		pf2id,		0,		0,
/* 2 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 3 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 4 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 5 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 6 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 7 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 8 */		0, 0, 0, 0, 0, 0, 0, 0,
		0,		0,		pfnacc,		0,
		0,		0,		pfpnacc,	0,
/* 9 */		0,		0,		0,		0,
		pfmin,		0,		pfrcp,		pfrsqrt,
		0,		pfcmpge,	pfsub,		0,
		0,		0,		pfadd,		0,
/* A */		0,		0,		0,		0,
		pfmax,		0,		pfrcpit1,	pfrsqit1,
		0,		pfcmpgt,	pfsubr,		0,
		0,		0,		pfacc,		0,
/* B */		0,		0,		0,		0,
		pfmul,		0,		pfrcpit2,	_3dnow_pmulhrw,
		0,		pfcmpeq,	0,		pswapd,
		0,		0,		0,		pavgusb,
/* C */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* D */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* E */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* F */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/* SSE2 extensions 0x660Fnn (operand size prefix, mmx -> xmm) */

static ifunc *
instr_660F[256] = {
/* 0 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 1 */		movupd,		0,		movlpd2mem,	movlpd2xmm,
		unpcklpd,	unpckhpd,	movhpd2mem,	movhpd2xmm,
		0, 0, 0, 0, 0, 0, 0, 0,
/* 2 */		0, 0, 0, 0,
		0,		0,		cvtpi2pd,	0,
		movapd2mem,	movapd2xmm,	0,		movntpd,
		cvttpd2pi,	cvtpd2pi,	ucomisd,	comisd,
/* 3 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 4 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 5 */		movmskpd,	sqrtpd,		0,		0,
		andpd,		andnpd,		orpd,		xorpd,
		addpd,		mulpd,		cvtpd2ps,	cvtps2dq,
		subpd,		minpd,		divpd,		maxpd,
/* 6 */		xpunpcklbw,	xpunpcklwd,	xpunpckldq,	xpacksswb,
		xpcmpgtb,	xpcmpgtw,	xpcmpgtd,	xpackuswb,
		xpunpckhbw,	xpunpckhwd,	xpunpckhdq,	xpackssdw,
		punpcklqdq,	punpckhqdq,	movd2xmm,	movq2xmm,
/* 7 */		pshufd,		xpshimw,	xpshimd,	xpshimq,
		xpcmpeqb,	xpcmpeqw,	xpcmpeqd,	emms,
		0,		0,		0,		0,
		0,		0,		movd2mem,	movq2mem,
/* 8 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 9 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* A */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* B */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* C */		0,		0,		cmppd,		0,
		xpinsrw,	xpextrw,	shufpd,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
/* D */		0,		xpsrlw,		xpsrld,		xpsrlq,
		0,		xpmullw,	0,		xpmovmskb,
		xpsubusb,	xpsubusw,	xpminub,	xpand,
		xpaddusb,	xpaddusw,	xpmaxub,	xpandn,
/* E */		xpavgb,		xpsraw,		xpsrad,		xpavgw,
		xpmulhuw,	xpmulhw,	cvttpd2dq,	movntdq,
		xpsubsb,	xpsubsw,	xpminsw,	xpor,
		xpaddsb,	xpaddsw,	xpmaxsw,	xpxor,
/* F */		0,		xpsllw,		xpslld,		xpsllq,
		pmuludq,	xpmaddwd,	xpsadbw,	maskmovdqu,
		xpsubb,		xpsubw,		xpsubd,		xpsubq,
		xpaddb,		xpaddw,		xpaddd,		xpaddq
};

/* SSE2 extensions 0xF20Fnn (REPNE prefix, scalar double) */

static ifunc *
instr_F20F[256] = {
/* 0 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 1 */		movsd,		movsd,		0,		0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 2 */		0, 0, 0, 0, 0, 0, 0, 0,
		0,		0,		cvtsi2sd,	0,
		cvttsd2si,	cvtsd2si,	0,		0,
/* 3 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 4 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 5 */		0,		sqrtsd,		0,		0,
		0,		0,		0,		0,
		addsd,		mulsd,		cvtsd2ss,	0,
		subsd,		minsd,		divsd,		maxsd,
/* 6 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 7 */		pshuflw,	0,		0,		0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 8 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 9 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* A */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* B */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* C */		0,		0,		cmpsd,		0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* D */		0,		0,		0,		0,
		0,		0,		movdq2q,	0,
		0, 0, 0, 0, 0, 0, 0, 0,
/* E */		0,		0,		0,		0,
		0,		0,		cvtpd2dq,	0,
		0, 0, 0, 0, 0, 0, 0, 0,
/* F */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/* SSE2 extensions 0xF30Fnn (REPE prefix, scalar single) */

/* F390 PAUSE */

static ifunc *
instr_F30F[256] = {
/* 0 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 1 */		movss2mem,	0,		0,		0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 2 */		0, 0, 0, 0, 0, 0, 0, 0,
		0,		0,		cvtsi2ss,	0,
		cvttss2si,	cvtss2si,	0,		0,
/* 3 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 4 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 5 */		0,		rcpss,		rsqrtss,	0,
		0,		0,		0,		0,
		addss,		mulss,		cvtss2sd,	cvttps2dq,
		subss,		minss,		divss,		maxss,
/* 6 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,		0,		0,		movdqu2xmm,
/* 7 */		pshufhw,	0,		0,		0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0,		0,		0,		movdqu2mem,
/* 8 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 9 */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* A */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* B */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* C */		0,		0,		cmpss,		0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* D */		0,		0,		0,		0,
		0,		0,		movq2dq,	0,
		0, 0, 0, 0, 0, 0, 0, 0,
/* E */		0,		0,		0,		0,
		0,		0,		cvtdq2pd,	0,
		0, 0, 0, 0, 0, 0, 0, 0,
/* F */		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

void
mmx_emu_ni(void)
{
	FAIL("instruction not implemented");
}

/* TODO */

void
mmx_emu_configure(cpu_class cpu)
{
	int sse2 = 0, sse_fp = 0, sse_int = 0, mmx = 0;
	int _3dnow = 0, _3dnow_amd = 0, cyrix = 0, cmov = 0;

	switch (cpu) {
	case CPU_PENTIUM_4:
		sse2 = 1;

	case CPU_PENTIUM_III:
		sse_fp = 1;
		sse_int = 1;

	case CPU_PENTIUM_II:
		cmov = 1;

	case CPU_PENTIUM_MMX:
		mmx = 1;
		break;

	case CPU_ATHLON:
		_3dnow_amd = 1;
		sse_int = 1;
		cmov = 1;

	case CPU_K6_2:
		_3dnow = 1;
		mmx = 1;
		break;

	case CPU_CYRIX_III:
		_3dnow = 1;
		cyrix = 1;

	case CPU_CYRIX_MII:
		cmov = 1;
		mmx = 1;
		break;

	default:
		FAIL("unknown cpu class %d", cpu);
	}

	if (!sse2) {
		memset(instr_660F, 0, sizeof(instr_660F));
		memset(instr_F20F, 0, sizeof(instr_F20F));
		memset(instr_F30F, 0, sizeof(instr_F30F));
	}
	
	if (!sse_fp) {
		memset(&instr_0F0F[0x10], 0, sizeof(instr_0F0F[0]) * 0x10);
		memset(&instr_0F0F[0x20], 0, sizeof(instr_0F0F[0]) * 0x10);
		memset(&instr_0F0F[0x50], 0, sizeof(instr_0F0F[0]) * 0x10);
		instr_0F[0xC2] = 0; /* cmpss */
		instr_0F[0xC6] = 0; /* shufps */
	}

	if (!sse_int) {
		instr_0F[0x18] = 0; /* sse_prefetch */
		instr_0F[0x70] = 0; /* pshufw */
		instr_0F[0xAE] = 0; /* grp0FAE */
		instr_0F[0xC4] = 0; /* pinsrw */
		instr_0F[0xC5] = 0; /* pextrw */
		instr_0F[0xD7] = 0; /* pmovmskb */
		instr_0F[0xDA] = 0; /* pminub */
		instr_0F[0xDE] = 0; /* pmaxub */
		instr_0F[0xE0] = 0; /* pavgb */
		instr_0F[0xE3] = 0; /* pavgw */
		instr_0F[0xE4] = 0; /* pmulhuw */
		instr_0F[0xE7] = 0; /* movntq */
		instr_0F[0xEA] = 0; /* pminsw */
		instr_0F[0xEE] = 0; /* pmaxsw */
		instr_0F[0xF6] = 0; /* psadbw */
		instr_0F[0xF7] = 0; /* maskmovq */
		instr_0F[0xFB] = 0; /* psubq */
		instr_0F[0xFF] = 0; /* paddq */
	}

	if (!_3dnow_amd) {
		instr_0F0F[0x0C] = 0; /* pi2fw */
		instr_0F0F[0x1C] = 0; /* pf2iw */
		instr_0F0F[0x8A] = 0; /* pfnacc */
		instr_0F0F[0x8E] = 0; /* pfpnacc */
		instr_0F0F[0xBB] = 0; /* pswapd */
	}

	if (!_3dnow) {
		memset(instr_0F0F, 0, sizeof(instr_0F0F));
		instr_0F[0x0D] = 0; /* _3dnow_prefetch */
		instr_0F[0x0E] = 0; /* femms */
		instr_0F[0x0F] = 0; /* 3dnow "prefix" */
	}

	if (cyrix) {
		memcpy(&instr_0F[0x50], instr_cyrix, sizeof(instr_cyrix));
	}

	if (!mmx) {
		memset(instr_0F, 0, sizeof(instr_0F));
	}
}

static void
print_context(sigcontext_t *context)
{
	int i, j;

	mmx_printf("\n\ncontext      = %p\n", context);

	if (1) {
		for (i = 0; i < sizeof(* context) / 4; i++)
			mmx_printf("%08lx ", ((long *) context)[i]);

		mmx_printf("\n");
	}

	/* FPU/MMX registers */

	for (i = 0; i < 8; i++) {
		mmx_printf("mm%d = ", i);

		for (j = 3; j >= 0; j--)
			mmx_printf("%04x", MMX_REG(i)->significand[j]);

		mmx_printf(" %04x", MMX_REG(i)->exponent);

		mmx_printf((i & 1) ? "\n" : "\t");
	}

	/* Opcodes, byte by byte */

	mmx_printf("EIP = %p (code:", (uint8_t *) context->eip);

	for (i = 0; i < 8; i++)
		mmx_printf(" %02x", ((uint8_t *) context->eip)[i]);

	mmx_printf(")\n");
}

static inline void *
disp8(uint8_t *addr, uint8_t **eipp)
{
	return addr + *(* (int8_t **) eipp)++;
}

static inline void *
disp32(uint8_t *addr, uint8_t **eipp)
{
	return addr + *(* (int32_t **) eipp)++;
}

static void *
sib(int mod, uint8_t **eipp, sigcontext_t *context)
{
	int ss, index, base;
	uint8_t *addr;

	base = *(* eipp)++;

	ss = base >> 6;
	index = (base >> 3) & 7;
	base &= 7;

	mmx_printf("sib base = %d  index = %d  ss = %d\n", base, index, ss);

	if (mod == 0 && base == 5)
		addr = 0; /* no base register */
	else
		addr = * (void **) GP_REG(base);

	if (index == 4) {
		if (ss)
			FAIL("invalid sib byte 0x%02x",
				base + (index << 3) + (ss << 6));
		/* else no index */
	} else
		addr += (* (uint32_t *) GP_REG(index)) << ss;

	if (mod == 1)
		addr = disp8(addr, eipp);
	else if (mod == 2 || base == 5)
		addr = disp32(addr, eipp);

	mmx_printf("sib addr = %p\n", addr);

	return addr;
}

static inline void *
modrm(void **srcp, void **destp, ifunc *instr, int opcode, int prefix,
	uint64_t *imm8p, uint8_t **eipp, sigcontext_t *context)
{
	void *hint = context;
	int mod, reg, rm;

	rm = *(* eipp)++;

	mod = (rm >> 6) & 3;
	reg = (rm >> 3) & 7;	/* "dest" */
	rm &= 7;		/* "src" */

	mmx_printf("mod = %d  reg = %d  X=  rm = %d\n", mod, reg, rm);

	*destp = prefix ? (void *) &xmm_reg[reg] : MMX_REG(reg);

	switch (mod) {
	case 0:
		if (rm == 4)
			*srcp = sib(mod, eipp, context);
		else if (rm == 5)
			*srcp = disp32(0, eipp);
		else
			*srcp = * (void **) GP_REG(rm);
		break;

	case 1:
		if (rm == 4)
			*srcp = sib(mod, eipp, context);
		else
			*srcp = disp8(* (void **) GP_REG(rm), eipp);
		break;

	case 2:
		if (rm == 4)
			*srcp = sib(mod, eipp, context);
		else
			*srcp = disp32(* (void **) GP_REG(rm), eipp);
		break;

	case 3:
		if (instr == pshimw ||
		    instr == pshimd ||
		    instr == pshimq) {
			*imm8p = *(* eipp)++;
			*srcp = imm8p;
			*destp = prefix ? (void *) &xmm_reg[rm] : MMX_REG(rm);
			hint = (void *) reg;
		} else if (instr == pinsrw) {
			*srcp = GP_REG(reg);
			*destp = prefix ? (void *) &xmm_reg[rm] : MMX_REG(rm);
			hint = (void *)(int) *(* eipp)++; /* imm8 */
		} else if (instr == pextrw) {
			*destp  = GP_REG(rm);
			*srcp = prefix ? (void *) &xmm_reg[reg] : MMX_REG(reg);
			hint = (void *)(int) *(* eipp)++; /* imm8 */
		} else if (opcode == 0x5F) {
			FAIL("sysenter/sysexit n/i");
		} else {
			*srcp = prefix ? (void *) &xmm_reg[rm] : MMX_REG(rm);
		}

		break;

	default:
		FAIL("invalid modr/m byte 0x%02x",
			(mod << 6) + (reg << 3) + rm);
	}

	if (instr == sse_prefetch || instr == _3dnow_prefetch) {
		if (mod == 3)
			FAIL("invalid modr/m byte 0x%02x",
				(mod << 6) + (reg << 3) + rm);

		hint = (void *) reg;
	} else if (instr == pshufw ||
		   instr == shufps ||
		   instr == pshuflw ||
		   instr == pshufhw ||
		   instr == pshufd ||
		   instr == cmpps ||
		   instr == cmpss) {
		hint = (void *)(int) *(* eipp)++; /* imm8 */
	}

	return hint;
}

int
mmx_emu_decode(sigcontext_t *context)
{
	int prefix, opcode;
	uint8_t *eip = (uint8_t *) context->eip;
	void *src = NULL, *dest = NULL;
	void *hint = context;
	ifunc *instr;
	uint64_t imm8;

	print_context(context);

	prefix = 0;

	switch (opcode = *eip++) {
	case 0x66: /* operand size / mmx -> xmm */
	case 0xF2: /* REPNE / scalar double */
	case 0xF3: /* REPE / scalar single */
		prefix = opcode;
		opcode = *eip++;
		break;

	case 0x2E: /* CS override / branch hint */
	case 0x3E: /* DS override / branch hint */
	case 0x36: /* SS override */
	case 0x64: /* FS override */
	case 0x65: /* GS override */
	case 0x67: /* address size */
	case 0xF0: /* LOCK */
		return 0;

	default:
	}

	if (opcode != 0x0F)
		return 0;

	opcode = *eip++;

	switch (prefix) {
	case 0x66:
		instr = instr_660F[opcode];
		break;

	case 0xF2:
		instr = instr_F20F[opcode];
		break;

	case 0xF3:
		instr = instr_F30F[opcode];
		break;

	default:
		instr = instr_0F[opcode];
	}

	if (!instr) {
		return 0;
	} else if (instr == emms || instr == femms) {
		;
	} else if (instr == grp0FAE) {
		/* XXX wrong */
		hint = (void *)(int) *eip++;
	} else {
		hint = modrm(&src, &dest, instr, opcode, prefix, &imm8, &eip, context);

		if (opcode == 0x0F) {
			instr = instr_0F0F[(int) *eip++];

			if (instr == 0)
				return 0;
		}

		context->fpstate->tag = 0;
	}

	mmx_printf("ebp = 0x%08lx (adr = %p)\n", context->ebp, &context->ebp);
	mmx_printf("esp = 0x%08lx (adr = %p)\n", context->esp, &context->esp);

	/* Execute the instruction */

	instr(src, dest, hint);

	context->fpstate->status &= 0xffffe3ffL;

	mmx_printf("New EIP: %p\n", eip);

	context->eip = (uint32_t) eip;

	return 1;
}

/*
 *  SSE floating point functions
 */

extern ifunc movups, movlps2mem, movlps2xmm;
extern ifunc unpcklps, unpckhps, movhps2mem,	movhps2xmm;
extern ifunc movaps2mem, movaps2xmm, cvtpi2ps, movntps;
extern ifunc cvttps2pi, cvtps2pi, ucomiss, comiss;
extern ifunc movmskps, sqrtps, rsqrtps, rcpps;
extern ifunc andps, andnps, orps, xorps;
extern ifunc addps, mulps, subps, minps, divps, maxps;
extern ifunc cmpps, shufps;

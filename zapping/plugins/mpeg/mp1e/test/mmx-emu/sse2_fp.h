/*
 *  SSE2 floating point functions
 */

extern ifunc cvtsi2ss, cvttss2si, cvtss2si, cvtss2sd, cvttps2dq, cvtdq2pd;
extern ifunc rcpss, rsqrtss, addss, subss, mulss, divss, minss, maxss;
extern ifunc movdqu2xmm, movdqu2mem, cmpss, movq2dq, movsd;
extern ifunc cvtsi2sd, cvttsd2si, cvtsd2si, cvtsd2ss, cvtpd2dq;
extern ifunc sqrtsd, addsd, subsd, mulsd, divsd, minsd, maxsd;
extern ifunc movdq2q, movss2mem, cmpsd;
extern ifunc movupd, movlpd2mem, movlpd2xmm;
extern ifunc unpcklpd, unpckhpd, movhpd2mem, movhpd2xmm;
extern ifunc cvtpi2pd, cvttpd2pi, cvtpd2pi, ucomisd, comisd;
extern ifunc movapd2mem, movapd2xmm, movntpd, movnti;
extern ifunc movmskpd, sqrtpd, andpd, andnpd, orpd, xorpd;
extern ifunc addpd, mulpd, cvtpd2ps, cvtps2dq, subpd, minpd, divpd, maxpd;
extern ifunc cmppd, shufpd, cvttpd2dq;

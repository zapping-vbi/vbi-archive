/*
 *  MMX functions
 */

extern ifunc movq2mem, movd2mem, movq2mmx, movd2mmx;
extern ifunc paddusb, paddusw, paddsb, paddsw;
extern ifunc paddb, paddw, paddd, psubusb, psubusw;
extern ifunc psubsb, psubsw, psubb, psubw, psubd;
extern ifunc pmullw, pmulhw, pmaddwd;
extern ifunc por, pxor, pand, pandn;
extern ifunc pcmpgtb, pcmpgtw, pcmpgtd, pcmpeqb, pcmpeqw, pcmpeqd;
extern ifunc psrlw, psrld, psrlq, psraw, psrad, psllw, pslld, psllq;
extern ifunc pshimw, pshimd, pshimq;
extern ifunc punpcklbw, punpcklwd, punpckldq, packsswb, packuswb;
extern ifunc punpckhbw, punpckhwd, punpckhdq, packssdw;
extern ifunc emms;

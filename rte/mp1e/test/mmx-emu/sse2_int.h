/*
 *  SSE2 integer functions
 */

extern ifunc movq2xmm, movd2xmm;
extern ifunc xpaddusb, xpaddusw, xpaddsb, xpaddsw;
extern ifunc xpaddb, xpaddw, xpaddd, xpsubusb, xpsubusw;
extern ifunc xpsubsb, xpsubsw, xpsubb, xpsubw, xpsubd;
extern ifunc xpmullw, xpmulhw, xpmaddwd;
extern ifunc xpor, xpxor, xpand, xpandn;
extern ifunc xpcmpgtb, xpcmpgtw, xpcmpgtd, xpcmpeqb, xpcmpeqw, xpcmpeqd;
extern ifunc xpsrlw, xpsrld, xpsrlq, xpsraw, xpsrad, xpsllw, xpslld, xpsllq;
extern ifunc xpshimw, xpshimd, xpshimq;
extern ifunc xpunpcklbw, xpunpcklwd, xpunpckldq, xpacksswb, xpackuswb;
extern ifunc xpunpckhbw, xpunpckhwd, xpunpckhdq, xpackssdw;

extern ifunc maskmovdq, xpavgb, xpavgw;
extern ifunc xpextrw, xpinsrw, xpmaxsw, xpmaxub, xpminsw, xpminub;
extern ifunc xpmovmskb, xpmulhuw, xpsadbw, xpshufw;
extern ifunc xpaddq, xpsubq; 

extern ifunc pmuludq, paddq, psubq, pshuflw, pshufhw, pshufd;
extern ifunc pslldq, psrldq, punpckhqdq, punpcklqdq;
extern ifunc movmm2dq, movdq2mm, maskmovdqu;
extern ifunc movntdq;

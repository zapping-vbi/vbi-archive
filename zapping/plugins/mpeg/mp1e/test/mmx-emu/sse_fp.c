/*
 *  Emulation of the SSE floating point functions
 */

#include "mmx-emu.h"
#include "macros.h"

MMX_EMU_NI_MACRO(movups)
MMX_EMU_NI_MACRO(movlps2mem)
MMX_EMU_NI_MACRO(movlps2xmm)
MMX_EMU_NI_MACRO(unpcklps)
MMX_EMU_NI_MACRO(unpckhps)
MMX_EMU_NI_MACRO(movhps2mem)
MMX_EMU_NI_MACRO(movhps2xmm)
MMX_EMU_NI_MACRO(movaps2mem)
MMX_EMU_NI_MACRO(movaps2xmm)
MMX_EMU_NI_MACRO(cvtpi2ps)
MMX_EMU_NI_MACRO(movntps)
MMX_EMU_NI_MACRO(cvttps2pi)
MMX_EMU_NI_MACRO(cvtps2pi)
MMX_EMU_NI_MACRO(ucomiss)
MMX_EMU_NI_MACRO(comiss)
MMX_EMU_NI_MACRO(movmskps)
MMX_EMU_NI_MACRO(sqrtps)
MMX_EMU_NI_MACRO(rsqrtps)
MMX_EMU_NI_MACRO(rcpps)
MMX_EMU_NI_MACRO(andps)
MMX_EMU_NI_MACRO(andnps)
MMX_EMU_NI_MACRO(orps)
MMX_EMU_NI_MACRO(xorps)
MMX_EMU_NI_MACRO(addps)
MMX_EMU_NI_MACRO(mulps)
MMX_EMU_NI_MACRO(subps)
MMX_EMU_NI_MACRO(minps)
MMX_EMU_NI_MACRO(divps)
MMX_EMU_NI_MACRO(maxps)
MMX_EMU_NI_MACRO(cmpps)
MMX_EMU_NI_MACRO(shufps)

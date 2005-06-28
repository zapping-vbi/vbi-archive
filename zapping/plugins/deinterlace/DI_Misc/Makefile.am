## Process this file with automake to produce Makefile.in

# Note these are defaults, user CFLAGS take precedence.
SIMD_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/plugins/deinterlace \
	-O3 -fomit-frame-pointer -D_REENTRANT

# We make extensive use of inline functions.  The --param switches here
# increase various inlining limits (gcc 3.4.x and later only).
if HAVE_GCC_LIMITS
SIMD_CFLAGS += \
	--param max-inline-insns-single=50000 \
	--param large-function-insns=150000 \
	--param inline-unit-growth=30000 \
	--param large-function-growth=30000
endif

# TODO DI_OldGame.c
SIMD_SOURCES = \
	DI_Bob.c \
	DI_EvenOnly.c \
	DI_Greedy.c \
	DI_Greedy2Frame.c \
	DI_MoComp2.c \
	DI_OddOnly.c \
	DI_ScalerBob.c \
	DI_TomsMoComp.c \
	DI_TwoFrame.c \
	DI_VideoBob.c \
	DI_VideoWeave.c \
	DI_Weave.c

SIMD_LIBS =

# x86 and x86-64 

if HAVE_MMX
SIMD_LIBS += libDI_Misc_MMX.la
libDI_Misc_MMX_la_CFLAGS = $(SIMD_CFLAGS) -mmmx -DSIMD=CPU_FEATURE_MMX
libDI_Misc_MMX_la_SOURCES = $(SIMD_SOURCES)
endif

if HAVE_3DNOW
SIMD_LIBS += libDI_Misc_3DNOW.la
libDI_Misc_3DNOW_la_CFLAGS = $(SIMD_CFLAGS) -m3dnow -DSIMD=CPU_FEATURE_3DNOW
libDI_Misc_3DNOW_la_SOURCES = $(SIMD_SOURCES)
endif

if HAVE_SSE
SIMD_LIBS += libDI_Misc_SSE.la
libDI_Misc_SSE_la_CFLAGS = $(SIMD_CFLAGS) -msse -DSIMD=CPU_FEATURE_SSE
libDI_Misc_SSE_la_SOURCES = $(SIMD_SOURCES)
endif

if HAVE_SSE2
SIMD_LIBS += libDI_Misc_SSE2.la
libDI_Misc_SSE2_la_CFLAGS = $(SIMD_CFLAGS) -msse2 -DSIMD=CPU_FEATURE_SSE2
libDI_Misc_SSE2_la_SOURCES = $(SIMD_SOURCES)
endif

# powerpc

if HAVE_ALTIVEC
SIMD_LIBS += libDI_Misc_ALTIVEC.la
libDI_Misc_ALTIVEC_la_CFLAGS = \
	$(SIMD_CFLAGS) \
	-maltivec -mabi=altivec -DSIMD=CPU_FEATURE_ALTIVEC
libDI_Misc_ALTIVEC_la_SOURCES = $(SIMD_SOURCES)
endif

libDI_Misc_la_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/plugins/deinterlace

# To do / removed
#	DI_Adaptive.c
#	DI_BlendedClip.c
libDI_Misc_la_SOURCES = \
	$(SIMD_SOURCES)

libDI_Misc_la_LIBADD = $(SIMD_LIBS)

noinst_LTLIBRARIES = \
	$(SIMD_LIBS) \
	libDI_Misc.la

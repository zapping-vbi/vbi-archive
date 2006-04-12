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

SIMD_SOURCES = \
	DI_GreedyHF.c \
	DI_GreedyHM.c DI_GreedyHM.h \
	DI_GreedyHMPulldown.c \
	DI_GreedyHM_V.c

SIMD_LIBS =

# x86 and x86-64 

if CAN_COMPILE_MMX
SIMD_LIBS += libDI_GreedyH_MMX.la
libDI_GreedyH_MMX_la_CFLAGS = $(SIMD_CFLAGS) -mmmx -DSIMD=CPU_FEATURE_MMX
libDI_GreedyH_MMX_la_SOURCES = $(SIMD_SOURCES)
endif

if CAN_COMPILE_3DNOW
SIMD_LIBS += libDI_GreedyH_3DNOW.la
libDI_GreedyH_3DNOW_la_CFLAGS = $(SIMD_CFLAGS) -m3dnow -DSIMD=CPU_FEATURE_3DNOW
libDI_GreedyH_3DNOW_la_SOURCES = $(SIMD_SOURCES)
endif

if CAN_COMPILE_SSE
SIMD_LIBS += libDI_GreedyH_SSE.la
libDI_GreedyH_SSE_la_CFLAGS = $(SIMD_CFLAGS) -msse -DSIMD=CPU_FEATURE_SSE_INT
libDI_GreedyH_SSE_la_SOURCES = $(SIMD_SOURCES)
endif

if CAN_COMPILE_SSE2
SIMD_LIBS += libDI_GreedyH_SSE2.la
libDI_GreedyH_SSE2_la_CFLAGS = $(SIMD_CFLAGS) -msse2 -DSIMD=CPU_FEATURE_SSE2
libDI_GreedyH_SSE2_la_SOURCES = $(SIMD_SOURCES)
endif

if CAN_COMPILE_SSE3
SIMD_LIBS += libDI_GreedyH_SSE3.la
libDI_GreedyH_SSE3_la_CFLAGS = $(SIMD_CFLAGS) -msse3 -DSIMD=CPU_FEATURE_SSE3
libDI_GreedyH_SSE3_la_SOURCES = $(SIMD_SOURCES)
endif

# powerpc

if CAN_COMPILE_ALTIVEC
SIMD_LIBS += libDI_GreedyH_ALTIVEC.la
libDI_GreedyH_ALTIVEC_la_CFLAGS = \
	$(SIMD_CFLAGS) \
	-maltivec -mabi=altivec -DSIMD=CPU_FEATURE_ALTIVEC
libDI_GreedyH_ALTIVEC_la_SOURCES = $(SIMD_SOURCES)
endif

libDI_GreedyH_la_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/plugins/deinterlace

libDI_GreedyH_la_SOURCES = \
	DI_GreedyH.c \
	DI_GreedyHM.c DI_GreedyHM.h \
	DI_GreedyHMPulldown.c

libDI_GreedyH_la_LIBADD = \
	$(SIMD_LIBS)

noinst_LTLIBRARIES = \
	$(SIMD_LIBS) \
	libDI_GreedyH.la

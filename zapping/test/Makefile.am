## Process this file with automake to produce Makefile.in

TESTS = guard1 guard2 simd clear_image memcpy simd
XFAIL_TESTS = guard1 guard2

EXTRA_DIST = \
	guard.h \
	pixel.h

INCLUDES = -I$(top_srcdir)

SIMD_CFLAGS =
SIMD_SOURCES = simd.c
SIMD_LIBS =

# x86 and x86-64 

if HAVE_MMX
SIMD_LIBS += libsimd_MMX.la
libsimd_MMX_la_CFLAGS = $(SIMD_CFLAGS) -mmmx -DSIMD=MMX
libsimd_MMX_la_SOURCES = $(SIMD_SOURCES)
endif

# Untested
#if HAVE_3DNOW
#SIMD_LIBS += libsimd_3DNOW.la
#libsimd_3DNOW_la_CFLAGS = $(SIMD_CFLAGS) -m3dnow -DSIMD=_3DNOW
#libsimd_3DNOW_la_SOURCES = $(SIMD_SOURCES)
#endif

if HAVE_SSE
SIMD_LIBS += libsimd_SSE.la
libsimd_SSE_la_CFLAGS = $(SIMD_CFLAGS) -msse -DSIMD=SSE
libsimd_SSE_la_SOURCES = $(SIMD_SOURCES)
endif

# Untested
#if HAVE_SSE2
#SIMD_LIBS += libsimd_SSE2.la
#libsimd_SSE2_la_CFLAGS = $(SIMD_CFLAGS) -msse2 -DSIMD=SSE2
#libsimd_SSE2_la_SOURCES = $(SIMD_SOURCES)
#endif

# powerpc

# Untested
#if HAVE_ALTIVEC
#SIMD_LIBS += libsimd_AVEC.la
#libsimd_AVEC_la_CFLAGS = $(SIMD_CFLAGS) -maltivec -mabi=altivec -DSIMD=ALTIVEC
#libsimd_AVEC_la_SOURCES = $(SIMD_SOURCES)
#endif

noinst_LTLIBRARIES = $(SIMD_LIBS)

check_PROGRAMS = \
	clear_image \
	guard1 \
	guard2 \
	memcpy \
	simd

simd_LDADD = \
	$(SIMD_LIBS) \
	$(top_builddir)/libtv/libtv.la

LDADD = \
	$(top_builddir)/libtv/libtv.la

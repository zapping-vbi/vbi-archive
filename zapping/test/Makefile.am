## Process this file with automake to produce Makefile.in

TESTS = guard1 guard2 clear_image convert_image memcpy
XFAIL_TESTS = guard1 guard2

EXTRA_DIST = \
	clear_image-mmx.sh \
	clear_image-sse.sh \
	convert_image-mmx.sh \
	guard.h \
	memcpy-mmx.sh \
	memcpy-sse.sh \
	pixel.h \
	simd-3dnow.sh \
	simd-altivec.sh \
	simd-emu.sh \
	simd-mmx.sh \
	simd-sse.sh \
	simd-sse2.sh

INCLUDES = -I$(top_srcdir)

SIMD_CFLAGS = -O3 -fomit-frame-pointer
SIMD_SOURCES = simd.c
SIMD_LIBS =

# x86 and x86-64 

if CAN_COMPILE_MMX
TESTS += \
	clear_image-mmx.sh \
	convert_image-mmx.sh \
	memcpy-mmx.sh \
	simd-mmx.sh
SIMD_LIBS += libsimd_MMX.la
libsimd_MMX_la_CFLAGS = $(SIMD_CFLAGS) -mmmx -DSIMD=CPU_FEATURE_MMX
libsimd_MMX_la_SOURCES = $(SIMD_SOURCES)
endif

if CAN_COMPILE_3DNOW
TESTS += simd-3dnow.sh
SIMD_LIBS += libsimd_3DNOW.la
libsimd_3DNOW_la_CFLAGS = $(SIMD_CFLAGS) -m3dnow -DSIMD=CPU_FEATURE_3DNOW
libsimd_3DNOW_la_SOURCES = $(SIMD_SOURCES)
endif

if CAN_COMPILE_SSE
TESTS += clear_image-sse.sh memcpy-sse.sh simd-sse.sh
SIMD_LIBS += libsimd_SSE.la
libsimd_SSE_la_CFLAGS = $(SIMD_CFLAGS) -msse -DSIMD=CPU_FEATURE_SSE
libsimd_SSE_la_SOURCES = $(SIMD_SOURCES)
endif

if CAN_COMPILE_SSE2
TESTS += simd-sse2.sh
SIMD_LIBS += libsimd_SSE2.la
libsimd_SSE2_la_CFLAGS = $(SIMD_CFLAGS) -msse2 -DSIMD=CPU_FEATURE_SSE2
libsimd_SSE2_la_SOURCES = $(SIMD_SOURCES)
endif

# powerpc

if CAN_COMPILE_ALTIVEC
TESTS += simd-altivec.sh
SIMD_LIBS += libsimd_AVEC.la
libsimd_AVEC_la_CFLAGS = $(SIMD_CFLAGS) \
	-maltivec -mabi=altivec -DSIMD=CPU_FEATURE_ALTIVEC
libsimd_AVEC_la_SOURCES = $(SIMD_SOURCES)
endif

noinst_LTLIBRARIES = $(SIMD_LIBS)

check_PROGRAMS = \
	cpudt \
	clear_image \
	convert_image \
	guard1 \
	guard2 \
	memcpy \
	simd

convert_image_LDADD = \
	-lm \
	$(top_builddir)/libtv/libtv.la

check_SCRIPTS = \
	simd-emu.sh

simd_LDADD = \
	$(SIMD_LIBS) \
	$(top_builddir)/libtv/libtv.la

LDADD = \
	$(top_builddir)/libtv/libtv.la

simd-emu.sh:
	$(MAKE) cpudt

memcpy-mmx.sh \
memcpy-sse.sh \
memcpy-altivec.sh:
	$(MAKE) memcpy

clear_image-mmx.sh \
clear_image-sse.sh \
clear_image-altivec.sh:
	$(MAKE) clear_image

convert_image-mmx.sh:
	$(MAKE) convert_image

simd-mmx.sh \
simd-3dnow.sh \
simd-sse.sh \
simd-sse2.sh \
simd-altivec.sh:
	$(MAKE) simd

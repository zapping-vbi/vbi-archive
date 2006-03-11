## Process this file with automake to produce Makefile.in

BUILT_SOURCES = \
	lut_rgb16.c \
	lut_yuv2rgb.c \
	lut_yuv2rgb.h

MAINTAINERCLEANFILES = \
	lut_rgb16-gen \
	lut_yuv2rgb-gen

EXTRA_DIST = \
	$(BUILT_SOURCES) \
	lut_rgb16-gen.c \
	lut_yuv2rgb-gen.c

AM_CFLAGS = \
	-D_GNU_SOURCE \
	-D_REENTRANT \
	-DUNTESTED_SIMD=1 \
	@X_CFLAGS@

# Note these are defaults, user CFLAGS take precedence.
SIMD_CFLAGS = \
	-O3 -fomit-frame-pointer

# We make extensive use of inline functions.  The --param switches here
# increase various inlining limits (gcc 3.4.x and later only).
if HAVE_GCC_LIMITS
SIMD_CFLAGS += \
	--param max-inline-insns-single=50000 \
	--param large-function-insns=150000 \
	--param inline-unit-growth=30000 \
	--param large-function-growth=300000
endif

SIMD_SOURCES = \
	rgb2rgb.c rgb2rgb.h \
	simd-consts.h \
	simd-conv.h \
	yuv2rgb.c yuv2rgb.h \
	yuv2yuv.c yuv2yuv.h

INCLUDES = -I$(top_srcdir)

libtv_la_SOURCES = \
	$(SIMD_SOURCES) \
	callback.c callback.h \
	clip_vector.c clip_vector.h \
	control.c control.h \
	cpu.c cpu.h \
	image_format.c image_format.h \
	lut_rgb16.c lut_rgb16.h \
	lut_yuv2rgb.c lut_yuv2rgb.h \
	macros.h \
	misc.c misc.h \
	overlay_buffer.h \
	pixel_format.c pixel_format.h \
	screen.c screen.h \
	simd.h

SUBDIRS =
SIMD_LIBS =
libtv_la_LIBADD =

if CAN_COMPILE_MMX
SUBDIRS += mmx
libtv_la_LIBADD += mmx/libmmx.la
SIMD_LIBS += libtv_MMX.la
libtv_MMX_la_CFLAGS = $(SIMD_CFLAGS) -mmmx -DSIMD=CPU_FEATURE_MMX
libtv_MMX_la_SOURCES = $(SIMD_SOURCES)
endif

if CAN_COMPILE_SSE
SUBDIRS += sse
libtv_la_LIBADD += sse/libsse.la
endif

if CAN_COMPILE_ALTIVEC
SUBDIRS += avec
libtv_la_LIBADD += avec/libavec.la
endif

libtv_la_LIBADD += $(SIMD_LIBS)

noinst_LTLIBRARIES = \
	$(SIMD_LIBS) \
	libtv.la

if MAINTAINER_MODE

noinst_PROGRAMS = \
	lut_rgb16-gen \
	lut_yuv2rgb-gen

lut_yuv2rgb_gen_LDADD = -lm

lut_rgb16.c: lut_rgb16-gen Makefile.am
	$(top_builddir)/libtv/lut_rgb16-gen$(EXEEXT) > $@


lut_yuv2rgb.c: lut_yuv2rgb-gen Makefile.am
	$(top_builddir)/libtv/lut_yuv2rgb-gen$(EXEEXT) > $@

lut_yuv2rgb.h: lut_yuv2rgb-gen Makefile.am
	$(top_builddir)/libtv/lut_yuv2rgb-gen$(EXEEXT) header > $@

endif

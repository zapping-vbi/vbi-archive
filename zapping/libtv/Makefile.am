## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libtv.la

INCLUDES = \
	-I$(top_srcdir) \
	-D_REENTRANT \
	-DUNTESTED_SIMD=1 \
	@X_CFLAGS@ \
	@PG_CFLAGS@

libtv_la_SOURCES = \
	callback.c callback.h \
	clip_vector.c clip_vector.h \
	cpu.c cpu.h \
	image_format.c image_format.h \
	macros.h \
	misc.c misc.h \
	overlay_buffer.h \
	pixel_format.c pixel_format.h \
	screen.c screen.h

SUBDIRS =
libtv_la_LIBADD =

if HAVE_MMX
SUBDIRS += mmx
libtv_la_LIBADD += mmx/libmmx.la
endif

if HAVE_SSE
SUBDIRS += sse
libtv_la_LIBADD += sse/libsse.la
endif

if HAVE_ALTIVEC
SUBDIRS += avec
libtv_la_LIBADD += avec/libavec.la
endif

## Process this file with automake to produce Makefile.in

if HAVE_MMX
mmx_dir = mmx
mmx_lib = mmx/mmx.o mmx/copy_block.o
else
mmx_dir =
mmx_lib =
endif

if HAVE_SSE
sse_dir = sse
sse_lib = sse/sse.o sse/copy_block.o
else
sse_dir =
sse_lib =
endif

if HAVE_ALTIVEC
avec_dir = avec
avec_lib = avec/avec.o
else
avec_dir =
avec_lib =
endif

SUBDIRS = $(mmx_dir) $(sse_dir) $(avec_dir)

noinst_LIBRARIES = libtv.a

INCLUDES = \
	-I$(top_srcdir) \
	-D_REENTRANT \
	-DUNTESTED_SIMD=1 \
	@X_CFLAGS@ \
	@PG_CFLAGS@

libtv_a_SOURCES = \
	callback.c callback.h \
	clip_vector.c clip_vector.h \
	image_format.c image_format.h \
	macros.h \
	misc.c misc.h \
	overlay_buffer.h \
	pixel_format.c pixel_format.h \
	screen.c screen.h

libtv_a_LIBADD = $(mmx_lib) $(sse_lib) $(avec_lib)

# libtv_a_LIBADD = $(X_LIBS) -lX11 $(X_XINERAMA_LIBS) $(X_XF86DGA_LIBS)

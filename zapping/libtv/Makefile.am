## Process this file with automake to produce Makefile.in

noinst_LIBRARIES = libtv.a

INCLUDES = -I$(top_srcdir) $(X_CFLAGS) -D_REENTRANT

libtv_a_SOURCES = \
	callback.c callback.h \
	clip_vector.c clip_vector.h \
	image_format.c image_format.h \
	macros.h \
	misc.c misc.h \
	overlay_buffer.h \
	pixel_format.c pixel_format.h \
	screen.c screen.h

# libtv_a_LIBADD = $(X_LIBS) -lX11 $(X_XINERAMA_LIBS) $(X_XF86DGA_LIBS)

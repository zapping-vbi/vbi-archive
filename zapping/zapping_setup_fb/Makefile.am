## Process this file with automake to produce Makefile.in

INCLUDES = \
$(ARCH_CFLAGS)

bin_PROGRAMS = zapping_setup_fb

INSTALL_PROGRAM = $(INSTALL) -m4711 -o root -g root

zapping_setup_fbSOURCES = \
	zapping_setup_fb.c videodev.h

zapping_setup_fb_LDADD = -L/usr/X11R6/lib -L/usr/lib -L/usr/X11/lib \
-lX11 -lXext $(X_DGA_LIBS)

## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	$(GNOME_INCLUDEDIR)

bin_PROGRAMS = zapping_setup_fb

INSTALL_PROGRAM = $(INSTALL) -m4711 -o root -g root

zapping_setup_fbSOURCES = \
	zapping_setup_fb.c videodev.h

zapping_setup_fb_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(INTLLIBS) -lXxf86dga

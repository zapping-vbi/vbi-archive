## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	$(GNOME_INCLUDEDIR)

bin_PROGRAMS = zapping

zapping_SOURCES = \
	main.c \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	tveng.c tveng.h \
	frequencies.c frequencies.h \
	io.c io.h \
	v4l2interface.c v4l2interface.h \
	plugins.c plugins.h

zapping_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(INTLLIBS) $(GNOME_XML_LIB) -lpng -lXxf86dga
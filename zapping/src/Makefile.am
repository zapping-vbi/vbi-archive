## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	$(GNOME_INCLUDEDIR)

bin_PROGRAMS = zapping

## Don't forget to put these two files into the distribution
EXTRA_DIST = videodev.h videodev2.h

zapping_SOURCES = \
	main.c \
	interface.c interface.h \
	callbacks.c callbacks.h \
	frequencies.c frequencies.h \
	v4linterface.c v4linterface.h \
	plugins.c plugins.h \
	zconf.c zconf.h \
	zmisc.c zmisc.h \
	properties.c callbacks.h \
	channel_editor.c callbacks.h \
	tveng1.c tveng1.h \
##	tveng2.c tveng2.h \
	tveng.c tveng.h 

zapping_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(INTLLIBS) \
$(GNOME_XML_LIB) $(LIBGLADE_LIBS) -lXxf86dga
## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	$(GNOME_INCLUDEDIR)

bin_PROGRAMS = zapping

## Don't forget to put these files into the distribution
EXTRA_DIST = videodev.h videodev2.h plugin_common.h ROADMAP

zapping_SOURCES = \
	main.c \
	interface.c interface.h \
	callbacks.c callbacks.h \
	frequencies.c frequencies.h \
	v4linterface.c v4linterface.h \
	sound.c sound.h \
	plugins.c plugins.h \
	zconf.c zconf.h \
	zmisc.c zmisc.h \
	properties.c callbacks.h \
	channel_editor.c callbacks.h \
	plugin_properties.c callbacks.h \
	tveng1.c tveng1.h \
	tveng2.c tveng2.h \
	tveng.c tveng.h

zapping_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(INTLLIBS) \
$(GNOME_XML_LIB) $(LIBGLADE_LIBS) $(X_DGA_LIBS) $(PTHREAD_LIB) \
$(PNG_LIB)

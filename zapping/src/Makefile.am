## Process this file with automake to produce Makefile.in

INCLUDES = \
-I$(top_srcdir)/libvbi \
-I$(top_srcdir)/intl \
$(GNOME_INCLUDEDIR) \
$(GDK_PIXBUF_CFLAGS)

bin_PROGRAMS = zapping

VBI_LIB = $(top_srcdir)/libvbi/libvbi.a

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
	properties.c \
	channel_editor.c \
	plugin_properties.c \
	txtcontrols.c \
	tveng1.c tveng1.h \
	tveng2.c tveng2.h \
	tveng.c tveng.h \
	zvbi.c zvbi.h

zapping_LDADD = $(VBI_LIB) $(GNOME_LIBDIR) $(GNOMEUI_LIBS) \
$(GDK_PIXBUF_LIBS) $(INTLLIBS) $(GNOME_XML_LIB) $(LIBGLADE_LIBS) \
$(X_DGA_LIBS) $(PTHREAD_LIB)


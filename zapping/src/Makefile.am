## Process this file with automake to produce Makefile.in

INCLUDES = \
-I$(top_srcdir)/libvbi \
-I$(top_srcdir)/intl \
$(ARCH_CFLAGS) \
$(GNOME_INCLUDEDIR) \
$(GDK_PIXBUF_CFLAGS)

bin_PROGRAMS = zapping

VBI_LIB = $(top_srcdir)/libvbi/libvbi.a

## Don't forget to put these files into the distribution
EXTRA_DIST = videodev.h videodev2.h plugin_common.h ROADMAP

zapping_SOURCES = \
	main.c \
	yuv2rgb_mmx.c \
	mmx.c mmx.h \
	yuv2rgb.c yuv2rgb.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	frequencies.c frequencies.h \
	v4linterface.c v4linterface.h \
	plugins.c plugins.h \
	zconf.c zconf.h \
	zmisc.c zmisc.h \
	properties.c \
	channel_editor.c \
	plugin_properties.c \
	txtcontrols.c \
	tveng1.c tveng1.h \
	tveng2.c tveng2.h \
	tvengxv.c tvengxv.h \
	tveng.c tveng.h tveng_private.h \
	x11stuff.c x11stuff.h \
	overlay.c overlay.h \
	capture.c capture.h \
	zvbi.c zvbi.h \
	remote.c remote.h \
	strnatcmp.c strnatcmp.h

zapping_LDADD = $(VBI_LIB) $(GNOME_LIBDIR) $(GNOMEUI_LIBS) \
$(GDK_PIXBUF_LIBS) $(INTLLIBS) $(GNOME_XML_LIB) $(LIBGLADE_LIBS) \
$(X_DGA_LIBS) $(X_XV_LIBS) $(PTHREAD_LIB)


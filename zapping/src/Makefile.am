## Process this file with automake to produce Makefile.in

INCLUDES = \
$(X_CFLAGS) \
-I$(top_srcdir)/libvbi \
-I$(top_srcdir)/intl \
$(ARCH_CFLAGS) \
$(X_CFLAGS) \
$(GNOME_INCLUDEDIR) \
$(GDK_PIXBUF_CFLAGS) \
$(UNICODE_CFLAGS) \
$(LIBXML_CFLAGS)

bin_PROGRAMS = zapping

VBI_LIB = $(top_srcdir)/libvbi/libvbi.a
COMMON_LIB = $(top_srcdir)/common/libcommon.a

## Don't forget to put these files into the distribution
EXTRA_DIST = ROADMAP

zapping_SOURCES = \
	main.c \
	fullscreen.c fullscreen.h \
	ttxview.c ttxview.h \
	osd.c osd.h \
	cpu.c cpu.h \
	yuv2rgb.c yuv2rgb.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	frequencies.c frequencies.h \
	v4linterface.c v4linterface.h \
	plugins.c plugins.h plugin_common.h \
	zconf.c zconf.h \
	zmisc.c error_console.c zmisc.h \
	properties.c \
	channel_editor.c keysyms.h \
	plugin_properties.c \
	tveng1.c tveng1.h \
	tveng2.c tveng2.h \
	tvengxv.c tvengxv.h \
	tveng.c tveng.h tveng_private.h \
	x11stuff.c x11stuff.h \
	overlay.c overlay.h \
	capture.c capture.h \
	zvbi.c zvbi.h \
	remote.c remote.h \
	strnatcmp.c strnatcmp.h \
	zmodel.c zmodel.h \
	gen_conv.s gen_conv.h

zapping_LDADD = $(VBI_LIB) $(COMMON_LIB) $(GNOME_LIBDIR) $(GNOMEUI_LIBS) \
$(GDK_PIXBUF_LIBS) $(INTLLIBS) $(LIBXML_LIBS) $(LIBGLADE_LIBS) \
$(X_DGA_LIBS) $(X_XV_LIBS) $(X_XDPMS_LIBS) $(PTHREAD_LIB) $(PNG_LIB) \
$(UNICODE_LIBS)

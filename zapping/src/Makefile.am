## Process this file with automake to produce Makefile.in

INCLUDES = $(COMMON_INCLUDES) -I$(top_srcdir)/libvbi -I$(top_srcdir)/intl

bin_PROGRAMS = zapping

VBI_LIB = $(top_srcdir)/libvbi/libvbi.a
COMMON_LIB = $(top_srcdir)/common/libcommon.a

## Don't forget to put these files into the distribution
EXTRA_DIST = ROADMAP gen_conv.s

install-binPROGRAMS: $(bin_PROGRAMS)
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) zapping $(DESTDIR)$(bindir)
	ln -sf $(DESTDIR)$(bindir)/zapping $(DESTDIR)$(bindir)/zapzilla

uninstall-binPROGRAMS:
	rm -f $(DESTDIR)$(bindir)/zapzilla
	rm -f $(DESTDIR)$(bindir)/zapping

zapping_SOURCES = \
	main.c zapzilla.c \
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
	@GEN_CONV_S@ gen_conv.h

zapping_LDADD = $(GEN_CONV_O) $(VBI_LIB) $(COMMON_LIB) $(GNOME_LIBDIR) \
$(GNOMEUI_LIBS) $(GDK_PIXBUF_LIBS) $(INTLLIBS) $(LIBXML_LIBS) \
$(LIBGLADE_LIBS) $(X_DGA_LIBS) $(X_XV_LIBS) $(X_XDPMS_LIBS) \
$(PTHREAD_LIB) $(PNG_LIB) $(UNICODE_LIBS)

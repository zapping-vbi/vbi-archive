## Process this file with automake to produce Makefile.in

INCLUDES = $(COMMON_INCLUDES) -I$(top_srcdir)/intl

bin_PROGRAMS = zapping

COMMON_LIB = $(top_builddir)/common/libcommon.a

## Don't forget to put these files into the distribution
EXTRA_DIST = ROADMAP gen_conv.s

if BUILD_GEN_CONV
gen_conv_lib = libgen_conv.a
endif

noinst_LIBRARIES = $(gen_conv_lib)

libgen_conv_a_SOURCES = gen_conv.s

install-binPROGRAMS: $(bin_PROGRAMS)
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) zapping $(DESTDIR)$(bindir)
	ln -sf $(DESTDIR)$(bindir)/zapping $(DESTDIR)$(bindir)/zapzilla

uninstall-binPROGRAMS:
	rm -f $(DESTDIR)$(bindir)/zapzilla
	rm -f $(DESTDIR)$(bindir)/zapping

zapping_SOURCES = \
	csconvert.c csconvert.h \
	audio.c esd.c oss.c arts.c audio.h \
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
	properties.c properties.h \
	properties-handler.c properties-handler.h \
	channel_editor.c keysyms.h \
	plugin_properties.c \
	tveng1.c tveng1.h \
	tveng2.c tveng2.h \
	tvengxv.c tvengxv.h \
	tveng25.c tveng25.h \
	tveng.c tveng.h tveng_private.h \
	x11stuff.c x11stuff.h video_xv.c \
	overlay.c overlay.h \
	capture.c capture.h \
	zvbi.c zvbi.h \
	remote.c remote.h \
	strnatcmp.c strnatcmp.h \
	zmodel.c zmodel.h \
	gen_conv.h \
	mixer.c mixer.h \
	keyboard.c keyboard.h

zapping_LDADD = $(gen_conv_lib) $(ZVBI_LIB) $(COMMON_LIB) $(GNOME_LIBDIR) \
$(GNOMEUI_LIBS) $(GDK_PIXBUF_LIBS) $(INTLLIBS) $(LIBXML_LIBS) \
$(LIBGLADE_LIBS) $(X_LIBS) $(X_DGA_LIBS) $(X_XV_LIBS) $(X_XDPMS_LIBS) \
$(PTHREAD_LIB) $(PNG_LIB) $(UNICODE_LIBS) $(ARTS_LIBS) $(CCMALLOC)

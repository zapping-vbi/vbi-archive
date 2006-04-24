## Process this file with automake to produce Makefile.in

bin_PROGRAMS = zapping

if HAVE_XMU
bin_PROGRAMS += zapping_remote
endif

BUILT_SOURCES = \
	zmarshalers.c \
	zmarshalers.h

EXTRA_DIST = \
	gen_conv.s \
	zmarshalers.list

# Adding -g here is bad but I need better BugBuddy backtraces.
AM_CFLAGS = \
	-D_GNU_SOURCE \
	@DEFAULT_CFLAGS@ \
	$(ZVBI_CFLAGS) \
	$(LIBXML_CFLAGS) \
	-DZAPPING8 \
	-DPACKAGE_VERSION_ID=@PACKAGE_VERSION_ID@ \
	-g

INCLUDES = \
	-I$(top_srcdir) \
	@COMMON_INCLUDES@

# Need gcc and as support for SSE to build this module.
# Cannot build for host x86_64, code is 32 bit.
if HAVE_X86
if CAN_COMPILE_SSE
gen_conv_lib = libgen_conv.a
else
gen_conv_lib =
endif
else
gen_conv_lib =
endif

noinst_LIBRARIES = $(gen_conv_lib)

libgen_conv_a_SOURCES = gen_conv.s

zmarshalers.c: zmarshalers.list
	@GLIB_GENMARSHAL@ --prefix=z_marshal $< --body > $@

zmarshalers.h: zmarshalers.list
	@GLIB_GENMARSHAL@ --prefix=z_marshal $< --header > $@

zapping_SOURCES = \
	arts.c esd.c oss.c \
	audio.c audio.h \
	capture.c capture.h \
	channel_editor.c channel_editor.h \
	cmd.c cmd.h \
	csconvert.c csconvert.h \
	eggcellrendererkeys.c eggcellrendererkeys.h \
	eggaccelerators.c eggaccelerators.h \
	frequencies.c frequencies.h \
	fullscreen.c fullscreen.h \
	globals.c globals.h \
	i18n.c i18n.h \
	interface.c interface.h \
	keyboard.c keyboard.h keysyms.h \
	main.c \
	mixer.c mixer.h \
	osd.c osd.h \
	overlay.c overlay.h \
	plugin_properties.c plugin_properties.h \
	plugins.c plugins.h plugin_common.h \
	properties.c properties.h \
	properties-handler.c properties-handler.h \
	remote.c remote.h \
	subtitle.c subtitle.h \
	tveng.c tveng.h tveng_private.h \
	tveng1.c tveng1.h \
	tveng25.c tveng25.h \
	tvengbktr.c tvengbktr.h \
	tvengemu.c tvengemu.h \
	tvengxv.c tvengxv.h \
	v4linterface.c v4linterface.h \
	vdr.c vdr.h \
	video_xv.c video_x11.c video_gdkrgb.c video_mem.c \
	x11stuff.c x11stuff.h \
	xawtv.c xawtv.h \
	yuv2rgb.c yuv2rgb.h gen_conv.h \
	zapping.c zapping.h \
	zconf.c zconf.h \
	zgconf.c zgconf.h \
	zimage.c zimage.h \
	zmarshalers.c zmarshalers.h \
	zmisc.c zmisc.h \
	zmodel.c zmodel.h \
	zspinslider.c zspinslider.h \
	zstack.c zstack.h \
	zvbi.c zvbi.h \
	zvideo.c zvideo.h

zapping_LDADD = \
	$(top_builddir)/common/libcommon.a \
	$(top_builddir)/libtv/libtv.la \
	$(top_builddir)/libvbi/libvbi.la \
	$(gen_conv_lib) \
	@PACKAGE_LIBS@ \
	@PYTHON_LIBS@ \
	@X_LIBS@ \
	-lX11 \
	@X_DPMS_LIBS@ \
	@X_XINERAMA_LIBS@ \
	@X_XF86DGA_LIBS@ \
	@X_XF86VMODE_LIBS@ \
	@X_XV_LIBS@ \
	@ZVBI_LIBS@ \
	@PNG_LIBS@ \
	@ARTS_LIBS@ \
	@ESD_LIBS@ \
	@LIBXML_LIBS@

zapping_LDFLAGS = -export-dynamic

zapping_remote_SOURCES = \
	zapping_remote.c

zapping_remote_LDADD = \
	@X_LIBS@ \
	-lX11 \
        @X_XMU_LIBS@

# No zapzilla in this version. When it returns this rule should be
# replaced by install-exec-local instead of overriding install-bin.
# install-binPROGRAMS: $(bin_PROGRAMS)
#	$(mkinstalldirs) $(DESTDIR)$(bindir)
#	$(INSTALL_PROGRAM) zapping $(DESTDIR)$(bindir)
#	ln -sf $(DESTDIR)$(bindir)/zapping $(DESTDIR)$(bindir)/zapzilla
#
# uninstall-binPROGRAMS:
#	rm -f $(DESTDIR)$(bindir)/zapping
#	rm -f $(DESTDIR)$(bindir)/zremote
#	rm -f $(DESTDIR)$(bindir)/zapzilla

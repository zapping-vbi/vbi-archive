## Process this file with automake to produce Makefile.in

plugindir = @PLUGIN_DEFAULT_DIR@

plugin_LTLIBRARIES = libscreenshot.zapping.la

EXTRA_DIST = screenshot.glade2
pkgdata_DATA = screenshot.glade2

AM_CFLAGS = \
	@PLUGIN_DEFAULT_FLAGS@ \
	-DZAPPING8

INCLUDES = \
	-I$(top_srcdir) \
	@COMMON_INCLUDES@

libscreenshot_zapping_la_SOURCES = \
	screenshot.c screenshot.h \
	deint.c \
	b_jpeg.c b_ppm.c

libscreenshot_zapping_la_LIBADD = \
	$(JPEG_LIB)

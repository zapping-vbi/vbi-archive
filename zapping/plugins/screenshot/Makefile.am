## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = libscreenshot.zapping.la

EXTRA_DIST = screenshot.glade screenshot_strings.c

libscreenshot_zapping_la_SOURCES = \
	screenshot.c screenshot.h \
	b_jpeg.c b_ppm.c

libscreenshot_zapping_la_LIBADD = $(JPEG_LIB)

pkgdata_DATA = screenshot.glade

CFLAGS = $(PLUGIN_DEFAULT_FLAGS)

INCLUDES = \
$(COMMON_INCLUDES) \
-I$(top_srcdir)/intl \
-I$(top_srcdir)/src ## Let the plugin access Zapping headers

## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = libscreenshot.zapping.la
EXTRA_DIST = screenshot.glade2

libscreenshot_zapping_la_SOURCES = \
	screenshot.c screenshot.h deint.c \
	b_jpeg.c b_ppm.c

libscreenshot_zapping_la_LIBADD = $(JPEG_LIB)

pkgdata_DATA = screenshot.glade2

INCLUDES = -I$(top_srcdir) $(COMMON_INCLUDES) \
	$(PLUGIN_DEFAULT_FLAGS) @PG_CFLAGS@

## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = libscreenshot.zapping.la

EXTRA_DIST = screenshot.glade screenshot_strings.c

libscreenshot_zapping_la_SOURCES = screenshot.c
libscreenshot_zapping_la_LIBADD = $(JPEG_LIB)

pkgdata_DATA = screenshot.glade

CFLAGS = @CFLAGS@ $(PLUGIN_DEFAULT_FLAGS)

INCLUDES = \
$(X_CFLAGS) \
-I$(top_srcdir)/intl \
-I$(includedir)	\
$(GNOME_INCLUDEDIR) \
-I$(top_srcdir)/src ## Let the plugin access Zapping headers

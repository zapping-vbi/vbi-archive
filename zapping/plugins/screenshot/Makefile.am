## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = libscreenshot.zapping.la

libscreenshot_zapping_la_SOURCES = screenshot.c
libscreenshot_zapping_la_LIBADD = $(JPEG_LIB)

CFLAGS = -Wall -Wno-unused # Avoid warnings about undeclared statics
INCLUDES = \
-I$(top_srcdir)/intl \
-I$(includedir)	\
$(ARCH_CFLAGS) \
$(GNOME_INCLUDEDIR) \
-I$(top_srcdir)/src ## Let the plugin access Zapping headers

## Process this file with automake to produce Makefile.in

plugindir = $(prefix)/share/zapping/plugins

plugin_LTLIBRARIES = libmpeg.zapping.la

EXTRA_DIST = mpeg_properties.glade mpeg_strings.c

libmpeg_zapping_la_SOURCES = plugin.c
libmpeg_zapping_la_LIBADD = -lrte

pkgdata_DATA = mpeg_properties.glade

CFLAGS = -Wall -Wno-unused # Avoid warnings about undeclared statics
INCLUDES = \
-I$(top_srcdir)/intl \
-I$(includedir)	\
$(ARCH_CFLAGS) \
$(GNOME_INCLUDEDIR) \
-I$(top_srcdir)/src ## Let the plugin access Zapping headers
## Process this file with automake to produce Makefile.in

plugindir = $(prefix)/share/zapping/plugins

plugin_LTLIBRARIES = libmpeg.zapping.la

EXTRA_DIST = interface.glade mpeg_strings.c

libmpeg_zapping_la_SOURCES = plugin.c

CFLAGS = -Wall -Wno-unused # Avoid warnings about undeclared statics
INCLUDES = \
-I$(top_srcdir)/intl \
-I$(includedir)	\
$(ARCH_CFLAGS) \
$(GNOME_INCLUDEDIR) \
-I$(top_srcdir)/src ## Let the plugin access Zapping headers
## Process this file with automake to produce Makefile.in

plugindir = $(prefix)/share/zapping/plugins

plugin_LTLIBRARIES = libscreenshot.zapping.la

libscreenshot_zapping_la_SOURCES = screenshot.c

CFLAGS = -Wall -Wno-unused # Avoid warnings about undeclared statics
INCLUDES = \
-I$(top_srcdir)/intl \
-I$(includedir)	\
$(GNOME_INCLUDEDIR) \
-I$(top_srcdir)/src ## Let the plugin access Zapping headers
## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = libmpeg.zapping.la

EXTRA_DIST = mpeg_properties.glade mpeg_strings.c

libmpeg_zapping_la_SOURCES = plugin.c
libmpeg_zapping_la_LIBADD = $(RTE_LIB)

pkgdata_DATA = mpeg_properties.glade

CFLAGS = @CFLAGS@ $(PLUGIN_DEFAULT_FLAGS)

INCLUDES = \
-I$(top_srcdir)/intl \
-I$(includedir)	\
$(GNOME_INCLUDEDIR) \
-I$(top_srcdir)/src ## Let the plugin access Zapping headers

## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = libmpeg.zapping.la

pixmapsdir = $(PACKAGE_PIXMAPS_DIR)
pixmaps_DATA = time.png disk_empty.png drop.png volume.png \
record.png pause.png stop.png

EXTRA_DIST = mpeg_properties.glade mpeg_strings.c $(pixmaps_DATA)

libmpeg_zapping_la_SOURCES = mpeg.c options.c mpeg.h

libmpeg_zapping_la_LIBADD = $(RTE_LIB)

pkgdata_DATA = mpeg_properties.glade

CFLAGS = $(PLUGIN_DEFAULT_FLAGS)

INCLUDES = \
$(COMMON_INCLUDES) \
-I$(top_srcdir)/intl \
-I$(top_srcdir)/src ## Let the plugin access Zapping headers

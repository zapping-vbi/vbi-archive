## Process this file with automake to produce Makefile.in

plugindir = @PLUGIN_DEFAULT_DIR@

plugin_LTLIBRARIES = libparrot.zapping.la

EXTRA_DIST = \
	parrot.glade \
	parrot_strings.h

pkgdata_DATA = parrot.glade

AM_CFLAGS = \
	@PLUGIN_DEFAULT_FLAGS@

INCLUDES = \
	-I$(top_srcdir) \
	@COMMON_INCLUDES@

libparrot_zapping_la_SOURCES = parrot.c

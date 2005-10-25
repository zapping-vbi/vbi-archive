## Process this file with automake to produce Makefile.in

plugindir = @PLUGIN_DEFAULT_DIR@

plugin_LTLIBRARIES = libalirc.zapping.la

EXTRA_DIST = README.alirc

AM_CFLAGS = \
	@PLUGIN_DEFAULT_FLAGS@ \
	-DZAPPING8

INCLUDES = \
	-I$(top_srcdir) \
	@COMMON_INCLUDES@

libalirc_zapping_la_SOURCES = alirc.c

libalirc_zapping_la_LIBADD = $(LIRC_LIB)

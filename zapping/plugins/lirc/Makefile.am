## Process this file with automake to produce Makefile.in

plugindir = @PLUGIN_DEFAULT_DIR@

plugin_LTLIBRARIES = liblirc.zapping.la

EXTRA_DIST = README

AM_CFLAGS = \
	@PLUGIN_DEFAULT_FLAGS@

INCLUDES = \
	-I$(top_srcdir) \
	@COMMON_INCLUDES@

liblirc_zapping_la_SOURCES = \
	lirc.c lirc.h

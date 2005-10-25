## Process this file with automake to produce Makefile.in

plugindir = @PLUGIN_DEFAULT_DIR@

plugin_LTLIBRARIES = libtemplate.zapping.la

AM_CFLAGS = \
	@PLUGIN_DEFAULT_FLAGS@

INCLUDES = \
	-I$(top_srcdir) \
	@COMMON_INCLUDES@

libtemplate_zapping_la_SOURCES = \
	template.c

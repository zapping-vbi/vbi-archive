## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = liblirc.zapping.la

liblirc_zapping_la_SOURCES = lirc.c lirc.h

EXTRA_DIST = README

INCLUDES = -I$(top_srcdir) $(COMMON_INCLUDES) \
	$(PLUGIN_DEFAULT_FLAGS)


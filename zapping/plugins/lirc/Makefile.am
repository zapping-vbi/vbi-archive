## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = liblirc.zapping.la

liblirc_zapping_la_SOURCES = lirc.c

EXTRA_DIST = README

CFLAGS = $(PLUGIN_DEFAULT_FLAGS)
INCLUDES = \
	-I$(top_srcdir)/intl \
	-I$(includedir)	\
	$(GNOME_INCLUDEDIR) \
	-I$(top_srcdir)/src ## Let the plugin access Zapping headers
## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = libtemplate.zapping.la

libtemplate_zapping_la_SOURCES = template.c

CFLAGS = -Wall -Wno-unused # Avoid warnings about undeclared statics
INCLUDES = \
	-I$(top_srcdir)/intl \
	-I$(includedir)	\
	$(GNOME_INCLUDEDIR) \
	-I$(top_srcdir)/src ## Let the plugin access Zapping headers

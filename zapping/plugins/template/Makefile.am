## Process this file with automake to produce Makefile.in

plugindir = $(prefix)/share/zapping/plugins

plugin_LTLIBRARIES = libtemplate.zapping.la

libtemplate_zapping_la_SOURCES = template.c

INCLUDES = \
	-I$(top_srcdir)/intl \
	-I$(includedir)	\
	$(GNOME_INCLUDEDIR) \
	-I$(top_srcdir)/src ## Let the plugin access Zapping headers
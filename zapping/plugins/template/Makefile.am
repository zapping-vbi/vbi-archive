## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = libtemplate.zapping.la

libtemplate_zapping_la_SOURCES = template.c

CFLAGS = @CFLAGS@ $(PLUGIN_DEFAULT_FLAGS)

INCLUDES = \
$(X_CFLAGS) \
-I$(top_srcdir)/intl \
-I$(includedir)	\
$(GNOME_INCLUDEDIR) \
$(X_CFLAGS) \
-I$(top_srcdir)/src ## Let the plugin access Zapping headers

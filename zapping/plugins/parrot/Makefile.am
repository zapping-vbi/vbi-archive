## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = libparrot.zapping.la

EXTRA_DIST = parrot.glade parrot_strings.h

libparrot_zapping_la_SOURCES = parrot.c

CFLAGS = @CFLAGS@ $(PLUGIN_DEFAULT_FLAGS)

INCLUDES = \
-I$(top_srcdir)/intl \
-I$(includedir)	\
$(GNOME_INCLUDEDIR) \
$(X_CFLAGS) \
-I$(top_srcdir)/src ## Let the plugin access Zapping headers

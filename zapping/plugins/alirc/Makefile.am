## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = libalirc.zapping.la

EXTRA_DIST = README.alirc

libalirc_zapping_la_SOURCES = alirc.c
libalirc_zapping_la_LIBADD = $(LIRC_LIB)

CFLAGS = @CFLAGS@ $(PLUGIN_DEFAULT_FLAGS)

INCLUDES = \
$(X_CFLAGS) \
-I$(top_srcdir)/intl \
-I$(includedir)	\
$(GNOME_INCLUDEDIR) \
-I$(top_srcdir)/src ## Let the plugin access Zapping headers

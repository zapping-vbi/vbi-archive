## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = libalirc.zapping.la

EXTRA_DIST = README.alirc

libalirc_zapping_la_SOURCES = alirc.c
libalirc_zapping_la_LIBADD = $(LIRC_LIB)

CFLAGS = $(PLUGIN_DEFAULT_FLAGS)

INCLUDES = $(COMMON_INCLUDES) \
-I$(top_srcdir)/src ## Let the plugin access Zapping headers

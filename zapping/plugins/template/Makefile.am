## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = libtemplate.zapping.la

libtemplate_zapping_la_SOURCES = template.c

INCLUDES = -I$(top_srcdir) $(COMMON_INCLUDES) \
	$(PLUGIN_DEFAULT_FLAGS) @PG_CFLAGS@

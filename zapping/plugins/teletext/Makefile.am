## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = libteletext.zapping.la

libteletext_zapping_la_SOURCES = \
	bookmark.c bookmark.h \
	color.c color.h \
	export.c export.h \
	main.c main.h \
	search.c search.h \
	toolbar.c toolbar.h \
	view.c view.h \
	window.c window.h

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src $(COMMON_INCLUDES) \
	$(PLUGIN_DEFAULT_FLAGS) -Wunused-function

## Process this file with automake to produce Makefile.in

plugindir = @PLUGIN_DEFAULT_DIR@

plugin_LTLIBRARIES = libteletext.zapping.la

libteletext_zapping_la_SOURCES = \
	bookmark.c bookmark.h \
	export.c export.h \
	main.c main.h \
	page_num.h \
	preferences.c preferences.h \
	search.c search.h \
	toolbar.c toolbar.h \
	view.c view.h \
	window.c window.h

INCLUDES = -I$(top_srcdir) \
	@COMMON_INCLUDES@ \
	@PLUGIN_DEFAULT_FLAGS@ \
	@PG_CFLAGS@ \
	-Wunused-function \
	-DZAPPING8

libteletext_zapping_la_LIBADD = \
	$(top_builddir)/libvbi/libvbi.la \
	$(PTHREAD_LIB) $(PNG_LIB) $(UNICODE_LIBS)

# must prevent link conflict with libzvbi 0.2
great_rename:
	for file in *.c *.h; do \
	  sed "s/vbi_/vbi3_/g;s/VBI_/VBI3_/g" <$$file >tmp; \
	  mv tmp $$file; \
	done

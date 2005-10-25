## Process this file with automake to produce Makefile.in

plugindir = @PLUGIN_DEFAULT_DIR@

plugin_LTLIBRARIES = libteletext.zapping.la

AM_CFLAGS = \
	@PLUGIN_DEFAULT_FLAGS@ \
	-Wunused-function \
	-DZAPPING8

INCLUDES = \
	-I$(top_srcdir) \
	@COMMON_INCLUDES@

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

libteletext_zapping_la_LIBADD = \
	$(top_builddir)/libvbi/libvbi.la \
	$(PTHREAD_LIB) \
	$(PNG_LIB) \
	$(UNICODE_LIBS)

# Must prevent link conflict with libzvbi 0.2.
great_rename:
	for file in *.c *.h; do \
	  sed "s/vbi_/vbi3_/g;s/VBI_/VBI3_/g" <$$file >tmp; \
	  mv tmp $$file; \
	done

great_emaner:
	for file in *.c *.h; do \
	  sed "s/vbi3_/vbi_/g;s/VBI3_/VBI_/g" <$$file >tmp; \
	  mv tmp $$file; \
	done

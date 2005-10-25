## Process this file with automake to produce Makefile.in

plugindir = @PLUGIN_DEFAULT_DIR@

plugin_LTLIBRARIES = libsubtitle.zapping.la

AM_CFLAGS = \
	@PLUGIN_DEFAULT_FLAGS@ \
	-DZAPPING8

INCLUDES = \
	-I$(top_srcdir) \
	@COMMON_INCLUDES@

libsubtitle_zapping_la_SOURCES = \
	view.c view.h \
	main.c main.h \
	preferences.c preferences.h

libsubtitle_zapping_la_LIBADD = \
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

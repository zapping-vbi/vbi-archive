## Process this file with automake to produce Makefile.in

plugindir = @PLUGIN_DEFAULT_DIR@

plugin_LTLIBRARIES = libsubtitle.zapping.la

libsubtitle_zapping_la_SOURCES = \
	view.c view.h \
	main.c main.h \
	preferences.c preferences.h

INCLUDES = -I$(top_srcdir) \
	@COMMON_INCLUDES@ \
	@PLUGIN_DEFAULT_FLAGS@ \
	-DZAPPING8

libsubtitle_zapping_la_LIBADD = \
	$(top_builddir)/libvbi/libvbi.la \
	$(PTHREAD_LIB) $(PNG_LIB) $(UNICODE_LIBS)

# must prevent link conflict with libzvbi 0.2
great_rename:
	for file in *.c *.h; do \
	  sed "s/vbi_/vbi3_/g;s/VBI_/VBI3_/g" <$$file >tmp; \
	  mv tmp $$file; \
	done

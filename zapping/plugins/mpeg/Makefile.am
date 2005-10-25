## Process this file with automake to produce Makefile.in

plugindir = @PLUGIN_DEFAULT_DIR@

plugin_LTLIBRARIES = libmpeg.zapping.la

PIXMAPS = \
	record.png \
	pause.png \
	stop.png \
	disk_empty.png \
	drop.png \
	time.png \
	volume.png

BUILT_SOURCES = pixmaps.h

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	$(PIXMAPS) \
	mpeg_properties.glade2

pkgdata_DATA = mpeg_properties.glade2

pixmaps.h: $(PIXMAPS)
	echo -e "\n/* Generated file, do not edit! */\n" > $@
	for fi in $(PIXMAPS); do \
	  @GDK_PIXBUF_CSOURCE@ --struct --build-list \
	    `echo $$fi | sed -e 's%[^[:alnum:]]%_%g'` $(srcdir)/$$fi >> $@; \
	done

AM_CFLAGS = \
	@PLUGIN_DEFAULT_FLAGS@ \
	-DZAPPING8

INCLUDES = \
	-I$(top_srcdir) \
	@COMMON_INCLUDES@

libmpeg_zapping_la_SOURCES = \
	mpeg.c mpeg.h \
	options.c \
	pixmaps.h

libmpeg_zapping_la_LIBADD = $(RTE_LIBS)

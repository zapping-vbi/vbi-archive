## Process this file with automake to produce Makefile.in

plugindir = $(PLUGIN_DEFAULT_DIR)

plugin_LTLIBRARIES = libmpeg.zapping.la

libmpeg_zapping_la_SOURCES = mpeg.c options.c mpeg.h
libmpeg_zapping_la_LIBADD = $(RTE_LIB)

pkgdata_DATA = mpeg_properties.glade2

CFLAGS = $(PLUGIN_DEFAULT_FLAGS)

INCLUDES = \
$(COMMON_INCLUDES) \
-I$(top_srcdir)/src ## Let the plugin access Zapping headers

PIXMAPS = \
	record.png pause.png stop.png \
	disk_empty.png drop.png time.png volume.png

EXTRA_DIST = mpeg_properties.glade2 mpeg_strings.c $(PIXMAPS)
BUILT_SOURCES = pixmaps.h
noinst_HEADERS = $(BUILT_SOURCES)
CLEANFILES = $(BUILT_SOURCES)

pixmaps.h: @GDK_PIXBUF_CSOURCE@

pixmaps.h: $(PIXMAPS)
	echo -e "\n/* Generated file, do not edit! */\n" > $@
	for fi in $(PIXMAPS); do \
	  @GDK_PIXBUF_CSOURCE@ --struct --build-list \
	    `echo $$fi | sed -e 's%[^[:alnum:]]%_%g'` $(srcdir)/$$fi >> $@; \
	done

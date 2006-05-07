## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libvideo.la

INCLUDES = -D_GNU_SOURCE -D_REENTRANT -include $(top_builddir)/config.h \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

if USE_SSE2
SSE2_FILES = motion_sse2.s
else
SSE2_FILES =
endif

BUILT_SOURCES = vlc_tables.h

EXTRA_DIST = \
	vlc_gen.c \
	$(BUILT_SOURCES)

MAINTAINERCLEANFILES = \
	vlc_gen

libvideo_la_SOURCES = \
	dct.c dct_mmx.s dct.h dct_ieee.h dct_ref.c \
	filter.c filter_mmx.s \
	mblock.c mblock.h ratectl.h \
	mpeg1.c mpeg2.c mpeg.h video.h libvideo.h \
	tables.c vlc.c vlc_mmx.s vlc.h vlc_tables.h \
	motion.c motion_mmx.s motion.h \
	$(SSE2_FILES)

if MAINTAINER_MODE

vlc_tables.h: vlc_gen
	./vlc_gen <$^ >$@

endif

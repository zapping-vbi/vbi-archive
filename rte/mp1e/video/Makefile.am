noinst_LTLIBRARIES = libvideo.la

INCLUDES = -D_GNU_SOURCE -D_REENTRANT

if USE_SSE2
SSE2_FILES = motion_sse2.s
else
SSE2_FILES =
endif

BUILT_SOURCES = vlc_tables.h
EXTRA_DIST = vlc_gen.c

CLEANFILES = vlc_tables.h vlc_gen

libvideo_la_SOURCES = \
	dct.c dct_mmx.s dct.h dct_ieee.h dct_ref.c \
	filter.c filter_mmx.s \
	mblock.c mblock.h \
	mpeg1.c mpeg.h video.h libvideo.h \
	tables.c vlc.c vlc_mmx.s vlc.h \
	motion.c motion_mmx.s motion.h \
	$(SSE2_FILES)

vlc_tables.h: vlc_gen.c
	$(CC) $^ -D_GNU_SOURCE -Wall -o vlc_gen
	./vlc_gen <$^ >$@

noinst_LTLIBRARIES = libvideo_rte.la libvideo.la

INCLUDES=-D_GNU_SOURCE -D_REENTRANT

if USE_SSE2
SSE2_FILES = motion_sse2.s
else
SSE2_FILES =
endif

libvideo_rte_la_SOURCES = \
	dct.c dct_mmx.s dct.h dct_ieee.h dct_ref.c \
	filter.c filter_mmx.s \
	mblock.c mblock.h \
	mpeg1.c mpeg.h libvideo.h \
	tables.c vlc.c vlc_mmx.s vlc.h \
	motion.c motion_mmx.s motion.h \
	$(SSE2_FILES)

libvideo_la_SOURCES = \
	dct.c dct_mmx.s dct.h dct_ieee.h dct_ref.c \
	filter.c filter_mmx.s \
	mblock.c mblock.h \
	mpeg1.c mpeg.h video.h libvideo.h \
	tables.c vlc.c vlc_mmx.s vlc.h \
	motion.c motion_mmx.s motion.h \
	file.c v4l.c v4l2.c \
	$(SSE2_FILES)

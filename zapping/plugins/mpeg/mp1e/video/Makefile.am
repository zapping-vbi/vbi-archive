noinst_LTLIBRARIES = libvideo_rte.la libvideo.la

INCLUDES=-D_GNU_SOURCE -D_REENTRANT

libvideo_rte_la_SOURCES = \
	dct.c dct_mmx.s dct.h \
	dct_ref.c \
	filter.c filter_mmx.s \
	mblock.c \
	mpeg1.c mpeg.h \
	tables.c predict.h \
	vlc.c vlc_mmx.s vlc.h \
	motion_mmx.s

libvideo_la_SOURCES = \
	dct.c dct_mmx.s dct.h \
	dct_ref.c \
	filter.c filter_mmx.s \
	mblock.c \
	mpeg1.c mpeg.h \
	tables.c predict.h \
	vlc.c vlc_mmx.s vlc.h \
	motion_mmx.s \
	file.c v4l.c v4l2.c video.h

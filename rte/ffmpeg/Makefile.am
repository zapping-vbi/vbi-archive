## Process this file with automake to produce Makefile.in

SUBDIRS = libav libavcodec

noinst_LTLIBRARIES = libffmpeg.la

EXTRA_DIST = README

INCLUDES= -D_REENTRANT -D_GNU_SOURCE -include $(top_builddir)/config.h \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

libffmpeg_la_SOURCES = \
	bswap.h mangle.h \
	b_ffmpeg.c b_ffmpeg.h

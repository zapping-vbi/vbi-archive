## Process this file with automake to produce Makefile.in

SUBDIRS = libav libavcodec

EXTRA_DIST = README

INCLUDES= -D_REENTRANT -D_GNU_SOURCE \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations
# -I$(srcdir)/libavcodec

noinst_LTLIBRARIES = libffmpeg.la

libffmpeg_la_SOURCES = \
	bswap.h mangle.h \
	b_ffmpeg.c b_ffmpeg.h

## Process this file with automake to produce Makefile.in

SUBDIRS = libav libavcodec

EXTRA_DIST = README

INCLUDES= -D_REENTRANT -D_GNU_SOURCE -Ilibavcodec

noinst_LTLIBRARIES = libffmpeg.la

libffmpeg_la_SOURCES = config.h bswap.h b_ffmpeg.c

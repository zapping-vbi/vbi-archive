## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libav.la

INCLUDES = -D_REENTRANT -DHAVE_AV_CONFIG_H \
	-Wall -Wunused -Wmissing-declarations

libav_la_SOURCES = \
	asf.c au.c avformat.h avi.h avidec.c avienc.c avio.c \
	avio.h aviobuf.c ffm.c file.c gif.c img.c jpeg.c \
	mov.c mpeg.c raw.c rm.c swf.c tick.h \
	utils.c wav.c

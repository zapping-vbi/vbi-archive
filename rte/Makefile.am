## Process this file with automake to produce Makefile.in

SUBDIRS = mp1e
## ffmpeg

lib_LTLIBRARIES = librte.la

noinst_PROGRAMS = rte_test

INCLUDES = -D_GNU_SOURCE -D_REENTRANT -D_LARGEFILE64_SOURCE

EXTRA_DIST = rte.spec.in rte.spec rte.sym

## ffmpeg = ffmpeg/libffmpeg.la ffmpeg/libav/libav.la \
## ffmpeg/libavcodec/libavcodec.la ffmpeg/libavcodec/i386/libi386.la \
## ffmpeg/libavcodec/libac3/libac3.la ffmpeg/libavcodec/mpglib/libmpglib.la

mp1e = mp1e/video/libvideo_rte.la mp1e/audio/libaudio_rte.la \
mp1e/systems/libsystems_rte.la mp1e/common/libcommon.la \
mp1e/libmp1e.la

librteincludedir = $(includedir)
librteinclude_HEADERS = rte.h
librte_la_SOURCES = rte.c rte.h rtepriv.h
librte_la_LIBADD = $(mp1e) -lpthread -lm
## librte_la_LIBADD = $(ffmpeg) $(mp1e) -lpthread -lm
librte_la_LDFLAGS = -export-dynamic -release @VERSION@ -export-symbols rte.sym

rte_test_SOURCES = rte_test_main.c

rte_test_INCLUDES = $(ESD_CFLAGS)
rte_test_LDADD = $(ESD_LIBS) -lpthread -lm .libs/librte.a

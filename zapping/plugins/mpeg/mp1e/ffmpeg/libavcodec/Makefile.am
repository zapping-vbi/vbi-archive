## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libavcodec.la

SUBDIRS = i386 libac3 mpglib

EXTRA_DIST = apiexample.c dct-test.c

INCLUDES=-D_REENTRANT

libavcodec_la_SOURCES = ac3dec.c ac3enc.c ac3enc.h ac3tab.h avcodec.h \
common.c common.h dsputil.c dsputil.h fdctref.c h263.c h263data.h \
h263dec.c imgconvert.c imgresample.c jfdctfst.c jrevdct.c mjpegenc.c \
motion_est.c mpeg12.c mpeg12data.h mpeg4data.h mpegaudio.c mpegaudio.h \
mpegaudiodec.c mpegaudiotab.h mpegvideo.c mpegvideo.h msmpeg4.c \
msmpeg4data.h resample.c rv10.c utils.c

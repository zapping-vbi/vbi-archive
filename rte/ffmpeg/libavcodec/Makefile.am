## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libavcodec.la

SUBDIRS = i386

INCLUDES = -D_REENTRANT -I. -DHAVE_AV_CONFIG_H

### using builtin liba52 or runtime linked liba52.so.0
##ifeq ($(CONFIG_A52BIN),no)
##OBJS+= liba52/bit_allocate.o liba52/bitstream.o liba52/downmix.o \
##	liba52/imdct.o  liba52/parse.o
##endif
##
##ifeq ($(CONFIG_MP3LAME),yes)
##OBJS += mp3lameaudio.o
##EXTRALIBS += -lmp3lame
##endif
##
### i386 mmx specific stuff
##ifeq ($(TARGET_MMX),yes)
##OBJS += i386/fdct_mmx.o i386/cputest.o \
##	i386/dsputil_mmx.o i386/mpegvideo_mmx.o \
##	i386/idct_mmx.o i386/motion_est_mmx.o \
##	i386/simple_idct_mmx.o
##endif
##
### armv4l specific stuff
##ifeq ($(TARGET_ARCH_ARMV4L),yes)
##ASM_OBJS += armv4l/jrevdct_arm.o
##OBJS += armv4l/dsputil_arm.o
##endif
##
##
### alpha specific stuff
##ifeq ($(TARGET_ARCH_ALPHA),yes)
##OBJS += alpha/dsputil_alpha.o alpha/mpegvideo_alpha.o
##CFLAGS += -Wa,-mpca56
##endif
##
### cpuid test
##cpuid_test: i386/cputest.c
##	$(CC) $(CFLAGS) -D__TEST__ -o $@ $<
##

libavcodec_la_SOURCES = \
ac3enc.c ac3enc.h ac3tab.h avcodec.c avcodec.h \
common.c common.h dsputil.c dsputil.h fastmemcpy.h \
fdctref.c h263.c h263data.h imgconvert.c imgresample.c \
jfdctfst.c jrevdct.c mangle.h mjpeg.c motion_est.c \
mpeg12.c mpeg12data.h mpeg4data.h mpegaudio.c \
mpegaudio.h mpegaudiodec.c mpegaudiodectab.h mpegaudiotab.h \
mpegvideo.c mpegvideo.h msmpeg4.c msmpeg4data.h pcm.c ratecontrol.c \
resample.c rv10.c simple_idct.c simple_idct.h utils.c

libavcodec_la_LIBADD = i386/libi386.la

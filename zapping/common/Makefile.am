noinst_LIBRARIES = libcommon.a

EXTRA_DIST = README

INCLUDES = $(DEFAULT_CFLAGS) $(UNICODE_CFLAGS)

#BUILT_SOURCES = fprintf_videodev.h
#
#fprintf_videodev.h: dump_gen videodev.h
#	./dump_gen videodev.h \
#	video_device.type VFL_TYPE_ video_device.hardware VID_HARDWARE_ \
#	video_capability.type VID_TYPE_ video_channel.flags VIDEO_VC_ \
#	video_channel.type VIDEO_TYPE_ video_tuner.flags VIDEO_TUNER_ \
#	video_tuner.mode VIDEO_MODE_ video_picture.palette VIDEO_PALETTE_ \
#	video_audio.flags VIDEO_AUDIO_ video_audio.mode VIDEO_SOUND_ \
#	video_window.flags VIDEO_WINDOW_ video_capture.flags VIDEO_CAPTURE_ \
#	vbi_format.flags VBI_ > $@

libcommon_a_SOURCES = \
	fifo.h fifo.c \
	alloc.h alloc.c \
	threads.h \
	list.h \
	types.h \
	math.h \
	ure.c ure.h \
	unicode.c ucs-2.h \
	videodev.h videodev2.h

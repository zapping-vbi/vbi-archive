## Process this file with automake to produce Makefile.in

SUBDIRS = audio common systems test video

bin_PROGRAMS = mp1e

INCLUDES=-D_GNU_SOURCE -D_REENTRANT

EXTRA_DIST = videodev1.h videodev2.h mp1e.lsm.in

mp1e_SOURCES = main.c \
	options.c options.h \
	rte.c rte.h rtepriv.h \
	convert_ry.c convert_ry.h

mp1e_LDADD = video/libvideo.a audio/libaudio.a systems/libsystems.a \
common/libcommon.a $(TEST_MODULES) $(X_LIBS) -lm -lpthread

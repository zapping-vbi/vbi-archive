## Process this file with automake to produce Makefile.in

SUBDIRS = video audio vbi common systems test

bin_PROGRAMS = mp1e rte_test

INCLUDES=-D_GNU_SOURCE -D_REENTRANT

EXTRA_DIST = videodev1.h videodev2.h mp1e.lsm.in

mp1e_SOURCES = main.c \
	options.c global_data.c options.h

mp1e_LDADD = video/libvideo.a audio/libaudio.a vbi/libvbi.a \
systems/libsystems.a common/libcommon.a test/libtest.a $(X_LIBS) -lm \
-lpthread $(ESD_LIBS)

rte_test_SOURCES = rte_test_main.c \
		global_data.c options.h \
		rte.c rte.h rtepriv.h \
		convert_ry.c convert_ry.h

rte_test_LDADD = video/libvideo_rte.a audio/libaudio_rte.a \
systems/libsystems_rte.a common/libcommon.a test/libtest.a $(X_LIBS) \
$(ESD_LIBS) -lm -lpthread

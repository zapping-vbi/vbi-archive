## Process this file with automake to produce Makefile.in

SUBDIRS = video audio vbi common systems test

lib_LTLIBRARIES = librte.la

bin_PROGRAMS = mp1e rte_test

INCLUDES=-D_GNU_SOURCE -D_REENTRANT

EXTRA_DIST = videodev1.h videodev2.h mp1e.lsm.in

librteincludedir = $(includedir)
librteinclude_HEADERS = rte.h
librte_la_SOURCES = global_data.c options.h \
		rte.c rte.h rtepriv.h \
		convert_ry.c convert_ry.h
librte_la_LIBADD = video/libvideo_rte.la audio/libaudio_rte.la \
systems/libsystems_rte.la common/libcommon.la -lpthread -lm
librte_la_LDFLAGS = -export-dynamic

mp1e_SOURCES = main.c \
	options.c global_data.c options.h

mp1e_LDADD = video/libvideo.la audio/libaudio.la vbi/libvbi.la \
systems/libsystems.la common/libcommon.la test/libtest.la $(X_LIBS) -lm \
-lpthread $(ESD_LIBS)

rte_test_SOURCES = rte_test_main.c

rte_test_LDADD = $(ESD_LIBS) -lpthread librte.la
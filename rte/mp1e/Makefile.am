## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.9 2002-06-12 04:01:05 mschimek Exp $

INCLUDES = -D_GNU_SOURCE -D_REENTRANT

EXTRA_DIST = \
	common/videodev.h common/videodev2.h \
	mp1e.lsm.in mp1e.spec.in $(man_MANS) BUGS \
	context.h codec.h rte.h option.h rtepriv.h

if BACKEND_MP1E

SUBDIRS = video audio systems common

noinst_LTLIBRARIES = libmp1e.la
libmp1e_la_SOURCES = b_mp1e.c b_mp1e.h

else

SUBDIRS = video audio vbi systems common devices test

bin_PROGRAMS = mp1e
mp1e_SOURCES = main.c options.c global_data.c options.h
mp1e_LDADD = \
	systems/libsystems.la \
	audio/libaudio.la vbi/libvbi.la \
	devices/libdevices.la video/libvideo.la common/libcommon.la \
	test/libtest.la $(X_LIBS) -lm -lpthread $(ESD_LIBS)
man_MANS = mp1e.1

endif

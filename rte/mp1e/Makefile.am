## Process this file with automake to produce Makefile.in

SUBDIRS = video audio vbi common systems test

noinst_LTLIBRARIES = libmp1e.la

bin_PROGRAMS = mp1e

INCLUDES = -D_GNU_SOURCE -D_REENTRANT

man_MANS = mp1e.1

## We don't include mp1e.spec any more in the distro, use
## the prepare_dist script to generate an rpm.
EXTRA_DIST = common/videodev.h common/videodev2.h \
mp1e.lsm.in mp1e.spec.in $(man_MANS) BUGS \
context.h codec.h rte.h option.h rtepriv.h

libmp1e_la_SOURCES = b_mp1e.c b_mp1e.h

mp1e_SOURCES = main.c options.c global_data.c options.h

mp1e_LDADD = video/libvideo.la audio/libaudio.la vbi/libvbi.la \
systems/libsystems.la common/libcommon.la test/libtest.la $(X_LIBS) -lm \
-lpthread $(ESD_LIBS)
## -lmmxemu

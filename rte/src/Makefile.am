## Process this file with automake to produce Makefile.in

INCLUDES = -D_GNU_SOURCE -D_REENTRANT -D_LARGEFILE64_SOURCE

EXTRA_DIST = rte-version.h.in rte.sym

if BACKEND_FFMPEG
ffmpeg = $(top_srcdir)/ffmpeg
ffmpeg_backend = ffmpeg.c
ffmpeg_libs = $(ffmpeg)/libffmpeg.la $(ffmpeg)/libav/libav.la \
$(ffmpeg)/libavcodec/libavcodec.la $(ffmpeg)/libavcodec/i386/libi386.la \
$(ffmpeg)/libavcodec/libac3/libac3.la \
$(ffmpeg)/libavcodec/mpglib/libmpglib.la
endif

if BACKEND_MP1E
mp1e = $(top_srcdir)/mp1e
mp1e_backend = mp1e.c
mp1e_libs = $(mp1e)/video/libvideo_rte.la \
$(mp1e)/audio/libaudio_rte.la \
$(mp1e)/systems/libsystems_rte.la $(mp1e)/common/libcommon.la \
$(mp1e)/libmp1e.la
endif

if BACKEND_DIVX4LINUX
divx4linux = $(top_srcdir)/divx4linux
divx4linux_backend = divx4linux.c
divx4linux_libs =
endif

lib_LTLIBRARIES = librte.la

librteincludedir = $(includedir)/rte
librteinclude_HEADERS = rte.h rte-enums.h rte-types.h rte-version.h
librte_la_SOURCES = rte.c rte.h rte-enums.h rte-types.h rte-version.h \
rtepriv.h $(mp1e_backend) $(ffmpeg_backend) $(divx4linux_backend)
librte_la_LIBADD = -lpthread -lm ../common/libcommon.la \
$(ffmpeg_libs) $(mp1e_libs) $(divx4linux_libs)
librte_la_LDFLAGS = -export-dynamic -release @VERSION@ -export-symbols rte.sym

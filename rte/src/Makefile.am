## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = librte.la

EXTRA_DIST = rte-version.h.in

INCLUDES = -D_GNU_SOURCE -D_REENTRANT -D_LARGEFILE64_SOURCE

if BACKEND_MP1E
mp1e = $(top_srcdir)/mp1e
mp1e_libs = $(mp1e)/video/libvideo_rte.la \
	$(mp1e)/audio/libaudio_rte.la \
	$(mp1e)/systems/libsystems_rte.la \
	$(mp1e)/common/libcommon.la \
	$(mp1e)/libmp1e.la
endif

if BACKEND_FFMPEG
ffmpeg = $(top_srcdir)/ffmpeg
ffmpeg_libs = $(ffmpeg)/libffmpeg.la \
	$(ffmpeg)/libav/libav.la \
	$(ffmpeg)/libavcodec/libavcodec.la \
	$(ffmpeg)/libavcodec/i386/libi386.la \
	$(ffmpeg)/libavcodec/libac3/libac3.la \
	$(ffmpeg)/libavcodec/mpglib/libmpglib.la
endif

if BACKEND_DIVX4LINUX
divx4linux = $(top_srcdir)/divx4linux
divx4linux_libs =
endif

BUILT_SOURCES = rte.sym
rte.sym: rte.h
	sed -n '/extern.*/!d;s/\(.*\)\(rte.*\)(\(.*\)/\2/p' <rte.h > $@

librte_la_SOURCES = \
	rte.c rte.h \
	context.c codec.c \
	rte-enums.h rte-types.h rte-version.h \
	rtepriv.h

librteincludedir = $(includedir)/rte
librteinclude_HEADERS = rte.h rte-enums.h rte-types.h rte-version.h

librte_la_LIBADD = -lpthread -lm \
	$(ffmpeg_libs) $(mp1e_libs) $(divx4linux_libs)

librte_la_LDFLAGS = -export-dynamic -export-symbols rte.sym \
	-release @VERSION@ -version-info 1:0:0

## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = librte.la

EXTRA_DIST = 

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
	context.c context.h \
	codec.c codec.h \
	option.h rtepriv.h

librteincludedir = $(includedir)/rte
librteinclude_HEADERS = librte.h
# rte.h rte-enums.h rte-types.h rte-version.h

librte_la_LIBADD = -lpthread -lm \
	$(ffmpeg_libs) $(mp1e_libs) $(divx4linux_libs)

librte_la_LDFLAGS = -export-dynamic -export-symbols rte.sym \
	-release @VERSION@ -version-info 1:0:0

# Concat the header files in this order and filter out all lines
#   before "^/* Public", after "^/* Private" and between "^/**"..."**/$"
librte.h: option.h context.h codec.h rte.h
	sed -ne "1,/\*\//p" rte.h > $@
	echo -e "\n/* Generated file, do not edit! */" >> $@
	echo -e "\n#ifndef __LIBRTE_H__\n#define __LIBRTE_H__" >> $@
	echo -e "\n#define RTE_VERSION_MAJOR" `echo @VERSION@ \
	 | sed "s/\./\\\\\n#define RTE_VERSION_MINOR /" \
	 | sed "s/\./\\\\\n#define RTE_VERSION_MICRO /"` >> $@
	echo -e "\n#ifdef __cplusplus\nextern "C" {\n#endif\n" >> $@
	for fi in $^; do \
		echo "/* $$fi */" >> $@; \
		sed -e "/^\/\* Public/,/^\/\* Private/!d;/^\/\* Public/d;/^\/\* Private/d;/^\/\*\*/,/\*\*\/$$/d;" $$fi >> $@; \
	done
	echo -e "\n#ifdef __cplusplus\n}\n#endif" >> $@
	echo -e "\n#endif /* __LIBRTE_H__ */" >> $@

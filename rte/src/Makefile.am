## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = librte.la

EXTRA_DIST = rte.sym

INCLUDES = -Wall -Wunused -Wmissing-prototypes -Wmissing-declarations
## -D_LARGEFILE64_SOURCE to enable lseek64, off64_t not portable, sigh.
INCLUDES += -D_REENTRANT

if BACKEND_MP1E
mp1e = ../mp1e
mp1e_libs = \
	$(mp1e)/video/libvideo.la \
	$(mp1e)/audio/libaudio.la \
	$(mp1e)/systems/libsystems.la \
	$(mp1e)/common/libcommon.la \
	$(mp1e)/libmp1e.la
endif

if BACKEND_FFMPEG
ffmpeg = ../ffmpeg
ffmpeg_libs = \
	$(ffmpeg)/libffmpeg.la \
	$(ffmpeg)/libav/libav.la \
	$(ffmpeg)/libavcodec/libavcodec.la
endif

if BACKEND_DIVX4LINUX
divx4linux = ../divx4linux
divx4linux_libs = \
	$(divx4linux)/libdivx4linux.la
endif

librte_la_SOURCES = \
	rte.c rte.h \
	context.c context.h \
	codec.c codec.h \
	option.h rtepriv.h

librteincludedir = $(includedir)
librteinclude_HEADERS = librte.h

librte_la_LIBADD = $(PTHREAD_LIB) $(CCMALLOC) \
	$(LTLIBICONV) $(LTLIBINTL) \
	$(ffmpeg_libs) $(mp1e_libs) $(divx4linux_libs)

librte_la_LDFLAGS = -export-dynamic -export-symbols rte.sym \
	-version-info $(LIBRTE_SO_VERSION)

BUILT_SOURCES = rte.sym librte.h

rte.sym: librte.h
	sed -n '/extern.*/!d;s/\(.*\)\(rte.*\)(\(.*\)/\2/p' <librte.h >$@

LIBRTE_HDRS = option.h context.h codec.h rte.h

# Concat the header files in this order and filter out all lines
#   before "^/* Public", after "^/* Private" and dox between "^/**"..."*/$"
librte.h: $(LIBRTE_HDRS)
	sed -ne "1,/\*\//p" $(srcdir)/rte.h > $@
	echo -e "\n/* Generated file, do not edit! */" >> $@
	echo -e "\n#ifndef __LIBRTE_H__\n#define __LIBRTE_H__" >> $@
	echo -e "\n#define RTE_VERSION_MAJOR" `echo @VERSION@ \
	 | sed "s/\./\\\\\n#define RTE_VERSION_MINOR /" \
	 | sed "s/\./\\\\\n#define RTE_VERSION_MICRO /"` >> $@
	echo -e "\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n" >> $@
	for fi in $(LIBRTE_HDRS); do \
		echo "/* $$fi */" >> $@; \
		sed -e "/^\/\* Public/,/^\/\* Private/!d;/^\/\* Public/d;/^\/\* Private/d;s/\(\/\*\*.*\*\/\)//;/^[ 	]*\/\*\*/,/\*\/[ 	]*$$/d;" $(srcdir)/$$fi >> $@; \
	done
	echo -e "\n#ifdef __cplusplus\n}\n#endif" >> $@
	echo -e "\n#endif /* __LIBRTE_H__ */" >> $@

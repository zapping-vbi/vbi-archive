## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = librte.la

librte_la_LIBADD = \
	$(PTHREAD_LIB) \
	$(INTLLIBS)

librte_la_LDFLAGS = \
	-export-dynamic \
	-export-symbols $(srcdir)/rte.sym \
	-version-info $(LIBRTE_SO_VERSION)

if BACKEND_MP1E
librte_la_LIBADD += \
	$(top_builddir)/mp1e/video/libvideo.la \
	$(top_builddir)/mp1e/audio/libaudio.la \
	$(top_builddir)/mp1e/systems/libsystems.la \
	$(top_builddir)/mp1e/common/libcommon.la \
	$(top_builddir)/mp1e/libmp1e.la
endif

if BACKEND_FFMPEG
librte_la_LIBADD += \
	$(top_builddir)/ffmpeg/libffmpeg.la \
	$(top_builddir)/ffmpeg/libav/libav.la \
	$(top_builddir)/ffmpeg/libavcodec/libavcodec.la
endif

if BACKEND_DIVX4LINUX
librte_la_LIBADD += \
	$(top_builddir)/divx4linux/libdivx4linux.la
endif

# -D_LARGEFILE64_SOURCE: enable lseek64, off64_t if possible.
INCLUDES = \
	-D_REENTRANT \
	-D_LARGEFILE64_SOURCE

EXTRA_DIST = rte.sym

librte_la_SOURCES = \
	rte.c rte.h \
	context.c context.h \
	codec.c codec.h \
	option.h rtepriv.h \
	rte.sym

librteincludedir = $(includedir)
librteinclude_HEADERS = librte.h

BUILT_SOURCES = \
	librte.h \
	rte.sym

rte.sym: librte.h
	sed -n '/extern.*/!d;s/\(.*\)\(rte.*\)(\(.*\)/\2/p' <$^ >$@

# Keep order.
LIBRTE_HDRS = \
	option.h \
	context.h \
	codec.h \
	rte.h

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

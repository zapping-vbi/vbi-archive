## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = libzvbi.la

BUILT_SOURCES = version.h libzvbi.h wstfont2.xbm ccfont3.xbm
EXTRA_DIST = wstfont2.pbm ccfont3.pbm
CLEANFILES = $(BUILT_SOURCES)

INCLUDES = \
	-W -Wall -Wunused -Wfloat-equal \
	-Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align \
	-Wwrite-strings -Winline -Wmissing-prototypes \
	-Wmissing-declarations
	-Wpadded -Wconversion 
INCLUDES += -D_REENTRANT -D_XOPEN_SOURCE -D_BSD_SOURCE
INCLUDES += $(X_CFLAGS) $(UNICODE_CFLAGS)

libzvbi_la_SOURCES = \
	aspect_ratio.h \
	bcd.c bcd.h \
	bit_slicer.c bit_slicer.h \
	cache.c cache.h \
	caption.c cc.h \
	conv.c conv.h \
	dlist.h \
	event.h \
	export.c export.h \
	exp-gfx.c exp-gfx.h \
	exp-html.c \
	exp-templ.c \
	exp-txt.c exp-txt.h \
	exp-vtx.c \
	format.h \
	graphics.c graphics.h \
	hamm.c hamm.h \
	intl.h intl-priv.h \
	io.c io.h \
	io-bktr.c \
	io-dvb.h \
	io-sim.c io-sim.h \
	io-v4l2k.c videodev2k.h \
	io-v4l2.c videodev2.h \
	io-v4l.c videodev.h \
	lang.c lang.h \
	link.h \
	macros.h \
	misc.h \
	network.c network.h \
	packet.c \
	packet-830.c packet-830.h \
	pdc.c pdc.h \
	pfc_demux.c pfc_demux.h \
	raw_decoder.c raw_decoder.h \
	sampling.c sampling.h \
	search.c search.h \
	sliced.h \
	tables.c tables.h \
	teletext.c \
	trigger.c trigger.h \
	ure.c ure.h \
	vbi.c vbi.h \
	vps.c vps.h \
	vt.h \
	wss.c wss.h \
	xds_demux.c xds_demux.h \
	xds_decoder.c xds_decoder.h

exp-gfx.c: wstfont2.xbm ccfont3.xbm

wstfont2.xbm: wstfont2.pbm fontgen Makefile.in
	$(top_builddir)/src/fontgen xbm wstfont2 10 < $< > $@
ccfont3.xbm: ccfont3.pbm fontgen Makefile.in
	$(top_builddir)/src/fontgen xbm ccfont3 13 < $< > $@

libzvbiincludedir = $(includedir)
libzvbiinclude_HEADERS = libzvbi.h

libzvbi_la_LIBADD = $(PTHREAD_LIB) $(PNG_LIB) $(UNICODE_LIBS)
libzvbi_la_LDFLAGS = -version-info $(LIBZVBI_SO_VERSION)

version.h: Makefile.in
	echo -e "\n#define VBI_VERSION_MAJOR" `echo @VERSION@ \
	 | sed "s/[^0-9.]//g" \
	 | sed "s/\./\\\\\n#define VBI_VERSION_MINOR /" \
	 | sed "s/\./\\\\\n#define VBI_VERSION_MICRO /"` > $@

LIBZVBI_HDRS = \
	macros.h intl.h \
	bcd.h hamm.h network.h link.h aspect_ratio.h event.h \
	format.h pdc.h lang.h export.h \
	cache.h search.h \
	sliced.h \
	graphics.h \
	sampling.h \
	bit_slicer.h \
	raw_decoder.h \
	io.h \
	io-sim.h \
	io-dvb.h \
	conv.h \
	exp-gfx.h \
	exp-txt.h \
	xds_demux.h xds_decoder.h cc.h vps.h pfc_demux.h \
	packet-830.h vt.h tables.h vbi.h

# Concat the header files in this order and filter out all lines
#   before "^/* Public", after "^/* Private" and between "^/**"..."**/$"
libzvbi.h: Makefile.in version.h $(LIBZVBI_HDRS)
	sed -ne "1,/\*\//p" $(srcdir)/vbi.h > $@
	echo -e "\n/* Generated file, do not edit! */" >> $@
	echo -e "\n#ifndef __LIBZVBI_H__\n#define __LIBZVBI_H__" >> $@
	cat version.h >> $@
	echo -e "\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n" >> $@
	echo -e "\ntypedef struct vbi_decoder vbi_decoder;\n" >> $@
	for fi in $(LIBZVBI_HDRS); do \
		echo "/* $$fi */" >> $@; \
		sed -e "/^\/\* Public/,/^\/\* Private/!d;/^\/\* Public/d;/^\/\* Private/d;s/\(\/\*\*.*\*\/\)//;/^[ 	]*\/\*\*/,/\*\/[ 	]*$$/d;" $(srcdir)/$$fi >> $@; \
	done
	echo -e "\n#ifdef __cplusplus\n}\n#endif" >> $@
	echo -e "\n#endif /* __LIBZVBI_H__ */" >> $@

## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = libzvbi.la

BUILT_SOURCES = version.h zvbi.h wstfont2.xbm ccfont3.xbm
EXTRA_DIST = version.h zvbi.h wstfont2.pbm ccfont3.pbm
CLEANFILES = $(BUILT_SOURCES)

INCLUDES = \
	-W -Wall -Wunused -Wfloat-equal \
	-Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align \
	-Wwrite-strings -Winline -Wmissing-prototypes \
	-Wmissing-declarations
#	-Wpadded -Wconversion
INCLUDES += -D_REENTRANT -D_XOPEN_SOURCE -D_BSD_SOURCE
INCLUDES += $(X_CFLAGS) $(UNICODE_CFLAGS)

INSTALLHDRS = \
	aspect_ratio.h \
	bcd.h \
	bit_slicer.h \
	conv.h \
	event.h \
	export.h \
	exp-gfx.h \
	exp-txt.h \
	format.h \
	graphics.h \
	hamm.h \
	intl.h \
	io.h \
	io-dvb.h \
	io-sim.h \
	lang.h \
	link.h \
	macros.h \
	network.h \
	packet-830.h \
	pdc.h \
	pfc_demux.h \
	raw_decoder.h \
	sampling.h \
	search.h \
	sliced.h \
	teletext_decoder.h \
	trigger.h \
	vbi_decoder.h \
	version.h \
	vps.h \
	wss.h \
	xds_demux.h \
	xds_decoder.h

libzvbi_la_SOURCES = $(INSTALLHDRS) \
	bcd.c \
	bit_slicer.c \
	cache.c cache.h \
	caption.c cc.h \
	conv.c \
	dlist.h \
	event.c \
	export.c export-priv.h \
	exp-gfx.c \
	exp-html.c \
	exp-sub.c \
	exp-templ.c \
	exp-txt.c \
	exp-vtx.c \
	format.h \
	graphics.c \
	hamm.c \
	intl-priv.h \
	io.c io-priv.h \
	io-bktr.c \
	io-dvb.h \
	io-sim.c \
	io-v4l2k.c videodev2k.h \
	io-v4l2.c videodev2.h \
	io-v4l.c videodev.h \
	lang.c \
	misc.h \
	network.c \
	packet.c vt.h \
	packet-830.c \
	pdc.c \
	pfc_demux.c \
	raw_decoder.c \
	sampling.c \
	search.c \
	tables.c tables.h \
	teletext.c \
	trigger.c \
	ure.c ure.h \
	vbi.c vbi.h \
	vps.c \
	wss.c \
	xds_demux.c \
	xds_decoder.c

libzvbiincludedir = $(includedir)/zvbi-0.3/zvbi
libzvbiinclude_HEADERS = $(INSTALLHDRS) \
	version.h \
	zvbi.h

exp-gfx.c: wstfont2.xbm ccfont3.xbm

wstfont2.xbm: wstfont2.pbm fontgen Makefile.in
	$(top_builddir)/src/fontgen xbm wstfont2 10 < $< > $@
ccfont3.xbm: ccfont3.pbm fontgen Makefile.in
	$(top_builddir)/src/fontgen xbm ccfont3 13 < $< > $@

libzvbi_la_LIBADD = $(PTHREAD_LIB) $(PNG_LIB) $(UNICODE_LIBS)
libzvbi_la_LDFLAGS = -version-info $(LIBZVBI_SO_VERSION)

version.h: Makefile.in
	echo -e "#ifndef __ZVBI_VERSION_H__" > $@
	echo -e "#define __ZVBI_VERSION_H__\n" >> $@
	echo -e "\n#define VBI_VERSION_MAJOR" `echo @VERSION@ \
	 | sed "s/[^0-9.]//g" \
	 | sed "s/\./\\\\\n#define VBI_VERSION_MINOR /" \
	 | sed "s/\./\\\\\n#define VBI_VERSION_MICRO /"` >> $@
	echo -e "\n#endif /* __ZVBI_VERSION_H__ */" >> $@

zvbi.h: Makefile.in $(INSTALLHDRS)
	echo -e "#ifndef __ZVBI_H__\n#define __ZVBI_H__\n" > $@
	for i in $(INSTALLHDRS); do \
		echo -e "#include \"$$i\"" >> $@; \
	done
	echo -e "\n#endif /* __ZVBI_H__ */" >> $@

icheck: $(libzvbiinclude_HEADERS)
	for i in $(libzvbiinclude_HEADERS); do \
		echo Checking $$i; \
		grep "VBI_BEGIN_DECLS" $$i >/dev/null || exit 1; \
		grep "VBI_END_DECLS" $$i >/dev/null || exit 1; \
		echo -e "#include \"$$i\"" >_include_test.c; \
		$(CC) -c _include_test.c || exit 1; \
	done
	rm _include_test*

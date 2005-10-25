## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libvbi.la

BUILT_SOURCES = \
	wstfont2.xbm \
	ccfont3.xbm

MAINTAINERCLEANFILES = \
	fontgen

EXTRA_DIST = \
	$(BUILT_SOURCES) \
	fontgen.c \
	wstfont2.pbm \
	ccfont3.pbm

# ZAPPING8: this is only a subset of libzvbi 0.3, compile accordingly.
AM_CFLAGS = \
	-D_GNU_SOURCE \
	-D_REENTRANT \
	-DZAPPING8 \
	$(UNICODE_CFLAGS)

INCLUDES = -I$(top_srcdir)

libvbi_la_SOURCES = \
	bcd.c bcd.h \
	cache.c cache.h cache-priv.h \
	caption_decoder.c caption_decoder.h caption_decoder-priv.h \
	conv.c conv.h \
	dlist.h \
	event.c event.h event-priv.h \
	exp-gfx.c exp-gfx.h wstfont2.xbm \
	exp-html.c \
	exp-sub.c \
	exp-txt.c exp-txt.h \
	exp-vtx.c \
	export.c export.h export-priv.h \
	hamm.c hamm.h \
	image_format.c image_format.h \
	lang.c lang.h \
	link.c link.h \
	macros.h \
	misc.c misc.h \
	network.c network.h network-table.h \
	packet-830.c packet-830.h \
	page.h page-priv.h \
	pdc.h \
	sampling_par.h \
	search.c search.h \
	sliced.h \
	teletext.c \
	teletext_decoder.c teletext_decoder-priv.h teletext_decoder.h \
	top_title.c top_title.h \
	ure.c ure.h \
	vbi_decoder.c vbi_decoder.h vbi_decoder-priv.h \
	vps.c vps.h \
	vt.h

exp-gfx.$(OBJEXT): wstfont2.xbm ccfont3.xbm

if MAINTAINER_MODE

wstfont2.xbm: wstfont2.pbm Makefile.am fontgen$(EXEEXT)
	$(top_builddir)/libvbi/fontgen$(EXEEXT) xbm wstfont2 10 < $< > $@

ccfont3.xbm: ccfont3.pbm Makefile.am fontgen$(EXEEXT)
	$(top_builddir)/libvbi/fontgen$(EXEEXT) xbm ccfont3 13 < $< > $@

endif

# Must prevent link conflict with libzvbi 0.2.
great_rename:
	for file in *.c *.h; do \
	  sed "s/vbi_/vbi3_/g;s/VBI_/VBI3_/g" <$$file >tmp; \
	  mv tmp $$file; \
	done

## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = libzvbi-0.3.la

BUILT_SOURCES = \
	hamm-tables.h \
	network-table.h \
	version.h \
	zvbi.h \
	wstfont2.xbm

EXTRA_DIST = \
	$(BUILT_SOURCES) \
	hamm-tables.h \
	network-table.pl \
	wstfont2.pbm

# -D_GNU_SOURCE to enable GNU extensions if available.
AM_CPPFLAGS = \
	-I$(top_srcdir) \
        $(UNICODE_CFLAGS) \
        -D_REENTRANT \
        -D_GNU_SOURCE

INSTALLHDRS = \
	bcd.h \
	cache.h \
	conv.h \
	dvb_demux.h \
	error.h \
	event.h \
	exp-gfx.h \
	hamm.h \
	image_format.h \
	intl.h \
	macros.h \
	network.h \
	page.h \
	sampling_par.h \
	sliced.h \
	ttx_charset.h \
	ttx_decoder.h \
	ttx_page_stat.h

libzvbi_0_3_la_SOURCES = \
	bcd.c bcd.h \
	cache.c cache.h cache-priv.h dlist.h \
	conv.c conv.h \
	dvb.h \
	dvb_demux.c dvb_demux.h \
	error.h \
	event.c event.h event-priv.h \
	exp-gfx.c exp-gfx.h \
	hamm.c hamm.h hamm-tables.h \
	intl.h intl-priv.h \
	macros.h \
	misc.c misc.h \
	network.c network.h network-table.h \
	page.h \
	sampling_par.h \
	sliced.h \
	ttx.h \
	ttx_charset.c ttx_charset.h \
	ttx_decoder.c ttx_decoder.h ttx_decoder-priv.h \
	ttx_page.c ttx_page.h ttx_page-priv.h \
	ttx_page_stat.h

libzvbi_0_3includedir = $(includedir)/zvbi-0.3/zvbi
libzvbi_0_3include_HEADERS = $(INSTALLHDRS) version.h zvbi.h

libzvbi_0_3_la_LIBADD = $(PTHREAD_LIB) $(PNG_LIB) $(UNICODE_LIBS)
libzvbi_0_3_la_LDFLAGS = -version-info $(LIBZVBI_SO_VERSION)

if MAINTAINER_MODE

hammgen: hammgen.c
	$(CC) $(AM_CPPFLAGS) -o hammgen $(srcdir)/hammgen.c

hamm-tables.h: hammgen Makefile.am
	$(top_builddir)/zvbi/hammgen > $@

fontgen: fontgen.c misc.h version.h hamm-tables.h
	$(CC) $(AM_CPPFLAGS) -o fontgen $(srcdir)/fontgen.c

wstfont2.xbm: wstfont2.pbm fontgen Makefile.am
	$(top_builddir)/zvbi/fontgen xbm wstfont2 10 < $< > $@

version.h: Makefile.am
	echo -e "/* Generated file, do not edit! */\n" > $@
	echo -e "#ifndef __ZVBI_VERSION_H__" >> $@
	echo -e "#define __ZVBI_VERSION_H__\n" >> $@
	echo -e "#include \"zvbi/macros.h\"\n" >> $@
	echo -e "VBI_BEGIN_DECLS\n" >> $@
	echo -e "#define VBI_VERSION_MAJOR" `echo @VERSION@ \
	 | sed "s/[^0-9.]//g" \
	 | sed "s/\./\\\\\n#define VBI_VERSION_MINOR /" \
	 | sed "s/\./\\\\\n#define VBI_VERSION_MICRO /"` >> $@
	echo -e "\nVBI_END_DECLS\n" >> $@
	echo -e "#endif /* __ZVBI_VERSION_H__ */" >> $@

zvbi.h: $(INSTALLHDRS) version.h Makefile.am
	echo -e "/* Generated file, do not edit! */\n" > $@
	echo -e "#ifndef __ZVBI_H__\n#define __ZVBI_H__\n" >> $@
	for header in $(INSTALLHDRS); do \
		echo -e "#include \"zvbi/$$header\"" >> $@; \
	done
	echo -e "#include \"zvbi/version.h\"\n" >> $@
	echo -e "\n#endif /* __ZVBI_H__ */" >> $@

network-table.h: network-table.pl Makefile.am
	wget -q -O- --proxy=off \
	  http://zapping.sourceforge.net/zvbi-0.3/networks.xml | \
	  $(srcdir)/network-table.pl > $@

endif

## Process this file with automake to produce Makefile.in

MANUALS = \
	zapping.xml \
	zapping_remote.xml \
	zapping_setup_fb.xml

if BUILD_MAN

# XXX zapping_setup_fb.1 needs to be installed only if we NEED_ZSFB
man_MANS = $(MANUALS:.xml=.1)

EXTRA_DIST = $(man_MANS) $(MANUALS)

%.1: %.xml
	@XSLTPROC@ -nonet "http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl" $<

else

EXTRA_DIST = $(MANUALS)

endif

xmlcheck: $(MANUALS)
	for file in $^; do \
		SP_CHARSET_FIXED=YES \
		SP_ENCODING=XML \
		onsgmls -s -wxml $$file; \
	done

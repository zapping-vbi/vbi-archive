## Process this file with automake to produce Makefile.in

MANUALS = \
	zapping.xml \
	zapping_remote.xml \
	zapping_setup_fb.xml

man_MANS = $(MANUALS:.xml=.1)

EXTRA_DIST = $(man_MANS) $(MANUALS)

if MAINTAINER_MODE

if BUILD_MAN
%.1: %.xml Makefile.am
	@XSLTPROC@ -nonet "http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl" $<
else
# $(man_MANS) are in CVS and source tarball, so BUILD_MAN is necessary
# only if $(MANUALS) changed.
%.1: %.xml
	echo The xsltproc tool, DocBook XML DTD V4.1.2 or DocBook XSL
	echo stylesheets are missing. Rerun autogen.sh or configure
	echo when you have them.
	exit 1
endif

endif

xmlcheck: $(MANUALS)
	for file in $^; do \
		SP_CHARSET_FIXED=YES \
		SP_ENCODING=XML \
		onsgmls -s -wxml $$file; \
	done

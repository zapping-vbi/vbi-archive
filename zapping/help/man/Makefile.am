## Process this file with automake to produce Makefile.in

# XXX zapping_setup_fb.1 only needed w/V4L(2) but we cannot
# but man_MANS in a conditional :-(
man_MANS = zapping.1 zapping_setup_fb.1

EXTRA_DIST = $(man_MANS) $(man_MANS:.1=.xml)

# Stolen from glib

if BUILD_MAN

%.1: %.xml
	@XSLTPROC@ -nonet "http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl" $<

endif

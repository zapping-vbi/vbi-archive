figdir = figures
docname = zapping
lang = C
omffile = zapping-C.omf
entities = \
	settings.xml \
	channel-editor.xml \
	zapzilla.xml \
	commands.xml \
	fdl-appendix.xml

include $(top_srcdir)/help/xmldocs.make

# Missing in xmldocs.make
DISTCLEANFILES = $(omffile).out

dist-hook: app-dist-hook

# This finds errors missed by scrollkeeper-update (make install).
xmlcheck: $(entities)
	SP_CHARSET_FIXED=YES \
	SP_ENCODING=XML \
	onsgmls -s -wxml zapping.xml

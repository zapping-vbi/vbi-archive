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

dist-hook: app-dist-hook

check: $(entitites)
	nsgmls -s -E2000 zapping.xml

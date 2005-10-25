## Process this file with automake to produce Makefile.in.

SUBDIRS = man C

EXTRA_DIST = \
	omf.make \
	xmldocs.make

xmlcheck:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir xmlcheck; \
	done

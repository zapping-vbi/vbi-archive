## Process this file with automake to produce Makefile.in

EXTRA_DIST = \
	BUGS \
	config.rpath \
	depcomp \
	zvbi-0.3.pc.in

DISTCLEANFILES = site_def.h

SUBDIRS = \
	m4 \
	zvbi \
	test \
	examples

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = zvbi-0.3.pc

dist-hook:
	chown -R 500:100 $(distdir)

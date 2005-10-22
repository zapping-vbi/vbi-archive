## Process this file with automake to produce Makefile.in

if BACKEND_MP1E
MP1E_SUBDIRS = mp1e
endif
if BACKEND_FFMPEG
FFMPEG_SUBDIRS = ffmpeg
endif
if BACKEND_DIVX4LINUX
DIVX4LINUX_SUBDIRS = divx4linux
endif

SUBDIRS = \
	$(MP1E_SUBDIRS) \
	$(FFMPEG_SUBDIRS) \
	$(DIVX4LINUX_SUBDIRS) \
	m4 src test po doc

EXTRA_DIST = config.rpath  BUGS rte.spec.in rte.spec depcomp

DISTCLEANFILES = site_def.h

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rte-0.5.pc

dist-hook:
	chown -R 500:100 $(distdir)

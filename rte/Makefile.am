## Process this file with automake to produce Makefile.in

if BACKEND_MP1E
MP1E_SUBDIRS = mp1e
endif
if BACKEND_FFMPEG
FFMPEG_SUBDIRS = ffmpeg
endif

SUBDIRS = \
	$(MP1E_SUBDIRS) \
	$(FFMPEG_SUBDIRS) \
	m4 src test po doc

EXTRA_DIST = config.rpath rte.spec.in rte.spec

DISTCLEANFILES = site_def.h

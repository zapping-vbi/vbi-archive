## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libcommon.la

INCLUDES = -D_GNU_SOURCE -D_REENTRANT -include $(top_builddir)/config.h \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

libcommon_la_SOURCES = \
	alloc.c alloc.h \
	bstream.c bstream.h bstream_mmx.s \
	fifo.c fifo.h \
	list.h log.h math.h \
	mmx.c mmx.h \
	profile.c profile.h \
	threads.h types.h \
	sync.c sync.h

EXTRA_DIST = videodev.h videodev2.h videodev25.h



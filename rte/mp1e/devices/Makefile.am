## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libdevices.la

INCLUDES = -D_GNU_SOURCE -D_REENTRANT -DFLOAT=float \
	-include $(top_builddir)/config.h \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

libdevices_la_SOURCES = \
	afl.c alsa.c arts.c esd.c oss.c \
	file.c raw.c v4l.c v4l2.c

libdevices_la_LIBADD = $(ESD_LIBS)

## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libdevices.la

INCLUDES = -D_GNU_SOURCE -D_REENTRANT -DFLOAT=float \
	-include $(top_builddir)/config.h \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations \
	$(ALSA_CFLAGS)

libdevices_la_SOURCES = \
	afl.c \
	alsa.c \
	arts.c \
	esd.c \
	file.c \
	oss.c \
	raw.c \
	v4l.c \
	v4l2.c \
	v4l25.c

libdevices_la_LIBADD = $(ALSA_LIBS) $(ESD_LIBS)

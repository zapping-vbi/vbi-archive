noinst_LTLIBRARIES = libdevices.la

INCLUDES = -D_GNU_SOURCE -D_REENTRANT -DFLOAT=float

libdevices_la_SOURCES = \
	afl.c alsa.c arts.c esd.c oss.c \
	file.c raw.c v4l.c v4l2.c

libdevices_la_LIBADD = $(ESD_LIBS)

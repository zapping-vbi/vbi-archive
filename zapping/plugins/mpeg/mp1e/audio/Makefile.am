noinst_LTLIBRARIES = libaudio_rte.la libaudio.la

INCLUDES=-D_GNU_SOURCE -D_REENTRANT -DFLOAT=float

libaudio_rte_la_SOURCES = \
	fft.c \
	filter.c filter_mmx.s \
	mp2.c mpeg.h \
	psycho.c tables.c

libaudio_la_SOURCES = \
	fft.c \
	filter.c filter_mmx.s \
	mp2.c mpeg.h \
	psycho.c tables.c \
	audio.h alsa.c esd.c oss.c afl.c

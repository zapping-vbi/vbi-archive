noinst_LTLIBRARIES = libaudio_rte.la libaudio.la

INCLUDES=-D_GNU_SOURCE -D_REENTRANT -DFLOAT=float $(ESD_CFLAGS)

libaudio_rte_la_SOURCES = \
	fft.c libaudio.h \
	filter.c filter_mmx.s \
	mp2.c mpeg.h audio.h \
	psycho.c tables.c

libaudio_la_SOURCES = \
	fft.c libaudio.h \
	filter.c filter_mmx.s \
	mp2.c mpeg.h audio.h \
	psycho.c tables.c \
	alsa.c esd.c arts.c oss.c afl.c

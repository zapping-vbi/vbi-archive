noinst_LTLIBRARIES = libaudio.la

INCLUDES = -D_GNU_SOURCE -D_REENTRANT -DFLOAT=float

libaudio_la_SOURCES = \
	fft.c libaudio.h \
	filter.c filter_mmx.s \
	mp2.c mpeg.h audio.h \
	psycho.c tables.c

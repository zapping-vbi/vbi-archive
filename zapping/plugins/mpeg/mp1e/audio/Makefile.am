noinst_LIBRARIES = libaudio_rte.a libaudio.a

INCLUDES=-D_GNU_SOURCE -D_REENTRANT -DFLOAT=float

libaudio_rte_a_SOURCES = \
	fft.c \
	filter.c filter_mmx.s \
	mp2.c mpeg.h \
	psycho.c tables.c

libaudio_a_SOURCES = \
	fft.c \
	filter.c filter_mmx.s \
	mp2.c mpeg.h \
	psycho.c tables.c \
	audio.h	oss.c tsp.c


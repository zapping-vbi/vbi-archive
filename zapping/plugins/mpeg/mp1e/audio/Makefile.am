noinst_LIBRARIES = libaudio.a

INCLUDES=-D_GNU_SOURCE -D_REENTRANT -DFLOAT=float

libaudio_a_SOURCES = \
	fft.c \
	filter.c filter_mmx.s \
	mp2.c mpeg.h \
	oss.c tsp.c \
	psycho.c tables.c \
	audio.h

## Two targets, one for mp1e (stdout output) and one for rte
## (rte_context callback output).
noinst_LIBRARIES = libsystems_rte.a libsystems.a

INCLUDES=-D_GNU_SOURCE -D_REENTRANT

libsystems_rte_a_SOURCES = \
	mpeg1.c mpeg2.c mpeg.h \
	rte_output.c output.h \
	stream.c stream.h \
	systems.h

libsystems_a_SOURCES = \
	mpeg1.c mpeg2.c mpeg.h \
	output.c output.h \
	stream.c stream.h \
	systems.h
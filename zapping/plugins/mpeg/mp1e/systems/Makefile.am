## Two targets, one for mp1e (stdout output) and one for rte
## (rte_context callback output).
noinst_LTLIBRARIES = libsystems_rte.la libsystems.la

INCLUDES=-D_GNU_SOURCE -D_REENTRANT

libsystems_rte_la_SOURCES = \
	mpeg1.c mpeg2.c mpeg.h \
	rte_output.c output.h \
	stream.c stream.h \
	systems.h

libsystems_la_SOURCES = \
	mpeg1.c mpeg2.c mpeg.h \
	output.c output.h \
	stream.c stream.h \
	systems.h
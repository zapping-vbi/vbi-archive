INCLUDES=-D_GNU_SOURCE -D_REENTRANT

EXTRA_DIST = \
	alloc.c alloc.h \
	fifo.c fifo.h \
	list.h math.h \
	threads.h types.h \
	errstr.c errstr.h

noinst_LTLIBRARIES = libcommon.la

if BACKEND_MP1E
## These symbols are already in mp1e, avoid linking twice
libcommon_la_SOURCES =
else
libcommon_la_SOURCES = $(EXTRA_DIST)
endif

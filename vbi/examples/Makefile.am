## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = \
	network \
	rawout \
	wss

INCLUDES = -I$(top_srcdir)/src

LDADD = $(top_builddir)/src/libzvbi-0.3.la \
	$(PTHREAD_LIB) -lm $(PNG_LIB)

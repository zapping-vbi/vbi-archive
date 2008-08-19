## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = \
	teletext

AM_CFLAGS = -I$(top_srcdir) $(X_CFLAGS)

LDADD = $(top_builddir)/zvbi/libzvbi-0.3.la \
	$(PTHREAD_LIB) -lm $(X_LIBS)

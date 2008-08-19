## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = \
	teletext

AM_CFLAGS = -I$(top_srcdir) $(X_CFLAGS)

# Libtool creates a wrapper script around the application until the
# library is installed, which is a major annoyance for debugging with
# gdb and valgrind. So we link these apps statically if possible.
if BUILD_STATIC_LIB
LDADD = $(top_builddir)/zvbi/.libs/libzvbi-0.3.a
else
LDADD = $(top_builddir)/zvbi/libzvbi-0.3.la
endif

LDADD += $(PTHREAD_LIB) -lm $(X_LIBS)


## Process this file with automake to produce Makefile.in

EXTRA_DIST = README

INCLUDES= -D_REENTRANT -D_GNU_SOURCE -include $(top_builddir)/config.h \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

noinst_LTLIBRARIES = libdivx4linux.la

libdivx4linux_la_SOURCES = \
	b_divx4linux.c b_divx4linux.h

libdivx4linux_la_LIBADD = -ldl

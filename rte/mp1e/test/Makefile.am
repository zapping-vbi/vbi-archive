## Process this file with automake to produce Makefile.in

INCLUDES = $(GTK_CFLAGS) -D_GNU_SOURCE

noinst_LTLIBRARIES = libtest.la

libtest_la_SOURCES = \
	support.c support.h \
	preview.c
libtest_la_LIBADD = $(GTK_LIBS)

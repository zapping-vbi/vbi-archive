## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libsystems.la

INCLUDES = -D_GNU_SOURCE -D_REENTRANT -include $(top_builddir)/config.h \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

libsystems_la_SOURCES = \
	mpeg1.c mpeg2.c vcd.c mpeg.h \
	output.c output.h \
	systems.c systems.h \
	libsystems.h

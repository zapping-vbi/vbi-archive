## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libvbi.la

INCLUDES = -D_GNU_SOURCE -D_REENTRANT -include $(top_builddir)/config.h \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

libvbi_la_SOURCES = vbi.c vbi.h libvbi.h

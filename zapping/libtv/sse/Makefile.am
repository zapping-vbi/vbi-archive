## Process this file with automake to produce Makefile.in

# SSE routines for x86 and x86_64.
# x86_64 untested

noinst_LTLIBRARIES = libsse.la

AM_CFLAGS = -O3 -fomit-frame-pointer -D_REENTRANT -msse -DSSE

INCLUDES = -I$(top_srcdir)

libsse_la_SOURCES = \
	copy_block.c \
	sse.c sse.h

## Process this file with automake to produce Makefile.in

# SSE routines for x86 and x86_64.
# x86_64 untested

noinst_LIBRARIES = libsse.a

AM_CFLAGS = -O3 -fomit-frame-pointer -D_REENTRANT -msse -DSSE

INCLUDES = -I$(top_srcdir)

libsse_a_SOURCES = \
	copy_block.c \
	sse.c sse.h

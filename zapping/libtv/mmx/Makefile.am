## Process this file with automake to produce Makefile.in

# MMX routines for x86 and x86_64.
# x86_64 untested

noinst_LTLIBRARIES = libmmx.la

AM_CFLAGS = -O3 -fomit-frame-pointer -D_REENTRANT -mmmx -DMMX

INCLUDES = -I$(top_srcdir)

# Note clear_block.h is also used by SSE routines.
libmmx_la_SOURCES = \
	clear_block.h \
	copy_block.c \
	cpu.c \
	mmx.c mmx.h

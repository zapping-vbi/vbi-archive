## Process this file with automake to produce Makefile.in

# MMX routines for x86 and x86_64.
# x86_64 untested

noinst_LIBRARIES = libmmx.a

AM_CFLAGS = -O3 -fomit-frame-pointer -D_REENTRANT -mmmx -DMMX

EXTRA_DIST = \
	clear_block.h

libmmx_a_SOURCES = \
	copy_block.c \
	mmx.c mmx.h

## Process this file with automake to produce Makefile.in

# AltiVec routines for powerpc.
# untested

noinst_LTLIBRARIES = libavec.la

AM_CFLAGS = -O3 -fomit-frame-pointer -D_REENTRANT \
	-maltivec -mabi=altivec -DALTIVEC

INCLUDES = -I$(top_srcdir)

libavec_la_SOURCES = \
	avec.c avec.h \
	clear_block.h \
	cpu.c

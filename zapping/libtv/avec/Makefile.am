## Process this file with automake to produce Makefile.in

# AltiVec routines for powerpc.
# untested

noinst_LIBRARIES = libavec.a

# No -maltivec here until we have a runtime detection
AM_CFLAGS = -O3 -fomit-frame-pointer -D_REENTRANT -DALTIVEC

EXTRA_DIST = \
	clear_block.h

libavec_a_SOURCES = \
	avec.c avec.h

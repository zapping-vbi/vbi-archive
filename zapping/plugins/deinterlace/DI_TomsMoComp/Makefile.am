## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/plugins/deinterlace \
	-masm=intel

noinst_LTLIBRARIES = libDI_TomsMoComp.la

libDI_TomsMoComp_la_SOURCES = \
	DI_TomsMoComp3DNOW.c \
	DI_TomsMoComp.c \
	DI_TomsMoCompMMX.c \
	DI_TomsMoCompSSE2.c \
	DI_TomsMoCompSSE.c

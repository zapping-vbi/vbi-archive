## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/plugins/deinterlace \
	-masm=intel

noinst_LTLIBRARIES = libDI_MoComp2.la

libDI_MoComp2_la_SOURCES = \
	DI_MoComp2.c \
	DI_MoComp2MMX.c \
	DI_MoComp2SSE.c \
	DI_MoComp23DNOW.c

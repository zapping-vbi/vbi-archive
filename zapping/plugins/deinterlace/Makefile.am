## Process this file with automake to produce Makefile.in

SUBDIRS = DI_GreedyH DI_Misc test

plugindir = @PLUGIN_DEFAULT_DIR@

plugin_LTLIBRARIES = libdeinterlace.zapping.la

AM_CFLAGS = \
	@PLUGIN_DEFAULT_FLAGS@ \
	-DZAPPING8

INCLUDES = \
	-I$(top_srcdir) \
	@COMMON_INCLUDES@

libdeinterlace_zapping_la_SOURCES = \
	DS_ApiCommon.h \
	DS_Control.h \
	DS_Deinterlace.h \
	main.c main.h \
	preferences.c preferences.h \
	windows.h

libdeinterlace_zapping_la_LIBADD = \
	DI_GreedyH/libDI_GreedyH.la \
	DI_Misc/libDI_Misc.la

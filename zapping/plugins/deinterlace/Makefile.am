## Process this file with automake to produce Makefile.in

SUBDIRS = DI_GreedyH DI_Misc test

INCLUDES = \
	-I$(top_srcdir) \
	-DZAPPING8 \
	@COMMON_INCLUDES@ \
	@PLUGIN_DEFAULT_FLAGS@

plugindir = @PLUGIN_DEFAULT_DIR@
plugin_LTLIBRARIES = libdeinterlace.zapping.la

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

## Process this file with automake to produce Makefile.in

SUBDIRS = DI_GreedyH DI_Misc

INCLUDES = \
	-I$(top_srcdir) \
	-DZAPPING8 \
	@COMMON_INCLUDES@ \
	@PLUGIN_DEFAULT_FLAGS@

noinst_PROGRAMS = ditest dicmp cpudt

ditest_SOURCES = \
	DS_ApiCommon.h \
	DS_Control.h \
	DS_Deinterlace.h \
	ditest.c \
	windows.h

ditest_LDADD = \
	$(top_builddir)/libtv/libtv.la \
	DI_GreedyH/libDI_GreedyH.la \
	DI_Misc/libDI_Misc.la

dicmp_SOURCES = \
	dicmp.c

cpudt_SOURCES = \
	cpudt.c

cpudt_LDADD = \
	$(top_builddir)/libtv/libtv.la

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

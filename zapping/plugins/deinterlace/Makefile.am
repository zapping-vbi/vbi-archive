## Process this file with automake to produce Makefile.in

SUBDIRS = DI_GreedyH DI_Misc DI_MoComp2 DI_TomsMoComp

INCLUDES = \
	-I$(top_srcdir) \
	-DZAPPING8 \
	@COMMON_INCLUDES@ \
	@PLUGIN_DEFAULT_FLAGS@

noinst_PROGRAMS = ditest
ditest_SOURCES = \
	ditest.c

ditest_LDADD = \
	DI_GreedyH/libDI_GreedyH.la \
	DI_Misc/libDI_Misc.la \
	DI_MoComp2/libDI_MoComp2.la \
	DI_TomsMoComp/libDI_TomsMoComp.la

plugindir = @PLUGIN_DEFAULT_DIR@
plugin_LTLIBRARIES = libdeinterlace.zapping.la

libdeinterlace_zapping_la_SOURCES = \
	main.c \
	preferences.c preferences.h

libdeinterlace_zapping_la_LIBADD = \
	DI_GreedyH/libDI_GreedyH.la \
	DI_Misc/libDI_Misc.la \
	DI_MoComp2/libDI_MoComp2.la \
	DI_TomsMoComp/libDI_TomsMoComp.la

## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/plugins/deinterlace \
	-masm=intel

noinst_LTLIBRARIES = libDI_GreedyH.la

libDI_GreedyH_la_SOURCES = \
	DI_GreedyH.c \
	DI_GreedyHF.c \
	DI_GreedyHM.c DI_GreedyHM.h \
	DI_GreedyHM2.h \
	DI_GreedyHMPulldown.c \
	DI_GreedyHM_V.c \
	DI_GreedyHM_NV.c \
	DScalerCalls.c

# Other files included by .c files (not asm code)
EXTRA_DIST = \
	DI_GrUpdtFS.asm \
	DI_GreedyDeLoop.asm \
	DI_GreedyH.asm \
	DI_GreedyHF.asm

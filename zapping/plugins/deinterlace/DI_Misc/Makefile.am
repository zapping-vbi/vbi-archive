## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/plugins/deinterlace \
	-masm=intel

noinst_LTLIBRARIES = libDI_Misc.la

libDI_Misc_la_SOURCES = \
	DI_Adaptive.c \
	DI_BlendedClip.c \
	DI_Bob.c \
	DI_EvenOnly.c \
	DI_Greedy.c \
	DI_Greedy2Frame.c \
	DI_OddOnly.c \
	DI_OldGame.c \
	DI_ScalerBob.c \
	DI_TwoFrame.c \
	DI_VideoBob.c \
	DI_VideoWeave.c \
	DI_Weave.c

# Other files included by .c files (not asm code)
EXTRA_DIST = \
	DI_Greedy.asm \
	DI_Greedy2Frame.asm \
	DI_OldGame.asm \
	DI_TwoFrame.asm \
	DI_VideoBob.asm \
	DI_VideoWeave.asm

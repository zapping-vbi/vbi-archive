## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/plugins/deinterlace \
	-masm=intel

noinst_LTLIBRARIES = libDI_TomsMoComp.la

libDI_TomsMoComp_la_SOURCES = \
	DI_TomsMoComp.c TomsMoComp.h \
	DI_TomsMoComp3DNOW.c \
	DI_TomsMoCompMMX.c \
	DI_TomsMoCompSSE.c \
	DI_TomsMoCompSSE2.c

# Other files included by .c files
EXTRA_DIST = \
	SearchLoop0A.inc \
	SearchLoopBottom.inc \
	SearchLoopEdgeA.inc \
	SearchLoopEdgeA8.inc \
	SearchLoopEdgeAH.inc \
	SearchLoopEdgeAH8.inc \
	SearchLoopOddA.inc \
	SearchLoopOddA2.inc \
	SearchLoopOddA6.inc \
	SearchLoopOddAH.inc \
	SearchLoopOddAH2.inc \
	SearchLoopOddAH6.inc \
	SearchLoopOddAHH2.inc \
	SearchLoopTop.inc \
	SearchLoopVA.inc \
	SearchLoopVAH.inc \
	StrangeBob.inc \
	TomsMoCompAll.inc \
	TomsMoCompAll2.inc \
	WierdBob.inc

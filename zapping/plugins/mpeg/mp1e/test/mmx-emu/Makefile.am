## Process this file with automake to produce Makefile.in

EXTRA_DIST = COPYING.LIB mmx-emu

lib_LTLIBRARIES = libmmxemu.la

libmmxemu_la_SOURCES = mmx-emu.h sigill_handler.c decode.c \
	mmx.h 3dnow_fp.h 3dnow_int.h cyrix.h sse_fp.h sse_int.h sse2_fp.h sse2_int.h \
	mmx.c 3dnow_fp.c 3dnow_int.c cyrix.c sse_fp.c sse_int.c sse2_fp.c sse2_int.c

libmmxemu_la_LIBADD = -lm
libmmxemu_la_LDFLAGS = -version-info 0:7:0 # NOT release

CFLAGS = -O2 -Wall -funroll-loops -fomit-frame-pointer
# -DMMX_DEBUG

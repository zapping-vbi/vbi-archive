## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libi386.la

INCLUDES=-D_REENTRANT

libi386_la_SOURCES = cputest.c dsputil_mmx.c dsputil_mmx_avg.h \
fdct_mmx.s fdctdata.c mmx.h sad_mmx.s cpuid.s

# FIXME2
cpuid.lo:
	gcc cpuid.s -c -o $@ $<

# FIXME: Configure check for nasm
.s.lo:
	nasm -f elf -o $@ $<

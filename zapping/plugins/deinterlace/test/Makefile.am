## Process this file with automake to produce Makefile.in

TESTS = ditest-scalar.sh

if CAN_COMPILE_MMX
TESTS += ditest-mmx.sh
endif

if CAN_COMPILE_3DNOW
TESTS += ditest-3dnow.sh
endif

if CAN_COMPILE_SSE
TESTS += ditest-sse.sh
endif

if CAN_COMPILE_SSE2
TESTS += ditest-sse2.sh
endif

if CAN_COMPILE_SSE3
TESTS += ditest-sse3.sh
endif

if CAN_COMPILE_ALTIVEC
TESTS += ditest-altivec.sh
endif

EXTRA_DIST = \
	ditest-3dnow.sh \
	ditest-all.sh \
	ditest-altivec.sh \
	ditest-mmx.sh \
	ditest-scalar.sh \
	ditest-sse.sh \
	ditest-sse2.sh \
	ditest-sse3.sh \
	ditest-x86.sh \
	emulator.sh \
	gen-md5sums.sh \
	md5sums-3dnow \
	md5sums-altivec \
	md5sums-mmx \
	md5sums-scalar \
	md5sums-sse \
	md5sums-sse2 \
	md5sums-sse3

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/plugins/deinterlace

noinst_PROGRAMS = ditest dicmp cpudt

ditest_SOURCES = \
	ditest.c \
	guard.h

ditest_LDADD = \
	$(top_builddir)/plugins/deinterlace/DI_GreedyH/libDI_GreedyH.la \
	$(top_builddir)/plugins/deinterlace/DI_Misc/libDI_Misc.la \
	$(top_builddir)/libtv/libtv.la

dicmp_SOURCES = \
	dicmp.c

cpudt_SOURCES = \
	cpudt.c

cpudt_LDADD = \
	$(top_builddir)/libtv/libtv.la

emulator.sh:
	$(MAKE) cpudt

ditest-all.sh:
	$(MAKE) ditest
	$(MAKE) emulator.sh

ditest-3dnow.sh \
ditest-altivec.sh \
ditest-mmx.sh \
ditest-scalar.sh \
ditest-sse.sh \
ditest-sse2.sh \
ditest-sse3.sh \
gen-md5sums.sh:
	$(MAKE) ditest-all.sh

ditest-x86.sh:
	$(MAKE) ditest-all.sh
	$(MAKE) dicmp

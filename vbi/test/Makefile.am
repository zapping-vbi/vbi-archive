## Process this file with automake to produce Makefile.in

TESTS = exoptest test-bcd test-hamm
# test-bitslicer

noinst_PROGRAMS = unicode glyph explist caption export osc capture \
	test-bcd test-hamm
# test-bitslicer

test_bcd_SOURCES = test-bcd.cc
test_hamm_SOURCES = test-hamm.cc
#test_bitslicer_SOURCES = test-bitslicer.cc

EXTRA_DIST = $(TESTS) README uclist sim.c

AM_CFLAGS = \
	-I$(top_srcdir)/src $(COMMON_INCLUDES) $(X_CFLAGS) \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

AM_CXXFLAGS = \
	-I$(top_srcdir)/src $(COMMON_INCLUDES) $(X_CFLAGS) \
	-Wall -Wunused -Wmissing-prototypes

LDADD = $(top_builddir)/src/.libs/libzvbi.a \
	$(PTHREAD_LIB) -lm $(PNG_LIB) $(X_LIBS)

test_hamm_LDADD = \
	$(top_builddir)/src/.libs/libzvbi.a \
	$(PTHREAD_LIB) -lm $(PNG_LIB) $(X_LIBS)

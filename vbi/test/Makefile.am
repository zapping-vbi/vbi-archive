## Process this file with automake to produce Makefile.in

TESTS = test-bcd test-hamm test-raw_decoder
# exoptest

noinst_PROGRAMS = unicode glyph explist caption export osc capture \
	test-bcd test-hamm test-raw_decoder

test_bcd_SOURCES = test-bcd.cc
test_hamm_SOURCES = test-hamm.cc
test_raw_decoder_SOURCES = test-raw_decoder.cc

EXTRA_DIST = $(TESTS) README uclist sim.c

AM_CFLAGS = \
	-I$(top_srcdir)/src $(COMMON_INCLUDES) $(X_CFLAGS) \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

AM_CXXFLAGS = \
	-I$(top_srcdir)/src $(COMMON_INCLUDES) $(X_CFLAGS) \
	-Wall -Wunused -Wmissing-prototypes

LDADD = $(top_builddir)/src/.libs/libzvbi.a \
	$(PTHREAD_LIB) -lm $(PNG_LIB) $(X_LIBS)

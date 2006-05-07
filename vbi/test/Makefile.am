## Process this file with automake to produce Makefile.in

TESTS = test-bcd test-hamm test-raw_decoder test-explist

noinst_PROGRAMS = \
	caption \
	capture \
	date \
	decode \
	event \
	explist \
	export \
	glyph \
	osc \
	sliced2pes \
	test-bcd \
	test-hamm \
	test-raw_decoder \
	ttxfilter \
	unicode

test_bcd_SOURCES = test-bcd.cc
test_hamm_SOURCES = test-hamm.cc
test_raw_decoder_SOURCES = test-raw_decoder.cc

caption_SOURCES = \
	caption.c \
	sliced.c sliced.h

capture_SOURCES = \
	capture.c \
	sliced.c sliced.h

decode_SOURCES = \
	decode.c \
	sliced.c sliced.h

export_SOURCES = \
	export.c \
	sliced.c sliced.h

sliced2pes_SOURCES = \
	sliced2pes.c \
	sliced.c sliced.h

ttxfilter_SOURCES = \
	ttxfilter.c \
	sliced.c sliced.h

EXTRA_DIST = test-explist README uclist

# $(top_srcdir) for VPATH zvbi.h
AM_CFLAGS = \
	-D_GNU_SOURCE \
	-I$(top_srcdir) -I$(top_srcdir)/src \
	$(COMMON_INCLUDES) $(X_CFLAGS) \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

AM_CXXFLAGS = \
	-I$(top_srcdir) -I$(top_srcdir)/src \
	$(COMMON_INCLUDES) $(X_CFLAGS) \
	-Wall -Wunused -Wmissing-prototypes

LDADD = $(top_builddir)/src/.libs/libzvbi-0.3.a \
	$(PTHREAD_LIB) -lm $(PNG_LIB) $(X_LIBS)

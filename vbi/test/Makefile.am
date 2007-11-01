## Process this file with automake to produce Makefile.in

UNIT_TESTS = \
	test-bcd \
	test-dvb_mux \
	test-hamm \
	test-raw_decoder \
	test-vps

TESTS = \
	$(UNIT_TESTS) \
	test-headers \
	test-io \
	test-explist

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
	test-dvb_mux \
	test-hamm \
	test-io \
	test-raw_decoder \
	test-vps \
	ttxfilter \
	unicode

check_SCRIPTS = \
	test-explist \
	test-headers

test_bcd_SOURCES = test-bcd.cc
test_hamm_SOURCES = test-hamm.cc
test_io_SOURCES = test-io.cc

test_dvb_mux_SOURCES = \
	test-dvb_mux.cc \
	test-common.cc test-common.h

test_raw_decoder_SOURCES = \
	test-raw_decoder.cc \
	test-common.cc test-common.h

caption_SOURCES = caption.c sliced.c sliced.h
capture_SOURCES = capture.c sliced.c sliced.h
date_SOURCES = date.c sliced.c sliced.h
decode_SOURCES = decode.c sliced.c sliced.h
event_SOURCES = event.c sliced.c sliced.h
export_SOURCES = export.c sliced.c sliced.h
osc_SOURCES = osc.c sliced.c sliced.h
sliced2pes_SOURCES = sliced2pes.c sliced.c sliced.h
ttxfilter_SOURCES = ttxfilter.c sliced.c sliced.h

EXTRA_DIST = \
	$(check_SCRIPTS)
	README \
	uclist

CLEANFILES = \
	test-hdrs.cc \
	test-hdrs.log \
	test-hdrs.out

# $(top_srcdir) for VPATH zvbi.h
AM_CFLAGS = \
	-D_GNU_SOURCE \
	-I$(top_srcdir) -I$(top_srcdir)/src \
	$(COMMON_INCLUDES) $(X_CFLAGS)

AM_CXXFLAGS = \
	-I$(top_srcdir) -I$(top_srcdir)/src \
	$(COMMON_INCLUDES) $(X_CFLAGS)

TESTS_ENVIRONMENT = \
	CXX="$(CXX)" \
	CXXFLAGS="$(CXXFLAGS)"

# Libtool creates a wrapper script around the application until the
# library is installed, which is a major annoyance for debugging with
# gdb and valgrind. So we link these apps statically if possible.
if BUILD_STATIC_LIB
LDADD = $(top_builddir)/src/.libs/libzvbi-0.3.a
else
LDADD = $(top_builddir)/src/libzvbi-0.3.la
endif

LDADD += $(PTHREAD_LIB) -lm $(PNG_LIB) $(X_LIBS)

check-valgrind: $(UNIT_TESTS)
	for test in $(UNIT_TESTS) ; do \
	  echo Testing $$test ; \
	  valgrind --leak-check=full $$test || \
	    (echo Test failed ; exit 1) ; \
	done

# NB this works only if you add -fprofile-arcs -ftest-coverage
# to CFLAGS and CXXFLAGS.
check-coverage: $(UNIT_TESTS)
	for test in $(UNIT_TESTS) ; do \
	  module=`echo "$$test" | sed 's/^test-//'` ; \
	  echo Testing $$test ; \
	  ./$$test ; \
	  cd ../src ; \
	  gcov $$module.c ; \
	  for chfile in $$module.c $$module.h ; do \
	    if test -f $$chfile.gcov ; then \
	      grep '###:' $$module.c.gcov ; \
	    fi ; \
	  done ; \
	  cd ../test ; \
	done

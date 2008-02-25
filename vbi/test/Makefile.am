## Process this file with automake to produce Makefile.in

compile_tests = \
	ctest \
	cpptest

cpptest_SOURCES = cpptest.cc

if HAVE_GCC_C89_SUPPORT
compile_tests += ctest-c89 ctest-gnu89

ctest_c89_SOURCES = ctest.c
ctest_c89_CFLAGS = -pedantic-errors -std=c89

ctest_gnu89_SOURCES = ctest.c
ctest_gnu89_CFLAGS = -pedantic-errors -std=gnu89
endif

if HAVE_GCC_C94_SUPPORT
compile_tests += ctest-c94

ctest_c94_SOURCES = ctest.c
ctest_c94_CFLAGS = -pedantic-errors -std=iso9899:199409
endif

if HAVE_GCC_C99_SUPPORT
compile_tests += ctest-c99 ctest-gnu99

ctest_c99_SOURCES = ctest.c
ctest_c99_CFLAGS = -pedantic-errors -std=c99

ctest_gnu99_SOURCES = ctest.c
ctest_gnu99_CFLAGS = -pedantic-errors -std=gnu99
endif

if HAVE_GXX_CXX98_SUPPORT
compile_tests += cpptest-cxx98 cpptest-gnuxx98

cpptest_cxx98_SOURCES = cpptest.cc
cpptest_cxx98_CXXFLAGS = -pedantic-errors -std=c++98

cpptest_gnuxx98_SOURCES = cpptest.cc
cpptest_gnuxx98_CXXFLAGS = -pedantic-errors -std=gnu++98
endif

unit_tests = \
	test-bcd \
	test-dvb_demux \
	test-dvb_mux \
	test-hamm \
	test-io \
	test-raw_decoder \
	test-vps

TESTS = \
	$(compile_tests) \
	$(unit_tests) \
	test-explist \
	test-headers \
	test-unicode

check_PROGRAMS = \
	$(compile_tests) \
	$(unit_tests)

test_bcd_SOURCES = test-bcd.cc

test_dvb_demux_SOURCES = \
	test-dvb_mux.cc \
	test-common.cc test-common.h

test_dvb_mux_SOURCES = \
	test-dvb_mux.cc \
	test-common.cc test-common.h

test_hamm_SOURCES = test-hamm.cc

test_io_SOURCES = test-io.cc

test_raw_decoder_SOURCES = \
	test-raw_decoder.cc \
	test-common.cc test-common.h

check_SCRIPTS = \
	test-explist \
	test-headers \
	test-unicode

test-explist: explist

test-unicode: unicode

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
	ttxfilter \
	unicode

caption_SOURCES = \
	caption.c \
	sliced.c sliced.h

capture_SOURCES = \
	capture.c \
	sliced.c sliced.h

date_SOURCES = \
	date.c \
	sliced.c sliced.h

decode_SOURCES = \
	decode.c \
	sliced.c sliced.h

event_SOURCES = \
	event.c \
	sliced.c sliced.h

export_SOURCES = \
	export.c \
	sliced.c sliced.h

osc_SOURCES = \
	osc.c \
	sliced.c sliced.h

sliced2pes_SOURCES = \
	sliced2pes.c \
	sliced.c sliced.h

ttxfilter_SOURCES = \
	ttxfilter.c \
	sliced.c sliced.h

noinst_SCRIPTS = \
	uclist

EXTRA_DIST = \
	$(noinst_SCRIPTS) \
	$(check_SCRIPTS) \
	README \
	unicode-out-ref.txt

CLEANFILES = \
	test-hdrs.cc \
	test-hdrs.log \
	test-hdrs.out \
	unicode-out.txt

# $(top_srcdir) for VPATH zvbi.h
AM_CFLAGS = \
	-D_GNU_SOURCE \
	-I$(top_srcdir) -I$(top_srcdir)/src \
	$(COMMON_INCLUDES) $(X_CFLAGS)

AM_CXXFLAGS = \
	-I$(top_srcdir) -I$(top_srcdir)/src \
	$(COMMON_INCLUDES) $(X_CFLAGS)

# For the test-headers script.
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

check-valgrind: $(unit_tests)
	for test in $(unit_tests) ; do \
	  echo Testing $$test ; \
	  valgrind --leak-check=full $$test || \
	    (echo Test failed ; exit 1) ; \
	done

# NB this works only if you add -fprofile-arcs -ftest-coverage
# to CFLAGS and CXXFLAGS when you configure.
check-coverage: $(unit_tests)
	for test in $(unit_tests) ; do \
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

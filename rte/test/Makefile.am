## Process this file with automake to produce Makefile.in

XFAIL_TESTS = \
	io-bad-context \
	io-bad-codec-1 \
	io-bad-codec-2 \
	io-n-codecs-1 \
	io-n-codecs-2 \
	io-n-codecs-3 \
	io-n-codecs-4

TESTS = \
	cpptest \
	io-all-formats \
	optest \
	$(XFAIL_TESTS)

check_PROGRAMS = \
	cpptest \
	info \
	io

cpptest_SOURCES = \
	cpptest.cc

EXTRA_DIST = \
	README \
	io-all-formats \
	optest \
	$(XFAIL_TESTS)

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir)/src \
	$(COMMON_INCLUDES) \
	-D_GNU_SOURCE \
	-D_REENTRANT \
	-D_LARGEFILE64_SOURCE

LDADD = \
	$(top_builddir)/src/.libs/librte.a \
	$(PTHREAD_LIB) \
	-lm \
	$(INTLLIBS)

## rte_test_INCLUDES = $(ESD_CFLAGS)
## rte_test_LDADD = $(ESD_LIBS) $(PTHREAD_LIB) -lm ../src/.libs/librte.a
## rte_test_SOURCES = rte_test.c

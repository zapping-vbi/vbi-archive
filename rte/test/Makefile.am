## Process this file with automake to produce Makefile.in

TESTS = optest cpptest
noinst_PROGRAMS = info io cpptest
cpptest_SOURCES = cpptest.cc

EXTRA_DIST = optest README

INCLUDES = -I$(top_srcdir)/src -I$(top_builddir)/src $(COMMON_INCLUDES) \
	-D_GNU_SOURCE -D_REENTRANT -D_LARGEFILE64_SOURCE \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

LDADD = $(top_builddir)/src/.libs/librte.a $(PTHREAD_LIB) -lm \
	$(LTLIBICONV) $(LTLIBINTL)

## rte_test_INCLUDES = $(ESD_CFLAGS)
## rte_test_LDADD = $(ESD_LIBS) $(PTHREAD_LIB) -lm ../src/.libs/librte.a
## rte_test_SOURCES = rte_test.c

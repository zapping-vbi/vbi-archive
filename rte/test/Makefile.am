## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = info io

INCLUDES = -I$(top_srcdir)/src $(RTE_CFLAGS) $(COMMON_INCLUDES) \
	-D_GNU_SOURCE -D_REENTRANT -D_LARGEFILE64_SOURCE \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

LDADD = $(top_builddir)/src/.libs/librte.a -lpthread -lm

## rte_test_INCLUDES = $(ESD_CFLAGS)
## rte_test_LDADD = $(ESD_LIBS) -lpthread -lm ../src/.libs/librte.a
## rte_test_SOURCES = rte_test.c

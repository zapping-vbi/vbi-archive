## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = rte_test

INCLUDES = -D_GNU_SOURCE -D_REENTRANT -D_LARGEFILE64_SOURCE

rte_test_INCLUDES = $(ESD_CFLAGS)
rte_test_LDADD = $(ESD_LIBS) -lpthread -lm ../src/.libs/librte.a
rte_test_SOURCES = rte_test.c

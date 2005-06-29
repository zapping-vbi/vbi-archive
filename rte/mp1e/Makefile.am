## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.17 2005-06-29 21:27:31 mschimek Exp $

INCLUDES = -D_GNU_SOURCE -D_REENTRANT -include $(top_builddir)/config.h \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

if BACKEND_MP1E

SUBDIRS = video audio systems common
DIST_SUBDIRS = video audio systems common
EXTRA_DIST =

#dist-hook:
#	for file in INSTALL README config.guess config.sub \
#	  configure configure.in depcomp install-sh \
#	  ltmain.sh missing mkinstalldirs; do rm -rf $(distdir)/$$file; done

noinst_LTLIBRARIES = libmp1e.la

## XXX remove options.h
libmp1e_la_SOURCES = b_mp1e.c b_mp1e.h options.h

DISTFILES = \
	$(libmp1e_la_SOURCES) \
	$(srcdir)/Makefile.am $(srcdir)/Makefile.in \
	AUTHORS ChangeLog NEWS

else

SUBDIRS = macros video audio vbi systems common devices test
DIST_SUBDIRS = macros video audio vbi systems common devices test
EXTRA_DIST = mp1e.lsm.in mp1e.spec.in $(man_MANS) BUGS b_mp1e.h rte

dist-hook:
	for file in b_mp1e.c; do \
		rm -rf $(distdir)/$$file; done

bin_PROGRAMS = mp1e

mp1e_SOURCES = main.c options.c global_data.c options.h

mp1e_LDADD = \
	systems/libsystems.la \
	audio/libaudio.la vbi/libvbi.la \
	devices/libdevices.la video/libvideo.la common/libcommon.la \
	test/libtest.la $(X_LIBS) -lm $(PTHREAD_LIB) $(ESD_LIBS)

man_MANS = mp1e.1

endif

DISTCLEANFILES = site_def.h

## Process this file with automake to produce Makefile.in
## Note $(DESTDIR)$(sbindir) also encoded in src/Makefile.am

INCLUDES = -I$(top_srcdir)/src -I$(top_srcdir)/common $(COMMON_INCLUDES) 

bin_PROGRAMS = zapping_setup_fb
bin_SCRIPTS = zapping_fix_overlay

EXTRA_DIST = zapping_setup_fb.pam zapping_fix_overlay.in

DISTCLEANFILES = zsfb.capps

install-binPROGRAMS: $(bin_PROGRAMS) zsfb.capps
if SUID_ZSFB
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(mkinstalldirs) $(DESTDIR)$(sbindir)
	$(INSTALL) -m4755 -o $(ZSFB_OWNER) -g $(ZSFB_GROUP) \
	  zapping_setup_fb $(DESTDIR)$(sbindir)
	ln -sf $(DESTDIR)$(sbindir)/zapping_setup_fb \
	  $(DESTDIR)$(bindir)/zapping_setup_fb
else
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(mkinstalldirs) $(DESTDIR)$(sbindir)
	$(mkinstalldirs) $(DESTDIR)/etc/pam.d
	$(mkinstalldirs) $(DESTDIR)/etc/security/console.apps
	$(INSTALL) zapping_setup_fb $(DESTDIR)$(sbindir)
	ln -sf $(CONSOLEHELPER_LOCATION) $(DESTDIR)$(bindir)/zapping_setup_fb
	if ! test -e $(DESTDIR)/etc/pam.d/zapping_setup_fb ; then \
	  $(INSTALL_DATA) $(top_srcdir)/zapping_setup_fb/zapping_setup_fb.pam \
	  $(DESTDIR)/etc/pam.d/zapping_setup_fb; \
	fi
	cp zsfb.capps $(DESTDIR)/etc/security/console.apps/zapping_setup_fb
endif

uninstall-binPROGRAMS:
if SUID_ZSFB
	rm -f $(DESTDIR)$(bindir)/zapping_setup_fb
else
	rm -f $(DESTDIR)$(sbindir)/zapping_setup_fb
	rm -f $(DESTDIR)$(bindir)/zapping_setup_fb
	rm -f $(DESTDIR)/etc/security/console.apps/zapping_setup_fb
## Should we really remove this file? The administrator might have
## changed it...
	rm -f $(DESTDIR)/etc/pam.d/zapping_setup_fb
endif

zapping_setup_fb_SOURCES = \
	zapping_setup_fb.c zapping_setup_fb.h \
	dga.c v4l.c v4l2.c v4l25.c

zapping_setup_fb_LDADD = $(X_LIBS) \
	-lX11 -lXext $(X_XF86DGA_LIBS) $(X_EXTRA_LIBS)

zsfb.capps:
	echo USER=root >$@
	echo SESSION=TRUE >>$@
	echo PROGRAM=$(sbindir)/zapping_setup_fb >>$@

## Process this file with automake to produce Makefile.in

INCLUDES = $(ARCH_CFLAGS)

bin_PROGRAMS = zapping_setup_fb

EXTRA_DIST = zapping_setup_fb.pam zsfb.capps.in

if SUID_ZSFB
install-binPROGRAMS: $(bin_PROGRAMS)
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(INSTALL) -m4755 -o root -g root zapping_setup_fb $(DESTDIR)$(bindir)

uninstall-binPROGRAMS:
	rm -f $(DESTDIR)$(bindir)/zapping_setup_fb
else
install-binPROGRAMS: $(bin_PROGRAMS)
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(mkinstalldirs) $(DESTDIR)$(sbindir)
	$(mkinstalldirs) $(DESTDIR)/etc/pam.d
	$(mkinstalldirs) $(DESTDIR)/etc/security/console.apps
	$(INSTALL) zapping_setup_fb $(DESTDIR)$(sbindir)
	ln -sf $(CONSOLEHELPER_LOCATION) $(DESTDIR)$(bindir)/zapping_setup_fb
	if ! test -e $(DESTDIR)/etc/pam.d/zapping_setup_fb ; then\
		$(INSTALL_DATA) zapping_setup_fb.pam \
			$(DESTDIR)/etc/pam.d/zapping_setup_fb;\
	fi
	cp zsfb.capps $(DESTDIR)/etc/security/console.apps/zapping_setup_fb

uninstall-binPROGRAMS:
	rm -f $(DESTDIR)$(sbindir)/zapping_setup_fb
	rm -f $(DESTDIR)$(bindir)/zapping_setup_fb
	rm -f $(DESTDIR)/etc/security/console.apps/zapping_setup_fb
## Should we really remove this file? The administrator might have
## changed it...
	rm -f $(DESTDIR)/etc/pam.d/zapping_setup_fb
endif

zapping_setup_fbSOURCES = \
	zapping_setup_fb.c videodev.h

zapping_setup_fb_LDADD = $(X_LIBS) \
-lX11 -lXext -lXxf86dga $(X_EXTRA_LIBS)

## Process this file with automake to produce Makefile.in

INCLUDES = \
$(ARCH_CFLAGS)

bin_PROGRAMS = zapping_setup_fb

EXTRA_DIST = zapping_setup_fb.pam

if SUID_ZSFB
install-binPROGRAMS: $(bin_PROGRAMS)
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(INSTALL) -m455 -o root -g root zapping_setup_fb $(DESTDIR)$(bindir)

uninstall-binPROGRAMS:
	rm -f $(DESTDIR)$(bindir)/zapping_setup_fb
else
install-binPROGRAMS: $(bin_PROGRAMS)
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(mkinstalldirs) $(DESTDIR)$(sbindir)
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/pam.d
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/security/console.apps
	$(INSTALL) zapping_setup_fb $(DESTDIR)$(sbindir)
	ln -sf $(CONSOLEHELPER_LOCATION) $(DESTDIR)$(bindir)/zapping_setup_fb
	if ! test -e $(DESTDIR)$(sysconfdir)/pam.d/zapping_setup_fb ; then\
		$(INSTALL) zapping_setup_fb.pam \
			$(DESTDIR)$(sysconfdir)/pam.d/zapping_setup_fb;\
	fi
	touch $(DESTDIR)$(sysconfdir)/security/console.apps/zapping_setup_fb

uninstall-binPROGRAMS:
	rm -f $(DESTDIR)$(sbindir)/zapping_setup_fb
	rm -f $(DESTDIR)$(bindir)/zapping_setup_fb
	rm -f $(DESTDIR)$(sysconfdir)/security/console.apps/zapping_setup_fb
## Should we really remove this file? The administrator might have
## changed it...
	rm -f $(DESTDIR)$(sysconfdir)/pam.d/zapping_setup_fb
endif

zapping_setup_fbSOURCES = \
	zapping_setup_fb.c videodev.h

zapping_setup_fb_LDADD = -L/usr/X11R6/lib -L/usr/lib -L/usr/X11/lib \
-lX11 -lXext -lXxf86dga

## Process this file with automake to produce Makefile.in

EXTRA_DIST = index.html topic.dat

zapping_helpdir = `$(GNOME_CONFIG) --datadir`/gnome/help/zapping/C

install-data-local:
	$(mkinstalldirs) $(zapping_helpdir)
	$(INSTALL_DATA) $(srcdir)/topic.dat $(zapping_helpdir)/topic.dat
	$(INSTALL_DATA) $(srcdir)/index.html $(zapping_helpdir)/index.html
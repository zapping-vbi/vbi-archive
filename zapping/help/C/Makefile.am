## Process this file with automake to produce Makefile.in

zapping_helpfiles = \
index.html plugin_devel.html properties.html \
device_info.jpeg general_options.jpeg topic.dat

EXTRA_DIST = $(zapping_helpfiles)

zapping_helpdir = $(datadir)/gnome/help/zapping/C

install-data-local:
	$(mkinstalldirs) $(zapping_helpdir)
	for i in $(zapping_helpfiles); do \
	  if test -f $$i; then \
		$(INSTALL_DATA) $(srcdir)/$$i $(zapping_helpdir); \
	  fi \
	done

uninstall-local:
	if test -d $(zapping_helpdir); then \
	  rm -rf $(zapping_helpdir); \
	fi

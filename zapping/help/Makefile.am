## Process this file with automake to produce Makefile.in.

SUBDIRS = C

zapping_helpdir = `$(GNOME_CONFIG) --datadir`/gnome/help/zapping

uninstall-local:
	if test -d $(zapping_helpdir); then \
	  rm -rf $(zapping_helpdir); \
	fi
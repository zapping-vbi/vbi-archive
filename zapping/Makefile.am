## Process this file with automake to produce Makefile.in

if NEED_ZSFB
ZSFB_DIRS = zapping_setup_fb
else
ZSFB_DIRS =
endif

SUBDIRS = common libtv po glade pixmaps plugins help $(ZSFB_DIRS) src 

EXTRA_DIST = autogen.sh acinclude.m4 \
	README.plugins BUGS THANKS \
	intltool-extract.in intltool-merge.in intltool-update.in \
	zapping.desktop.in zapping.schemas

Multimediadir = $(datadir)/gnome/apps/Multimedia
Multimedia_in_files = zapping.desktop.in
# Gnome docs recommend $(Multimedia_in_files:.desktop.in=.desktop)
# but that's gmake-ish.
Multimedia_DATA = zapping.desktop
@INTLTOOL_DESKTOP_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	gconftool-2 --makefile-install-rule $(srcdir)/zapping.schemas
else
install-data-local:
endif

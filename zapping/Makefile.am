## Process this file with automake to produce Makefile.in

if NEED_ZSFB
ZSFB_DIRS = zapping_setup_fb
else
ZSFB_DIRS =
endif

# Keep order
SUBDIRS = \
	common \
	libtv \
	libvbi \
	po \
	glade \
	pixmaps \
	plugins \
	help \
	$(ZSFB_DIRS) \
	src 

EXTRA_DIST = \
	acinclude.m4 \
	autogen.sh \
	BUGS \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	README.plugins \
	THANKS \
	zapping.desktop.in \
	zapping.schemas

desktopdir = $(datadir)/applications
desktop_DATA = zapping.desktop
@INTLTOOL_DESKTOP_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	gconftool-2 --makefile-install-rule $(srcdir)/zapping.schemas
uninstall-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	gconftool-2 --makefile-uninstall-rule $(srcdir)/zapping.schemas
else
install-data-local:
uninstall-local:
endif

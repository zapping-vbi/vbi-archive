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
	src \
	test

EXTRA_DIST = \
	acinclude.m4 \
	autogen.sh \
	BUGS \
	ChangeLog.01 \
	ChangeLog.03 \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	README.plugins \
	THANKS \
	zapping.desktop.in \
	zapping.schemas.in

desktopdir = $(datadir)/applications
desktop_DATA = zapping.desktop
@INTLTOOL_DESKTOP_RULE@

schemadir = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = zapping.schemas
@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	gconftool-2 --makefile-install-rule $(top_builddir)/zapping.schemas
uninstall-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	gconftool-2 --makefile-uninstall-rule $(top_builddir)/zapping.schemas
else
install-data-local:
uninstall-local:
endif

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update \
	zapping.desktop \
	zapping.schemas

# configure creates site_def.h if it doesn't already exist.
distcleancheck_listfiles = \
	find . -name site_def.h -prune -o -type f -print

# make uninstall leaves a scrollkeeper contents skeleton in pkglocalstatedir.
# make distuninstallcheck shall ignore that.
distuninstallcheck_listfiles = \
	find . -name scrollkeeper -prune -o -type f -print

dist-hook:
	chown -R 500:100 $(distdir)

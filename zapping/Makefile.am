## Process this file with automake to produce Makefile.in

SUBDIRS = common libtv po glade pixmaps plugins help zapping_setup_fb src 

EXTRA_DIST = autogen.sh acinclude.m4 \
	README.plugins BUGS THANKS \
	intltool-extract.in intltool-merge.in intltool-update.in \
	zapping.desktop.in

Multimediadir = $(datadir)/gnome/apps/Multimedia
Multimedia_in_files = zapping.desktop.in
# Gnome docs recommend $(Multimedia_in_files:.desktop.in=.desktop)
# but that's gmake-ish.
Multimedia_DATA = zapping.desktop
@INTLTOOL_DESKTOP_RULE@

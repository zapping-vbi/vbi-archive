## Process this file with automake to produce Makefile.in

SUBDIRS = common po glade pixmaps plugins help zapping_setup_fb src 

EXTRA_DIST = autogen.sh acinclude.m4 \
	README.plugins BUGS THANKS \
	intltool-extract.in intltool-merge.in intltool-update.in \
	zapping.desktop.in

Multimediadir = $(datadir)/gnome/apps/Multimedia
Multimedia_in_files = zapping.desktop.in
Multimedia_DATA = $(Multimedia_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

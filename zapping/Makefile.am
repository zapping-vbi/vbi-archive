## Process this file with automake to produce Makefile.in

Multimediadir = $(datadir)/gnome/apps/Multimedia
Multimedia_DATA = zapping.desktop

EXTRA_DIST = config.rpath  $(Multimedia_DATA) \
README.plugins BUGS THANKS zapping.spec.in zapping.spec prepare_dist.sh \
autogen.sh

SUBDIRS = m4  intl po m4 macros common src zapping_setup_fb glade help \
plugins pixmaps

ACLOCAL_AMFLAGS = -I m4

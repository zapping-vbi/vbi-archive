## Process this file with automake to produce Makefile.in

SUBDIRS = \
	alirc \
	screenshot \
	mpeg

# Presently it works only with V4L2 drivers.
if BUILD_DEINTERLACE_PLUGIN
SUBDIRS += deinterlace
endif

# Need libzvbi 0.2.x to build this module.
if HAVE_LIBZVBI
SUBDIRS += \
	subtitle \
	teletext
endif

# Not built this time:
# lirc parrot template

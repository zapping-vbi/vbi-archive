## Process this file with automake to produce Makefile.in

SUBDIRS = \
	alirc \
	deinterlace \
	screenshot \
	mpeg

# Need libzvbi 0.2.x to build this module.
if HAVE_LIBZVBI
SUBDIRS += teletext
endif

# Not built this time:
# lirc parrot template

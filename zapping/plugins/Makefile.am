## Process this file with automake to produce Makefile.in

SUBDIRS = \
	alirc \
	screenshot \
	mpeg

# Need libzvbi 0.2.x to build this module.
if HAVE_LIBZVBI
SUBDIRS += teletext
endif

if HAVE_MMX
SUBDIRS += deinterlace
endif
if HAVE_ALTIVEC
SUBDIRS += deinterlace
endif

# Not built this time:
# lirc parrot template

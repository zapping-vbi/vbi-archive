## Process this file with automake to produce Makefile.in

SUBDIRS = \
	alirc \
	screenshot \
	mpeg

# Need libzvbi 0.2.x to build this module.
if HAVE_LIBZVBI
SUBDIRS += teletext
endif

# Cannot build for target x86_64, code is 32 bit (being worked on).
if HAVE_X86
# Need gcc and as support for MMX, SSE, SSE2 to build this module.
# HAVE_SSE2 implies working SSE2 intrinsics, which is not necessary.
if HAVE_SSE2_ASM
SUBDIRS += deinterlace
endif
endif

# Not built this time:
# lirc parrot template

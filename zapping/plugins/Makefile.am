## Process this file with automake to produce Makefile.in

# Need libzvbi 0.2.x to build this module.
if HAVE_LIBZVBI
teletext_plugin = teletext
else
teletext_plugin =
endif

# Need gcc and as support for MMX, SSE, SSE2 to build this module.
# Cannot build for target x86_64, code is 32 bit.
if HAVE_X86
if HAVE_SSE2
deinterlace_plugin = deinterlace
else
deinterlace_plugin =
endif
else
deinterlace_plugin =
endif

SUBDIRS = \
	alirc \
	screenshot \
	mpeg \
	$(teletext_plugin) \
	$(deinterlace_plugin)

# Not built this time:
# lirc parrot template

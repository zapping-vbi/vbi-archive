noinst_LIBRARIES = libvbi.a

EXTRA_DIST = wstfont.xbm ccfont.xbm README

CFLAGS = -g -O2 -Wall -Wunused -Wmissing-prototypes -Wmissing-declarations
CFLAGS += -D_REENTRANT -D_XOPEN_SOURCE=500 -D_BSD_SOURCE

INCLUDES = $(X_CFLAGS) $(UNICODE_CFLAGS)

libvbi_a_SOURCES = \
	decoder.c decoder.h sliced.h \
	v4lx.c v4lx.h \
	vbi.c vbi.h libvbi.h \
	packet.c teletext.c vt.h \
	caption.c cc.h \
	trigger.c trigger.h \
	search.c \
	cache.c cache.h \
	exp-html.c exp-txt.c exp-vtx.c exp-gfx.c export.c export.h format.h \
	hamm.c hamm.h \
	lang.c lang.h \
	tables.c tables.h

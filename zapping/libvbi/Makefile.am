noinst_LIBRARIES = libvbi.a

EXTRA_DIST = wstfont.xbm ccfont.xbm README

CFLAGS = -D_GNU_SOURCE -g -O2 -Wall $(ARCH_CFLAGS) $(UNICODE_CFLAGS)

libvbi_a_SOURCES = \
	decoder.c decoder.h sliced.h v4lx.c v4lx.h \
	vbi.c vbi.h libvbi.h \
	packet.c teletext.c vt.h \
	caption.c cc.h \
	search.c \
	cache.c cache.h \
	exp-gfx.c exp-html.c exp-txt.c exp-vtx.c export.c export.h format.h \
	dllist.h \
	hamm.c hamm.h \
	lang.c lang.h \
	tables.c tables.h \
	os.h

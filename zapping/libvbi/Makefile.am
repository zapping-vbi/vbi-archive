noinst_LIBRARIES = libvbi.a

EXTRA_DIST = wstfont.xbm README

## mhs testing new vbi code; CFLAGS = -w $(ARCH_CFLAGS)
CFLAGS = -D_GNU_SOURCE -g -O0 -Wall $(ARCH_CFLAGS)

libvbi_a_SOURCES = \
	decoder.c decoder.h v4lx.c v4lx.h \
	cache.c cache.h \
	exp-gfx.c exp-html.c exp-txt.c export.c export.h \
	dllist.h \
##	fdset.c fdset.h \
##	font.c font.h fontsize.h \
	hamm.c hamm.h \
	lang.c lang.h \
	misc.c misc.h \
	vbi.c vbi.h libvbi.h \
	tables.c tables.h \
##	vbi_v4l2.c vbi_v4l.c \
	os.h vt.h

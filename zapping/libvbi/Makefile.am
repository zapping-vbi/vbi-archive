noinst_LIBRARIES = libvbi.a

EXTRA_DIST = font1.xbm font2.xbm font3.xbm font4.xbm README

## Avoid warning messages (i'm not going to maintain this code)
CFLAGS = -w $(ARCH_CFLAGS)

libvbi_a_SOURCES = \
	cache.c cache.h \
	exp-gfx.c exp-html.c exp-txt.c export.c export.h \
	dllist.h \
	fdset.c fdset.h \
	font.c font.h fontsize.h \
	hamm.c hamm.h \
	lang.c lang.h \
	misc.c misc.h \
	vbi.c vbi.h libvbi.h \
	vbi_v4l2.c vbi_v4l.c \
	os.h vt.h
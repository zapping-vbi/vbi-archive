## Process this file with automake to produce Makefile.in

noinst_LTLIBRARIES = libi386.la

##
## -O2 and -fomit-frame-pointer are required to free
## enough regs for inline asm()
##
CFLAGS = -D_REENTRANT -DHAVE_AV_CONFIG_H -O2 -fomit-frame-pointer \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

EXTRA_DIST = mpegvideo_mmx_template.c

libi386_la_SOURCES = \
	cputest.c dsputil_mmx.c dsputil_mmx_avg.h fdct_mmx.c idct_mmx.c \
	mmx.h motion_est_mmx.c mpegvideo_mmx.c simple_idct_mmx.c

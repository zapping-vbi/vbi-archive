## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = libzvbi-0.3.la

structpr = structpr.pl

BUILT_SOURCES = \
	_videodev.h \
	_videodev2.h \
	_videodev2k.h \
	ccfont3.xbm \
	network-table.h \
	version.h \
	zvbi.h \
	wstfont2.xbm

EXTRA_DIST = $(structpr) network-table.pl $(BUILT_SOURCES)
CLEANFILES = $(BUILT_SOURCES)

INCLUDES = \
	-W -Wall -Wunused -Wfloat-equal \
	-Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align \
	-Wwrite-strings -Winline -Wmissing-prototypes \
	-Wmissing-declarations
#	-Wpadded -Wconversion
INCLUDES += -D_REENTRANT -D_XOPEN_SOURCE -D_BSD_SOURCE
INCLUDES += $(X_CFLAGS) $(UNICODE_CFLAGS)

INSTALLHDRS = \
	aspect_ratio.h \
	bcd.h \
	bit_slicer.h \
	cache.h \
	caption_decoder.h \
	conv.h \
	event.h \
	export.h \
	exp-gfx.h \
	exp-txt.h \
	format.h \
	graphics.h \
	hamm.h \
	intl.h \
	io.h \
	io-dvb.h \
	io-sim.h \
	lang.h \
	link.h \
	macros.h \
	network.h \
	packet-830.h \
	pdc.h \
	pfc_demux.h \
	program_info.h \
	raw_decoder.h \
	sampling.h \
	search.h \
	sliced.h \
	teletext_decoder.h \
	trigger.h \
	vbi.h \
	vbi_decoder.h \
	version.h \
	vps.h \
	wss.h \
	xds_demux.h \
	xds_decoder.h

libzvbi_0_3_la_SOURCES = \
	$(INSTALLHDRS) \
	bcd.c \
	bit_slicer.c \
	cache.c cache-priv.h \
	caption.c cc.h \
	conv.c \
	dlist.h \
	event.c \
	export.c export-priv.h \
	exp-gfx.c \
	exp-html.c \
	exp-sub.c \
	exp-templ.c \
	exp-txt.c \
	exp-vtx.c \
	format.h \
	graphics.c \
	hamm.c \
	intl-priv.h \
	io.c io-priv.h \
	io-bktr.c \
	io-dvb.c \
	io-sim.c \
	io-v4l2k.c videodev2k.h \
	io-v4l2.c videodev2.h \
	io-v4l.c videodev.h \
	lang.c \
	link.c \
	misc.h \
	network.c \
	packet.c vt.h \
	packet-830.c \
	page-priv.h \
	pdc.c \
	pfc_demux.c \
	program_info.c \
	raw_decoder.c \
	sampling.c \
	search.c \
	tables.c tables.h \
	teletext.c \
	top.c \
	trigger.c \
	ure.c ure.h \
	vbi_decoder.c vbi_decoder-priv.h \
	vbi.c \
	vps.c \
	wss.c \
	xds_demux.c \
	xds_decoder.c

libzvbi_0_3includedir = $(includedir)/zvbi-0.3/zvbi
libzvbi_0_3include_HEADERS = $(INSTALLHDRS) version.h zvbi.h

exp-gfx.c: wstfont2.xbm ccfont3.xbm

wstfont2.xbm: wstfont2.pbm fontgen Makefile.am
	$(top_builddir)/src/fontgen xbm wstfont2 10 < $< > $@
ccfont3.xbm: ccfont3.pbm fontgen Makefile.am
	$(top_builddir)/src/fontgen xbm ccfont3 13 < $< > $@

libzvbi_0_3_la_LIBADD = $(PTHREAD_LIB) $(PNG_LIB) $(UNICODE_LIBS)
libzvbi_0_3_la_LDFLAGS = -version-info $(LIBZVBI_SO_VERSION)

version.h: Makefile.am
	echo -e "#ifndef __ZVBI_VERSION_H__" > $@
	echo -e "#define __ZVBI_VERSION_H__\n" >> $@
	echo -e "\n#define VBI_VERSION_MAJOR" `echo @VERSION@ \
	 | sed "s/[^0-9.]//g" \
	 | sed "s/\./\\\\\n#define VBI_VERSION_MINOR /" \
	 | sed "s/\./\\\\\n#define VBI_VERSION_MICRO /"` >> $@
	echo -e "\n#endif /* __ZVBI_VERSION_H__ */" >> $@

zvbi.h: $(INSTALLHDRS) version.h Makefile.am
	echo -e "#ifndef __ZVBI_H__\n#define __ZVBI_H__\n" > $@
	for i in $(INSTALLHDRS); do \
		echo -e "#include \"$$i\"" >> $@; \
	done
	echo -e "#include \"version.h\"\n" >> $@
	echo -e "\n#endif /* __ZVBI_H__ */" >> $@

network-table.h: network-table.pl Makefile.am
	wget -q -O- http://zapping.sourceforge.net/zvbi-0.3/networks.xml | \
	  $(srcdir)/network-table.pl > $@

icheck: $(libzvbi_0_3include_HEADERS)
	for i in $(libzvbi_0_3include_HEADERS); do \
		echo Checking $$i; \
		grep "VBI_BEGIN_DECLS" $$i >/dev/null || exit 1; \
		grep "VBI_END_DECLS" $$i >/dev/null || exit 1; \
		echo -e "#include \"$$i\"" >_include_test.c; \
		$(CC) -c _include_test.c || exit 1; \
	done
	rm _include_test*

_videodev.h: Makefile.am $(structpr) videodev.h
	$(srcdir)/$(structpr) <$(srcdir)/videodev.h \
	video_device.type=VFL_TYPE_ \
	video_device.hardware=VID_HARDWARE_ \
	video_capability.type=VID_TYPE_ \
	video_channel=R \
	video_channel.channel=WR \
	video_channel.norm=WR \
	video_channel.flags=VIDEO_VC_ \
	video_channel.type=VIDEO_TYPE_ \
	video_tuner=R \
	video_tuner.tuner=WR \
	video_tuner.flags=VIDEO_TUNER_ \
	video_tuner.mode=WR,VIDEO_MODE_ \
	video_picture.palette=VIDEO_PALETTE_ \
	video_audio.flags=VIDEO_AUDIO_ \
	video_audio.mode=VIDEO_SOUND_ \
	video_window.flags=VIDEO_WINDOW_ \
	video_capture.flags=VIDEO_CAPTURE_ \
	vbi_format.flags=VBI_ \
	> $@

# XXX v4l2_pix_format.flags has V4L2_FMT_CS_field subset.
# XXX v4l2_timecode.flags has V4L2_TC_USERBITS_field subset.
# XXX v4l2_buffer.flags has V4L2_BUF_FLAG_, V4L2_BUF_ATTR_, V4L2_BUF_REQ_.
_videodev2.h: Makefile.am $(structpr) videodev2.h
	$(srcdir)/$(structpr) <$(srcdir)/videodev2.h \
	v4l2_capability.type=V4L2_TYPE_ \
	v4l2_capability.flags=V4L2_FLAG_ \
	v4l2_pix_format.pixelformat=V4L2_PIX_FMT_ \
	v4l2_pix_format.flags=V4L2_FMT_FLAG_ \
	v4l2_fmtdesc=R \
	v4l2_fmtdesc.index=WR \
	v4l2_fmtdesc.pixelformat=V4L2_PIX_FMT_ \
	v4l2_cvtdesc=R \
	v4l2_cvtdesc.index=WR \
	v4l2_fxdesc=R \
	v4l2_fxdesc.index=WR \
	v4l2_timecode.flags=V4L2_TC_FLAG_ \
	v4l2_timecode.type=V4L2_TC_TYPE_ \
	v4l2_compression=WR \
	v4l2_requestbuffers=WR \
	v4l2_requestbuffers.type=V4L2_BUF_TYPE_ \
	v4l2_buffer=R \
	v4l2_buffer.index=WR \
	v4l2_buffer.type=WR,V4L2_BUF_TYPE_ \
	v4l2_buffer.flags=V4L2_BUF_FLAG_ \
	v4l2_framebuffer.capability=V4L2_FBUF_CAP_ \
	v4l2_framebuffer.flags=V4L2_FBUF_FLAG_ \
	v4l2_captureparm.capability=V4L2_CAP_ \
	v4l2_captureparm.capturemode=V4L2_MODE_ \
	v4l2_zoomcap.capability=V4L2_ZOOM_ \
	v4l2_standard.colorstandard=V4L2_COLOR_STD_, \
				    V4L2_COLOR_STD_PAL:colorstandard_data.pal, \
				    V4L2_COLOR_STD_NTSC:colorstandard_data.ntsc, \
				    V4L2_COLOR_STD_SECAM:colorstandard_data.secam \
	v4l2_standard.transmission=V4L2_TRANSM_STD_ \
	v4l2_enumstd=R \
	v4l2_enumstd.index=WR \
	v4l2_input=R \
	v4l2_input.index=WR \
	v4l2_input.type=V4L2_INPUT_TYPE_ \
	v4l2_input.capability=V4L2_INPUT_CAP_ \
	v4l2_output=R \
	v4l2_output.index=WR \
	v4l2_output.type=V4L2_OUTPUT_TYPE_ \
	v4l2_output.capability=V4L2_OUTPUT_CAP_ \
	v4l2_control.id=V4L2_CID_ \
	v4l2_queryctrl=R \
	v4l2_queryctrl.id=WR,V4L2_CID_ \
	v4l2_queryctrl.type=V4L2_CTRL_TYPE_ \
	v4l2_queryctrl.flags=V4L2_CTRL_FLAG_ \
	v4l2_queryctrl.group=string \
	v4l2_queryctrl.category=V4L2_CTRL_CAT_ \
	v4l2_querymenu=R \
	v4l2_querymenu.id=WR,V4L2_CID_ \
	v4l2_querymenu.index=WR \
	v4l2_fxcontrol.id=V4L2_CID_ \
	v4l2_tuner=R \
	v4l2_tuner.input=WR \
	v4l2_tuner.capability=V4L2_TUNER_CAP_ \
	v4l2_tuner.rxsubchans=V4L2_TUNER_SUB_ \
	v4l2_tuner.audmode=V4L2_TUNER_MODE_ \
	v4l2_modulator=R \
	v4l2_modulator.output=WR \
	v4l2_modulator.capability=V4L2_TUNER_CAP_ \
	v4l2_modulator.txsubchans=V4L2_TUNER_SUB_ \
	v4l2_audio=R \
	v4l2_audio.audio=WR \
	v4l2_audio.capability=V4L2_AUDCAP_ \
	v4l2_audio.mode=V4L2_AUDMODE_ \
	v4l2_audioout=R \
	v4l2_audioout.audio=WR \
	v4l2_vbi_format.sample_format=V4L2_VBI_SF_ \
	v4l2_vbi_format.flags=V4L2_VBI_ \
	v4l2_format=R \
	v4l2_format.type=WR,V4L2_BUF_TYPE_, \
		V4L2_BUF_TYPE_CAPTURE:fmt.pix, \
		V4L2_BUF_TYPE_VBI:fmt.vbi \
	v4l2_streamparm=R \
	v4l2_streamparm.type=WR,V4L2_BUF_TYPE_, \
		V4L2_BUF_TYPE_CAPTURE:parm.capture, \
		V4L2_BUF_TYPE_VIDEOOUT:parm.output \
	> $@

# XXX v4l2_timecode.flags has V4L2_TC_USERBITS_field subset.
_videodev2k.h: Makefile.am $(structpr) videodev2k.h
	$(srcdir)/$(structpr) <$(srcdir)/videodev2k.h \
	"v4l2_fract={ fprintf (fp, \"%u/%u\", t->numerator, t->denominator); }" \
	v4l2_capability.driver=string \
	v4l2_capability.card=string \
	v4l2_capability.bus_info=string \
	v4l2_capability.version=hex \
	v4l2_capability.capabilities=V4L2_CAP_ \
	v4l2_pix_format.pixelformat=V4L2_PIX_FMT_ \
	v4l2_fmtdesc=R \
	v4l2_fmtdesc.index=WR \
	v4l2_fmtdesc.flags=V4L2_FMT_FLAG_ \
	v4l2_fmtdesc.description=string \
	v4l2_fmtdesc.pixelformat=fourcc \
	v4l2_timecode.type=V4L2_TC_TYPE_ \
	v4l2_timecode.flags=V4L2_TC_FLAG_ \
	v4l2_jpegcompression.jpeg_markers=V4L2_JPEG_MARKER_ \
	v4l2_buffer=R \
	v4l2_buffer.index=WR \
	v4l2_buffer.type=WR \
	v4l2_buffer.flags=WR,V4L2_BUF_FLAG_ \
	v4l2_buffer.memory=V4L2_MEMORY_MMAP:m.offset, \
			   V4L2_MEMORY_USERPTR:m.userptr \
	v4l2_framebuffer.capability=V4L2_FBUF_CAP_ \
	v4l2_framebuffer.flags=V4L2_FBUF_FLAG_ \
	v4l2_captureparm.capability=V4L2_CAP_ \
	v4l2_captureparm.capturemode=V4L2_MODE_ \
	v4l2_std_id=V4L2_STD_ \
	v4l2_standard=R \
	v4l2_standard.index=WR \
	v4l2_input=R \
	v4l2_input.index=WR \
	v4l2_input.type=V4L2_INPUT_TYPE_ \
	v4l2_input.status=V4L2_IN_ST_ \
	v4l2_output=R \
	v4l2_output.index=WR \
	v4l2_output.type=V4L2_OUTPUT_TYPE_ \
	v4l2_control.id=V4L2_CID_ \
	v4l2_queryctrl=R \
	v4l2_queryctrl.id=WR,V4L2_CID_ \
	v4l2_queryctrl.flags=V4L2_CTRL_FLAG_ \
	v4l2_querymenu=R \
	v4l2_querymenu.id=WR,V4L2_CID_ \
	v4l2_querymenu.index=WR,V4L2_CID_ \
	v4l2_tuner=R \
	v4l2_tuner.index=WR \
	v4l2_tuner.capability=V4L2_TUNER_CAP_ \
	v4l2_tuner.rxsubchans=V4L2_TUNER_SUB_ \
	v4l2_tuner.audmode=V4L2_TUNER_MODE_ \
	v4l2_modulator=R \
	v4l2_modulator.index=WR \
	v4l2_modulator.capability=V4L2_TUNER_CAP_ \
	v4l2_modulator.txsubchans=V4L2_TUNER_SUB_ \
	v4l2_audio=R \
	v4l2_audio.index=WR \
	v4l2_audio.capability=V4L2_AUDCAP_ \
	v4l2_audio.mode=V4L2_AUDMODE_ \
	v4l2_audioout=R \
	v4l2_audioout.index=WR \
	v4l2_vbi_format.sample_format=V4L2_PIX_FMT_ \
	v4l2_vbi_format.flags=V4L2_VBI_ \
	v4l2_format.type=V4L2_BUF_TYPE_VIDEO_CAPTURE:fmt.pix, \
			 V4L2_BUF_TYPE_VIDEO_OVERLAY:fmt.win, \
			 V4L2_BUF_TYPE_VBI_CAPTURE:fmt.vbi \
	> $@

_bktr.h: Makefile.am $(structpr) ioctl_bt848.h ioctl_meteor.h
	cat $(srcdir)/ioctl_bt848.h $(srcdir)/ioctl_meteor.h | \
	$(srcdir)/$(structpr) \
	> $@

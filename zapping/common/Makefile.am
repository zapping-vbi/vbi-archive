noinst_LIBRARIES = libcommon.a

libcommon_a_SOURCES = \
	device.c device.h \
	fifo.h fifo.c \
	alloc.h alloc.c \
	threads.h \
	list.h \
	types.h \
	math.h \
	videodev.h videodev2.h videodev25.h \
	ioctl_meteor.h ioctl_bt848.h

structpr = structpr_gen.pl

BUILT_SOURCES = \
	_videodev.h \
	_videodev2.h \
	_videodev25.h \
	_bktr.h

EXTRA_DIST = \
	README \
	$(structpr) \
	$(BUILT_SOURCES)

INCLUDES = $(DEFAULT_CFLAGS) $(UNICODE_CFLAGS) @COMMON_INCLUDES@
	-I$(srcdir)

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
_videodev25.h: Makefile.am $(structpr) videodev25.h
	$(srcdir)/$(structpr) <$(srcdir)/videodev25.h \
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

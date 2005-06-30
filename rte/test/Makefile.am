## Process this file with automake to produce Makefile.in

XFAIL_TESTS = \
	io-bad-context \
	io-bad-codec-1 \
	io-bad-codec-2 \
	io-n-codecs-1 \
	io-n-codecs-2 \
	io-n-codecs-3 \
	io-n-codecs-4

TESTS = \
	cpptest \
	io-all-formats \
	optest \
	$(XFAIL_TESTS)

check_PROGRAMS = \
	cpptest \
	info \
	io

cpptest_SOURCES = \
	cpptest.cc

EXTRA_DIST = \
	README \
	io-all-formats \
	optest \
	$(XFAIL_TESTS)

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir)/src \
	$(COMMON_INCLUDES) \
	-D_GNU_SOURCE \
	-D_REENTRANT \
	-D_LARGEFILE64_SOURCE

LDADD = \
	$(top_builddir)/src/.libs/librte.a \
	$(PTHREAD_LIB) \
	-lm \
	$(INTLLIBS)

# Output of io-all-formats (make check)
CLEANFILES = \
	mp1e_mpeg1_ps-mpeg1_video.mpg \
	mp1e_mpeg1_ps-mpeg1_video-mpeg1_audio_layer2.mpg \
	mp1e_mpeg1_ps-mpeg1_audio_layer2.mpg \
	mp1e_mpeg1_ps-mpeg1_video-mpeg2_audio_layer2.mpg \
	mp1e_mpeg1_ps-mpeg2_audio_layer2.mpg \
	mp1e_mpeg1_vcd-mpeg1_video-mpeg1_audio_layer2.mpg \
	mp1e_mpeg_video-mpeg1_video.mpg \
	mp1e_mpeg_audio-mpeg1_audio_layer2.mp2 \
	mp1e_mpeg_audio-mpeg2_audio_layer2.mp2 \
	ffmpeg_mpeg1_ps-mpeg1_video-mpeg1_audio_layer2.mpg \
	ffmpeg_real-rv10_video-ac3_audio.rm \
	ffmpeg_asf-msmpeg4v3_video-mpeg1_audio_layer2.asf \
	ffmpeg_swf-mjpeg_video-mpeg1_audio_layer2.swf \
	ffmpeg_avi-msmpeg4v3_video-mpeg1_audio_layer2.avi \
	ffmpeg_mpeg_video-mpeg1_video.mpg \
	ffmpeg_mpeg_video-h263_video.mpg \
	ffmpeg_mpeg_video-h263p_video.mpg \
	ffmpeg_mpeg_video-mpeg4_video.mpg \
	ffmpeg_mpeg_video-msmpeg4v1_video.mpg \
	ffmpeg_mpeg_video-msmpeg4v2_video.mpg \
	ffmpeg_mpeg_video-msmpeg4v2_video.mpg \
	ffmpeg_mpeg_video-mjpeg_video.mjpg \
	ffmpeg_mpeg_video-rv10_video.rm \
	ffmpeg_riff_wave-mpeg1_audio_layer2.wav \
	ffmpeg_riff_wave-mpeg2_audio_layer2.wav \
	ffmpeg_riff_wave-ac3_audio.wav \
	ffmpeg_riff_wave-pcm_s16le.wav \
	ffmpeg_riff_wave-pcm_u8.wav \
	ffmpeg_riff_wave-pcm_alaw.wav \
	ffmpeg_riff_wave-pcm_mulaw.wav \
	ffmpeg_mpeg_audio-mpeg1_audio_layer2.mp2 \
	ffmpeg_mpeg_audio-mpeg2_audio_layer2.mp2 \
	ffmpeg_ac3_audio-ac3_audio.ac3 \
	ffmpeg_au_audio-pcm_s16be.au

## rte_test_INCLUDES = $(ESD_CFLAGS)
## rte_test_LDADD = $(ESD_LIBS) $(PTHREAD_LIB) -lm ../src/.libs/librte.a
## rte_test_SOURCES = rte_test.c

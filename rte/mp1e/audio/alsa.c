/*
 *  MPEG Real Time Encoder
 *  Advanced Linux Sound Architecture interface
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *
 *  ALSA PCM interface based on http://revengershome.cjb.net/xmms-recorder
 *  Copyright (C) 2000 Thomas Skopkó <skopko@tigris.klte.hu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: alsa.c,v 1.16 2002-02-20 19:58:33 garetxe Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "../common/log.h" 

#include "audio.h"

#ifdef HAVE_ALSA

#define ASSERT_ALSA(what, func, args...)				\
do {									\
	int _err;							\
									\
	if ((_err = func) < 0) {					\
		fprintf(stderr,	"%s:" __FILE__ ":" ISTF1(__LINE__) ": "	\
			"Failed to " what " (%d, %s)\n",		\
			program_invocation_short_name			\
			 ,##args, _err, snd_strerror(_err));		\
		exit(EXIT_FAILURE);					\
	} else if (verbose >= 3) {					\
		fprintf(stderr,	"%s:" __FILE__ ":" ISTF1(__LINE__) ": "	\
			what " - ok\n", program_invocation_short_name	\
			 ,##args);					\
	}								\
} while (0)

#include <string.h>
#include <ctype.h>

#include <sys/asoundlib.h>

#include "../common/math.h" 

#define ALSA_DROP_TEST(x) /* x */
#define ALSA_TIME_LOG(x) /* x */

extern int test_mode;
/* 64 - capture raw audio as ./raw-audio */

#if !defined(SND_LIB_MAJOR) || (SND_LIB_MAJOR == 0 && SND_LIB_MINOR < 9)

static const int alsa_format_preference[][2] = {
	{ SND_PCM_SFMT_S16_LE, RTE_SNDFMT_S16LE },
	{ SND_PCM_SFMT_U16_LE, RTE_SNDFMT_U16LE },
	{ SND_PCM_SFMT_U8, RTE_SNDFMT_U8 },
	{ SND_PCM_SFMT_S8, RTE_SNDFMT_S8 },
	{ -1, -1 }
};

/*
 *  ALSA Library (0.5.10b) PCM Device
 */

struct alsa_context {
	struct pcm_context	pcm;

	snd_pcm_t *		handle;
	snd_pcm_mmap_control_t *control;
	uint8_t *		mmapped;
	int			fd, fd2;
	int			curr_frag;
	int			num_frags;

	double			time;
	double			slag;
	struct tfmem		tfmem;
};

static void
wait_full(fifo *f)
{
	struct alsa_context *alsa = f->user_data;
	buffer *b = PARENT(f->buffers.head, buffer, added);
	struct timeval tv;
	double now;
	int frag, i;

	assert(b->data == NULL); /* no queue */

	ALSA_DROP_TEST(
		if ((rand() % 100) > 95)
			usleep(500000);
	)

	while (!alsa->control->fragments[alsa->curr_frag].data) {
		fd_set rdset;
		ssize_t r;

		switch (alsa->control->status.status) {
		case SND_PCM_STATUS_RUNNING:
			break;

		case SND_PCM_STATUS_OVERRUN:
			/*
			 *  XXX The buffer (fragment) time stamps will suffice
			 *  to detect overrun, but we need some method to reliable
			 *  stamp old fragments, ie. first_lost - current + number
			 *  of lost fragments + number stored until gettimeofday.
			 *  status.overrun doesn't seem to work.
			 */
			ASSERT_ALSA("prepare PCM channel",
				snd_pcm_channel_prepare(alsa->handle,
			     		SND_PCM_CHANNEL_CAPTURE));

			ASSERT_ALSA("restart PCM capturing",
				snd_pcm_channel_go(alsa->handle,
					SND_PCM_CHANNEL_CAPTURE));

			alsa->curr_frag = 0; /* correct? */

			continue;

		default:
			FAIL("Uh-oh. Unexpected ALSA status %d, please report\n"
			     "at http://zapping.sourceforge.net\n",
			     alsa->control->status.status);
		}

		FD_ZERO(&rdset);
		FD_SET(alsa->fd, &rdset);

		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(alsa->fd + 1, &rdset, NULL, NULL, &tv);

		if (r == 0)
			FAIL("ALSA read timeout");
		else if (r < 0)
			FAIL("ALSA select error (%d, %s)",
			     errno, strerror(errno));
	}

	/* See oss.c for discussion */

	i = 5; /* retries */

	do {
		frag = alsa->control->status.frag_io;
		gettimeofday(&tv, NULL);
	} while (frag != alsa->control->status.frag_io && i--);

	if (frag < alsa->curr_frag)
		frag += alsa->num_frags;

	now = tv.tv_sec + tv.tv_usec * (1 / 1e6)
		- (frag - alsa->curr_frag) * alsa->tfmem.ref;

	if (alsa->time > 0) {
#if 1
		alsa->time += alsa->tfmem.ref;
#else
		double dt = now - alsa->time;

		if (dt - alsa->tfmem.err > alsa->tfmem.ref * 1.98) {
			/* data lost, out of sync; XXX 1.98 bad */
			alsa->time = now;
#if 0
			printv(0, "alsa dropped dt=%f t/b=%f\n",
				 dt, alsa->tfmem.ref);
#endif
		} else {
			alsa->time += mp1e_timestamp_filter
				(&alsa->tfmem, dt, 0.05, 1e-7, 0.08);
		}
#if 0
		printv(0, "alsa %f dt %+f err %+f t/b %+f\n",
		       now, dt, alsa->tfmem.err, alsa->tfmem.ref);
#endif
#endif
	} else {
		snd_pcm_channel_status_t status;
		double stime;

                status.channel = SND_PCM_CHANNEL_CAPTURE;
                ASSERT_ALSA("query ALSA channel status",
			    snd_pcm_plugin_status(alsa->handle, &status));

		stime = status.stime.tv_sec                                                     
                        + status.stime.tv_usec * (1 / 1e6);

		alsa->time = now;
		alsa->slag = stime - now;

		ALSA_TIME_LOG(printv(0, "start %f stime %f lag %f\n",
				     now, stime, alsa->slag));
	}

	b->time = alsa->time + alsa->slag;
	b->data = alsa->mmapped + alsa->control->fragments[alsa->curr_frag].addr;

	if (test_mode & 64)
		ASSERT("write raw audio data",
		       write(alsa->fd2, b->data, b->used) == b->used);

	send_full_buffer(&alsa->pcm.producer, b);
}

static void
send_empty(consumer *c, buffer *b)
{
	struct alsa_context *alsa = c->fifo->user_data;

	// XXX
	unlink_node(&c->fifo->full, &b->node);

	b->data = NULL;

	alsa->control->fragments[alsa->curr_frag++].data = 0;

	if (alsa->curr_frag >= alsa->num_frags)
		alsa->curr_frag = 0;
}

static bool
start(fifo *f)
{
	struct alsa_context *alsa = f->user_data;

	ASSERT_ALSA("start PCM capturing",
		snd_pcm_channel_go(alsa->handle,
				   SND_PCM_CHANNEL_CAPTURE));
	return TRUE;
}

void
open_pcm_alsa(char *dev_name1, int sampling_rate, bool stereo, fifo **f)
{
	struct alsa_context *alsa;
	snd_pcm_info_t pcm_info;
	snd_pcm_channel_info_t ch_info;
	snd_pcm_channel_params_t params;
	snd_pcm_channel_setup_t setup;
	int card = 0, device = 0, bpf, i;
	char *dev_name = dev_name1;
	buffer *b;

	while (*dev_name && !isdigit(*dev_name))
		dev_name++;

	sscanf(dev_name, "%d,%d", &card, &device);

	ASSERT("allocate pcm context",
		(alsa = calloc(1, sizeof(struct alsa_context))));

	alsa->pcm.format = RTE_SNDFMT_S16LE;
	alsa->pcm.sampling_rate = sampling_rate;
	alsa->pcm.stereo = stereo;

	ASSERT_ALSA("open ALSA PCM card %d,%d",
		snd_pcm_open(&alsa->handle, card, device, SND_PCM_OPEN_CAPTURE),
		card, device);

	alsa->fd = snd_pcm_file_descriptor(alsa->handle, SND_PCM_CHANNEL_CAPTURE);

	if (test_mode & 64)
		ASSERT("open raw audio file",
		       (alsa->fd2 = open("raw-audio", O_WRONLY | O_CREAT, 0666)) != -1);

	ASSERT_ALSA("obtain PCM device info",
		snd_pcm_info(alsa->handle, &pcm_info));

	memset(&ch_info, 0, sizeof(ch_info));
	ch_info.channel = SND_PCM_CHANNEL_CAPTURE;

	ASSERT_ALSA("obtain PCM capture channel info",
		snd_pcm_channel_info(alsa->handle, &ch_info));

	printv(2, "Opened ALSA 0.5 PCM device, card #%d device #%d (%s)\n",
		card, device, pcm_info.name);

	/*
	 *  I get an error here: -77, File descriptor in bad state
	 *  But xmms-r does the same, w/o error check.
	 */
	snd_pcm_capture_flush(alsa->handle);

#define PROP_CHECK_FAILURE(why, args...)				\
do {									\
	FAIL("Sorry, ALSA device '%s' (%s), subdevice '%s'\n"		\
	     why ".\n", dev_name1, pcm_info.name, ch_info.subname	\
	     ,##args );							\
} while (0)

	/* XXX non-continuous rates, XXX could be converted */
	if (ch_info.min_rate > sampling_rate
	    || ch_info.max_rate < sampling_rate)
		PROP_CHECK_FAILURE("supports only sampling frequencies %d ... %d Hz",
				   ch_info.min_rate, ch_info.max_rate);

	if ((ch_info.flags & (SND_PCM_CHNINFO_MMAP | SND_PCM_CHNINFO_BLOCK))
		!= (SND_PCM_CHNINFO_MMAP | SND_PCM_CHNINFO_BLOCK))
		PROP_CHECK_FAILURE("supports no memory mapping and block mode");

	if (stereo)
		if (!(ch_info.flags & SND_PCM_CHNINFO_INTERLEAVE))
			PROP_CHECK_FAILURE("supports no interleaved stereo");

	for (i = 0;; i++)
		if (alsa_format_preference[i][0] < 0)
			PROP_CHECK_FAILURE("doesn't support sample format U|S 8|16LE");
		else if (ch_info.formats & (1 << alsa_format_preference[i][0]))
			break;

	alsa->pcm.format = alsa_format_preference[i][1]; /* rte_sndfmt */

	if (snd_pcm_format_size(alsa_format_preference[i][0], 1) < 2)
		printv(1, "Warning: ALSA device '%s' (%s), subdevice '%s'\n"
		       "supports no 16 bit LE audio, will compress 8 bit audio\n",
		       dev_name1, pcm_info.name, ch_info.subname);

	bpf = snd_pcm_format_size(alsa_format_preference[i][0], 1 << stereo);

	/*
	 *  xmms-r, makes no sense to me.
	 */
	snd_pcm_munmap(alsa->handle, SND_PCM_CHANNEL_CAPTURE);
	snd_pcm_channel_flush(alsa->handle, SND_PCM_CHANNEL_CAPTURE);

	memset(&params, 0, sizeof(params));

	params.channel = SND_PCM_CHANNEL_CAPTURE;
	params.mode = SND_PCM_MODE_BLOCK;
	params.format.interleave = 1;
	params.format.format = alsa_format_preference[i][0];
	params.format.rate = sampling_rate;
	params.format.voices = stereo + 1;
	params.start_mode = SND_PCM_START_DATA;
	params.stop_mode = SND_PCM_STOP_STOP;
	/* unlike xmms-r, for time stamping: */
	params.buf.block.frag_size = 4096 * bpf;
	params.buf.block.frags_max = -1; /* ? */
	params.buf.block.frags_min = 1;
	params.time = 1;

	ASSERT_ALSA("set PCM channel parameters",
		snd_pcm_channel_params(alsa->handle, &params));

	ASSERT_ALSA("memory map audio buffers",
		snd_pcm_mmap(alsa->handle, SND_PCM_CHANNEL_CAPTURE,
			     &alsa->control, (void **) &alsa->mmapped));

	/*
	 *  xmms-r calls snd_pcm_plugin_prepare here, guess that
	 *  should be channel_prepare.
	 */
	ASSERT_ALSA("prepare PCM channel",
		snd_pcm_channel_prepare(alsa->handle, SND_PCM_CHANNEL_CAPTURE));

	memset(&setup, 0, sizeof(setup));
	setup.channel = SND_PCM_CHANNEL_CAPTURE;
	setup.mode = SND_PCM_MODE_BLOCK; /* xmms-r, purpose? */

	ASSERT_ALSA("query PCM setup",
		snd_pcm_channel_setup(alsa->handle, &setup));

	printv(3, "ALSA setup: ilv %d, format %d (%s), rate %d, voices %d, "
	       "frags %d, frag_size %d, bpf %d\n",
	       setup.format.interleave, setup.format.format,
	       snd_pcm_get_format_name(setup.format.format),
	       setup.format.rate, setup.format.voices,
	       setup.buf.block.frags, setup.buf.block.frag_size, bpf);

	if (setup.format.rate != sampling_rate)
		PROP_CHECK_FAILURE("didn't accept sampling frequency %d, suggests %d Hz",
			sampling_rate, setup.format.rate);

	if (setup.buf.block.frags < 2)
		PROP_CHECK_FAILURE("provided only one audio fragment, "
				   "need at least two");

	alsa->curr_frag = 0;
	alsa->num_frags = setup.buf.block.frags;

	alsa->time = 0.0;
	mp1e_timestamp_init(&alsa->tfmem, (setup.buf.block.frag_size / bpf)
		/ (double) sampling_rate);

	*f = &alsa->pcm.fifo;

	ASSERT("init alsa fifo", init_callback_fifo(*f, "audio-alsa1",
		NULL, NULL, wait_full, send_empty,
		1, 0));

	ASSERT("init alsa producer",
		add_producer(*f, &alsa->pcm.producer));

	(*f)->user_data = alsa;
	(*f)->start = start;

	b = PARENT((*f)->buffers.head, buffer, added);

	b->data = NULL;
	b->used = setup.buf.block.frag_size;
	b->offset = 0;
}

#else /* SND_LIB >= 0.9 */

/*
 *  ALSA Library (0.9.0beta7) PCM Device
 *
 *  This is untested and time stamping chews on the gums,
 *  expect the worst. Try OSS.
 */

struct alsa_context {
	struct pcm_context	pcm;

	snd_pcm_t *		handle;
	int			sleep;
	double			time, start;
	double			buffer_period;
};

static void
wait_full(fifo *f)
{
	struct alsa_context *alsa = f->user_data;
	buffer *b = PARENT(f->buffers.head, buffer, added);
	unsigned char *p;
	double now;
	ssize_t r, n;

	assert(b->data == NULL); /* no queue */

	for (p = b->allocated, n = b->size; n > 0;) {
		r = snd_pcm_readi(alsa->handle, p, n);

		if (r == -EINTR)
			continue;

		if (r == 0 || r == -EAGAIN) {
			usleep(alsa->sleep);
			continue;
		}

		ASSERT_ALSA("read PCM data, %d bytes", r, n);

		p += r;
		n -= r;

		if (r < 200)
			usleep(alsa->sleep);
	}

	now = current_time();

	if (alsa->time > 0) {
		double dt = now - alsa->time;
		double ddt = alsa->buffer_period - dt;
		double q = 128 * fabs(ddt) / alsa->buffer_period;

		alsa->buffer_period = ddt * MIN(q, 0.999) + dt;
		b->time = alsa->time + alsa->start;
		alsa->time += alsa->buffer_period;
	} else {
		b->time = alsa->start = now;
	}

	b->data = b->allocated;

	send_full_buffer(&alsa->pcm.producer, b);
}

static void
send_empty(consumer *c, buffer *b)
{
	// XXX
	unlink_node(&c->fifo->full, &b->node);

	b->data = NULL;
}

static bool
start(fifo *f)
{
	struct alsa_context *alsa = f->user_data;

	ASSERT_ALSA("start PCM capturing",
		snd_pcm_prepare(alsa->handle));

	return TRUE;
}

void
open_pcm_alsa(char *dev_name, int sampling_rate, bool stereo, fifo **f)
{
	struct alsa_context *alsa;
	snd_pcm_info_t *info;
	snd_pcm_hw_params_t *params;
	snd_pcm_sw_params_t *swparams;
	int buffer_time = 500000; /* µs */
	int period_size = 2048; /* frames (channel * bytes_per_sample) */
	int buffer_size, err;
	snd_output_t *log;
	buffer *b;

	FAIL("Sorry, alsa 0.9 interface broken. Please use OSS interface (-p/dev/dsp).\n");

	snd_pcm_info_alloca(&info);
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_sw_params_alloca(&swparams);

	ASSERT("allocate pcm context",
		(alsa = calloc(1, sizeof(struct alsa_context))));

	alsa->pcm.format = RTE_SNDFMT_S16_LE;
	alsa->pcm.sampling_rate = sampling_rate;
	alsa->pcm.stereo = stereo;

	if (snd_output_stdio_attach(&log, stderr, 0) < 0)
		log = NULL;

	if (0 == strncasecmp(dev_name, "alsa", 4))
		dev_name += 4;
	while (*dev_name && !isalnum(*dev_name))
		dev_name++;
	if (!*dev_name)
		dev_name = "default";

	ASSERT_ALSA("open ALSA device '%s'",
		snd_pcm_open(&alsa->handle, dev_name, SND_PCM_STREAM_CAPTURE, 0),
		dev_name);
	
	ASSERT_ALSA("obtain snd_pcm_info", snd_pcm_info(alsa->handle, info));

	printv(2, "Opened ALSA 0.9 PCM device '%s' (%s)\n",
		dev_name, snd_pcm_info_get_name(info));

	ASSERT_ALSA("obtain PCM hw configuration",
		snd_pcm_hw_params_any(alsa->handle, params));
	ASSERT_ALSA("set PCM access mode",
		snd_pcm_hw_params_set_access(alsa->handle,
			params, SND_PCM_ACCESS_RW_INTERLEAVED));
	ASSERT_ALSA("set PCM sample format signed 16 bit LE",
		snd_pcm_hw_params_set_format(alsa->handle,
			params, SND_PCM_FORMAT_S16_LE));
	ASSERT_ALSA("set PCM channels to %d",
		snd_pcm_hw_params_set_channels(alsa->handle,
			params, 1 + !!stereo), 1 + !!stereo);
	ASSERT_ALSA("set PCM sampling rate %d Hz",
		snd_pcm_hw_params_set_rate_near(alsa->handle,
			params, sampling_rate, 0), sampling_rate);
	ASSERT_ALSA("set PCM DMA buffer size",
		snd_pcm_hw_params_set_buffer_time_near(alsa->handle,
			params, buffer_time, 0));
	ASSERT_ALSA("set PCM DMA period size",
		snd_pcm_hw_params_set_period_size(alsa->handle,
			params, period_size, 0));
	if ((err = snd_pcm_hw_params(alsa->handle, params)) < 0) {
		fprintf(stderr, "%s:" __FILE__ ":" ISTF1(__LINE__) ": "
			"Failed to install PCM hw parameters (%d, %s)",
			program_invocation_short_name, err, snd_strerror(err));

		if (log) snd_pcm_hw_params_dump(params, log);

		exit(EXIT_FAILURE);
	}

	period_size = snd_pcm_hw_params_get_period_size(params, 0);
	buffer_size = snd_pcm_hw_params_get_buffer_size(params);
	if (period_size == buffer_size) {
		FAIL("Can't use period equal to buffer size %d", period_size);
		exit(EXIT_FAILURE);
	}

	ASSERT_ALSA("obtain PCM sw configuration",
		snd_pcm_sw_params_current(alsa->handle, swparams));
	ASSERT_ALSA("set PCM sw sleep min 0",
		snd_pcm_sw_params_set_sleep_min(alsa->handle, swparams, 0));
	ASSERT_ALSA("set PCM sw avail min %d",
		snd_pcm_sw_params_set_avail_min(alsa->handle, swparams, period_size),
		period_size);
	ASSERT_ALSA("set PCM xfer align %d",
		snd_pcm_sw_params_set_xfer_align(alsa->handle, swparams, period_size),
		period_size);
	if ((err = snd_pcm_sw_params(alsa->handle, swparams)) < 0) {
		fprintf(stderr, "%s:" __FILE__ ":" ISTF1(__LINE__) ": "
			"Failed to install PCM sw parameters (%d, %s)",
			program_invocation_short_name, err, snd_strerror(err));

		if (log) snd_pcm_sw_params_dump(swparams, log);

		exit(EXIT_FAILURE);
	}

	if (verbose >= 3 && log)
		snd_pcm_dump(alsa->handle, log);

	buffer_size = period_size << (stereo + 1);
	alsa->sleep = 400000.0 * period_size / (double) sampling_rate;

	*f = &alsa->pcm.fifo;

	ASSERT("init alsa fifo", init_callback_fifo(*f, "audio-alsa2",
		NULL, NULL, wait_full, send_empty,
		1, buffer_size));

	ASSERT("init alsa producer",
		add_producer(*f, &alsa->pcm.producer));

	(*f)->user_data = alsa;
	(*f)->start = start;

	b = PARENT((*f)->buffers.head, buffer, added);

	b->data = NULL;
	b->used = b->size;
	b->offset = 0;
}

#endif /* SND_LIB >= 0.9 */

#else /* !HAVE_ALSA */

void
open_pcm_alsa(char *dev_name, int sampling_rate, bool stereo, fifo **f)
{
	FAIL("Not compiled with ALSA interface.\n"
	     "For more info about ALSA visit http://www.alsa-project.org\n");
}

#endif /* !HAVE_LIBASOUND */

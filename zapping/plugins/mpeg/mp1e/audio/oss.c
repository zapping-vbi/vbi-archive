/*
 *  MPEG-1 Real Time Encoder
 *  Open Sound System Interface
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/* $Id: oss.c,v 1.19 2001-07-31 12:59:50 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/soundcard.h>
#include "../common/log.h" 
#include "../common/mmx.h" 
#include "../common/math.h" 
#include "../common/alloc.h"
#include "audio.h"

#define IOCTL(fd, cmd, data) (TEMP_FAILURE_RETRY(ioctl(fd, cmd, data)))

#define TEST 0

/*
 *  OSS PCM Device
 */

#define BUFFER_SIZE 8192 // bytes per read(), appx.

struct oss_context {
	struct pcm_context	pcm;

	int			fd, fd2;
	int			scan_range;
	int			look_ahead;
	int			samples_per_frame;
	short *			p;
	int			left;
	double			time;
};

#define FLAT 0
#if FLAT

static void
wait_full(fifo2 *f)
{
	struct oss_context *oss = f->user_data;
	buffer2 *b = PARENT(f->buffers.head, buffer2, added);
	struct audio_buf_info info;
	unsigned char *p;
	ssize_t r, n;

	assert(b->data == NULL); /* no queue */

	for (p = b->allocated, n = b->size; n > 0;) {
		r = read(oss->fd, p, n);

		if (r < 0 && errno == EINTR)
			continue;

		if (r == 0) {
			memset(p, 0, n); // redundant except 2|4 multiple
			break;
		}

		ASSERT("read PCM data, %d bytes", r > 0, n);

		p += r;
		n -= r;
	}

	oss->time = current_time();

	ASSERT("SNDCTL_DSP_GETISPACE",
		IOCTL(oss->fd, SNDCTL_DSP_GETISPACE, &info) == 0);

	if (TEST)
		write(oss->fd2, b->allocated, b->size);

	oss->time -=
		((b->size - (n + info.bytes) / sizeof(short)) >> oss->pcm.stereo)
			/ (double) oss->pcm.sampling_rate;

	b->time = oss->time;
	b->data = b->allocated;

	send_full_buffer2(&oss->pcm.producer, b);
}

#else /* !FLAT */

/*
 *  Read window: samples_per_frame (1152 * channels) + look_ahead
 *  (480 * channels); from subband window size 512 samples, step
 *  width 32 samples (32 * 3 * 12 total)
 */
static void
wait_full(fifo2 *f)
{
	struct oss_context *oss = f->user_data;
	buffer2 *b = PARENT(f->buffers.head, buffer2, added);

	assert(b->data == NULL); /* no queue */

	if (oss->left <= 0) {
		struct audio_buf_info info;
		unsigned char *p;
		ssize_t r, n;
/*
		if (first) {
			p = b->allocated;
			n = (oss->scan_range + oss->look_ahead) * sizeof(short);
			first = FALSE;
		} else
*/
		{
			memcpy(b->allocated, (short *) b->allocated + oss->scan_range,
				oss->look_ahead * sizeof(short));

			p = b->allocated + oss->look_ahead * sizeof(short);
			n = oss->scan_range * sizeof(short);
		}

		while (n > 0) {
			r = read(oss->fd, p, n);

			if (r < 0 && errno == EINTR)
				continue;

			if (r == 0) {
				memset(p, 0, n);
				break;
			}

			ASSERT("read PCM data, %d bytes", r > 0, n);

			p += r;
			n -= r;
		}

		oss->time = current_time();

		ASSERT("SNDCTL_DSP_GETISPACE",
			IOCTL(oss->fd, SNDCTL_DSP_GETISPACE, &info) == 0);

		if (TEST)
			write(oss->fd2, b->allocated, oss->scan_range * sizeof(short));

		oss->time -=
			((oss->scan_range - (n + info.bytes) / sizeof(short)) >> oss->pcm.stereo)
				/ (double) oss->pcm.sampling_rate;

		oss->p = (short *) b->allocated;
		oss->left = oss->scan_range - oss->samples_per_frame;

		b->time = oss->time;
		b->data = b->allocated;

		send_full_buffer2(&oss->pcm.producer, b);
		return;
	}

	b->time = oss->time
		+ ((oss->p - (short *) b->allocated) >> oss->pcm.stereo)
			/ (double) oss->pcm.sampling_rate;

	oss->p += oss->samples_per_frame;
	oss->left -= oss->samples_per_frame;

	b->data = (unsigned char *) oss->p;

	send_full_buffer2(&oss->pcm.producer, b);
}

#endif

static void
send_empty(consumer *c, buffer2 *b)
{
	// XXX
	rem_node3(&c->fifo->full, &b->node);

	b->data = NULL;
}

fifo2 *
open_pcm_oss(char *dev_name, int sampling_rate, bool stereo)
{
	struct oss_context *oss;
	int oss_format = AFMT_S16_LE;
	int oss_speed = sampling_rate;
	int oss_stereo = stereo;
	int buffer_size;
	buffer2 *b;

	ASSERT("allocate pcm context",
		(oss = calloc(1, sizeof(struct oss_context))));

	oss->pcm.sampling_rate = sampling_rate;
	oss->pcm.stereo = stereo;

	oss->samples_per_frame = SAMPLES_PER_FRAME << stereo;
	oss->scan_range = MAX(BUFFER_SIZE / sizeof(short) / oss->samples_per_frame, 1) * oss->samples_per_frame;
	oss->look_ahead = (512 - 32) << stereo;

	buffer_size = (oss->scan_range + oss->look_ahead) * sizeof(short);

	if (TEST)
		ASSERT("open raw", (oss->fd2 = open("raw", O_WRONLY | O_CREAT)) != -1);

	ASSERT("open OSS PCM device %s",
		(oss->fd = open(dev_name, O_RDONLY)) != -1, dev_name);

	printv(2, "Opened OSS PCM device %s\n", dev_name);

	ASSERT("set OSS PCM AFMT_S16_LE",
		IOCTL(oss->fd, SNDCTL_DSP_SETFMT, &oss_format) == 0);

	ASSERT("set OSS PCM %d channels",
		IOCTL(oss->fd, SNDCTL_DSP_STEREO, &oss_stereo) == 0, oss_stereo + 1);

	ASSERT("set OSS PCM sampling rate %d Hz",
		IOCTL(oss->fd, SNDCTL_DSP_SPEED, &oss_speed) == 0, oss_speed);

	printv(3, "Set %s to signed 16 bit little endian, %d Hz, %s\n",
		dev_name, oss->pcm.sampling_rate, oss->pcm.stereo ? "stereo" : "mono");

	if (verbose > 2) {
		int frag_size;

		ASSERT("SNDCTL_DSP_GETBLKSIZE",
			IOCTL(oss->fd, SNDCTL_DSP_GETBLKSIZE, &frag_size) == 0);

		printv(3, "Dsp buffer size %i\n", frag_size);
	}

	ASSERT("init oss fifo",	init_callback_fifo2(
		&oss->pcm.fifo, "audio-oss",
		NULL, NULL, wait_full, send_empty,
		1, buffer_size));

	ASSERT("init oss producer",
		add_producer(&oss->pcm.fifo, &oss->pcm.producer));

	oss->pcm.fifo.user_data = oss;

	b = PARENT(oss->pcm.fifo.buffers.head, buffer2, added);

	b->data = NULL;

#if FLAT
	b->used = b->size;
	b->offset = 0;
#else
	b->used = (oss->samples_per_frame + oss->look_ahead) * sizeof(short);
	b->offset = oss->look_ahead * sizeof(short);
#endif

	return &oss->pcm.fifo;
}


/*
 *  OSS Mixer Device
 */

static const char *	sources[] = SOUND_DEVICE_NAMES;

extern char *		mix_dev;
extern int		mix_line;
extern int		mix_volume;

static int		old_recsrc;
static int		old_recvol;

static void
mix_restore(void)
{
	int fd;

	if ((fd = open(mix_dev, O_RDWR)) != -1) {
		IOCTL(fd, SOUND_MIXER_WRITE_RECSRC, &old_recsrc);
		IOCTL(fd, MIXER_WRITE(SOUND_MIXER_LINE), &old_recvol);
		close(fd);
	}
}

void
mix_init(void)
{
	int recsrc = 1 << mix_line;
	int recvol = (mix_volume << 8) | mix_volume;
	int fd;

	if ((fd = open(mix_dev, O_RDWR)) == -1) {
		printv(2, "Cannot open mixer %s (%d, %s) (ignored)\n", mix_dev, errno, strerror(errno));
		return;
	}

	ASSERT("get PCM rec source", IOCTL(fd, SOUND_MIXER_READ_RECSRC, &old_recsrc) == 0);
	ASSERT("get PCM rec volume", IOCTL(fd, MIXER_READ(SOUND_MIXER_LINE), &old_recvol) == 0);

	atexit(mix_restore);

	ASSERT("set PCM rec source %d:%s", IOCTL(fd, SOUND_MIXER_WRITE_RECSRC,
		&recsrc) == 0, mix_line, sources[mix_line]);
	ASSERT("set PCM rec volume %d%%", IOCTL(fd, MIXER_WRITE(SOUND_MIXER_LINE),
		&recvol) == 0, mix_volume);

	close(fd);

	printv(3, "%s initialized, source %d:%s, vol %d%%\n",
		mix_dev, mix_line, sources[mix_line], mix_volume);
}

// Enumerate available recording sources for usage()

char *
mix_sources(void)
{
	int i, recmask = 0;
	static char str[512];
	int fd;

	str[0] = 0;

	if ((fd = open(mix_dev, O_RDWR)) != -1) {
		IOCTL(fd, SOUND_MIXER_READ_RECMASK, &recmask);
		close(fd);
	}

	if (recmask) {
		strcat(str, " (");

		for (i = 0; i < 31; i++)
			if (recmask & (1 << i))
				sprintf(str + strlen(str), "%d:%s, ", i, sources[i]);

		strcpy(str + strlen(str) - 2, ")");
	}

	return str;
}

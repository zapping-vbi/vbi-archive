/*
 *  MPEG Real Time Encoder
 *  OSS Interface
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek,
 *  additions by Justin Schoeman
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

/* $Id: oss.c,v 1.7 2000-10-16 05:39:09 mschimek Exp $ */

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
#include "audio.h"
#include "mpeg.h"

#define TEST 0

/*
 *  PCM Device, OSS API
 */

#define BUFFER_SIZE 8192 // bytes per read(), appx.

static int		fd, fd2;
static short *		abuffer;
static int		scan_range;
static int		look_ahead;
static char		first;
static int		samples_per_frame;
static buffer		buf;
static fifo		pcm_fifo;

extern char *		pcm_dev;
extern int		sampling_rate;
extern int		stereo;

/*
 *  Read window: samples_per_frame (1152 * channels) + look_ahead (480 * channels);
 *  Subband window size 512 samples, step width 32 samples (32 * 3 * 12 total)
 */
/*
 *  If you have a better idea for timestamping go ahead.
 *  [DSP_CAP_TRIGGER, DSP_CAP_MMAP? Not really what I want but maybe
 *   closer to the sampling instant.]
 */

// XXX only one at a time, not checked
static buffer *
pcm_wait_full(fifo *f)
{
	static double rtime, utime;
	static int left = 0;
	static short *p;

	if (left <= 0)
	{
		struct audio_buf_info info;
		struct timeval tv;
		ssize_t r;
		int n;
/*
		if (first) {
			p = abuffer;
			n = (scan_range + look_ahead) * sizeof(abuffer[0]);
			first = 0;
		} else
*/
		{
			memcpy(abuffer, abuffer + scan_range, look_ahead * sizeof(abuffer[0]));

			p = abuffer + look_ahead;
			n = scan_range * sizeof(abuffer[0]);
		}

		while (n > 0) {
			r = read(fd, p, n);

			if (r < 0 && errno == EINTR)
				continue;

			if (r == 0) {
				memset(p, 0, n);
				break;
			}

			ASSERT("read PCM data, %d bytes", r > 0, n);

			(char *) p += r;
			n -= r;
		}

		gettimeofday(&tv, NULL);
		ASSERT("check PCM hw buffer maximum occupancy(tm)",
			ioctl(fd, SNDCTL_DSP_GETISPACE, &info) != 1);

		if (TEST)
			write(fd2, abuffer, scan_range * sizeof(abuffer[0]));

		rtime = tv.tv_sec + tv.tv_usec / 1e6;
		rtime -= (scan_range - n + info.bytes) / (double) sampling_rate;

		left = scan_range - samples_per_frame;
		p = abuffer;

		buf.time = rtime;
		buf.data = (unsigned char *) p;
		return &buf;
	}

	utime = rtime + ((p - abuffer) >> stereo) / (double) sampling_rate;
	left -= samples_per_frame;

	p += samples_per_frame;

	buf.time = utime;
	buf.data = (unsigned char *) p;
	return &buf;
}

static void
pcm_send_empty(fifo *f, buffer *b)
{
}

void
pcm_init(void)
{
	int format = AFMT_S16_LE;
	int speed = sampling_rate;
	int buffer_size;
	int frag_size;

	samples_per_frame = SAMPLES_PER_FRAME << stereo;
	scan_range = MAX(BUFFER_SIZE / sizeof(short) / samples_per_frame, 1) * samples_per_frame;
	look_ahead = (512 - 32) << stereo;
	first = 1;

	buffer_size = (scan_range + look_ahead)	* sizeof(abuffer[0]);

	ASSERT("allocate PCM buffer, %d bytes",
		(abuffer = calloc_aligned(buffer_size, CACHE_LINE)) != NULL, buffer_size);

	printv(3, "Allocated PCM buffer, %d bytes\n", buffer_size);

	if (TEST)
		ASSERT("open raw", (fd2 = open("raw", O_WRONLY | O_CREAT)) != -1);

	ASSERT("open PCM device %s", (fd = open(pcm_dev, O_RDONLY)) != -1, pcm_dev);

	printv(2, "Opened PCM device %s\n", pcm_dev);

	ASSERT("set PCM AFMT_S16_LE", ioctl(fd, SNDCTL_DSP_SETFMT, &format) != -1);
	ASSERT("set PCM %d channels", ioctl(fd, SNDCTL_DSP_STEREO, &stereo) != -1, stereo + 1);
	ASSERT("set PCM sampling rate %d Hz", ioctl(fd, SNDCTL_DSP_SPEED, &speed) != -1, sampling_rate);

	printv(3, "Set %s to signed 16 bit little endian, %d Hz, %s\n",
		pcm_dev, sampling_rate, stereo ? "stereo" : "mono");

	if (verbose > 2) {
		ASSERT("check buffering parameters", ioctl(fd, SNDCTL_DSP_GETBLKSIZE, &frag_size) != -1);
		printv(3, "Dsp buffer size %i\n", frag_size);
	}

	init_callback_fifo(audio_cap_fifo = &pcm_fifo,
		pcm_wait_full, pcm_send_empty,
		NULL, NULL, 0, 0);
}


/*
 *  Mixer Device
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
		ioctl(fd, SOUND_MIXER_WRITE_RECSRC, &old_recsrc);
		ioctl(fd, MIXER_WRITE(SOUND_MIXER_LINE), &old_recvol);
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

	ASSERT("get PCM rec source", ioctl(fd, SOUND_MIXER_READ_RECSRC, &old_recsrc) != -1);
	ASSERT("get PCM rec volume", ioctl(fd, MIXER_READ(SOUND_MIXER_LINE), &old_recvol) != -1);

	atexit(mix_restore);

	ASSERT("set PCM rec source %d:%s", ioctl(fd, SOUND_MIXER_WRITE_RECSRC, &recsrc) != -1, mix_line, sources[mix_line]);
	ASSERT("set PCM rec volume %d%%", ioctl(fd, MIXER_WRITE(SOUND_MIXER_LINE), &recvol) != -1, mix_volume);

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
		ioctl(fd, SOUND_MIXER_READ_RECMASK, &recmask);
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

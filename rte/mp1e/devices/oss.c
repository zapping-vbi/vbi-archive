/*
 *  MPEG-1 Real Time Encoder
 *  Open Sound System interface
 *
 *  Copyright (C) 1999, 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: oss.c,v 1.6 2002-12-14 00:43:44 mschimek Exp $ */

#include "../common/log.h"

#include "../audio/audio.h"

#ifdef HAVE_OSS

#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <sys/soundcard.h>

#include "../common/math.h" 

#define IOCTL(fd, cmd, data)						\
({ int __result; do __result = ioctl(fd, cmd, data);			\
   while (__result == -1L && errno == EINTR); __result; })

#define OSS_DROP_TEST(x) /* x */
#define OSS_TIME_LOG(x) /* x */

extern int test_mode;
/* 64 - capture raw audio as ./raw-audio */
/* 256 - clock drift code */

/*
 *  OSS PCM Device
 */

struct oss_context {
	struct pcm_context	pcm;

	int			fd, fd2;
	double			time;
	struct tfmem		tfmem;
};

static void
wait_full(fifo *f)
{
	struct oss_context *oss = f->user_data;
	buffer *b = PARENT(f->buffers.head, buffer, added);
	struct audio_buf_info info;
	struct timeval tv1, tv2;
	unsigned char *p;
	ssize_t r, n;
	double now;

	assert(b->data == NULL); /* no queue */

	for (;;) {
		for (p = b->allocated, n = b->size; n > 0;) {
			r = read(oss->fd, p, n);

			if (r < 0) {
				ASSERT("read PCM data, %d bytes", errno == EINTR, n);
				continue;
			}

			if (r == 0)
				FAIL("Got EOF from PCM device??");

			p += r;
			n -= r;
		}

		r = 5; /* retries */

		do {
			gettimeofday(&tv1, NULL);

			if (ioctl(oss->fd, SNDCTL_DSP_GETISPACE, &info) != 0) {
				ASSERT("SNDCTL_DSP_GETISPACE", errno != EINTR);
				continue;
			}

			gettimeofday(&tv2, NULL);

			tv2.tv_sec -= tv1.tv_sec;
			tv2.tv_usec -= tv1.tv_usec + (tv2.tv_sec ? 1000000 : 0);

			/*
			 *  When the dt is substantial we have been interrupted
			 *  and must assume info and tv1 are not in sync.
			 */
		} while ((tv2.tv_sec > 1 || tv2.tv_usec > 100) && r--);

		now = tv1.tv_sec + tv1.tv_usec * (1 / 1e6);

		OSS_DROP_TEST(if ((rand() % 100) < 98))
			break;

		printv(0, "audio drop\n");
	}

	/*
	 *  Subtract the number of samples we've read (b->size - n)
	 *  and the samples still available from the device when
	 *  sampling the system clock, to arrive at the sampling
	 *  instant of buffer[0].
	 */
	if ((n + info.bytes) == 0) /* usually */
		now -= oss->tfmem.ref; /* frag size in tod seconds */
	else
		now -= (b->size - n + info.bytes)
			* oss->tfmem.ref / (double) b->size;

	/*
	 *  SNDCTL_DSP_GETISPACE has only an accuracy of one fragment
	 *  (one irq per fragment, fragment equal buffer to read as
	 *  early as possible) plus unknown hw/driver buffering
	 *  plus unknown irq delay. That's far too inaccurate for
	 *  resampling and drop detection, hence the filter.
	 */
	if (oss->time > 0) {
		if (test_mode & 256) {
			double dt = now - oss->time;

			if (dt - oss->tfmem.err > oss->tfmem.ref * 1.98) {
				/* data lost, out of sync; XXX 1.98 bad */
				oss->time = now;

				printv(0, "audio dropped, dt=%f\n", dt);
			} else {
				oss->time += mp1e_timestamp_filter
					(&oss->tfmem, dt, 0.05, 1e-7, 0.08);
			}

			OSS_TIME_LOG(printv(0, "oss %f dt %+f err %+f t/b %+f\n",
					    now, dt, oss->tfmem.err, oss->tfmem.ref));
		} else {
			oss->time += oss->tfmem.ref;
		}
	} else
		oss->time = now;

	b->time = oss->time;
	b->data = b->allocated;

	if (test_mode & 64)
		ASSERT("write raw audio data",
		       write(oss->fd2, b->data, b->used) == b->used);

	send_full_buffer(&oss->pcm.producer, b);
}

static void
send_empty(consumer *c, buffer *b)
{
	// XXX
	unlink_node(&c->fifo->full, &b->node);

	b->data = NULL;
}

static const int format_preference[][2] = {
	{ AFMT_S16_LE, RTE_SNDFMT_S16_LE },
	{ AFMT_U16_LE, RTE_SNDFMT_U16_LE },
	{ AFMT_U8, RTE_SNDFMT_U8 },
	{ AFMT_S8, RTE_SNDFMT_S8 },
	{ -1, -1 }
};

void
open_pcm_oss(char *dev_name, int sampling_rate, rte_bool stereo, fifo **f)
{
	struct oss_context *oss;
	int oss_format = AFMT_S16_LE;
	int oss_speed = sampling_rate;
	int oss_stereo = stereo;
	int oss_frag_size;
	int dma_size = 128 << (10 + !!stereo); /* bytes */
	int i;
	buffer *b;

	ASSERT("allocate pcm context",
		(oss = calloc(1, sizeof(struct oss_context))));

	oss->pcm.format = RTE_SNDFMT_S16_LE;
	oss->pcm.sampling_rate = sampling_rate;
	oss->pcm.stereo = stereo;

	if (test_mode & 64)
		ASSERT("open raw audio file",
		       (oss->fd2 = open("raw-audio", O_WRONLY | O_CREAT, 0666)) != -1);

	ASSERT("open OSS PCM device '%s'",
		(oss->fd = open(dev_name, O_RDONLY)) != -1, dev_name);

	printv(2, "Opened OSS PCM device %s\n", dev_name);
#if 1
	oss_frag_size = 11 /* 2^11 = 2048 bytes */
		+ (sampling_rate > 24000)
		+ (!!stereo);

	oss_frag_size += (saturate(dma_size, 2 << oss_frag_size, 1 << 20)
			  / (1 << oss_frag_size)) << 16;

	while (IOCTL(oss->fd, SNDCTL_DSP_SETFRAGMENT, &oss_frag_size) != 0)
		if ((oss_frag_size -= 1 << 16) < (2 << 16))
			break;
#endif
	for (i = 0; (oss_format = format_preference[i][0]) >= 0; i++)
		if (IOCTL(oss->fd, SNDCTL_DSP_SETFMT, &oss_format) == 0)
			break;

	oss->pcm.format = format_preference[i][1];

	if (format_preference[i][0] < 0)
		FAIL("OSS driver did not accept sample format "
		     "S|U8 or S|U16LE\n");

	ASSERT("set OSS PCM %d channels",
		IOCTL(oss->fd, SNDCTL_DSP_STEREO, &oss_stereo) == 0, stereo + 1);

	if (test_mode & 32) {
		printv(0, "Deliberate sampling rate mismatch: "
			  "request 32 kHz, presumed to be %d Hz\n", sampling_rate);
		oss_speed = 32000;
		ASSERT("set OSS PCM sampling rate %d Hz",
	    		IOCTL(oss->fd, SNDCTL_DSP_SPEED, &oss_speed) == 0,
			oss_speed);
	} else {
		ASSERT("set OSS PCM sampling rate %d Hz",
		    	IOCTL(oss->fd, SNDCTL_DSP_SPEED, &oss_speed) == 0
			 && abs(oss_speed - sampling_rate) < (sampling_rate / 50),
			sampling_rate);
	}

	if (IOCTL(oss->fd, SNDCTL_DSP_GETBLKSIZE, &oss_frag_size) != 0)
		oss_frag_size = 4096; /* bytes */

	oss->time = 0.0;
	mp1e_timestamp_init(&oss->tfmem, oss_frag_size
		/ (double)(sampling_rate * sizeof(short) << stereo));

	printv(3, "Set %s to signed 16 bit little endian, %d Hz, %s, buffer size %d bytes\n",
		dev_name, oss->pcm.sampling_rate,
		oss->pcm.stereo ? "stereo" : "mono",
		oss_frag_size);

	*f = &oss->pcm.fifo;

	ASSERT("init oss fifo",	init_callback_fifo(*f, "audio-oss",
		NULL, NULL, wait_full, send_empty,
		1, oss_frag_size));

	ASSERT("init oss producer",
		add_producer(*f, &oss->pcm.producer));

	(*f)->user_data = oss;

	b = PARENT((*f)->buffers.head, buffer, added);

	b->data = NULL;
	b->used = b->size;
	b->offset = 0;
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
		IOCTL(fd, MIXER_WRITE(mix_line), &old_recvol);
		IOCTL(fd, SOUND_MIXER_WRITE_RECSRC, &old_recsrc);
		close(fd);
	}
}

void
mix_init(void)
{
	int recsrc = 1 << mix_line;
	int recvol;
	int fd;

	if (mix_line < 0 || mix_line >= SOUND_MIXER_NRDEVICES)
		FAIL("Mixer: invalid record source %d\n", mix_line);

	mix_volume = saturate(mix_volume, 0, 100);

	recvol = (mix_volume << 8) | mix_volume;

	if ((fd = open(mix_dev, O_RDWR)) == -1) {
		printv(1, "Cannot open mixer %s (%d, %s) (ignored)\n",
		       mix_dev, errno, strerror(errno));
		return;
	}

	ASSERT("get PCM rec source",
	       IOCTL(fd, SOUND_MIXER_READ_RECSRC, &old_recsrc) == 0);
	ASSERT("get PCM rec volume",
	       IOCTL(fd, MIXER_READ(mix_line), &old_recvol) == 0);

	atexit(mix_restore);

	ASSERT("set PCM rec source %d:%s",
	       IOCTL(fd, SOUND_MIXER_WRITE_RECSRC,
		     &recsrc) == 0, mix_line, sources[mix_line]);
	ASSERT("set PCM rec volume %d%%",
	       IOCTL(fd, MIXER_WRITE(mix_line),
		     &recvol) == 0, mix_volume);

	close(fd);

	printv(3, "%s initialized, source %d:%s, vol %d%%\n",
		mix_dev, mix_line, sources[mix_line], mix_volume);
}

/* Enumerate available recording sources for usage() */

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

#else /* !HAVE_OSS */

void
open_pcm_oss(char *dev_name, int sampling_rate, rte_bool stereo, fifo **f)
{
	FAIL("Not compiled with OSS interface.");
}

void
mix_init(void)
{
}

char *
mix_sources(void)
{
	return "none";
}

#endif /* !HAVE_OSS */

/*
 *  MPEG-1 Real Time Encoder
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

/* $Id: v4l.c,v 1.3 2000-09-23 03:57:54 mschimek Exp $ */

#include <ctype.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <asm/types.h>
#include "../videodev1.h"
#include "../common/log.h"
#include "../common/fifo.h"
#include "../common/math.h"
#include "../options.h"
#include "video.h"

void
v4l_init(void)
{
	FAIL("V4L interface is unmaintained.\n");
}

#if 0

static int			cap_fd;
static int			uindex = -1;
static unsigned char *		cap_buffer[256];

static struct video_capability	vcap;
static struct video_channel	chan;
static struct video_mbuf	buf;
static struct video_mmap	vmmap;
static struct video_audio	vaud;
static int			cframe;

static struct timeval		tv;

extern int			min_cap_buffers;

static void
capture_on(void)
{
	for (cframe = 0; cframe < cap_buffers; cframe++)
	{
		vmmap.frame = cframe;
		ASSERT("activate capturing", ioctl(cap_fd, VIDIOCMCAPTURE, &vmmap) == 0);
	}

	cframe = 0;
}

static unsigned char *
wait_frame(double *time, int *buf_index)
{
	static double utime;
	int r = -1;

	if (uindex >= 0) {
		*buf_index = uindex;
		uindex = -1;
		*time = utime;
	} else
	while (r <= 0)
	{
        	r = ioctl(cap_fd, VIDIOCSYNC, &cframe);

		gettimeofday(&tv, NULL);

		if (r < 0 && errno == EINTR)
                	continue;

		ASSERT("execute video sync", r >= 0);

		*time = utime = tv.tv_sec + tv.tv_usec / 1e6;
		*buf_index = cframe;

		break;
	}

	return cap_buffer[*buf_index];
}

static void
frame_done(int buf_index)
{
	vmmap.frame = buf_index;

	ASSERT("enqueue capture buffer", ioctl(cap_fd, VIDIOCMCAPTURE, &vmmap) == 0);

	cframe++;

	if (cframe == cap_buffers) cframe = 0;
}

static void
unget_frame(int buf_index)
{
	assert(uindex < 0);
	uindex = buf_index;
}

void
v4l_init(void)
{
	int aligned_width;
	int aligned_height;
	unsigned long buf_base;

	grab_width = width = saturate(width, 1, MAX_WIDTH);
	grab_height = height = saturate(height, 1, MAX_HEIGHT);

	aligned_width  = (width + 15) & -16;
	aligned_height = (height + 15) & -16;

	ASSERT("open capture device", (cap_fd = open(cap_dev, O_RDONLY)) != -1);
	ASSERT("query video capture capabilities", ioctl(cap_fd, VIDIOCGCAP, &vcap) == 0);

	if (!(vcap.type&VID_TYPE_CAPTURE))
		FAIL("%s ('%s') is not a capture device", cap_dev, vcap.name);

	ASSERT("query audio capabilities", ioctl(cap_fd, VIDIOCGAUDIO, &vaud) == 0);

	vaud.flags &= ~VIDEO_AUDIO_MUTE;
	vaud.volume = 60000;

	ASSERT("Audio unmuting", ioctl(cap_fd, VIDIOCSAUDIO, &vaud) == 0);

	ASSERT("unmute audio", ioctl(cap_fd, VIDIOCSAUDIO, &vaud) == 0);

	printv(2, "Opened %s ('%s')\n", cap_dev, vcap.name);

	ASSERT("query video channel", ioctl(cap_fd, VIDIOCGCHAN, &chan) == 0);

	if (chan.norm == 0) /* PAL */
		frame_rate_code = 3;
	else /* NTSC */
		frame_rate_code = 4;

	printv(2, "Video standard is '%s'\n", chan.norm == 0 ? "PAL" : "NTSC");

	if (frame_rate_code == 4 && grab_height == 288)
		height = aligned_height = grab_height = 240; // XXX DAU

	vmmap.width	= aligned_width;
	vmmap.height	= aligned_height;
	vmmap.format	= VIDEO_PALETTE_YUV420P;

	filter_mode = CM_YUV;
	filter_init(vmmap.width);

	ASSERT("request capture buffers", ioctl(cap_fd, VIDIOCGMBUF, &buf) == 0);

	if (buf.frames == 0)
		FAIL("No capture buffers granted");

	printv(2, "%d capture buffers granted\n", buf.frames);

	printv(3, "Mapping capture buffers.\n");

	buf_base=(unsigned long)mmap(NULL, buf.size, PROT_READ,
		  MAP_SHARED, cap_fd, 0);

	cap_buffer[0] = (unsigned char *)(buf_base + buf.offsets[0]);

	for (cap_buffers = 1; cap_buffers < buf.frames; cap_buffers++) {
		cap_buffer[cap_buffers] = (unsigned char *)(buf_base + buf.offsets[cap_buffers]);
		ASSERT("map capture buffer", (int) cap_buffer[cap_buffers] != -1);
	}

	video_start = capture_on;
	video_wait_frame = wait_frame;
	video_frame_done = frame_done;
	video_unget_frame = unget_frame;
}

#endif // 0

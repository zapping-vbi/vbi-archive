/*
 *  MPEG-1 Real Time Encoder
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

/* $Id: common.c,v 1.2 2000-07-05 18:09:34 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "../video/mpeg.h"
#include "../video/video.h"
#include "../audio/mpeg.h"
#include "../options.h"
#include "../fifo.h"
#include "../log.h"
#include "systems.h"

pthread_mutex_t		mux_mutex;
pthread_cond_t		mux_cond;

buffer			mux_buffer;

int			bytes_out;

double			system_idle = 0.0;

extern int		frame_rate_code;

double
get_idle(void)
{
	static double last_uptime = -1, last_idle;
	double uptime, idle, period;
	char buffer[80];
	int i, fd;

	if ((fd = open("/proc/uptime", O_RDONLY)) < 0)
		return system_idle;

	read(fd, buffer, sizeof buffer);

	close(fd);

	for (i = 0; i < sizeof(buffer) - 1; i++)
		if (buffer[i] == '\n')
			break;
	buffer[i] = 0;

	sscanf(buffer, "%lf %lf", &uptime, &idle);

	period = uptime - last_uptime;

	if (period > 0.5) {
		if (last_uptime >= 0.0)
			system_idle = 0.5 * (system_idle + (idle - last_idle) / period);

		last_idle = idle;
		last_uptime = uptime;
	}

	return system_idle;
}

/*
 *  Generic output routine (audio OR video)
 */
void
stream_output(fifo *fifo)
{
	int fps = lroundn(frame_rate_value[frame_rate_code]);
	int i;

	for (i = 0;; i++) {
		buffer *buf;

		pthread_mutex_lock(&mux_mutex);

		while (!(buf = (buffer *) rem_head(&fifo->full)))
			pthread_cond_wait(&mux_cond, &mux_mutex);

		pthread_mutex_unlock(&mux_mutex);

		if (!buf->size)
			break;

		buf = output(buf);

		if (verbose > 0) {
			static int upd_idle = 16;
			int min, sec, frame;

			if (--upd_idle <= 0) {
				get_idle();
				upd_idle = 16;
			}

			if (fifo == &vid) {
				frame = lroundn(i * frame_rate / frame_rate_value[frame_rate_code]);
				// exclude padding frames
				sec = frame / fps;
				frame -= sec * fps;
				min = sec / 60;
				sec -= min * 60;

				if (video_frames_dropped > 0)
					printv(1, "%d:%02d.%02d (%.1f MB), %.2f %% dropped, system load %.1f %%  %c",
						min, sec, frame, bytes_out / ((double)(1 << 20)),
						100.0 * video_frames_dropped / video_frame_count,
						100.0 * (1.0 - system_idle), (verbose > 3) ? '\n' : '\r');
				else
					printv(1, "%d:%02d.%02d (%.1f MB), system load %.1f %%    %c",
						min, sec, frame, bytes_out / ((double)(1 << 20)),
						100.0 * (1.0 - system_idle), (verbose > 3) ? '\n' : '\r');
			} else {
				sec = bytes_out / (audio_bit_rate >> 3);
				min = sec / 60;
				sec -= min * 60;

				printv(1, "%d:%02d (%.3f MB), system load %.1f %%    %c",
					min, sec, bytes_out / ((double)(1 << 20)),
					100.0 * (1.0 - system_idle), (verbose > 3) ? '\n' : '\r');
			}

			fflush(stderr);
		}

		empty_buffer(fifo, buf);
	}

	/* End of file (end code appended by producer) */

	printv(1, "\n%s: Done.\n", my_name);
}

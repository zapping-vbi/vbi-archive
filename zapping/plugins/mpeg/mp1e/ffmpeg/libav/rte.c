/*
 * rte io for ffmpeg.
 * Copyright (c) 2001 Iñaki García Etxebarria
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#define _GNU_SOURCE 1

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/time.h>
#include <assert.h>
#include "avformat.h"
#include <pthread.h>

#include "../../common/fifo.h"
#include "../../rtepriv.h"

static int rte_open(URLContext *h, const char *filename, int flags)
{
	return sscanf(filename, "rte:%p", &(h->priv_data))-1;
}

static int rte_read(URLContext *h, unsigned char *buf, int size)
{
	/* shouldn't be needed */
	assert(0);
}

static int rte_write(URLContext *h, unsigned char *buf, int size)
{
	rte_context *context = (rte_context*)h->priv_data;

	if (!context->private->encode_callback)
		return 0;

	context->private->bytes_out += size;

	context->private->encode_callback(context, buf, size,
					  context->private->user_data);

	return size;
}

static offset_t rte_seek(URLContext *h, offset_t pos, int whence)
{
	rte_context *context = (rte_context*)h->priv_data;

	if (!context->private->seek_callback)
		return (offset_t)-1;

	return context->private->seek_callback(context, pos, whence,
					       context->private->user_data);
}

static int rte_close(URLContext *h)
{
	return 1;
}

URLProtocol rte_protocol = {
    "rte",
    rte_open,
    rte_read,
    rte_write,
    rte_seek,
    rte_close,
};

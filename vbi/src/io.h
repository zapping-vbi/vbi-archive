/*
 *  libzvbi - Device interfaces
 *
 *  Copyright (C) 2002 Michael H. Schimek
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

/* $Id: io.h,v 1.6.2.6 2006-05-07 06:04:58 mschimek Exp $ */

#ifndef IO_H
#define IO_H

#include "raw_decoder.h"
#include "conv.h"

/* Public */

#include <sys/time.h> /* struct timeval */

/**
 * @ingroup Device
 */
typedef struct vbi3_capture_buffer {
	void *			data;
	int			size;
	double			timestamp;
} vbi3_capture_buffer;

/**
 * @ingroup Device
 * @brief Opaque device interface handle.
 **/
typedef struct vbi3_capture vbi3_capture;

/**
 * @addtogroup Device
 * @{
 */
extern vbi3_capture *	vbi3_capture_v4l2_new(const char *dev_name, int buffers,
					     unsigned int *services, int strict,
					     char **errorstr, vbi3_bool trace);
extern vbi3_capture *	vbi3_capture_v4l2k_new(const char *	dev_name,
					      int		fd,
					      int		buffers,
					      unsigned int *	services,
					      int		strict,
					      char **		errorstr,
					      vbi3_bool		trace);
extern vbi3_capture *	vbi3_capture_v4l_new(const char *dev_name, int scanning,
					    unsigned int *services, int strict,
					    char **errorstr, vbi3_bool trace);
extern vbi3_capture *	vbi3_capture_v4l_sidecar_new(const char *dev_name, int given_fd,
						    unsigned int *services,
						    int strict, char **errorstr, 
						    vbi3_bool trace);
extern vbi3_capture *	vbi3_capture_bktr_new (const char *	dev_name,
					      int		scanning,
					      unsigned int *	services,
					      int		strict,
					      char **		errstr,
					      vbi3_bool		trace);

extern int		vbi3_capture_read_raw(vbi3_capture *capture, void *data,
					     double *timestamp, struct timeval *timeout);
extern int		vbi3_capture_read_sliced(vbi3_capture *capture, vbi3_sliced *data, int *lines,
						double *timestamp, struct timeval *timeout);
extern int		vbi3_capture_read(vbi3_capture *capture, void *raw_data,
					 vbi3_sliced *sliced_data, int *lines,
					 double *timestamp, struct timeval *timeout);
extern int		vbi3_capture_pull_raw(vbi3_capture *capture, vbi3_capture_buffer **buffer,
					     struct timeval *timeout);
extern int		vbi3_capture_pull_sliced(vbi3_capture *capture, vbi3_capture_buffer **buffer,
						struct timeval *timeout);
extern int		vbi3_capture_pull(vbi3_capture *capture, vbi3_capture_buffer **raw_buffer,
					 vbi3_capture_buffer **sliced_buffer, struct timeval *timeout);
extern vbi3_raw_decoder *vbi3_capture_parameters(vbi3_capture *capture);
extern int		vbi3_capture_fd(vbi3_capture *capture);

extern void		vbi3_capture_delete(vbi3_capture *capture);
/** @} */

/* Private */

#endif /* IO_H */

#if 0

typedef struct {
	void *			data;
	unsigned int		size;

	struct timeval		capture_time;
	int64_t			stream_time;
} vbi3_buffer;

typedef struct {
	void *			data;
	unsigned int		size;

	struct timeval		capture_time;
	int64_t			stream_time;

	const sampling_par *	sampling_par;
} vbi3_raw_buffer;

typedef struct {
	vbi3_sliced *		data,
	unsigned int		size,

	struct timeval		capture_time;
	int64_t			stream_time;

	unsigned int		lines;
} vbi3_sliced_buffer;

typedef struct vbi3_capture vbi3_capture;

vbi3_capture_delete
vbi3_capture_new
get error string
open v4l
find v4l video standard
open v4l2
open bktr
open proxy
read write pull push * raw sliced both
get fd
get sampling parameters

#endif

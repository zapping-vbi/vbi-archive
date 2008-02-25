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

/* $Id: io.h,v 1.6.2.8 2008-02-25 20:12:06 mschimek Exp $ */

#ifndef __ZVBI3_IO_H__
#define __ZVBI3_IO_H__

#include "macros.h"
#include "sliced.h"
#include "sampling_par.h"
#include "bit_slicer.h"
#include "version.h"

VBI3_BEGIN_DECLS

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
 * @ingroup Device
 * @brief Properties of capture file handle
 */
typedef enum {
       /**
        * Is set when select(2) can be used to wait for
        * new data on the capture device file handle.
        */
        VBI3_FD_HAS_SELECT  = 1<<0,
       /**
        * Is set when the capture device supports
        * "user-space DMA".  In this case it's more efficient
        * to use one of the "pull" functions to read raw data
        * because otherwise the data has to be copied once
        * more into the passed buffer.
        */
        VBI3_FD_HAS_MMAP    = 1<<1,
       /**
        * Is not set when the capture device file handle is
        * not the actual device.  In this case it can only be
        * used for select(2) and not for ioctl(2)
        */
        VBI3_FD_IS_DEVICE   = 1<<2
} VBI3_CAPTURE_FD_FLAGS;

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
extern int		vbi3_capture_dvb_filter(vbi3_capture *cap, int pid);
extern vbi3_capture*	vbi3_capture_dvb_new(char *dev, int scanning,
				        unsigned int *services, int strict,
				        char **errstr, vbi3_bool trace);
extern int64_t
vbi3_capture_dvb_last_pts	(const vbi3_capture *	cap);
extern vbi3_capture *
vbi3_capture_dvb_new2		(const char *		device_name,
				 unsigned int		pid,
				 char **		errstr,
				 vbi3_bool		trace);

struct vbi3_proxy_client;
 
extern vbi3_capture *
vbi3_capture_proxy_new( struct vbi3_proxy_client * vpc,
                        int buffers, int scanning,
                        unsigned int *p_services, int strict,
                        char **pp_errorstr );

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
extern vbi3_bool
vbi3_capture_sampling_point	(vbi3_capture *		cap,
				 vbi3_bit_slicer_point *point,
				 unsigned int		row,
				 unsigned int		nth_bit);
extern vbi3_bool
vbi3_capture_debug		(vbi3_capture *		cap,
				 vbi3_bool		enable);

#if 3 == VBI_VERSION_MINOR
extern const vbi3_sampling_par *
vbi3_capture_parameters		(vbi3_capture *		capture);
#else
extern vbi_raw_decoder *
vbi3_capture_parameters		(vbi_capture *		capture);
#endif

extern int		vbi3_capture_fd(vbi3_capture *capture);
extern unsigned int     vbi3_capture_update_services(vbi3_capture *capture,
                                                    vbi3_bool reset, vbi3_bool commit,
                                                    unsigned int services, int strict,
                                                    char ** errorstr);
extern int              vbi3_capture_get_scanning(vbi3_capture *capture);
extern void             vbi3_capture_flush(vbi3_capture *capture);
extern void		vbi3_capture_delete(vbi3_capture *capture);

extern vbi3_bool         vbi3_capture_set_video_path(vbi3_capture *capture, const char * p_dev_video);
extern VBI3_CAPTURE_FD_FLAGS vbi3_capture_get_fd_flags(vbi3_capture *capture);
extern void
vbi3_capture_set_log_fn		(vbi3_capture *		cap,
				 vbi3_log_mask		mask,
				 vbi3_log_fn *		log_fn,
				 void *			user_data);
/** @} */

/* Private */

#if 0 /* -> io-priv.h */

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>		/* open() */

extern const char _zvbi3_intl_domainname[];

#include "version.h"
#include "intl-priv.h"

#if defined (_IOC_SIZE) /* Linux */

#define IOCTL_ARG_SIZE(cmd)	_IOC_SIZE (cmd)
#define IOCTL_READ(cmd)		(_IOC_DIR (cmd) & _IOC_READ)
#define IOCTL_WRITE(cmd)	(_IOC_DIR (cmd) & _IOC_WRITE)
#define IOCTL_READ_WRITE(cmd)	(_IOC_DIR (cmd) == (_IOC_READ | _IOC_WRITE))
#define IOCTL_NUMBER(cmd)	_IOC_NR (cmd)

#elif defined (IOCPARM_LEN) /* FreeBSD */

#define IOCTL_ARG_SIZE(cmd)	IOCPARM_LEN (cmd)
#define IOCTL_READ(cmd)		((cmd) & IOC_OUT)
#define IOCTL_WRITE(cmd)	((cmd) & IOC_IN)
#define IOCTL_READ_WRITE(cmd)	(((cmd) & IOC_DIRMASK) == (IOC_IN | IOC_OUT))
#define IOCTL_NUMBER(cmd)	((cmd) & 0xFF)

#else /* Don't worry, only used for debugging */

#define IOCTL_ARG_SIZE(cmd)	0
#define IOCTL_READ(cmd)		0
#define IOCTL_WRITE(cmd)	0
#define IOCTL_READ_WRITE(cmd)	0
#define IOCTL_NUMBER(cmd)	0

#endif

typedef void
ioctl_log_fn			(FILE *			fp,
				 unsigned int		cmd,
				 int			rw,
				 void *			arg);
extern void
fprint_symbolic			(FILE *			fp,
				 int			mode,
				 unsigned long		value,
				 ...);
extern void
fprint_unknown_ioctl		(FILE *			fp,
				 unsigned int		cmd,
				 void *			arg);
extern int
device_open			(FILE *			fp,
				 const char *		pathname,
				 int			flags,
				 mode_t			mode);
extern int
device_close			(FILE *			fp,
				 int			fd);
extern int
device_ioctl			(FILE *			fp,
				 ioctl_log_fn *		fn,
				 int			fd,
				 unsigned int		cmd,
				 void *			arg);

extern void
vbi3_capture_set_log_fp		(vbi3_capture *		capture,
				 FILE *			fp);

/* Preliminary hack for tests. */
extern vbi3_bool vbi3_capture_force_read_mode;



/**
 * @ingroup Devmod
 */
struct vbi3_capture {
        int		        (* read)(vbi3_capture *,
					 vbi3_capture_buffer **,
					 vbi3_capture_buffer **,
					 const struct timeval *);
	vbi3_raw_decoder *	(* parameters)(vbi3_capture *);
        unsigned int            (* update_services)(vbi3_capture *,
                                         vbi3_bool, vbi3_bool,
                                         unsigned int, int, char **);
        int                     (* get_scanning)(vbi3_capture *);
	void			(* flush)(vbi3_capture *);
	int			(* get_fd)(vbi3_capture *);
	VBI3_CAPTURE_FD_FLAGS	(* get_fd_flags)(vbi3_capture *);
	vbi3_bool 		(* set_video_path)(vbi3_capture *, const char *);
	void			(* _delete)(vbi3_capture *);

	/* Log all system calls if non-NULL. */
	FILE *			sys_log_fp;
};

#endif /* 0 */

VBI3_END_DECLS

#endif /* __ZVBI3_IO_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/

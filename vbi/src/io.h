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

/* $Id: io.h,v 1.10.2.1 2004-01-27 21:07:39 tomzo Exp $ */

#ifndef IO_H
#define IO_H

#include "decoder.h"

/* Public */

#include <sys/time.h> /* struct timeval */

/**
 * @ingroup Device
 */
typedef struct vbi_capture_buffer {
	void *			data;
	int			size;
	double			timestamp;
} vbi_capture_buffer;

/**
 * @ingroup Device
 * @brief Opaque device interface handle.
 **/
typedef struct vbi_capture vbi_capture;

/**
 * @ingroup Device
 * @brief TV input channel description for channel changes.
 *
 * This structure is passed to the channel switch function. Any of
 * the elements in the union can be set to -1 if they shall remain
 * unchanged.
 *
 * The mode elements describe the color encoding or video norm; only
 * one of them can be used. mode_std is a v4l2_std_id descriptor, see
 * videodev2.h (2002 revision). mode_color is a v4l1 mode descriptor,
 * i.e. 0=PAL, 1=NTSC, 2=SECAM, 3=AUTO. modes are converted as required
 * for v4l1 or v4l2 devices.
 */
typedef enum {
	VBI_CHN_DESC_TYPE_NULL,
	VBI_CHN_DESC_TYPE_ANALOG,
	VBI_CHN_DESC_TYPE_COUNT
} VBI_CHN_DESC_TYPE;

typedef struct {
	VBI_CHN_DESC_TYPE		type;
	union {
		struct {
			int 		channel;
			int 		freq;
			int 		tuner;
			int 		mode_color;
			long long 	mode_std;
		} analog;
		struct {
			char		reserved[64];
		} x;
	} u;
} vbi_channel_desc;

/**
 * @ingroup Device
 * @brief Flags for channel switching (bitfield, can be combined with OR)
 */
enum {
	VBI_CHN_PRIO_ONLY  = 1,
	VBI_CHN_FLUSH_ONLY = 2
};

/**
 * @ingroup Device
 * @brief Priority levels for channel switching (equivalent to enum v4l2_priority)
 */
enum {
	VBI_CHN_PRIO_BACKGROUND  = 1,
	VBI_CHN_PRIO_INTERACTIVE = 2,
	VBI_CHN_PRIO_RECORD      = 3
};

/**
 * @ingroup Device
 * @brief Sub-priority levels for background channel switching (proxy only)
 *
 * This enum describes recommended sub-priority levels for channel profiles.
 * They're intended for channel switching through a VBI proxy at background
 * priority level.  The daemon uses this priority to decide which request
 * to grant first if there are multiple outstanding requests.  To the daemon
 * these are just numbers (highest wins) but for successful cooperation
 * clients need to use agree on values for similar tasks. Hence the following
 * values are recommended:
 *
 * INITIAL: Initial reading of data after program start (and long pause since
 * last start); once all data is read the client should lower it's priority.
 * UPDATE: A change in the data transmission has been detected or a long
 * time has passed since the initial reading, so data needs to be read newly.
 * CHECK: After INITIAL or CHECK is completed clients can use this level to
 * ontinuously check for change marks.  PASSIVE: client does not request a
 * specific channel.
 */
enum {
	VBI_CHN_SUBPRIO_PASSIVE  = 0x00,
	VBI_CHN_SUBPRIO_CHECK    = 0x10,
	VBI_CHN_SUBPRIO_UPDATE   = 0x20,
	VBI_CHN_SUBPRIO_INITIAL  = 0x30,
	VBI_CHN_SUBPRIO_VPS_PDC  = 0x40
};

/**
 * @ingroup Device
 * @brief Proxy scheduler configuration for background channel switching
 */
typedef struct
{
	uint8_t			is_valid;       /* boolean: ignore struct unless TRUE */
	uint8_t			sub_prio;       /* background prio: VPS/PDC, initial load, update, check, passive */
	uint8_t			allow_suspend;  /* boolean: interruption allowed */
	uint8_t			reserved0;
	time_t			min_duration;   /* min. continuous use of that channel */
	time_t			exp_duration;   /* expected duration of use of that channel */
	uint8_t			reserved1[16];
} vbi_channel_profile;

/**
 * @ingroup Device
 * @brief Control parameters for VBI capture.
 *
 * This union is used to pass advanced configuration parameters
 * to the capture IO modules.
 */
typedef enum {
	VBI_SETUP_GET_FD_TYPE,
	VBI_SETUP_ENABLE_CHN_IND,
	VBI_SETUP_GET_CHN_DESC,
	VBI_SETUP_V4L_VIDEO_PATH,
	VBI_SETUP_PROXY_ANY_DEVICE,
	VBI_SETUP_PROXY_CHN_PROFILE,
	VBI_SETUP_PROXY_GET_API,
	VBI_SETUP_TYPE_COUNT
} VBI_SETUP_TYPE;

typedef struct {
	VBI_SETUP_TYPE			type;
	union {
		struct {
			vbi_bool	has_select;
			vbi_bool	is_device;
		} get_fd_type;
		struct {
			vbi_bool	do_report;
			vbi_bool	* p_report_flag;
		} enable_chn_ind;
		struct {
			unsigned int	scanning;
			vbi_bool	chn_granted;
			vbi_channel_desc chn_desc;
		} get_chn_desc;
		struct {
			vbi_bool	use_any_device;
		} proxy_any_device;
		struct {
			vbi_channel_profile chn_profile;
		} proxy_chn_profile;
		struct {
			int             api_rev;
		} proxy_get_api;
		struct {
			char    	* p_dev_path;
		} v4l_video_path;
	} u;
} vbi_setup_parm;

/**
 * @addtogroup Device
 * @{
 */
extern vbi_capture *	vbi_capture_v4l2_new(const char *dev_name, int buffers,
					     unsigned int *services, int strict,
					     char **errorstr, vbi_bool trace);
extern vbi_capture *	vbi_capture_v4l2k_new(const char *	dev_name,
					      int		fd,
					      int		buffers,
					      unsigned int *	services,
					      int		strict,
					      char **		errorstr,
					      vbi_bool		trace);
extern vbi_capture *	vbi_capture_v4l_new(const char *dev_name, int scanning,
					    unsigned int *services, int strict,
					    char **errorstr, vbi_bool trace);
extern vbi_capture *	vbi_capture_v4l_sidecar_new(const char *dev_name, int given_fd,
						    unsigned int *services,
						    int strict, char **errorstr, 
						    vbi_bool trace);
extern vbi_capture *	vbi_capture_bktr_new (const char *	dev_name,
					      int		scanning,
					      unsigned int *	services,
					      int		strict,
					      char **		errstr,
					      vbi_bool		trace);

extern vbi_capture *    vbi_capture_proxy_new(const char *dev_name,
                                              int buffers,
                                              int scanning,
                                              unsigned int *services,
                                              int strict,
                                              char **pp_errorstr,
                                              vbi_bool trace);

extern int		vbi_capture_read_raw(vbi_capture *capture, void *data,
					     double *timestamp, struct timeval *timeout);
extern int		vbi_capture_read_sliced(vbi_capture *capture, vbi_sliced *data, int *lines,
						double *timestamp, struct timeval *timeout);
extern int		vbi_capture_read(vbi_capture *capture, void *raw_data,
					 vbi_sliced *sliced_data, int *lines,
					 double *timestamp, struct timeval *timeout);
extern int		vbi_capture_pull_raw(vbi_capture *capture, vbi_capture_buffer **buffer,
					     struct timeval *timeout);
extern int		vbi_capture_pull_sliced(vbi_capture *capture, vbi_capture_buffer **buffer,
						struct timeval *timeout);
extern int		vbi_capture_pull(vbi_capture *capture, vbi_capture_buffer **raw_buffer,
					 vbi_capture_buffer **sliced_buffer, struct timeval *timeout);
extern vbi_raw_decoder *vbi_capture_parameters(vbi_capture *capture);
extern int		vbi_capture_fd(vbi_capture *capture);
extern unsigned int     vbi_capture_add_services(vbi_capture *capture,
                                                 vbi_bool reset, vbi_bool commit,
                                                 unsigned int services, int strict,
                                                 char ** errorstr);
extern int		vbi_capture_channel_change(vbi_capture *capture,
						   int chn_flags, int chn_prio,
						   vbi_channel_desc * p_chn_desc,
						   vbi_bool * p_has_tuner, int * p_scanning,
						   char ** errorstr);
extern vbi_bool		vbi_capture_setup(vbi_capture *capture, vbi_setup_parm *config);
extern void		vbi_capture_delete(vbi_capture *capture);
/** @} */

/* Private */

#ifndef DOXYGEN_SHOULD_IGNORE_THIS

#include <stdarg.h>
#include <stddef.h>

extern const char _zvbi_intl_domainname[];

#ifndef _
#  ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) dgettext (_zvbi_intl_domainname, String)
#    ifdef gettext_noop
#      define N_(String) gettext_noop (String)
#    else
#      define N_(String) (String)
#    endif
#  else /* Stubs that do something close enough.  */
#    define gettext(Msgid) ((const char *) (Msgid))
#    define dgettext(Domainname, Msgid) ((const char *) (Msgid))
#    define dcgettext(Domainname, Msgid, Category) ((const char *) (Msgid))
#    define ngettext(Msgid1, Msgid2, N) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define dngettext(Domainname, Msgid1, Msgid2, N) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define dcngettext(Domainname, Msgid1, Msgid2, N, Category) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define textdomain(Domainname) ((const char *) (Domainname))
#    define bindtextdomain(Domainname, Dirname) ((const char *) (Dirname))
#    define bind_textdomain_codeset(Domainname, Codeset) ((const char *) (Codeset))
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#endif

#endif /* !DOXYGEN_SHOULD_IGNORE_THIS */

/**
 * @ingroup Devmod
 */
struct vbi_capture {
	vbi_bool		(* read)(vbi_capture *, vbi_capture_buffer **,
					 vbi_capture_buffer **, struct timeval *);
	vbi_raw_decoder *	(* parameters)(vbi_capture *);
        unsigned int            (* add_services)(vbi_capture *vc,
                                         vbi_bool reset, vbi_bool commit,
                                         unsigned int services, int strict,
                                         char ** errorstr);
	int			(* channel_change)(vbi_capture *vc,
					 int chn_flags, int chn_prio,
					 vbi_channel_desc * p_chn_desc,
					 vbi_bool * p_has_tuner, int * p_scanning,
					 char ** errorstr);
	int			(* get_fd)(vbi_capture *);
	int			(* setup)(vbi_capture *, vbi_setup_parm *);
	void			(* _delete)(vbi_capture *);
};

/* Private */
extern void 		vbi_capture_io_update_timeout( struct timeval * tv_start,
						       struct timeval * timeout );
extern int		vbi_capture_io_select( int fd, struct timeval * timeout );

#endif /* IO_H */

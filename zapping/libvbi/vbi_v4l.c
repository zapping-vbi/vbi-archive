#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include "os.h"
#include "vt.h"
#include "misc.h"
#include "vbi.h"
#include "fdset.h"
#include "hamm.h"
#include "lang.h"

static u8 *rawbuf;		// one common buffer for raw vbi data.
static int rawbuf_size;		// its current size


/***** bttv api *****/

#define BASE_VIDIOCPRIVATE	192
#define BTTV_VERSION		_IOR('v' , BASE_VIDIOCPRIVATE+6, int)
#define BTTV_VBISIZE		_IOR('v' , BASE_VIDIOCPRIVATE+8, int)

/***** v4l2 vbi-api *****/

struct v4l2_vbi_format
{
    u32 sampling_rate;		/* in 1 Hz */
    u32 offset;			/* sampling starts # samples after rising hs */
    u32 samples_per_line;
    u32 sample_format;		/* V4L2_VBI_SF_* */
    s32 start[2];
    u32 count[2];
    u32 flags;			/* V4L2_VBI_* */
    u32 reserved2;		/* must be zero */
};

struct v4l2_format
{
    u32	type;			/* V4L2_BUF_TYPE_* */
    union
    {
	struct v4l2_vbi_format vbi;	/*  VBI data  */
	u8 raw_data[200];		/* user-defined */
    } fmt;
};

#define V4L2_VBI_SF_UBYTE	1
#define V4L2_BUF_TYPE_VBI       0x00000009
#define VIDIOC_G_FMT		_IOWR('V',  4, struct v4l2_format)

/***** end of api definitions *****/

// called when new vbi data is waiting

static void
vbi_handler(struct vbi *vbi, int fd)
{
    int n, i;
    u32 seq;

    n = read(vbi->fd, rawbuf, vbi->bufsize);

    if (dl_empty(vbi->clients))
	return;

    if (n != vbi->bufsize)
	return;

    seq = *(u32 *)&rawbuf[n - 4];
    if (vbi->seq+1 != seq)
    {
	out_of_sync(vbi);
	if (seq < 3 && vbi->seq >= 3)
	    vbi_reset(vbi);
    }
    vbi->seq = seq;

    if (seq > 1)	// the first may contain data from prev channel
	for (i = 0; i+vbi->bpl <= n; i += vbi->bpl)
	    vbi_line(vbi, rawbuf + i);
}

static int
set_decode_parms(struct vbi *vbi, struct v4l2_vbi_format *p)
{
    double fs;		// sampling rate
    double bpb;		// bytes per bit
    int soc, eoc;	// start/end of clock run-in
    int bpl;		// bytes per line

    if (p->sample_format != V4L2_VBI_SF_UBYTE)
    {
	error("v4l2: unsupported vbi data format");
	return -1;
    }

    // some constants from the standard:
    //   horizontal frequency			fh = 15625Hz
    //   teletext bitrate			ft = 444*fh = 6937500Hz
    //   teletext identification sequence	10101010 10101010 11100100
    //   13th bit of seq rel to falling hsync	12us -1us +0.4us
    // I search for the clock run-in (10101010 10101010) from 12us-1us-12.5/ft
    // (earliest first bit) to 12us+0.4us+3.5/ft (latest last bit)
    //   earlist first bit			tf = 12us-1us-12.5/ft = 9.2us
    //   latest last bit			tl = 12us+0.4us+3.5/ft = 12.9us
    //   total number of used bits		n = (2+1+2+40)*8 = 360

    bpl = p->samples_per_line;
    fs = p->sampling_rate;
    bpb = fs/6937500.0;
    soc = (int)(9.2e-6*fs) - (int)p->offset;
    eoc = (int)(12.9e-6*fs) - (int)p->offset;
    if (soc < 0)
	soc = 0;
    if (eoc > bpl - (int)(43*8*bpb))
	eoc = bpl - (int)(43*8*bpb);
    if (eoc - soc < (int)(16*bpb))
    {
	// line too short or offset too large or wrong sample_rate
	error("v4l2: broken vbi format specification");
	return -1;
    }
    if (eoc > 240)
    {
	// the vbi_line routine can hold max 240 values in its work buffer
	error("v4l2: unable to handle these sampling parameters");
	return -1;
    }

    vbi->bpb = bpb * FAC + 0.5;
    vbi->soc = soc;
    vbi->eoc = eoc;
    vbi->bp8bl = 0.97 * 8*bpb; // -3% tolerance
    vbi->bp8bh = 1.03 * 8*bpb; // +3% tolerance

    vbi->bpl = bpl;
    vbi->bufsize = bpl * (p->count[0] + p->count[1]);

    return 0;
}

int
v4l_vbi_setup_dev(struct vbi *vbi)
{
    struct v4l2_format v4l2_format[1];
    struct v4l2_vbi_format *vbifmt = &v4l2_format->fmt.vbi;

    if (ioctl(vbi->fd, VIDIOC_G_FMT, v4l2_format) == -1
	|| v4l2_format->type != V4L2_BUF_TYPE_VBI)
    {
	// not a v4l2 device.  assume bttv and create a standard fmt-struct.
	int size;

	vbifmt->sample_format = V4L2_VBI_SF_UBYTE;
	vbifmt->sampling_rate = 35468950;
	vbifmt->samples_per_line = 2048;
	vbifmt->offset = 244;
	if ((size = ioctl(vbi->fd, BTTV_VBISIZE, 0)) == -1)
	{
	    // BSD or older bttv driver.
	    vbifmt->count[0] = 16;
	    vbifmt->count[1] = 16;
	}
	else if (size % 2048)
	{
	    error("broken bttv driver (bad buffer size)");
	    return -1;
	}
	else
	{
	    size /= 2048;
	    vbifmt->count[0] = size/2;
	    vbifmt->count[1] = size - size/2;
	}
    }

    if (set_decode_parms(vbi, vbifmt) == -1)
	return -1;

    if (vbi->bpl < 1 || vbi->bufsize < vbi->bpl || vbi->bufsize % vbi->bpl != 0)
    {
	error("strange size of vbi buffer (%d/%d)", vbi->bufsize, vbi->bpl);
	return -1;
    }

    // grow buffer if necessary
    if (rawbuf_size < vbi->bufsize)
    {
	if (rawbuf)
	    free(rawbuf);
	if (not(rawbuf = malloc(rawbuf_size = vbi->bufsize)))
	    out_of_mem(rawbuf_size); // old buffer already freed.  abort.
    }

    fdset_add_fd(fds, vbi->fd, vbi_handler, vbi);

    return 0;
}

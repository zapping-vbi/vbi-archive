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
#include <linux/types.h>
#include "../src/videodev2.h"
#include <errno.h>
#include <sys/mman.h>

/* OBSOLETE */

static u8 *rawbuf;		// one common buffer for raw vbi data.
static int rawbuf_size;		// its current size

// called when new vbi data is waiting
static void
vbi_handler(struct vbi *vbi, int fd)
{
    int n, i;
    u32 seq;
    struct v4l2_buffer qbuf;
    fd_set fds;
    
    n = -1;
    while(n<=0)
    {
        FD_ZERO(&fds);
        FD_SET(vbi->fd, &fds);
        n = select(vbi->fd + 1, &fds, NULL, NULL, NULL);
        if(n < 0 && errno == EINTR) continue;
        if(n<0) perror("select");
    }
    qbuf.type = V4L2_BUF_TYPE_VBI;
    if(ioctl(vbi->fd, VIDIOC_DQBUF, &qbuf)<0)
    {
    	perror("dqbuf");
    	return;
    }
    rawbuf = vbi->bufs[qbuf.index];

    if (dl_empty(vbi->clients))
        goto done;

    seq = qbuf.sequence;
    if (vbi->seq+1 != seq)
    {
	out_of_sync(vbi);
	if (seq < 3 && vbi->seq >= 3)
	    vbi_reset(vbi);
    }
    vbi->seq = seq;

    if (seq > 0)	// the first may contain data from prev channel
	for (i = 0; i < vbi->bufsize; i += vbi->bpl)
	  vbi_line(vbi, rawbuf + i);

done:
    if(ioctl(vbi->fd, VIDIOC_QBUF, &qbuf)<0)
    	perror("qbuf");
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
//	return -1;
    }
    if (eoc > 240)
    {
	// the vbi_line routine can hold max 240 values in its work buffer
	error("v4l2: unable to handle these sampling parameters");
//	return -1;
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
v4l2_vbi_setup_dev(struct vbi *vbi)
{
    struct v4l2_format v4l2_format[1];
    struct v4l2_vbi_format *vbifmt = &v4l2_format->fmt.vbi;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer qbuf;
    struct v4l2_capability vcap;
    int i;

    if (ioctl(vbi->fd, VIDIOC_QUERYCAP, &vcap) != 0)
      return -1;

    if (vcap.type != V4L2_TYPE_VBI)
      return -1;

    /* fixme: implement read() interface too */
    if (!(vcap.flags & V4L2_FLAG_STREAMING))
      return -1;

    v4l2_format->type = V4L2_BUF_TYPE_VBI;
    vbifmt->start[0]=6;
    vbifmt->start[1]=318;
    vbifmt->count[0]=16;
    vbifmt->count[1]=16;
    vbifmt->sample_format = V4L2_VBI_SF_UBYTE;
    vbifmt->sampling_rate = 27000000;
    if (ioctl(vbi->fd, VIDIOC_S_FMT, v4l2_format)<0)
      {
	perror("sfmt");
	return -1;
      }

    req.type = V4L2_BUF_TYPE_VBI;
    req.count = BUFS;
    if(ioctl(vbi->fd, VIDIOC_REQBUFS, &req)<0)
    {
        perror("req");
        return -1;
    }
    vbi->nbufs = req.count;
    for(i=0; i<vbi->nbufs; i++)
    {
        qbuf.type = V4L2_BUF_TYPE_VBI;
        qbuf.index = i;
        if(ioctl(vbi->fd, VIDIOC_QUERYBUF, &qbuf)<0)
        {
            perror("querybuf");
            return -1;
        }
        vbi->bufs[i] = mmap(NULL, qbuf.length, PROT_READ, MAP_SHARED, vbi->fd, qbuf.offset);
        if((int)vbi->bufs[i]==-1)
        {
            perror("mmap");
            return -1;
        }
        if(ioctl(vbi->fd, VIDIOC_QBUF, &qbuf)<0)
        {
            perror("qbuf");
            return -1;
        }
    }
    i = V4L2_BUF_TYPE_VBI;
    if(ioctl(vbi->fd, VIDIOC_STREAMON, &i)<0)
    {
        perror("streamon");
        return -1;
    }

    vbifmt->offset = 240;

    if (set_decode_parms(vbi, vbifmt) == -1)
	return -1;
	
    if (vbi->bpl < 1 || vbi->bufsize < vbi->bpl || vbi->bufsize % vbi->bpl != 0)
    {
	error("strange size of vbi buffer (%d/%d)", vbi->bufsize, vbi->bpl);
	return -1;
    }

    fdset_add_fd(fds, vbi->fd, vbi_handler, vbi);

    return 0;
}

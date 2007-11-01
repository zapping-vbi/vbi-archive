/*
 *  libzvbi test
 *
 *  Copyright (C) 2007 Michael H. Schimek
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

/* $Id: test-io.cc,v 1.1.2.1 2007-11-01 00:21:26 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#undef NDEBUG

#include <assert.h>
#include <stdlib.h>

#include "src/sampling_par.h"

#ifdef ENABLE_V4L2
#  ifndef HAVE_S64_U64
#    include <inttypes.h>
  /* Linux 2.6.x asm/types.h defines __s64 and __u64 only
     if __GNUC__ is defined. */
typedef int64_t __s64;
typedef uint64_t __u64;
#  endif

#  include <asm/types.h>	/* __u32 et al */
#  include <sys/time.h>		/* struct timeval */
#  include "src/videodev2k.h"

#endif

static void
test_video_standards		(void)
{
#ifdef ENABLE_V4L2
	v4l2_std_id std_id = 0;
#endif
	vbi3_videostd_set videostd_set = 0;

	assert (0 == VBI3_VIDEOSTD_NONE);

	assert (0 == VBI3_VIDEOSTD_UNKNOWN);

	assert (VBI3_VIDEOSTD_SET_UNKNOWN
		== VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_UNKNOWN));

	assert (0 == VBI3_VIDEOSTD_SET_EMPTY);

#undef CASE
#ifdef ENABLE_V4L2
#  define CASE(x)							\
	assert (V4L2_STD_ ## x						\
		== (VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_ ## x) >> 1));	\
	std_id |= V4L2_STD_ ## x;					\
	videostd_set |= VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_ ## x)
#else
#  define CASE(x)							\
	videostd_set |= VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_ ## x)
#endif

	CASE (PAL_B);
	CASE (PAL_B1);
	CASE (PAL_G);
	CASE (PAL_H);
	CASE (PAL_I);
	CASE (PAL_D);
	CASE (PAL_D1);
	CASE (PAL_K);
	CASE (PAL_M);
	CASE (PAL_N);
	CASE (PAL_60);

#ifdef ENABLE_V4L2
	assert (V4L2_STD_PAL_Nc
		== (VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_PAL_NC) >> 1));
	std_id |= V4L2_STD_PAL_Nc;
#endif
	videostd_set |= VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_PAL_NC);

	CASE (NTSC_M);
	CASE (NTSC_M_JP);
	CASE (NTSC_443);
	CASE (NTSC_M_KR);

	CASE (SECAM_B);
	CASE (SECAM_D);
	CASE (SECAM_G);
	CASE (SECAM_H);
	CASE (SECAM_K);
	CASE (SECAM_K1);
	CASE (SECAM_L);
	CASE (SECAM_LC);

	/* Libzvbi has no equivalent of V4L2_STD_ATSC_8_VSB and
	   V4L2_STD_ATSC_16_VSB. */

	assert (VBI3_VIDEOSTD_SET_ALL == videostd_set);

#ifdef ENABLE_V4L2
	assert (V4L2_STD_ALL == std_id);
	assert (V4L2_STD_ALL == (VBI3_VIDEOSTD_SET_ALL >> 1));

#  undef CASE
#  define CASE(x)							\
	assert (V4L2_STD_ ## x == (VBI3_VIDEOSTD_SET_ ## x >> 1))	\

	CASE (PAL_BG);
	CASE (PAL_DK);
	CASE (PAL);
	CASE (NTSC);
	CASE (SECAM_DK);
	CASE (SECAM);
	CASE (525_60);
	CASE (625_50);

	/* Libzvbi has no equivalent of V4L2_STD_ATSC. */
#endif

	assert (0 == (VBI3_VIDEOSTD_SET_525_60
		      & VBI3_VIDEOSTD_SET_625_50));

	assert (VBI3_VIDEOSTD_SET_ALL == (VBI3_VIDEOSTD_SET_525_60
					  | VBI3_VIDEOSTD_SET_625_50));
}

int
main				(void)
{
	test_video_standards ();

	exit (EXIT_SUCCESS);
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/

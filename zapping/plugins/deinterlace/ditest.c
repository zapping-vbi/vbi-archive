/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: ditest.c,v 1.6 2006-02-25 17:37:43 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "windows.h"
#include "DS_Deinterlace.h"
#include "libtv/cpu.h"		/* cpu_features */
#include "libtv/simd-consts.h"

cpu_feature_set			cpu_features;

static DEINTERLACE_METHOD *	method;

static TDeinterlaceInfo		info;
static TPicture			pictures[MAX_PICTURE_HISTORY];

static int			quiet;

static void
deinterlace			(char *			buffer,
				 unsigned int		width,
				 unsigned int		field_parity)
{
	static unsigned int field_count = 0;
	TPicture *p;

	if (!quiet) {
		fputc ('.', stderr);
		fflush (stderr);
	}

	p = info.PictureHistory[MAX_PICTURE_HISTORY - 1];

	memmove (info.PictureHistory + 1,
		 info.PictureHistory + 0,
		 (MAX_PICTURE_HISTORY - 1) * sizeof (TPicture *));

	info.PictureHistory[0] = p;

	if (0 == field_parity) {
		p->pData = (void *) buffer;
		p->Flags = PICTURE_INTERLACED_EVEN;	/* sic, if PAL */
		p->IsFirstInSeries = (0 == field_count);
	} else {
		p->pData = (void *)(buffer + width * 2);
		p->Flags = PICTURE_INTERLACED_ODD;
		p->IsFirstInSeries = (0 == field_count);
	}

	++field_count;

	if (field_count < (unsigned int) method->nFieldsRequired)
		return;

	method->pfnAlgorithm (&info);

	/* NOTE if method->bIsHalfHeight only the upper half of out_buffer
	   contains data, must be scaled. */
}

static void
init_info			(char *			out_buffer,
				 unsigned int		width,
				 unsigned int		height)
{
	unsigned int i;

	memset (&info, 0, sizeof (info));
  
	info.Version = DEINTERLACE_INFO_CURRENT_VERSION;
    
	for (i = 0; i < MAX_PICTURE_HISTORY; ++i)
		info.PictureHistory[i] = pictures + i;

	info.Overlay = (void *) out_buffer;
	info.OverlayPitch = width * 2;
	info.LineLength = width * 2;
	info.FrameWidth = width;
	info.FrameHeight = height;
	info.FieldHeight = height / 2;
	info.pMemcpy = (void *) memcpy;		/* XXX */
	info.InputPitch = width * 2 * 2;

	assert (!method->bNeedFieldDiff);
	assert (!method->bNeedCombFactor);
}

/* Make sure we produce predictable output on all platforms. */
static unsigned int
myrand				(void)
{
	static unsigned int seed = 1;

	seed = seed * 1103515245 + 12345;
	return (seed / 65536) % 32768;
}

static void
swab16				(char *			buffer,
				 unsigned int		size)
{
	unsigned int i;

	assert (0 == (size % 4));

	for (i = 0; i < size; i += 4) {
		char c;

		c = buffer[i + 0];
		buffer[i + 0] = buffer[i + 1];
		buffer[i + 1] = c;
	}
}

static void
swab32				(char *			buffer,
				 unsigned int		size)
{
	unsigned int i;

	assert (0 == (size % 4));

	for (i = 0; i < size; i += 4) {
		char c, d;

		c = buffer[i + 0];
		d = buffer[i + 1];
		buffer[i + 0] = buffer[i + 3];
		buffer[i + 1] = buffer[i + 2];
		buffer[i + 2] = d;
		buffer[i + 3] = c;
	}
}

static char *
new_buffer			(unsigned int		width,
				 unsigned int 		height)
{
	char *buffer;
	unsigned int size;
	unsigned int i;

	size = width * height * 2;

	buffer = malloc (size);
	assert (NULL != buffer);

	for (i = 0; i < size; i += 2) {
		buffer[i + 0] = 0x00;
		buffer[i + 1] = 0x80;
	}

	return buffer;
}

static void
write_buffer			(const char *		name,
				 char *			buffer,
				 unsigned int		width,
				 unsigned int		height)
{
	unsigned int size;
	size_t actual;
	FILE *fp;

	size = width * height * 2;

	if (name) {
		fp = fopen (name, "wb");
		assert (NULL != fp);
	} else {
		fp = stdout;
	}

	actual = fwrite (buffer, 1, size, fp);
	if (actual < size || ferror (fp)) {
		perror ("fwrite");
		exit (EXIT_FAILURE);
	}

	if (name) {
		fclose (fp);
	}
}

static const char
short_options [] = "24c:h:m:n:p:qrw:HV";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "swab16",	no_argument,		NULL,		'2' },
	{ "swab32",	no_argument,		NULL,		'4' },
	{ "cpu",	required_argument,	NULL,		'c' },
	{ "height",	required_argument,	NULL,		'h' },
	{ "help",	no_argument,		NULL,		'H' },
	{ "method",	required_argument,	NULL,		'm' },
	{ "nframes",	required_argument,	NULL,		'n' },
	{ "prefix",	required_argument,	NULL,		'p' },
	{ "quiet",	no_argument,		NULL,		'q' },
	{ "rand",	no_argument,		NULL,		'r' },
	{ "width",	required_argument,	NULL,		'w' },
	{ "version",	no_argument,		NULL,		'V' },
	{ 0, 0, 0, 0 }
};
#else
#  define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static void
usage				(FILE *			fp,
				 char **		argv)
{
	fprintf (fp,
		 "Zapping deinterlacer test version " VERSION "\n"
		 "Copyright (C) 2004-2005 Michael H. Schimek\n"
		 "This program is licensed under GPL 2. NO WARRANTIES.\n\n"
		 "Usage: %s [options] <images >images\n\n"
		 "Source images must be in raw YUYV format without headers.\n"
		 "Options:\n"
		 "-c | --cpu     CPU features:\n"
		 "		 0x0001 x86 time stamp counter\n"
		 "		 0x0002 x86 conditional moves\n"
		 "		 0x0004 MMX\n"
		 "		 0x0008 SSE\n"
		 "		 0x0010 SSE2\n"
		 "		 0x0020 AMD MMX extensions\n"
		 "		 0x0040 3DNow!\n"
		 "		 0x0080 3DNow! extensions\n"
		 "		 0x0100 Cyrix MMX extensions\n"
		 "		 0x0200 AltiVec\n"
		 "		 0x0400 SSE3\n"
		 "-h | --height  Image height (288)\n"
		 "-m | --method  Number or name of deinterlace method:\n"
		 "               VideoBob, VideoWeave, TwoFrame, Weave,\n"
		 "               Bob, ScalerBob, EvenOnly, OddOnly, Greedy,\n"
		 "               Greedy2Frame, GreedyH, TomsMoComp, MoComp2\n"
		 "-n | --nframes Number of frames to convert (5)\n"
		 "-p | --prefix  Name of output images instead of writing to\n"
		 "               stdout\n"
		 "-q | --quiet   Quiet, no output\n"
		 "-r | --rand    Convert pseudo-random images (for automated\n"
		 "               tests), otherwise read from stdin\n"
		 "-2 | --swab16  Swap every two bytes AB -> BA of the source\n"
		 "-4 | --swab32  Swap every four bytes ABCD -> DCBA\n"
		 "-w | --width   Image width (352)\n"
		 , argv[0]);
}

int
main				(int			argc,
				 char **		argv)
{
	char *out_buffer;
	char *in_buffers[(MAX_PICTURE_HISTORY + 1) / 2];
	int random_source;
	int swab2;
	int swab4;
	unsigned int n_frames;
	unsigned int width;
	unsigned int height;
	unsigned int size;
	char *method_name;
	char *prefix;
	unsigned int i;
	int index;
	int c;

	cpu_features = CPU_FEATURE_MMX;

	n_frames = 5;

	width = 352;
	height = 288;

	method_name = strdup ("1");

	prefix = NULL;

	random_source = FALSE;
	swab2 = FALSE;
	swab4 = FALSE;
	quiet = FALSE;

	while (-1 != (c = getopt_long (argc, argv, short_options,
				       long_options, &index))) {
		switch (c) {
		case '2':
			swab2 ^= TRUE;
			break;

		case '4':
			swab4 ^= TRUE;
			break;

                case 'c':
			cpu_features = strtol (optarg, NULL, 0);
                        break;

                case 'h':
			height = strtol (optarg, NULL, 0);
			if (0 == height || 0 != (height % 16)) {
				fprintf (stderr,
					 "Height must be a multiple of 16.\n");
				exit (EXIT_FAILURE);
			}
                        break;

                case 'm':
			method_name = optarg;
			break;

                case 'n':
			n_frames = strtol (optarg, NULL, 0);
			if (0 == n_frames) {
				fprintf (stderr,
					 "Number of frames must be "
					 "one or more.\n");
				exit (EXIT_FAILURE);
			}
                        break;

		case 'p':
			prefix = optarg;
			break;

		case 'q':
			quiet ^= TRUE;
			break;

		case 'r':
			random_source ^= TRUE;
			break;

                case 'w':
			width = strtol (optarg, NULL, 0);
			if (0 == width || 0 != (width % 16)) {
				fprintf (stderr,
					 "Width must be a multiple of 16.\n");
				exit (EXIT_FAILURE);
			}
                        break;

		case 'H':
			usage (stdout, argv);
			exit (EXIT_SUCCESS);

		case 'V':
			printf (VERSION "\n");
			exit (EXIT_SUCCESS);

		default:
			usage (stderr, argv);
			exit (EXIT_FAILURE);
		}
	}

	size = width * height * 2;

	i = 0;

#undef ELSEIF
#define ELSEIF(x)							\
  else if (++i == strtoul (method_name, NULL, 0)			\
	   || 0 == strcmp (#x, method_name))				\
      method = DI_##x##_GetDeinterlacePluginInfo (0);

	if (0) {
		exit (EXIT_FAILURE);    
	}
	ELSEIF (VideoBob)
	ELSEIF (VideoWeave)
	ELSEIF (TwoFrame)
	ELSEIF (Weave)
	ELSEIF (Bob)
	ELSEIF (ScalerBob)
	ELSEIF (EvenOnly)
	ELSEIF (OddOnly)
	/* No longer supported. ELSEIF (BlendedClip) */
	/* ELSEIF (Adaptive) */
	ELSEIF (Greedy)
	ELSEIF (Greedy2Frame)
	ELSEIF (GreedyH)
	/* ELSEIF (OldGame) */
	ELSEIF (TomsMoComp)
	ELSEIF (MoComp2)
	else {
		fprintf (stderr, "Unknown deinterlace method '%s'\n",
			 method_name);
		exit (EXIT_FAILURE);
	}

	if (NULL == method->pfnAlgorithm) {
		fprintf (stderr,
			 "No version of %s supports CPU features 0x%x\n",
			 method->szName, (unsigned int) cpu_features);
		exit (EXIT_SUCCESS);
	}

	out_buffer = new_buffer (width, height);

	for (i = 0; i < (MAX_PICTURE_HISTORY + 1) / 2; ++i)
		in_buffers[i] = new_buffer (width, height);

	init_info (out_buffer, width, height);

	if (!quiet)
		fprintf (stderr,
			 "Using '%s' ShortName='%s' "
			 "HalfHeight=%d FilmMode=%d\n"
			 "FrameRate=%lu,%lu ModeChanges=%ld ModeTicks=%ld\n"
			 "NeedFieldDiff=%d NeedCombFactor=%d\n",
			 method->szName, method->szShortName,
			 (int) method->bIsHalfHeight,
			 (int) method->bIsFilmMode,
			 method->FrameRate50Hz, method->FrameRate60Hz,
			 method->ModeChanges, method->ModeTicks,
			 (int) method->bNeedFieldDiff,
			 (int) method->bNeedCombFactor);

	for (i = 0; i < n_frames; ++i) {
		char name[40];
		size_t actual;

		if (random_source) {
			unsigned int j;

			for (j = 0; j < size; ++j) {
				in_buffers[i % 4][j] = myrand ();
			}
		} else {
			assert (!feof (stdin));

			actual = fread (in_buffers[i % 4], 1, size, stdin);
			if (actual < size || ferror (stdin)) {
				perror ("Read error");
				exit (EXIT_FAILURE);
			}

			if (swab2)
				swab16 (in_buffers[i % 4], size);
			if (swab4)
				swab32 (in_buffers[i % 4], size);
		}

		/* Top field */

		deinterlace (in_buffers[i % 4], width, 0);

		if (prefix) {
			snprintf (name, sizeof (name),
				  "%s%u-t.yuv", prefix, i);
			write_buffer (name, out_buffer, width, height);
		} else {
			write_buffer (NULL, out_buffer, width, height);
		}

		/* Bottom field */

		deinterlace (in_buffers[i % 4], width, 1);

		if (prefix) {
			snprintf (name, sizeof (name),
				  "%s%u-b.yuv", prefix, i);
			write_buffer (name, out_buffer, width, height);
		} else {
			write_buffer (NULL, out_buffer, width, height);
		}
	}

	if (!quiet)
		fprintf (stderr, "\n");

	return EXIT_SUCCESS;
}

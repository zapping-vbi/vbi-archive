/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2005 Michael H. Schimek
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

/* $Id: ditest.c,v 1.4.2.3 2005-05-17 19:58:32 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>		/* isatty() */
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "windows.h"
#include "DS_Deinterlace.h"
#include "libtv/cpu.h"		/* cpu_features */

/* See macros.h */
#define s8(n)  { n * 0x0101010101010101ULL, n * 0x0101010101010101ULL }
#define s16(n) { n * 0x0001000100010001ULL, n * 0x0001000100010001ULL }
#define s32(n) { n * 0x0000000100000001ULL, n * 0x0000000100000001ULL }
const int64_t vsplat8_m1[2]	= s8 (0xFF);
const int64_t vsplat8_1[2]	= s8 (1);
const int64_t vsplat8_127[2]	= s8 (127);
const int64_t vsplat8_15[2]	= s8 (15);
const int64_t vsplat16_255[2]	= s16 (255);
const int64_t vsplat16_256[2]	= s16 (256);
const int64_t vsplat16_m256[2]	= s16 (0xFF00);
const int64_t vsplat32_1[2]	= s32 (1);
const int64_t vsplat32_2[2]	= s32 (2);

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
		 "-c | --cpu     Expect CPU features (also mmx|sse|sse2):\n"
		 "		 tsc       x86 time stamp counter\n"
		 "		 cmov      x86 conditional moves\n"
		 "		 mmx\n"
		 "		 sse\n"
		 "		 sse2\n"
		 "		 amd-mmx   AMD MMX extensions\n"
		 "		 3dnow     3DNow!\n"
		 "		 3dnow-ext 3DNow! extensions\n"
		 "		 cyrix-mmc Cyrix MMX extensions\n"
		 "		 altivec\n"
		 "		 sse3\n"
		 "-h | --height  Image height (288)\n"
		 "-m | --method  Number or name of deinterlace method:\n"
		 "               VideoBob = 1, VideoWeave, TwoFrame, Weave,\n"
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
			cpu_features = cpu_feature_set_from_string (optarg);
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

	if (!random_source) {
		if (isatty (STDIN_FILENO)) {
			fprintf (stderr, "No image data on stdin\n");
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

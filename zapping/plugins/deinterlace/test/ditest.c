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

/* $Id: ditest.c,v 1.5 2006-04-12 01:42:17 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>		/* isatty() */
#include <sys/time.h>		/* gettimeofday() */
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "windows.h"
#include "DS_Deinterlace.h"
#include "guard.h"
#include "libtv/cpu.h"		/* cpu_features */

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

	assert (NULL != method->pfnAlgorithm);

	method->pfnAlgorithm (&info);

	/* NOTE if method->bIsHalfHeight only the upper half of out_buffer
	   contains data, must be scaled. */

	{
		struct timeval tv;
		long l;
		double d;

		gettimeofday (&tv, NULL);
		l = tv.tv_usec + field_count;
		d = tv.tv_usec + (double) field_count;
		/* vempty() check. XXX did we compile
		   for scalar SSE2 or x87? */
		assert (l == (long) d);
	}
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
	static uint32_t seed = 1;

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
	unsigned int padding;
	unsigned int i;

	size = width * height * 2;
	padding = size % getpagesize ();

	buffer = guard_alloc (size + padding);
	assert (NULL != buffer);

	buffer += padding;
	assert (0 == ((unsigned long) buffer % 16));

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
short_options [] = "24c:h:m:n:o:p:qrw:zHV";

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
	{ "option",	required_argument,	NULL,		'o' },
	{ "prefix",	required_argument,	NULL,		'p' },
	{ "quiet",	no_argument,		NULL,		'q' },
	{ "rand",	no_argument,		NULL,		'r' },
	{ "width",	required_argument,	NULL,		'w' },
	{ "zero",	required_argument,	NULL,		'z' },
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
		 "		 sse3\n"
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
		 "-z | --zero    Convert blank images (for automated tests)\n"
		 , argv[0]);
}

static void
set_method_options		(DEINTERLACE_METHOD *	method,
				 unsigned int		n_options,
				 char **		options)
{
	unsigned int i;

	if (0 == method->nSettings && n_options > 0) {
		fprintf (stderr, "This method has no options.\n");
		exit (EXIT_FAILURE);
	}

	for (i = 0; i < n_options; ++i) {
		const char *s;
		char *key;
		unsigned int length;
		unsigned int j;

		s = options[i];

		while (isalnum (*s))
			++s;

		length = s - options[i];
		key = malloc (length + 1);
		key[length] = 0;
		assert (NULL != key);
		strncpy (key, options[i], length);

		for (j = 0; j < (unsigned int) method->nSettings; ++j)
			if (0 == strcmp (method->pSettings[j].szIniEntry, key))
				break;

		if (j >= (unsigned int) method->nSettings) {
			fprintf (stderr, "Unknown method option '%s'. "
				 "Valid options are: ", key);

			for (j = 0; j < (unsigned int) method->nSettings; ++j)
				fprintf (stderr, "%s%s",
					 method->pSettings[j].szIniEntry,
					 (j + 1 ==
					  (unsigned int) method->nSettings) ?
					 "\n" : ", ");

			exit (EXIT_FAILURE);
		}

		free (key);
		key = NULL;

		while (0 != *s && !isalnum (*s))
			++s;

		*method->pSettings[j].pValue = strtol (s, NULL, 0);
	}
}

int
main				(int			argc,
				 char **		argv)
{
	char *out_buffer;
	char *in_buffers[(MAX_PICTURE_HISTORY + 1) / 2];
	int random_source;
	int zero_source;
	int swab2;
	int swab4;
	unsigned int n_frames;
	unsigned int width;
	unsigned int height;
	unsigned int size;
	char *method_name;
	char **method_options;
	unsigned int n_options;
	char *prefix;
	unsigned int i;
	int index;
	int c;

	cpu_features = 0;

	n_frames = 5;

	width = 352;
	height = 288;

	method_name = strdup ("1");

	method_options = NULL;
	n_options = 0;

	prefix = NULL;

	random_source = FALSE;
	zero_source = FALSE;
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

		case 'o':
		{
			unsigned int new_size;

			new_size = (n_options + 1)
				* sizeof (method_options[0]);

			method_options = realloc (method_options, new_size);
			assert (NULL != method_options);

			method_options[n_options++] = optarg;

			break;
		}

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

		case 'z':
			zero_source ^= TRUE;
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

	if (!random_source && !zero_source) {
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
      method = DI_##x##_GetDeinterlacePluginInfo ();

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

	if (NULL == method) {
		if (!cpu_features) {
			fprintf (stderr, "Have no scalar version of %s\n",
				 method_name);
		} else {
			fprintf (stderr, "Have no version of %s optimized "
				 "for feature 0x%x\n", method_name,
				 (unsigned int) cpu_features);
		}

		exit (55);
	}

	set_method_options (method, n_options, method_options);

	out_buffer = new_buffer (width, height);

	for (i = 0; i < (MAX_PICTURE_HISTORY + 1) / 2; ++i)
		in_buffers[i] = new_buffer (width, height);

	init_info (out_buffer, width, height);

	if (!quiet)
		fprintf (stderr,
			 "Using '%s' ShortName='%s' "
			 "HalfHeight=%d FilmMode=%d\n"
			 "FrameRate=%d,%d ModeChanges=%d ModeTicks=%d\n"
			 "NeedFieldDiff=%d NeedCombFactor=%d\n",
			 method->szName, method->szShortName,
			 (int) method->bIsHalfHeight,
			 (int) method->bIsFilmMode,
			 (int) method->FrameRate50Hz,
			 (int) method->FrameRate60Hz,
			 (int) method->ModeChanges,
			 (int) method->ModeTicks,
			 (int) method->bNeedFieldDiff,
			 (int) method->bNeedCombFactor);

	for (i = 0; i < n_frames; ++i) {
		char name[40];
		size_t actual;

		if (zero_source) {
			/* Nothing to do. */
		} else if (random_source) {
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

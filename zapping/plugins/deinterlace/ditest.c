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

/* $Id: ditest.c,v 1.2 2005-02-12 13:32:27 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "windows.h"
#include "DS_Deinterlace.h"

static long			cpu_feature_flags;

static DEINTERLACE_METHOD *	method;

static TDeinterlaceInfo		info;
static TPicture			pictures[MAX_PICTURE_HISTORY];

static void
deinterlace			(char *			buffer,
				 unsigned int		width,
				 unsigned int		field_parity)
{
  static unsigned int field_count = 0;
  TPicture *p;

  fputc ('.', stderr);
  fflush (stderr);

  p = info.PictureHistory[MAX_PICTURE_HISTORY - 1];

  memmove (info.PictureHistory + 1,
	   info.PictureHistory + 0,
	   (MAX_PICTURE_HISTORY - 1) * sizeof (TPicture *));

  info.PictureHistory[0] = p;

  if (0 == field_parity)
    {
      p->pData = buffer;
      p->Flags = PICTURE_INTERLACED_EVEN;	/* sic, if PAL */
      p->IsFirstInSeries = (0 == field_count);
    }
  else
    {
      p->pData = buffer + width * 2;
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

  for (i = 0; i < size; i += 2)
    {
      buffer[i + 0] = 0x00;
      buffer[i + 1] = 0x80;
    }

  return buffer;
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

  info.Overlay = out_buffer;
  info.OverlayPitch = width * 2;
  info.LineLength = width * 2;
  info.FrameWidth = width;
  info.FrameHeight = height;
  info.FieldHeight = height / 2;
  info.pMemcpy = (void *) memcpy;		/* XXX */
  info.CpuFeatureFlags = cpu_feature_flags;
  info.InputPitch = width * 2 * 2;

  assert (!method->bNeedFieldDiff);
  assert (!method->bNeedCombFactor);
}

static void
swab32				(char *			buffer,
				 unsigned int		size)
{
  unsigned int i;
  char c;
  char d;

  assert (0 == (size % 4));

  for (i = 0; i < size; i += 4)
    {
      c = buffer[i + 0];
      d = buffer[i + 1];
      buffer[i + 0] = buffer[i + 3];
      buffer[i + 1] = buffer[i + 2];
      buffer[i + 2] = d;
      buffer[i + 3] = c;
    }
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

  fp = fopen (name, "wb");
  assert (NULL != fp);

  actual = fwrite (buffer, 1, size, fp);
  if (actual < size || ferror (fp))
    {
      perror ("fwrite");
      exit (EXIT_FAILURE);
    }

  fclose (fp);
}

int
main				(int			argc,
				 char **		argv)
{
  char *out_buffer;
  char *in_buffers[(MAX_PICTURE_HISTORY + 1) / 2];
  unsigned int n_frames;
  unsigned int width;
  unsigned int height;
  unsigned int size;
  unsigned int i;

  cpu_feature_flags = (FEATURE_MMX |		/* XXX */
		       FEATURE_TSC);

  assert (5 == argc);

  n_frames = strtoul (argv[1], NULL, 0);

  assert (n_frames > 0);

  width = strtoul (argv[2], NULL, 0);
  height = strtoul (argv[3], NULL, 0);

  assert (width > 0 && 0 == (width % 2));	/* YUYV */
  assert (height > 0 && 0 == (height % 2));	/* interlaced */

  size = width * height * 2;

  i = 0;

#undef ELSEIF
#define ELSEIF(x)							\
  else if (++i == strtoul (argv[4], NULL, 0)				\
	   || 0 == strcmp (#x, argv[4]))				\
    {									\
      extern DEINTERLACE_METHOD *DI_##x##_GetDeinterlacePluginInfo (long); \
      method = DI_##x##_GetDeinterlacePluginInfo (cpu_feature_flags);	\
    }

  if (0)
    {
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
  ELSEIF (BlendedClip)
  ELSEIF (Adaptive)
  ELSEIF (Greedy)
  ELSEIF (Greedy2Frame)
  ELSEIF (GreedyH)
  ELSEIF (OldGame)
  ELSEIF (TomsMoComp)
  ELSEIF (MoComp2)
  else
    {
      assert (!"unknown method");
    }

  out_buffer = new_buffer (width, height);

  for (i = 0; i < (MAX_PICTURE_HISTORY + 1) / 2; ++i)
    in_buffers[i] = new_buffer (width, height);

  init_info (out_buffer, width, height);

  fprintf (stderr, "Using '%s' ShortName='%s' HalfHeight=%d FilmMode=%d\n"
	   "FrameRate=%lu,%lu ModeChanges=%ld ModeTicks=%ld\n"
	   "NeedFieldDiff=%d NeedCombFactor=%d\n",
	   method->szName, method->szShortName,
	   method->bIsHalfHeight, method->bIsFilmMode,
	   method->FrameRate50Hz, method->FrameRate60Hz,
	   method->ModeChanges, method->ModeTicks,
	   method->bNeedFieldDiff, method->bNeedCombFactor);

  for (i = 0; i < n_frames; ++i) {
    char name[40];
    size_t actual;

    assert (!feof (stdin));

    actual = fread (in_buffers[i % 4], 1, size, stdin);
    if (actual < size || ferror (stdin))
      {
	perror ("fread");
	exit (EXIT_FAILURE);
      }

    swab32 (in_buffers[i % 4], size);

    deinterlace (in_buffers[i % 4], width, 0);
    snprintf (name, sizeof (name), "di%03u0.yuv", i);
    write_buffer (name, out_buffer, width, height);

    deinterlace (in_buffers[i % 4], width, 1);
    snprintf (name, sizeof (name), "di%03u1.yuv", i);
    write_buffer (name, out_buffer, width, height);
  }

  fprintf (stderr, "\n");

  return EXIT_SUCCESS;
}

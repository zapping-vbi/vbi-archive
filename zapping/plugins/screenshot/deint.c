/*
 * Screenshot saving plugin for Zapping
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 * Copyright (C) 2001 Michael H. Schimek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "screenshot.h"

/* Deinterlace, simple version for now */

gchar *
screenshot_deinterlace (screenshot_data *data, gint parity)
{
  guchar *deint_data, *src, *src2, *dest;
  gint src_bpl, dest_bpl, x, y, pair;

  if (data->format.height & 1)
    return NULL;

  deint_data = g_malloc (data->format.width * data->format.height * 3);

  src = (gchar *) data->data.linear.data;
  src_bpl = data->data.linear.stride;

  dest = deint_data;
  dest_bpl = data->format.width * 3;

  for (y = 0; y < data->format.height; y++, src += src_bpl, dest += dest_bpl)
    memcpy (dest, src, data->format.width * 3);

  if (parity)
    {
      src = deint_data + dest_bpl;
      src2 = deint_data;
      pair = - dest_bpl * 2;
    }
  else
    {
      src = deint_data;
      src2 = deint_data + dest_bpl;
      pair = + dest_bpl * 2;
    }

  for (y = 0; y < data->format.height; y += 2)
    {
      for (x = 0; x < data->format.width; x++)
	{
	  const gint level = 12;
	  gint d, d1, m1;

	  d = src[0] - src2[0]; d1  = d * d;
	  d = src[1] - src2[1]; d1 += d * d;
	  d = src[2] - src2[2]; d1 += d * d;

	  if (d1 > (1 << (level / 3)))
	    {
	      if (d1 > (1 << level))
		d1 = 1 << level;
	      m1 = (1 << level) - d1;

	      if (y >= 2 && y < data->format.height - 2)
		{
		  d = (src[0] + src[0 + pair] + 1) >> 1;
		  src2[0] = (src2[0] * m1 + d * d1) >> level;
		  d = (src[1] + src[1 + pair] + 1) >> 1;
		  src2[1] = (src2[1] * m1 + d * d1) >> level;
		  d = (src[2] + src[2 + pair] + 1) >> 1;
		  src2[2] = (src2[2] * m1 + d * d1) >> level;
		}
	      else
		{
		  src2[0] = (src2[0] * m1 + src[0] * d1) >> level;
		  src2[1] = (src2[1] * m1 + src[1] * d1) >> level;
		  src2[2] = (src2[2] * m1 + src[2] * d1) >> level;
		}
	    }

	  src += 3;
	  src2 += 3;
	}

      src += dest_bpl;
      src2 += dest_bpl;
    }

  return deint_data;
}





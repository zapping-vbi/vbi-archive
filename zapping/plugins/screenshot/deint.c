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

void
screenshot_deinterlace		(void *				image,
				 const tv_image_format *	format,
				 gint				parity)
{
  uint8_t *src1;
  uint8_t *src2;
  guint height;
  gint pair;

  if (format->height & 1)
    return;

  if (format->height < 6)
    return;

  if (parity)
    {
      src1 = (uint8_t *) image + 2 * format->bytes_per_line[0];
      src2 = (uint8_t *) image;
      pair = -2 * format->bytes_per_line[0];
    }
  else
    {
      src1 = (uint8_t *) image;
      src2 = (uint8_t *) image + 2 * format->bytes_per_line[0];
      pair = +2 * format->bytes_per_line[0];
    }

  for (height = format->height - 4; height > 0; height -= 2)
    {
      guint width;

      for (width = format->width; width > 0; --width)
	{
	  const gint level = 8;
	  gint d, d1, m1;

	  d = src1[0] - src2[0]; d1  = d * d;
	  d = src1[1] - src2[1]; d1 += d * d;
	  d = src1[2] - src2[2]; d1 += d * d;

	  if (d1 > (1 << (level / 3)))
	    {
	      d1 = MIN (d1, 1 << level);
	      m1 = (1 << level) - d1;

	      d = (src1[0] + src1[0 + pair] + 1) >> 1;
	      src2[0] = (src2[0] * m1 + d * d1) >> level;
	      d = (src1[1] + src1[1 + pair] + 1) >> 1;
	      src2[1] = (src2[1] * m1 + d * d1) >> level;
	      d = (src1[2] + src1[2 + pair] + 1) >> 1;
	      src2[2] = (src2[2] * m1 + d * d1) >> level;
	    }

	  src1 += 3;
	  src2 += 3;
	}

      src1 += format->bytes_per_line[0];
      src2 += format->bytes_per_line[0];
    }
}

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/

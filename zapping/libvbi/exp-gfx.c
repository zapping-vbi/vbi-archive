
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lang.h"
#include "export.h"

#include "wstfont.xbm"

#undef WW
#undef WH
#undef CW
#undef CH

#define CW 12		/* character cell dimensions - hardcoded (DRCS) */
#define CH 10
#define WW (W*CW)
#define WH (H*CH)

#if 0

static void init_gfx(void) __attribute__ ((constructor));

static void
init_gfx(void)
{
	unsigned char *buf, *p;
	int i, j;

	/* de-interleave font image (puts all chars in row 0) */
	/* could load font image here rather than inline */
	/* XXX need blank line #10 for c & (1 << 11) or diacr bitmap */

	if (!(buf = malloc(wstfont_width * wstfont_height / 8)))
		exit(EXIT_FAILURE);
	p = buf;

	for (i = 0; i < CH; i++)
		for (j = 0; j < wstfont_height; p += wstfont_width / 8, j += CH)
			memcpy(p, wstfont_bits + (j + i) * wstfont_width / 8,
				wstfont_width / 8);

	memcpy(wstfont_bits, buf, wstfont_width * wstfont_height / 8);

	free(buf);
}

#endif

#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))

///////////////////////////////////////////////////////
// COMMON ROUTINES FOR PPM AND PNG

#if CW < 9 || CW > 16
#error CW out of range
#endif

static inline void
draw_char(unsigned int *canvas, unsigned int *pen, int glyph,
	int bold, int underline, glyph_size size)
{
	unsigned char *src1, *src2;
	int shift1, shift2;
	int x, y, base = 0; // XXX italic ? GL_ITALICS : 0;
	int ch = CH;
#if 1
	x = (glyph & 31) * CW;
	shift1 = x & 7;
	src1 = wstfont_bits + ((glyph & 0x03FF) >> 5) * CH * CW * 32 / 8 + (x >> 3);

	x = (glyph >> 20) * CW;
	shift2 = (x & 7) + ((glyph >> 18) & 1);
	src2 = wstfont_bits + ((base + 0xC0) >> 5) * CH * CW * 32 / 8 + (x >> 3);
	if (glyph & 0x080000) src2 += CW * 32 / 8;
#else
	x = (glyph & 0xFFFF) * CW;
	shift1 = x & 7;
	src1 = wstfont_bits + (x >> 3);

	x = (base + 0xC0 + (glyph >> 20)) * CW;
	shift2 = (x & 7) + ((glyph >> 18) & 1);
	src2 = wstfont_bits + (x >> 3);
	if (glyph & (1 << 19))
		src2 += wstfont_width * (wstfont_height / CH) / 8;
#endif

	for (y = 0; y < ch; y++) {
#if #cpu (i386)
		int bits = (*((u16 *) src1) >> shift1) | (*((u16 *) src2) >> shift2);
#else
		int bits = ((src1[1] * 256 + src1[0]) >> shift1)
			 | ((src2[1] * 256 + src2[0]) >> shift2); /* unaligned/little endian */
#endif
		if (bold)
			bits |= bits << 1;

		if (underline && y == 9)
			bits |= 0x0FFF;

		switch (size) {
		case NORMAL:
			for (x = 0; x < CW; bits >>= 1, x++)
				canvas[x] = pen[bits & 1];

			canvas += WW;

			break;

		case DOUBLE_HEIGHT:
			for (x = 0; x < CW; bits >>= 1, x++) {
				unsigned int col = pen[bits & 1];

				canvas[x] = col;
				canvas[x + WW] = col;
			}

			canvas += 2 * WW;

			break;

		case DOUBLE_WIDTH:
			for (x = 0; x < CW * 2; bits >>= 1, x += 2) {
				unsigned int col = pen[bits & 1];

				canvas[x + 0] = col;
				canvas[x + 1] = col;
			}

			canvas += WW;

			break;

		case DOUBLE_SIZE:
			for (x = 0; x < CW * 2; bits >>= 1, x += 2) {
				unsigned int col = pen[bits & 1];

				canvas[x + 0] = col;
				canvas[x + 1] = col;
				canvas[x + WW + 0] = col;
				canvas[x + WW + 1] = col;
			}

			canvas += 2 * WW;

			break;

		default:
			break;
		}
#if 1
		src1 += CW * 32 / 8;
		src2 += CW * 32 / 8;
#else
		src1 += wstfont_width * (wstfont_height / CH) / 8;
		src2 += wstfont_width * (wstfont_height / CH) / 8;
#endif
	}
}

static inline void
draw_drcs(unsigned int *canvas, unsigned char *src, unsigned int *pen, int glyph, glyph_size size)
{
	unsigned int col;
	int x, y;

	src += (glyph & 0x3F) * 60;
	pen += (glyph >> 16);

	switch (size) {
	case NORMAL:
		for (y = 0; y < CH; canvas += WW, y++)
			for (x = 0; x < 12; src++, x += 2) {
				canvas[x + 0] = pen[*src & 15];
				canvas[x + 1] = pen[*src >> 4];
			}
		break;

	case DOUBLE_HEIGHT:
		for (y = 0; y < CH; canvas += 2 * WW, y++)
			for (x = 0; x < 12; src++, x += 2) {
				col = pen[*src & 15];
				canvas[x + 0] = col;
				canvas[x + WW + 0] = col;

				col = pen[*src >> 4];
				canvas[x + 1] = col;
				canvas[x + WW + 1] = col;
			}
		break;

	case DOUBLE_WIDTH:
		for (y = 0; y < CH; y++)
			for (x = 0; x < 12 * 2; src++, x += 4) {
				col = pen[*src & 15];
				canvas[x + 0] = col;
				canvas[x + 1] = col;

				col = pen[*src >> 4];
				canvas[x + 2] = col;
				canvas[x + 3] = col;
			}
		break;

	case DOUBLE_SIZE:
		for (y = 0; y < CH; canvas += 2 * WW, y++)
			for (x = 0; x < 12 * 2; src++, x += 4) {
				col = pen[*src & 15];
				canvas[x + 0] = col;
				canvas[x + 1] = col;
				canvas[x + WW + 0] = col;
				canvas[x + WW + 1] = col;

				col = pen[*src >> 4];
				canvas[x + 2] = col;
				canvas[x + 3] = col;
				canvas[x + WW + 2] = col;
				canvas[x + WW + 3] = col;
			}
	default:
		break;
	}
}

static void
draw_page(struct fmt_page *pg, unsigned int *canvas)
{
	unsigned int pen[64];
	int row, column;
	attr_char *ac;
	int i;

	for (i = 2; i < 64; i++)
		pen[i] = pg->colour_map[pg->drcs_clut[i]];

	for (row = 0; row < H; canvas += W * CW * CH - W * CW, row++) {
		for (column = 0; column < W; canvas += CW, column++) {
			ac = &pg->data[row][column];

			pen[0] = pg->colour_map[ac->background];
			pen[1] = pg->colour_map[ac->foreground];

			if (ac->size <= DOUBLE_SIZE) {
				if ((ac->glyph & 0xFFFF) >= GL_DRCS) {
					draw_drcs(canvas, pg->drcs[(ac->glyph & 0x1F00) >> 8],
						pen, ac->glyph, ac->size);
				} else
					draw_char(canvas, pen, ac->glyph,
						ac->bold, ac->underline, ac->size);
			}
		}
	}
}

///////////////////////////////////////////////////////
// STUFF FOR PPM OUTPUT

static int ppm_output(struct export *e, char *name, struct fmt_page *pg);

struct export_module export_ppm[1] =	// exported module definition
{
  {
    "ppm",			// id
    "ppm",			// extension
    0,				// options
    0,				// size
    0,				// open
    0,				// close
    0,				// option
    ppm_output			// output
  }
};

static int
ppm_output(struct export *e, char *name, struct fmt_page *pg)
{
  FILE *fp;
  long n;
  static u8 rgb1[][3]={{0,0,0},
		      {1,0,0},
		      {0,1,0},
		      {1,1,0},
		      {0,0,1},
		      {1,0,1},
		      {0,1,1},
		      {1,1,1}};
  
  unsigned char *colour_matrix;

return 0;

  if (!(colour_matrix=malloc(WH*WW))) 
    {
      export_error("cannot allocate memory");
      return 0;
    }

//  prepare_colour_matrix(/*e,*/ pg, (unsigned char *)colour_matrix); 
  
  if (not(fp = fopen(name, "w")))
    {
      free(colour_matrix);
      export_error("cannot create file");
      return -1;
    }
  
  fprintf(fp,"P6 %d %d 1\n", WW, WH);

  for(n=0;n<WH*WW;n++)
    {
      if (!fwrite(rgb1[(int) *(colour_matrix+n)], 3, 1, fp))
	{
	  export_error("error while writting to file");
	  free(colour_matrix);
	  fclose(fp);
	  return -1;
	}
    }
  
  free(colour_matrix);
  fclose(fp);
  return 0;
}

/* garetxe: This doesn't make sense in alevt, but it's useful in other
 contexts */
unsigned int *
mem_output(struct fmt_page *pg, int *width, int *height)
{
  unsigned int *mem;

  if ((!pg) || (!width) || (!height))
    return (unsigned int *) -1;

  mem = malloc(CW * CH * W * H * sizeof(unsigned int));
  if (!mem)
    {
      perror("malloc");
      return NULL;
    }

  *width = WW;
  *height = WH;

  draw_page(pg, mem);
  return mem;
}


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
#define CW 12
#define CH 10
#define WW (W*CW)
#define WH (H*CH)

// static void init_gfx(void) __attribute__ ((constructor));

#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))

///////////////////////////////////////////////////////
// COMMON ROUTINES FOR PPM AND PNG

#if CW < 9 || CW > 16
#error CW out of range
#endif

static inline void
draw_char(unsigned int *canvas, unsigned int *pen, int c, glyph_size size)
{
	unsigned char *src1, *src2;
	int shift1, shift2;
	int x, y;

// printf("DC <%c> %d\n", printable(c), size);

	x = (c & 31) * CW;
	shift1 = x & 7;
	src1 = wstfont_bits + ((c & 0x3FF) >> 5) * CH * CW * 32 / 8 + (x >> 3);

	x = (c >> 12) * CW;
	shift2 = (x & 7) + ((c >> 10) & 1);
	src2 = wstfont_bits + (0xC0 >> 5) * CH * CW * 32 / 8 + (x >> 3);
	if (c & 0x800) src2 += CW * 32 / 8;

	for (y = 0; y < CH; y++) {
#if #cpu (i386)
		int bits = (*((u16 *) src1) >> shift1) | (*((u16 *) src2) >> shift2);
#else
		int bits = ((src1[1] * 256 + src1[0]) >> shift1)
			 | ((src2[1] * 256 + src2[0]) >> shift2); /* unaligned/little endian */
#endif
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

		src1 += CW * 32 / 8;
		src2 += CW * 32 / 8;
	}
}

static inline void
draw_drcs(unsigned int *canvas, unsigned char *src, unsigned int *pen, int c, glyph_size size)
{
	unsigned int col;
	int x, y;

	src += (c & 0x3F) * 60;
	pen += (c >> 10);

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
				if ((ac->glyph & 0x3FF) >= 0x3C0)
					draw_drcs(canvas, pg->drcs, pen, ac->glyph, ac->size);
				else
					draw_char(canvas, pen, ac->glyph, ac->size);
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

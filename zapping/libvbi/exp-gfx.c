/* Copyright 1999 by Paul Ortyl <ortylp@from.pl> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lang.h"
#include "export.h"

#include "font.h"
#define WW	(W*CW)			/* pixel width of window */
#define WH	(H*CH)			/* pixel hegiht of window */


///////////////////////////////////////////////////////
// COMMON ROUTINES FOR PPM AND PNG

static inline void
draw_char(unsigned char * colour_matrix, 
	  int fg, int bg, int c, int dbl, int _x, int _y, 
	  int sep)
{
  int x,y;
  unsigned char* src= (latin1 ? vbi_get_glyph_bitmap(1) :
		       vbi_get_glyph_bitmap(2));
  int dest_x=_x*CW;
  int dest_y=_y*CH;
      
  for(y=0;y<(CH<<dbl); y++)
    {
      for(x=0;x<CW; x++)
	{
	  int bitnr, bit, maskbitnr, maskbit;
	  bitnr=(c/32*CH + (y>>dbl))*CW*32+ c%32*CW +x;
	  bit=(*(src+bitnr/8))&(1<<bitnr%8);
	  if (sep)
	    {
	      maskbitnr=(0xa0/32*CH + (y>>dbl))*CW*32+ 0xa0%32*CW +x;
	      maskbit=(*(src+maskbitnr/8))&(1<<maskbitnr%8);
	      *(colour_matrix+WW*(dest_y+y)+dest_x+x)=
		(char)((bit && (!maskbit)) ? fg : bg);
	    }
	  else 
	    *(colour_matrix+WW*(dest_y+y)+dest_x+x)=
	      (char)(bit ? fg : bg);
	}
    }
  return;
}

static void
prepare_colour_matrix(/*struct export *e,*/
		      struct fmt_page *pg, 
		      unsigned char *colour_matrix)
{
   int x, y;
   //   bzero(colour_matrix, WH*WW);
   for (y = 0; y < H; ++y)
	{
	  for (x = 0; x < W; ++x)
	    { 
	      if (pg->dbl & (1<<(y-1)))
		{
		  if (pg->data[y-1][x].attr & EA_HDOUBLE)
		     draw_char(colour_matrix, pg->data[y][x].fg, 
			    pg->data[y][x].bg, pg->data[y][x].ch, 
			    (0), 
			    x, y, 
			    ((pg->data[y][x].attr & EA_SEPARATED) ? 1 : 0)
			    );
		}
	      else
		{
		  draw_char(colour_matrix, pg->data[y][x].fg, 
			    pg->data[y][x].bg, pg->data[y][x].ch, 
			    ((pg->data[y][x].attr & EA_DOUBLE) ? 1 : 0), 
			    x, y, 
			    ((pg->data[y][x].attr & EA_SEPARATED) ? 1 : 0)
			    );
		}
	    }
	}
    return;
}



///////////////////////////////////////////////////////
// STUFF FOR PPM OUTPUT

static int ppm_output(struct export *e, char *name, struct fmt_page *pg);

struct export_module export_ppm[1] =	// exported module definition
{
    "ppm",			// id
    "ppm",			// extension
    0,				// options
    0,				// size
    0,				// open
    0,				// close
    0,				// option
    ppm_output			// output
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

  if (!(colour_matrix=malloc(WH*WW))) 
    {
      export_error("cannot allocate memory");
      return 0;
    }

  prepare_colour_matrix(/*e,*/ pg, (unsigned char *)colour_matrix); 
  
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
unsigned char *
mem_output(struct fmt_page *pg, int *width, int *height)
{
  unsigned char *mem;

  if ((!pg) || (!width) || (!height))
    return -1;

  mem = malloc(WW*WH);
  if (!mem)
    {
      perror("malloc");
      return NULL;
    }

  *width = WW;
  *height = WH;

  prepare_colour_matrix(pg, mem);
  return mem;
}

/*
 * Some experiments with shaped windows and scrolling.
 * This will go into caption.c when functional.
 * gcc -o shaped_caption `gtk-config --cflags --libs` shaped_caption.c
 */
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>

#include <stdlib.h>
#include <time.h>

#include "ccfont.xbm"
#define NUM_COLS	34
#define NUM_ROWS	15
#define CHAR_WIDTH (bitmap_width/32)
#define CHAR_HEIGHT (bitmap_height/8)
#define CC_WIDTH (CHAR_WIDTH*NUM_COLS)
#define CC_HEIGHT (CHAR_HEIGHT*NUM_ROWS)

static GtkWidget * win; /* window we show */
static GdkPixmap * pixmap; /* pixmap we draw to */
static GdkBitmap * bitmap; /* transparency mask for pixmap */
static GdkGC * bitmap_gc; /* graphics context for the bitmap */
static GdkBitmap * alphabet; /* server-side copy of the glyphs */
static GdkColor bitmap_white, bitmap_black;
static gint draw_offset = 0; /* vertical shift between pixmap and win */
static guint32 char_canvas[CHAR_WIDTH*CHAR_HEIGHT];

/* 1 for smoother scroll, CHAR_HEIGHT or above for no scroll */
#define LINES_PER_BLOCK CHAR_HEIGHT-1
#define MOVES_PER_SEC ((CHAR_HEIGHT+5)*5)

static guint32 palette[9] = {
	0x00000000,
	0xff000000,
	0x00ff0000,
	0xffff0000,
	0x0000ff00,
	0xff00ff00,
	0x00ffff00,
	0xffffff00
};

static inline void
render_char(guint32 * canvas, unsigned int c, int underline,
	    int fg_color, int bg_color)
{
	guint16 *s = ((guint16 *) bitmap_bits)
		+ (c & 31) + (c >> 5) * 32 * CHAR_HEIGHT;
	int x, y, b;
	guint32 pen[2];
	
	pen[0] = palette[fg_color];
	pen[1] = palette[(bg_color!=8)?bg_color:0];
	
	for (y=0; y < CHAR_HEIGHT; y++) {
		b = *s;
		s+=32;
		
		if (underline && (y >= 24 && y <= 25))
			b = ~0;

		for (x=0; x < CHAR_WIDTH; x++) {
			canvas[x] = pen[b & 1];
			b >>= 1;
		}

		canvas += CHAR_WIDTH;
	}
}

static inline
void draw_char(int c, int underline, int fg_color, int bg_color,
	       int x, int y)
{
	int glyph_x, glyph_y;

	glyph_x = (c & 31)*CHAR_WIDTH;
	glyph_y = (c>>5)*CHAR_HEIGHT;

	render_char(char_canvas, c, underline, fg_color, bg_color);
	gdk_draw_rgb_32_image(pixmap, win->style->white_gc, x,
			      y, CHAR_WIDTH, CHAR_HEIGHT,
			      GDK_RGB_DITHER_NORMAL,
			      (guchar*)char_canvas,
			      CHAR_WIDTH*4);
	if (bg_color != 8) {
		gdk_gc_set_foreground(bitmap_gc, &bitmap_white);
		gdk_draw_rectangle(bitmap, bitmap_gc, TRUE, x, y,
				   CHAR_WIDTH, CHAR_HEIGHT);
	} else /* chroma */
		gdk_draw_pixmap(bitmap, bitmap_gc, alphabet, glyph_x, glyph_y,
				x, y, CHAR_WIDTH, CHAR_HEIGHT);
}

static void
draw_tspaces(int x, int y, int num_chars)
{
	gdk_gc_set_foreground(bitmap_gc, &bitmap_black);
	gdk_draw_rectangle(bitmap, bitmap_gc, TRUE, x, y,
			   num_chars * CHAR_WIDTH, CHAR_HEIGHT);
}

static
gint shift_one_block(gpointer user_data)
{
	gint * lines_to_scroll = (gint*) user_data;
	gint scroll_amount;

	if (*lines_to_scroll > 0) {
		scroll_amount = (*lines_to_scroll >= LINES_PER_BLOCK) ?
			LINES_PER_BLOCK : *lines_to_scroll;
		scroll_amount = (scroll_amount <= CHAR_HEIGHT) ?
		  scroll_amount : CHAR_HEIGHT;
		gdk_window_copy_area(win->window,
				     win->style->white_gc,
				     0, 0, win->window,
				     0, scroll_amount,
				     CC_WIDTH,
				     CC_HEIGHT-scroll_amount);
		gdk_draw_pixmap(win->window, win->style->white_gc,
				pixmap, 0,
				(CC_HEIGHT-1)+scroll_amount+draw_offset, 0,
				CC_HEIGHT-scroll_amount, CC_WIDTH,
				scroll_amount);
		gdk_draw_pixmap(bitmap, bitmap_gc, bitmap, 0,
				scroll_amount, 0, 0,
				CC_WIDTH, CC_HEIGHT);
		gdk_window_shape_combine_mask(win->window, bitmap, 0, 0);
		*lines_to_scroll -= scroll_amount;
		draw_offset += scroll_amount;
		return TRUE;
	}

	draw_offset -= CHAR_HEIGHT;
	/* Done scrolling, update back pixmap */
	gdk_draw_pixmap(pixmap, win->style->white_gc, pixmap,
			0, 0, 0, CC_HEIGHT+CHAR_HEIGHT,
			CC_WIDTH, CHAR_HEIGHT);
	gdk_draw_pixmap(pixmap, win->style->white_gc, pixmap,
			0, CHAR_HEIGHT, 0, 0, CC_WIDTH,	CC_HEIGHT);
	gdk_draw_pixmap(pixmap, win->style->white_gc, pixmap,
			0, CC_HEIGHT+CHAR_HEIGHT, 0, CC_HEIGHT,
			CC_WIDTH, CHAR_HEIGHT);
	g_free(lines_to_scroll);
	return FALSE;
}

static
void rollup(void)
{
	gint *lines_to_scroll =
		g_malloc(sizeof(gint));

	*lines_to_scroll = CHAR_HEIGHT;

	gtk_timeout_add(1000/MOVES_PER_SEC, shift_one_block,
			lines_to_scroll);
}

static void
draw_line(const char *text, int fg, int bg, int underline, int italic)
{
	static int draw_pointer = 0;
	int i, x = rand()%10;
	
	/* clear this line */
	gdk_gc_set_foreground(bitmap_gc, &bitmap_black);
	gdk_draw_rectangle(bitmap, bitmap_gc, TRUE, 0,
			   draw_pointer*CHAR_HEIGHT,
			   CC_WIDTH, CHAR_HEIGHT);

	for (i=0; (i<strlen(text)) && (i<34); i++)
		draw_char(text[i]+(italic?128:0), underline, fg, bg,
			  (i+x)*CHAR_WIDTH, draw_pointer*CHAR_HEIGHT);

	if (draw_pointer == 15) {
		rollup();
		draw_pointer--;
	}

	gdk_window_shape_combine_mask(win->window, bitmap, 0, 0);

	draw_pointer ++;
}

static gint
dump_strange_info(gpointer user_data)
{
	char *messages[] = {
		"Hello, i'm your string",
		"testing scrolling",
		"Whew, who's that scrolling little long and short and"
		" green and black line that doesn't fit?",
		"I hope the clouds get better",
		"How many words are there in the Enciclopaedia?"
	};
	int num_messages = sizeof(messages)/sizeof(char*);

	draw_line(messages[rand()%num_messages],
		  rand()%8, rand()%8, rand()%2, rand()%2);

	return TRUE;
}


static
gboolean
on_cc_test_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
	gtk_main_quit();
  
	return FALSE;
}

static
gboolean
on_cc_key_press_event		       (GtkWidget	*widget,
					GdkEvent	*event,
					gpointer	user_data)
{
	if ((event->key.keyval == GDK_Escape) ||
	    (event->key.keyval == GDK_q) ||
	    (event->key.keyval == GDK_Q))
		gtk_main_quit();

	return FALSE;
}

#define update_region(x, y, w, h) \
  gdk_draw_pixmap(win->window, win->style->white_gc, pixmap, x, y+draw_offset, x, y, w, h)

static void
on_cc_test_expose_event		(GtkWidget	*widget,
				 GdkEvent	*event,
				 gpointer	user_data)
{
	update_region(event->expose.area.x, event->expose.area.y,
		      event->expose.area.width, event->expose.area.height);
}

int main(int argc, char * argv[])
{
	gtk_init(&argc, &argv);
	
	gdk_rgb_init();
	gtk_widget_set_default_colormap (gdk_rgb_get_cmap());
	gtk_widget_set_default_visual (gdk_rgb_get_visual());
	
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	srand(time(NULL));
	
	gtk_window_set_title(GTK_WINDOW(win), "CC decoder test");
	gtk_widget_set_usize(win, CC_WIDTH, CC_HEIGHT);
	gtk_window_set_policy(GTK_WINDOW(win), FALSE, FALSE, FALSE);
	gtk_widget_add_events(win, GDK_EXPOSURE_MASK |
			      GDK_KEY_PRESS_MASK);
	gtk_widget_set_app_paintable(win, TRUE);
	gtk_widget_realize(win);
	while (gtk_events_pending() || !(win->window))
		gtk_main_iteration(); /* wait until the window is
					 created */
	
	pixmap = gdk_pixmap_new(win->window, CC_WIDTH,
				CC_HEIGHT+CHAR_HEIGHT*2, -1);
	bitmap = gdk_pixmap_new(win->window, CC_WIDTH,
				CC_HEIGHT+CHAR_HEIGHT, 1);
	bitmap_gc = gdk_gc_new(bitmap);
	bitmap_white.pixel = 1;
	bitmap_black.pixel = 0;
	gdk_gc_set_foreground(bitmap_gc, &bitmap_black);
	gdk_draw_rectangle(bitmap, bitmap_gc, TRUE, 0, 0, CC_WIDTH,
			   CC_HEIGHT+CHAR_HEIGHT);
	alphabet = gdk_bitmap_create_from_data(win->window, bitmap_bits,
					       bitmap_width, bitmap_height);

	gdk_window_set_back_pixmap(win->window, NULL, FALSE);
	gdk_window_set_decorations(win->window, 0);
	gdk_window_shape_combine_mask(win->window, bitmap, 0, 0);
	
	gtk_signal_connect(GTK_OBJECT(win), "expose-event",
			   GTK_SIGNAL_FUNC(on_cc_test_expose_event),
			   NULL);

	gtk_signal_connect(GTK_OBJECT(win), "key-press-event",
			   GTK_SIGNAL_FUNC(on_cc_key_press_event),
			   NULL);
	
	gtk_signal_connect(GTK_OBJECT(win), "delete-event",
			   GTK_SIGNAL_FUNC(on_cc_test_delete_event), NULL);
	
	gtk_timeout_add(1500, dump_strange_info, NULL);
	
	gtk_widget_show(win);
	
	gtk_main();
	
	return 0;
}


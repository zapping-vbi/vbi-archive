/*
  File saving routines. Currently it only supports PNG format, but
  some sort of video (not just screenshot) capture should be added
  later on
*/

#include "io.h"

/* Conversions between different pixel formats */
/* This Convert... functions aren't exported because they are very
   io.c especific
*/
gchar *
Convert_RGB565_RGB24 (int width, gchar* src, gchar* dest);

/* Conversions between different pixel formats */
gchar *
Convert_RGB555_RGB24 (int width, gchar* src, gchar* dest);

/* Removes the last byte (suppoosedly alpha channel info) from the
   image */
gchar *
Convert_RGBA_RGB24 (int width, gchar* src, gchar* dest);

/* The definition of a line converter function */
/* The purpose of this kind of functions is to convert one line of src
 into one line of dest */
typedef gchar* (*LineConverter) (int width, gchar* src, gchar* dest);

/* 
   Returns TRUE if the capture could be successfully completed.
   Args: 
      - handle: the handle we will write to
      - window_title: If show_progress = TRUE, give to the progress
                      window this title
      - show_progress: if TRUE, show a progress bar while saving
      - data: The data to be saved
      - info: The capture device struct 
FIXME: this gives some warnings bacause of the nasty setjmp, don't
worry about them (i don't jnow how to fix them)
*/
gboolean
zapping_save_png (FILE * handle,
		  gchar * window_title,
		  gboolean show_progress,
		  gchar * data,
		  tveng_device_info * info)
{
  GtkWidget * window = NULL;
  GtkWidget * label = NULL;
  GtkWidget * progressbar = NULL;
  GtkWidget * vbox = NULL;
  gint width=0, height=0, depth=0, rowstride=0;
  gchar * pixels; /* Buffer we will write */
  png_structp png_ptr;
  png_infop info_ptr;
  png_text text[2];
  int i=0;
  __u32 cur_pixformat = info -> pix_format.fmt.pix.pixelformat;
  gboolean set_bgr = FALSE; /* FALSE if RGB is on */
  gchar * dest = NULL; /* Where to store the converted buffer (always RGB) */
  LineConverter Converter = NULL; /* The line converter, could be NULL (no
				      conversion) */

  /* A buffer for storing a converted row */
  dest = (gchar*) malloc(info -> pix_format.fmt.pix.width * 3);

  if (!dest)
    {
      ShowBox(_("Cannot allocate enough memory for saving the image"),
	      GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    }

  png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png_ptr == NULL)
    {
      ShowBox(_("Cannot init the first PNG saving structure"),
	      GNOME_MESSAGE_BOX_ERROR);

      g_free(dest);
      
      return FALSE;
    }

  info_ptr = png_create_info_struct (png_ptr);
  if (info_ptr == NULL)
    {
      ShowBox(_("Cannot init the second PNG saving structure"),
	      GNOME_MESSAGE_BOX_ERROR);

      g_free(dest);

      return FALSE;
    }
  
  width = info -> pix_format.fmt.pix.width;
  height = info -> pix_format.fmt.pix.height;
  depth = 8; /* We are always writing 0..255 */
  pixels = data;
  rowstride = info -> bpl;

  /* Make necessary format corrections */
  switch (cur_pixformat)
    {
    case V4L2_PIX_FMT_RGB32:
      set_bgr = FALSE;
      Converter = (LineConverter) Convert_RGBA_RGB24;
      break;
    case V4L2_PIX_FMT_RGB24:
      set_bgr = FALSE;
      Converter = NULL;
      break;
    case V4L2_PIX_FMT_BGR32:
      set_bgr = TRUE;
      Converter = (LineConverter) Convert_RGBA_RGB24;
      break;
    case V4L2_PIX_FMT_BGR24:
      set_bgr = TRUE;
      Converter = NULL; /* No conversion needed */
      break;
    case V4L2_PIX_FMT_RGB565:
      set_bgr = TRUE; /* FIXME: Only if we are in an intel-like machine */
      Converter = (LineConverter) Convert_RGB565_RGB24;
      break;
    case V4L2_PIX_FMT_RGB555:
      set_bgr = TRUE; /* FIXME: Only if we are in an intel-like
			 machine */
      Converter = (LineConverter) Convert_RGB555_RGB24;
      break;
    }

  /* OK, create the progress window if necessary */
  if (show_progress)
    {
      window = gtk_window_new(GTK_WINDOW_DIALOG);
      progressbar = gtk_progress_bar_new_with_adjustment(
                    GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 100, 1,
						      10, 10)));
      gtk_window_set_title(GTK_WINDOW(window), window_title);
      gtk_widget_show(progressbar);
      vbox = gtk_vbox_new (FALSE, 0);
      label = gtk_label_new(window_title);
      gtk_widget_show(label);
      gtk_box_pack_start_defaults(GTK_BOX(vbox),label);
      gtk_box_pack_start_defaults(GTK_BOX(vbox), progressbar);
      gtk_widget_show(vbox);
      gtk_container_add (GTK_CONTAINER(window), vbox);
      gtk_window_set_modal(GTK_WINDOW(window), TRUE);
      gtk_widget_show (window);
    }

  if (setjmp (png_ptr->jmpbuf))
    {
      /* Error handler */
      png_destroy_write_struct (&png_ptr, &info_ptr);

      printf(_("Error writing the PNG file\n"));

      if (show_progress)
	gtk_widget_destroy(window);

      g_free(dest);

      return FALSE;
    }

  png_init_io (png_ptr, handle);

  png_set_IHDR (png_ptr, info_ptr, width, height,
		depth, PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE,
		PNG_FILTER_TYPE_BASE);

  if (set_bgr)
    png_set_bgr (png_ptr);

  /* Some text to go with the png image */
  text[0].key = "Title";
  text[0].text = window_title;
  text[0].compression = PNG_TEXT_COMPRESSION_NONE;
  text[1].key = "Software";
  text[1].text = "Zapping";
  text[1].compression = PNG_TEXT_COMPRESSION_NONE;
  png_set_text (png_ptr, info_ptr, text, 2);

  /* Write header data */
  png_write_info (png_ptr, info_ptr);

  for (i = 0; i < height; i++)
    {
      png_bytep row_pointer;

      if (Converter) /* The row needs conversion */
	row_pointer = (*Converter)(width, pixels, dest);
      else /* No conversion needed */
	row_pointer = pixels;

      png_write_row (png_ptr, row_pointer);
      pixels += rowstride;

      if (show_progress)
	gtk_progress_set_value(GTK_PROGRESS(progressbar),
			       ((float)i*100)/height);

      /* 
	 To update the progress bar and don't letting the GUI freeze
       */
      while (gtk_events_pending ())
	gtk_main_iteration ();
    }

  png_write_end (png_ptr, info_ptr);
  png_destroy_write_struct (&png_ptr, &info_ptr);

  if (show_progress)
    gtk_widget_destroy(window); /* Delete the window */

  g_free(dest);

  return TRUE;
}

/* Takes a RGB565 line and saves it as RGB24 */
gchar *
Convert_RGB565_RGB24 (int width, gchar* src, gchar* dest)
{
  gint16 * line = (gint16*) src;
  gchar * where_have_we_written = dest;
  int i;
  gint16 word_value;

  for (i = 0; i < width; i++)
    {
      word_value = *line;
      *(dest++) = (word_value & 0x1f) << 3;
      word_value >>= 5;
      *(dest++) = (word_value & 0x3f) << 2;
      word_value >>= 6;
      *(dest++) = (word_value) << 3;
      line ++;
    }
  return where_have_we_written;
}

/* Takes a RGB555 line and saves it as RGB24 */
gchar *
Convert_RGB555_RGB24 (int width, gchar* src, gchar* dest)
{
  gint16 * line = (gint16*) src;
  gchar * where_have_we_written = dest;
  int i;
  gint16 word_value;

  for (i = 0; i < width; i++)
    {
      word_value = *line;
      *(dest++) = (word_value & 0x1f) << 3;
      word_value >>= 5;
      *(dest++) = (word_value & 0x1f) << 3;
      word_value >>= 5;
      *(dest++) = (word_value & 0x1f) << 3;
      line ++;
    }
  return where_have_we_written;
}

/* Removes the last byte (suppoosedly alpha channel info) from the
   image */
gchar *
Convert_RGBA_RGB24 (int width, gchar* src, gchar* dest)
{
  gchar * where_have_we_written = dest;
  int i;

  for (i = 0; i < width; i++)
    {
      *(dest++) = *(src++);
      *(dest++) = *(src++);
      *(dest++) = *(src++);
      src++;
    }
  return where_have_we_written;
}

/*
  This is the function that will be called when the user hits the
  'Screenshot' button.
  mem: A pointer to the data to be saved
  info: Structure with info about the capture
  src_dir: The directory where the file will be stored
  prefix: The file prefix
  show_progress: TRUE if a progress bar should be shown while saving
*/
void
Save_PNG_shot(gchar * mem, tveng_device_info * info, gchar * src_dir,
	      gchar * prefix, gboolean show_progress)
{
  gchar * filename = NULL;
  gchar * error_message = NULL;
  int image_index = 1; /* Start by prefix1.png */
  FILE * handle = NULL;
  gchar buffer[256];
  buffer[255] = 0;

  g_assert(prefix != NULL);
  g_assert(src_dir != NULL);

  do
    {
      if (handle)
	fclose(handle);

      g_free(filename);
      g_snprintf(buffer, 255, "%d", image_index++);

      /* Add a slash if needed */
      if ((!*src_dir) || (src_dir[strlen(src_dir)-1] != '/'))
	filename = g_strconcat(src_dir, "/", prefix, buffer, ".png", NULL);
      else
	filename = g_strconcat(src_dir, prefix, buffer, ".png", NULL);

      /* Open this file  */
      handle = fopen(filename, "rb");
    } while (handle);

  /* Create this file */
  if (!(handle = fopen(filename, "wb")))
    {
      error_message = g_strconcat(_("Sorry, but I cannot write\n"),
				  filename, NULL);

      g_free(filename);

      ShowBox(error_message,
	      GNOME_MESSAGE_BOX_ERROR);

      g_free(error_message);

      return;
    }

  /* Build string for the progress bar title */
  g_snprintf(buffer, 255, "%s%d.png", prefix, image_index-1);
  
  if (!zapping_save_png(handle,
			buffer,
			show_progress,
			mem,
			info))
    ShowBox(_("Cannot take screenshot"),
	    GNOME_MESSAGE_BOX_ERROR);

  fclose(handle); /* Close the file */
  g_free(filename);
}

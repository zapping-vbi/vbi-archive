/* Header file for io.c (PNG file saving) */
#ifndef __IO_H__
#define __IO_H__

#include <png.h>
#include "tveng.h"

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
	      gchar * prefix, gboolean show_progress);

/* 
   Returns TRUE if the capture could be successfully completed.
   Args: 
      - handle: the handle we will write to
      - window_title: If show_progress = TRUE, give to the progress
                      window this title
      - show_progress: if TRUE, show a progress bar while saving
      - data: The data to be saved
      - info: The capture device struct 
*/
gboolean
zapping_save_png (FILE * handle,
		  gchar * window_title,
		  gboolean show_progress,
		  gchar * data,
		  tveng_device_info * info);
#endif

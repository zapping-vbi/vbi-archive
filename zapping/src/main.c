/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 Iñaki García Etxebarria
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>

#include "interface.h"
#include "support.h"
#include "tveng.h"
#include "v4l2interface.h"
#include "io.h"
#include "plugins.h"

extern gboolean flag_exit_program;
extern gboolean take_screenshot; /* Set to TRUE when they want an
				    screenshot */
extern int cur_tuned_channel;

tveng_device_info info; /* make this global, so all modules have
			   access to it */

/* Current configuration */
struct config_struct config;

/* plugin list, empty on startup */
GList * plugin_list = NULL;

/* This will be extern */
int zapping_window_x, zapping_window_y;
int zapping_window_width, zapping_window_height;
tveng_channels * current_country;

/*
  Configuration saving/loading functions. The configuration will be
  stored in a file called "$(HOME)/.zappingrc", in XML format.
*/

#define ZAPPING_CONFIG_FILE ".zappingrc"

/* Reads the configuration, FALSE on error */
gboolean ReadConfig(struct config_struct * config);

/* Save the configuration to the given file, FALSE on error */
gboolean SaveConfig(struct config_struct * config);

/* Saves the info regarding tuning */
void
SaveTuning(xmlNodePtr tree, struct config_struct * config);

/* Saves all the info regarding (main) window dimensions */
void
SaveWindowDimensions(xmlNodePtr tree, struct config_struct * config);

/* Saves the miscellaneous options */
void
SaveMiscOptions(xmlNodePtr tree, struct config_struct * config);

/* Saves all the info regarding video capture */
void
SaveVideoCapture(xmlNodePtr tree, struct config_struct * config);

/* Saves currently tuned channel info */
void
SaveTunedChannels(xmlNodePtr tree);

/* Saves PNG saving options info */
void
SavePNGOptions(xmlNodePtr tree, struct config_struct * config);

/* Saves plugin info */
void
SavePlugins(xmlNodePtr tree);

/* Saves the config document in XML format (could change, better call
   SaveConfig()) */
int
SaveConfigDoc (gchar * file_name, struct config_struct * config);

/* 
   This is the Parser. It is given the xmlDocPtr to parse,
   the xmlNodePtr to start from, and a ParseStruct to operate on
*/
void
Parser(xmlDocPtr config_doc,
       xmlNodePtr node, struct ParseStruct * parser);

/*
  Parses the given tree and fills in the given config structure. In
  case of error, returns FALSE, and config_struct fields are undefined.
*/
gboolean
ParseConfigDoc (xmlDocPtr config_doc, struct config_struct *
		config);

/* Parses window dimensions block, don't call this directly */
void
ParseWindowDimensions(xmlDocPtr config_doc, 
		      xmlNodePtr node, struct config_struct * config);

/* Parses misc options block */
void
ParseMiscOptions(xmlDocPtr config_doc, 
		 xmlNodePtr node, struct config_struct * config);

/* Parses video capture block */
void
ParseVideoCapture(xmlDocPtr config_doc, 
		      xmlNodePtr node, struct config_struct * config);

/* Parses the tuning info block  */
void
ParseTuning(xmlDocPtr config_doc, 
	    xmlNodePtr node, struct config_struct * config);

/* 
   This function doesn't require any config_struct parameter since it
   operates on tveng_insert_tuned_channel directly.
*/
void
ParseTunedChannels(xmlDocPtr config_doc,
		   xmlNodePtr node);

/* Parses the PNG options block */
void
ParsePNGOptions(xmlDocPtr config_doc, 
	    xmlNodePtr node, struct config_struct * config);

/* 
   This function doesn't require any config_struct parameter since it
   operates on plugin_list directly.
*/
void
ParsePlugins(xmlDocPtr config_doc,
	     xmlNodePtr node);

/*
  Fills in a config_struct with default values, just to have the
  struct working
*/
void
ParseDummyDoc (struct config_struct * config);

/*
  Open config file, and check for its validity, returns NULL on
   error, the XML document to parse otherwise. Name is the file name
   to open.
*/
xmlDocPtr
OpenConfigDoc (gchar * name);

int
main (int argc, char *argv[])
{
  GtkWidget *zapping;
  GtkWidget *da; /* drawing area */

  tveng_tuned_channel* last_tuned_channel;

  fd_set rdset;
  struct timeval timeout;
  int n;
  GList * p; /* For traversing the plugins */
  struct plugin_info * plug_info;

#ifndef NDEBUG
  int i;
#endif

  /* Load plugins */
  plugin_list = plugin_load_plugins("/home/garetxe/cvs/plugins",
				    ".zapping.so", plugin_list);

  /* Scan some more places for plugins */
  plugin_list = plugin_load_plugins("/usr/lib", 
				    ".zapping.so", plugin_list);

  plugin_list = plugin_load_plugins("/usr/local/lib", 
				    ".zapping.so", plugin_list);

  plugin_list = plugin_load_plugins("/lib", 
				    ".zapping.so", plugin_list);

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif

  gnome_init ("zapping", VERSION, argc, argv);

#ifndef NDEBUG  
  gdk_rgb_set_verbose (TRUE);
#endif
  
  gdk_rgb_init();
  
  gtk_widget_set_default_colormap (gdk_rgb_get_cmap());
  gtk_widget_set_default_visual (gdk_rgb_get_visual());

  info.num_desired_buffers = 8; /* Try to use 8 buffers by default */
  if (!ReadConfig(&config))
    ShowBox(_("Cannot read $HOME/.zappingrc, defaulting to standards..."),
	    GNOME_MESSAGE_BOX_ERROR);

 open_devices:; /* So we can retry later */
  if (tveng_init_device(config.video_device, O_RDONLY, &info) > 0)
    {
      /*
	We print useful info about this device if debugging is on
      */
#ifndef NDEBUG
      printf("Dimensions of %s: min=(%d, %d), Max=(%d,%d)\n"
	     "%s Teletext\n", 
	     info.caps.name,
	     info.caps.minwidth,
	     info.caps.minheight,
	     info.caps.maxwidth,
	     info.caps.maxheight,
	     (info.caps.flags & V4L2_FLAG_DATA_SERVICE) ? "Has" : "No"
	     );

      printf("%d input%s available\n",
	     info.num_inputs,
	     info.num_inputs == 1 ? "" : "s");

      for (i=0; i < info.num_inputs; i++)
	printf("  - %s is an %s with%s audio\n",
	       info.inputs[i].input.name,
	       (info.inputs[i].input.type == V4L2_INPUT_TYPE_TUNER) ?
	           "analog TV tuner" : "analog baseband input",
	       (info.inputs[i].input.capability & V4L2_INPUT_CAP_AUDIO) ? 
	           "" : "out");
#endif

      if (tveng_start_capturing(&info) == NULL)
	{
	  ShowBox(_("Sorry, cannot start capturing frames"),
		  GNOME_MESSAGE_BOX_ERROR);

	  plugin_unload_plugins(plugin_list);
	  return 0;
	}

      zapping = create_zapping ();

      da = lookup_widget(zapping,"tv_screen");

      /* Set minimum size */
      gtk_widget_set_usize(da, info.caps.minwidth, info.caps.minheight);

      gdk_window_set_back_pixmap((GdkWindow*)zapping, NULL, FALSE);

      gtk_widget_set_app_paintable(da, FALSE);
      gtk_widget_set_app_paintable(zapping, FALSE);

      if (tveng_update_standard(&info) == -1)
	printf("Update standard failed...\n");

      if (tveng_update_input(&info) == -1)
	printf("Update input failed...\n");

      /* Set the tune to the first country */
      current_country = tveng_get_country_tune_by_id(0);

      /* Apply configuration */
      tveng_set_standard(config.standard, &info);
      tveng_set_input_by_name(config.input, &info);

      current_country =
	tveng_get_country_tune_by_name(config.country);

      /* If the previous didn't work this works for sure */
      if (!current_country)
	current_country = tveng_get_country_tune_by_id(0);

      last_tuned_channel =
	tveng_retrieve_tuned_channel_by_name(config.tuned_channel, 0);

      if (last_tuned_channel)
	cur_tuned_channel = last_tuned_channel -> index;

      if (config.freq)
	tveng_tune_input(info.cur_input, config.freq,
			 &info);

      update_standards_menu(zapping, &info);

      /* Now that we have a GUI, add the plugins to it and init them
	 after that */
      p = g_list_first(plugin_list);
      while (p)
	{
	  plug_info = (struct plugin_info *) p -> data;
	  plugin_add_gui(zapping, plug_info);
	  plugin_init(&info, plug_info);
	  p = p -> next;
	}

      gtk_widget_show (zapping);

      info.interlaced = config.capture_interlaced;

      /* Set to nearest 4-multiplus */
      config.width = ((config.width+3) >> 2) << 2;

      gdk_window_move_resize(zapping -> window,
			     config.x, config.y, config.width, 
			     config.height);

      flag_exit_program = FALSE;

#ifndef NDEBUG
      if (info.pix_format.fmt.pix.flags & V4L2_FMT_FLAG_BYTESPERLINE)
	printf("Bytes per line field is valid\n");

      printf("Capture buffer is size %dx%dx%d\n", 
	     info.pix_format.fmt.pix.width, 
	     info.pix_format.fmt.pix.height,
	     info.bpl);

      printf("We have %d buffers\n", info.num_buffers);

#endif

      /* Start sound, avoid having to open the ToolBox */
      tveng_set_mute(0, &info);

      while (!flag_exit_program)
	{
	  while (gtk_events_pending())
	    gtk_main_iteration();

	  if (info.current_mode != TVENG_CAPTURE_MMAPED_BUFFERS)
	    continue;

	  if (flag_exit_program)
	    continue; /* Exit the loop if the exit flag has been
			 raised */

	  FD_ZERO(&rdset);
	  FD_SET(info.fd, &rdset);
	  timeout.tv_sec = 1;
	  timeout.tv_usec = 0;
	  n = select(info.fd + 1, &rdset, NULL, NULL, &timeout);
	  if (n == -1)
	    fprintf(stderr, _("select() error\n"));
	  else if (n == 0)
	    fprintf(stderr, _("select() timeout\n"));
	  else if (FD_ISSET(info.fd, &rdset))
	    {
	      /* We have data to be dequeued, dequeue it */
	      n = tveng_dqbuf(&info);
	      if (n != -1) /* No errors */
		{
		  /* Wait for loaded frames */
		  do{
		    FD_ZERO(&rdset);
		    FD_SET(info.fd, &rdset);
		    timeout.tv_sec = timeout.tv_usec = 0;
		    if (select(info.fd +1, &rdset, NULL, NULL,
			       &timeout) < 1)
		      break;
		    tveng_qbuf(n, &info);
		    n = tveng_dqbuf(&info);
		      } while (TRUE);

		  /* Copy the data to the format struct */
		  /* FIXME: This is utterly provisional, TVEng should
		     contain something like tveng_read_frame(&info),
		     but i've been so many hours without sleeping
		     now... */
		  memcpy(info.format.data, info.buffers[n].vmem,
			 info.format.bytesperline * info.format.height);

		  /* Feed all the plugins with this frame */
		  p = g_list_first(plugin_list);
		  while (p)
		    {
		      plugin_eat_frame(&info.format, 
				       (struct plugin_info *) p->data);
		      p = p->next;
		    }

		  switch (info.pix_format.fmt.pix.pixelformat)
		    {
		    case V4L2_PIX_FMT_BGR32:
		      info.ximage -> data = info.format.data;
		      gdk_draw_image(da -> window,
				     da -> style -> white_gc,
				     info.image,
				     0, 0, 0, 0,
				     info.ppl,
				     info.pix_format.fmt.pix.height);
		      break;

		    case V4L2_PIX_FMT_BGR24:
		      info.ximage -> data = info.format.data;
		      gdk_draw_image(da -> window,
				     da -> style -> white_gc,
				     info.image,
				     0, 0, 0, 0,
				     info.ppl,
				     info.pix_format.fmt.pix.height);
		      break;

		    case V4L2_PIX_FMT_RGB32:
		      gdk_draw_rgb_32_image(da -> window,
					    da -> style -> white_gc,
					    0, 0,
					    info.ppl,
					    info.pix_format.fmt.pix.height,
					    GDK_RGB_DITHER_MAX,
					    info.format.data,
					    info.bpl);
		      break;
		    case V4L2_PIX_FMT_RGB24:
		      gdk_draw_rgb_image(da -> window,
					 da -> style -> white_gc,
					 0, 0, 
					 info.ppl,
					 info.pix_format.fmt.pix.height,
					 GDK_RGB_DITHER_MAX,
					 info.format.data,
					 info.bpl);
					 break;
		      break;
		    case V4L2_PIX_FMT_RGB565:
		      info.ximage -> data = info.format.data;

		      gdk_draw_image(da -> window,
				     da -> style -> white_gc,
				     info.image,
				     0, 0, 0, 0,
				     info.ppl,
				     info.pix_format.fmt.pix.height);
		      break;
		    case V4L2_PIX_FMT_RGB555:
		      info.ximage -> data = info.format.data;
		      
		      gdk_draw_image(da -> window,
				     da -> style -> white_gc,
				     info.image,
				     0,0,0,0,
				     info.ppl,
				     info.pix_format.fmt.pix.height);
		      break;
		    default:
#ifndef NDEBUG
		      fprintf(stderr,"SWITCH ERROR !!!\n");
#endif
		      break;
		    }

		  /* Take the screenshot of the current image */
		  if (take_screenshot)
		    {
		      Save_PNG_shot(info.buffers[n].vmem,
				    &info,
				    config.png_src_dir,
				    config.png_prefix,
				    config.png_show_progress);

		      /* Clear flag */
		      take_screenshot = FALSE;
		    }
		  /* and queue  the buffer again*/
		  tveng_qbuf(n, &info);
		}
	      else
		perror("tveng_dqbuf");
	    }
	  /* The channels have been updated by the channel editor,
	     show the changes */
	  if (channels_updated)
	    {
	      channels_updated = FALSE; /* We are done updating the
					   channels */
	      update_channels_menu(zapping, &info);
	    }
	}
      /* Stop current capture */
      switch (info.current_mode)
	{
	case TVENG_CAPTURE_MMAPED_BUFFERS:
	  tveng_stop_capturing(&info);
	  break;
	case TVENG_CAPTURE_FULLSCREEN:
	  tveng_stop_fullscreen_previewing(&info);
	  break;
	default:
	  break;
	};

      /* Fill in the config struct */
      config.x = zapping_window_x;
      config.y = zapping_window_y;
      config.width = zapping_window_width;
      config.height = zapping_window_height;
      config.capture_interlaced = info.interlaced;
      
      if (current_country)
	g_snprintf(config.country, 32, current_country->name);

      if (cur_tuned_channel < tveng_tuned_channel_num())
      g_snprintf(config.tuned_channel, 32,
		 tveng_retrieve_tuned_channel_by_index(cur_tuned_channel) 
		 -> name);

      /* Get the current tuning values */
      if (tveng_get_tune(&(config.freq), &info) == -1)
	config.freq = 0; /* Don't store anything */

      tveng_update_input(&(info));
      tveng_update_standard(&(info));

      g_snprintf(config.input, 32, info.inputs[info.cur_input].input.name);
      g_snprintf(config.standard, 32, info.cur_standard.name);

      /* Save current configuration */
      if (!SaveConfig(&config))
	printf(_("There was some error writing the configuration\n"));

      /* Close the plugins here */
      p = g_list_first(plugin_list);
      while (p)
	{
	  plug_info = (struct plugin_info *) p -> data;
	  plugin_close(plug_info);
	  p = p->next;
	}

      tveng_close_device(&info);
    }
  else
    {
      if (!strcasecmp(config.video_device, "/dev/video"))
	ShowBox(_("Sorry, dude. No v4l2 for you today...\n"
		  "(v4l2 must be correctly installed before running this program)"
		  ),
		GNOME_MESSAGE_BOX_ERROR);
      else /* Other device selected, try with /dev/video */
	{
	  GtkWidget * question_box = gnome_message_box_new(
               _("Should I try \"/dev/video\"?"),
	       GNOME_MESSAGE_BOX_QUESTION,
	       GNOME_STOCK_BUTTON_YES,
	       GNOME_STOCK_BUTTON_NO,
	       NULL);
	 
	  switch (gnome_dialog_run(GNOME_DIALOG(question_box)))
	    {
	    case 0: /* Retry */
	      sprintf(config.video_device, "/dev/video");
	      goto open_devices;
	    default:
	      break; /* Don't do anything */
	    }
	}
    }

  plugin_unload_plugins(plugin_list);
  
  return 0;
}

/*
  Configuration saving/loading functions. The configuration will be
  stored in a file called "$(HOME)/.zappingrc", in XML format.
*/

/* Reads the configuration, FALSE on error */
gboolean ReadConfig(struct config_struct * config)
{
  gchar * buffer;
  gchar * home;
  xmlDocPtr config_doc;

  home = getenv("HOME");

  if (home[strlen(home)-1] == '/')
    buffer = g_strconcat(home, ZAPPING_CONFIG_FILE, NULL);
  else
    buffer = g_strconcat(home, "/", ZAPPING_CONFIG_FILE, NULL);

  if (!buffer)
    return FALSE;

  config_doc = OpenConfigDoc(buffer);

  g_free(buffer);

  /* No config, open defaults */
  if (!config_doc)
    {
#ifndef NDEBUG
      printf("Defaulting to standard values...\n");
#endif
      ParseDummyDoc(config);
      return TRUE;
    }

  return (ParseConfigDoc(config_doc, config));
}

/* Saves configuration to the given file. FALSE on error */
gboolean SaveConfig(struct config_struct * config)
{
  gchar * buffer;
  gchar * home;
  int returned_value;

  home = getenv("HOME");

  if (home[strlen(home)-1] == '/')
    buffer = g_strconcat(home, ZAPPING_CONFIG_FILE, NULL);
  else
    buffer = g_strconcat(home, "/", ZAPPING_CONFIG_FILE, NULL);

  if (!buffer)
    return FALSE;

  returned_value = SaveConfigDoc(buffer, config);

  g_free(buffer);

  if (returned_value < 1) /* SaveConfigDoc returns the number of items
			   written */
    return FALSE;

  return TRUE;
}

/*
  Open config file, and check for its validity, returns NULL on
   error, the XML document to parse otherwise. Name is the file name
   to open.
*/
xmlDocPtr
OpenConfigDoc (gchar * name)
{
 xmlDocPtr config_doc;

 config_doc = xmlParseFile(name);
 
 if (!name)
    return NULL;

 return (config_doc);
}

/*
  Fills in a config_struct with default values, just to have the
  struct working
*/
void
ParseDummyDoc (struct config_struct * config)
{
  /* We have here more than one country for sure (at least for 0.2
     release) */
  config -> country[31] = 0;

  g_snprintf(config->country, 31, tveng_get_country_tune_by_id (0) ->
	     name);

  config->tuned_channel[0] = 0; /* No tuned channel stored */
  config->input[0] = 0; /* No default input */
  config->standard[0] = 0; /* No default standard */

  /* Config should have been called after initing V4L2 */
  config -> width = 200; /* Just something to start with */
  config -> height = 320; /* Just something to start with */
  config -> x = 50;
  config -> y = 50;
  config -> freq = 0; /* No freq code stored */

  /* PNG entries */
  config -> png_src_dir[PATH_MAX-1] = 0;
  g_snprintf(config -> png_src_dir, PATH_MAX-1, getenv("HOME"));
  config -> png_prefix[31] = 0;
  g_snprintf(config -> png_prefix, 31, "shot");
  config -> png_show_progress = TRUE; /* Show GUI item by default */

  config -> capture_interlaced = TRUE; /* Interlaced capture by default */
  g_snprintf(config -> video_device, FILENAME_MAX-1, "/dev/video");

  config -> zapping_setup_fb_verbosity = 0; /* Quiet by default */
  config -> avoid_noise = 1; /* Avoid noises by default */
}

/* 
   This is the Parser. It is given the xmlDocPtr to parse,
   the xmlNodePtr to start from, and a ParseStruct to operate on
*/
void
Parser(xmlDocPtr config_doc,
       xmlNodePtr node, struct ParseStruct * parser)
{
  gchar * read_string;
  int parser_index;

  while (node)
    {
      /* Check which entry in the parser are we going to use (if any)
       */
      for (parser_index = 0; parser[parser_index].name; parser_index++)
	{
	  if (!strcasecmp(node->name, parser[parser_index].name))
	    break; /* We have found an entry */
	}
      
      if (parser[parser_index].name == NULL)
	{
	  node = node->next;
	  continue;
	}
      
      read_string = xmlNodeListGetString(config_doc, node->childs,
					 1);
      
      node = node -> next;
      
      if (!read_string) /* We can get a NULL string here, avoid
			   segfault */
	continue;

      if (parser[parser_index].max_length < 1)
	sscanf(read_string, parser[parser_index].format,
	       parser[parser_index].where);
      else /*  We are getting a string */
	{
	  ((gchar*) parser[parser_index].where)
	    [parser[parser_index].max_length - 1] = 0;

	  g_snprintf((gchar*) parser[parser_index].where,
		     parser[parser_index].max_length - 1,
		     read_string);
	}
    }
}

/* Parses window dimensions block */
void
ParseWindowDimensions(xmlDocPtr config_doc, 
		      xmlNodePtr node, struct config_struct * config)
{
  struct ParseStruct parser[] =
  {
    {"XPos", "%d", (gpointer) &(config->x), 0},
    {"YPos", "%d", (gpointer) &(config->y), 0},
    {"Width", "%d", (gpointer) &(config->width), 0},
    {"Height", "%d", (gpointer) &(config->height), 0},
    {NULL, NULL, NULL, 0} /* End-of-struct */
  };

  Parser(config_doc, node, parser);
}

/* Parses misc options block */
void
ParseMiscOptions(xmlDocPtr config_doc, 
		 xmlNodePtr node, struct config_struct * config)
{
  struct ParseStruct parser[] =
  {
    {"ZappingSetupFbVerbosity", "%d", (gpointer)
     &(config->zapping_setup_fb_verbosity), 0},
    {"AvoidNoise", "%d", (gpointer) &(config->avoid_noise), 0},
    {NULL, NULL, NULL, 0} /* End-of-struct */
  };

  Parser(config_doc, node, parser);
}

/* Parses video capture info */
void
ParseVideoCapture(xmlDocPtr config_doc, 
		      xmlNodePtr node, struct config_struct * config)
{
  struct ParseStruct parser[] =
  {
    {"Interlaced", "%d", (gpointer) &(config->capture_interlaced), 0},
    {"VideoDevice", "%s", (gpointer) &(config->video_device),
     FILENAME_MAX},
    {"NumDesiredBuffers", "%d", (gpointer)
     &(info.num_desired_buffers), 0},
    {NULL, NULL, NULL, 0} /* End-of-struct */
  };

  Parser(config_doc, node, parser);
}

/* Parses the tuning info block  */
void
ParseTuning(xmlDocPtr config_doc, 
	    xmlNodePtr node, struct config_struct * config)
{
  struct ParseStruct parser[] =
  {
    {"Country", "%s", (gpointer) &(config->country), 32},
    {"Channel", "%s", (gpointer) &(config->tuned_channel), 32},
    {"Input", "%s", (gpointer) &(config->input), 32},
    {"Standard", "%s", (gpointer) &(config->standard), 32},
    {"Frequence", "%u", (gpointer) &(config->freq), 0},
    {NULL, NULL, NULL} /* End-of-struct */
  };

  Parser(config_doc, node, parser);
}

/* Parses the PNG options block */
void
ParsePNGOptions(xmlDocPtr config_doc, 
	    xmlNodePtr node, struct config_struct * config)
{
  struct ParseStruct parser[] =
  {
    {"SrcDir", "%s", (gpointer) &(config->png_src_dir), PATH_MAX},
    {"Prefix", "%s", (gpointer) &(config->png_prefix), 32},
    {"ShowProgress", "%d", (gpointer) &(config->png_show_progress), 0},
    {NULL, NULL, NULL} /* End-of-struct */
  };

  Parser(config_doc, node, parser);
}

/* 
   This function doesn't require any config_struct parameter since it
   operates on tveng_insert_tuned_channel directly.
*/
void
ParseTunedChannels(xmlDocPtr config_doc,
		   xmlNodePtr node)
{
  /* This function doesn't use the Parser facilities since its
     structure is slightly different (although it's mostly
     cut'n'paste from Parser) */
  gchar * read_string; /* frequence as a string */
  const xmlChar * property; /* Property value holding the "real name" */
  const xmlChar * name; /* Given name to the channel */
  __u32 freq; /* Frequence of the tunned channel */
  tveng_tuned_channel channel_added; /* Channel we are adding */

  while (node)
    {
      read_string = xmlNodeListGetString(config_doc, node->childs,
					 1);

      /* get name Property */
      name = xmlGetProp(node, "Name");

      /* get Real name property */
      property = xmlGetProp(node, "RName");

      node = node -> next;      
      
      if (!read_string) /* We can get a NULL string here, avoid
			   segfault */
	continue;

      if (sscanf(read_string, "%u", &freq) < 1)
	continue;

      if (!name)
	continue;

      /* Copy the relevant members of the struct */
      channel_added.name = (gchar*) name;
      channel_added.real_name = (gchar*) property;
      channel_added.freq = freq;

      tveng_insert_tuned_channel(&channel_added);
    }  
}

/* 
   This function doesn't require any config_struct parameter since it
   operates on plugin_list directly.
*/
void
ParsePlugins(xmlDocPtr config_doc,
	     xmlNodePtr node)
{
  GList *p;
  struct plugin_info * pi;
  const xmlChar * property;

  while (node) /* Iterate through all the tree */
    {
      /* Only use plugin entries for the moment */
      if (!strcasecmp(node -> name, "plugin"))
	{
	  property = xmlGetProp(node, "name");
	  if (!property) /* avoid possible segfault */
	    {
	      node = node->next;
	      continue;
	    }

	  /* Look which plugin we are referring to */
	  p = g_list_first(plugin_list);
	  while (p)
	    {
	      pi = (struct plugin_info*) p -> data;
	      if (!strcmp(plugin_get_canonical_name(pi),
			  (gchar*)property))
		{
		  /* We found it, invoque the parser */
		  Parser(config_doc, node->childs, pi->parse_struct);
		  break; /* while (p) */
		}
	      p = p -> next;
	    }
	}
      node = node -> next;
    }
}

/*
  Parses the given tree and fills in the given config structure. In
  case of error, returns FALSE, and config_struct fields are undefined.
*/
gboolean
ParseConfigDoc (xmlDocPtr config_doc, struct config_struct *
		config)
{
  xmlNodePtr node = config_doc -> root -> childs;

  if (!config)
    return FALSE;

  /* Fill in with defaults */
  ParseDummyDoc(config);

  /* Start parsing */
  while (node)
    {
      if (!strcasecmp(node -> name, "Window_Dimensions"))
	ParseWindowDimensions(config_doc, node -> childs, config);

      if (!strcasecmp(node -> name, "Misc_Options"))
	ParseMiscOptions(config_doc, node -> childs, config);

      else if (!strcasecmp(node -> name, "Video_Capture"))
	ParseVideoCapture(config_doc, node -> childs, config);

      else if (!strcasecmp(node -> name, "Tuning"))
	ParseTuning(config_doc, node -> childs, config);
      
      else if (!strcasecmp(node -> name, "Tuned_Channels"))
	ParseTunedChannels(config_doc, node->childs);

      else if (!strcasecmp(node -> name, "PNG_Options"))
	ParsePNGOptions(config_doc, node->childs, config);

      else if (!strcasecmp(node -> name, "Plugins"))
	ParsePlugins(config_doc, node->childs);

      node = node -> next;
    };
  
  return TRUE;
}

/* Saves all the info regarding (main) window dimensions */
void
SaveWindowDimensions(xmlNodePtr tree, struct config_struct * config)
{
  gchar buffer[256];
  buffer[255] = 0;

  g_snprintf(buffer, 255, "%d", config->x);
  xmlNewChild(tree, NULL, "XPos", buffer);
  
  g_snprintf(buffer, 255, "%d", config->y);
  xmlNewChild(tree, NULL, "YPos", buffer);

  g_snprintf(buffer, 255, "%d", config->width);
  xmlNewChild(tree, NULL, "Width", buffer);
  
  g_snprintf(buffer, 255, "%d", config->height);
  xmlNewChild(tree, NULL, "Height", buffer);
}

/* Saves the miscellaneous options */
void
SaveMiscOptions(xmlNodePtr tree, struct config_struct * config)
{
  gchar buffer[256];
  buffer[255] = 0;

  g_snprintf(buffer, 255, "%d", config->zapping_setup_fb_verbosity);
  xmlNewChild(tree, NULL, "ZappingSetupFbVerbosity", buffer);

  g_snprintf(buffer, 255, "%d", config->avoid_noise);
  xmlNewChild(tree, NULL, "AvoidNoise", buffer);
}

/* Saves all the info regarding video capture */
void
SaveVideoCapture(xmlNodePtr tree, struct config_struct * config)
{
  gchar buffer[FILENAME_MAX];
  buffer[FILENAME_MAX-1] = 0;

  g_snprintf(buffer, FILENAME_MAX-1, "%d", config -> capture_interlaced);
  xmlNewChild(tree, NULL, "Interlaced", buffer);

  g_snprintf(buffer, FILENAME_MAX-1, "%d", info.num_desired_buffers);
  xmlNewChild(tree, NULL, "NumDesiredBuffers", buffer);

  g_snprintf(buffer, FILENAME_MAX-1, "%s", config -> video_device);
  xmlNewChild(tree, NULL, "VideoDevice", buffer);
}

/* Saves the info regarding tuning */
void
SaveTuning(xmlNodePtr tree, struct config_struct * config)
{
  gchar buffer[256];
  buffer[255] = 0;

  g_snprintf(buffer, 255, "%s", config -> country);
  xmlNewChild(tree, NULL, "Country", buffer);

  g_snprintf(buffer, 255, "%s", config -> tuned_channel);
  xmlNewChild(tree, NULL, "Channel", buffer);

  g_snprintf(buffer, 255, "%s", config -> input);
  xmlNewChild(tree, NULL, "Input", buffer);

  g_snprintf(buffer, 255, "%s", config -> standard);
  xmlNewChild(tree, NULL, "Standard", buffer);

  /* Currently tuned frequence */
  g_snprintf(buffer, 255, "%u", config -> freq);
  xmlNewChild(tree, NULL, "Frequence", buffer);
}

/* Saves currently tuned channel info */
void
SaveTunedChannels(xmlNodePtr tree)
{
  int i = 0;
  tveng_tuned_channel * tune;
  xmlNodePtr new_node;
  gchar buffer[256];
  buffer[255] = 0;

  while ((tune = tveng_retrieve_tuned_channel_by_index(i++)))
	 {
	   g_snprintf(buffer, 255, "%u", tune -> freq);

	   /* The name for this node is irrelevant */
	   new_node = xmlNewChild(tree, NULL, "Channel", 
				  buffer);

	   /* Set real name Property */
	   xmlSetProp(new_node, (const xmlChar*) "RName", 
		      (const xmlChar*) tune -> real_name);

	   /* Set name Property */
	   xmlSetProp(new_node, (const xmlChar*) "Name",
		      (const xmlChar*) tune -> name);
	 }
}

/* Saves PNG saving options info */
void
SavePNGOptions(xmlNodePtr tree, struct config_struct * config)
{
  gchar buffer[256];
  buffer[255] = 0;

  g_snprintf(buffer, 255, "%s", config -> png_src_dir);
  xmlNewChild(tree, NULL, "SrcDir", buffer);

  g_snprintf(buffer, 255, "%s", config -> png_prefix);
  xmlNewChild(tree, NULL, "Prefix", buffer);

  g_snprintf(buffer, 255, "%d", config -> png_show_progress);
  xmlNewChild(tree, NULL, "ShowProgress", buffer);
}

/* Saves plugins info */
void
SavePlugins(xmlNodePtr parent)
{
  GList * p = g_list_first(plugin_list);
  struct plugin_info * pi;
  xmlNodePtr tree;
  int i;
  struct ParseStruct * ps;
  gchar buffer[256];
  buffer[255] = 0;

  while (p)
    {
      pi = (struct plugin_info*) p -> data;
      tree = xmlNewChild(parent, NULL, "plugin", NULL);
      xmlSetProp(tree, "name", plugin_get_canonical_name(pi));
      ps = pi -> parse_struct;
      for (i=0; ps[i].name != NULL; i++)
	{
	  if (ps[i].max_length == 0)
	    {
	      g_snprintf(buffer, 255, ps[i].format, *(ps[i].where));
	      xmlNewChild(tree, NULL, ps[i].name, buffer);
	    }
	  else
	    xmlNewChild(tree, NULL, ps[i].name, (gchar*) ps[i].where);
	}
      p = p->next;
    }
}

/* 
   Saves the config document in XML format (could change, better call
   SaveConfig()) 
*/
int
SaveConfigDoc (gchar * file_name, struct config_struct * config)
{
  xmlDocPtr doc;
  xmlNodePtr tree;
  gchar buffer[256];
  buffer[255] = 0;

  doc = xmlNewDoc("1.0");
  doc -> root = xmlNewDocNode(doc, NULL, "Configuration",
   _("\nThis is the configuration file for Zapping, the TV viewer.\n"
     "Feel free to modify this file as you want (standard XML, please).\n"
     "Any changes made to this file will be loaded the next time Zapping\n"
     "is started, and silently overwritten when closed.\n"
     "If you are going to manually modify this file, please keep a backup\n"
     "file, since in case of parse error, Zapping will load the defaults.\n"));

  tree = xmlNewChild(doc->root, NULL, "Window_Dimensions",
		     _("\nInfo regarding main window dimensions\n"));

  SaveWindowDimensions(tree, config);

  tree = xmlNewChild(doc->root, NULL, "Misc_Options", NULL);

  SaveMiscOptions(tree, config);

  tree = xmlNewChild(doc->root, NULL, "Video_Capture",
		     _("\nInfo about capture parameters\n"));

  SaveVideoCapture(tree, config);

  tree = xmlNewChild(doc->root, NULL, "Tuning",
		     _("\nInfo regarding current tuners\n"));

  SaveTuning(tree, config);

  tree = xmlNewChild(doc->root, NULL, "Tuned_Channels",
		     _("\nThese are the pretuned channels\n"));
  
  SaveTunedChannels(tree);

  tree = xmlNewChild(doc->root, NULL, "PNG_Options",
		     _("\nOptions for saving PNG screenshots\n"));
  
  SavePNGOptions(tree, config);

  /* Save the plugins */
  tree = xmlNewChild(doc->root, NULL, "Plugins",
		     _("\nAny options the plugins want to store is here\n"));
  SavePlugins(tree);
  
  return (xmlSaveFile(file_name, doc));
}

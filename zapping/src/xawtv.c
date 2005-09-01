/*
 * Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2003 Michael H. Schimek
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

/* $Id: xawtv.c,v 1.13 2005-09-01 01:28:09 mschimek Exp $ */

/*
   XawTV compatibility functions:
   * Import XawTV configuration - currently channels only
   * XawTV IPC (w/nxtvepg etc) - more to do
 */

#include "site_def.h"

#include <glib.h>
#include <gnome.h>
#include <fcntl.h>
#include <string.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>		/* XA_STRING */

#include "xawtv.h"
#include "zmisc.h"
#include "remote.h"
#include "osd.h"
#include "globals.h"
#include "v4linterface.h"

#ifndef XAWTV_CONFIG_TEST
#define XAWTV_CONFIG_TEST 0
#endif

static gboolean
get_value			(FILE *			fp,
				 char			buffer[300],
				 char **		ident,
				 char **		value)
{
  int c;
  char *s;
  unsigned int len;

  do
    c = fgetc (fp);
  while (' ' == c || '\t' == c);

  if (EOF == c || ferror (fp))
    return FALSE;

  ungetc (c, fp);

  if (c < 'a' || c > 'z')
    return FALSE;

  fgets (buffer, 300, fp);

  if (feof (fp) || ferror (fp))
    return FALSE;

  s = buffer;

  while (' ' == *s || '\t' == *s)
    ++s;

  *ident = s;

  len = 0;

  while ('-' == (c = s[len])
	 || (c >= '0' && c <= '9')
	 || (c >= 'a' && c <= 'z'))
    ++len;

  s += len;

  while (' ' == *s || '\t' == *s)
    ++s;

  if (0 == len || '=' != *s)
    return FALSE;

  (*ident)[len] = 0;
  ++s;

  while (' ' == *s || '\t' == *s)
    ++s;

  len = strlen (s);
  while (len > 0 && s[len - 1] <= ' ')
    --len;

  *value = s;
  s[len] = 0;

  if (XAWTV_CONFIG_TEST)
    fprintf (stderr, "<%s>=<%s>\n", *ident, *value);

  return TRUE;
}

static gboolean
skip_section			(FILE *			fp)
{
  gboolean newline;
  int c;

  newline = TRUE;

  while (EOF != (c = fgetc (fp)))
    {
      if (ferror (fp))
	return FALSE;

      switch (c)
	{
	case '\n':
	  newline = TRUE;
	  break;

	case ' ':
	case '\t':
	  break;

	case '[':
	  if (newline)
	    return (c == ungetc (c, fp));
	  break;

	default:
	  newline = FALSE;
	  break;
	}
    }

  return TRUE;
}

static gboolean
global_section			(FILE *			fp,
				 tv_rf_channel *	rf_ch)
{
  char buffer[300];
  char *ident;
  char *value;
  gchar *freqtab;
  gboolean r;

  freqtab = NULL;
  r = FALSE;

  while (get_value (fp, buffer, &ident, &value))
    {
      if (0 == value[0])
	continue;

      /* mixer = <device_file>:<line_number> */
      /* fullscreen = <width> x <height> */
      /* pixsize = <width> x <height> */
      /* pixcols = <int> */
      /* wm-off-by = <xoff> <yoff> */
      /* ratio = <grabx> <graby> */
      /* jpeg-quality = <int> */
      /* mjpeg-quality = <int> */
      /* keypad-ntsc = <bool (0|1|yes|no)> */
      /* keypad-partial = <bool> */
      /* keypad-osd = <bool> */
      /* mov-driver = <string> */
      /* mov-video = <string> */
      /* mov-fps = <string> */
      /* mov-audio = <string> */
      /* mov-rate = <string> */
      /* osd = <bool> */
      /* osd-position = <x> , <y> */
      /* use-wm-fullscreen = <bool> */

      if (0 == strcmp (ident, "freqtab"))
	{
	  g_free (freqtab);
	  freqtab = g_strdup (value);
	}
      else
	{
	  if (XAWTV_CONFIG_TEST)
	    fprintf (stderr, "Unknown %s\n", ident);
	}
    }

  if (rf_ch && freqtab)
    r = tv_rf_channel_table_by_name (rf_ch, freqtab);

  g_free (freqtab);

  return r;
}

static gboolean
set_control			(tveng_device_info *	info,
				 tveng_tuned_channel *	ch,
				 tv_control_id		id,
				 const gchar *		value)
{
  tv_control *c;
  gdouble d;

  d = strtod (value, NULL) / 100.0;
  d = SATURATE (d, 0.0, 1.0);

  c = tv_control_by_id (info, id);

  if (!c)
    return TRUE; /* not supported by device */

  return tveng_tuned_channel_set_control (ch, c->label, d);
}

static gboolean
channel_section			(FILE *			fp,
				 tveng_device_info *	info,
				 tv_rf_channel *	rf_ch,
				 tveng_tuned_channel *	tt_ch,
				 gboolean *		have_channel)
{
  char buffer[300];
  char *ident;
  char *value;
  gint fine_tuning;

  tt_ch->frequ = 0; /* Hz */
  fine_tuning = 0; /* Hz */

  *have_channel = FALSE;

  while (get_value (fp, buffer, &ident, &value))
    {
      if (0 == value[0])
	continue;

      /* capture = on | off | overlay | grabdisplay */
      /* audio = <int> */
      /* group = <string> -? */

      if (0 == strcmp (ident, "input")
	  || 0 == strcmp (ident, "source"))
	{
	  const tv_video_line *l;

	  for (l = NULL; (l = tv_next_video_input (info, l));)
	    if (0 == g_ascii_strcasecmp (l->label, value))
	      break;

	  if (l)
	    {
	      tt_ch->input = l->hash;

	      if (TV_VIDEO_LINE_TYPE_BASEBAND == l->type)
		*have_channel = TRUE;
	    }
	}
      else if (0 == strcmp (ident, "norm"))
	{
	  const tv_video_standard *s;

	  for (s = NULL; (s = tv_next_video_standard (info, s));)
	    if (0 == g_ascii_strcasecmp (s->label, value))
	      break;
	  if (s)
	    tt_ch->standard = s->hash;
	}
      else if (0 == strcmp (ident, "channel"))
	{
	  if (tv_rf_channel_by_name (rf_ch, value))
	    {
	      if (0 == tt_ch->frequ)
		tt_ch->frequ = rf_ch->frequency;

	      *have_channel = TRUE;      
	    }
	}
      else if (0 == strcmp (ident, "freq"))
	{
	  double d;

	  d = strtod (value, NULL);
	  if (d > 100 && d < 1000)
	    {
	      guint frequ;

	      frequ = (guint)(d * 1e6); /* -> Hz */

	      tt_ch->frequ = frequ;

	      if (!*have_channel)
		if (!tv_rf_channel_by_frequency (rf_ch, frequ))
		  tv_rf_channel_first (rf_ch);

	      *have_channel = TRUE;
	    }
	}
      /* FIXME "freq" ? */
      else if (0 == strcmp (ident, "freq"))
	{
	  fine_tuning = strtol (value, NULL, 0);
	  if (fine_tuning < -128 || fine_tuning > 127)
	    fine_tuning = 0;
	  else
	    fine_tuning *= 62500; /* -> Hz */
	}
      else if (0 == strcmp (ident, "key"))
	{
	  /* XXX Xt application resource translation, like
	     "qual<Key>key: command" (try apropos resource), stored here
	     as "key" or "qual+key". How can we properly translate? */
	}
      /* color = n % */
      else if (0 == strcmp (ident, "color"))
	{
	  set_control (info, tt_ch, TV_CONTROL_ID_SATURATION, value);
	}
      else if (0 == strcmp (ident, "bright"))
	{
	  set_control (info, tt_ch, TV_CONTROL_ID_BRIGHTNESS, value);
	}
      else if (0 == strcmp (ident, "hue"))
	{
	  set_control (info, tt_ch, TV_CONTROL_ID_HUE, value);
	}
      else if (0 == strcmp (ident, "contrast"))
	{
	  set_control (info, tt_ch, TV_CONTROL_ID_CONTRAST, value);
	}
      else
	{
	  if (XAWTV_CONFIG_TEST)
	    fprintf (stderr, "Unknown %s\n", ident);
	}
    }

  tt_ch->frequ += fine_tuning;

  return TRUE;
}

static gboolean
section				(FILE *			fp,
				 const tveng_device_info *info,
				 tveng_tuned_channel **	channel_list,
				 guint *		pass,
				 tv_rf_channel *	rf_channel,
				 tveng_tuned_channel *	default_channel,
				 const gchar *		identifier)
{
  if (XAWTV_CONFIG_TEST)
    fprintf (stderr, "Section <%s>\n", identifier);

  if (0 == strcmp (identifier, "global"))
    {
      if (0 == *pass)
	{
	  *pass = 1;

	  /* rf_channel = frequency table */
	  if (!global_section (fp, rf_channel))
	    return FALSE;
	}
      else
	{
	  skip_section (fp);
	}
    }
  else if (0 == strcmp (identifier, "launch"))
    {
      /* Key bindings */
      skip_section (fp);
    }
  else if (0 == strcmp (identifier, "defaults"))
    {
      /* Default channel attributes */

      if (1 == *pass)
	{
	  gboolean dummy;

	  *pass = 2;

	  if (!channel_section (fp, info, rf_channel, default_channel, &dummy))
	    return FALSE;
	}
      else
	{
	  skip_section (fp);
	}
    }
  else /* channel */
    {
      gchar *t;

      if (!(t = g_locale_to_utf8 (identifier, -1, NULL, NULL, NULL)))
	return FALSE;

      if (2 == *pass)
	{
	  tveng_tuned_channel channel;
	  tveng_tuned_channel *ch;
	  gboolean have_channel;

	  channel = *default_channel;

	  channel.controls =
	    g_memdup (default_channel->controls,
		      default_channel->num_controls
		      * sizeof (*default_channel->controls));

	  if (!channel_section (fp, info, rf_channel, &channel, &have_channel)
	      || !have_channel)
	    {
	      g_free (channel.controls);
	      g_free (t);
	      return FALSE;
	    }

	  channel.name = (gchar *) t;

	  channel.rf_table = (gchar *) rf_channel->table_name;
	  channel.rf_name = rf_channel->channel_name;

	  if ((ch = tveng_tuned_channel_by_name (*channel_list, t)))
	    {
	      tveng_tuned_channel_copy (ch, &channel);
	    }
	  else
	    {
	      ch = tveng_tuned_channel_new (&channel);
	      tveng_tuned_channel_insert (channel_list, ch, G_MAXINT);
	    }

	  g_free (channel.controls);
	}
      else
	{
	  skip_section (fp);
	}

      g_free (t);
    }

  return TRUE;
}

gboolean
xawtv_config_present		(void)
{
  gchar *filename;
  int fd;

  filename = g_strconcat (g_get_home_dir (), "/.xawtv", NULL);

  fd = open (filename, O_RDONLY);

  g_free (filename);

  if (-1 != fd)
    close (fd);

  return (-1 != fd);
}

gboolean
xawtv_import_config		(const tveng_device_info *info,
				 tveng_tuned_channel **	channel_list)
{
  guint pass;
  FILE *fp;
  tv_rf_channel rf_channel;
  tveng_tuned_channel default_channel;

  fp = NULL;
  CLEAR (default_channel);

  /* Usually the file will contain the information we need in
     proper order. If not we run a second and third pass. */
  for (pass = 0; pass < 3; ++pass)
    {
      gchar *filename;
      int c;

      filename = g_strconcat (g_get_home_dir (), "/.xawtv", NULL);

      if (!(fp = fopen (filename, "r")))
	{
	  g_free (filename);
	  return FALSE; /* XXX error message please */
	}

      g_free (filename);

      while (EOF != (c = fgetc (fp)))
	{
	  if (ferror (fp))
	    goto failure;

	  switch (c)
	    {
	      char buffer[300];
	      unsigned int len;

	    case ' ':
	    case '\t':
	    case '\n':
	      continue;

	    case '#':
	    case '[':
	      fgets (buffer, sizeof (buffer), fp);

	      if (ferror (fp))
		goto failure;

	      if ('#' == c)
		continue;

	      len = strlen (buffer);
	      while (len > 0 && buffer[len - 1] <= ' ')
		--len;

	      if (len < 2 || ']' != buffer[--len])
		goto failure;

	      buffer[len] = 0;

	      if (!section (fp, info, channel_list,
			    &pass, &rf_channel, &default_channel,
			    buffer))
		goto failure;

	      break;
	    }
	}
    }

  return TRUE;

 failure:
  if (fp)
    fclose (fp);

  return FALSE;
}

/* XawTV IPC */

#ifndef XAWTV_IPC_DEBUG
#define XAWTV_IPC_DEBUG 0
#endif

static GdkAtom _GA_XAWTV_STATION;
static GdkAtom _GA_XAWTV_REMOTE;
static GdkAtom _GA_ZAPPING_REMOTE;
static GdkAtom _GA_STRING;

static gboolean
xawtv_command_setstation	(int			argc,
				 char **		argv)
{
  unsigned int nr;
  char *end;
  tveng_tuned_channel *ch;

  ch = NULL;

  if (1 == argc)
    return FALSE;

  if (0 == strcmp (argv[1], "next"))
    {
      python_command (NULL, "zapping.channel_up()");
      return TRUE;
    }

  if (0 == strcmp (argv[1], "prev"))
    {
      python_command (NULL, "zapping.channel_down()");
      return TRUE;
    }

  if (0 == strcmp (argv[1], "back"))
    {
      /* TODO */
      return FALSE;
    }

  nr = strtoul (argv[1], &end, 0);

  if (0 == *end)
    {
      tveng_tuned_channel *ch;

      if (!(ch = tveng_tuned_channel_nth (global_channel_list, nr)))
	{
	  fprintf (stderr, "Invalid channel number %u\n", nr);
	  return FALSE;
	}
    }
  else
    {
      gchar *t;

      if (!(t = g_locale_to_utf8 (argv[1], -1, NULL, NULL, NULL)))
	return FALSE;

      if (!(ch = tveng_tuned_channel_by_name (global_channel_list, t)))
	{
	  g_free (t);
	  fprintf (stderr, "Cannot find channel '%s'\n", argv[1]);
	  return FALSE;
	}

      g_free (t);
    }

  z_switch_channel (ch, zapping->info);

  return TRUE;
}

static gboolean
xawtv_command_vtx		(int			argc,
				 char **		argv)
{
  GString *s;
  gboolean in_span;
  int i;

  if (1 == argc)
    {
      osd_clear ();
      return TRUE;
    }

  s = g_string_new (NULL);

  in_span = FALSE;

  for (i = 1; i < argc; ++i)
    {
      gchar *t;
      unsigned int j0;
      unsigned int j;

      if (!(t = g_locale_to_utf8 (argv[i], -1, NULL, NULL, NULL)))
	{
	  g_string_free (s, /* cstring too */ TRUE);
	  return FALSE;
	}

      j0 = 0;

      for (j = 0; t[j]; ++j)
	{
	  if ('\033' == t[j])
	    {
	      gint col[2];

	      g_string_append_len (s, t + j0, (gint)(j - j0));

	      col[0] = -1;
	      col[1] = -1;

	      if ('[' == t[j + 1])
		{
		  /* ANSI sequence. */

		  for (j += 2; t[j];)
		    {
		      switch (t[j])
			{
			case '3': /* foreground */
			case '4': /* background */
			  if (t[j + 1] >= '0' && t[j + 1] <= '7')
			    {
			      col[t[j] & 2] = t[j + 1] & 7;
			      j += 2;
			    }
			  break;

			case '1':
			case ';':
			  ++j;
			  break;

			case 'm':
			  ++j;

			default:
			  goto done;
			}
		    }
		}
	      else
		{
		  /* Color ESC */

		  if (t[j + 1] >= '0' && t[j + 1] <= '7'
		      && t[j + 2] >= '0' && t[j + 2] <= '7')
		    {
		      col[1] = t[j + 1] & 7;
		      col[0] = t[j + 2] & 7;
		      j += 3;
		    }
		  else
		    {
		      ++j;
		    }
		}

	    done:

	      j0 = j;

	      if (col[0] >= 0 && col[1] >= 0)
		{
		  static const gchar *colors[8] =
		    {
		      "000000", "FF0000", "00FF00", "FFFF00",
		      "0000FF", "FF00FF", "00FFFF", "FFFFFF",
		    };

		  g_string_append_printf
		    (s, "%s<span foreground=\"#%s\" background=\"#%s\">",
		     in_span ? "</span>" : "",
		     colors[col[1]],
		     colors[col[0]]);

		  in_span = TRUE;
		}
	    }
	}

      g_string_append_len (s, t + j0, (gint)(j - j0));

      if (in_span)
	g_string_append (s, "</span>");

      g_free (t);

      /* XXX presently we can render only one row */
      break;
    }

  osd_render_markup (NULL, OSD_TYPE_SCREEN, s->str);

  g_string_free (s, /* cstring too */ TRUE);

  return TRUE;
}

static gboolean
xawtv_command			(int			argc,
				 char **		argv)
{
  if (XAWTV_IPC_DEBUG)
    {
      int i;

      fprintf (stderr, "xawtv_command: ");

      for (i = 0; i < argc; ++i)
	fprintf (stderr, "<%s> ", argv[i]);

      fputc ('\n', stderr);
    }

  if (0 == argc)
    return TRUE;

  /*
     From xawtv-remote man page:

       setstation [ <name> | <nr> | next | prev | back ]
              Set the TV station.  This selects on of the TV sta-
              tions  which  are  configured  in the .xawtv config
              file.  The argument can be the station  name  or  a
              number  (the first one listed in the config file is
              0, ...).  next/prev jumps to the next/previous sta-
              tion  in  the list, back to the previously selected
              one.

       setchannel [ <name> | next | prev ]
              Tune in some channel.

       setfreqtab <table>
              Set the frequency table.  See the menu in xawtv for
              a list of valid choices.

       setnorm <norm>
              Set the TV norm (NTSC/PAL/SECAM).

       setinput [ <input> | next ]
              Set the video input (Television/Composite1/...)

       capture [ on | off | overlay | grabdisplay ]
              Set capture mode.

       volume mute on | off
              mute / unmute audio.

       volume <arg>

       color <arg>
       hue <arg>
       bright <arg>
       contrast <arg>
              Set  the  parameter  to the specified value.  <arg>
              can be one of the following: A percent value ("70%"
              for  example).   Some absolute value ("32768"), the
              valid range is hardware specific.  Relative  values
              can be specified too by prefixing with "+=" or "-="
              ("+=10%" or  "-=2000").   The  keywords  "inc"  and
              "dec"   are  accepted  to  and  will  increase  and
              decrease the given value in small steps.
                              
       setattr <name> <value>
              Set the value of some  attribute  (color,  con-
              trast, ... can be set this way too).

       show [ <name> ]
              Show the value current of some attribute.

       list   List  all  available attributes with all properties
              (default value, range, ...)

       snap [ jpeg | ppm ] [ full | win | widthxheight ] <file-
              name>
              Capture one image.

       webcam <filename>
              Capture  one  image.   Does  basically  the same as
              "snap jpeg win <filename>".  Works also  while  avi
              recording is active.  It writes to a temporary file
              and renames it when  done,  so  there  is  never  a
              invalid file.

       movie driver [ files | raw | avi | qt ]

       movie  video [ ppm | pgm | jpeg | rgb | gray | 422 | 422p
              | rgb15 | rgb24 | mjpeg | jpeg | raw | mjpa | png ]

       movie fps <frames per second>

       movie audio [ mono8 | mono16 | stereo ]

       movie rate <sample rate>

       movie fvideo <filename>

       movie faudio <filename>

       movie start

       movie stop
              control xawtv's movie recorder.

       fullscreen
              Toggle fullscreen mode.

       showtime
              Display time (same what the 'D' key does in xawtv).

       quit   quit xawtv

       keypad n
              enter digit  'n'.   That's  the  two-digit  channel
              selection,  entering  two  digits  within 5 seconds
              switches to the selected station.  Useful for lirc.

       vdr command
              send  "command"  to  vdr  (via  connect  on  local-
              host:2001).
  */

  if (0 == strcmp (argv[0], "message") ||
      0 == strcmp (argv[0], "msg"))
    {
      gchar *t;

      if (argc < 2)
	return FALSE;

      if (!(t = g_locale_to_utf8 (argv[1], -1, NULL, NULL, NULL)))
	return FALSE;

      gtk_window_set_title (GTK_WINDOW (zapping), t);

      g_free (t);

      return TRUE;
    }

  if (0 == strcmp (argv[0], "quit"))
    {
      python_command (NULL, "zapping.quit()");
      return TRUE;
    }

  if (0 == strcmp (argv[0], "setstation"))
    return xawtv_command_setstation (argc, argv);

  if (0 == strcmp (argv[0], "vtx"))
    return xawtv_command_vtx (argc, argv);

  fprintf (stderr, "Command '%s' not implemented\n", argv[0]);

  return FALSE;
}

static GString *
property_get_string		(GdkWindow *		window,
				 GdkAtom		atom)
{
  /*  GdkDisplay *display; */
  Atom xatom;
  Atom actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_after;
  unsigned char *prop;
  GString *s;

  /* 2.6 stuff */
  /*  display = gdk_drawable_get_display (GDK_DRAWABLE (window));*/
  /*  xatom = gdk_x11_atom_to_xatom_for_display (display, atom);*/
  xatom = gdk_x11_atom_to_xatom (atom);

  /* GDK 2.6 gdk_property_get() documentation advises
     use of XGetWindowProperty() because function is fubar. */

  if (Success != XGetWindowProperty (GDK_WINDOW_XDISPLAY (window),
				     GDK_WINDOW_XWINDOW (window),
				     xatom,
				     /* long_offset */ 0 / sizeof (long),
				     /* long_length */ 65536 / sizeof (long),
				     /* delete */ True,
				     /* req_type */ XA_STRING,
				     &actual_type,
				     &actual_format,
				     &nitems,
				     &bytes_after,
				     &prop))
    return NULL;

  if (0 == nitems)
    return NULL;

  s = g_string_new (NULL);
  g_string_append_len (s, (gchar *) prop, (gint) nitems);

  /* Make sure we have a cstring. */
  g_string_append_len (s, "", 1);

  XFree (prop);

  return s;
}

static gboolean
on_event			(GtkWidget *		widget,
				 GdkEvent *		event,
				 gpointer		user_data _unused_)
{
  if (GDK_PROPERTY_NOTIFY == event->type)
    {
      if (_GA_XAWTV_REMOTE == event->property.atom)
	{
	  GString *s;
	  gsize i;
	  int argc;
	  char *argv[32];

	  if (!(s = property_get_string (event->property.window,
					 _GA_XAWTV_REMOTE)))
	    return TRUE;

	  argc = 0;

	  /* Format: "command\0par\0par\0\0command\0..." */

	  for (i = 0; i <= s->len; i += strlen (&s->str[i]) + 1)
	    {
	      if (argc >= (gint) G_N_ELEMENTS (argv) - 1)
		break; /* die graceful */

	      if (i == s->len || 0 == s->str[i])
		{
		  argv[argc] = NULL;

		  if (!xawtv_command (argc, argv))
		    break;

		  argc = 0;
		}
	      else
		{
		  argv[argc++] = &s->str[i];
		}
	    }

	  g_string_free (s, /* cstring too */ TRUE);

	  return TRUE; /* handled */
	}
      else if (_GA_ZAPPING_REMOTE == event->property.atom)
	{
	  GString *s;

	  if (!(s = property_get_string (event->property.window,
					 _GA_ZAPPING_REMOTE)))
	    return TRUE;

	  python_command (widget, s->str);

	  g_string_free (s, /* cstring too */ TRUE);

	  return TRUE; /* handled */
	}
    }

  return FALSE; /* pass on */
}

gboolean
xawtv_ipc_set_station		(GtkWidget *		window,
				 tveng_tuned_channel *	ch)
{
  gchar *name;
  gchar *rf_name;
  gboolean r;

  r = FALSE;

  name = g_locale_from_utf8 (ch->name, -1, NULL, NULL, NULL);
  rf_name = g_locale_from_utf8 (ch->rf_name, -1, NULL, NULL, NULL);

  if (name && rf_name)
    {
      GString *s;

      /* Format: "freq\0channel_name\0network_name\0". RF %.3f MHz,
	 channel name "21", "E2", "?", network name "foobar", "?",
	 presumably locale encoding. */

      s = g_string_new (NULL);
      g_string_printf (s, "%.3f%c%s%c%s",
		       ch->frequ / 1e6, 0,
		       ch->rf_name ? rf_name : "?", 0,
		       ch->name ? name : "?");

      gdk_property_change (window->window,
			   _GA_XAWTV_STATION,
			   _GA_STRING, /* bits */ 8,
			   GDK_PROP_MODE_REPLACE,
			   s->str, (gint) s->len + 1);

      g_string_free (s, /* cstring too */ TRUE);

      r = TRUE;
    }

  g_free (rf_name);
  g_free (name);

  return r;
}

gboolean
xawtv_ipc_init			(GtkWidget *		window)
{
  const gchar station[] = "0.000\0?\0?";
  GdkEventMask mask;

  _GA_XAWTV_STATION = gdk_atom_intern ("_XAWTV_STATION",
				       /* dont create */ FALSE);

  _GA_XAWTV_REMOTE = gdk_atom_intern ("_XAWTV_REMOTE", FALSE);
  _GA_ZAPPING_REMOTE = gdk_atom_intern ("_ZAPPING_REMOTE", FALSE);

  _GA_STRING = gdk_atom_intern ("STRING", FALSE);

  /* _XAWTV_STATION identifies us as XawTV clone which understands
     _XAWTV_REMOTE commands. Make sure the lights are on. */
  gdk_property_change (window->window,
		       _GA_XAWTV_STATION,
		       _GA_STRING, /* bits */ 8,
		       GDK_PROP_MODE_REPLACE,
		       station, sizeof (station));

  g_signal_connect (G_OBJECT (window), "event",
		    G_CALLBACK (on_event), /* user_data */ NULL);

  mask = gdk_window_get_events (window->window);
  mask |= GDK_PROPERTY_CHANGE_MASK;
  gdk_window_set_events (window->window, mask);

  return TRUE;
}

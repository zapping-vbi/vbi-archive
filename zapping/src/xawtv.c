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

/* $Id: xawtv.c,v 1.3 2004-05-16 11:41:01 mschimek Exp $ */

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

#ifndef XAWTV_CONFIG_TEST
#define XAWTV_CONFIG_TEST 0
#endif

static const GScannerConfig
config = {
  .cset_skip_characters = " \t",

  .cset_identifier_first = (G_CSET_a_2_z
			    G_CSET_A_2_Z
			    G_CSET_LATINC
			    G_CSET_LATINS),
  .cset_identifier_nth   = ("_+-"
			    G_CSET_DIGITS
			    G_CSET_a_2_z
			    G_CSET_A_2_Z
			    G_CSET_LATINC
			    G_CSET_LATINS),

  .cpair_comment_single = "#\n",

  .case_sensitive = TRUE,

  .skip_comment_multi = FALSE,
  .skip_comment_single = TRUE,
  .scan_comment_multi = FALSE,
  .scan_identifier = TRUE,
  .scan_identifier_1char = FALSE,
  .scan_identifier_NULL = FALSE,
  .scan_symbols = FALSE,
  .scan_binary = FALSE,
  .scan_octal = FALSE,
  .scan_float = TRUE,
  .scan_hex = TRUE,
  .scan_hex_dollar = FALSE,
  .scan_string_sq = FALSE,
  .scan_string_dq = FALSE,
  .numbers_2_int = FALSE,
  .int_2_float = FALSE,
  .identifier_2_string = FALSE,
  .char_2_token = TRUE,
  .symbol_2_token = FALSE,
  .scope_0_fallback = FALSE,
  .store_int64 = FALSE,
};

typedef enum {
  MIXER = 1,
  FREQTAB,
  FULLSCREEN,
  PIXSIZE,
  PIXCOLS,
  WM_OFF_BY,
  RATIO,
  JPEG_QUALITY,
  MJPEG_QUALITY,
  KEYPAD_NTSC,
  KEYPAD_PARTIAL,
  KEYPAD_OSD,
  MOV_DRIVER,
  MOV_VIDEO,
  MOV_FPS,
  MOV_AUDIO,
  MOV_RATE,
  OSD,
  OSD_POSITION,
  USE_WM_FULLSCREEN,
} global_symbol;

typedef enum {
  CAPTURE = 1,
  INPUT,
  SOURCE,
  NORM,
  AUDIO,
  CHANNEL,
  FREQ,
  FINE,
  KEY,
  COLOR,
  BRIGHT,
  HUE,
  CONTRAST,
  GROUP,
} channel_symbol;

static const gchar *
global_symbols [] = {
  "mixer",
  "freqtab",
  "fullscreen",
  "pixsize",
  "pixcols",
  "wm-off-by",
  "ratio",
  "jpeg-quality",
  "mjpeg-quality",
  "keypad-ntsc",
  "keypad-partial",
  "keypad-osd",
  "mov-driver",
  "mov-video",
  "mov-fps",
  "mov-audio",
  "mov-rate",
  "osd",
  "osd-position",
  "use-wm-fullscreen",
};

static const gchar *
channel_symbols [] = {
  "capture",
  "input",
  "source",
  "norm",
  "audio",
  "channel",
  "freq",
  "fine",
  "key",
  "color",
  "bright",
  "hue",
  "contrast",
  "group",
};

#define eat(symbol)							\
  if (symbol != g_scanner_get_next_token (scanner))			\
    goto failure;

#define expect(symbol)							\
  if (symbol != g_scanner_get_next_token (scanner))			\
    goto bad_symbol;

static void
skip_section			(GScanner *		scanner)
{
  while (G_TOKEN_EOF != g_scanner_peek_next_token (scanner))
    if (G_TOKEN_LEFT_BRACE != scanner->next_token)
      do
	if (G_TOKEN_EOF == g_scanner_get_next_token (scanner))
	  return;
      while ('\n' != scanner->token);
    else
      break;
}

static gboolean
global_section			(GScanner *		scanner,
				 tv_rf_channel *	rf_ch)
{
  gchar *freqtab;

  freqtab = NULL;

  while (G_TOKEN_EOF != g_scanner_peek_next_token (scanner)
	 && G_TOKEN_LEFT_BRACE != scanner->next_token)
    {
      gpointer symbol;

      if ('\n' == g_scanner_get_next_token (scanner))
	continue;

      if (G_TOKEN_IDENTIFIER != scanner->token)
	goto failure;

      symbol = g_scanner_scope_lookup_symbol
	(scanner, 1, scanner->value.v_identifier);

      if (XAWTV_CONFIG_TEST)
	if (0 == symbol)
	  fprintf (stderr, "Unknown global identifier '%s'\n",
		   scanner->value.v_identifier);

      eat (G_TOKEN_EQUAL_SIGN);

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

      switch (GPOINTER_TO_INT (symbol))
	{
	case FREQTAB:
	  if (!rf_ch)
	    goto bad_symbol;

	  expect (G_TOKEN_IDENTIFIER);

	  if (freqtab)
	    g_free (freqtab);

	  freqtab = g_strdup (scanner->value.v_identifier);

	  eat ('\n');

	  break;

	default:
	bad_symbol:
	  while ('\n' != scanner->token)
	    {
	      if (G_TOKEN_EOF == scanner->token)
		goto finish;

	      g_scanner_get_next_token (scanner);
	    }

	  break;
	}
    }

 finish:
  if (rf_ch)
    {
      gboolean r;

      if (!freqtab)
	return FALSE;

      r = tv_rf_channel_table_by_name (rf_ch, freqtab);

      g_free (freqtab);

      if (!r)
	return FALSE;
    }

  return TRUE;

 failure:
  g_free (freqtab);
  return TRUE;
}

static gboolean
set_control			(const tveng_device_info *info,
				 tveng_tuned_channel *	ch,
				 channel_symbol		symbol,
				 gfloat			value)
{
  tv_control *c;
  tv_control_id id;

  switch (symbol)
    {
    case COLOR:
      id = TV_CONTROL_ID_SATURATION;
      break;

    case BRIGHT:
      id = TV_CONTROL_ID_BRIGHTNESS;
      break;

    case HUE:
      id = TV_CONTROL_ID_HUE;
      break;

    case CONTRAST:
      id = TV_CONTROL_ID_CONTRAST;
      break;

    default:
      assert (0);
    }

  for (c = NULL; (c = tv_next_control (info, c));)
    if (c->id == id)
      break;

  if (NULL == c)
    return TRUE; /* not supported by device */

  return tveng_tuned_channel_set_control (ch, c->label, value);
}

static gboolean
channel_section			(GScanner *		scanner,
				 const tveng_device_info *info,
				 tv_rf_channel *	rf_ch,
				 tveng_tuned_channel *	tt_ch,
				 gboolean *		have_channel)
{
  gint fine_tuning;

  tt_ch->frequ = 0; /* Hz */

  *have_channel = FALSE;

  fine_tuning = 0; /* Hz */

  while (G_TOKEN_EOF != g_scanner_peek_next_token (scanner)
	 && G_TOKEN_LEFT_BRACE != scanner->next_token)
    {
      gpointer symbol;

      if ('\n' == g_scanner_get_next_token (scanner))
	continue;

      if (G_TOKEN_IDENTIFIER != scanner->token)
	goto failure;

      symbol = g_scanner_scope_lookup_symbol
	(scanner, 3, scanner->value.v_identifier);

      if (XAWTV_CONFIG_TEST)
	if (0 == symbol)
	  fprintf (stderr, "Unknown channel identifier '%s'\n",
		   scanner->value.v_identifier);

      eat (G_TOKEN_EQUAL_SIGN);

      switch (GPOINTER_TO_INT (symbol))
	{
	case INPUT:
	case SOURCE:
	  {
	    const tv_video_line *l;

	    expect (G_TOKEN_IDENTIFIER);

	    for (l = NULL; (l = tv_next_video_input (info, l));)
	      if (0 == g_ascii_strcasecmp (l->label, scanner->value.v_identifier))
		break;

	    eat ('\n');

	    if (NULL != l)
	      tt_ch->input = l->hash;

	    break;
	  }

	case NORM:
	  {
	    const tv_video_standard *s;

	    expect (G_TOKEN_IDENTIFIER);

	    for (s = NULL; (s = tv_next_video_standard (info, s));)
	      if (0 == g_ascii_strcasecmp (s->label, scanner->value.v_identifier))
		break;

	    eat ('\n');

	    if (NULL != s)
	      tt_ch->standard = s->hash;

	    break;
	  }

	case CHANNEL:
	  {
	    gchar *s;

	    g_scanner_get_next_token (scanner);

	    if (G_TOKEN_IDENTIFIER == scanner->token)
	      s = g_strdup (scanner->value.v_identifier);
	    else if (G_TOKEN_INT == scanner->token)
	      s = g_strdup_printf ("%u", (guint) scanner->value.v_int);
	    else
	      goto bad_symbol;

	    if ('\n' != g_scanner_get_next_token (scanner))
	      {
		g_free (s);
		goto failure;
	      }

	    if (tv_rf_channel_by_name (rf_ch, s))
	      {
		if (0 == tt_ch->frequ)
		  tt_ch->frequ = rf_ch->frequency;
 
		*have_channel = TRUE;	      
	      }

	    g_free (s);

	    break;
	  }

	case FREQ:
	  {
	    guint frequ;

	    g_scanner_get_next_token (scanner);

	    if (G_TOKEN_FLOAT == scanner->token)
	      {
		if (scanner->value.v_float < 0
		    || scanner->value.v_float > 1000)
		  goto bad_symbol;

		frequ = (guint)(scanner->value.v_float * 1e6);
	      }
	    else if (G_TOKEN_INT == scanner->token)
	      {
		if (scanner->value.v_int < 0
		    || scanner->value.v_int > 1000)
		  goto bad_symbol;

		frequ = scanner->value.v_int * 1000000; /* -> Hz */
	      }
	    else
	      {
		goto bad_symbol;
	      }

	    eat ('\n');

	    tt_ch->frequ = frequ;

	    if (!*have_channel)
	      if (!tv_rf_channel_by_frequency (rf_ch, frequ))
		tv_rf_channel_first (rf_ch);

	    *have_channel = TRUE;

	    break;
	  }

	case FINE:
	  expect (G_TOKEN_INT);

	  fine_tuning = scanner->value.v_int;

	  eat ('\n');

	  if (fine_tuning < -128 || fine_tuning > 127)
	    {
	      fine_tuning = 0;
	      goto bad_symbol;
	    }

	  fine_tuning *= 62500; /* -> Hz */

	  break;

	case KEY:
	  /* XXX Xt application resource translation, like
	     "qual<Key>key: command" (try apropos resource), stored here
	     as "key" or "qual+key". How can we properly translate? */
	  goto bad_symbol;

	case COLOR:
	case BRIGHT:
	case HUE:
	case CONTRAST:
	  {
	    gfloat value;

	    g_scanner_get_next_token (scanner);

	    if (G_TOKEN_INT == scanner->token)
	      value = scanner->value.v_int / 100.0;
	    else if (G_TOKEN_FLOAT == scanner->token)
	      value = scanner->value.v_float / 100.0;
	    else
	      goto bad_symbol;

	    if ('%' == g_scanner_peek_next_token (scanner))
	      g_scanner_get_next_token (scanner);

	    eat ('\n');

	    /* Error ignored */
	    set_control (info, tt_ch,
			 GPOINTER_TO_INT (symbol),
			 SATURATE (value, 0.0, 1.0));

	    break;
	  }

	case CAPTURE: /* on | off | overlay | grabdisplay */
	case AUDIO: /* <int> */
	case GROUP: /* <string> -? */
	default:

	bad_symbol:
	  while ('\n' != scanner->token)
	    {
	      if (G_TOKEN_EOF == scanner->token)
		goto finish;

	      g_scanner_get_next_token (scanner);
	    }

	  break;
	}
    }

 finish:
  tt_ch->frequ += fine_tuning;

  return TRUE;

 failure:
  return FALSE;
}

static gboolean
section				(GScanner *		scanner,
				 const tveng_device_info *info,
				 tveng_tuned_channel **	channel_list,
				 guint *		pass,
				 tv_rf_channel *	rf_channel,
				 tveng_tuned_channel *	default_channel,
				 const gchar *		identifier)
{
  if (0 == strcmp (identifier, "global"))
    {
      if (0 == *pass)
	{
	  *pass = 1;

	  /* rf_channel = frequency table */
	  if (!global_section (scanner, rf_channel))
	    return FALSE;
	}
      else
	{
	  skip_section (scanner);
	}
    }
  else if (0 == strcmp (identifier, "launch"))
    {
      /* Key bindings */
      skip_section (scanner);
    }
  else if (0 == strcmp (identifier, "defaults"))
    {
      /* Default channel attributes */

      if (1 == *pass)
	{
	  gboolean dummy;

	  *pass = 2;

	  if (!channel_section (scanner, info,
				rf_channel, default_channel,
				&dummy))
	    return FALSE;
	}
      else
	{
	  skip_section (scanner);
	}
    }
  else
    {
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

	  if (!channel_section (scanner, info, rf_channel, &channel, &have_channel)
	      || !have_channel)
	    {
	      g_free (channel.controls);
	      return FALSE;
	    }

	  channel.name = (gchar *) identifier;

	  channel.rf_table = (gchar *) rf_channel->table_name;
	  channel.rf_name = rf_channel->channel_name;

	  if ((ch = tveng_tuned_channel_by_name (*channel_list, identifier)))
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
	  skip_section (scanner);
	}
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
  tv_rf_channel rf_channel;
  tveng_tuned_channel default_channel;

  CLEAR (default_channel);

  /* Usually the file will contain the information we need in
     proper order. If not we run a second and third pass. */
  for (pass = 0; pass < 3; ++pass)
    {
      gchar *filename;
      int fd;
      GScanner *scanner;
      guint i;

      filename = g_strconcat (g_get_home_dir (), "/.xawtv", NULL);

      if (-1 == (fd = open (filename, O_RDONLY)))
	{
	  g_free (filename);
	  return FALSE; /* XXX error message please */
	}

      scanner = g_scanner_new (&config);

      scanner->input_name = filename;

      g_scanner_input_file (scanner, fd);

      for (i = 0; i < G_N_ELEMENTS (global_symbols); ++i)
	g_scanner_scope_add_symbol (scanner, 1, global_symbols[i],
				    GINT_TO_POINTER (i + 1));

      for (i = 0; i < G_N_ELEMENTS (channel_symbols); ++i)
	g_scanner_scope_add_symbol (scanner, 3, channel_symbols[i],
				    GINT_TO_POINTER (i + 1));

      scanner->token = '0';

      while (G_TOKEN_EOF != scanner->token)
	{
	  gchar *s;

	  s = NULL;

	  eat (G_TOKEN_LEFT_BRACE);
	  eat (G_TOKEN_IDENTIFIER);

	  s = g_locale_to_utf8 (scanner->value.v_identifier,
				strlen (scanner->value.v_identifier),
				NULL, NULL, NULL);

	  g_assert (NULL != s);

	  eat (G_TOKEN_RIGHT_BRACE);
	  eat ('\n');

	  if (!section (scanner, info, channel_list,
			&pass, &rf_channel, &default_channel, s))
	    {
	      g_free (s);

	      g_scanner_destroy (scanner);
	      close (fd);
	      g_free (filename);

	      return FALSE;
	    }

	  g_free (s);

	  continue;

	failure:
	  g_free (s);

	  while (G_TOKEN_EOF != scanner->token
		 && '\n' != scanner->token)
	    g_scanner_get_next_token (scanner);
	}

      g_scanner_destroy (scanner);
      close (fd);
      g_free (filename);
    }

  return TRUE;
}

/* XawTV IPC */

static GdkAtom _GA_XAWTV_STATION;
static GdkAtom _GA_XAWTV_REMOTE;
static GdkAtom _GA_ZAPPING_REMOTE;
static GdkAtom _GA_STRING;

static gboolean
xawtv_command			(int			argc,
				 char **		argv)
{
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

       msg text
              Display text on the on-screen display (window title
              / upper left corner in fullscreen mode).         

       vtx line1 line2 [ ... ]
              Display subtitles.  It pops up a  small  window  at
              the  bottom  of  the screen.  It is supported to be
              used as interface for displaying  subtitles  (often
              on  videotext  page  150  in  europe, thats why the
              name) by external programs.
              Every command line argument is one line, zero lines
              removes the window.  You can colorize the text with
              the control sequence "ESC  foreground  background".
              foreground/background  has the range 0-7 (ansi term
              colors).  Example: "\03347 hello world " is blue on
              white.  "\033" must be a real escape character, the
              string does'nt work.  With the bash you'll  get  it
              with ^V ESC.  vtx does also understand the ANSI tty
              escape sequences for color.

       quit   quit xawtv

       keypad n
              enter digit  'n'.   That's  the  two-digit  channel
              selection,  entering  two  digits  within 5 seconds
              switches to the selected station.  Useful for lirc.

       vdr command
              send  "command"  to  vdr  (via  connect  on  local-
              host:2001).
  */

  fprintf (stderr, "Command '%s' not implemented\n", argv[0]);

  return FALSE;
}

static GString *
property_get_string		(GdkWindow *		window,
				 GdkAtom		atom)
{
  GdkDisplay *display;
  Atom xatom;
  Atom actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_after;
  unsigned char *prop;
  GString *s;

  display = gdk_drawable_get_display (GDK_DRAWABLE (window));
  xatom = gdk_x11_atom_to_xatom_for_display (display, atom);

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
  g_string_append_len (s, prop, nitems);

  /* Make sure we have a cstring. */
  g_string_append_len (s, "", 1);

  XFree (prop);

  return s;
}

static gboolean
on_event			(GtkWidget *		widget,
				 GdkEvent *		event,
				 gpointer		user_data)
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
	      if (argc >= G_N_ELEMENTS (argv) - 1)
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

	  on_python_command1 (widget, s->str);

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
			   s->str, s->len + 1);

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

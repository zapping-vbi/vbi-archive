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

/* $Id: xawtv.c,v 1.2 2003-11-29 19:43:24 mschimek Exp $ */

/*
   XawTV compatibility functions:
   * Import XawTV configuration - currently channels only
   * XawTV IPC (w/nxtvepg etc) - to do
 */

#include "site_def.h"

#include <glib.h>
#include <gnome.h>
#include <fcntl.h>
#include <string.h>

#include "xawtv.h"
#include "zmisc.h"

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

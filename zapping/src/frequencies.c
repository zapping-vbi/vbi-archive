/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 Iñaki García Etxebarria 
 * Copyright (C) 2002-2003 Michael H. Schimek
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
/**
 * The frequency table is taken from xawtv with donations around the globe.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stddef.h>
#include "common/math.h"

#include <gnome.h>
#include "strnatcmp.h"
#include "frequencies.h"

#include "zmisc.h"

/* FIXME video standards need a review. */

#define ALPHA	 (1 << 0) /* first ... last is alphabetic (ASCII) */
#define UPSTREAM (1 << 1) /* Upstream channel (eg. cable modem) */
#define ALIGN    (1 << 2) /* See tv_rf_channel_align() */

struct range {
	/* Note the prefixes must match the XawTV tables to
	   properly parse the .xawtv config file. */
	const char		prefix[3];
	unsigned		flags		: 8;	/* ALPHA | ... */
	unsigned 		first		: 8;	/* first channel number */
	unsigned 		last		: 8;	/* last, inclusive */
	unsigned		bandwidth	: 16;	/* kHz */
	unsigned		frequency	: 32;	/* of first channel, kHz */
};

#define RANGE_END		{ "", 0, 0, 0, 0, 0 }
#define IS_RANGE_END(r) ((r)->frequency == 0)

/* Some common ranges. */

#define RANGE_EIA_2_4		{ "",  ALIGN,  2,  4, 6000,  55250 }
#define RANGE_EIA_5_6		{ "",  ALIGN,  5,  6, 6000,  77250 }
#define RANGE_EIA_VHF		{ "",  ALIGN,  7, 13, 6000, 175250 }
#define RANGE_EIA_UHF		{ "",  ALIGN, 14, 83, 6000, 471250 }

/* "E"? "S0"? "SE"? Anyway, have to keep that for XawTV compatibility. */
#define RANGE_CCIR_BAND_I	{ "E",  0,  2,  4, 7000,  48250 }
#define RANGE_CCIR_SUBBAND	{ "S0", 0,  1,  3, 7000,  69250 }
/* CCIR Band II FM 88-108 MHz here */
#define RANGE_CCIR_USB		{ "SE", 0,  1, 10, 7000, 105250 } /* Lower Hyperband */
#define RANGE_CCIR_BAND_III	{ "E",  0,  5, 12, 7000, 175250 }
#define RANGE_CCIR_OSB		{ "SE", 0, 11, 20, 7000, 231250 } /* Upper Hyperband */
#define RANGE_CCIR_ESB		{ "S",  0, 21, 41, 8000, 303250 } /* Extended Hyperband */
#define RANGE_CCIR_UHF		{ "",   0, 21, 69, 8000, 471250 } /* Band IV (21-37), Band V (38-69) */

struct table {
	const char *		name;			/* Canonical table name for config */
	const char *		name_zapping;	        /* Old Zapping table name */
	const char *		name_xawtv;		/* XawTV table name */
	const char *		countries;		/* ISO 3166 2-char */
	const char *		domain;			/* e.g. "cable" */
	unsigned int		video_standards;	/* set of TV_VIDEOSTD_XXX */
	struct range		freq_ranges [40];	/* XXX see "ie" table */
};

#define TABLE_END { NULL, NULL, NULL, NULL, NULL, 0, { RANGE_END } }
#define IS_TABLE_END(t) ((t)->name == NULL)

static const struct table
frequency_tables [] =
{
  {
    /* http://www.eia.org/ */
    "eia", "US terrestrial", "us-bcast",
    "US" "CA",
    N_("broadcast"),
    TV_VIDEOSTD_NTSC_M,
    {
      RANGE_EIA_2_4,
      RANGE_EIA_5_6,
      RANGE_EIA_VHF,
      RANGE_EIA_UHF,
      RANGE_END
    }
  }, {
    /* IRC: Incrementally Related Carriers */
    "eia-irc", "US cable", "us-cable",
    "US" "CA",
    N_("cable IRC"),
    TV_VIDEOSTD_NTSC_M,
    {
      { "", ALIGN, 1, 1, 6000, 73250 },
      RANGE_EIA_2_4,
      RANGE_EIA_5_6,
      RANGE_EIA_VHF,
      { "", ALIGN,  14,  22, 6000, 121250 },  /* EIA Midband */
      { "", ALIGN,  23,  94, 6000, 217250 },  /* EIA Superband (23-36), Hyperband (37-53) */
      { "", ALIGN,  95,  96, 6000,  91250 },
      { "", ALIGN,  97,  99, 6000, 103250 },
      { "", ALIGN, 100, 125, 6000, 649250 },
      { "T", ALIGN | UPSTREAM, 7, 14, 6000, 8250 },
      RANGE_END
    }
  }, {
    /* HRC: Harmonically Related Carriers */
    "eia-hrc", "US cable-hrc", "us-cable-hrc",
    "US" "CA",
    N_("cable HRC"),
    TV_VIDEOSTD_NTSC_M,
    {
      { "", ALIGN,   1,   1, 6000,  72000 },
      { "", ALIGN,   2,   4, 6000,  54000 },
      { "", ALIGN,   5,   6, 6000,  78000 },
      { "", ALIGN,   7,  13, 6000, 174000 },
      { "", ALIGN,  14,  22, 6000, 120000 },
      { "", ALIGN,  23,  94, 6000, 216000 },
      { "", ALIGN,  95,  96, 6000,  90000 },
      { "", ALIGN,  97,  99, 6000, 102000 },
      { "", ALIGN, 100, 125, 6000, 648000 },
      { "T", ALIGN | UPSTREAM, 7, 14, 6000, 7000 },
      RANGE_END
    }
#if 0 /* XXX apparently not, check this */
  }, {
    "ca-cable", "Canada cable", "canada-cable",
    "CA", N_("cable"),
    TV_VIDEOSTD_NTSC_M,
    {
      { "", ALIGN,   2,   4, 6000,  61750 },
      { "", ALIGN,   5,   6, 6000,  83750 },
      { "", ALIGN,   7,  13, 6000, 181750 },
      { "", ALIGN,  14,  22, 6000, 127750 },
      { "", ALIGN,  23,  94, 6000, 223750 },
      { "", ALIGN,  95,  95, 6000,  97750 },
      { "", ALIGN,  96,  99, 6000, 103750 },
      { "", ALIGN, 100, 125, 6000, 655750 },
      RANGE_END
    }
#endif
  }, {
    "jp", "Japan terrestrial", "japan-bcast",
    "JP", N_("broadcast"),
    TV_VIDEOSTD_NTSC_M_JP,
    {
      { "", ALIGN,  1,  3, 6000,  91250 },
      { "", ALIGN,  4,  7, 6000, 171250 },  /* NB #7 is 189250 */
      { "", ALIGN,  8, 12, 6000, 193250 },
      { "", ALIGN, 13, 62, 6000, 471250 },
      RANGE_END
    }
  }, {
    /* ALIGN? */
    "jp-cable", "Japan cable", "japan-cable",
    "JP", N_("cable"),
    TV_VIDEOSTD_NTSC_M_JP,
    {
      { "", ALIGN, 13, 22, 6000, 109250 },
      { "", ALIGN, 23, 23, 6000, 223250 },
      { "", ALIGN, 24, 27, 6000, 231250 }, /* NB #27 is 249250 */
      { "", ALIGN, 28, 63, 6000, 253250 },
      RANGE_END
    }
  }, {
    /* CCIR is now ITU-R. http://www.itu.int/ */
    /* Not all countries use all bands, maybe we should
       split this table accordingly. */
    "ccir", "Europe", "europe-west",
    "AT" "BE" "CH" "DE" "DK" "ES" "FI" "GR"
    "IN" "KH" "MY" "NL" "NO" "PT" "SE" "SG" "TH"
    "UK",
    NULL,
    TV_VIDEOSTD_PAL_B | TV_VIDEOSTD_PAL_G, /* I in UK? */
    {
      RANGE_CCIR_BAND_I,
      RANGE_CCIR_SUBBAND,
      RANGE_CCIR_USB,
      RANGE_CCIR_BAND_III,
      RANGE_CCIR_OSB,
      RANGE_CCIR_ESB,
      RANGE_CCIR_UHF,
      RANGE_END
    }
  }, {
    "au", "Australia", "australia",
    "AU", NULL, TV_VIDEOSTD_PAL_B, /* I? */
    {
      { "", 0,  0,  0, 7000,  46250 },
      { "", 0,  1,  2, 7000,  57250 },
      { "", 0,  3,  3, 7000,  86250 },
      { "", 0,  4,  5, 7000,  95250 },
      { "", 0,  6,  9, 7000, 175250 },
      { "", 0, 10, 11, 7000, 209250 },
      { "", 0, 28, 69, 7000, 527250 }, /* Band IV-V */
      RANGE_END
    }
  }, {
    "au-optus", "Australia Optus", "australia-optus",
    "AU", "Optus",
    TV_VIDEOSTD_PAL_B, /* I? */
    {
      { "", 0,  1,  1, 7000, 138250 },
      { "", 0,  2,  9, 7000, 147250 },
      { "", 0, 10, 11, 7000, 209250 },
      { "", 0, 12, 22, 7000, 224250 },
      { "", 0, 23, 26, 7000, 303250 },
      { "", 0, 27, 48, 7000, 338250 },
      RANGE_END
    }
  }, {
    "it", "Italy", "italy",
    "IT", NULL,
    TV_VIDEOSTD_PAL_B,
    {
      { "", ALPHA, 'A', 'A', 7000,  53750 }, /* Band I (A-B) */
      { "", ALPHA, 'B', 'B', 7000,  62250 },
      { "", ALPHA, 'C', 'C', 7000,  82250 }, /* Band II (C) */
      { "", ALPHA, 'D', 'D', 7000, 175250 }, /* Band III (D-H) */
      { "", ALPHA, 'E', 'E', 7000, 183250 },
      { "", ALPHA, 'F', 'F', 7000, 192250 },
      { "", ALPHA, 'G', 'G', 7000, 201250 },
      { "", ALPHA, 'H', 'H', 7000, 210250 },
      { "H",    0,   1,   2, 7000, 217250 }, /* Band III (H1-H2) */
      RANGE_CCIR_UHF,
      RANGE_END
    }
  }, {
    /* OIRT unexists, merged with EBU in 1993. */
    "oirt", "Eastern Europe", "europe-east",
    "AL" "BA" "BG" "CZ" "HR" "HU" "MK" "PL"
    "RO" "SK" "YU",
    NULL,
    0, /* STD? */
    {
      { "R",  0,  1,  1, 7000,  49750 }, /* Band I (R1-R3) ... */
      { "R",  0,  2,  2, 7000,  59250 },
      { "R",  0,  3,  4, 7000,  77250 }, /* Band II (R4-R5) ... */
      { "R",  0,  5,  5, 7000,  93250 },
      RANGE_CCIR_USB,
      { "SR", 0,  1,  8, 8000, 111250 },
      { "R",  0,  6, 12, 8000, 175250 }, /* Band III ... */
      { "SR", 0, 11, 18, 8000, 231250 },
/* XXX which one is right? */
#if 1 /* XawTV */
      { "S",  0, 19, 40, 8000, 295250 },
#else /* ? */
      { "S",  0, 11, 19, 8000, 231250 },
      { "S",  0, 21, 41, 8000, 303250 },
#endif
      RANGE_CCIR_UHF,
      RANGE_END
    }


  }, {
    "pl-atk", "Poland Autocom cable", NULL,
    /* TRANSLATORS: Leave "Autocom" untranslated. */
    "PL", N_("Autocom cable"),
    0, /* STD? */
    {
      { "S", 0,  1, 12, 8000, 111250 },
      { "S", 0, 13, 42, 8000, 215250 },
      { "S", 0, 43, 50, 8000, 471250 },
      { "S", 0, 51, 64, 8000, 551250 },
      { "S", 0, 65, 66, 8000, 711250 },
      { "S", 0, 67, 67, 8000, 743250 },
      RANGE_END
    }
  }, {
    /* OIRT Russia (?) */
    "ru", "Russia", "russia",
    "RU", NULL,
    0, /* STD? */
    {
/* XXX which one is right? */
#if 1 /* XawTV */
      { "R",  0,  1,  1, 7000,  49750 }, /* Band I (R1-R3) ... */
      { "R",  0,  2,  2, 7000,  59250 },
      { "R",  0,  3,  4, 7000,  77250 }, /* Band II (R4-R5) ... */
      { "R",  0,  5,  5, 7000,  93250 },
      { "SR", 0,  1,  8, 8000, 111250 },
      { "R",  0,  6, 12, 8000, 175250 }, /* Band III ... */
      { "SR", 0, 11, 18, 8000, 231250 },
      { "S",  0, 19, 40, 8000, 295250 },
#else /* ? */
      { "",   0,  1,  1, 8000,  48500 },
      { "",   0,  2,  2, 8000,  58000 },
      { "",   0,  3,  5, 8000,  76000 },
      { "",   0,  6, 12, 8000, 174000 },
      { "",   0, 21, 60, 8000, 470000 },
#endif
      RANGE_CCIR_UHF,
      RANGE_END
    }
  }, {
    "ie", "Ireland", "ireland",
    "IE",
    NULL, TV_VIDEOSTD_PAL_I,
    {
/* XXX which one is right? And WTF? */
#if 1 /* XawTV */
      { "A", 0,  0,  0, 8000,  45750 },
      { "A", 0,  1,  1, 8000,  48000 },
      { "A", 0,  2,  2, 8000,  53570 },
      { "A", 0,  3,  3, 8000,  56000 },
      { "A", 0,  4,  4, 8000,  61750 },
      { "A", 0,  5,  5, 8000,  64000 },
      { "A", 0,  6,  6, 8000,  175250 },
      { "A", 0,  7,  7, 8000,  176000 },
      { "A", 0,  8,  8, 8000,  183250 },
      { "A", 0,  9,  9, 8000,  184000 },
      { "A", 0, 10, 10, 8000,  191250 },
      { "A", 0, 11, 11, 8000,  192000 },
      { "A", 0, 12, 12, 8000,  199250 },
      { "A", 0, 13, 13, 8000,  200000 },
      { "A", 0, 14, 14, 8000,  207250 },
      { "A", 0, 15, 15, 8000,  208000 },
      { "A", 0, 16, 16, 8000,  215250 },
      { "A", 0, 17, 17, 8000,  216000 },
      { "A", 0, 18, 19, 8000,  224000 },
      { "A", 0, 20, 29, 8000,  248000 },
      { "A", 0, 30, 31, 8000,  344000 },
      { "A", 0, 32, 33, 8000,  408000 },
      { "A", 0, 34, 34, 8000,  448000 },
      { "A", 0, 35, 35, 8000,  480000 },
      { "A", 0, 36, 36, 8000,  520000 },
#else
      { "", 0,  0,  2, 8000,  45750 }, /* Band I (A-C) */
      { "", 0,  3,  8, 8000, 175750 }, /* Band III (D-J) */
#endif
      RANGE_CCIR_UHF,
      RANGE_END
    }
  }, {
    "fr", "France", "france",
    "FR",
    NULL, TV_VIDEOSTD_SECAM_L,
    {
      { "K0", 0,  1,  1, 7000,  47750 },
      { "K0", 0,  2,  2, 7000,  55750 },
      { "K0", 0,  3,  3, 7000,  60500 },
      { "K0", 0,  4,  4, 7000,  63750 },
      { "K0", 0,  5,  9, 8000, 176000 }, /* Band III (K5-K10) */
      { "K",  0, 10, 10, 8000, 216000 },
      { "K", ALPHA, 'B', 'Q', 12000, 116750 }, /* former System E (1984) (KB-KQ) */
      { "H0", 0,  1,  9, 8000, 303250 },
      { "H",  0, 10, 19, 8000, 375250 },
      RANGE_CCIR_UHF,
      RANGE_END
    }
  }, {
    "nz", "New Zealand", "newzealand",
    "NZ", NULL, 0, /* STD? */
    {
      { "", 0, 1,  1, 7000,  45250 },
      { "", 0, 2,  3, 7000,  55250 },
      { "", 0, 4,  5, 7000, 175250 },
      { "5", ALPHA, 'A', 'A', 7000, 138250 },
      { "", 0, 6,  7, 7000, 189250 }, /* NB #7 is 196250 */
      { "", 0, 8, 10, 7000, 203250 },
      RANGE_CCIR_UHF,
      RANGE_END
    }
  }, {
    "za", "South Africa", "southafrica",
    "ZA",
    NULL, 0, /* STD? */
    {
/* XXX which one is right? */
#if 1 /* XawTV */
      { "", 0,  1, 13, 8000, 175250 }, /* Band III */
#else /* ? */
      { "", 0,  4, 11, 8000, 175250 }, /* Band III (4-11) */
      { "", 0, 13, 13, 8000, 247430 }, /* Band III (247,43 sic) */
#endif
      RANGE_CCIR_UHF,
      RANGE_END
    }
  }, {
    "cn", "China", "china-bcast",
    "CN",
    NULL, 0, /* STD? */
    {
      { "", 0,  1,  3, 8000,  49750 },
      { "", 0,  4,  5, 8000,  77250 },
      { "", 0,  6, 49, 8000, 112250 },
      { "", 0, 50, 94, 8000, 463250 },
      RANGE_END
    }
  }, {
    "ar", "Argentina", "argentina",
    "AR",
    NULL, TV_VIDEOSTD_PAL_NC,
    {
      /* Prefix "0" from XawTV. */
      { "0", 0,  1,  3, 6000,  56250 },
      { "0", 0,  4,  4, 6000,  78250 },
      { "0", 0,  5,  5, 6000,  84250 },
      { "0", 0,  6, 12, 6000, 176250 },
      { "0", 0, 13, 21, 6000, 122250 },
      { "0", 0, 22, 93, 6000, 218250 },
      RANGE_END
    }
  }, {
    /* Like CCIR, except Band I & III differently numbered */
    "id", NULL, NULL,
    "ID",
    NULL, TV_VIDEOSTD_PAL_B | TV_VIDEOSTD_PAL_G,
    {
      { "", 0,  1,  3, 7000,  48250 },
      { "", 0,  4, 11, 7000, 175250 },
      RANGE_CCIR_UHF,
      RANGE_END
    }
  },

  TABLE_END
};

static void
get_channel			(tv_rf_channel *	ch,
				 const struct range *	r,
				 unsigned int		i)
{
	snprintf (ch->channel_name, sizeof (ch->channel_name),
		  (r->flags & ALPHA) ? "%s%c" : "%s%u",
		  r->prefix, r->first + i);

	ch->frequency = (r->frequency + r->bandwidth * i) * 1000;

	ch->_range = r;
	ch->_channel = i;
}

static tv_bool
first_channel			(tv_rf_channel *	ch,
				 const struct table *	t)
{
	const struct range *r;

	ch->_table		= t;

	ch->table_name		= t->name;

	ch->country_code[0]	= t->countries[0];
	ch->country_code[1]	= t->countries[1];
	ch->country_code[2]	= 0;

	ch->domain		= t->domain;
	ch->video_standards	= t->video_standards;

	r = t->freq_ranges;

	get_channel (ch, r, 0);

	ch->bandwidth		= r->bandwidth * 1000;

	return TRUE;
}

/*
 *  Enumerate channel name prefixes ("S", "E", "R" etc.) used
 *  in the frequency table this RF channel is in, by incrementing
 *  index from 0 up until this function returns NULL.
 */
const char *
tv_rf_channel_table_prefix	(tv_rf_channel *	ch,
				 unsigned int		index)
{
	const struct table *t;
	const struct range *r;

	t = ch->_table;

	for (r = t->freq_ranges; !IS_RANGE_END (r); r++) {
		const struct range *s;

		if (r->prefix[0] == 0)
			continue;

		for (s = t->freq_ranges; s != r; s++)
			if (0 == memcmp (s->prefix, r->prefix, sizeof (s->prefix)))
				break;

		if (s == r /* unique */
		    && 0 == index--)
			return r->prefix;
	}

	return NULL;
}

static tv_bool
first_table_by_country		(tv_rf_channel *	ch,
				 const struct table **	tp,
				 const char *		country)
{
	const struct table *t = *tp;
	const char *s;

	if (country[0] == 0 || country[1] == 0)
		return FALSE;

	for (; !IS_TABLE_END (t); t++)
		for (s = t->countries; *s; s += 2)
			if (s[0] == country[0] && s[1] == country[1])
				return first_channel (ch, *tp = t);

	return FALSE;
}

/*
 *  Return the first RF channel of the first frequency table
 *  with this name, either country code or table name or both
 *  separated by an at-sign (e.g. "DE@ccir").
 *
 *  The older Zapping frequency table name or a XawTV table
 *  name will match too.
 */
tv_bool
tv_rf_channel_table_by_name	(tv_rf_channel *	ch,
				 const char *		name)
{
	const struct table *t;

	if (!name)
		return first_channel (ch, frequency_tables);

	/* Table name match */

	for (t = frequency_tables; !IS_TABLE_END (t); t++)
		if (0 == strcmp (t->name, name)
		    || (t->name_zapping && 0 == strcmp (t->name_zapping, name))
		    || (t->name_xawtv && 0 == strcmp (t->name_xawtv, name)))
			return first_channel (ch, t);

	/* Country code "US" or with modifier "US@eia-terr" */

	if (name[0] && name[1]) {
		tv_rf_channel ch2;
		const char *modifier;

		t = frequency_tables;

		if (name[2] == 0)
			return first_table_by_country (ch, &t, name);
		else if (name[2] != '@')
			return FALSE;

		modifier = name + 3;

		for (; first_table_by_country (&ch2, &t, name); t++) {
			if (0 == strcmp (t->name, modifier)
			    || (t->name_zapping && 0 == strcmp (t->name_zapping, modifier))
			    || (t->name_xawtv && 0 == strcmp (t->name_xawtv, modifier))) {
				*ch = ch2;
				return TRUE;
			}
		}
	}

	return FALSE;
}

/*
 *  Take the frequency table this channel is in and
 *  return the first RF channel of the next frequency table.
 *
 *  tv_rf_channel_first_table (&ch);
 *  do ... while (tv_rf_channel_next_table (&ch));
 */
tv_bool
tv_rf_channel_next_table	(tv_rf_channel *	ch)
{
	const struct table *t;

	t = ch->_table;

	++t;

	if (!IS_TABLE_END (t))
		return first_channel (ch, t);

	return FALSE;
}

/*
 *  Return the first RF channel of the nth (0++) frequency table.
 */
tv_bool
tv_rf_channel_nth_table		(tv_rf_channel *	ch,
				 unsigned int		index)
{
	if (index >= N_ELEMENTS (frequency_tables))
		return FALSE;

	return first_channel (ch, frequency_tables + index);
}

/*
 *  Return the number of RF channels in the frequency table this
 *  channel is in.
 */
unsigned int
tv_rf_channel_table_size	(tv_rf_channel *	ch)
{
	const struct table *t;
	const struct range *r;
	unsigned int count = 0;

	t = ch->_table;

	for (r = t->freq_ranges; !IS_RANGE_END (r); r++)
		count += r->last - r->first + 1;

	return count;
}

/*
 *  Whether channels names should align with channel numbers,
 *  US style.
 */
extern tv_bool
tv_rf_channel_align	(tv_rf_channel *	ch)
{
	const struct table *t;

	t = ch->_table;

	return !!(t->freq_ranges[0].flags & ALIGN);
}

/*
 *  Given an ISO 3166 country code (e.g. "US"), take the frequency
 *  table this RF channel is in and return the first
 *  RF channel of the next frequency table used in this country.
 *
 *  tv_rf_channel_first_table_by_country (&ch, "US");
 *  do {
 *    do ... while (tv_rf_channel_next (&ch));
 *  } while (tv_rf_channel_next_table_by_country (&ch, "US"));
 */
tv_bool
tv_rf_channel_next_table_by_country
				(tv_rf_channel *	ch,
				 const char *		country)
{
	const struct table *t;

	t = ch->_table;

	if (NULL == t)
		t = frequency_tables;

	return first_table_by_country (ch, &t, country);
}

/*
 *  Return the RF channel with this name (e.g. "S3") in the
 *  same frequency table as the given channel.
 *
 *  tv_rf_channel_first_table_by_country (&ch, "US");
 *  tv_rf_channel_by_name (&ch, "7");
 *  tv_rf_channel_by_name (&ch, "13");
 *  tv_rf_channel_by_name (&ch, "S21");
 *
 *  XXX accept "S21_DE@ccir"?
 */
tv_bool
tv_rf_channel_by_name		(tv_rf_channel *	ch,
				 const char *		name)
{
	const struct table *t;
	const struct range *r;
	tv_rf_channel ch1;
	unsigned int i;

	t = ch->_table;

	for (r = t->freq_ranges; !IS_RANGE_END (r); r++) {
		for (i = r->first; i <= r->last; i++) {
			get_channel (&ch1, r, i - r->first);

			if (0 == strcmp (ch1.channel_name, name)) {
				strcpy (ch->channel_name, ch1.channel_name);
				ch->frequency = ch1.frequency;
				return TRUE;
			}
		}

		ch->_range++;
	}

	return FALSE;
}

tv_bool
tv_rf_channel_first		(tv_rf_channel *	ch)
{
	const struct table *t;

	t = ch->_table;

	get_channel (ch, t->freq_ranges, 0);

	return TRUE;
}

/*
 *  Return the next RF channel in the same frequency table.
 */
tv_bool
tv_rf_channel_next		(tv_rf_channel *	ch)
{
	const struct range *r;

	r = ch->_range;

	if ((r->first + ch->_channel + 1) > (r->last)) {
		++r;

		if (IS_RANGE_END (r))
			return FALSE;

		ch->_range += 1;
		ch->_channel = 0;
	} else {
		ch->_channel += 1;
	}

	get_channel (ch, r, ch->_channel);

	return TRUE;
}

/*
 *  Return the RF channel in the same frequency table, with this frequency.
 *  On success ch will change such that frequency lies between
 *  ch->frequency and ch->frequency + ch->bandwidth. Note frequency refers
 *  to the video carrier, not RF channel boundaries.
 */
tv_bool
tv_rf_channel_by_frequency	(tv_rf_channel *	ch,
				 unsigned int		frequency)
{
	const struct table *t;
	const struct range *r;
	unsigned int i;

	t = ch->_table;

	frequency /= 1000;

	for (r = t->freq_ranges; !IS_RANGE_END (r); r++) {
		if (frequency < r->frequency)
			continue;

		i = (frequency - r->frequency) / r->bandwidth;

		if (i + r->first > r->last)
			continue;

		get_channel (ch, r, i);

		return TRUE;
	}

	return FALSE;
}

/*
 *  Return the next country (in ch->country_code) using the
 *  frequency table this RF channel is in. The country
 *  is reset to the first one when switching tables, e.g. with
 *  tv_rf_channel_next_table ().
 *
 *  Note a country may use more than one table. To traverse
 *  tables by countries use tv_rf_channel_first|next_table_by_country().
 */
tv_bool
tv_rf_channel_next_country	(tv_rf_channel *	ch)
{
	const struct table *t;
	const char *s = "";

	t = ch->_table;

	for (s = t->countries; *s; s += 2)
		if (s[0] == ch->country_code[0]
		    && s[1] == ch->country_code[1])
			break;

	if (s[0] && s[2]) {
		ch->country_code[0] = s[2];
		ch->country_code[1] = s[3];
		ch->country_code[2] = 0;
		return TRUE;
	}

	return FALSE;
}

/*
 *  Tuned channels
 */

gboolean
tveng_tuned_channel_set_control	(tveng_tuned_channel *	tc,
				 const gchar *		name,
				 gfloat			value)
{
  tveng_tc_control *tcc;
  guint i;

  for (i = 0; i < tc->num_controls; ++i)
    if (0 == strncmp (tc->controls[i].name, name,
		      sizeof (tc->controls[i].name) - 1))
      {
	tc->controls[i].value = value;
	return TRUE;
      }

  tcc = g_realloc (tc->controls, (i + 1) * sizeof (*tc->controls));

  if (NULL == tcc)
    return FALSE;

  tc->controls = tcc;
  tc->num_controls = i + 1;

  g_strlcpy (tcc[i].name, name, 32);
  tcc[i].value = value;

  return TRUE;
}

tveng_tuned_channel *
tveng_tuned_channel_first	(const tveng_tuned_channel *list)
{
  if (!list)
    return NULL;

  while (list->prev)
    list = list->prev;

  return (tveng_tuned_channel *) list;
}

tveng_tuned_channel *
tveng_tuned_channel_nth		(const tveng_tuned_channel *list,
				 guint			index)
{
  if (!list)
    return NULL;

  if (list->index > index)
    list = tveng_tuned_channel_first (list);

  while (list && list->index != index)
    list = list->next;

  return (tveng_tuned_channel *) list;
}

static tveng_tuned_channel *
tveng_tuned_channel_by_string	(tveng_tuned_channel *	list,
				 const gchar *		name,
				 guint			offset)
{
  tveng_tuned_channel *tc;
  gchar *s, *t;

  if (!name || !list)
    return NULL;

  g_assert (list->prev == NULL);

  t = g_utf8_casefold (name, -1);
  s = g_utf8_normalize (t, -1, G_NORMALIZE_DEFAULT);
  g_free (t);

  for (tc = list; tc; tc = tc->next)
    {
      t = g_utf8_casefold (* (const gchar **)(((char *) tc) + offset), -1);

      if (0 == strcmp (s, t))
	{
	  g_free (t);
	  g_free (s);
	  return tc;
	}

      g_free (t);
    }

  g_free (s);

  return NULL;
}

tveng_tuned_channel *
tveng_tuned_channel_by_name	(tveng_tuned_channel *	list,
				 const gchar *		name)
{
  return tveng_tuned_channel_by_string
    (list, name, offsetof (tveng_tuned_channel, name));
}

tveng_tuned_channel *
tveng_tuned_channel_by_rf_name	(tveng_tuned_channel *	list,
				 const gchar *		rf_name)
{
  return tveng_tuned_channel_by_string
    (list, rf_name, offsetof (tveng_tuned_channel, rf_name));
}

void
tveng_tuned_channel_insert_replace
				(tveng_tuned_channel **	list,
				 tveng_tuned_channel *	tc,
				 guint			index,
				 gboolean		replace)
{
  tveng_tuned_channel *tci;

  g_assert (list != NULL);
  g_assert (tc != NULL);

  if (*list == NULL)
    {
      if (replace && index > 0)
	{
	  *list = tveng_tuned_channel_new (NULL);
	}
      else
	{
	  tc->next = NULL;
	  tc->prev = NULL;
	  tc->index = 0;

	  *list = tc;

	  return;
	}
    }

  for (tci = tveng_tuned_channel_first (*list);
       tci->index < index; tci = tci->next)
    if (!tci->next)
      {
	if (replace && (tci->index + 1) < index)
	  {
	    tveng_tuned_channel *ntc;

	    ntc = tveng_tuned_channel_new (NULL);

	    ntc->prev = tci;
	    ntc->index = tci->index + 1;

	    tci->next = ntc;
	  }
	else
	  {
	    tc->next = NULL;
	    tc->prev = tci;
	    tc->index = tci->index + 1;

	    tci->next = tc;

	    return;
	  }
      }

  if (replace && tci->index == index)
    {
      tc->prev = tci->prev;
      tc->next = tci->next;
      tc->index = tci->index;

      tc->prev->next = tc;
      tc->next->prev = tc;

      tveng_tuned_channel_delete (tci);
    }
  else /* insert before tci */
    {
      if (tci->prev)
	{
	  index = tci->prev->index + 1;
	  tci->prev->next = tc;
	}
      else
	{
	  index = 0;
	  *list = tc;
	}
      
      tc->prev = tci->prev;
      tc->next = tci;
      
      tci->prev = tc;
      
      for (; tc; tc = tc->next)
	tc->index = index++;
    }
}

void
tveng_tuned_channel_remove	(tveng_tuned_channel **	list,
				 tveng_tuned_channel *	tc)
{
  guint index;

  if (!list || !tc)
    return;

  g_assert (*list != NULL);
  g_assert ((*list)->prev == NULL);

  if (*list == tc)
    *list = tc->next;

  if (tc->prev)
    {
      index = tc->prev->index + 1;
      tc->prev->next = tc->next;
    }
  else
    {
      index = 0;
    }

  if (tc->next)
    {
      tc->next->prev = tc->prev;

      for (tc = tc->next; tc; tc = tc->next)
	tc->index = index++;
    }
}

void
tveng_tuned_channel_move	(tveng_tuned_channel **	list,
				 tveng_tuned_channel *	tc,
				 guint			new_index)
{
  if (!list || !*list || (!tc->next && !tc->prev))
    return;

  g_assert ((*list)->prev == NULL);

  /* XXX should not change tc */

  tveng_tuned_channel_insert (list, tveng_tuned_channel_new (tc), new_index);
  tveng_tuned_channel_remove (list, tc);
}

void
tveng_tuned_channel_copy	(tveng_tuned_channel *	d,
				 const tveng_tuned_channel *s)
{
  g_assert (s != NULL);
  g_assert (d != NULL);

  g_free (d->name);
  g_free (d->rf_name);
  g_free (d->rf_table);
  g_free (d->controls);

  d->name		= g_utf8_normalize (s->name ? s->name : "",
					    -1, G_NORMALIZE_DEFAULT_COMPOSE);
  d->rf_name		= g_utf8_normalize (s->rf_name ? s->rf_name : "",
					    -1, G_NORMALIZE_DEFAULT_COMPOSE);
  d->rf_table		= g_utf8_normalize (s->rf_table ? s->rf_table : "",
					    -1, G_NORMALIZE_DEFAULT_COMPOSE);
  d->accel		= s->accel;
  d->frequ		= s->frequ;
  d->input		= s->input;
  d->standard		= s->standard;
  d->num_controls	= s->num_controls;

  if (s->num_controls > 0)
    d->controls = g_memdup (s->controls,
      d->num_controls * sizeof (*(d->controls)));
  else
    d->controls = NULL;

  d->caption_pgno	= s->caption_pgno;
}

tveng_tuned_channel *
tveng_tuned_channel_new		(const tveng_tuned_channel *tc)
{
  tveng_tuned_channel *new_tc;
  tveng_tuned_channel empty_tc;

  new_tc = g_malloc0 (sizeof (*new_tc));

  if (!tc)
    {
      CLEAR (empty_tc);

      empty_tc.name = "";
      empty_tc.rf_name = "";
      empty_tc.rf_table = "";

      tc = &empty_tc;
    }

  tveng_tuned_channel_copy (new_tc, tc);

  return new_tc;
}

void
tveng_tuned_channel_delete	(tveng_tuned_channel *	tc)
{
  if (!tc)
    return;

  g_free (tc->name);
  g_free (tc->rf_name);
  g_free (tc->rf_table);
  g_free (tc->controls);
  g_free (tc);
}

tveng_tuned_channel *
tveng_tuned_channel_list_new	(tveng_tuned_channel *	list)
{
  tveng_tuned_channel *new_list;
  tveng_tuned_channel *tc_prev;
  tveng_tuned_channel *tc;
  guint index = 1;

  if (!list)
    return NULL;

  new_list = tveng_tuned_channel_new (list);
  new_list->index = 0;

  tc_prev = new_list;
  list = list->next;

  while (list)
    {
      tc = tveng_tuned_channel_new (list);
      tc->index = index++;

      tc_prev->next = tc;
      tc->prev = tc_prev;

      tc_prev = tc;
      list = list->next;
    }

  return new_list;
}

void
tveng_tuned_channel_list_delete	(tveng_tuned_channel **	list)
{
  if (!list ||!*list)
    return;

  g_assert ((*list)->prev == NULL);

  while (*list)
    {
      tveng_tuned_channel *tc = (*list)->next;

      tveng_tuned_channel_delete (*list);

      *list = tc;
    }
}

gboolean
tveng_tuned_channel_in_list	(tveng_tuned_channel *	list,
				 tveng_tuned_channel *	tc)
{
  if (!list || !tc)
    return FALSE;

  g_assert (list->prev == NULL);

  return (list == tveng_tuned_channel_first (tc));
}




/*
  Returns the number of items in the list
*/
int
tveng_tuned_channel_num (const tveng_tuned_channel * list)
{
  int num_channels = 0;
  tveng_tuned_channel * tc_ptr = tveng_tuned_channel_first (list);

  while (tc_ptr)
    {
      num_channels++;
      tc_ptr = tc_ptr -> next;
    }

  return (num_channels);
}

/*
  Removes an specific channel form the list. You must provide its
  the radio frequency name, e.g. "64" instead of "Tele5". Returns -1
  if the channel could not be found. If rf_name is NULL, then id is
  interpreted as the index in the tuned_channel list. Then -1 means
  out of bounds. if rf_name is not NULL, then the first matching
  item from id is deleted.
*/
tveng_tuned_channel *
tveng_remove_tuned_channel (gchar * rf_name, int id,
			    tveng_tuned_channel * list)
{
  tveng_tuned_channel *tc;

  if (!list)
    return NULL;

  tc = tveng_tuned_channel_nth (list, id);

  if (rf_name)
    tc = tveng_tuned_channel_by_rf_name (tc, rf_name); /* sic, >= id */

  if (tc)
    {
      tveng_tuned_channel_remove (&list, tc);
      tveng_tuned_channel_delete (tc);

      return tveng_tuned_channel_first (list);
    }

  return list;
}

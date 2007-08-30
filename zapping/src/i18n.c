/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2001 Iñaki García Etxebarria
 * Copyright (C) 2003, 2004 Michael H. Schimek
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

/* $Id: i18n.c,v 1.5 2007-08-30 14:14:34 mschimek Exp $ */

/* Shouldn't we have a library for these kinds of functions? */

#include <string.h>
#include <ctype.h>
#include <locale.h>

#include "i18n.h"

static const struct {
	const char		code[4];
	const char *		name;
} country_table [] =
{
	{ "AL", "Albania" },
	{ "AT", "Austria" },
	{ "AU", "Australia" },
	{ "BA", "Bosnia and Herzegovina" },
	{ "BE", "Belgium" },
	{ "BG", "Bulgaria" },
	{ "BJ", "Benin" },
	{ "CA", "Canada" },
	{ "CH", "Switzerland" },
	{ "CN", "China" },
	{ "CZ", "Czech Republic" },
	{ "DE", "Germany" },
	{ "DK", "Denmark" },
	{ "ES", "Spain" },
	{ "FI", "Finland" },
	{ "FR", "France" },
	{ "GR", "Greece" },
	{ "HR", "Croatia" },
	{ "HU", "Hungary" },
	{ "ID", "Indonesia" },
	{ "IE", "Ireland" },
	{ "IN", "India" },
	{ "IS", "Iceland" },
	{ "IT", "Italy" },
	{ "JP", "Japan" },
	{ "KH", "Cambodia" },
	{ "MK", "Macedonia" },
	{ "MY", "Malaysia" },
	{ "NL", "Netherlands" },
	{ "NO", "Norway" },
	{ "NZ", "New Zealand" },
	{ "PK", "Pakistan" },
	{ "PL", "Poland" },
	{ "PT", "Portugal" },
	{ "RO", "Romania" },
	{ "RU", "Russia" },
	{ "SE", "Sweden" },
	{ "SG", "Singapore" },
	{ "SK", "Slovakia" },
	{ "TH", "Thailand" },
	{ "UK", "United Kingdom" },
	{ "US", "United States" },
	{ "YU", "Yugoslavia" },
	{ "ZA", "South Africa" },
	{ "", NULL }
};

const char *
iso3166_to_country_name		(const char *		code)
{
	unsigned int i;

	if (code[0] == 0 || code[1] == 0)
		return NULL;

	for (i = 0; country_table[i].name; i++)
		if (country_table[i].code[0] == code[0]
		    && country_table[i].code[1] == code[1])
			return country_table[i].name;

	return NULL;
}

const char *
country_name_to_iso3166		(const char *		name)
{
	unsigned int i;

	for (i = 0; country_table[i].name; i++)
		if (0 == strcmp (country_table[i].name, name))
			return country_table[i].code;

	return NULL;
}

const char *
locale_country			(void)
{
	char *s;
	unsigned int i;

	/* language[_territory][.codeset][@modifier]
	   [+special][,[sponsor][_revision]] */

	s = setlocale (LC_CTYPE, NULL);

	if (!s)
		return NULL;

	while (*s && isalnum (*s))
		s++;

	if (*s++ != '_')
		return NULL;

	if (s[0] == 0 || s[1] == 0 || isalnum (s[2]))
		return NULL;

	for (i = 0; country_table[i].name; i++)
		if (country_table[i].code[0] == s[0]
		    && country_table[i].code[1] == s[1])
			return country_table[i].code;

	/*
	    Other possible source:
	    /etc/localtime -> /usr/share/zoneinfo/foo
	    /usr/share/zoneinfo/zone.tab
	 */

	return NULL;
}

static const struct {
	const char		code[4];
	const char *		name;
} language_table [] =
{
	{ "ar", "Arabic" },
	{ "bg", "Bulgarian" },
	{ "cs", "Czech" },
	{ "de", "German" },
	{ "el", "Greek" },
	{ "en", "English" },
	{ "es", "Spanish" },
	{ "et", "Estonian" },
	{ "fi", "Finnish" },
	{ "fr", "French" },
	{ "sv", "Swedish" },
	{ "he", "Hebrew" },
	{ "hr", "Croatian" },
	{ "hu", "Hungarian" },
	{ "it", "Italian" },
	{ "lt", "Lithuanian" },
	{ "lv", "Lettish" },
	{ "pl", "Polish" },
	{ "pt", "Portuguese" },
	{ "ro", "Rumanian" },
	{ "ru", "Russian" },
	{ "sk", "Slovak" },
	{ "sl", "Slovenian" },
	{ "sr", "Serbian" },
	{ "tr", "Turkish" },
	{ "uk", "Ukranian" },
	{ "", NULL }
};

const char *
iso639_to_language_name		(const char *		code)
{
	unsigned int i;

	if (code[0] == 0 || code[1] == 0)
		return NULL;

	for (i = 0; language_table[i].name; i++)
		if (language_table[i].code[0] == code[0]
		    && language_table[i].code[1] == code[1])
			return language_table[i].name;

	return NULL;
}

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/

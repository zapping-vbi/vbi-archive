/*
 *  Preliminary helper.
 */

/* $Id: i18n.c,v 1.1.2.1 2003-01-08 16:42:05 mschimek Exp $ */

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
	{ "IE", "Ireland" },
	{ "IS", "Iceland" },
	{ "IT", "Italy" },
	{ "JP", "Japan" },
	{ "MK", "Macedonia" },
	{ "NL", "Netherlands" },
	{ "NO", "Norway" },
	{ "NZ", "New Zealand" },
	{ "PK", "Pakistan" },
	{ "PL", "Poland" },
	{ "PT", "Portugal" },
	{ "RO", "Romania" },
	{ "RU", "Russia" },
	{ "SE", "Sweden" },
	{ "SK", "Slovakia" },
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

/*
 *  Preliminary helper.
 */

/* $Id: i18n.h,v 1.1.2.1 2003-01-08 16:42:06 mschimek Exp $ */

#ifndef I18N_H
#define I18N_H

extern const char *
iso3166_to_country_name		(const char *		code);
extern const char *
country_name_to_iso3166		(const char *		name);
extern const char *
locale_country			(void);

#endif

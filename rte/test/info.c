/*
 *  librte test
 *
 *  Copyright (C) 2000, 2001, 2002 Iñaki García Etxebarria
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: info.c,v 1.12 2002-09-12 12:26:15 mschimek Exp $ */

#undef NDEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <getopt.h>

#include "librte.h"

#ifndef _
#ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) gettext (String)
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif
#endif

rte_bool check = FALSE;
rte_bool brief = FALSE;

#define TYPE_STR(type) case type : type_str = #type ; break

#define INT_TYPE(oi)    ((oi)->type == RTE_OPTION_BOOL			\
			|| (oi)->type == RTE_OPTION_INT			\
			|| (oi)->type == RTE_OPTION_MENU)

#define REAL_TYPE(oi)   ((oi)->type == RTE_OPTION_REAL)

#define MENU_TYPE(oi)   ((oi)->menu.str != NULL)

#define ASSERT_ERRSTR(expr)						\
do {									\
	if (!(expr)) {							\
		printf("Assertion '" #expr "' failed; errstr=\"%s\"\n",	\
		        rte_errstr(cx));				\
		exit(EXIT_FAILURE);					\
	}								\
} while (0)

#define BOUNDS_CHECK(type)						\
do {									\
	if (oi->menu.type) {						\
		assert(oi->def.num >= 0);				\
		assert(oi->def.num <= oi->max.num);			\
		assert(oi->min.num == 0);				\
		assert(oi->max.num > 0);				\
		assert(oi->step.num == 1);				\
	} else {							\
	      	assert(oi->max.type >= oi->min.type);			\
		assert(oi->step.type > 0);				\
		assert(oi->def.type >= oi->min.type			\
		       && oi->def.type <= oi->max.type);		\
	}								\
} while (0)

static void
keyword_check(const char *keyword)
{
	int i, l;

	assert(keyword != NULL);
	l = strlen(keyword);
	assert(strlen(keyword) > 0);

	for (i = 0; i < l; i++) {
		if (isalnum(keyword[i]))
			continue;
		if (strchr("_", keyword[i]))
			continue;
		fprintf(stderr, "Bad keyword: '%s'\n", keyword);
		exit(EXIT_FAILURE);
	}
}

static void
print_current(rte_option_info *oi, rte_option_value current)
{
	if (REAL_TYPE(oi)) {
		printf("    current value=%g ", current.dbl);
		if (!oi->menu.dbl
		    && (current.dbl < oi->min.dbl
			|| current.dbl > oi->max.dbl))
			printf("** WARNING: out of bounds");
	} else {
		printf("    current value=%d ", current.num);
		if (!oi->menu.num
		    && (current.num < oi->min.num
			|| current.num > oi->max.num))
			printf("** WARNING: out of bounds");
	}
	printf("\n");
}

static void
test_modified(rte_option_info *oi, rte_option_value old, rte_option_value new)
{
	if (REAL_TYPE(oi)) {
		if (old.dbl != new.dbl) {
			printf("but modified current value to %g\n", new.dbl);
			exit(EXIT_FAILURE);
		}
	} else {
		if (old.num != new.num) {
			printf("but modified current value to %d\n", new.num);
			exit(EXIT_FAILURE);
		}
	}
}

static void
test_set_int(rte_context *cx, rte_codec *cc, rte_option_info *oi,
	     rte_option_value *current, int value)
{
	rte_option_value new_current;
	rte_bool r, r2;

	printf("    try to set %d: ", value);
	if (cc)
		r = rte_codec_option_set(cc, oi->keyword, value);
	else
		r = rte_context_option_set(cx, oi->keyword, value);

	if (r)
		printf("success.");
	else
		printf("failed, errstr=\"%s\".", rte_errstr(cx));

	new_current.num = 0x54321;

	if (cc)
		r2 = rte_codec_option_get(cc, oi->keyword, &new_current);
	else
		r2 = rte_context_option_get(cx, oi->keyword, &new_current);

	if (!r2) {
		printf("rte_*_option_get failed, errstr==\"%s\"\n",
		       rte_errstr(cx));
		if (new_current.num != 0x54321)
			printf("but modified destination to %d\n",
			       new_current.num);
		exit(EXIT_FAILURE);
	}

	if (!r)
		test_modified(oi, *current, new_current);

	print_current(oi, *current = new_current);
}

static void
test_set_real(rte_context *cx, rte_codec *cc, rte_option_info *oi,
	      rte_option_value *current, double value)
{
	rte_option_value new_current;
	rte_bool r, r2;

	printf("    try to set %g: ", value);
	if (cc)
		r = rte_codec_option_set(cc, oi->keyword, value);
	else
		r = rte_context_option_set(cx, oi->keyword, value);

	if (r)
		printf("success.");
	else
		printf("failed, errstr=\"%s\".", rte_errstr(cx));

	new_current.dbl = 8192.0;

	if (cc)
		r2 = rte_codec_option_get(cc, oi->keyword, &new_current);
	else
		r2 = rte_context_option_get(cx, oi->keyword, &new_current);

	if (!r2) {
		printf("rte_*_option_get failed, errstr==\"%s\"\n",
		       rte_errstr(cx));
		if (new_current.dbl != 8192.0)
			printf("but modified destination to %g\n",
			       new_current.dbl);
		exit(EXIT_FAILURE);
	}

	if (!r)
		test_modified(oi, *current, new_current);

	print_current(oi, *current = new_current);
}

static void
test_set_entry(rte_context *cx, rte_codec *cc, rte_option_info *oi,
	       rte_option_value *current, int entry)
{
	rte_option_value new_current;
	int new_entry;
	rte_bool r0, r1;
	rte_bool valid;

	valid = (MENU_TYPE(oi)
		 && entry >= oi->min.num
		 && entry <= oi->max.num);

	printf("    try to set menu entry %d: ", entry);
	if (cc)
		r0 = rte_codec_option_menu_set(cc, oi->keyword, entry);
	else
		r0 = rte_context_option_menu_set(cx, oi->keyword, entry);

	switch (r0 = r0 * 2 + valid) {
	case 0:
		printf("failed as expected, errstr=\"%s\".", rte_errstr(cx));
		break;
	case 1:
		printf("failed, errstr=\"%s\".", rte_errstr(cx));
		break;
	case 2:
		printf("** WARNING unexpected success.");
		break;
	default:
		printf("success.");
	}

	if (cc)
		ASSERT_ERRSTR(rte_codec_option_get(cc, oi->keyword, &new_current));
	else
		ASSERT_ERRSTR(rte_context_option_get(cx, oi->keyword, &new_current));
	if (r0 == 0 || r0 == 1)
		test_modified(oi, *current, new_current);

	valid = MENU_TYPE(oi);

	new_entry = 0x33333;
	if (cc)
		r1 = rte_codec_option_menu_get(cc, oi->keyword, &new_entry);
	else
		r1 = rte_context_option_menu_get(cx, oi->keyword, &new_entry);

	switch (r1 = r1 * 2 + valid) {
	case 1:
		printf("\nrte_export_option_menu_get failed, errstr==\"%s\"\n",
		       rte_errstr(cx));
		break;
	case 2:
		printf("\nrte_export_option_menu_get: unexpected success.\n");
		break;
	default:
		printf("\n");
		break;
	}

	if ((r1 == 0 || r1 == 1) && new_entry != 0x33333) {
		printf("rte_export_option_menu_get failed, "
		       "but modified destination to %d\n",
		       new_current.num);
		exit(EXIT_FAILURE);
	}

	if (r0 == 1 /* || r0 == 2 */ || r1 == 1 || r1 == 2)
		exit(EXIT_FAILURE);

	switch (oi->type) {
	case RTE_OPTION_BOOL:
	case RTE_OPTION_INT:
		if (oi->menu.num)
			assert(new_current.num == oi->menu.num[new_entry]);
		else
			test_modified(oi, *current, new_current);
		print_current(oi, *current = new_current);
		break;

	case RTE_OPTION_REAL:
		if (oi->menu.dbl)
			assert(new_current.dbl == oi->menu.dbl[new_entry]);
		else
			test_modified(oi, *current, new_current);
		print_current(oi, *current = new_current);
		break;

	case RTE_OPTION_MENU:
		print_current(oi, *current = new_current);
		break;

	default:
		assert(!"reached");
		break;
	}
}

static void
show_option_info(rte_context *cx, rte_codec *cc, rte_option_info *oi)
{
	rte_option_value val;
	char *type_str;
	int i;

	switch (oi->type) {
	TYPE_STR(RTE_OPTION_BOOL);
	TYPE_STR(RTE_OPTION_INT);
	TYPE_STR(RTE_OPTION_REAL);
	TYPE_STR(RTE_OPTION_STRING);
	TYPE_STR(RTE_OPTION_MENU);
	default:
		printf("    * Option %s has invalid type %d\n",
		       oi->keyword, oi->type);
		exit(EXIT_FAILURE);
	}

	printf("    * type=%s keyword=%s label=\"%s\" tooltip=\"%s\"\n",
	       type_str, oi->keyword, _(oi->label), _(oi->tooltip));

	keyword_check(oi->keyword);

	switch (oi->type) {
	case RTE_OPTION_BOOL:
	case RTE_OPTION_INT:
		BOUNDS_CHECK(num);
		if (oi->menu.num) {
			printf("      %d menu entries, default=%d: ",
			       oi->max.num - oi->min.num + 1, oi->def.num);
			for (i = oi->min.num; i <= oi->max.num; i++)
				printf("%d%s", oi->menu.num[i],
				       (i < oi->max.num) ? ", " : "");
			printf("\n");
		} else
			printf("      default=%d, min=%d, max=%d, step=%d\n",
			       oi->def.num, oi->min.num, oi->max.num, oi->step.num);

		if (cc)
			ASSERT_ERRSTR(rte_codec_option_get
				      (cc, oi->keyword, &val));
		else
			ASSERT_ERRSTR(rte_context_option_get
				      (cx, oi->keyword, &val));
		print_current(oi, val);
		if (check) {
			if (oi->menu.num) {
				test_set_entry(cx, cc, oi, &val, oi->min.num);
				test_set_entry(cx, cc, oi, &val, oi->max.num);
				test_set_entry(cx, cc, oi, &val, oi->min.num - 1);
				test_set_entry(cx, cc, oi, &val, oi->max.num + 1);
				test_set_int(cx, cc, oi, &val, oi->menu.num[oi->min.num]);
				test_set_int(cx, cc, oi, &val, oi->menu.num[oi->max.num]);
				test_set_int(cx, cc, oi, &val, oi->menu.num[oi->min.num] - 1);
				test_set_int(cx, cc, oi, &val, oi->menu.num[oi->max.num] + 1);
			} else {
				test_set_entry(cx, cc, oi, &val, 0);
				test_set_int(cx, cc, oi, &val, oi->min.num);
				test_set_int(cx, cc, oi, &val, oi->max.num);
				test_set_int(cx, cc, oi, &val, oi->min.num - 1);
				test_set_int(cx, cc, oi, &val, oi->max.num + 1);
			}
		}
		break;

	case RTE_OPTION_REAL:
		BOUNDS_CHECK(dbl);
		if (oi->menu.dbl) {
			printf("      %d menu entries, default=%d: ",
			       oi->max.num - oi->min.num  + 1, oi->def.num);
			for (i = oi->min.num; i <= oi->max.num; i++)
				printf("%g%s", oi->menu.dbl[i],
				       (i < oi->max.num) ? ", " : "");
		} else
			printf("      default=%g, min=%g, max=%g, step=%g\n",
			       oi->def.dbl, oi->min.dbl, oi->max.dbl, oi->step.dbl);
		if (cc)
			ASSERT_ERRSTR(rte_codec_option_get
				      (cc, oi->keyword, &val));
		else
			ASSERT_ERRSTR(rte_context_option_get
				      (cx, oi->keyword, &val));
		print_current(oi, val);
		if (check) {
			if (oi->menu.num) {
				test_set_entry(cx, cc, oi, &val, oi->min.num);
				test_set_entry(cx, cc, oi, &val, oi->max.num);
				test_set_entry(cx, cc, oi, &val, oi->min.num - 1);
				test_set_entry(cx, cc, oi, &val, oi->max.num + 1);
				test_set_real(cx, cc, oi, &val, oi->menu.dbl[oi->min.num]);
				test_set_real(cx, cc, oi, &val, oi->menu.dbl[oi->max.num]);
				test_set_real(cx, cc, oi, &val, oi->menu.dbl[oi->min.num] - 1);
				test_set_real(cx, cc, oi, &val, oi->menu.dbl[oi->max.num] + 1);
			} else {
				test_set_entry(cx, cc, oi, &val, 0);
				test_set_real(cx, cc, oi, &val, oi->min.dbl);
				test_set_real(cx, cc, oi, &val, oi->max.dbl);
				test_set_real(cx, cc, oi, &val, oi->min.dbl - 1);
				test_set_real(cx, cc, oi, &val, oi->max.dbl + 1);
			}
		}
		break;

	case RTE_OPTION_STRING:
		if (oi->menu.str) {
			BOUNDS_CHECK(str);
			printf("      %d menu entries, default=%d: ",
			       oi->max.num - oi->min.num  + 1, oi->def.num);
			for (i = oi->min.num; i <= oi->max.num; i++)
				printf("%s%s", oi->menu.str[i],
				       (i < oi->max.num) ? ", " : "");
		} else
			printf("      default=\"%s\"\n", oi->def.str);
		if (cc)
			ASSERT_ERRSTR(rte_codec_option_get
				      (cc, oi->keyword, &val));
		else
			ASSERT_ERRSTR(rte_context_option_get
				      (cx, oi->keyword, &val));
		printf("      current value=\"%s\"\n", val.str);
		assert(val.str);
		free(val.str);
		if (check) {
			rte_bool r;

			printf("      try to set \"foobar\": ");
			if (cc)
				r = rte_codec_option_set(cc, oi->keyword, "foobar");
			else
				r = rte_context_option_set(cx, oi->keyword, "foobar");
			if (r)
				printf("success.");
			else
				printf("failed, errstr=\"%s\".",
				       rte_errstr(cx));
			if (cc)
				ASSERT_ERRSTR(rte_codec_option_get
					      (cc, oi->keyword, &val));
			else
				ASSERT_ERRSTR(rte_context_option_get
					      (cx, oi->keyword, &val));
			printf("    current value=\"%s\"\n", val.str);
			assert(val.str);
			free(val.str);
		}
		break;

	case RTE_OPTION_MENU:
		printf("      %d menu entries, default=%d: ",
		       oi->max.num - oi->min.num + 1, oi->def.num);
		for (i = oi->min.num; i <= oi->max.num; i++) {
			assert(oi->menu.str[i] != NULL);
			printf("%s%s", _(oi->menu.str[i]),
			       (i < oi->max.num) ? ", " : "");
		}
		printf("\n");
		if (cc)
			ASSERT_ERRSTR(rte_codec_option_get
				      (cc, oi->keyword, &val));
		else
			ASSERT_ERRSTR(rte_context_option_get
				      (cx, oi->keyword, &val));
		print_current(oi, val);
		if (check) {
			test_set_entry(cx, cc, oi, &val, oi->min.num);
			test_set_entry(cx, cc, oi, &val, oi->max.num);
			test_set_entry(cx, cc, oi, &val, oi->min.num - 1);
			test_set_entry(cx, cc, oi, &val, oi->max.num + 1);
		}
		break;

	default:
		assert(!"reached");
		break;
	}
}

#define STREAM_TYPE_STR(type) case type : type_str = #type ; break

static void
show_codec_info (rte_context *context, rte_codec_info *ci)
{
	rte_codec *codec;
	char *type_str;
	rte_option_info *oi;
	int i;

	switch (ci->stream_type) {
	STREAM_TYPE_STR(RTE_STREAM_VIDEO);
	STREAM_TYPE_STR(RTE_STREAM_AUDIO);
	STREAM_TYPE_STR(RTE_STREAM_RAW_VBI);
	STREAM_TYPE_STR(RTE_STREAM_SLICED_VBI);
	default:
		printf("Codec %s has invalid stream type %d\n",
		       ci->keyword, ci->stream_type);
		exit(EXIT_FAILURE);
	}

	printf("\tKeyword:\t%s\tLabel:\t\"%s\"\n", ci->keyword, _(ci->label));

	if (!brief) {
		printf("\tStream type:\t%s\n", type_str);
		printf("\tTooltip:\t\"%s\"\n", _(ci->tooltip));
		printf("\tAvailable options:\n");

		keyword_check(ci->keyword);

		codec = rte_set_codec(context, ci->keyword, 0, NULL);

		if (!codec) {
			fprintf(stderr, "\tCannot create/set codec %s: %s\n",
				ci->keyword, rte_errstr(context));
			exit(EXIT_FAILURE);
		}

		for (i = 0; (oi = rte_codec_option_info_enum(codec, i)); i++)
			show_option_info(context, codec, oi);

		printf("\n");
	}

	rte_remove_codec(context, ci->stream_type, 0);
}

static void
show_context_info (rte_context_info *ci)
{
	rte_context *context;
	rte_codec_info *di;
	rte_option_info *oi;
	char *errstr;
	int i;

	printf("* keyword %s label \"%s\"\n", ci->keyword, _(ci->label));

	if (!brief) {
		printf("Backend:\t%s\n", ci->backend);
		printf("Tooltip:\t\"%s\"\n", _(ci->tooltip));
		printf("Mime types:\t%s\n", ci->mime_type);
		printf("Extension:\t%s\n", ci->extension);
		printf("Audio elementary:\t%d ... %d\n",
		       ci->min_elementary[RTE_STREAM_AUDIO],
		       ci->max_elementary[RTE_STREAM_AUDIO]);
		printf("Video elementary:\t%d ... %d\n",
		       ci->min_elementary[RTE_STREAM_VIDEO],
		       ci->max_elementary[RTE_STREAM_VIDEO]);
		printf("VBI elementary:\t\t%d ... %d\n",
		       ci->min_elementary[RTE_STREAM_SLICED_VBI],
		       ci->max_elementary[RTE_STREAM_SLICED_VBI]);

		keyword_check(ci->keyword);

		printf("Available options:\n");
	}

	if (!(context = rte_context_new(ci->keyword, &errstr, NULL))) {
		fprintf(stderr, "Unable to create context:\n%s", errstr);
		exit(EXIT_FAILURE);
	}

	if (!brief) {
		for (i = 0; (oi = rte_context_option_info_enum(context, i)); i++)
			show_option_info(context, NULL, oi);
	}

	printf("Codecs:\n");

	for (i = 0; (di = rte_codec_info_enum(context, i)); i++)
		show_codec_info(context, di);

	rte_context_delete(context);
}

static void
show_contexts (void)
{
	rte_context_info *ci;
	int i;

	for (i = 0; (ci = rte_context_info_enum(i)); i++)
		show_context_info(ci);

	puts("-- end of list --");
}

static const char *short_options = "cb";

static const struct option
long_options[] = {
	{ "check", no_argument, NULL, 'c' },
	{ "brief", no_argument, NULL, 'b' },
	{ 0, 0, 0, 0 }
};

int
main (int argc, char **argv)
{
	int c;

	printf("Welcome to Unimatrix 5, tertiary subunit of Zapping 6.\n\n");

	while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1)
		switch (c) {
		case 'c':
			check = TRUE;
			break;

		case 'b':
			brief = TRUE;
			break;

		default:
			fprintf(stderr, "Unknown option\n");
			exit(EXIT_FAILURE);
		}

	show_contexts();

	exit(EXIT_SUCCESS);
}

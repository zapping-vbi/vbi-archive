/*
 *  libzvbi test
 *
 *  Copyright (C) 2000, 2001 Michael H. Schimek
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

/* $Id: explist.c,v 1.7.2.5 2008-03-01 15:51:12 mschimek Exp $ */

#undef NDEBUG

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#include <locale.h>
#include <libintl.h>

#include "src/zvbi.h"

vbi3_bool check = FALSE;

#define IS_REAL_TYPE(oi) (VBI3_OPTION_REAL == (oi)->type)
#define IS_MENU_TYPE(oi) (NULL != (oi)->menu.str)

#define ASSERT_ERRSTR(expr)						\
do {									\
	if (!(expr)) {							\
		printf("Assertion '" #expr "' failed; errstr=\"%s\"\n",	\
		        vbi3_export_errstr (e));				\
		exit (EXIT_FAILURE);					\
	}								\
} while (0)

#define BOUNDS_CHECK(menu_memb, memb)					\
do {									\
	if (oi->menu.menu_memb) {					\
		assert (oi->def.num >= 0);				\
		assert (oi->def.num <= oi->max.num);			\
		assert (oi->min.num == 0);				\
		assert (oi->max.num > 0);				\
		assert (oi->step.num == 1);				\
	} else {							\
	      	assert (oi->max.memb >= oi->min.memb);			\
		assert (oi->step.memb > 0);				\
		assert (oi->def.memb >= oi->min.memb			\
		        && oi->def.memb <= oi->max.memb);		\
	}								\
} while (0)

static void
keyword_check			(const char *		keyword)
{
	unsigned int i, l;

	assert (NULL != keyword);

	l = strlen (keyword);
	assert (l > 0);

	for (i = 0; i < l; ++i) {
		if (isalnum (keyword[i]))
			continue;
		if (strchr ("_", keyword[i]))
			continue;
		fprintf (stderr, "Bad keyword: '%s'\n", keyword);
		exit (EXIT_FAILURE);
	}
}

static void
print_current			(const vbi3_option_info *oi,
				 vbi3_option_value	current)
{
	if (IS_REAL_TYPE (oi)) {
		printf ("    current value=%f\n", current.dbl);
		if (!oi->menu.dbl)
			assert (current.dbl >= oi->min.dbl
				&& current.dbl <= oi->max.dbl);
	} else {
		printf ("    current value=%d\n", current.num);
		if (!oi->menu.num)
			assert (current.num >= oi->min.num
				&& current.num <= oi->max.num);
	}
}

static void
test_modified			(const vbi3_option_info *oi,
				 vbi3_option_value	old,
				 vbi3_option_value	new)
{
	if (IS_REAL_TYPE (oi)) {
		if (old.dbl != new.dbl) {
			printf ("but modified current value to %f\n", new.dbl);
			exit (EXIT_FAILURE);
		}
	} else {
		if (old.num != new.num) {
			printf ("but modified current value to %d\n", new.num);
			exit (EXIT_FAILURE);
		}
	}
}

static void
test_set_int			(vbi3_export *		e,
				 const vbi3_option_info *oi,
				 vbi3_option_value *	current,
				 int			value)
{
	vbi3_option_value new_current;
	vbi3_bool r;

	printf ("    try to set %d: ", value);
	r = vbi3_export_option_set (e, oi->keyword, value);

	if (r)
		printf ("success.");
	else
		printf ("failed, errstr=\"%s\".", vbi3_export_errstr (e));

	new_current.num = 0x54321;

	if (!vbi3_export_option_get (e, oi->keyword, &new_current)) {
		printf ("vbi3_export_option_get failed, errstr==\"%s\"\n",
			vbi3_export_errstr (e));

		if (new_current.num != 0x54321)
			printf ("but modified destination to %d\n",
				new_current.num);

		exit (EXIT_FAILURE);
	}

	if (!r)
		test_modified (oi, *current, new_current);

	print_current (oi, *current = new_current);
}

static void
test_set_real			(vbi3_export *		e,
				 const vbi3_option_info *oi,
				 vbi3_option_value *	current,
				 double			value)
{
	vbi3_option_value new_current;
	vbi3_bool r;

	printf ("    try to set %f: ", value);
	r = vbi3_export_option_set (e, oi->keyword, value);

	if (r)
		printf ("success.");
	else
		printf ("failed, errstr=\"%s\".", vbi3_export_errstr (e));

	new_current.dbl = 8192.0;

	if (!vbi3_export_option_get (e, oi->keyword, &new_current)) {
		printf ("vbi3_export_option_get failed, errstr==\"%s\"\n",
			vbi3_export_errstr (e));

		if (new_current.dbl != 8192.0)
			printf ("but modified destination to %f\n",
				new_current.dbl);

		exit (EXIT_FAILURE);
	}

	if (!r)
		test_modified (oi, *current, new_current);

	print_current (oi, *current = new_current);
}

#define test_set_values(type, memb)					\
do {									\
	test_set_##type (e, oi, &val, oi->min.memb);			\
	test_set_##type (e, oi, &val, oi->max.memb);			\
	test_set_##type (e, oi, &val, oi->min.memb - 1);		\
	test_set_##type (e, oi, &val, oi->max.memb + 1);		\
} while (0)

#define test_set_menus(type, memb)					\
do {									\
	test_set_##type (e, oi, &val, oi->menu.memb[oi->min.num]);	\
	test_set_##type (e, oi, &val, oi->menu.memb[oi->max.num]);	\
	test_set_##type (e, oi, &val, oi->menu.memb[oi->min.num] - 1);	\
	test_set_##type (e, oi, &val, oi->menu.memb[oi->max.num] + 1);	\
} while (0)

static void
test_set_entry			(vbi3_export *		e,
				 const vbi3_option_info *oi,
				 vbi3_option_value *	current,
				 int			entry)
{
	vbi3_option_value new_current;
	int new_entry;
	vbi3_bool r0, r1;
	vbi3_bool valid;

	valid = (IS_MENU_TYPE (oi)
		 && entry >= oi->min.num
		 && entry <= oi->max.num);

	printf ("    try to set menu entry %d: ", entry);
	r0 = vbi3_export_option_menu_set (e, oi->keyword, entry);

	switch (r0 = r0 * 2 + valid) {
	case 0:
		printf ("failed as expected, errstr=\"%s\".",
			vbi3_export_errstr (e));
		break;

	case 1:
		printf ("failed, errstr=\"%s\".", 
			vbi3_export_errstr (e));
		break;

	case 2:
		printf ("unexpected success.");
		break;

	default:
		printf ("success.");
	}

	ASSERT_ERRSTR (vbi3_export_option_get (e, oi->keyword, &new_current));

	if (r0 == 0 || r0 == 1)
		test_modified (oi, *current, new_current);

	valid = IS_MENU_TYPE (oi);

	new_entry = 0x33333;
	r1 = vbi3_export_option_menu_get (e, oi->keyword, &new_entry);

	switch (r1 = r1 * 2 + valid) {
	case 1:
		printf ("\nvbi3_export_option_menu_get failed, "
			"errstr==\"%s\"\n", vbi3_export_errstr (e));
		break;

	case 2:
		printf ("\nvbi3_export_option_menu_get: unexpected success.\n");
		break;

	default:
		break;
	}

	if ((r1 == 0 || r1 == 1) && new_entry != 0x33333) {
		printf ("vbi3_export_option_menu_get failed, "
			"but modified destination to %d\n",
			new_current.num);
		exit (EXIT_FAILURE);
	}

	if (r0 == 1 || r0 == 2 || r1 == 1 || r1 == 2)
		exit (EXIT_FAILURE);

	switch (oi->type) {
	case VBI3_OPTION_BOOL:
	case VBI3_OPTION_INT:
		if (oi->menu.num)
			assert (new_current.num == oi->menu.num[new_entry]);
		else
			test_modified (oi, *current, new_current);
		print_current (oi, *current = new_current);
		break;

	case VBI3_OPTION_REAL:
		if (oi->menu.dbl)
			assert (new_current.dbl == oi->menu.dbl[new_entry]);
		else
			test_modified (oi, *current, new_current);
		print_current (oi, *current = new_current);
		break;

	case VBI3_OPTION_MENU:
		print_current (oi, *current = new_current);
		break;

	default:
		assert (!"reached");
		break;
	}
}

#define test_set_entries()						\
do {									\
	test_set_entry (e, oi, &val, oi->min.num);			\
	test_set_entry (e, oi, &val, oi->max.num);			\
	test_set_entry (e, oi, &val, oi->min.num - 1);			\
	test_set_entry (e, oi, &val, oi->max.num + 1);			\
} while (0)

static void
dump_option_info		(vbi3_export *		e,
				 const vbi3_option_info *oi)
{
	vbi3_option_value val;
	const char *type_str;
	int i;

	switch (oi->type) {

#undef CASE
#define CASE(type) case type: type_str = #type; break;

	CASE (VBI3_OPTION_BOOL);
	CASE (VBI3_OPTION_INT);
	CASE (VBI3_OPTION_REAL);
	CASE (VBI3_OPTION_STRING);
	CASE (VBI3_OPTION_MENU);
	default:
		printf ("  * Option %s has invalid type %d\n",
			oi->keyword, oi->type);
		exit (EXIT_FAILURE);
	}

	printf ("  * type=%s keyword=%s label=\"%s\" tooltip=\"%s\"\n",
		type_str, oi->keyword, oi->label, oi->tooltip);

	keyword_check (oi->keyword);

	switch (oi->type) {
	case VBI3_OPTION_BOOL:
	case VBI3_OPTION_INT:
		BOUNDS_CHECK (num, num);
		if (oi->menu.num) {
			printf ("    %d menu entries, default=%d: ",
				oi->max.num - oi->min.num + 1, oi->def.num);
			for (i = oi->min.num; i <= oi->max.num; ++i)
				printf ("%d%s", oi->menu.num[i],
					(i < oi->max.num) ? ", " : "");
			printf ("\n");
		} else {
			printf ("    default=%d, min=%d, max=%d, step=%d\n",
				oi->def.num, oi->min.num,
				oi->max.num, oi->step.num);
		}

		ASSERT_ERRSTR (vbi3_export_option_get (e, oi->keyword, &val));
		print_current (oi, val);

		if (check) {
			if (oi->menu.num) {
				test_set_entries ();
				test_set_menus (int, num);
			} else {
				test_set_entry (e, oi, &val, 0);
				test_set_values (int, num);
			}
		}

		break;

	case VBI3_OPTION_REAL:
		BOUNDS_CHECK (dbl, dbl);
		if (oi->menu.dbl) {
			printf ("    %d menu entries, default=%d: ",
				oi->max.num - oi->min.num + 1, oi->def.num);
			for (i = oi->min.num; i <= oi->max.num; ++i)
				printf ("%f%s", oi->menu.dbl[i],
					(i < oi->max.num) ? ", " : "");
		} else {
			printf ("    default=%f, min=%f, max=%f, step=%f\n",
				oi->def.dbl, oi->min.dbl,
				oi->max.dbl, oi->step.dbl);
		}

		ASSERT_ERRSTR (vbi3_export_option_get (e, oi->keyword, &val));
		print_current (oi, val);

		if (check) {
			if (oi->menu.num) {
				test_set_entries ();
				test_set_menus (real, dbl);
			} else {
				test_set_entry (e, oi, &val, 0);
				test_set_values (real, dbl);
			}
		}
		
		break;

	case VBI3_OPTION_STRING:
		if (oi->menu.str) {
			BOUNDS_CHECK (str, num);

			printf ("    %d menu entries, default=%d: ",
				oi->max.num - oi->min.num + 1, oi->def.num);

			for (i = oi->min.num; i <= oi->max.num; ++i)
				printf ("%s%s", oi->menu.str[i],
					(i < oi->max.num) ? ", " : "");
		} else {
			printf("    default=\"%s\"\n", oi->def.str);
		}

		ASSERT_ERRSTR (vbi3_export_option_get (e, oi->keyword, &val));

		printf("    current value=\"%s\"\n", val.str);
		assert (NULL != val.str);
		free (val.str);

		if (check) {
			printf ("    try to set \"foobar\": ");
			if (vbi3_export_option_set (e, oi->keyword, "foobar"))
				printf ("success.");
			else
				printf ("failed, errstr=\"%s\".",
					vbi3_export_errstr (e));

			ASSERT_ERRSTR (vbi3_export_option_get
				       (e, oi->keyword, &val));

			printf ("    current value=\"%s\"\n", val.str);
			assert (NULL != val.str);
			free (val.str);
		}

		break;

	case VBI3_OPTION_MENU:
		printf ("    %d menu entries, default=%d: ",
			oi->max.num - oi->min.num + 1, oi->def.num);

		for (i = oi->min.num; i <= oi->max.num; ++i) {
			assert (NULL != oi->menu.str[i]);
			printf ("%s%s", oi->menu.str[i],
				(i < oi->max.num) ? ", " : "");
		}

		printf("\n");

		ASSERT_ERRSTR (vbi3_export_option_get (e, oi->keyword, &val));

		print_current (oi, val);

		if (check) {
			test_set_entries ();
		}

		break;

	default:
		assert (!"reached");
		break;
	}
}

static void
list_options			(vbi3_export *		e)
{
	const vbi3_option_info *oi;
	int i;

	puts ("  List of options:");

	for (i = 0; (oi = vbi3_export_option_info_enum (e, i)); ++i) {
		assert (oi->keyword != NULL);
		ASSERT_ERRSTR (oi == vbi3_export_option_info_by_keyword
			       (e, oi->keyword));
		dump_option_info (e, oi);
	}
}

static void
list_modules			(void)
{
	const vbi3_export_info *xi;
	vbi3_export *e;
	char *errstr;
	int i;

	puts ("List of export modules:");

	for (i = 0; (xi = vbi3_export_info_enum (i)); ++i) {
		assert (xi->keyword != NULL);
		assert (xi == vbi3_export_info_by_keyword (xi->keyword));

		printf ("* keyword=%s label=\"%s\"\n"
			"  tooltip=\"%s\" mime_type=%s extension=%s\n",
			xi->keyword, xi->label,
			xi->tooltip, xi->mime_type, xi->extension);

		keyword_check (xi->keyword);

		if (!(e = vbi3_export_new (xi->keyword, &errstr))) {
			printf ("Could not open '%s': %s\n",
				xi->keyword, errstr);
			exit (EXIT_FAILURE);
		}

		ASSERT_ERRSTR (xi == vbi3_export_info_from_export (e));

		list_options (e);

		vbi3_export_delete (e);
	}

	puts ("-- end of list --");
}

static const char short_options [] = "c";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options[] = {
	{ "check",	no_argument,		NULL,		'c' },
	{ NULL, 0, 0, 0 }
};
#else
#define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

int
main				(int			argc,
				 char **		argv)
{
	int index, c;

	setlocale (LC_ALL, "");
	textdomain ("foobar"); /* we are not the library */

	while ((c = getopt_long (argc, argv, short_options,
				 long_options, &index)) != -1)
		switch (c) {
		case 'c':
			check = TRUE;
			break;
		default:
			fprintf (stderr, "Unknown option\n");
			exit (EXIT_FAILURE);
		}

	list_modules ();

	exit (EXIT_SUCCESS);
}

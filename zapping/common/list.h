/*
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: list.h,v 1.1 2000-12-11 04:12:53 mschimek Exp $ */

#ifndef LIST_H
#define LIST_H

#include "types.h"

typedef struct _node {
	struct _node *		next;
} node;

typedef struct _list {
	struct _node *		head;
	struct _node *		tail;
} list;

static inline void
init_list(list *l)
{
	l->head = NULL;
	l->tail = NULL;
}

static inline int
empty_list(list *l)
{
	return !((long) l->head | (long) l->tail);
}

static inline void
add_head(list *l, node *n)
{
	n->next = l->head;

	if (!l->tail)
		l->tail = n;

	l->head = n;
}

static inline void
add_tail(list *l, node *n)
{
	node *p;

	n->next = (node *) 0;

	if ((p = l->tail))
		p->next = n;

	if (!l->head)
		l->head = n;

	l->tail = n;
}

static inline node *
rem_head(list *l)
{
	node *n;

	if ((n = l->head)) {
		if (!n->next)
			l->tail = (node *) 0;

		l->head = n->next;
	}

	return n;
}

#endif /* LIST_H */

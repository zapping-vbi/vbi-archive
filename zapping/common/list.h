/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2001 Michael H. Schimek
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

/* $Id: list.h,v 1.5 2001-05-29 08:10:51 mschimek Exp $ */

#ifndef LIST_H
#define LIST_H

#include "types.h"

typedef struct _node {
	struct _node *		next;
	struct _node *		prev;
} node;

typedef struct _list {
	struct _node *		head;
	struct _node *		tail;
	int			members;
} list;

static inline void
init_list(list *l)
{
	l->head = (node *) 0;
	l->tail = (node *) 0;
	l->members = 0;
}

static inline int
empty_list(list *l)
{
	return l->members == 0;
}

static inline int
list_members(list *l)
{
	return l->members;
}

static inline void
add_head(list *l, node *n)
{
	node *head = l->head;

	n->next = head;
	n->prev = (node *) 0;

	if (head)
		head->prev = n;
	if (!l->tail)
		l->tail = n;

	l->head = n;
	l->members++;
}

static inline void
add_tail(list *l, node *n)
{
	node *tail = l->tail;

	n->next = (node *) 0;
	n->prev = tail;

	if (tail)
		tail->next = n;
	if (!l->head)
		l->head = n;

	l->tail = n;
	l->members++;
}

static inline node *
rem_head(list *l)
{
	node *n = l->head;

	if (n) {
		node *head = l->head = n->next;

		if (head)
			head->prev = (node *) 0;
		if (!head)
			l->tail = (node *) 0;

		l->members--;
	}

	return n;
}

static inline node *
rem_tail(list *l)
{
	node *n = l->tail;

	if (n) {
		node *tail = l->tail = n->prev;

		if (tail)
			tail->next = (node *) 0;
		if (!tail)
			l->head = (node *) 0;

		l->members--;
	}

	return n;
}

static inline node *
rem_node(list *l, node *n)
{
	node *next = n->next, *prev = n->prev;

	if (next)
		next->prev = prev;
	if (prev)
		prev->next = next;
	if (!next)
		l->tail = prev;
	if (!prev)
		l->head = next;

	l->members--;

	return n;
}

#define unlink_node(l, n) rem_node(l, n)

#endif // LIST_H

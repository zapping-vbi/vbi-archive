/*
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

/* $Id: list.h,v 1.7 2001-07-05 08:25:30 mschimek Exp $ */

#ifndef LIST_H
#define LIST_H

#include <assert.h>
#include <pthread.h>
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

/*
 *  NEW STUFF
 */

/*
 *  Your familiar doubly linked list type, plus member
 *  count for fast resource accounting and rwlock.
 *
 *  Warning: No verification of the validity of list and
 *  node parameters. The number of members must not
 *  exceed INT_MAX.
 *
 *  Traverse:
 *  for (node = list->head; node->succ; node = node->succ)
 *
 *  Remove:
 *  for (n1 = list->head; (n2 = n1->succ); n1 = n2)
 *  	rem_node(n1);
 */

typedef struct node3 node3; // preliminary
typedef struct list3 list3;
typedef struct xlist xlist;

struct node3 {
	node3 *			succ;
	node3 *			pred;
};

struct list3 {
	node3 *			head;
	node3 *			null;
	node3 *			tail;
	int			members;
};

struct xlist {
	node3 *			head;
	node3 *			null;
	node3 *			tail;
	int			members;
	pthread_rwlock_t	rwlock;
};

/**
 * destroy_list:
 * 
 * Free all resources associated with the list,
 * you must pair this with an init_list() call.
 *
 * Does not free the list object or any nodes.
 **/
static inline void
destroy_xlist(xlist *l)
{
	assert(l->members == 0);
	assert(pthread_rwlock_destroy(&l->rwlock) == 0);
}

static inline void
destroy_list(list3 *l)
{
}

static inline void
destroy_invalid_xlist(xlist *l)
{
	pthread_rwlock_destroy(&l->rwlock);
}

/**
 * init_list:
 * @l: list3 * 
 * 
 * Return value:
 * The list pointer.
 **/
static inline list3 *
init_list3(list3 *l)
{
	l->head = (node3 *) &l->null;
	l->null = (node3 *) 0;
	l->tail = (node3 *) &l->head;

	l->members = 0;

	return l;
}

static inline xlist *
init_xlist(xlist *l)
{
	assert(pthread_rwlock_init(&l->rwlock, NULL) == 0);

	l->head = (node3 *) &l->null;
	l->null = (node3 *) 0;
	l->tail = (node3 *) &l->head;

	l->members = 0;

	return l;
}

/**
 * list_members:
 * @l: list3 *
 * 
 * Return value:
 * Number of nodes linked in the list. You can read
 * l->members directly when the rwlock is unused.
 **/
static inline unsigned int
list_members3(list3 *l)
{
	return l->members;
}

static inline unsigned int
list_xmembers(xlist *l)
{
	int members;

	pthread_rwlock_rdlock(&l->rwlock);
	members = l->members;
	pthread_rwlock_unlock(&l->rwlock);

	return members;
}

/**
 * empty_list:
 * @l: list3 *
 * 
 * Return value:
 * 1 if the list is empty, 0 otherwise. You can read
 * l->members directly when the rwlock is unused.
 **/
static inline int
empty_list3(list3 *l)
{
	return l->members == 0;
}

static inline int
empty_xlist(xlist *l)
{
	return list_xmembers(l) == 0;
}

/**
 * add_head:
 * @l: list3 *
 * @n: node3 *
 * 
 * Add node at the head of the list.
 *
 * Return value:
 * The node pointer.
 **/
static inline node3 *
add_head3(list3 *l, node3 *n)
{
	n->pred = (node3 *) &l->head;
	n->succ = l->head;
	l->head->pred = n;
	l->head = n;
	l->members++;

	return n;
}

static inline node3 *
add_xhead(xlist *l, node3 *n)
{
	pthread_rwlock_wrlock(&l->rwlock);
	n = add_head3((list3 *) l, n);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

/**
 * add_tail:
 * @l: list3 *
 * @n: node3 *
 * 
 * Add node at the end of the list.
 * 
 * Return value: 
 * The node pointer.
 **/
static inline node3 *
add_tail3(list3 *l, node3 *n)
{
	n->succ = (node3 *) &l->null;
	n->pred = l->tail;
	l->tail->succ = n;
	l->tail = n;
	l->members++;

	return n;
}

static inline node3 *
add_xtail(xlist *l, node3 *n)
{
	pthread_rwlock_wrlock(&l->rwlock);
	n = add_tail3((list3 *) l, n);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

/**
 * rem_head:
 * @l: list3 *
 * 
 * Remove first node of the list.
 * 
 * Return value: 
 * Node pointer, or NULL if the list is empty.
 **/
static inline node3 *
rem_head3(list3 *l)
{
	node3 *n = l->head, *s = n->succ;

	if (s) {
		s->pred = (node3 *) &l->head;
		l->head = s;
		l->members--;
		return n;
	}

	return NULL;
}

static inline node3 *
rem_xhead(xlist *l)
{
	node3 *n;

	pthread_rwlock_wrlock(&l->rwlock);
	n = rem_head3((list3 *) l);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

/**
 * rem_tail:
 * @l: list3 *
 * 
 * Remove last node of the list.
 * 
 * Return value: 
 * Node pointer, or NULL if the list is empty.
 **/
static inline node3 *
rem_tail3(list3 *l)
{
	node3 *n = l->tail, *p = n->pred;

	if (p) {
		p->succ = (node3 *) &l->null;
		l->tail = p;
		l->members--;
		return n;
	}

	return NULL;
}

static inline node3 *
rem_xtail(xlist *l)
{
	node3 *n;

	pthread_rwlock_wrlock(&l->rwlock);
	n = rem_tail3((list3 *) l);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

/**
 * rem_node:
 * @l: list3 *
 * @n: node3 *
 * 
 * Remove the node from its list. The node must
 * be a member of the list, not verified.
 * 
 * Return value: 
 * The node pointer.
 **/
static inline node3 *
rem_node3(list3 *l, node3 *n)
{
	n->pred->succ = n->succ;
	n->succ->pred = n->pred;
	l->members--;

	return n;
}

static inline node3 *
rem_xnode(xlist *l, node3 *n)
{
	pthread_rwlock_wrlock(&l->rwlock);
	n = rem_node3((list3 *) l, n);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

#endif /* LIST_H */

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

/* $Id: list.h,v 1.6 2001-06-30 10:33:46 mschimek Exp $ */

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
 *  Traverse list:
 *  for (node = list->head; node; node = node->next)
 */

typedef struct node2 node2; // preliminary
typedef struct list2 list2;

struct node2 {
	node2 *			next;
	node2 *			prev;
};

struct list2 {
	node2 *			head;
	node2 *			tail;
	pthread_rwlock_t	rwlock;
	int			members;
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
destroy_list(list2 *l)
{
	assert(l->members == 0);
	assert(pthread_rwlock_destroy(&l->rwlock) == 0);
}

static inline void
destroy_list_u(list2 *l)
{
}

static inline void
destroy_invalid_list(list2 *l)
{
	pthread_rwlock_destroy(&l->rwlock);
}

/**
 * init_list:
 * @l: list2 * 
 * 
 * Initialize a list. Static initialization to all zero is
 * sufficient when the rwlock is not used.
 *
 * Return value:
 * The list pointer.
 **/
static inline list2 *
init_list2(list2 *l)
{
	assert(pthread_rwlock_init(&l->rwlock, NULL) == 0);

	l->head = (node2 *) 0;
	l->tail = (node2 *) 0;

	l->members = 0;

	return l;
}

static inline list2 *
init_list_u(list2 *l)
{
	memset(l, 0, sizeof(*l));

	return l;
}

/**
 * list_members:
 * @l: list2 *
 * 
 * Return value:
 * Number of nodes linked in the list. You can read
 * l->members directly when the rwlock is unused.
 **/
static inline unsigned int
list_members2(list2 *l)
{
	int members;

	pthread_rwlock_rdlock(&l->rwlock);
	members = l->members;
	pthread_rwlock_unlock(&l->rwlock);

	return members;
}

static inline unsigned int
list_members_u(list2 *l)
{
	return l->members;
}

/**
 * empty_list:
 * @l: list2 *
 * 
 * Return value:
 * 1 if the list is empty, 0 otherwise. You can read
 * l->members directly when the rwlock is unused.
 **/
static inline int
empty_list2(list2 *l)
{
	return list_members2(l) == 0;
}

static inline int
empty_list_u(list2 *l)
{
	return l->members == 0;
}

/**
 * add_head2:
 * @l: list2 * 
 * @n: node2 *
 * 
 * Add node at the head of the list.
 *
 * Return value:
 * The node pointer.
 **/
static inline node2 *
add_head_u(list2 *l, node2 *n)
{
	node2 *head = l->head;

	n->next = head;
	n->prev = (node2 *) 0;

	if (head)
		head->prev = n;

	if (!l->tail)
		l->tail = n;

	l->head = n;
	l->members++;

	return n;
}

static inline node2 *
add_head2(list2 *l, node2 *n)
{
	pthread_rwlock_wrlock(&l->rwlock);
	n = add_head_u(l, n);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

/**
 * add_tail:
 * @l: list2 * 
 * @n: node2 *
 * 
 * Add node at the end of the list.
 * 
 * Return value: 
 * The node pointer.
 **/
static inline node2 *
add_tail_u(list2 *l, node2 *n)
{
	node2 *tail = l->tail;

	n->next = (node2 *) 0;
	n->prev = tail;

	if (tail)
		tail->next = n;

	if (!l->head)
		l->head = n;

	l->tail = n;
	l->members++;

	return n;
}

static inline node2 *
add_tail2(list2 *l, node2 *n)
{
	pthread_rwlock_wrlock(&l->rwlock);
	n = add_tail_u(l, n);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

/**
 * rem_head:
 * @l: list2 *
 * 
 * Remove first node of the list.
 * 
 * Return value: 
 * Node pointer, or NULL if the list is empty.
 **/
static inline node2 *
rem_head_u(list2 *l)
{
	node2 *n;

	if ((n = l->head)) {
		node2 *head = l->head = n->next;

		if (head)
			head->prev = (node2 *) 0;
		else
			l->tail = (node2 *) 0;

		l->members--;
	}

	return n;
}

static inline node2 *
rem_head2(list2 *l)
{
	node2 *n;

	pthread_rwlock_wrlock(&l->rwlock);
	n = rem_head_u(l);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

/**
 * rem_tail:
 * @l: list2 *
 * 
 * Remove last node of the list.
 * 
 * Return value: 
 * Node pointer, or NULL if the list is empty.
 **/
static inline node2 *
rem_tail_u(list2 *l)
{
	node2 *n;

	if ((n = l->tail)) {
		node2 *tail = l->tail = n->prev;

		if (tail)
			tail->next = (node2 *) 0;
		else
			l->head = (node2 *) 0;

		l->members--;
	}

	return n;
}

static inline node2 *
rem_tail2(list2 *l)
{
	node2 *n;

	pthread_rwlock_wrlock(&l->rwlock);
	n = rem_tail_u(l);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

/**
 * rem_node:
 * @l: list2 *
 * @n: node2 *
 * 
 * Remove the node from the list. The node must
 * be a member of the list, not verified.
 * 
 * Return value: 
 * The node pointer.
 **/
static inline node2 *
rem_node_u(list2 *l, node2 *n)
{
	node2 *next, *prev;

	next = n->next;
	prev = n->prev;

	if (next)
		next->prev = prev;
	else
		l->tail = prev;

	if (prev)
		prev->next = next;
	else
		l->head = next;

	l->members--;

	return n;
}

static inline node2 *
rem_node2(list2 *l, node2 *n)
{
	pthread_rwlock_wrlock(&l->rwlock);
	n = rem_node_u(l, n);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

#endif /* LIST_H */

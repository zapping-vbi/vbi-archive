/*
 *  Copyright (C) 1999-2001 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: list.h,v 1.1 2002-06-18 02:23:44 mschimek Exp $ */

#ifndef LIST_H
#define LIST_H

#include <assert.h>
#include <pthread.h>
#include "types.h"

/*
 *  Your familiar doubly linked list type, plus member
 *  count for fast resource accounting and optional rwlock.
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

typedef struct node node;
typedef struct list list;
typedef struct xlist xlist; /* Xclusive access */

struct node {
	node *			succ;
	node *			pred;
};

struct list {
	node *			head;
	node *			null;
	node *			tail;
	int			members;
};

struct xlist {
	node *			head;
	node *			null;
	node *			tail;
	int			members;
	pthread_rwlock_t	rwlock;
};

/*
 * list foo_list;
 * struct foo { int baz; node bar; }, *foop;
 *
 * for_all_nodes(foop, &foo_list, bar)
 *   foop->baz = 0;
 *
 * Not useful to delete list members.
 */
#define for_all_nodes(p, l, _node_)					\
for ((p) = PARENT((l)->head, typeof(*(p)), _node_);			\
     (p)->_node_.succ;							\
     (p) = PARENT((p)->_node_.succ, typeof(*(p)), _node_))

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
destroy_list(list *l)
{
}

static inline void
destroy_invalid_xlist(xlist *l)
{
	pthread_rwlock_destroy(&l->rwlock);
}

/**
 * init_list:
 * @l: list * 
 * 
 * Return value:
 * The list pointer.
 **/
static inline list *
init_list(list *l)
{
	l->head = (node *) &l->null;
	l->null = (node *) 0;
	l->tail = (node *) &l->head;

	l->members = 0;

	return l;
}

static inline xlist *
init_xlist(xlist *l)
{
	assert(pthread_rwlock_init(&l->rwlock, NULL) == 0);

	l->head = (node *) &l->null;
	l->null = (node *) 0;
	l->tail = (node *) &l->head;

	l->members = 0;

	return l;
}

/**
 * list_members:
 * @l: list *
 * 
 * Return value:
 * Number of nodes linked in the list. You can read
 * l->members directly when the rwlock is unused.
 **/
static inline unsigned int
list_members(list *l)
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
 * @l: list *
 * 
 * Return value:
 * 1 if the list is empty, 0 otherwise. You can read
 * l->members directly when the rwlock is unused.
 **/
static inline int
empty_list(list *l)
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
 * @l: list *
 * @n: node *
 * 
 * Add node at the head of the list.
 *
 * Return value:
 * The node pointer.
 **/
static inline node *
add_head(list *l, node *n)
{
	n->pred = (node *) &l->head;
	n->succ = l->head;
	l->head->pred = n;
	l->head = n;
	l->members++;

	return n;
}

static inline node *
add_xhead(xlist *l, node *n)
{
	pthread_rwlock_wrlock(&l->rwlock);
	n = add_head((list *) l, n);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

/**
 * add_tail:
 * @l: list *
 * @n: node *
 * 
 * Add node at the end of the list.
 * 
 * Return value: 
 * The node pointer.
 **/
static inline node *
add_tail(list *l, node *n)
{
	n->succ = (node *) &l->null;
	n->pred = l->tail;
	l->tail->succ = n;
	l->tail = n;
	l->members++;

	return n;
}

static inline node *
add_xtail(xlist *l, node *n)
{
	pthread_rwlock_wrlock(&l->rwlock);
	n = add_tail((list *) l, n);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

/**
 * rem_head:
 * @l: list *
 * 
 * Remove first node of the list.
 * 
 * Return value: 
 * Node pointer, or NULL if the list is empty.
 **/
static inline node *
rem_head(list *l)
{
	node *n = l->head, *s = n->succ;

	if (s) {
		s->pred = (node *) &l->head;
		l->head = s;
		l->members--;
		return n;
	}

	return NULL;
}

static inline node *
rem_xhead(xlist *l)
{
	node *n;

	pthread_rwlock_wrlock(&l->rwlock);
	n = rem_head((list *) l);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

/**
 * rem_tail:
 * @l: list *
 * 
 * Remove last node of the list.
 * 
 * Return value: 
 * Node pointer, or NULL if the list is empty.
 **/
static inline node *
rem_tail(list *l)
{
	node *n = l->tail, *p = n->pred;

	if (p) {
		p->succ = (node *) &l->null;
		l->tail = p;
		l->members--;
		return n;
	}

	return NULL;
}

static inline node *
rem_xtail(xlist *l)
{
	node *n;

	pthread_rwlock_wrlock(&l->rwlock);
	n = rem_tail((list *) l);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

/**
 * unlink_node:
 * @l: list *
 * @n: node *
 * 
 * Remove the node from its list. The node must
 * be a member of the list, not verified.
 * 
 * Return value: 
 * The node pointer.
 **/
static inline node *
unlink_node(list *l, node *n)
{
	n->pred->succ = n->succ;
	n->succ->pred = n->pred;
	l->members--;

	return n;
}

static inline node *
unlink_xnode(xlist *l, node *n)
{
	pthread_rwlock_wrlock(&l->rwlock);
	n = unlink_node((list *) l, n);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

/**
 * rem_node:
 * @l: list *
 * @n: node *
 * 
 * Remove the node if member of the list.
 * 
 * Return value: 
 * The node pointer or NULL if the node is not
 * member of the list.
 **/
static inline node *
rem_node(list *l, node *n)
{
	node *q;

	for (q = l->head; q->succ; q = q->succ)
		if (n == q) {
			unlink_node(l, n);
			return n;
		}

	return NULL;
}

static inline node *
rem_xnode(xlist *l, node *n)
{
	pthread_rwlock_wrlock(&l->rwlock);
	n = rem_node((list *) l, n);
	pthread_rwlock_unlock(&l->rwlock);

	return n;
}

#endif /* LIST_H */

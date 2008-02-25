/*
 *  libzvbi - Double linked wheel, reinvented
 *
 *  Copyright (C) 2004 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig
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

/* $Id: dlist.h,v 1.1.2.6 2008-02-25 20:58:34 mschimek Exp $ */

#ifndef DLIST_H
#define DLIST_H

#include <assert.h>
#include "macros.h"
#include "misc.h"

#ifndef DLIST_CONSISTENCY
#  define DLIST_CONSISTENCY 0
#endif

struct node {
	struct node *		_succ;
	struct node *		_pred;
};

_vbi3_inline void
verify_ring			(const struct node *	n)
{
	unsigned int counter;
	const struct node *start;

	if (!DLIST_CONSISTENCY)
		return;

	start = n;
	counter = 0;

	do {
		const struct node *_succ = n->_succ;

		assert (counter++ < 30000);
		assert (n == _succ->_pred);
		n = _succ;
	} while (n != start);
}

_vbi3_inline struct node *
_remove_nodes			(struct node *		before,
				 struct node *		after,
				 struct node *		first,
				 struct node *		last,
				 vbi3_bool		close_ring,
				 vbi3_bool		return_ring)
{
	verify_ring (before);

	if (close_ring) {
		before->_succ = after;
		after->_pred = before;
	} else {
		before->_succ = NULL;
		after->_pred = NULL;
	}

	if (return_ring) {
		first->_pred = last;
		last->_succ = first;
	} else {
		first->_pred = NULL;
		last->_succ = NULL;
	}

	return first;
}

_vbi3_inline struct node *
_insert_nodes			(struct node *		before,
				 struct node *		after,
				 struct node *		first,
				 struct node *		last)
{
	verify_ring (before);

	first->_pred = before;
        last->_succ = after;

	after->_pred = last;
	before->_succ = first;

	return first;
}

/**
 * @internal
 * Adds struct node n to a list or ring after struct node a.
 */
_vbi3_inline struct node *
insert_after			(struct node *		a,
				 struct node *		n)
{
	return _insert_nodes (a, a->_succ, n, n);
}

/**
 * @internal
 * Adds struct node n to a list or ring before struct node b.
 */
_vbi3_inline struct node *
insert_before			(struct node *		b,
				 struct node *		n)
{
	return _insert_nodes (b->_pred, b, n, n);
}

/**
 * @internal
 * Removes struct node n from its list or ring.
 */
_vbi3_inline struct node *
unlink_node			(struct node *		n)
{
	return _remove_nodes (n->_pred, n->_succ, n, n, TRUE, FALSE);
}

/**
 * @internal
 *
 * Traverses a list. p points to the parent structure of a node. p1 is
 * a pointer of same type as p, used to remember the _succ struct node in the
 * list. This permits unlink_node(p) in the loop. Resist the temptation
 * to unlink p->_succ or p->_pred. l points to the list to traverse.
 * _node is the name of the struct node element. Example:
 *
 * struct mystruct { struct node foo; int bar; };
 *
 * list mylist; // assumed initialized
 * struct mystruct *p, *p1;
 *
 * FOR_ALL_NODES (p, p1, &mylist, foo)
 *   do_something (p);
 */
#define FOR_ALL_NODES(p, p1, l, _node)					\
for (verify_ring (l), p = PARENT ((l)->_succ, __typeof__ (* p), _node);	\
     p1 = PARENT (p->_node._succ, __typeof__ (* p), _node), 		\
     &p->_node != (l); p = p1)

#define FOR_ALL_NODES_REVERSE(p, p1, l, _node)				\
for (verify_ring (l), p = PARENT ((l)->_pred, __typeof__ (* p), _node);	\
     p1 = PARENT (p->_node._pred, __typeof__ (* p), _node),		\
     &p->_node != (l); p = p1)

/**
 * @internal
 * Destroys list l (not its nodes).
 */
_vbi3_inline struct node *
list_destroy			(struct node *		l)
{
	return _remove_nodes (l->_pred, l->_succ, l, l, FALSE, FALSE);
}

/**
 * @internal
 * Initializes list l.
 */
_vbi3_inline struct node *
list_init			(struct node *		l)
{
	l->_succ = l;
	l->_pred = l;

	return l;
}

/**
 * @internal
 * TRUE if struct node n is at head of list l.
 */
_vbi3_inline vbi3_bool
is_head				(const struct node *	l,
				 const struct node *	n)
{
	verify_ring (l);

	return (n == l->_succ);
}

/**
 * @internal
 * TRUE if struct node n is at tail of list l.
 */
_vbi3_inline vbi3_bool
is_tail				(const struct node *	l,
				 const struct node *	n)
{
	verify_ring (l);

	return (n == l->_pred);
}

/**
 * @internal
 * TRUE if list l is empty.
 */
_vbi3_inline int
is_empty			(const struct node *	l)
{
	return is_head (l, l);
}

/**
 * @internal
 * TRUE if struct node n is a member of list l.
 */
_vbi3_inline vbi3_bool
is_member			(const struct node *	l,
				 const struct node *	n)
{
	const struct node *q;

	verify_ring (l);

	for (q = l->_succ; q != l; q = q->_succ) {
		if (unlikely (q == n)) {
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * @internal
 * Adds struct node n at begin of list l.
 */
_vbi3_inline struct node *
add_head			(struct node *		l,
				 struct node *		n)
{
	return _insert_nodes (l, l->_succ, n, n);
}

/**
 * @internal
 * Adds struct node n at end of list l.
 */
_vbi3_inline struct node *
add_tail			(struct node *		l,
				 struct node *		n)
{
	return _insert_nodes (l->_pred, l, n, n);
}

/**
 * @internal
 * Removes all nodes from list l2 and adds them at end of list l1.
 */
_vbi3_inline struct node *
add_tail_list			(struct node *		l1,
				 struct node *		l2)
{
	struct node *h2 = l2->_succ;

	verify_ring (l2);

	if (unlikely (l2 == h2)) {
		return NULL;
	}

	_insert_nodes (l1->_pred, l1, h2, l2->_pred);

	l2->_succ = l2;
	l2->_pred = l2;

	return h2;
}

/**
 * @internal
 * Removes struct node n if member of list l.
 */
_vbi3_inline struct node *
rem_node			(struct node *		l,
				 struct node *		n)
{
	if (is_member (l, n)) {
		return unlink_node (n);
	} else {
		return NULL;
	}
}

/**
 * @internal
 * Removes first struct node of list l, returns NULL if empty list.
 */
_vbi3_inline struct node *
rem_head			(struct node *		l)
{
	struct node *n = l->_succ;

	if (likely (n != l)) {
		return _remove_nodes (l, n->_succ, n, n, TRUE, FALSE);
	} else {
		return NULL;
	}
}

/**
 * @internal
 * Removes last struct node of list l, returns NULL if empty list.
 */
_vbi3_inline struct node *
rem_tail			(struct node *		l)
{
	struct node *n = l->_pred;

	if (likely (n != l)) {
		return _remove_nodes (n->_pred, l, n, n, TRUE, FALSE);
	} else {
		return NULL;
	}
}

/**
 * @internal
 * Returns number of nodes in list l.
 */
_vbi3_inline unsigned int
list_length			(struct node *		l)
{
	unsigned int count = 0;
	struct node *n;

	verify_ring (l);

	for (n = l->_succ; n != l; n = n->_succ)
		++count;

	return count;
}

#endif /* DLIST_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/

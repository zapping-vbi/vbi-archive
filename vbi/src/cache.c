/*
 *  libzvbi - Teletext cache
 *
 *  Copyright (C) 2001-2003 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
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

#include "../site_def.h"

#include <stdlib.h>		/* malloc() */
#include "dlist.h"
#include "cache-priv.h"
#include "vbi.h"		/* _vbi_page_type_name() */

#ifndef CACHE_DEBUG
#define CACHE_DEBUG 0
#endif

#ifndef CACHE_STATUS
#define CACHE_STATUS 0
#endif

#ifndef CACHE_LOCK_TEST
#define CACHE_LOCK_TEST 0
#endif

#define HASH_SIZE 113

typedef enum {
	/* Locked page to be deleted when unlocked. */
	CACHE_PRI_ZOMBIE,

	/* Ordinary pages of other channels, oldest at head of list.
	   These are deleted first when we run out of memory. */
	CACHE_PRI_ATTIC_NORMAL,

	/* Special pages of other channels, oldest at head of list. */
	CACHE_PRI_ATTIC_SPECIAL,

	/* Ordinary pages of the current channel. */
	CACHE_PRI_NORMAL,

	/* Pages we expect to use frequently, or which take long to reload:
	   - pgno x00 and xyz with x=y=z
	   - shared pages (objs, drcs, navigation)
	   - subpages
	   - visited pages (most recently at end of list) */
	CACHE_PRI_SPECIAL,

	CACHE_PRI_NUM
} cache_priority;

struct _vbi_cache {
	/* Teletext pages by pgno (normal access), cache_page.hash_node,
	   most recently used at head of list. */
	list			hash[HASH_SIZE];

	/* Total number of pages cached, for statistics. */
	unsigned int		n_pages;

	unsigned int		ref_count;

	/* Teletext pages by delete priority (to speed up replacement),
	   cache_page.pri_node, oldest at head of list. */
	list			priority[CACHE_PRI_NUM];

	unsigned int		memory_used;
	unsigned int		memory_limit;

	/* Locked Teletext pages (ref_count > 0), cache_page.pri_node. */
	list			locked;

	/* Cached networks, cache_network.node,
	   most recently used at head of list. */
	list			networks;

	unsigned int		n_networks;
	unsigned int		network_limit;

	vbi_lock_fn *		lock;
	vbi_unlock_fn *		unlock;

	void *			lock_user_data;
};

typedef struct {
	/* Network chain. */
	node			node;

	/* Cache this network belongs to. */
	vbi_cache *		cache;

	unsigned int		ref_count;
	unsigned int		locked_pages;

	unsigned int		current_count;

	vbi_bool		zombie;

	vt_network		network;
} cache_network;

typedef struct {
	node			hash_node;
	node			pri_node;

	/* Network sending this page. */
	cache_network *		network;

	unsigned int		ref_count;
	cache_priority		priority;

	vt_page			page;

	/* Dynamic size, don't add fields below */
} cache_page;

static const char *
cache_priority_name		(cache_priority		pri)
{
	switch (pri) {

#undef CASE
#define CASE(pri) case CACHE_PRI_ ## pri : return #pri ;

	CASE (ZOMBIE)
	CASE (ATTIC_NORMAL)
	CASE (ATTIC_SPECIAL)
	CASE (NORMAL)
	CASE (SPECIAL)

	default:
		assert (!"reached");
		return NULL;
	}
}

static void
_vbi_cache_dump			(const vbi_cache *	ca,
				 FILE *			fp)
{
	fprintf (fp, "cache ref=%u pages=%u mem=%u/%u KiB networks=%u/%u",
		 ca->ref_count,
		 ca->n_pages,
		 (ca->memory_used + 1023) >> 10,
		 (ca->memory_limit + 1023) >> 10,
		 ca->n_networks,
		 ca->network_limit);
}

static void
cache_network_dump		(const cache_network *	cn,
				 FILE *			fp)
{
	fprintf (fp, "network ref=%u locked=%u curr=%u z=%u",
		 cn->ref_count,
		 cn->locked_pages,
		 cn->current_count,
		 cn->zombie);
}

/* Removes Teletext page from network statistics. */ 
static void
cache_network_remove_page	(cache_network *	cn,
				 cache_page *		cp)
{
	page_stat *ps;

	assert (cn == cp->network);

	cp->network = NULL;

	--cn->network.n_pages;

	ps = vt_network_page_stat (&cn->network, cp->page.pgno);

	--ps->n_subpages;
}

/* Adds Teletext page to network statistics. */
static void
cache_network_add_page		(cache_network *	cn,
				 cache_page *		cp)
{
	page_stat *ps;
	vbi_subno subno;

	if (cn->zombie) {
		assert (NULL != cn->cache);

		++cn->cache->n_networks;
		cn->zombie = FALSE;
	}

	cp->network = cn;

	++cn->network.n_pages;

	if (cn->network.n_pages > cn->network.max_pages)
		cn->network.max_pages = cn->network.n_pages;

	/* Consistency check: page number range 0x100 ... 0x8FF
	   including hex pages, and we store at most subpages
	   0x00 ... 0x79. */
	assert (cn->network.n_pages <= 0x800 * 80);

	ps = vt_network_page_stat (&cn->network, cp->page.pgno);

	++ps->n_subpages;

	if (ps->n_subpages > ps->max_subpages)
		ps->max_subpages = ps->n_subpages;

	/* See above. */
	assert (ps->n_subpages <= 80);

	subno = cp->page.subno;

	if ((unsigned int) subno <= 0x79) {
		if (0 == ps->subno_min
		    || subno < ps->subno_min)
			ps->subno_min = subno;
		if (subno > ps->subno_max)
			ps->subno_max = subno;
	}
}

static cache_network *
network_by_nuid			(vbi_cache *		ca,
				 vbi_nuid		nuid)
{
	cache_network *cn, *cn1;

	FOR_ALL_NODES (cn, cn1, &ca->networks, node)
		if (cn->network.client_nuid == nuid
		    || cn->network.client_nuid == nuid) {
			/* Find faster next time. */
			add_head (&ca->networks, unlink_node (&cn->node));
			return cn;
		}

	return NULL;
}

static void
delete_all_pages		(vbi_cache *		ca,
				 cache_network *	cn);

static void
delete_network			(vbi_cache *		ca,
				 cache_network *	cn)
{
	assert (ca == cn->cache);

	if (CACHE_DEBUG) {
		fputs ("Delete ", stderr);
		cache_network_dump (cn, stderr);
		fputc ('\n', stderr);
	}

	if (!cn->zombie)
		--ca->n_networks;

	if (cn->network.n_pages > 0) {
		/* Delete all unreferenced pages. */
		delete_all_pages (ca, cn);
	}

	if (cn->ref_count > 0
	    || cn->locked_pages > 0) {
		cn->zombie = TRUE;
		return;
	}

	unlink_node (&cn->node);

	free (cn);
}

static void
delete_surplus_networks		(vbi_cache *		ca)
{
	cache_network *cn, *cn1;

	/* Remove last recently used networks first. */
	FOR_ALL_NODES_REVERSE (cn, cn1, &ca->networks, node) {
		if (cn->ref_count > 0
		    || cn->locked_pages > 0)
			continue;

		if (cn->zombie
		    || ca->n_networks > ca->network_limit)
			delete_network (ca, cn);
	}
}

static cache_network *
add_network			(vbi_cache *		ca,
				 vbi_nuid		client_nuid)
{
	cache_network *cn, *cn1;

	if ((cn = network_by_nuid (ca, client_nuid)))
		return cn;

	if (ca->n_networks >= ca->network_limit) {
		/* We absorb the last recently used cache_network
		   without references. */

		FOR_ALL_NODES_REVERSE (cn, cn1, &ca->networks, node)
			if (0 == cn->ref_count
			    && 0 == cn->locked_pages)
				break;

		if (!cn->node.pred) {
			if (CACHE_DEBUG)
				fprintf (stderr,
					 "%s: network_limit=%u, all locked\n",
					 __FUNCTION__, ca->network_limit);
			return NULL;
		}

		if (cn->network.n_pages > 0)
			delete_all_pages (ca, cn);

		unlink_node (&cn->node);

		cn->network.n_pages = 0;
		cn->network.max_pages = 0;

		cn->ref_count = 0;
		cn->locked_pages = 0;

		cn->current_count = 0;

		cn->zombie = FALSE;

		vt_network_init (&cn->network);
	} else {
		if (!(cn = calloc (1, sizeof (*cn)))) {
			if (CACHE_DEBUG)
				fprintf (stderr, "%s: out of memory\n",
					 __FUNCTION__);
			return NULL;
		}

		vt_network_init (&cn->network);

		++ca->n_networks;
	}

	cn->network.client_nuid = client_nuid;
	cn->network.received_nuid = VBI_NUID_UNKNOWN;

	cn->cache = ca;

	add_head (&ca->networks, &cn->node);

	return cn;
}

static const unsigned int
cache_page_overhead =
	sizeof (cache_page) - sizeof (((cache_page *) 0)->page);

static void
cache_page_dump			(const cache_page *	cp,
				 FILE *			fp)
{
	const cache_network *cn;

	fprintf (fp, "page %x.%x ", cp->page.pgno, cp->page.subno);

	if ((cn = cp->network)) {
		const page_stat *ps;

		ps = vt_network_const_page_stat (&cn->network, cp->page.pgno);

		fprintf (fp, "%llx/%llx %s/L%u/S%04x "
			 "subp=%u/%u (%u-%u) ",
			 cn->network.client_nuid,
			 cn->network.received_nuid,
			 _vbi_page_type_name (ps->page_type),
			 ps->charset_code, ps->subcode,
			 ps->n_subpages, ps->max_subpages,
			 ps->subno_min, ps->subno_max);
	} else {
		fputs ("nuid=n/a ", fp);
	}

	fprintf (stderr, "ref=%u %s",
		 cp->ref_count, cache_priority_name (cp->priority));
}

vbi_inline unsigned int
hash				(vbi_pgno		pgno)
{
	return pgno % HASH_SIZE;
}

static void
delete_page			(vbi_cache *		ca,
				 cache_page *		cp)
{
	assert (NULL != cp->network);
	assert (ca == cp->network->cache);

	if (cp->ref_count > 0) {
		/* Remove from cache, mark for deletion.
		   cp->pri_node remains on ca->locked. */

		unlink_node (&cp->hash_node);

		cache_network_remove_page (cp->network, cp);

		cp->priority = CACHE_PRI_ZOMBIE;

		return;
	}

	if (CACHE_DEBUG) {
		fputs ("Delete ", stderr);
		cache_page_dump (cp, stderr);
		fputc (' ', stderr);
		cache_network_dump (cp->network, stderr);
		fputc ('\n', stderr);
	}

	if (CACHE_PRI_ZOMBIE != cp->priority)
		ca->memory_used -=
			vt_page_size (&cp->page) + cache_page_overhead;

	unlink_node (&cp->pri_node);
	unlink_node (&cp->hash_node);

	cache_network_remove_page (cp->network, cp);

	free (cp);

	--ca->n_pages;
}

static void
delete_all_pages_by_priority	(vbi_cache *		ca,
				 cache_network *	cn,
				 cache_priority		pri)
{
	cache_page *cp, *cp1;

	assert (ca == cn->cache);

	FOR_ALL_NODES (cp, cp1, &ca->priority[pri], pri_node)
		if (!cn || cp->network == cn)
			delete_page (ca, cp);
}

static void
delete_all_pages		(vbi_cache *		ca,
				 cache_network *	cn)
{
	cache_priority pri;

	assert (ca == cn->cache);

	for (pri = CACHE_PRI_ATTIC_NORMAL; pri < CACHE_PRI_NUM; ++pri)
		delete_all_pages_by_priority (ca, cn, pri);
}

static void
delete_surplus_pages		(vbi_cache *		ca)
{
	cache_priority pri;

	for (pri = CACHE_PRI_ATTIC_NORMAL; pri < CACHE_PRI_NUM; ++pri) {
		cache_page *cp, *cp1;

		FOR_ALL_NODES (cp, cp1, &ca->priority[pri], pri_node) {
			if (ca->memory_used <= ca->memory_limit)
				return;

			if (0 == cp->ref_count)
				delete_page (ca, cp);
		}
	}
}

static void
change_priority_of_all_pages	(vbi_cache *		ca,
				 cache_network *	cn,
				 cache_priority		pri_to,
				 cache_priority		pri_from)
{
	list *pri_to_list;
	list *pri_from_list;
	cache_page *cp, *cp1;

	assert (ca == cn->cache);

	assert (pri_to != CACHE_PRI_ZOMBIE);
	assert (pri_from != CACHE_PRI_ZOMBIE);

	if (pri_to == pri_from)
		return;

	pri_to_list = ca->priority + pri_to;
	pri_from_list = ca->priority + pri_from;

	if (cn) {
		FOR_ALL_NODES (cp, cp1, pri_from_list, pri_node) {
			if (cp->network != cn)
				continue;

			cp->priority = pri_to;

			add_tail (pri_to_list, unlink_node (&cp->pri_node));
		}
	} else {
		FOR_ALL_NODES (cp, cp1, pri_from_list, pri_node)
			cp->priority = pri_to;

		add_tail_list (pri_to_list, pri_from_list);
	}

	FOR_ALL_NODES (cp, cp1, &ca->locked, pri_node)
		if (cp->priority == pri_from)
			cp->priority = pri_to;
}

static cache_page *
page_by_pgno			(vbi_cache *		ca,
				 cache_network *	cn,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 vbi_subno		subno_mask)
{
	list *hash_list;
	cache_page *cp, *cp1;

	assert (ca == cn->cache);

	hash_list = ca->hash + hash (pgno);

	FOR_ALL_NODES (cp, cp1, hash_list, hash_node) {
		if (CACHE_DEBUG > 1) {
			fputs ("Try ", stderr);
			cache_page_dump (cp, stderr);
			fputc ('\n', stderr);
		}

		if (cp->page.pgno == pgno
		    && (cp->page.subno & subno_mask) == subno
		    && (!cn || cp->network == cn)) {
			/* Find faster next time. */
			add_head (hash_list, unlink_node (&cp->hash_node));
			return cp;
		}
	}

	return NULL;
}

static cache_page *
lock_page			(vbi_cache *		ca,
				 cache_network *	cn,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 vbi_subno		subno_mask)
{
	cache_page *cp;

	assert (ca == cn->cache);

	if (!(cp = page_by_pgno (ca, cn, pgno, subno, subno_mask)))
		return NULL;

	if (CACHE_DEBUG) {
		fputs ("Found ", stderr);
		cache_page_dump (cp, stderr);
	}

	if (0 == cp->ref_count) {
		if (CACHE_DEBUG) {
			fputc (' ', stderr);
			cache_network_dump (cn, stderr);
		}

		if (cn->zombie) {
			++ca->n_networks;
			cn->zombie = FALSE;
		}

		++cn->locked_pages;

		ca->memory_used -=
			vt_page_size (&cp->page) + cache_page_overhead;

		add_tail (&ca->locked, unlink_node (&cp->pri_node));
	}

	if (CACHE_DEBUG)
		fputc ('\n', stderr);

	++cp->ref_count;

	return cp;
}

/*
 *  Interface
 */

/**
 * @internal
 *
 * Unreferences a page returned by vbi_cache_put_page(),
 * vbi_cache_get_page(), or vbi_page_dup_page_ref().
 * @a vtp can be @c NULL.
 */
void
_vbi_cache_release_page		(vbi_cache *		ca,
				 const vt_page *	vtp)
{
	cache_page *cp;

	assert (NULL != ca);

	if (!vtp)
		return;

	cp = PARENT ((vt_page *) vtp, cache_page, page);

	ca->lock (ca->lock_user_data);

	if (0 == cp->ref_count) {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"Unreferenced page %p", cp);
		ca->unlock (ca->lock_user_data);
		return;
	}

	if (CACHE_DEBUG) {
		fputs ("Unref ", stderr);
		_vbi_cache_dump (ca, stderr);
		fputc (' ', stderr);
		cache_page_dump (cp, stderr);
	}

	if (0 == --cp->ref_count) {
		cache_network *cn;

		cn = cp->network;

		switch (cp->priority) {
		case CACHE_PRI_ZOMBIE:
			delete_page (ca, cp);
			break;

		default:
			if (CACHE_DEBUG) {
				fputc (' ', stderr);
				cache_network_dump (cn, stderr);
			}

			add_tail (&ca->priority[cp->priority],
				  unlink_node (&cp->pri_node));

			ca->memory_used += vt_page_size (&cp->page)
				+ cache_page_overhead;

			break;
		}

		--cn->locked_pages;

		if (cn->zombie
		    && 0 == cn->locked_pages
		    && 0 == cn->ref_count)
			delete_network (ca, cn);

		if (ca->memory_used > ca->memory_limit)
			delete_surplus_pages (ca);
	}

	if (CACHE_DEBUG)
		fputc ('\n', stderr);

	ca->unlock (ca->lock_user_data);
}

/**
 * @internal
 *
 * Duplicates a page reference.
 *
 * @return 
 * vt_page pointer, never fails. Horrible things will happen
 * when @a vtp was not returned by vbi_cache functions.
 */
const vt_page *
_vbi_cache_dup_page_ref		(vbi_cache *		ca,
				 const vt_page *	vtp)
{
	cache_page *cp;

	assert (NULL != ca);
	assert (NULL != vtp);

	cp = PARENT ((vt_page *) vtp, cache_page, page);

	ca->lock (ca->lock_user_data);

	++cp->ref_count;

	ca->unlock (ca->lock_user_data);

	return vtp;
}

/**
 * @internal
 *
 * Gets a page from the cache. When @a subno is VBI_SUB_ANY, the most
 * recently received subpage of that page is returned.
 * 
 * The reference counter of the page is incremented, you must call
 * vbi_cache_release_page() to unreference the page. When @a user_access
 * is set, the page gets higher priority to stay in cache and is marked
 * as most recently received subpage.
 * 
 * @return 
 * vt_page pointer, NULL when the requested page is not cached.
 */
const vt_page *
_vbi_cache_get_page		(vbi_cache *		ca,
				 const vt_network *	vtn,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 vbi_subno		subno_mask,
				 vbi_bool		user_access)
{
	cache_network *cn;
	cache_page *cp;

	assert (NULL != ca);
	assert (NULL != vtn);

	ca->lock (ca->lock_user_data);

	cn = PARENT ((vt_network *) vtn, cache_network, network);

	if (CACHE_DEBUG) {
		fprintf (stderr, "Ref %s%x.%x/%x ",
			 user_access ? "user " : "",
			 pgno, subno, subno_mask);
		_vbi_cache_dump (ca, stderr);
		fputc (' ', stderr);
		cache_network_dump (cn, stderr);
		fputc ('\n', stderr);
	}

	if (VBI_ANY_SUBNO == subno)
		subno_mask = 0;

	if (!(cp = lock_page (ca, cn, pgno, subno, subno_mask)))
		goto failure;

	if (user_access) {
		/* Now the page is on the ca->locked list. Unreferencing
		   reenqueues on one of the delete priority lists
		   according to its cp->priority. */

		switch (cp->priority) {
		case CACHE_PRI_ATTIC_NORMAL:
			cp->priority = CACHE_PRI_ATTIC_SPECIAL;
			break;

		case CACHE_PRI_NORMAL:
			cp->priority = CACHE_PRI_SPECIAL;
			break;

		default:
			break;
		}
	}

	ca->unlock (ca->lock_user_data);

	return &cp->page;

 failure:
 	ca->unlock (ca->lock_user_data);

	return NULL;
}

/**
 * @internal
 *
 * Puts a Teletext page in the cache.
 *
 * When @a new_ref is @c TRUE, the cached page will be referenced
 * as with vbi_cache_get_page().
 * 
 * @return 
 * vt_page pointer (in the cache, not @a vtp), NULL on failure
 * (out of memory).
 */
const vt_page *
_vbi_cache_put_page		(vbi_cache *		ca,
				 const vt_network *	vtn,
				 const vt_page *	vtp,
				 vbi_bool		new_ref)
{
	cache_page *death_row[20];
	unsigned int death_count;
	cache_network *cn;
	cache_page *old_cp;
	long memory_available;	/* NB can be < 0 */
	long memory_needed;
	cache_priority pri;
	cache_page *cp;

	assert (NULL != ca);
	assert (NULL != vtn);
	assert (NULL != vtp);

	ca->lock (ca->lock_user_data);

	memory_needed = vt_page_size (vtp) + cache_page_overhead;
	memory_available = ca->memory_limit - ca->memory_used;

	cn = PARENT ((vt_network *) vtn, cache_network, network);

	if (CACHE_DEBUG) {
		fprintf (stderr, "Put %x.%x ",
			 vtp->pgno, vtp->subno);
		_vbi_cache_dump (ca, stderr);
		fputc (' ', stderr);
		cache_network_dump (cn, stderr);
		fputc (' ', stderr);
	}

	death_count = 0;

	{
		const page_stat *ps;
		vbi_subno subno_mask;

		/* EN 300 706, A.1, E.2: Pages with subno > 0x79 do not really
		   have subpages. In this case we use subno_mask zero,
		   replacing any previously received subpage. XXX doesn't
		   work for clock pages without page_type between 00:00
		   and 00:59. */

		ps = vt_network_page_stat (&cn->network, vtp->pgno);

		if (VBI_NONSTD_SUBPAGES == (vbi_page_type) ps->page_type)
			subno_mask = 0;
		else
			subno_mask = - ((unsigned int) vtp->subno <= 0x79);

		old_cp = page_by_pgno (ca, cn, vtp->pgno,
				       vtp->subno, subno_mask);
	}

	if (old_cp) {
		if (CACHE_DEBUG) {
			fputs ("is cached ", stderr);
			cache_page_dump (old_cp, stderr);
			fputc (' ', stderr);
		}

		if (old_cp->ref_count > 0) {
			/* This page is still in use. We remove it from
			   the cache and mark for deletion when unref'd. */
			unlink_node (&old_cp->hash_node);
			cache_network_remove_page (old_cp->network, old_cp);

			old_cp->priority = CACHE_PRI_ZOMBIE;
			old_cp = NULL;
		} else {
			/* Got our first replacement candidate. */
			death_row[death_count++] = old_cp;
			memory_available += vt_page_size (&old_cp->page)
				+ cache_page_overhead;
		}
	}

	if (memory_available >= memory_needed)
		goto replace;

	for (pri = CACHE_PRI_ATTIC_NORMAL; pri < CACHE_PRI_NUM; ++pri) {
		cache_page *cp, *cp1;

		FOR_ALL_NODES (cp, cp1, &ca->priority[pri], pri_node) {
			if (memory_available >= memory_needed)
				goto replace;

			if (cp == old_cp)
				continue;

			assert (death_count < N_ELEMENTS (death_row));

			death_row[death_count++] = cp;
			memory_available += vt_page_size (&cp->page)
				+ cache_page_overhead;
		}
	}

	if (CACHE_DEBUG) {
		fprintf (stderr, "need %lu bytes but only %lu available\n",
			 memory_needed, memory_available);
	}

	goto failure;

 replace:
	if (memory_available == memory_needed
	    && 1 == death_count) {
		cp = death_row[0];

		if (CACHE_DEBUG) {
			fputs ("reusing ", stderr);
			cache_page_dump (cp, stderr);
			fputc (' ', stderr);
		}

		unlink_node (&cp->pri_node);
		unlink_node (&cp->hash_node);

		cache_network_remove_page (cp->network, cp);

		ca->memory_used -= memory_needed;
	} else {
		unsigned int i;

		if (!(cp = malloc (memory_needed))) {
			if (CACHE_DEBUG)
				fputs ("out of memory\n", stderr);

			goto failure;
		}

		for (i = 0; i < death_count; ++i)
			delete_page (ca, death_row[i]);

		++ca->n_pages;
	}

	add_head (ca->hash + hash (vtp->pgno), &cp->hash_node);

	/* 100, 200, 300, ... magazine start page. */
	if (0x00 == (vtp->pgno & 0xFF))
		cp->priority = CACHE_PRI_SPECIAL;
	/* 111, 222, 333, ... magic page number. */
	else if ((vtp->pgno >> 4) == (vtp->pgno & 0xFF))
		cp->priority = CACHE_PRI_SPECIAL;
	/* Something we may not want in cache, much less all subpages. */
	else if (PAGE_FUNCTION_UNKNOWN == vtp->function)
		cp->priority = CACHE_PRI_NORMAL;
	/* POP, GPOP, DRCS, GDRCS */
	else if (PAGE_FUNCTION_LOP != vtp->function)
		cp->priority = CACHE_PRI_SPECIAL;
	/* Regular subpage, not clock, not rotating ads etc. */
	else if (vtp->subno > 0x00 && vtp->subno < 0x79)
		cp->priority = CACHE_PRI_SPECIAL;
	else
		cp->priority = CACHE_PRI_NORMAL;

	memcpy (&cp->page, vtp, memory_needed - cache_page_overhead);

	if (new_ref) {
		cp->ref_count = 1;
		ca->memory_used += 0; /* see lock_page() */

		++cn->locked_pages;

		add_tail (&ca->locked, &cp->pri_node);
	} else {
		cp->ref_count = 0;
		ca->memory_used += memory_needed;

		add_tail (ca->priority + cp->priority, &cp->pri_node);
	}

	cache_network_add_page (cn, cp);

	if (CACHE_STATUS) {
		fprintf (stderr, "cache status:\n");
		_vbi_cache_dump (ca, stderr);
		fputc ('\n', stderr);
		cache_page_dump (cp, stderr);
		fputc ('\n', stderr);
		cache_network_dump (cp->network, stderr);
		fputc ('\n', stderr);
	}

	ca->unlock (ca->lock_user_data);

	return &cp->page;

 failure:
 	ca->unlock (ca->lock_user_data);

	return NULL;
}

/**
 * @internal
 */
void
_vbi_cache_release_network	(vbi_cache *		ca,
				 const vt_network *	vtn)
{
	cache_network *cn;

	assert (NULL != ca);

	if (!vtn)
		return;

	cn = PARENT ((vt_network *) vtn, cache_network, network);

	ca->lock (ca->lock_user_data);

	if (0 == cn->ref_count) {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"Unreferenced network %p", cn);
		ca->unlock (ca->lock_user_data);
		return;
	}

	--cn->ref_count;

	if (0 == cn->ref_count
	    && 0 == cn->locked_pages
	    && (cn->zombie || ca->n_networks > ca->network_limit))
		delete_surplus_networks (ca);

	ca->unlock (ca->lock_user_data);
}

/**
 * @internal
 */
vt_network *
_vbi_cache_get_network		(vbi_cache *		ca,
				 vbi_nuid		client_nuid)
{
	cache_network *cn;

	assert (NULL != ca);

	ca->lock (ca->lock_user_data);

	if ((cn = network_by_nuid (ca, client_nuid))) {
		if (cn->zombie) {
			++ca->n_networks;
			cn->zombie = FALSE;
		}

		++cn->ref_count;
	}

	ca->unlock (ca->lock_user_data);

	return cn ? &cn->network : NULL;
}

/**
 * @internal
 */
vt_network *
_vbi_cache_new_network		(vbi_cache *		ca,
				 vbi_nuid		client_nuid)
{
	cache_network *cn;

	assert (NULL != ca);

	ca->lock (ca->lock_user_data);

	if ((cn = add_network (ca, client_nuid))) {
		++cn->ref_count;
	}

	ca->unlock (ca->lock_user_data);

	return cn ? &cn->network : NULL;
}

/* Deletes all data associated with a network. A referenced network and
 * referenced Teletext pages are marked for deletion when unreferenced.
 */
void
_vbi_cache_purge_by_nuid	(vbi_cache *		ca,
				 vbi_nuid		nuid)
{
	cache_network *cn;

	assert (NULL != ca);

	ca->lock (ca->lock_user_data);

	if (!(cn = network_by_nuid (ca, nuid))) {
		ca->unlock (ca->lock_user_data);
		return;
	}

	delete_network (ca, cn);

	ca->unlock (ca->lock_user_data);
}

/* Deletes all cache contents. Referenced networks and Teletext pages
 * are marked for deletion when unreferenced.
 */
void
_vbi_cache_purge		(vbi_cache *		ca)
{
	cache_network *cn, *cn1;

	assert (NULL != ca);

	ca->lock (ca->lock_user_data);

	FOR_ALL_NODES (cn, cn1, &ca->networks, node)
		delete_network (ca, cn);

	ca->unlock (ca->lock_user_data);
}

/**
 * @param force When TRUE force the network out of "current" state,
 *   otherwise require pairing with _vbi_cache_current_network() calls. 
 *
 * Counterpart of _vbi_cache_current_network(). This function assigns
 * a network normal priority.
 */
void
_vbi_cache_normal_network	(vbi_cache *		ca,
				 vbi_nuid		nuid,
				 vbi_bool		force)
{
	cache_network *cn;

	assert (NULL != ca);

	ca->lock (ca->lock_user_data);

	if (!(cn = network_by_nuid (ca, nuid))) {
		/* No error. */
		ca->unlock (ca->lock_user_data);
		return;
	}

	if (force || 1 == cn->current_count) {
		cn->current_count = 0;

		change_priority_of_all_pages
			(ca, cn, CACHE_PRI_ATTIC_SPECIAL, CACHE_PRI_SPECIAL);
		change_priority_of_all_pages
			(ca, cn, CACHE_PRI_ATTIC_NORMAL, CACHE_PRI_NORMAL);
	} else {
		--cn->current_count;
	}

	ca->unlock (ca->lock_user_data);
}

/**
 * @internal
 * @param exclusive When TRUE assign normal priority to all other
 *   networks in the cache.
 *
 * Selects the current network, typically after a channel switch. When
 * the cache becomes too small, data of other networks will be deleted first.
 *
 * You can select more than one "current" network, this is useful when
 * you feed the cache from multiple sources.
 */
void
_vbi_cache_current_network	(vbi_cache *		ca,
				 vbi_nuid		nuid,
				 vbi_bool		exclusive)
{
	cache_network *cn;

	assert (NULL != ca);

	ca->lock (ca->lock_user_data);

	if (!(cn = network_by_nuid (ca, nuid))) {
		ca->unlock (ca->lock_user_data);
		return;
	}

	if (exclusive && ca->n_networks > 1) {
		change_priority_of_all_pages
			(ca, NULL, CACHE_PRI_ATTIC_SPECIAL, CACHE_PRI_SPECIAL);
		change_priority_of_all_pages
			(ca, NULL, CACHE_PRI_ATTIC_NORMAL, CACHE_PRI_NORMAL);
	}

	change_priority_of_all_pages
		(ca, cn, CACHE_PRI_SPECIAL, CACHE_PRI_ATTIC_SPECIAL);
	change_priority_of_all_pages
		(ca, cn, CACHE_PRI_NORMAL, CACHE_PRI_ATTIC_NORMAL);

	++cn->current_count;

	ca->unlock (ca->lock_user_data);
}

static void
dummy_lock			(void *			user_data)
{
	if (CACHE_LOCK_TEST) {
		assert (0 == * (int *) user_data);
		* (int *) user_data = 1;
	}
}

static void
dummy_unlock			(void *			user_data)
{
	if (CACHE_LOCK_TEST) {
		assert (1 == * (int *) user_data);
		* (int *) user_data = 0;
	}
}

void
_vbi_cache_set_lock_functions	(vbi_cache *		ca,
				 vbi_lock_fn *		lock,
				 vbi_unlock_fn *	unlock,
				 void *			user_data)
{
	assert (NULL != ca);

	/* Both or none. */
	assert ((NULL == lock) == (NULL == unlock));

	if (lock) {
		ca->lock = lock;
		ca->unlock = unlock;
		ca->lock_user_data = user_data;
	} else {
		ca->lock = dummy_lock;
		ca->unlock = dummy_unlock;
		
		if (CACHE_LOCK_TEST) {
			static int is_locked;

			is_locked = 0;
			ca->lock_user_data = &is_locked;
		}
	}
}

/*
 * Limits the amount of memory used by the Teletext page cache. Reasonable
 * values range from 16 KB to 1 GB, default is 1 GB as in libzvbi 0.2.
 * The number of pages transmitted by networks varies. Expect on the order
 * of one megabyte for a complete set.
 * 
 * When the cache is too small to contain all pages of a network,
 * newly received pages will replace older pages. Pages of the
 * current network, or which have been recently requested, or will
 * likely be requested in the future, or take longer to reload
 * are last deleted.
 *
 * When @a limit is smaller than the amount of memory currently
 * used, this function attempts to delete an appropriate number of
 * pages.
 */
void
_vbi_cache_set_memory_limit	(vbi_cache *		ca,
				 unsigned int		limit)
{
	assert (NULL != ca);

	ca->lock (ca->lock_user_data);

	ca->memory_limit = SATURATE (limit, 1 << 10, 1 << 30);

	delete_surplus_pages (ca);

	ca->unlock (ca->lock_user_data);
}

/* Limits the number of networks cached. The default is 1
 * as in libzvbi 0.2. Currently each network takes about 16 KB
 * additional to the vbi_cache_set_memory_limit().
 *
 * When the number is smaller than the current number of networks
 * cached this function attempts to delete the data of the last
 * recently requested stations.
 */
void
_vbi_cache_set_network_limit	(vbi_cache *		ca,
				 unsigned int		limit)
{
	assert (NULL != ca);

	ca->lock (ca->lock_user_data);

	ca->network_limit = SATURATE (limit, 1, 3000);

	delete_surplus_networks (ca);

	ca->unlock (ca->lock_user_data);
}

vbi_cache *
_vbi_cache_dup_ref		(vbi_cache *		ca)
{
	assert (NULL != ca);

	ca->lock (ca->lock_user_data);

	++ca->ref_count;

	ca->unlock (ca->lock_user_data);

	return ca;
}

void
vbi_cache_delete		(vbi_cache *		ca)
{
	unsigned int i;

	if (!ca)
		return;

	ca->lock (ca->lock_user_data);

	if (--ca->ref_count > 0) {
		ca->unlock (ca->lock_user_data);
		return;
	}

	ca->unlock (ca->lock_user_data);

	_vbi_cache_purge (ca);

	if (!empty_list (&ca->locked)) {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"Warning: some cached pages still "
				"referenced, memory leaks.\n");
	}

	if (!empty_list (&ca->networks)) {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"Warning: some cached networks still "
				"referenced, memory leaks.\n");
	}

	list_destroy (&ca->locked);
	list_destroy (&ca->networks);

	for (i = 0; i < N_ELEMENTS (ca->priority); ++i)
		list_destroy (ca->priority + i);

	for (i = 0; i < N_ELEMENTS (ca->hash); ++i)
		list_destroy (ca->hash + i);

	CLEAR (*ca);

	free (ca);
}

vbi_cache *
vbi_cache_new			(void)
{
	vbi_cache *ca;
	unsigned int i;

	if (!(ca = calloc (1, sizeof (*ca)))) {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"Out of memory (%u)", sizeof (*ca));
		return NULL;
	}

	for (i = 0; i < N_ELEMENTS (ca->hash); ++i)
		list_init (ca->hash + i);

	for (i = 0; i < N_ELEMENTS (ca->priority); ++i)
		list_init (ca->priority + i);

	list_init (&ca->locked);
	list_init (&ca->networks);

	ca->memory_limit = 1 << 30;
	ca->network_limit = 1;

	_vbi_cache_set_lock_functions (ca, NULL, NULL, NULL);

	ca->ref_count = 1;

	return ca;
}



#if 0

/* XXX rethink */
int
vbi_cache_foreach		(vbi_decoder *		vbi,
				 vbi_nuid		client_nuid,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 int			dir,
				 foreach_callback *	func,
				 void *			data)
{
	cache *ca = vbi->cache;
	cache_stat *cs;
	cache_page *cp;
	page_stat *ps;
	vbi_bool wrapped = FALSE;
	list *hash_list;

	cache_lock (ca);

	cs = cache_stat_from_nuid (ca, client_nuid);

	if (!cs || cs->num_pages == 0)
		return 0;

	if ((cp = page_lookup (ca, cs, pgno, subno, ~0, &hash_list))) {
		subno = cp->page.subno;
	} else if (subno == VBI_ANY_SUBNO) {
		cp = NULL;
		subno = 0;
	}

	ps = cs->pages + pgno - 0x100;

	for (;;) {
		if (cp) {
			int r;

			cache_unlock (ca); /* XXX */

			if ((r = func (data, &cp->page, wrapped)))
				return r;

			cache_lock (ca);
		}

		subno += dir;

		while (ps->num_pages == 0
		       || subno < ps->subno_min
		       || subno > ps->subno_max) {
			if (dir < 0) {
				pgno--;
				ps--;

				if (pgno < 0x100) {
					pgno = 0x8FF;
					ps = cs->pages + 0x7FF;
					wrapped = 1;
				}

				subno = ps->subno_max;
			} else {
				pgno++;
				ps++;

				if (pgno > 0x8FF) {
					pgno = 0x100;
					ps = cs->pages + 0x000;
					wrapped = 1;
				}

				subno = ps->subno_min;
			}
		}

		cp = page_lookup (ca, cs, pgno, subno, ~0, &hash_list);
	}
}



#endif


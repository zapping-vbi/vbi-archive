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
#include "cache-priv.h"

#ifndef CACHE_DEBUG
#define CACHE_DEBUG 0
#endif

#ifndef CACHE_STATUS
#define CACHE_STATUS 0
#endif

#ifndef CACHE_CONSISTENCY
#define CACHE_CONSISTENCY 0
#endif

/** @internal */
struct _vbi_cache {
	/**
	 * Teletext pages by pgno, most recently used at
	 * head of list. Uses cache_page.hash_node.
	 */
	list			hash[HASH_SIZE];

	/** Total number of pages cached, for statistics. */
	unsigned int		n_pages;

	unsigned int		ref_count;

	/**
	 * Teletext pages to be replaced when out of memory, oldest at
	 * head of list, uses cache_page.pri_node.
	 */
	list			priority;

	/** Referenced Teletext pages, uses cache_page.pri_node. */
	list			referenced;

	/**
	 * Memory used by all pages except referenced and zombies. (May
	 * deadlock if all pages are referenced and the caller releases
	 * only when receiving new pages.)
	 */
	unsigned int		memory_used;
	unsigned int		memory_limit;

	/** Cached networks, most recently used at head of list. */
	list			networks;

	/** Number of networks in cache except zombies. */
	unsigned int		n_networks;
	unsigned int		network_limit;
};

static const char *
cache_priority_name		(cache_priority		pri)
{
	switch (pri) {

#undef CASE
#define CASE(pri) case CACHE_PRI_ ## pri : return #pri ;

	CASE (ZOMBIE)
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
	fprintf (fp, "network ref=%u referenced=%u zombie=%u",
		 cn->ref_count,
		 cn->n_referenced_pages,
		 cn->zombie);
}

/* Removes Teletext page from network statistics. */ 
static void
cache_network_remove_page	(cache_network *	cn,
				 cache_page *		cp)
{
	page_stat *ps;

	if (CACHE_CONSISTENCY)
		assert (cn == cp->network);

	cp->network = NULL;

	--cn->n_pages;

	ps = cache_network_page_stat (cn, cp->pgno);

	--ps->n_subpages;
}

/* Adds Teletext page to network statistics. */
static void
cache_network_add_page		(cache_network *	cn,
				 cache_page *		cp)
{
	page_stat *ps;

	if (cn->zombie) {
		assert (NULL != cn->cache);

		/* Zombies don't count. */
		++cn->cache->n_networks;

		cn->zombie = FALSE;
	}

	cp->network = cn;

	++cn->n_pages;

	/* Consistency check: page number range 0x100 ... 0x8FF
	   including hex pages, and we store at most subpages
	   0x00 ... 0x79. */
	if (CACHE_CONSISTENCY)
		assert (cn->n_pages <= 0x800 * 80);

	if (cn->n_pages > cn->max_pages)
		cn->max_pages = cn->n_pages;

	ps = cache_network_page_stat (cn, cp->pgno);

	++ps->n_subpages;

	if (ps->n_subpages > ps->max_subpages)
		ps->max_subpages = ps->n_subpages;

	/* See above. */
	if (CACHE_CONSISTENCY)
		assert (ps->n_subpages <= 80);

	/* See above. */
	if (CACHE_CONSISTENCY)
		assert ((unsigned int) cp->subno <= 0x79);

	if (0 == ps->subno_min /* none yet */
	    || cp->subno < ps->subno_min)
		ps->subno_min = cp->subno;

	if (cp->subno > ps->subno_max)
		ps->subno_max = cp->subno;
}

static cache_network *
network_by_id			(vbi_cache *		ca,
				 const vbi_network *	nk)
{
	cache_network *cn, *cn1;

	/* Shortcut if this is one of our pointers (e.g. event->network). */
	FOR_ALL_NODES (cn, cn1, &ca->networks, node)
		if (&cn->network == nk)
			goto found;

	if (nk->user_data) {
		FOR_ALL_NODES (cn, cn1, &ca->networks, node)
			if (cn->network.user_data == nk->user_data)
				goto found2;

		/* Perhaps no user_data has been assigned to this
		   network yet. Try to find it by CNI or call_sign. */
	}

	if (nk->cni_vps) {
		FOR_ALL_NODES (cn, cn1, &ca->networks, node)
			if (cn->network.cni_vps == nk->cni_vps)
				goto found3;

		/* Perhaps we did not receive nk->cni_vps yet but
		   another CNI or call_sign. */
	}

	if (nk->cni_8301)
		FOR_ALL_NODES (cn, cn1, &ca->networks, node)
			if (cn->network.cni_8301 == nk->cni_8301)
				goto found3;

	if (nk->cni_8302)
		FOR_ALL_NODES (cn, cn1, &ca->networks, node)
			if (cn->network.cni_8302 == nk->cni_8302)
				goto found3;

	if (nk->call_sign[0])
		FOR_ALL_NODES (cn, cn1, &ca->networks, node)
			if (0 == strcmp (cn->network.call_sign, nk->call_sign))
				goto found3;

	return NULL;

	/* All given IDs must match unless the ID is not stored yet. */

 found3:
	if (nk->user_data && cn->network.user_data)
		return NULL;

 found2:
	if (nk->cni_vps && cn->network.cni_vps
	    && cn->network.cni_vps != nk->cni_vps)
		return NULL;

	if (nk->cni_8301 && cn->network.cni_8301
	    && cn->network.cni_8301 != nk->cni_8301)
		return NULL;

	if (nk->cni_8302 && cn->network.cni_8302
	    && cn->network.cni_8302 != nk->cni_8302)
		return NULL;

	if (nk->call_sign[0] && cn->network.call_sign[0]
	    && 0 != strcmp (cn->network.call_sign, nk->call_sign))
		return NULL;

 found:
	/* Find faster next time, delete last. */
	add_head (&ca->networks, unlink_node (&cn->node));

	return cn;
}

static void
delete_all_pages		(vbi_cache *		ca,
				 cache_network *	cn);

static void
delete_network			(vbi_cache *		ca,
				 cache_network *	cn)
{
	vbi_pid_channel ch;

	if (CACHE_CONSISTENCY) {
		assert (ca == cn->cache);
		assert (is_member (&ca->networks, &cn->node));
	}

	if (CACHE_DEBUG) {
		fputs ("Delete ", stderr);
		cache_network_dump (cn, stderr);
		fputc ('\n', stderr);
	}

	if (cn->n_pages > 0) {
		/* Delete all unreferenced pages. */
		delete_all_pages (ca, cn);
	}

	/* Zombies don't count. */
	if (!cn->zombie)
		--ca->n_networks;

	if (cn->ref_count > 0
	    || cn->n_referenced_pages > 0) {
		cn->zombie = TRUE;
		return;
	}

	unlink_node (&cn->node);

	vbi_network_destroy (&cn->network);

	vbi_program_info_destroy (&cn->program_info);
	vbi_aspect_ratio_destroy (&cn->aspect_ratio);

	for (ch = 0; ch < N_ELEMENTS (cn->program_id); ++ch)
		vbi_program_id_destroy (&cn->program_id[ch]);

	cache_network_destroy_caption (cn);
	cache_network_destroy_teletext (cn);

	free (cn);
}

static void
delete_surplus_networks		(vbi_cache *		ca)
{
	cache_network *cn, *cn1;

	/* Remove last recently used networks first. */
	FOR_ALL_NODES_REVERSE (cn, cn1, &ca->networks, node) {
		if (cn->ref_count > 0
		    || cn->n_referenced_pages > 0)
			continue;

		if (cn->zombie
		    || ca->n_networks > ca->network_limit)
			delete_network (ca, cn);
	}
}

static cache_network *
add_network			(vbi_cache *		ca,
				 const vbi_network *	nk,
				 vbi_videostd_set	videostd_set)
{
	cache_network *cn, *cn1;
	vbi_pid_channel ch;

	if (nk && (cn = network_by_id (ca, nk))) {
		/* Note does not merge nk. */
		return cn;
	}

	/* Allow +1 for channel change. */
	if (ca->n_networks >= ca->network_limit + 1) {
		/* We absorb the last recently used cache_network
		   without references. */

		FOR_ALL_NODES_REVERSE (cn, cn1, &ca->networks, node)
			if (0 == cn->ref_count
			    && 0 == cn->n_referenced_pages)
				break;

		if (!cn->node.pred) {
			if (CACHE_DEBUG)
				fprintf (stderr,
					 "%s: network_limit=%u, all referenced\n",
					 __FUNCTION__, ca->network_limit);
			return NULL;
		}

		if (cn->n_pages > 0)
			delete_all_pages (ca, cn);

		unlink_node (&cn->node);

		cn->ref_count = 0;

		cn->zombie = FALSE;

		vbi_network_destroy (&cn->network);

		cn->confirm_cni_vps = 0;
		cn->confirm_cni_8301 = 0;
		cn->confirm_cni_8302 = 0;

		vbi_program_info_destroy (&cn->program_info);
		vbi_aspect_ratio_destroy (&cn->aspect_ratio);

		for (ch = 0; ch < N_ELEMENTS (cn->program_id); ++ch)
			vbi_program_id_destroy (&cn->program_id[ch]);

		cn->n_pages = 0;
		cn->max_pages = 0;

		cn->n_referenced_pages = 0;

		cache_network_destroy_caption (cn);
		cache_network_destroy_teletext (cn);
	} else {
		if (!(cn = calloc (1, sizeof (*cn)))) {
			if (CACHE_DEBUG)
				fprintf (stderr, "%s: out of memory\n",
					 __FUNCTION__);
			return NULL;
		}

		++ca->n_networks;
	}

	add_head (&ca->networks, &cn->node);

	cn->cache = ca;

	if (nk)
		vbi_network_copy (&cn->network, nk);

	vbi_program_info_init (&cn->program_info);
	vbi_aspect_ratio_init (&cn->aspect_ratio, videostd_set);

	for (ch = 0; ch < N_ELEMENTS (cn->program_id); ++ch)
		vbi_program_id_init (&cn->program_id[ch], ch);

	cache_network_init_caption (cn);
	cache_network_init_teletext (cn);

	return cn;
}

/**
 * @internal
 * @param cp Teletext page.
 * 
 * @returns
 * Storage size required for the raw Teletext page,
 * depending on its function and the data union member used.
 */
unsigned int
cache_page_size			(const cache_page *	cp)
{
	const unsigned int header_size = sizeof (*cp) - sizeof (cp->data);

	switch (cp->function) {
	case PAGE_FUNCTION_UNKNOWN:
	case PAGE_FUNCTION_LOP:
		if (cp->x28_designations & 0x13)
			return header_size + sizeof (cp->data.ext_lop);
		else if (cp->x26_designations)
			return header_size + sizeof (cp->data.enh_lop);
		else
			return header_size + sizeof (cp->data.lop);

	case PAGE_FUNCTION_GPOP:
	case PAGE_FUNCTION_POP:
		return header_size + sizeof (cp->data.pop);

	case PAGE_FUNCTION_GDRCS:
	case PAGE_FUNCTION_DRCS:
		return header_size + sizeof (cp->data.drcs);

	case PAGE_FUNCTION_AIT:
		return header_size + sizeof (cp->data.ait);

	default:
		return sizeof (*cp);
	}
}

/** internal */
vbi_bool
cache_page_copy			(cache_page *		dst,
				 const cache_page *	src)
{
	if (dst == src)
		return TRUE;

	assert (NULL != dst);

	if (src) {
		memcpy (dst, src, cache_page_size (src));

		dst->network = NULL; /* not cached */
	} else {
		CLEAR (*dst);
	}

	return TRUE;
}

static void
cache_page_dump			(const cache_page *	cp,
				 FILE *			fp)
{
	const cache_network *cn;

	fprintf (fp, "page %x.%x ", cp->pgno, cp->subno);

	if ((cn = cp->network)) {
		const page_stat *ps;

		ps = cache_network_const_page_stat (cn, cp->pgno);

		fprintf (fp, "%s/L%u/S%04x subp=%u/%u (%u-%u) ",
			 vbi_ttx_page_type_name (ps->page_type),
			 ps->charset_code,
			 ps->subcode,
			 ps->n_subpages,
			 ps->max_subpages,
			 ps->subno_min,
			 ps->subno_max);
	}

	fprintf (stderr, "ref=%u %s",
		 cp->ref_count, cache_priority_name (cp->priority));
}

vbi_inline unsigned int
hash				(vbi_pgno		pgno)
{
	return pgno % HASH_SIZE;
}

static vbi_bool
cache_page_in_cache		(const vbi_cache *	ca,
				 const cache_page *	cp)
{
	const list *hash_list;
	const list *pri_list;

	hash_list = &ca->hash[hash (cp->pgno)];

	if (cp->ref_count > 0)
		pri_list = &ca->referenced;
	else
		pri_list = &ca->priority;

	return (is_member (hash_list, &cp->hash_node)
		&& is_member (pri_list, &cp->pri_node));
}

static void
delete_page			(vbi_cache *		ca,
				 cache_page *		cp)
{
	if (CACHE_CONSISTENCY) {
		assert (NULL != cp->network);
		assert (ca == cp->network->cache);
		assert (cache_page_in_cache (ca, cp));
	}

	if (cp->ref_count > 0) {
		/* Remove from cache, mark for deletion.
		   cp->pri_node remains on ca->referenced. */

		unlink_node (&cp->hash_node);

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

	/* Zombies don't count. */
	if (CACHE_PRI_ZOMBIE != cp->priority)
		ca->memory_used -= cache_page_size (cp);

	unlink_node (&cp->pri_node);
	unlink_node (&cp->hash_node);

	cache_network_remove_page (cp->network, cp);

	free (cp);

	--ca->n_pages;
}

static void
delete_all_pages		(vbi_cache *		ca,
				 cache_network *	cn)
{
	cache_page *cp, *cp1;

	if (CACHE_CONSISTENCY && NULL != cn) {
		assert (ca == cn->cache);
		assert (is_member (&ca->networks, &cn->node));
	}

	FOR_ALL_NODES (cp, cp1, &ca->priority, pri_node)
		if (!cn || cp->network == cn)
			delete_page (ca, cp);
}

static void
delete_surplus_pages		(vbi_cache *		ca)
{
	cache_priority pri;
	cache_page *cp, *cp1;

	for (pri = CACHE_PRI_NORMAL; pri <= CACHE_PRI_SPECIAL; ++pri) {
		FOR_ALL_NODES (cp, cp1, &ca->priority, pri_node) {
			if (ca->memory_used <= ca->memory_limit)
				return;
			else if (cp->priority == pri
				 && 0 == cp->network->ref_count)
				delete_page (ca, cp);
		}
	}

	for (pri = CACHE_PRI_NORMAL; pri <= CACHE_PRI_SPECIAL; ++pri) {
		FOR_ALL_NODES (cp, cp1, &ca->priority, pri_node) {
			if (ca->memory_used <= ca->memory_limit)
				return;
			else if (cp->priority == pri)
				delete_page (ca, cp);
		}
	}
}

static cache_page *
page_by_pgno			(vbi_cache *		ca,
				 const cache_network *	cn,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 vbi_subno		subno_mask)
{
	list *hash_list;
	cache_page *cp, *cp1;

	if (CACHE_CONSISTENCY) {
		assert (ca == cn->cache);
		assert (is_member (&ca->networks, &cn->node));
	}

	hash_list = ca->hash + hash (pgno);

	FOR_ALL_NODES (cp, cp1, hash_list, hash_node) {
		if (CACHE_DEBUG > 1) {
			fputs ("Try ", stderr);
			cache_page_dump (cp, stderr);
			fputc ('\n', stderr);
		}

		if (cp->pgno == pgno
		    && (cp->subno & subno_mask) == subno
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

	if (CACHE_CONSISTENCY) {
		assert (ca == cn->cache);
		assert (is_member (&ca->networks, &cn->node));
	}

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

		++cn->n_referenced_pages;

		ca->memory_used -= cache_page_size (cp);

		add_tail (&ca->referenced, unlink_node (&cp->pri_node));
	}

	if (CACHE_DEBUG)
		fputc ('\n', stderr);

	++cp->ref_count;

	return cp;
}

/**
 * @internal
 * @param cp
 *
 * Unreferences a page returned by vbi_cache_put_cache_page(),
 * vbi_cache_get_cache_page(), or vbi_page_new_cache_page_ref().
 * @a cp can be @c NULL.
 */
void
cache_page_release		(cache_page *		cp)
{
	vbi_cache *ca;

	if (NULL == cp)
		return;

	assert (NULL != cp->network);
	assert (NULL != cp->network->cache);

	ca = cp->network->cache;

	if (CACHE_CONSISTENCY)
		assert (cache_page_in_cache (ca, cp));

	if (0 == cp->ref_count) {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"Unreferenced page %p", cp);
		return;
	}

	if (CACHE_DEBUG) {
		fputs ("Unref ", stderr);
		_vbi_cache_dump (ca, stderr);
		fputc (' ', stderr);
		cache_page_dump (cp, stderr);
	}

	if (1 == cp->ref_count) {
		cache_network *cn;

		cp->ref_count = 0;

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

			add_tail (&ca->priority, unlink_node (&cp->pri_node));

			ca->memory_used += cache_page_size (cp);

			break;
		}

		--cn->n_referenced_pages;

		if (cn->zombie
		    && 0 == cn->n_referenced_pages
		    && 0 == cn->ref_count)
			delete_network (ca, cn);

		if (ca->memory_used > ca->memory_limit)
			delete_surplus_pages (ca);
	} else {
		--cp->ref_count;
	}

	if (CACHE_DEBUG)
		fputc ('\n', stderr);
}

/**
 * @internal
 * @param cp
 *
 * Duplicates a page reference.
 *
 * @returns
 * @a cp, never fails.
 */
cache_page *
cache_page_new_ref		(cache_page *		cp)
{
	assert (NULL != cp);

	++cp->ref_count;

	return cp;
}

/**
 * @internal
 *
 * Gets a page from the cache. When @a subno is VBI_SUB_ANY, the most
 * recently received subpage of that page is returned.
 * 
 * The reference counter of the page is incremented, you must call
 * vbi_cache_release_cache_page() to unreference the page. When @a user_access
 * is set, the page gets higher priority to stay in cache and is marked
 * as most recently received subpage.
 * 
 * @return 
 * cache_page pointer, NULL when the requested page is not cached.
 */
cache_page *
_vbi_cache_get_page		(vbi_cache *		ca,
				 cache_network *	cn,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 vbi_subno		subno_mask)
{
	cache_page *cp;

	assert (NULL != ca);
	assert (NULL != cn);

	assert (ca == cn->cache);

	if (pgno < 0x100 || pgno > 0x8FF) {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"pgno 0x%x out of bounds",
				pgno);
		return NULL;
	}

	if (CACHE_CONSISTENCY)
		assert (is_member (&ca->networks, &cn->node));

	if (CACHE_DEBUG) {
		fprintf (stderr, "Ref %x.%x/%x ",
			 pgno, subno, subno_mask);
		_vbi_cache_dump (ca, stderr);
		fputc (' ', stderr);
		cache_network_dump (cn, stderr);
		fputc ('\n', stderr);
	}

	if (VBI_ANY_SUBNO == subno)
		subno_mask = 0;

	if (!(cp = lock_page (ca, (cache_network *) cn,
			      pgno, subno, subno_mask)))
		goto failure;

	return cp;

 failure:
	return NULL;
}

/**
 * @internal
 * @param ca Cache.
 * @param cn Network this page belongs to.
 * @param cp Teletext page to store in the cache.
 * @param new_ref When @c TRUE, the stored page will be referenced
 *   as with vbi_cache_get_cache_page().
 *
 * Puts a copy of @a cp in the cache.
 * 
 * @returns
 * cache_page pointer (in the cache, not @a cp), NULL on failure
 * (out of memory).
 */
cache_page *
_vbi_cache_put_page		(vbi_cache *		ca,
				 cache_network *	cn,
				 const cache_page *	cp)
{
	cache_page *death_row[20];
	unsigned int death_count;
	cache_page *old_cp;
	long memory_available;	/* NB can be < 0 */
	long memory_needed;
	cache_priority pri;
	cache_page *new_cp;

	assert (NULL != ca);
	assert (NULL != cn);
	assert (NULL != cp);

	assert (ca == cn->cache);

	memory_needed = cache_page_size (cp);
	memory_available = ca->memory_limit - ca->memory_used;

	if (CACHE_CONSISTENCY)
		assert (is_member (&ca->networks, &cn->node));

	if (CACHE_DEBUG) {
		fprintf (stderr, "Put %x.%x ", cp->pgno, cp->subno);
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

		ps = cache_network_const_page_stat (cn, cp->pgno);

		if (VBI_NONSTD_SUBPAGES == (vbi_ttx_page_type) ps->page_type)
			subno_mask = 0;
		else
			subno_mask = - ((unsigned int) cp->subno <= 0x79);

		old_cp = page_by_pgno (ca, cn, cp->pgno,
				       cp->subno, subno_mask);
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

			old_cp->priority = CACHE_PRI_ZOMBIE;
			old_cp = NULL;
		} else {
			/* Got our first replacement candidate. */
			death_row[death_count++] = old_cp;
			memory_available += cache_page_size (old_cp);
		}
	}

	if (memory_available >= memory_needed)
		goto replace;

	/* Find more pages to replace until have enough memory. */

	for (pri = CACHE_PRI_NORMAL; pri <= CACHE_PRI_SPECIAL; ++pri) {
		cache_page *cp, *cp1;

		FOR_ALL_NODES (cp, cp1, &ca->priority, pri_node) {
			if (memory_available >= memory_needed)
				goto replace;

			if (pri != cp->priority
			    || cp->network->ref_count > 0
			    || cp == old_cp)
				continue;

			assert (death_count < N_ELEMENTS (death_row));

			death_row[death_count++] = cp;
			memory_available += cache_page_size (cp);
		}
	}

	for (pri = CACHE_PRI_NORMAL; pri <= CACHE_PRI_SPECIAL; ++pri) {
		cache_page *cp, *cp1;

		FOR_ALL_NODES (cp, cp1, &ca->priority, pri_node) {
			if (memory_available >= memory_needed)
				goto replace;

			if (pri != cp->priority
			    || cp == old_cp)
				continue;

			assert (death_count < N_ELEMENTS (death_row));

			death_row[death_count++] = cp;
			memory_available += cache_page_size (cp);
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
		/* Usually we can replace a single page of same size. */

		new_cp = death_row[0];

		if (CACHE_DEBUG) {
			fputs ("reusing ", stderr);
			cache_page_dump (new_cp, stderr);
			fputc (' ', stderr);
		}

		unlink_node (&new_cp->pri_node);
		unlink_node (&new_cp->hash_node);

		cache_network_remove_page (new_cp->network, new_cp);

		ca->memory_used -= memory_needed;
	} else {
		unsigned int i;

		if (!(new_cp = malloc (memory_needed))) {
			if (CACHE_DEBUG)
				fputs ("out of memory\n", stderr);

			goto failure;
		}

		for (i = 0; i < death_count; ++i)
			delete_page (ca, death_row[i]);

		++ca->n_pages;
	}

	add_head (ca->hash + hash (cp->pgno), &new_cp->hash_node);

	/* 100, 200, 300, ... magazine start page. */
	if (0x00 == (cp->pgno & 0xFF))
		new_cp->priority = CACHE_PRI_SPECIAL;
	/* 111, 222, 333, ... magic page number. */
	else if ((cp->pgno >> 4) == (cp->pgno & 0xFF))
		new_cp->priority = CACHE_PRI_SPECIAL;
	/* Something we may not want in cache, much less all subpages. */
	else if (PAGE_FUNCTION_UNKNOWN == cp->function)
		new_cp->priority = CACHE_PRI_NORMAL;
	/* POP, GPOP, DRCS, GDRCS */
	else if (PAGE_FUNCTION_LOP != cp->function)
		new_cp->priority = CACHE_PRI_SPECIAL;
	/* Regular subpage, not clock, not rotating ads etc. */
	else if (cp->subno > 0x00 && cp->subno < 0x79)
		new_cp->priority = CACHE_PRI_SPECIAL;
	else
		new_cp->priority = CACHE_PRI_NORMAL;

	new_cp->function		= cp->function;

	new_cp->pgno			= cp->pgno;
	new_cp->subno			= cp->subno;

	new_cp->national		= cp->national;

	new_cp->flags			= cp->flags;

	new_cp->lop_packets		= cp->lop_packets;
	new_cp->x26_designations	= cp->x26_designations;
	new_cp->x27_designations	= cp->x27_designations;
	new_cp->x28_designations	= cp->x28_designations;

	memcpy (&new_cp->data, &cp->data,
		memory_needed - (sizeof (*new_cp) - sizeof (new_cp->data)));

	new_cp->ref_count = 1;
	ca->memory_used += 0; /* see lock_page() */

	++cn->n_referenced_pages;

	add_tail (&ca->referenced, &new_cp->pri_node);

	cache_network_add_page (cn, new_cp);

	if (CACHE_STATUS) {
		fprintf (stderr, "cache status:\n");
		_vbi_cache_dump (ca, stderr);
		fputc ('\n', stderr);
		cache_page_dump (new_cp, stderr);
		fputc ('\n', stderr);
		cache_network_dump (new_cp->network, stderr);
		fputc ('\n', stderr);
	}

	return new_cp;

 failure:
	return NULL;
}

/**
 */
void
vbi_ttx_page_stat_destroy	(vbi_ttx_page_stat *	ps)
{
	assert (NULL != ps);

	CLEAR (*ps);
}

/**
 */
void
vbi_ttx_page_stat_init		(vbi_ttx_page_stat *	ps)
{
	assert (NULL != ps);

	ps->page_type		= VBI_UNKNOWN_PAGE;

	ps->charset_code	= 0; /* en */

	ps->subpages		= 0;

	ps->subno_min		= 0;
	ps->subno_max		= 0;
}

/**
 * @internal
 */
void
cache_network_get_ttx_page_stat	(const cache_network *	cn,
				 vbi_ttx_page_stat *	ps,
				 vbi_pgno		pgno)
{
	const page_stat *ps1;

	assert (NULL != ps);

	ps1 = cache_network_const_page_stat (cn, pgno);

	if (VBI_NORMAL_PAGE == (vbi_ttx_page_type) ps1->page_type) {
		unsigned int flags;

		flags = ps1->flags & (C5_NEWSFLASH |
				      C6_SUBTITLE |
				      C7_SUPPRESS_HEADER);

		if ((C5_NEWSFLASH | C7_SUPPRESS_HEADER) == flags)
			ps->page_type = VBI_NEWSFLASH_PAGE;
		else if ((C6_SUBTITLE | C7_SUPPRESS_HEADER) == flags)
			ps->page_type = VBI_SUBTITLE_PAGE;
		else
			ps->page_type = VBI_NORMAL_PAGE;
	} else {
		ps->page_type = (vbi_ttx_page_type) ps1->page_type;
	}

	if (0xFF == ps1->charset_code)
		ps->charset_code = 0; /* unknown -> en */
	else
		ps->charset_code = (vbi_character_set_code) ps1->charset_code;

	if (ps1->subcode <= 9)
		ps->subpages	= ps1->subcode; /* common */
	else if (SUBCODE_UNKNOWN == ps1->subcode)
		ps->subpages	= 0;
	else if (SUBCODE_MULTI_PAGE == ps1->subcode)
		ps->subpages	= 2; /* two or more */
	else if (ps1->subcode >= 0x80)
		ps->subpages	= 0; /* non-standard (clock etc) */
	else
		ps->subpages	= vbi_bcd2dec (ps1->subcode);

	ps->subno_min	= (vbi_subno) ps1->subno_min;
	ps->subno_max	= (vbi_subno) ps1->subno_max;
}

/**
 */
vbi_bool
vbi_cache_get_ttx_page_stat	(vbi_cache *		ca,
				 vbi_ttx_page_stat *	ps,
				 const vbi_network *	nk,
				 vbi_pgno		pgno)
{
	cache_network *cn;

	assert (NULL != ca);
	assert (NULL != ps);
	assert (NULL != nk);

	if (pgno < 0x100 || pgno > 0x8FF)
		return FALSE;

	if (!(cn = _vbi_cache_get_network (ca, nk)))
		return FALSE;

	cache_network_get_ttx_page_stat	(cn, ps, pgno);

	cache_network_release (cn);

	return TRUE;
}

/**
 */
vbi_network *
vbi_cache_get_networks		(vbi_cache *		ca,
				 unsigned int *		array_size)
{
	vbi_network *nk;
	cache_network *cn, *cn1;
	unsigned int size;
	unsigned int i;

	assert (NULL != ca);
	assert (NULL != array_size);

	*array_size = 0;

	if (0 == ca->n_networks)
		return NULL;

	size = ca->n_networks * sizeof (*nk);

	if (!(nk = malloc (size))) {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"Out of memory (%u)", size);
		return NULL;
	}

	i = 0;

	FOR_ALL_NODES (cn, cn1, &ca->networks, node) {
		assert (i < ca->n_networks);

		if (!(vbi_network_copy (nk + i, &cn->network))) {
			vbi_network_array_delete (nk, i);
			return NULL;
		}

		++i;
	}

	*array_size = i;

	return nk;
}

/**
 * @internal
 * @param cn cache_network obtained with _vbi_cache_add_cache_network()
 *   or _vbi_cache_get_cache_network(), can be @c NULL.
 *
 * Releases a network reference.
 */
void
cache_network_release		(cache_network *	cn)
{
	vbi_cache *ca;

	if (NULL == cn)
		return;

	assert (NULL != cn->cache);

	ca = cn->cache;

	if (CACHE_CONSISTENCY)
		assert (is_member (&ca->networks, &cn->node));

	if (0 == cn->ref_count) {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"Unreferenced network %p", cn);
		return;
	} else if (1 == cn->ref_count) {
		cn->ref_count = 0;

		if (0 == cn->n_referenced_pages
		    && (cn->zombie
			|| ca->n_networks > ca->network_limit))
			delete_surplus_networks (ca);
	} else {
		--cn->ref_count;
	}
}

/**
 * @internal
 * @param cn
 *
 * Duplicates a network reference.
 *
 * @returns
 * @a cn, never fails.
 */
cache_network *
cache_network_new_ref		(cache_network *	cn)
{
	assert (NULL != cn);

	++cn->ref_count;

	return cn;
}

/**
 * @internal
 * @param ca Cache allocated with vbi_cache_new().
 * @param nk Identifies the network by cni_8301, cni_8302, cni_vps,
 *   call_sign or user_data, whichever is non-zero.
 *
 * Finds a network in the cache.
 *
 * @returns
 * Pointer to a cache_network structure, @c NULL on error.  You must call
 * _vbi_cache_release_cache_network() when this reference is no longer
 * needed.
 */
cache_network *
_vbi_cache_get_network		(vbi_cache *		ca,
				 const vbi_network *	nk)
{
	cache_network *cn;

	assert (NULL != ca);
	assert (NULL != nk);

	if ((cn = network_by_id (ca, nk))) {
		if (cn->zombie) {
			++ca->n_networks;
			cn->zombie = FALSE;
		}

		++cn->ref_count;
	}

	return cn;
}

/**
 * @internal
 * @param ca Cache allocated with vbi_cache_new().
 * @param nk Create network with this cni_8301, cni_8302, cni_vps,
 *   call_sign or temp_id, whichever is non-zero. Can be @c NULL
 *   to add an anonymous network.
 *
 * Adds a network to the cache.
 *
 * @returns
 * Pointer to a new cache_network structure, an already existing
 * structure or @c NULL on error.  You must call
 * _vbi_cache_release_cache_network() when this reference is no
 * longer needed.
 */
cache_network *
_vbi_cache_add_network		(vbi_cache *		ca,
				 const vbi_network *	nk,
				 vbi_videostd_set	videostd_set)
{
	cache_network *cn;

	assert (NULL != ca);

	if ((cn = add_network (ca, nk, videostd_set))) {
		++cn->ref_count;
	}

	return cn;
}

/**
 * @internal
 * @param ca Cache allocated with vbi_cache_new().
 *
 * Deletes all cache contents. Referenced networks and Teletext pages
 * are marked for deletion when unreferenced.
 */
static void
vbi_cache_purge			(vbi_cache *		ca)
{
	cache_network *cn, *cn1;

	assert (NULL != ca);

	FOR_ALL_NODES (cn, cn1, &ca->networks, node)
		delete_network (ca, cn);
}

/**
 * @internal
 * @param ca Cache allocated with vbi_cache_new().
 * @param limit Amount of memory in bytes.
 *
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

	ca->memory_limit = SATURATE (limit, 1 << 10, 1 << 30);

	delete_surplus_pages (ca);
}

/**
 * @internal
 * @param ca Cache allocated with vbi_cache_new().
 * @param limit Number of networks.
 *
 * Limits the number of networks cached. The default is 1
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

	ca->network_limit = SATURATE (limit, 1, 3000);

	delete_surplus_networks (ca);
}

/**
 * @param ca Cache allocated with vbi_cache_new(), can be @c NULL.
 *
 * Frees all resources associated with the cache, regardless of
 * any remaining references to it.
 */
void
vbi_cache_delete		(vbi_cache *		ca)
{
	unsigned int i;

	if (NULL == ca)
		return;

	vbi_cache_purge (ca);

	if (!empty_list (&ca->referenced)) {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"Warning: some cached pages still "
				"referenced, memory leaks.\n");
	}

	if (!empty_list (&ca->networks)) {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"Warning: some cached networks still "
				"referenced, memory leaks.\n");
	}

	list_destroy (&ca->networks);
	list_destroy (&ca->priority);
	list_destroy (&ca->referenced);

	for (i = 0; i < N_ELEMENTS (ca->hash); ++i)
		list_destroy (ca->hash + i);

	CLEAR (*ca);

	free (ca);
}

/**
 * @param ca Cache allocated with vbi_cache_new(), can be @c NULL.
 *
 * Releases a cache reference. When this is the last reference
 * the function calls vbi_cache_delete().
 */
void
vbi_cache_release		(vbi_cache *		ca)
{
	if (NULL == ca)
		return;

	if (ca->ref_count > 1) {
		--ca->ref_count;
		return;
	}

	vbi_cache_delete (ca);
}

/**
 * @param ca Cache allocated with vbi_cache_new().
 *
 * Creates a new reference to the cache.
 *
 * @returns
 * @a ca. You must call vbi_cache_release() when the reference is
 * no longer needed.
 */
vbi_cache *
vbi_cache_new_ref		(vbi_cache *		ca)
{
	assert (NULL != ca);

	++ca->ref_count;

	return ca;
}

/**
 * Allocates a new cache for VBI decoders.
 *
 * A cache is a shared object with a reference counter. To create
 * a new reference call vbi_cache_new_ref().
 *
 * @returns
 * Pointer to newly allocated cache which must be freed with
 * vbi_cache_release() or vbi_cache_delete() when done. @c NULL on
 * failure (out of memory).
 */
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

	list_init (&ca->referenced);
	list_init (&ca->priority);
	list_init (&ca->networks);

	ca->memory_limit = 1 << 30;
	ca->network_limit = 1;

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


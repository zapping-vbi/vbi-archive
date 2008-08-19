/*
 *  libzvbi -- Teletext cache
 *
 *  Copyright (C) 2001, 2002, 2003, 2004, 2007 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 */

#include "site_def.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>

#include "version.h"
#if 2 == VBI_VERSION_MINOR
#  include "event.h"
#  include "cache-priv.h"
#  include "intl-priv.h"
#  include "vbi.h"		/* vbi_page_type */
#  define VBI_CLOCK_PAGE VBI_NONSTD_SUBPAGES
#elif 3 == VBI_VERSION_MINOR
#  include "event-priv.h"
#  include "cache-priv.h"
#  ifdef ZAPPING8
#    include "common/intl-priv.h"
#  else
#    include "intl-priv.h"
#  endif
#else
#  error VBI_VERSION_MINOR == ?
#endif

#include "ttx_page_stat.h"
#include "ttx.h"

/* Enable logging by default (site_def.h). */
#ifndef CACHE_DEBUG
#  define CACHE_DEBUG 0
#endif

/* Compile status reports in. */
#ifndef CACHE_STATUS
#  define CACHE_STATUS 0
#endif

/* Compile cache consistency checks in. */
#ifndef CACHE_CONSISTENCY
#  define CACHE_CONSISTENCY 0
#endif


static void
delete_all_pages		(vbi_cache *		ca,
				 cache_network *	cn);

#if 0
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
		assert (0);
		return NULL;
	}
}
#endif

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
	struct ttx_page_stat *ps;

	if (CACHE_CONSISTENCY)
		assert (cn == cp->network);

	cp->network = NULL;

	--cn->n_cached_pages;

	ps = cache_network_page_stat (cn, cp->pgno);

	--ps->n_subpages;
}

/* Adds Teletext page to network statistics. */
static void
cache_network_add_page		(cache_network *	cn,
				 cache_page *		cp)
{
	struct ttx_page_stat *ps;

	if (cn->zombie) {
		assert (NULL != cn->cache);

		++cn->cache->n_cached_networks;
		cn->zombie = FALSE;
	}

	cp->network = cn;

	++cn->n_cached_pages;

	/* Consistency check: page number range 0x100 ... 0x8FF
	   including hex pages, and we store at most subpages
	   0x00 ... 0x79. */
	if (CACHE_CONSISTENCY)
		assert (cn->n_cached_pages <= 0x800 * 80);

	if (cn->n_cached_pages > cn->max_cached_pages)
		cn->max_cached_pages = cn->n_cached_pages;

	ps = cache_network_page_stat (cn, cp->pgno);

	++ps->n_subpages;

	if (ps->n_subpages > ps->max_subpages)
		ps->max_subpages = ps->n_subpages;

	/* See above. */
	if (CACHE_CONSISTENCY)
		assert (ps->n_subpages <= 80);

	if (CACHE_CONSISTENCY) {
		assert (cp->pgno >= 0);
		assert (cp->pgno <= 0x8FF);
		assert (cp->subno >= 0);
		assert (cp->subno <= 0x3F7F);

		if (vbi_is_bcd (cp->pgno)) {
			/* We store only subno numbers
			   0 - no subpages,
			   0x01 ... 0x79 bcd - regular subpages,
			   0x0000 ... 0x2359 bcd - clock page. */
			assert (vbi_is_bcd (cp->subno));
			if (cp->subno >= 0x0100) {
				assert (cp->subno <= 0x2359);
				assert ((cp->subno & 0xFF) <= 0x59);
			} else {
				assert (cp->subno <= 0x79);
			}
		} else {
			/* We do not store filler/terminator. */
			assert (0xFF != (cp->pgno & 0xFF));

			/* All subcodes are valid (0xnnXs). */
			assert (0 == (cp->subno & ~0x3F7F));
		}
	}

	if (0 == ps->subno_min /* none yet */
	    || cp->subno < ps->subno_min)
		ps->subno_min = cp->subno;

	if (cp->subno > ps->subno_max)
		ps->subno_max = cp->subno;
}

static void
delete_network			(vbi_cache *		ca,
				 cache_network *	cn)
{
	if (CACHE_CONSISTENCY) {
		assert (ca == cn->cache);
		assert (is_member (&ca->networks, &cn->node));
	}

	if (CACHE_DEBUG) {
		fputs ("Delete ", stderr);
		cache_network_dump (cn, stderr);
		fputc ('\n', stderr);
	}

	if (cn->n_cached_pages > 0) {
		/* Delete all unreferenced pages. */
		delete_all_pages (ca, cn);
	}

	/* Zombies don't count. */
	if (!cn->zombie)
		--ca->n_cached_networks;

#if 0
	if (ca->handlers.event_mask & VBI_EVENT_REMOVE_NETWORK) {
		vbi_event e;

		CLEAR (e);

		e.type		= VBI_EVENT_REMOVE_NETWORK;
		e.network	= &cn->network;

		_vbi_event_handler_list_send (&ca->handlers, &e);
	}
#endif
	if (cn->ref_count > 0
	    || cn->n_referenced_pages > 0) {
		cn->zombie = TRUE;
		return;
	}

	unlink_node (&cn->node);

	vbi_network_destroy (&cn->network);

#if 0
	vbi_program_info_destroy (&cn->program_info);
	vbi_aspect_ratio_destroy (&cn->aspect_ratio);

	{
		vbi_pid_channel ch;

		for (ch = 0; ch < N_ELEMENTS (cn->program_id); ++ch)
			vbi_program_id_destroy (&cn->program_id[ch]);
	}
#endif /* !ZAPPING8 */

//	cache_network_destroy_caption (cn);
	cache_network_destroy_teletext (cn);

	CLEAR (*cn);

	vbi_cache_free (cn);
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

	FOR_ALL_NODES (cn, cn1, &ca->networks, node) {
		delete_network (ca, cn);
	}
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
		    || vbi_network_anonymous (&cn->network)
		    || ca->n_cached_networks > ca->n_networks_limit)
			delete_network (ca, cn);
	}
}

#if 0

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
vbi_cache_set_network_limit	(vbi_cache *		ca,
				 unsigned int		limit)
{
	assert (NULL != ca);

	if (2 == VBI_VERSION_MINOR)
		ca->n_networks_limit = 1;
	else
		ca->n_networks_limit = SATURATE (limit, 1, 3000);

	delete_surplus_networks (ca);
}

#endif

static cache_network *
network_by_id			(vbi_cache *		ca,
				 const vbi_network *	nk)
{
	cache_network *cn, *cn1;

	/* Shortcut if this is one of our pointers (e.g. event->network). */
	FOR_ALL_NODES (cn, cn1, &ca->networks, node)
		if (&cn->network == nk)
			goto found;

#if 3 == VBI_VERSION_MINOR
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
#endif /* 3 == VBI_VERSION_MINOR */

	return NULL;

#if 3 == VBI_VERSION_MINOR
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
#endif /* 3 == VBI_VERSION_MINOR */

 found:
	/* Find faster next time, delete last. */
	add_head (&ca->networks, unlink_node (&cn->node));

	return cn;
}

static cache_network *
recycle_network			(vbi_cache *		ca)
{
	cache_network *cn, *cn1;

	/* We absorb the last recently used cache_network
	   without references. */

	FOR_ALL_NODES_REVERSE (cn, cn1, &ca->networks, node) {
		if (0 == cn->ref_count
		    && 0 == cn->n_referenced_pages) {
			goto found;
		}
	}

	return NULL;

 found:
	if (cn->n_cached_pages > 0)
		delete_all_pages (ca, cn);

	unlink_node (&cn->node);

	cn->ref_count = 0;

	cn->zombie = FALSE;

	vbi_network_destroy (&cn->network);

	cn->confirm_cni_vps = 0;
	cn->confirm_cni_8301 = 0;
	cn->confirm_cni_8302 = 0;

#if 0
	vbi_program_info_destroy (&cn->program_info);
	vbi_aspect_ratio_destroy (&cn->aspect_ratio);

	{
		vbi_pid_channel ch;

		for (ch = 0; ch < N_ELEMENTS (cn->program_id); ++ch)
			vbi_program_id_destroy (&cn->program_id[ch]);
	}
#endif

	cn->n_cached_pages = 0;
	cn->max_cached_pages = 0;

	cn->n_referenced_pages = 0;

//	cache_network_destroy_caption (cn);
	cache_network_destroy_teletext (cn);

	return cn;
}

static cache_network *
add_network			(vbi_cache *		ca,
				 const vbi_network *	nk,
				 vbi_videostd_set	videostd_set)
{
	cache_network *cn;

	videostd_set = videostd_set; /* unused, no warning please */

	if (nk && (cn = network_by_id (ca, nk))) {
		/* Note does not merge nk. */
		return cn;
	}

	if (ca->n_cached_networks < ca->n_networks_limit
	    || NULL == (cn = recycle_network (ca))) {
		if (!(cn = vbi_cache_malloc (sizeof (*cn)))) {
			errno = ENOMEM;
			return NULL;
		}

		CLEAR (*cn);

		++ca->n_cached_networks;
	}

	add_head (&ca->networks, &cn->node);

	cn->cache = ca;

	if (nk)
		vbi_network_copy (&cn->network, nk);

#if 0
	{
		unsigned int ch;

		vbi_program_info_init (&cn->program_info);
		vbi_aspect_ratio_init (&cn->aspect_ratio, videostd_set);

		for (ch = 0; ch < N_ELEMENTS (cn->program_id); ++ch)
			vbi_program_id_init (&cn->program_id[ch], ch);
	}
#endif /* !ZAPPING8 */

//	cache_network_init_caption (cn);
	cache_network_init_teletext (cn);

	return cn;
}

/**
 * @internal
 * @param cn cache_network obtained with _vbi_cache_add_network()
 *   or _vbi_cache_get_network(), can be @c NULL.
 *
 * Releases a network reference.
 */
void
cache_network_unref		(cache_network *	cn)
{
	vbi_cache *ca;

	if (NULL == cn)
		return;

	assert (NULL != cn->cache);

	ca = cn->cache;

	if (CACHE_CONSISTENCY)
		assert (is_member (&ca->networks, &cn->node));

	if (0 == cn->ref_count) {
#if 0
		warning (&ca->log,
			 "Network %p already unreferenced.",
			 (void *) cn);
#endif
	} else if (1 == cn->ref_count) {
		cn->ref_count = 0;

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
cache_network_ref		(cache_network *	cn)
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
 * _vbi_cache_network_unref() when this reference is no longer
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
			++ca->n_cached_networks;
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
 * _vbi_cache_network_unref() when this reference is no
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

#if 0

/** internal */
void
cache_page_dump			(const cache_page *	cp,
				 FILE *			fp)
{
	const cache_network *cn;

	fprintf (fp, "page %x.%x ", cp->pgno, cp->subno);

	if ((cn = cp->network)) {
		const struct ttx_page_stat *ps;

		ps = cache_network_const_page_stat (cn, cp->pgno);

		fprintf (fp, "%s/L%u/S%04x subp=%u/%u (%u-%u) ",
			 vbi_page_type_name (ps->page_type),
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

#endif

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

_vbi_inline unsigned int
hash				(vbi_pgno		pgno)
{
	return pgno % HASH_SIZE;
}

static vbi_bool
page_in_cache			(const vbi_cache *	ca,
				 const cache_page *	cp)
{
	const struct node *hash_list;
	const struct node *pri_list;

	if (CACHE_PRI_ZOMBIE == cp->priority) {
		/* Note cp->ref_count may be zero if the page is
		   about to be deleted. */
		return is_member (&ca->referenced, &cp->pri_node);
	}

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
		assert (page_in_cache (ca, cp));
	}

	if (cp->ref_count > 0) {
		if (CACHE_PRI_ZOMBIE != cp->priority) {
			/* Remove from cache, mark for deletion.
			   cp->pri_node remains on ca->referenced. */

			unlink_node (&cp->hash_node);

			cp->priority = CACHE_PRI_ZOMBIE;
		}

		return;
	}

	if (CACHE_DEBUG) {
		fputs ("Delete ", stderr);
		cache_page_dump (cp, stderr);
		fputc (' ', stderr);
		cache_network_dump (cp->network, stderr);
		fputc ('\n', stderr);
	}

	if (CACHE_PRI_ZOMBIE != cp->priority) {
		/* Referenced and zombie pages don't count. */ 
		ca->memory_used -= cache_page_size (cp);

		unlink_node (&cp->hash_node);
	}

	unlink_node (&cp->pri_node);

	cache_network_remove_page (cp->network, cp);

	vbi_cache_free (cp);

	--ca->n_cached_pages;
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

#if 3 == VBI_VERSION_MINOR

/**
 * @param ca Cache allocated with vbi_cache_new().
 * @param limit Amount of memory in bytes.
 *
 * Limits the amount of memory used by the Teletext page cache. Reasonable
 * values range from 16 KB to 1 GB, default is 1 GB as in libzvbi 0.2.
 * The number of pages transmitted by networks varies. Expect on the order
 * of one or two megabytes for a complete set.
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
vbi_cache_set_memory_limit	(vbi_cache *		ca,
				 unsigned long		limit)
{
	assert (NULL != ca);

#if 2 == VBI_VERSION_MINOR
	/* We must never delete pages because reference counting
	   doesn't work right and we can't fix that without
	   breaking compatibility. (E.g. DRCS data referenced in
	   a vbi_page isn't reference counted. Also some apps may
	   not call vbi_unref_page() because that did not cause
	   problems in earlier versions.) */
	limit = 1 << 30;
#endif

	ca->memory_limit = SATURATE (limit, 1 << 10, 1 << 30);

	delete_surplus_pages (ca);
}

#endif /* 3 == VBI_VERSION_MINOR */

static cache_page *
page_by_pgno			(vbi_cache *		ca,
				 const cache_network *	cn,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 vbi_subno		subno_mask)
{
	struct node *hash_list;
	cache_page *cp, *cp1;

	if (CACHE_CONSISTENCY) {
		assert (ca == cn->cache);
		assert (is_member (&ca->networks, &cn->node));
	}

	subno &= subno_mask;

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

/**
 * @internal
 * @param cp
 *
 * Unreferences a page returned by vbi_cache_put_cache_page(),
 * vbi_cache_get_cache_page(), or vbi_page_new_cache_page_ref().
 * @a cp can be @c NULL.
 */
void
cache_page_unref		(cache_page *		cp)
{
	vbi_cache *ca;

	if (NULL == cp)
		return;

	assert (NULL != cp->network);
	assert (NULL != cp->network->cache);

	ca = cp->network->cache;

	if (CACHE_CONSISTENCY)
		assert (page_in_cache (ca, cp));

	if (0 == cp->ref_count) {
#if 0
		warning (&ca->log,
			 "Page %p already unreferenced.",
			 (void *) cp);
#endif
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
cache_page_ref			(cache_page *		cp)
{
	assert (NULL != cp);

	if (CACHE_DEBUG) {
		fputs ("Ref ", stderr);
		cache_page_dump (cp, stderr);
	}

	if (0 == cp->ref_count) {
		vbi_cache *ca;
		cache_network *cn;

		cn = cp->network;
		ca = cn->cache;

		if (CACHE_DEBUG) {
			fputc (' ', stderr);
			cache_network_dump (cn, stderr);
		}

		if (cn->zombie) {
			++ca->n_cached_networks;
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

#if 2 == VBI_VERSION_MINOR

/**
 * @param pg Previously fetched vbi_page.
 *
 * A vbi_page fetched from cache with vbi_fetch_vt_page() or
 * vbi_fetch_cc_page() may reference other resource in cache which
 * are locked after fetching. When done processing the page, you
 * must call this function to unlock all the resources associated
 * with this vbi_page.
 */
void
vbi_unref_page			(vbi_page *		pg)
{
	/* Reference counting never really worked and we can't
	   easily fix that in libzvbi 0.2 without breaking binary
	   compatibility. For example DRCS data referenced in a
	   vbi_page isn't reference counted. Also some apps may
	   not call vbi_unref_page() as there are no consequences. */

	/* Nothing to do. */

	pg = pg;
}

#endif

/**
 * @internal
 *
 * Gets a page from the cache. When @a subno is @c VBI_ANY_SUBNO, the most
 * recently received subpage of that page is returned.
 * 
 * The reference counter of the page is incremented, you must call
 * cache_page_unref() to unreference the page.
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

	if (CACHE_CONSISTENCY)
		assert (is_member (&ca->networks, &cn->node));

	if (pgno < 0x100 || pgno > 0x8FF || 0xFF == (pgno & 0xFF)) {
#if 0
		warning (&ca->log,
			 "Invalid pgno 0x%x.", pgno);
#endif
		return NULL;
	}

	if (VBI_ANY_SUBNO == subno)
		subno_mask = 0;

	if (CACHE_DEBUG) {
		fprintf (stderr, "Get %x.%x/%x ", pgno, subno, subno_mask);
		_vbi_cache_dump (ca, stderr);
		fputc (' ', stderr);
		cache_network_dump (cn, stderr);
		fputc ('\n', stderr);
	}

	cp = page_by_pgno (ca, cn, pgno, subno, subno_mask);
	if (NULL == cp) {
		if (CACHE_DEBUG)
			fputs ("Page not cached\n", stderr);
		return NULL;
	} else {
		if (CACHE_DEBUG) {
			fputs ("Found ", stderr);
			cache_page_dump (cp, stderr);
			fputc ('\n', stderr);
		}
	}

	return cache_page_ref (cp);
}

/**
 * @internal
 * For vbi_search.
 */
int
_vbi_cache_foreach_page		(vbi_cache *		ca,
				 cache_network *	cn,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 int			dir,
				 _vbi_cache_foreach_cb *callback,
				 void *			user_data)
{
	cache_page *cp;
	struct ttx_page_stat *ps;
	vbi_bool wrapped;

	assert (NULL != ca);
	assert (NULL != cn);
	assert (NULL != callback);

	if (0 == cn->n_cached_pages)
		return 0;

	if ((cp = _vbi_cache_get_page (ca, cn, pgno, subno, -1))) {
		subno = cp->subno;
	} else if (VBI_ANY_SUBNO == subno) {
		cp = NULL;
		subno = 0;
	}

	ps = cache_network_page_stat (cn, pgno);

	wrapped = FALSE;

	for (;;) {
		if (cp) {
			int r;

			r = callback (cp, wrapped, user_data);

			cache_page_unref (cp);
			cp = NULL;

			if (0 != r)
				return r;
		}

		subno += dir;

		while (0 == ps->n_subpages
		       || subno < ps->subno_min
		       || subno > ps->subno_max) {
			if (dir < 0) {
				--pgno;
				--ps;

				if (pgno < 0x100) {
					pgno = 0x8FF;
					ps = cache_network_page_stat(cn, pgno);
					wrapped = TRUE;
				}

				subno = ps->subno_max;
			} else {
				++pgno;
				++ps;

				if (pgno > 0x8FF) {
					pgno = 0x100;
					ps = cache_network_page_stat(cn, pgno);
					wrapped = TRUE;
				}

				subno = ps->subno_min;
			}
		}

		cp = _vbi_cache_get_page (ca, cn, pgno, subno, -1);
	}
}

/**
 * @internal
 * @param ca Cache.
 * @param cn Network this page belongs to.
 * @param cp Teletext page to store in the cache.
 *
 * Puts a copy of @a cp in the cache.
 * 
 * @returns
 * cache_page pointer (in the cache, not @a cp), @c NULL on failure
 * (out of memory). You must unref the returned page if no longer needed.
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
	vbi_subno subno;
	vbi_subno subno_mask;

	assert (NULL != ca);
	assert (NULL != cn);
	assert (NULL != cp);

	assert (ca == cn->cache);

	memory_needed = cache_page_size (cp);
	memory_available = ca->memory_limit - ca->memory_used;

	death_count = 0;

	if (CACHE_CONSISTENCY)
		assert (is_member (&ca->networks, &cn->node));

	if (CACHE_DEBUG) {
		fprintf (stderr, "Put %x.%x ", cp->pgno, cp->subno);
		_vbi_cache_dump (ca, stderr);
		fputc (' ', stderr);
		cache_network_dump (cn, stderr);
		fputc (' ', stderr);
	}

	/* EN 300 706 Section A.1, E.2. */

	if (0xFF == (cp->pgno & 0xFF)) {
#if 0
		warning (&ca->log,
			 "Invalid pgno 0x%x.", cp->pgno);
#endif
		return NULL;
	}

	subno = cp->subno;
	subno_mask = 0;

	if (likely (vbi_is_bcd (cp->pgno))) {
		if (likely (0 == subno)) {
			/* The page has no subpages or is a clock page
			   at 00:00. We store only one version. */
		} else {
			const struct ttx_page_stat *ps;
			vbi_ttx_page_type page_type;

			ps = cache_network_const_page_stat (cn, cp->pgno);
			page_type = ps->page_type;

			if (VBI_CLOCK_PAGE == page_type
			    || subno >= 0x0100) {
				/* A clock page or a rolling page without
				   subpages (Section A.1 Note 1).
				   One version. */
				if (vbi_bcd_digits_greater (subno, 0x2959)
				    || subno > 0x2300)
					subno = 0; /* invalid */
			} else if (vbi_bcd_digits_greater (subno, 0x79)) {
				/* A rolling page without subpages.
				   One version. */
				subno = 0; /* invalid */
			} else {
				/* A page with subpages or an unmarked
				   clock page between 00:00 and 00:59.
				   We store all versions. */
				subno_mask = 0xFF;
			}
		}
	} else {
		/* S1 element is the subpage number. */
		subno_mask = 0x000F;
	}

	old_cp = page_by_pgno (ca, cn,
			       cp->pgno,
			       subno & subno_mask,
			       subno_mask);
	if (NULL != old_cp) {
		if (CACHE_DEBUG) {
			fputs ("is cached ", stderr);
			cache_page_dump (old_cp, stderr);
			fputc (' ', stderr);
		}

		if (old_cp->ref_count > 0) {
			/* This page is still in use. We remove it from
			   the cache and mark it for deletion when unref'd.
			   old_cp->pri_node remains on ca->referenced. */
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

	/* Find more pages to replace until we have enough memory. */

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
		fprintf (stderr, "need %lu bytes but only %lu available ",
			 memory_needed, memory_available);
	}

	goto failure;

 replace:
	if (likely (memory_available == memory_needed
		    && 1 == death_count)) {
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

		if (!(new_cp = vbi_cache_malloc ((size_t) memory_needed))) {
			errno = ENOMEM;
			goto failure;
		}

		for (i = 0; i < death_count; ++i)
			delete_page (ca, death_row[i]);

		++ca->n_cached_pages;
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
	else if (vbi_is_bcd (cp->pgno)
		 && subno > 0x00
		 && subno <= 0x79)
		new_cp->priority = CACHE_PRI_SPECIAL;
	else
		new_cp->priority = CACHE_PRI_NORMAL;

	new_cp->function		= cp->function;

	new_cp->pgno			= cp->pgno;
	new_cp->subno			= subno;

	new_cp->national		= cp->national;

	new_cp->flags			= cp->flags;

	new_cp->lop_packets		= cp->lop_packets;
	new_cp->x26_designations	= cp->x26_designations;
	new_cp->x27_designations	= cp->x27_designations;
	new_cp->x28_designations	= cp->x28_designations;

	memcpy (&new_cp->data, &cp->data,
		memory_needed - (sizeof (*new_cp) - sizeof (new_cp->data)));

	new_cp->ref_count = 1;
	ca->memory_used += 0; /* see _vbi_cache_get_page() */

	++cn->n_referenced_pages;

	add_tail (&ca->referenced, &new_cp->pri_node);

	cache_network_add_page (cn, new_cp);

	if (CACHE_DEBUG) {
		fputc ('\n', stderr);
	}

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
	if (CACHE_DEBUG) {
		fputc ('\n', stderr);
	}

	return NULL;
}

/** @internal */
void
_vbi_cache_dump			(const vbi_cache *	ca,
				 FILE *			fp)
{
	fprintf (fp, "cache ref=%u pages=%u mem=%lu/%lu KiB networks=%u/%u",
		 ca->ref_count,
		 ca->n_cached_pages,
		 (ca->memory_used + 1023) >> 10,
		 (ca->memory_limit + 1023) >> 10,
		 ca->n_cached_networks,
		 ca->n_networks_limit);
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

	if (!is_empty (&ca->referenced)) {
#if 0
		warning (&ca->log,
			 "Some cached pages still referenced, memory leaks.");
#endif
	}

	if (!is_empty (&ca->networks)) {
#if 0
		warning (&ca->log,
			 "Some cached networks still referenced, "
			 "memory leaks.");
#endif
	}

#if 0
	_vbi_event_handler_list_destroy (&ca->handlers);
#endif

	list_destroy (&ca->networks);
	list_destroy (&ca->priority);
	list_destroy (&ca->referenced);

	for (i = 0; i < N_ELEMENTS (ca->hash); ++i)
		list_destroy (ca->hash + i);

	CLEAR (*ca);

	vbi_free (ca);
}

/**
 * @param ca Cache allocated with vbi_cache_new(), can be @c NULL.
 *
 * Releases a cache reference. When this is the last reference
 * the function calls vbi_cache_delete().
 */
void
vbi_cache_unref			(vbi_cache *		ca)
{
	if (NULL == ca)
		return;

	if (1 == ca->ref_count) {
		vbi_cache_delete (ca);
	} else {
		--ca->ref_count;
	}
}

/**
 * @param ca Cache allocated with vbi_cache_new().
 *
 * Creates a new reference to the cache.
 *
 * @returns
 * @a ca. You must call vbi_cache_unref() when the reference is
 * no longer needed.
 */
vbi_cache *
vbi_cache_ref			(vbi_cache *		ca)
{
	assert (NULL != ca);

	++ca->ref_count;

	return ca;
}

/**
 * Allocates a new cache for VBI decoders.
 *
 * A cache is a shared object with a reference counter. To create
 * a new reference call vbi_cache_ref().
 *
 * @returns
 * Pointer to newly allocated cache which must be freed with
 * vbi_cache_unref() or vbi_cache_delete() when done. @c NULL on
 * failure (out of memory).
 */
vbi_cache *
vbi_cache_new			(void)
{
	vbi_cache *ca;
	unsigned int i;

	ca = vbi_malloc (sizeof (*ca));
	if (NULL == ca) {
		return NULL;
	}

	CLEAR (*ca);

#if 0
	if (CACHE_DEBUG) {
		ca->log.fn = vbi_log_on_stderr;
		ca->log.mask = -1; /* all */
	}
#endif

	for (i = 0; i < N_ELEMENTS (ca->hash); ++i) {
		list_init (ca->hash + i);
	}

	list_init (&ca->referenced);
	list_init (&ca->priority);
	list_init (&ca->networks);

	ca->memory_limit = 1 << 30;
	ca->n_networks_limit = 1;

	ca->ref_count = 1;

#if 0
	if (!_vbi_event_handler_list_init (&ca->handlers)) {
		vbi_cache_delete (ca);
		ca = NULL;
	}
#endif

	return ca;
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/

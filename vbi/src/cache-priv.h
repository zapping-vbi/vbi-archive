/*
 *  libzvbi - Teletext page cache
 *
 *  Copyright (C) 2001, 2002, 2003, 2004 Michael H. Schimek
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

/* $Id: cache-priv.h,v 1.1.2.8 2008-02-25 20:58:43 mschimek Exp $ */

#ifndef CACHE_PRIV_H
#define CACHE_PRIV_H

#include "cache.h"
#include "dlist.h"		/* list, node & funcs */
#ifndef ZAPPING8
#  include "aspect_ratio.h"	/* vbi3_aspect_ratio */
#  include "program_info.h"	/* vbi3_program_info */
#endif
#include "sampling_par.h"	/* vbi3_videostd_set */
#include "vt.h"			/* Teletext definitions */

#define HASH_SIZE 113

/** @internal */
typedef enum {
	/** Pages to be deleted when no longer referenced. */
	CACHE_PRI_ZOMBIE,
	/**
	 * Ordinary pages, oldest at head of list.
	 * These are deleted first when we run out of memory.
	 */
	CACHE_PRI_NORMAL,
	/**
	 * Pages we expect to use frequently, or which take long to reload:
	 * - pgno 0x100 * n and 0x111 * n
	 * - shared pages (objs, drcs, navigation)
	 * - subpages
	 */
	CACHE_PRI_SPECIAL,
} cache_priority;

/**
 * @internal
 * Network related data.
 */
typedef struct {
	/* Cache internal stuff. */

	/** Network chain. */
	struct node		node;

	/** Cache this struct network belongs to. */
	vbi3_cache *		cache;

	unsigned int		ref_count;

	/** Delete this network when no longer referenced. */
	vbi3_bool		zombie;


	/* Decoder stuff. */

	/** Network identification. */
	vbi3_network		network;

	/** Used by vbi3_decoder and vbi3_teletext_decoder, see there. */
	unsigned int		confirm_cni_vps;
	unsigned int		confirm_cni_8301;
	unsigned int		confirm_cni_8302;

#ifndef ZAPPING8
	/** Last received program information. */
	vbi3_program_info	program_info;

	/** Last received aspect ratio information. */
	vbi3_aspect_ratio	aspect_ratio;

	/** Last received program ID, sorted by vbi3_program_id.channel. */
	vbi3_program_id		program_id[6];
#endif

	/* Caption stuff. */

	/** Last XDS_CHANNEL_NAME. */
	uint8_t			channel_name[32];


	/* Teletext stuff. */

	/** Pages cached now and ever, maintained by cache routines. */
	unsigned int		n_cached_pages;
	unsigned int		max_cached_pages;

	/** Number of referenced Teletext pages of this network. */
	unsigned int		n_referenced_pages;

	/** Usually 100. */
        pagenum			initial_page;

	/** BTT links to TOP pages. */
	pagenum			btt_link[2 * 5];
	vbi3_bool		have_top;

	/** Magazine defaults. Use vt_network_magazine() to access. */
	struct magazine		_magazines[8];

	/** Last received packet 8/30 Status Display, with parity bits. */
	uint8_t			status[20];

	/** Page statistics. Use cache_network_page_stat() to access. */
	struct page_stat	_pages[0x800];
} cache_network;

/**
 * @internal
 * @brief Cached preprocessed Teletext page.
 *
 * In the Teletext decoder we check for transmission errors before storing
 * data in the cache, and for efficiency store data in decoded format.
 *
 * Caution! To save memory this structure uses a variable size union.
 */
typedef struct {
	/* Cache internal stuff. */

	/** See struct vbi3_cache. */
	struct node		hash_node;
	struct node		pri_node;

	/** Network sending this page. */
	cache_network *		network;

	unsigned int		ref_count;

	/** Current priority of this page. */
	cache_priority		priority;


	/* Teletext stuff. */

	/**
	 * Defines the page function and which member of the
	 * union applies.
	 */
	page_function		function;

	/** Page and subpage number. */
	vbi3_pgno		pgno;
	vbi3_subno		subno;

	/**
	 * National character set designator 0 ... 7
	 * (3 lsb of a vbi3_ttx_charset_code).
	 */
	int			national;

	/**
	 * Page flags C4 ... C14.
	 * Other bits will be set, just ignore them.
	 */
	unsigned int		flags;

	/**
	 * Sets of packets we received. This may include packets
	 * with hamming errors.
	 *
	 * lop_packets:	1 << packet 0 ... 25
	 * x26_designations: 1 << X/26 designation 0 ... 15
	 */
	unsigned int		lop_packets;
	unsigned int		x26_designations;
	unsigned int		x27_designations;
	unsigned int		x28_designations;

	union {
		/** Raw page, content unknown. */
		struct lop		unknown;

		/** Plain level one page. */
		struct lop		lop;

		/** Level one page with X/26 page enhancements. */
		struct {
			struct lop		lop;
			enhancement		enh;
		}			enh_lop;

		/**
		 * Level one page with X/26 page enhancements
		 * and X/28 extensions for Level 2.5 / 3.5.
		 */
		struct {
			struct lop		lop;
			enhancement		enh;
			struct extension	ext;
		}			ext_lop;

		/** (Global) public object page. */
		struct {
			/**
			 * 12 * 2 triplet pointers from packet 1 ... 4.
			 * Valid range 0 ... 506 (39 packets * 13 triplets),
			 * unused pointers 511 (10.5.1.2), broken -1.
			 */
			uint16_t		pointer[4 * 12 * 2];

			/**
			 * 13 triplets from each of packet 3 ... 25 and
			 * 26/0 ... 26/15.
			 *
			 * Valid range of mode 0x00 ... 0x1F, broken -1.
			 */
		  	struct triplet		triplet[39 * 13 + 1];
		}			gpop, pop;

		/**
		 * (Global) dynamically redefinable characters
		 * download page.
		 */
		struct {
			/** DRCS in raw format for error correction. */
			struct lop		lop;

			/**
			 * Each character consists of 12x10 pixels, stored
			 * left to right and top to bottom. Pixels can assume
			 * up to 16 colors. Every two pixels
			 * are stored in one byte, left pixel in bits 0x0F,
			 * right pixel in bits 0xF0.
			 */
			uint8_t		chars[DRCS_PTUS_PER_PAGE][12 * 10 / 2];

			/** See 9.4.6. */
			uint8_t			mode[DRCS_PTUS_PER_PAGE];

			/**
			 * 1 << (0 ... (DRCS_PTUS_PER_PAGE - 1)).
			 *
			 * Note characters can span multiple successive PTUs,
			 * see get_drcs_data().
			 */
			uint64_t		invalid;
		}			gdrcs, drcs;

		/** TOP AIT page. */
		struct {
			struct ait_title	title[46];

			/** Used to detect changes. */
			unsigned int		checksum;
		}			ait;

	}			data;

	/* Dynamic size, add no fields below unless
	   cache_page is statically allocated. */
} cache_page;

/** @internal */
_vbi3_inline struct magazine *
cache_network_magazine		(cache_network *	cn,
				 vbi3_pgno		pgno)
{
	assert (pgno >= 0x100 && pgno <= 0x8FF);
	return &cn->_magazines[(pgno >> 8) - 1];
}

/** @internal */
_vbi3_inline const struct magazine *
cache_network_const_magazine	(const cache_network *	cn,
				 vbi3_pgno		pgno)
{
	assert (pgno >= 0x100 && pgno <= 0x8FF);
	return &cn->_magazines[(pgno >> 8) - 1];
}

/** @internal */
_vbi3_inline struct page_stat *
cache_network_page_stat		(cache_network *	cn,
				 vbi3_pgno		pgno)
{
	assert (pgno >= 0x100 && pgno <= 0x8FF);
	return &cn->_pages[pgno - 0x100];
}

/** @internal */
_vbi3_inline const struct page_stat *
cache_network_const_page_stat	(const cache_network *	cn,
				 vbi3_pgno		pgno)
{
	assert (pgno >= 0x100 && pgno <= 0x8FF);
	return &cn->_pages[pgno - 0x100];
}

/* in top.c */
extern const struct ait_title *
cache_network_get_ait_title	(cache_network *	cn,
				 cache_page **		ait_cp,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno);
extern vbi3_bool
cache_network_get_top_title	(cache_network *	cn,
				 vbi3_top_title *	tt,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno);
extern vbi3_top_title *
cache_network_get_top_titles	(cache_network *	cn,
				 unsigned int *		n_elements);
/* in cache.c */
extern void
cache_network_get_ttx_page_stat	(const cache_network *	cn,
				 vbi3_ttx_page_stat *	ps,
				 vbi3_pgno		pgno);
extern void
cache_network_unref		(cache_network *	cn);
extern cache_network *
cache_network_ref		(cache_network *	cn);
extern cache_network *
_vbi3_cache_get_network		(vbi3_cache *		ca,
				 const vbi3_network *	nk);
extern cache_network *
_vbi3_cache_add_network		(vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set);
/* in caption.c */
extern void
cache_network_destroy_caption	(cache_network *	cn);
extern void
cache_network_init_caption	(cache_network *	cn);

/* in packet.c */
extern void
cache_network_dump_teletext	(const cache_network *	cn,
				 FILE *			fp);
extern void
cache_network_destroy_teletext	(cache_network *	cn);
extern void
cache_network_init_teletext	(cache_network *	cn);

/* in cache.c */
extern void
cache_page_dump			(const cache_page *	cp,
				 FILE *			fp);
extern unsigned int
cache_page_size			(const cache_page *	cp);
extern vbi3_bool
cache_page_copy			(cache_page *		dst,
				 const cache_page *	src);
extern void
cache_page_unref		(cache_page *		cp);
extern cache_page *
cache_page_ref			(cache_page *		cp);
extern cache_page *
_vbi3_cache_get_page		(vbi3_cache *		ca,
				 cache_network *	cn,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 vbi3_subno		subno_mask);
extern cache_page *
_vbi3_cache_put_page		(vbi3_cache *		ca,
				 cache_network *	cn,
				 const cache_page *	cp);
extern void
_vbi3_cache_dump		(const vbi3_cache *	ca,
				 FILE *			fp);

/* Other stuff. */

typedef int
_vbi3_cache_foreach_cb		(cache_page *		cp,
				 vbi3_bool		wrapped,
				 void *			user_data);

/* in cache.c */
extern int
_vbi3_cache_foreach_page	(vbi3_cache *		ca,
				 cache_network *	cn,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 int			dir,
				 _vbi3_cache_foreach_cb *callback,
				 void *			user_data);

/* in teletext.c */
extern void
_vbi3_ttx_charset_init		(const vbi3_ttx_charset *charset[2],
				 vbi3_ttx_charset_code	default_code_0,
				 vbi3_ttx_charset_code	default_code_1,
				 const struct extension *ext,
				 const cache_page *	cp);

#endif /* CACHE_PRIV_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/

/*
 *  Copyright (C) 2001-2004 Michael H. Schimek
 *  Copyright (C) 2000-2003 Iñaki García Etxebarria
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

/* $Id: callback.h,v 1.2 2005-01-31 07:13:17 mschimek Exp $ */

#ifndef __ZTV_CALLBACK_H__
#define __ZTV_CALLBACK_H__

#include "macros.h"

TV_BEGIN_DECLS

typedef struct _tv_callback tv_callback;

typedef void
tv_callback_fn			(void *			object,
				 void *			user_data);

extern void
tv_nullify_pointer		(void *			object,
				 void *			user_data)
  __attribute__ ((_tv_nonnull (2)));

extern tv_callback *
tv_callback_add			(tv_callback **		list,
				 tv_callback_fn *	notify,
				 tv_callback_fn *	destroy,
				 void *			user_data)
  __attribute__ ((_tv_nonnull (1)));
extern void
tv_callback_remove		(tv_callback *		cb);
extern void
tv_callback_delete		(tv_callback *		cb,
				 void *			object)
  __attribute__ ((_tv_nonnull (2)));
extern void
tv_callback_remove_all		(tv_callback *		list,
				 tv_callback_fn *	notify,
				 tv_callback_fn *	destroy,
				 void *			user_data);
extern void
tv_callback_delete_all		(tv_callback *		list,
				 tv_callback_fn *	notify,
				 tv_callback_fn *	destroy,
				 void *			user_data,
				 void *			object);
extern void
tv_callback_block		(tv_callback *		cb)
  __attribute__ ((_tv_nonnull (1)));
extern void
tv_callback_block_all		(tv_callback *		list,
				 tv_callback_fn *	notify,
				 tv_callback_fn *	destroy,
				 void *			user_data)
  __attribute__ ((_tv_nonnull (1)));
extern void
tv_callback_unblock		(tv_callback *		cb)
  __attribute__ ((_tv_nonnull (1)));
extern void
tv_callback_unblock_all	       	(tv_callback *		list,
				 tv_callback_fn *	notify,
				 tv_callback_fn *	destroy,
				 void *			user_data)
  __attribute__ ((_tv_nonnull (1)));

#define TV_CALLBACK_BLOCK(cb, statement)				\
do {									\
	tv_callback_block (cb);						\
	statement;							\
	tv_callback_unblock (cb);					\
} while (0)

TV_END_DECLS

#endif /* __ZTV_CALLBACK_H__ */

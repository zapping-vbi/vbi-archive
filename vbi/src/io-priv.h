/*
 *  libzvbi - Device interfaces
 *
 *  Copyright (C) 2002 Michael H. Schimek
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

/* $Id: io-priv.h,v 1.1.2.1 2004-04-09 05:17:20 mschimek Exp $ */

#ifndef IO_PRIV_H
#define IO_PRIV_H

#include <stdarg.h>
#include <stddef.h>

#include "io.h"
#include "misc.h"

/**
 * @ingroup Devmod
 */
struct vbi_capture {
	vbi_bool		(* read)(vbi_capture *, vbi_capture_buffer **,
					 vbi_capture_buffer **, struct timeval *);
	vbi_raw_decoder *	(* parameters)(vbi_capture *);
	int			(* get_fd)(vbi_capture *);
	void			(* _delete)(vbi_capture *);
};

#endif /* IO_PRIV_H */

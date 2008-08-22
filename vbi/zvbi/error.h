/*
 *  libzvbi -- Error codes
 *
 *  Copyright (C) 2008 Michael H. Schimek
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

#ifndef __ZVBI_ERROR_H__
#define __ZVBI_ERROR_H__

#include <errno.h>
#include "zvbi/macros.h"

VBI_BEGIN_DECLS

typedef enum _vbi_error {
	/* FIXME group these error codes. */
	VBI_ERR_NULL_ARG		= 0x3E1234,
	VBI_ERR_NULL_CALLBACK,
	VBI_ERR_INVALID_CNI_TYPE,
	VBI_ERR_UNKNOWN_CNI,
	VBI_ERR_CNI_MISMATCH,
	VBI_ERR_INVALID_LINK_TYPE,
	VBI_ERR_RAW_BUFFER_OVERFLOW,
	VBI_ERR_SLICED_BUFFER_OVERFLOW,
	VBI_ERR_FAULTY_DATA,
	VBI_ERR_SYNC_LOST,
	VBI_ERR_SCRAMBLED,
	VBI_ERR_DU_OVERFLOW,
	VBI_ERR_DU_LENGTH,
	VBI_ERR_DU_LINE_NUMBER,
	VBI_ERR_DU_RAW_SEGMENT_POSITION,
	VBI_ERR_DU_RAW_SEGMENT_LOST,
	VBI_ERR_DU_RAW_DATA_INCOMPLETE,
	VBI_ERR_BAD_PAGE,
	VBI_ERR_NO_LINK,
	VBI_ERR_NO_TOP_TITLE,
	VBI_ERR_INVALID_ARG
} vbi_error;

extern const char *
vbi_errstr			(vbi_error		errnum)
  _vbi_const;

VBI_END_DECLS

#endif /* __ZVBI_ERROR_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/

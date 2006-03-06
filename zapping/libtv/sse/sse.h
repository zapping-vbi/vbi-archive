#ifndef SSE_H
#define SSE_H

#include <stdlib.h>
#include "libtv/misc.h"

extern clear_block_fn *		clear_block_mmx_nt [4];
extern copy_plane_fn		copy_plane_SSE;

extern void
memcpy_sse_nt			(void *			dst,
				 const void *		src,
				 size_t			n_bytes);

#endif /* SSE_H */

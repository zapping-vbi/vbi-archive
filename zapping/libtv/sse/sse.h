#ifndef SSE_H
#define SSE_H

#include <stdlib.h>
#include "libtv/misc.h"

extern clear_block_fn *		clear_block_mmx_nt [4];
extern copy_block_fn		copy_block1_sse_nt;

extern void
memcpy_sse_nt			(void *			dst,
				 const void *		src,
				 size_t			n_bytes);

#endif /* SSE_H */

#ifndef MMX_H
#define MMX_H

#include "libtv/misc.h"
#include "libtv/cpu.h"

extern clear_block_fn *		clear_block_mmx [4];
extern copy_block_fn 		copy_block1_mmx;
extern copy_block_fn 		copy_block1_generic;

extern cpu_feature_set
cpu_detection_mmx		(void);

extern void
memcpy_mmx			(void *			dst,
				 const void *		src,
				 size_t			n_bytes);

#endif /* MMX_H */

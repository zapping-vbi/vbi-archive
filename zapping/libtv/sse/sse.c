#include "sse.h"

#define SSE_NON_TEMPORAL 1
#define BPP 1
#include "libtv/mmx/clear_block.h"
#define BPP 3
#include "libtv/mmx/clear_block.h"
#define BPP 4
#include "libtv/mmx/clear_block.h"

clear_block_fn *
clear_block_mmx_nt [4] = {
	clear_block1_mmx_nt,
	NULL,
	clear_block3_mmx_nt,
	clear_block4_mmx_nt,
};

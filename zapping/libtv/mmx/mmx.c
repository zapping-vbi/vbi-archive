#include "mmx.h"

#define SSE_NON_TEMPORAL 0
#define BPP 1
#include "clear_block.h"
#define BPP 3
#include "clear_block.h"
#define BPP 4
#include "clear_block.h"

clear_block_fn *
clear_block_mmx [4] = {
	clear_block1_mmx,
	NULL,
	clear_block3_mmx,
	clear_block4_mmx,
};

#include "avec.h"

#define BPP 1
#include "clear_block.h"
#define BPP 3
#include "clear_block.h"
#define BPP 4
#include "clear_block.h"

clear_block_fn *
clear_block_altivec [4] = {
	clear_block1_altivec,
	NULL,
	clear_block3_altivec,
	clear_block4_altivec,
};

#ifndef AVEC_H
#define AVEC_H

#include "libtv/misc.h"
#include "libtv/cpu.h"

extern clear_block_fn *		clear_block_altivec [4];

extern cpu_feature_set
cpu_detection_altivec		(void);

#endif /* AVEC_H */

/* Generated file, do not edit! */

#ifndef LUT_YUV2RGB_H
#define LUT_YUV2RGB_H

#include <inttypes.h>
#include "simd.h"

#define CY_SH 14
#define GU_BU_SH 13
#define RV_GV_SH 14

#if SIMD
extern const v16		_tv_vsplat16_yuv2rgb_cy;
extern const v16		_tv_vsplat16_yuv2rgb_gu;
extern const v16		_tv_vsplat16_yuv2rgb_bu;
extern const v16		_tv_vsplat16_yuv2rgb_rv;
extern const v16		_tv_vsplat16_yuv2rgb_gv;
#endif

extern const int16_t		_tv_lut_yuv2rgb_gu [256];
extern const int16_t		_tv_lut_yuv2rgb_gv [256];
extern const int16_t		_tv_lut_yuv2rgb_rv [256];
extern const int16_t		_tv_lut_yuv2rgb_bu [256];

extern const uint8_t		_tv_lut_yuv2rgb8 [256 + 512];
extern const uint16_t		_tv_lut_yuv2rgb16 [2][6][256 + 512];

#endif /* LUT_YUV2RGB_H */


/*
 * PCM codecs
 * Copyright (c) 2001 Gerard Lantau.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "avcodec.h"

/* from g711.c by SUN microsystems (unrestricted use) */

#define	SIGN_BIT	(0x80)		/* Sign bit for a A-law byte. */
#define	QUANT_MASK	(0xf)		/* Quantization field mask. */
#define	NSEGS		(8)		/* Number of A-law segments. */
#define	SEG_SHIFT	(4)		/* Left shift for segment number. */
#define	SEG_MASK	(0x70)		/* Segment field mask. */

#define	BIAS		(0x84)		/* Bias for linear code. */

/*
 * alaw2linear() - Convert an A-law value to 16-bit linear PCM
 *
 */
static int alaw2linear(unsigned char	a_val)
{
	int		t;
	int		seg;

	a_val ^= 0x55;

	t = (a_val & QUANT_MASK) << 4;
	seg = ((unsigned)a_val & SEG_MASK) >> SEG_SHIFT;
	switch (seg) {
	case 0:
		t += 8;
		break;
	case 1:
		t += 0x108;
		break;
	default:
		t += 0x108;
		t <<= seg - 1;
	}
	return ((a_val & SIGN_BIT) ? t : -t);
}

static int ulaw2linear(unsigned char	u_val)
{
	int		t;

	/* Complement to obtain normal u-law value. */
	u_val = ~u_val;

	/*
	 * Extract and bias the quantization bits. Then
	 * shift up by the segment number and subtract out the bias.
	 */
	t = ((u_val & QUANT_MASK) << 3) + BIAS;
	t <<= ((unsigned)u_val & SEG_MASK) >> SEG_SHIFT;

	return ((u_val & SIGN_BIT) ? (BIAS - t) : (t - BIAS));
}

/* 16384 entries per table */
static UINT8 *linear_to_alaw = NULL;
static int linear_to_alaw_ref = 0;

static UINT8 *linear_to_ulaw = NULL;
static int linear_to_ulaw_ref = 0;

static void build_xlaw_table(UINT8 *linear_to_xlaw, 
                             int (*xlaw2linear)(unsigned char),
                             int mask) 
{
    int i, j, v, v1, v2;

    j = 0;
    for(i=0;i<128;i++) {
        if (i != 127) {
            v1 = xlaw2linear(i ^ mask);
            v2 = xlaw2linear((i + 1) ^ mask);
            v = (v1 + v2 + 4) >> 3;
        } else {
            v = 8192;
        }
        for(;j<v;j++) {
            linear_to_xlaw[8192 + j] = (i ^ mask);
            if (j > 0)
                linear_to_xlaw[8192 - j] = (i ^ (mask ^ 0x80));
        }
    }
    linear_to_xlaw[0] = linear_to_xlaw[1];
}

static int encode_init(AVCodecContext *avctx)
{
    avctx->frame_size = 1;
    switch(avctx->codec->id) {
    case CODEC_ID_PCM_ALAW:
        if (linear_to_alaw_ref == 0) {
            linear_to_alaw = malloc(16384);
            if (!linear_to_alaw)
                return -1;
            build_xlaw_table(linear_to_alaw, alaw2linear, 0xd5);
        }
        linear_to_alaw_ref++;
        break;
    case CODEC_ID_PCM_MULAW:
        if (linear_to_ulaw_ref == 0) {
            linear_to_ulaw = malloc(16384);
            if (!linear_to_ulaw)
                return -1;
            build_xlaw_table(linear_to_ulaw, ulaw2linear, 0xff);
        }
        linear_to_ulaw_ref++;
        break;
    default:
        break;
    }
    return 0;
}

static int encode_close(AVCodecContext *avctx)
{
    switch(avctx->codec->id) {
    case CODEC_ID_PCM_ALAW:
        if (--linear_to_alaw_ref == 0)
            free(linear_to_alaw);
        break;
    case CODEC_ID_PCM_MULAW:
        if (--linear_to_ulaw_ref == 0)
            free(linear_to_ulaw);
        break;
    default:
        /* nothing to free */
        break;
    }
    return 0;
}

static int encode_frame(AVCodecContext *avctx,
                        unsigned char *frame, int buf_size, void *data)
{
    int n, sample_size, v;
    short *samples;
    unsigned char *dst;

    switch(avctx->codec->id) {
    case CODEC_ID_PCM_S16LE:
    case CODEC_ID_PCM_S16BE:
    case CODEC_ID_PCM_U16LE:
    case CODEC_ID_PCM_U16BE:
        sample_size = 2;
        break;
    default:
        sample_size = 1;
        break;
    }
    n = buf_size / sample_size;
    samples = data;
    dst = frame;

    switch(avctx->codec->id) {
    case CODEC_ID_PCM_S16LE:
        for(;n>0;n--) {
            v = *samples++;
            dst[0] = v & 0xff;
            dst[1] = v >> 8;
            dst += 2;
        }
        break;
    case CODEC_ID_PCM_S16BE:
        for(;n>0;n--) {
            v = *samples++;
            dst[0] = v >> 8;
            dst[1] = v;
            dst += 2;
        }
        break;
    case CODEC_ID_PCM_U16LE:
        for(;n>0;n--) {
            v = *samples++;
            v += 0x8000;
            dst[0] = v & 0xff;
            dst[1] = v >> 8;
            dst += 2;
        }
        break;
    case CODEC_ID_PCM_U16BE:
        for(;n>0;n--) {
            v = *samples++;
            v += 0x8000;
            dst[0] = v >> 8;
            dst[1] = v;
            dst += 2;
        }
        break;
    case CODEC_ID_PCM_S8:
        for(;n>0;n--) {
            v = *samples++;
            dst[0] = (v + 128) >> 8;
            dst++;
        }
        break;
    case CODEC_ID_PCM_U8:
        for(;n>0;n--) {
            v = *samples++;
            dst[0] = ((v + 128) >> 8) + 128;
            dst++;
        }
        break;
    case CODEC_ID_PCM_ALAW:
        for(;n>0;n--) {
            v = *samples++;
            dst[0] = linear_to_alaw[(v + 32768) >> 2];
            dst++;
        }
        break;
    case CODEC_ID_PCM_MULAW:
        for(;n>0;n--) {
            v = *samples++;
            dst[0] = linear_to_ulaw[(v + 32768) >> 2];
            dst++;
        }
        break;
    default:
        return -1;
    }
    avctx->key_frame = 1;
    //avctx->frame_size = (dst - frame) / (sample_size * avctx->channels);

    return dst - frame;
}

typedef struct PCMDecode {
    short table[256];
} PCMDecode;

static int decode_init(AVCodecContext * avctx)
{
    PCMDecode *s = avctx->priv_data;
    int i;

    switch(avctx->codec->id) {
    case CODEC_ID_PCM_ALAW:
        for(i=0;i<256;i++)
            s->table[i] = alaw2linear(i);
        break;
    case CODEC_ID_PCM_MULAW:
        for(i=0;i<256;i++)
            s->table[i] = ulaw2linear(i);
        break;
    default:
        break;
    }
    return 0;
}

static int decode_frame(AVCodecContext *avctx, 
                        void *data, int *data_size,
                        UINT8 *buf, int buf_size)
{
    PCMDecode *s = avctx->priv_data;
    int n;
    short *samples;
    UINT8 *src;

    samples = data;
    src = buf;

    switch(avctx->codec->id) {
    case CODEC_ID_PCM_S16LE:
        n = buf_size >> 1;
        for(;n>0;n--) {
            *samples++ = src[0] | (src[1] << 8);
            src += 2;
        }
        break;
    case CODEC_ID_PCM_S16BE:
        n = buf_size >> 1;
        for(;n>0;n--) {
            *samples++ = (src[0] << 8) | src[1];
            src += 2;
        }
        break;
    case CODEC_ID_PCM_U16LE:
        n = buf_size >> 1;
        for(;n>0;n--) {
            *samples++ = (src[0] | (src[1] << 8)) - 0x8000;
            src += 2;
        }
        break;
    case CODEC_ID_PCM_U16BE:
        n = buf_size >> 1;
        for(;n>0;n--) {
            *samples++ = ((src[0] << 8) | src[1]) - 0x8000;
            src += 2;
        }
        break;
    case CODEC_ID_PCM_S8:
        n = buf_size;
        for(;n>0;n--) {
            *samples++ = src[0] << 8;
            src++;
        }
        break;
    case CODEC_ID_PCM_U8:
        n = buf_size;
        for(;n>0;n--) {
            *samples++ = ((int)src[0] - 128) << 8;
            src++;
        }
        break;
    case CODEC_ID_PCM_ALAW:
    case CODEC_ID_PCM_MULAW:
        n = buf_size;
        for(;n>0;n--) {
            *samples++ = s->table[src[0]];
            src++;
        }
        break;
    default:
        *data_size = 0;
        return -1;
    }
    *data_size = (UINT8 *)samples - (UINT8 *)data;
    return src - buf;
}

#define PCM_CODEC(id, name)                     \
AVCodec name ## _encoder = {                    \
    #name,                                      \
    CODEC_TYPE_AUDIO,                           \
    id,                                         \
    0,                                          \
    encode_init,                                \
    encode_frame,                               \
    encode_close,                               \
    NULL,                                       \
};                                              \
AVCodec name ## _decoder = {                    \
    #name,                                      \
    CODEC_TYPE_AUDIO,                           \
    id,                                         \
    sizeof(PCMDecode),                          \
    decode_init,                                \
    NULL,                                       \
    NULL,                                       \
    decode_frame,                               \
};

PCM_CODEC(CODEC_ID_PCM_S16LE, pcm_s16le);
PCM_CODEC(CODEC_ID_PCM_S16BE, pcm_s16be);
PCM_CODEC(CODEC_ID_PCM_U16LE, pcm_u16le);
PCM_CODEC(CODEC_ID_PCM_U16BE, pcm_u16be);
PCM_CODEC(CODEC_ID_PCM_S8, pcm_s8);
PCM_CODEC(CODEC_ID_PCM_U8, pcm_u8);
PCM_CODEC(CODEC_ID_PCM_ALAW, pcm_alaw);
PCM_CODEC(CODEC_ID_PCM_MULAW, pcm_mulaw);

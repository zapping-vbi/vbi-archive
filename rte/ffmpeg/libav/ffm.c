/*
 * FFM (ffserver live feed) encoder and decoder
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
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "avformat.h"

/* The FFM file is made of blocks of fixed size */
#define FFM_HEADER_SIZE 14
#define PACKET_ID       0x666d

/* each packet contains frames (which can span several packets */
#define FRAME_HEADER_SIZE    5
#define FLAG_KEY_FRAME       0x01

typedef struct FFMStream {
    INT64 pts;
} FFMStream;

enum {
    READ_HEADER,
    READ_DATA,
};

typedef struct FFMContext {
    /* only reading mode */
    offset_t write_index, file_size;
    int read_state; 
    UINT8 header[FRAME_HEADER_SIZE];

    /* read and write */
    int first_packet; /* true if first packet, needed to set the discontinuity tag */
    int packet_size;
    int frame_offset;
    INT64 pts;
    UINT8 *packet_ptr, *packet_end;
    UINT8 packet[1]; /* must be last */
} FFMContext;

static void flush_packet(AVFormatContext *s)
{
    FFMContext *ffm = s->priv_data;
    int fill_size, h;
    ByteIOContext *pb = &s->pb;

    fill_size = ffm->packet_end - ffm->packet_ptr;
    memset(ffm->packet_ptr, 0, fill_size);
    
    /* put header */
    put_be16(pb, PACKET_ID); 
    put_be16(pb, fill_size);
    put_be64(pb, ffm->pts);
    h = ffm->frame_offset;
    if (ffm->first_packet)
        h |= 0x8000;
    put_be16(pb, h);
    put_buffer(pb, ffm->packet, ffm->packet_end - ffm->packet);

    /* prepare next packet */
    ffm->frame_offset = 0; /* no key frame */
    ffm->pts = 0; /* no pts */
    ffm->packet_ptr = ffm->packet;
    ffm->first_packet = 0;
}

/* 'first' is true if first data of a frame */
static void ffm_write_data(AVFormatContext *s, 
                           UINT8 *buf, int size, 
                           INT64 pts, int first)
{
    FFMContext *ffm = s->priv_data;
    int len;

    if (first && ffm->frame_offset == 0)
        ffm->frame_offset = ffm->packet_ptr - ffm->packet + FFM_HEADER_SIZE;
    if (first && ffm->pts == 0)
        ffm->pts = pts;

    /* write as many packets as needed */
    while (size > 0) {
        len = ffm->packet_end - ffm->packet_ptr;
        if (len > size)
            len = size;
        memcpy(ffm->packet_ptr, buf, len);
        
        ffm->packet_ptr += len;
        buf += len;
        size -= len;
        if (ffm->packet_ptr >= ffm->packet_end) {
            /* special case : no pts in packet : we leave the current one */
            if (ffm->pts == 0)
                ffm->pts = pts;

            flush_packet(s);
        }
    }
}

static int ffm_write_header(AVFormatContext *s)
{
    AVStream *st;
    FFMStream *fst;
    FFMContext *ffm;
    ByteIOContext *pb = &s->pb;
    AVCodecContext *codec;
    int bit_rate, i;

    ffm = av_mallocz(sizeof(FFMContext) + FFM_PACKET_SIZE);
    if (!ffm)
        return -1;

    s->priv_data = ffm;
    ffm->packet_size = FFM_PACKET_SIZE;
    
    /* header */
    put_tag(pb, "FFM1");
    put_be32(pb, ffm->packet_size);
    /* XXX: store write position in other file ? */
    put_be64(pb, ffm->packet_size); /* current write position */

    put_be32(pb, s->nb_streams);
    bit_rate = 0;
    for(i=0;i<s->nb_streams;i++) {
        st = s->streams[i];
        bit_rate += st->codec.bit_rate;
    }
    put_be32(pb, bit_rate);

    /* list of streams */
    for(i=0;i<s->nb_streams;i++) {
        st = s->streams[i];
        fst = av_mallocz(sizeof(FFMStream) + ffm->packet_size);
        if (!fst)
            goto fail;
        st->priv_data = fst;

        codec = &st->codec;
        /* generic info */
        put_be32(pb, codec->codec_id);
        put_byte(pb, codec->codec_type);
        put_be32(pb, codec->bit_rate);
        /* specific info */
        switch(codec->codec_type) {
        case CODEC_TYPE_VIDEO:
            put_be32(pb, (codec->frame_rate * 1000) / FRAME_RATE_BASE);
            put_be16(pb, codec->width);
            put_be16(pb, codec->height);
            break;
        case CODEC_TYPE_AUDIO:
            put_be32(pb, codec->sample_rate);
            put_le16(pb, codec->channels);
            break;
        }
        /* hack to have real time */
        fst->pts = gettime();
    }

    /* flush until end of block reached */
    while ((url_ftell(pb) % ffm->packet_size) != 0)
        put_byte(pb, 0);

    put_flush_packet(pb);

    /* init packet mux */
    ffm->packet_ptr = ffm->packet;
    ffm->packet_end = ffm->packet + ffm->packet_size - FFM_HEADER_SIZE;
    ffm->frame_offset = 0;
    ffm->pts = 0;
    ffm->first_packet = 1;

    return 0;
 fail:
    for(i=0;i<s->nb_streams;i++) {
        st = s->streams[i];
        fst = st->priv_data;
        if (fst)
            free(fst);
    }
    free(ffm);
    return -1;
}

static int ffm_write_packet(AVFormatContext *s, int stream_index,
                            UINT8 *buf, int size)
{
    AVStream *st = s->streams[stream_index];
    FFMStream *fst = st->priv_data;
    INT64 pts;
    UINT8 header[FRAME_HEADER_SIZE];

    pts = fst->pts;
    /* packet size & key_frame */
    header[0] = stream_index;
    header[1] = 0;
    if (st->codec.key_frame)
        header[1] |= FLAG_KEY_FRAME;
    header[2] = (size >> 16) & 0xff;
    header[3] = (size >> 8) & 0xff;
    header[4] = size & 0xff;
    ffm_write_data(s, header, FRAME_HEADER_SIZE, pts, 1);
    ffm_write_data(s, buf, size, pts, 0);

    if (st->codec.codec_type == CODEC_TYPE_AUDIO) {
        fst->pts += (INT64)((float)st->codec.frame_size / st->codec.sample_rate * 1000000.0);
    } else {
        fst->pts += (INT64)(1000000.0 * FRAME_RATE_BASE / (float)st->codec.frame_rate);
    }
    return 0;
}

static int ffm_write_trailer(AVFormatContext *s)
{
    ByteIOContext *pb = &s->pb;
    FFMContext *ffm = s->priv_data;
    int i;

    /* flush packets */
    if (ffm->packet_ptr > ffm->packet)
        flush_packet(s);

    put_flush_packet(pb);

    for(i=0;i<s->nb_streams;i++)
        free(s->streams[i]->priv_data);
    free(ffm);
    return 0;
}

/* ffm demux */

static int ffm_is_avail_data(AVFormatContext *s, int size)
{
    FFMContext *ffm = s->priv_data;
    offset_t pos, avail_size;
    int len;

    len = ffm->packet_end - ffm->packet_ptr;
    if (size <= len)
        return 1;
    pos = url_ftell(&s->pb);
    if (pos == ffm->write_index) {
        /* exactly at the end of stream */
        return 0;
    } else if (pos < ffm->write_index) {
        avail_size = ffm->write_index - pos;
    } else {
        avail_size = (ffm->file_size - pos) + (ffm->write_index - FFM_PACKET_SIZE);
    }
    avail_size = (avail_size / ffm->packet_size) * (ffm->packet_size - FFM_HEADER_SIZE) + len;
    if (size <= avail_size)
        return 1;
    else
        return 0;
}

/* first is true if we read the frame header */
static int ffm_read_data(AVFormatContext *s, 
                          UINT8 *buf, int size, int first)
{
    FFMContext *ffm = s->priv_data;
    ByteIOContext *pb = &s->pb;
    int len, fill_size, size1, frame_offset;

    size1 = size;
    while (size > 0) {
    redo:
        len = ffm->packet_end - ffm->packet_ptr;
        if (len > size)
            len = size;
        if (len == 0) {
            if (url_ftell(pb) == ffm->file_size)
                url_fseek(pb, ffm->packet_size, SEEK_SET);
            get_be16(pb); /* PACKET_ID */
            fill_size = get_be16(pb);
            ffm->pts = get_be64(pb);
            frame_offset = get_be16(pb);
            get_buffer(pb, ffm->packet, ffm->packet_size - FFM_HEADER_SIZE);
            ffm->packet_end = ffm->packet + (ffm->packet_size - FFM_HEADER_SIZE - fill_size);
            /* if first packet or resynchronization packet, we must
               handle it specifically */
            if (ffm->first_packet || (frame_offset & 0x8000)) {
                ffm->first_packet = 0;
                ffm->packet_ptr = ffm->packet + (frame_offset & 0x7fff) - FFM_HEADER_SIZE;
                if (!first)
                    break;
            } else {
                ffm->packet_ptr = ffm->packet;
            }
            goto redo;
        }
        memcpy(buf, ffm->packet_ptr, len);
        buf += len;
        ffm->packet_ptr += len;
        size -= len;
        first = 0;
    }
    return size1 - size;
}


static int ffm_read_header(AVFormatContext *s, AVFormatParameters *ap)
{
    AVStream *st;
    FFMStream *fst;
    FFMContext *ffm;
    ByteIOContext *pb = &s->pb;
    AVCodecContext *codec;
    int i;
    UINT32 tag;

    ffm = av_mallocz(sizeof(FFMContext) + FFM_PACKET_SIZE);
    if (!ffm)
        return -1;

    s->priv_data = ffm;

    /* header */
    tag = get_le32(pb);
    if (tag != MKTAG('F', 'F', 'M', '1'))
        goto fail;
    ffm->packet_size = get_be32(pb);
    if (ffm->packet_size != FFM_PACKET_SIZE)
        goto fail;
    ffm->write_index = get_be64(pb);
    /* get also filesize */
    if (!url_is_streamed(pb)) {
        ffm->file_size = url_filesize(url_fileno(pb));
    } else {
        ffm->file_size = (1ULL << 63) - 1;
    }

    s->nb_streams = get_be32(pb);
    get_be32(pb); /* total bitrate */
    /* read each stream */
    for(i=0;i<s->nb_streams;i++) {
        st = av_mallocz(sizeof(AVStream));
        if (!st)
            goto fail;
        s->streams[i] = st;
        fst = av_mallocz(sizeof(FFMStream) + ffm->packet_size);
        if (!fst)
            goto fail;
        st->priv_data = fst;

        codec = &st->codec;
        /* generic info */
        st->codec.codec_id = get_be32(pb);
        st->codec.codec_type = get_byte(pb); /* codec_type */
        codec->bit_rate = get_be32(pb);
        /* specific info */
        switch(codec->codec_type) {
        case CODEC_TYPE_VIDEO:
            codec->frame_rate = ((INT64)get_be32(pb) * FRAME_RATE_BASE) / 1000;
            codec->width = get_be16(pb);
            codec->height = get_be16(pb);
            break;
        case CODEC_TYPE_AUDIO:
            codec->sample_rate = get_be32(pb);
            codec->channels = get_le16(pb);
            break;
        }

    }

    /* get until end of block reached */
    while ((url_ftell(pb) % ffm->packet_size) != 0)
        get_byte(pb);

    /* init packet demux */
    ffm->packet_ptr = ffm->packet;
    ffm->packet_end = ffm->packet;
    ffm->frame_offset = 0;
    ffm->pts = 0;
    ffm->read_state = READ_HEADER;
    ffm->first_packet = 1;
    return 0;
 fail:
    for(i=0;i<s->nb_streams;i++) {
        st = s->streams[i];
        if (st) {
            fst = st->priv_data;
            if (fst)
                free(fst);
            free(st);
        }
    }
    if (ffm)
        free(ffm);
    return -1;
}

/* return < 0 if eof */
static int ffm_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    int size;
    FFMContext *ffm = s->priv_data;

    switch(ffm->read_state) {
    case READ_HEADER:
        if (!ffm_is_avail_data(s, FRAME_HEADER_SIZE))
            return -EAGAIN;
#if 0
        printf("pos=%08Lx spos=%Lx, write_index=%Lx size=%Lx\n", 
               url_ftell(&s->pb), s->pb.pos, ffm->write_index, ffm->file_size);
#endif
        if (ffm_read_data(s, ffm->header, FRAME_HEADER_SIZE, 1) != FRAME_HEADER_SIZE)
            return -EAGAIN;
            
#if 0
        {
            int i;
            for(i=0;i<FRAME_HEADER_SIZE;i++)
                printf("%02x ", ffm->header[i]);
            printf("\n");
        }
#endif
        ffm->read_state = READ_DATA;
        /* fall thru */
    case READ_DATA:
        size = (ffm->header[2] << 16) | (ffm->header[3] << 8) | ffm->header[4];
        if (!ffm_is_avail_data(s, size)) {
            return -EAGAIN;
        }

        av_new_packet(pkt, size);
        pkt->stream_index = ffm->header[0];
        if (ffm->header[1] & FLAG_KEY_FRAME)
            pkt->flags |= PKT_FLAG_KEY;
    
        ffm->read_state = READ_HEADER;
        if (ffm_read_data(s, pkt->data, size, 0) != size) {
            /* bad case: desynchronized packet. we cancel all the packet loading */
            av_free_packet(pkt);
            return -EAGAIN;
        }
        break;
    }
    return 0;
}

//#define DEBUG_SEEK

/* pos is between 0 and file_size - FFM_PACKET_SIZE. It is translated
   by the write position inside this function */
static void ffm_seek1(AVFormatContext *s, offset_t pos1)
{
    FFMContext *ffm = s->priv_data;
    ByteIOContext *pb = &s->pb;
    offset_t pos;

    pos = pos1 + ffm->write_index;
    if (pos >= ffm->file_size)
        pos -= (ffm->file_size - FFM_PACKET_SIZE);
#ifdef DEBUG_SEEK
    printf("seek to %Lx -> %Lx\n", pos1, pos);
#endif
    url_fseek(pb, pos, SEEK_SET);
}

static INT64 get_pts(AVFormatContext *s, offset_t pos)
{
    ByteIOContext *pb = &s->pb;
    INT64 pts;

    ffm_seek1(s, pos); 
    url_fskip(pb, 4);
    pts = get_be64(pb);
#ifdef DEBUG_SEEK
    printf("pts=%0.6f\n", pts / 1000000.0);
#endif
    return pts;
}

/* seek to a given time in the file. The file read pointer is
   positionned at or before pts. XXX: the following code is quite
   approximative */
static int ffm_seek(AVFormatContext *s, INT64 wanted_pts)
{
    FFMContext *ffm = s->priv_data;
    offset_t pos_min, pos_max, pos;
    INT64 pts_min, pts_max, pts;
    double pos1;

#ifdef DEBUG_SEEK
    printf("wanted_pts=%0.6f\n", wanted_pts / 1000000.0);
#endif
    /* find the position using linear interpolation (better than
       dichotomy in typical cases) */
    pos_min = 0;
    pos_max = ffm->file_size - 2 * FFM_PACKET_SIZE;
    while (pos_min <= pos_max) {
        pts_min = get_pts(s, pos_min);
        pts_max = get_pts(s, pos_max);
        /* linear interpolation */
        pos1 = (double)(pos_max - pos_min) * (double)(wanted_pts - pts_min) / 
            (double)(pts_max - pts_min);
        pos = (((INT64)pos1) / FFM_PACKET_SIZE) * FFM_PACKET_SIZE;
        if (pos <= pos_min)
            pos = pos_min;
        else if (pos >= pos_max)
            pos = pos_max;
        pts = get_pts(s, pos);
        /* check if we are lucky */
        if (pts == wanted_pts) {
            goto found;
        } else if (pts > wanted_pts) {
            pos_max = pos - FFM_PACKET_SIZE;
        } else {
            pos_min = pos + FFM_PACKET_SIZE;
        }
    }
    pos = pos_min;
    if (pos > 0)
        pos -= FFM_PACKET_SIZE;
 found:
    ffm_seek1(s, pos);
    return 0;
}

offset_t ffm_read_write_index(int fd)
{
    UINT8 buf[8];
    offset_t pos;
    int i;

    lseek(fd, 8, SEEK_SET);
    read(fd, buf, 8);
    pos = 0;
    for(i=0;i<8;i++) 
        pos |= buf[i] << (56 - i * 8);
    return pos;
}

void ffm_write_write_index(int fd, offset_t pos)
{
    UINT8 buf[8];
    int i;

    for(i=0;i<8;i++) 
        buf[i] = (pos >> (56 - i * 8)) & 0xff;
    lseek(fd, 8, SEEK_SET);
    write(fd, buf, 8);
}

void ffm_set_write_index(AVFormatContext *s, offset_t pos, offset_t file_size)
{
    FFMContext *ffm = s->priv_data;
    ffm->write_index = pos;
    ffm->file_size = file_size;
}

static int ffm_read_close(AVFormatContext *s)
{
    AVStream *st;
    int i;

    for(i=0;i<s->nb_streams;i++) {
        st = s->streams[i];
        free(st->priv_data);
    }
    free(s->priv_data);
    return 0;
}

AVFormat ffm_format = {
    "ffm",
    "ffm format",
    "",
    "ffm",
    /* not really used */
    CODEC_ID_MP2,
    CODEC_ID_MPEG1VIDEO,
    ffm_write_header,
    ffm_write_packet,
    ffm_write_trailer,
    ffm_read_header,
    ffm_read_packet,
    ffm_read_close,
    ffm_seek,
};


#define AVIF_HASINDEX		0x00000010	// Index at end of file?
#define AVIF_MUSTUSEINDEX	0x00000020
#define AVIF_ISINTERLEAVED	0x00000100
#define AVIF_TRUSTCKTYPE	0x00000800	// Use CKType to find key frames?
#define AVIF_WASCAPTUREFILE	0x00010000
#define AVIF_COPYRIGHTED	0x00020000

offset_t start_tag(ByteIOContext *pb, char *tag);
void end_tag(ByteIOContext *pb, offset_t start);

typedef struct CodecTag {
    int id;
    unsigned int tag;
} CodecTag;

void put_bmp_header(ByteIOContext *pb, AVCodecContext *enc, CodecTag *tags);
int put_wav_header(ByteIOContext *pb, AVCodecContext *enc);
int wav_codec_get_id(unsigned int tag, int bps);

extern CodecTag codec_bmp_tags[];
extern CodecTag codec_wav_tags[];

unsigned int codec_get_tag(const CodecTag *tags, int id);
int codec_get_id(const CodecTag *tags, unsigned int tag);

/* avidec.c */
int avi_read_header(AVFormatContext *s, AVFormatParameters *ap);
int avi_read_packet(AVFormatContext *s, AVPacket *pkt);
int avi_read_close(AVFormatContext *s);

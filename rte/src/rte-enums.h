#ifndef __RTE_ENUMS_H__
#define __RTE_ENUMS_H__

/* Stream types */
typedef enum {
  RTE_STREAM_VIDEO = 0,  /* XXX STREAM :-( need a better term */
  RTE_STREAM_AUDIO,	 /* input/output distinction? */
  RTE_STREAM_RAW_VBI,
  RTE_STREAM_SLICED_VBI,
  RTE_STREAM_MAX = 15
} rte_stream_type;

typedef enum {
  RTE_BOOL,
  RTE_INT,
  RTE_REAL,
  RTE_STRING
} rte_basic_type;

/**
 * rte_pixfmt:
 * 
 * <table frame=all><title>Sample formats</title><tgroup cols=5 align=center>
 * <colspec colname=c1><colspec colname=c2><colspec colname=c3><colspec colname=c4>
 * <colspec colname=c5>
 * <spanspec spanname=desc1 namest=c1 nameend=c5 align=left>
 * <spanspec spanname=desc2 namest=c2 nameend=c5 align=left>
 * <thead>
 * <row><entry>Symbol</><entry>Byte&nbsp;0</><entry>Byte&nbsp;1</>
 * <entry>Byte&nbsp;2</><entry>Byte&nbsp;3</></row>
 * </thead><tbody>
 * <row><entry spanname=desc1>Planar YUV 4:2:0 data.</></row>
 * <row><entry>@RTE_PIXFMT_YUV420</><entry spanname=desc2>
 *  <informaltable frame=none><tgroup cols=3><thead>
 *   <row><entry>Y plane</><entry>U plane</><entry>V plane</></row>
 *   </thead><tbody><row>
 *   <entry><informaltable frame=1><tgroup cols=4><tbody>
 *    <row><entry>Y00</><entry>Y01</><entry>Y02</><entry>Y03</></row>
 *    <row><entry>Y10</><entry>Y11</><entry>Y12</><entry>Y13</></row>
 *    <row><entry>Y20</><entry>Y21</><entry>Y22</><entry>Y23</></row>
 *    <row><entry>Y30</><entry>Y31</><entry>Y32</><entry>Y33</></row>
 *   </tbody></tgroup></informaltable></entry>
 *   <entry><informaltable frame=1><tgroup cols=2><tbody>
 *    <row><entry>Cb00</><entry>Cb01</></row>
 *    <row><entry>Cb10</><entry>Cb11</></row>
 *   </tbody></tgroup></informaltable></entry>
 *   <entry><informaltable frame=1><tgroup cols=2><tbody>
 *    <row><entry>Cr00</><entry>Cr01</></row>
 *    <row><entry>Cr10</><entry>Cr11</></row>
 *   </tbody></tgroup></informaltable></entry>
 *  </row></tbody></tgroup></informaltable></entry>
 * </row>
 * <row><entry spanname=desc1>Packed YUV 4:2:2 data.</></row>
 * <row><entry>@RTE_PIXFMT_YUYV</><entry>Y0</><entry>Cb</><entry>Y1</><entry>Cr</></row>
 * <row><entry>@RTE_PIXFMT_YVYU</><entry>Y0</><entry>Cr</><entry>Y1</><entry>Cb</></row>
 * <row><entry>@RTE_PIXFMT_UYVY</><entry>Cb</><entry>Y0</><entry>Cr</><entry>Y1</></row>
 * <row><entry>@RTE_PIXFMT_VYUY</><entry>Cr</><entry>Y0</><entry>Cb</><entry>Y1</></row>
 * <row><entry spanname=desc1>Packed 32 bit RGB data.</></row>
 * <row><entry>@RTE_PIXFMT_RGBA32_LE @RTE_PIXFMT_ARGB32_BE</>
 * <entry>r7&nbsp;...&nbsp;r0</><entry>g7&nbsp;...&nbsp;g0</>
 * <entry>b7&nbsp;...&nbsp;b0</><entry>a7&nbsp;...&nbsp;a0</></row>
 * <row><entry>@RTE_PIXFMT_BGRA32_LE @RTE_PIXFMT_ARGB32_BE</>
 * <entry>b7&nbsp;...&nbsp;b0</><entry>g7&nbsp;...&nbsp;g0</>
 * <entry>r7&nbsp;...&nbsp;r0</><entry>a7&nbsp;...&nbsp;a0</></row>
 * <row><entry>@RTE_PIXFMT_ARGB32_LE @RTE_PIXFMT_BGRA32_BE</>
 * <entry>a7&nbsp;...&nbsp;a0</><entry>r7&nbsp;...&nbsp;r0</>
 * <entry>g7&nbsp;...&nbsp;g0</><entry>b7&nbsp;...&nbsp;b0</></row>
 * <row><entry>@RTE_PIXFMT_ABGR32_LE @RTE_PIXFMT_RGBA32_BE</>
 * <entry>a7&nbsp;...&nbsp;a0</><entry>b7&nbsp;...&nbsp;b0</>
 * <entry>g7&nbsp;...&nbsp;g0</><entry>r7&nbsp;...&nbsp;r0</></row>
 * <row><entry spanname=desc1>Packed 24 bit RGB data.</></row>
 * <row><entry>@RTE_PIXFMT_RGBA24</>
 * <entry>r7&nbsp;...&nbsp;r0</><entry>g7&nbsp;...&nbsp;g0</>
 * <entry>b7&nbsp;...&nbsp;b0</><entry></></row>
 * <row><entry>@RTE_PIXFMT_BGRA24</>
 * <entry>b7&nbsp;...&nbsp;b0</><entry>g7&nbsp;...&nbsp;g0</>
 * <entry>r7&nbsp;...&nbsp;r0</><entry></></row>
 * <row><entry spanname=desc1>Packed 16 bit RGB data.</></row>
 * <row><entry>@RTE_PIXFMT_RGB16_LE</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</>
 * <entry>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g5&nbsp;g4&nbsp;g3</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_BGR16_LE</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</>
 * <entry>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g5&nbsp;g4&nbsp;g3</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_RGB16_BE</>
 * <entry>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g5&nbsp;g4&nbsp;g3</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_BGR16_BE</>
 * <entry>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g5&nbsp;g4&nbsp;g3</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</>
 * <entry></><entry></></row>
 * <row><entry spanname=desc1>Packed 15 bit RGB data.</></row>
 * <row><entry>@RTE_PIXFMT_RGBA15_LE</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</>
 * <entry>a0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_BGRA15_LE</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</>
 * <entry>a0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_ARGB15_LE</>
 * <entry>g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;a0</>
 * <entry>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3&nbsp;g2</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_ABGR15_LE</>
 * <entry>g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;a0</>
 * <entry>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3&nbsp;g2</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_RGBA15_BE</>
 * <entry>a0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_BGRA15_BE</>
 * <entry>a0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_ARGB15_BE</>
 * <entry>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3&nbsp;g2</>
 * <entry>g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;a0</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_ABGR15_BE</>
 * <entry>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3&nbsp;g2</>
 * <entry>g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;a0</>
 * <entry></><entry></></row>
 * </tbody></tgroup></table>
 **/
/* Attn: keep this in sync with zvbi, don't change order */
typedef enum {
  RTE_PIXFMT_YUV420 = 1,
  RTE_PIXFMT_YUYV,
  RTE_PIXFMT_YVYU,
  RTE_PIXFMT_UYVY,
  RTE_PIXFMT_VYUY,
  RTE_PIXFMT_RGBA32_LE = 32,
  RTE_PIXFMT_RGBA32_BE,
  RTE_PIXFMT_BGRA32_LE,
  RTE_PIXFMT_BGRA32_BE,
  RTE_PIXFMT_RGB24,
  RTE_PIXFMT_BGR24,
  RTE_PIXFMT_RGB16_LE,
  RTE_PIXFMT_RGB16_BE,
  RTE_PIXFMT_BGR16_LE,
  RTE_PIXFMT_BGR16_BE,
  RTE_PIXFMT_RGBA15_LE,
  RTE_PIXFMT_RGBA15_BE,
  RTE_PIXFMT_BGRA15_LE,
  RTE_PIXFMT_BGRA15_BE,
  RTE_PIXFMT_ARGB15_LE,
  RTE_PIXFMT_ARGB15_BE,
  RTE_PIXFMT_ABGR15_LE,
  RTE_PIXFMT_ABGR15_BE
} rte_pixfmt;

#define RTE_PIXFMT_ABGR32_BE RTE_PIXFMT_RGBA32_LE
#define RTE_PIXFMT_ARGB32_BE RTE_PIXFMT_BGRA32_LE
#define RTE_PIXFMT_ABGR32_LE RTE_PIXFMT_RGBA32_BE
#define RTE_PIXFMT_ARGB32_LE RTE_PIXFMT_BGRA32_BE

/**
 * rte_sndfmt:
 * @RTE_SNDFMT_S8: Signed 8 bit samples. 
 * @RTE_SNDFMT_U8: Unsigned 8 bit samples.
 * @RTE_SNDFMT_S16_LE: Signed 16 bit samples, little endian.
 * @RTE_SNDFMT_S16_BE: Signed 16 bit samples, big endian.
 * @RTE_SNDFMT_U16_LE: Unsigned 16 bit samples, little endian.
 * @RTE_SNDFMT_U16_BE: Unsigned 16 bit samples, big endian.
 *
 * RTE PCM audio formats.
 **/
typedef enum {
  RTE_SNDFMT_S8 = 1,
  RTE_SNDFMT_U8,
  RTE_SNDFMT_S16_LE,
  RTE_SNDFMT_S16_BE,
  RTE_SNDFMT_U16_LE,
  RTE_SNDFMT_U16_BE
} rte_sndfmt;

/* VBI parameters defined in libzvbi.h */

#endif /* rte-enums.h */

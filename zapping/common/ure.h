/*
 * Copyright 1997, 1998, 1999 Computing Research Labs,
 * New Mexico State University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COMPUTING RESEARCH LAB OR NEW MEXICO STATE UNIVERSITY BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
 * OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/*
 * See the included README for more info on using URE.
 */
#ifndef _h_ure
#define _h_ure

/*
 * $Id: ure.h,v 1.2 2001-01-10 00:11:01 garetxe Exp $
 */

#include <stdio.h>
#include <unicode.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef __
#ifdef __STDC__
#define __(x) x
#else
#define __(x) ()
#endif

/*
 * Set of character class flags.
 */
#define _URE_NONSPACING  0x00000001
#define _URE_COMBINING   0x00000002
#define _URE_NUMDIGIT    0x00000004
#define _URE_NUMOTHER    0x00000008
#define _URE_SPACESEP    0x00000010
#define _URE_LINESEP     0x00000020
#define _URE_PARASEP     0x00000040
#define _URE_CNTRL       0x00000080
#define _URE_PUA         0x00000100

#define _URE_UPPER       0x00000200
#define _URE_LOWER       0x00000400
#define _URE_TITLE       0x00000800
#define _URE_MODIFIER    0x00001000
#define _URE_OTHERLETTER 0x00002000
#define _URE_DASHPUNCT   0x00004000
#define _URE_OPENPUNCT   0x00008000
#define _URE_CLOSEPUNCT  0x00010000
#define _URE_OTHERPUNCT  0x00020000
#define _URE_MATHSYM     0x00040000
#define _URE_CURRENCYSYM 0x00080000
#define _URE_OTHERSYM    0x00100000

#define _URE_LTR         0x00200000
#define _URE_RTL         0x00400000

#define _URE_EURONUM     0x00800000
#define _URE_EURONUMSEP  0x01000000
#define _URE_EURONUMTERM 0x02000000
#define _URE_ARABNUM     0x04000000
#define _URE_COMMONSEP   0x08000000

#define _URE_BLOCKSEP    0x10000000
#define _URE_SEGMENTSEP  0x20000000

#define _URE_WHITESPACE  0x40000000
#define _URE_OTHERNEUT   0x80000000

/*
 * Error codes.
 */
#define _URE_OK               0
#define _URE_UNEXPECTED_EOS   -1
#define _URE_CCLASS_OPEN      -2
#define _URE_UNBALANCED_GROUP -3
#define _URE_INVALID_PROPERTY -4

/*
 * Options that can be combined for searching.
 */
#define URE_IGNORE_NONSPACING      0x01
#define URE_DOT_MATCHES_SEPARATORS 0x02

typedef unsigned long ucs4_t;
typedef unsigned short ucs2_t;

/*
 * Opaque type for memory used when compiling expressions.
 */
typedef struct _ure_buffer_t *ure_buffer_t;

/*
 * Opaque type for the minimal DFA used when matching.
 */
typedef struct _ure_dfa_t *ure_dfa_t;

/*************************************************************************
 *
 * API.
 *
 *************************************************************************/
  
/**
 * Alloc memory for the regex internal buffer, NULL on error.
 * Use ure_buffer_free to free the returned buffer.
 */
extern ure_buffer_t ure_buffer_create __((void));

extern void ure_buffer_free __((ure_buffer_t buf));

/**
 * Compile the given expression into a dfa.
 * @re: Buffer containing the UCS-2 regexp.
 * @relen: Size in characters of the regexp.
 * @casefold: %TRUE for matching disregarding case.
 * @buf: The regexp buffer.
 * Returns: The compiled DFA, %NULL on error.
 */
extern ure_dfa_t ure_compile __((ucs2_t *re, unsigned long relen,
                                 int casefold, ure_buffer_t buf));

extern void ure_dfa_free __((ure_dfa_t dfa));

extern void ure_write_dfa __((ure_dfa_t dfa, FILE *out));

/**
 * Run the compiled regexp search on the given text.
 * @dfa: The compiled expression.
 * @flags: Or'ed
      URE_IGNORED_NONSPACING: Set if nonspacing chars should be ignored.
      URE_DOT_MATCHES_SEPARATORS: Set if dot operator matches
      separator characters too.
 * @text: UCS-2 text to run the compiled regexp against.
 * @textlen: Size in characters of the text.
 * @match_start: Index in text of the first matching char.
 * @match_end: Index in text of the first non-matching char after the
               matching characters.
 * Returns: TRUE if the search suceeded.
 */
extern int ure_exec __((ure_dfa_t dfa, int flags,
                        ucs2_t *text, unsigned long textlen,
                        unsigned long *match_start, unsigned long *match_end));

#undef __

#ifdef __cplusplus
}
#endif

#endif /* _h_ure */

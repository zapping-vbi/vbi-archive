/*
 * Unicode support.
 * (C) Iñaki García Etxebarria 2001
 * Under the GPL v2
 *
 * All of these routines work on native endian UCS-2 strings.
 */
#ifndef __UCS_2__
#define __UCS_2__

#include "ure.h"

/**
 * Length in chars of the given string. The length in bytes will be
 * ucs2_strlen*sizeof(ucs2_t).
 */
size_t ucs2_strlen(const void *string);

/**
 * Convert as much as possible, up to bytes num bytes, of @string in
 * charset @input to charset @output. The converted string is malloc'ed
 * and returned, or NULL on error.
 */
void*
convert (const void *string, int bytes,
	 const char *input, const char *output);

/**
 * Convert from the charset the current locale uses to UCS-2, and
 * back. NULL on error.
 * If the charset the current locale uses cannot be determined,
 * LATIN-1 is used.
 */
void*
local2ucs2 (const void *string);

void*
ucs22local (const void *string);

/*
 * The same as before, but with LATIN-1 (ISO-8859-1)
 */
void*
latin2ucs2 (const void *string);

void*
ucs22latin (const void *string);

/*
 * Startup/shutdown functions. Must be called before using any other
 * routine in this file, or in URE.
 */
/**
 * 1 on success, 0 on error
 */
int
startup_ucs2 (void);

void
shutdown_ucs2 (void);

#endif /* ucs-2 */

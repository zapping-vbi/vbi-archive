/*	mmx.h

	MultiMedia eXtensions GCC interface library for IA32.

	To use this library, simply include this header file
	and compile with GCC.  You MUST have inlining enabled
	in order for mmx_ok() to work; this can be done by
	simply using -O on the GCC command line.

	Compiling with -DMMX_TRACE will cause detailed trace
	output to be sent to stderr for each mmx operation.
	This adds lots of code, and obviously slows execution to
	a crawl, but can be very useful for debugging.

	THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY
	EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
	LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY
	AND FITNESS FOR ANY PARTICULAR PURPOSE.

	1997-99 by H. Dietz and R. Fisher

 Notes:
	It appears that the latest gas has the pand problem fixed, therefore
	  I'll undefine BROKEN_PAND by default.
*/

#include "mmx.h"

/*	Function to test if mmx instructions are supported...
*/
int
mmx_ok(void)
{
  /* Returns 1 if MMX instructions are supported, 0 otherwise */
  return ( mm_support() & 0x1 );
}

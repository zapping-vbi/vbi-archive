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

/*	Function to test if multimedia instructions are supported...
*/
#ifdef USE_MMX
int
mm_support(void)
{
	/* Returns 1 if MMX instructions are supported,
	   3 if Cyrix MMX and Extended MMX instructions are supported
	   5 if AMD MMX and 3DNow! instructions are supported
	   0 if hardware does not support any of these
	*/
  int rval = 0;
  
  __asm__ __volatile__ (
       "pushl %%ebx\n\t"
       "pushl %%ecx\n\t"
       "pushl %%edx\n\t"
       
       /* See if CPUID instruction is supported ... */
       /* ... Get copies of EFLAGS into eax and ecx */
       "pushf\n\t"
       "popl %%eax\n\t"
       "movl %%eax, %%ecx\n\t"
       
       /* ... Toggle the ID bit in one copy and store */
       /*     to the EFLAGS reg */
       "xorl $0x200000, %%eax\n\t"
       "push %%eax\n\t"
       "popf\n\t"
       
       /* ... Get the (hopefully modified) EFLAGS */
       "pushf\n\t"
       "popl %%eax\n\t"
       
       /* ... Compare and test result */
       "xorl %%eax, %%ecx\n\t"
       "testl $0x200000, %%ecx\n\t"
       "jz NotSupported1\n\t"		/* CPUID not supported */
       
       
       /* Get standard CPUID information, and
	  go to a specific vendor section */
       "movl $0, %%eax\n\t"
       "cpuid\n\t"
       
       /* Check for Intel */
       "cmpl $0x756e6547, %%ebx\n\t"
       "jne TryAMD\n\t"
       "cmpl $0x49656e69, %%edx\n\t"
       "jne TryAMD\n\t"
       "cmpl $0x6c65746e, %%ecx\n"
       "jne TryAMD\n\t"
       "jmp Intel\n\t"
       
       /* Check for AMD */
       "\nTryAMD:\n\t"
       "cmpl $0x68747541, %%ebx\n\t"
       "jne TryCyrix\n\t"
       "cmpl $0x69746e65, %%edx\n\t"
       "jne TryCyrix\n\t"
       "cmpl $0x444d4163, %%ecx\n"
       "jne TryCyrix\n\t"
       "jmp AMD\n\t"
       
       /* Check for Cyrix */
       "\nTryCyrix:\n\t"
       "cmpl $0x69727943, %%ebx\n\t"
       "jne NotSupported2\n\t"
       "cmpl $0x736e4978, %%edx\n\t"
       "jne NotSupported3\n\t"
       "cmpl $0x64616574, %%ecx\n\t"
       "jne NotSupported4\n\t"
       /* Drop through to Cyrix... */
       
       
       /* Cyrix Section */
       /* See if extended CPUID level 80000001 is supported */
       /* The value of CPUID/80000001 for the 6x86MX is undefined
	  according to the Cyrix CPU Detection Guide (Preliminary
	  Rev. 1.01 table 1), so we'll check the value of eax for
	  CPUID/0 to see if standard CPUID level 2 is supported.
	  According to the table, the only CPU which supports level
	  2 is also the only one which supports extended CPUID levels.
       */
       "cmpl $0x2, %%eax\n\t"
       "jne MMXtest\n\t"	/* Use standard CPUID instead */
       
       /* Extended CPUID supported (in theory), so get extended
	  features */
       "movl $0x80000001, %%eax\n\t"
       "cpuid\n\t"
       "testl $0x00800000, %%eax\n\t"	/* Test for MMX */
       "jz NotSupported5\n\t"		/* MMX not supported */
       "testl $0x01000000, %%eax\n\t"	/* Test for Ext'd MMX */
       "jnz EMMXSupported\n\t"
       "movl $1, %0\n\n\t"		/* MMX Supported */
       "jmp Return\n\n"
       "EMMXSupported:\n\t"
       "movl $3, %0\n\n\t"		/* EMMX and MMX Supported */
       "jmp Return\n\t"
       

       /* AMD Section */
       "AMD:\n\t"
       
       /* See if extended CPUID is supported */
       "movl $0x80000000, %%eax\n\t"
       "cpuid\n\t"
       "cmpl $0x80000000, %%eax\n\t"
       "jl MMXtest\n\t"	/* Use standard CPUID instead */
       
       /* Extended CPUID supported, so get extended features */
       "movl $0x80000001, %%eax\n\t"
       "cpuid\n\t"
       "testl $0x00800000, %%edx\n\t"	/* Test for MMX */
       "jz NotSupported6\n\t"		/* MMX not supported */
       "testl $0x80000000, %%edx\n\t"	/* Test for 3DNow! */
       "jnz ThreeDNowSupported\n\t"
       "movl $1, %0\n\n\t"		/* MMX Supported */
       "jmp Return\n\n"
       "ThreeDNowSupported:\n\t"
       "movl $5, %0\n\n\t"		/* 3DNow! and MMX Supported */
       "jmp Return\n\t"
       

       /* Intel Section */
       "Intel:\n\t"
       
       /* Check for MMX */
       "MMXtest:\n\t"
       "movl $1, %%eax\n\t"
       "cpuid\n\t"
       "testl $0x00800000, %%edx\n\t"	/* Test for MMX */
       "jz NotSupported7\n\t"		/* MMX Not supported */
       "movl $1, %0\n\n\t"		/* MMX Supported */
       "jmp Return\n\t"

       /* Nothing supported */
       "\nNotSupported1:\n\t"
       "#movl $101, %0:\n\n\t"
       "\nNotSupported2:\n\t"
       "#movl $102, %0:\n\n\t"
       "\nNotSupported3:\n\t"
       "#movl $103, %0:\n\n\t"
       "\nNotSupported4:\n\t"
       "#movl $104, %0:\n\n\t"
       "\nNotSupported5:\n\t"
       "#movl $105, %0:\n\n\t"
       "\nNotSupported6:\n\t"
       "#movl $106, %0:\n\n\t"
       "\nNotSupported7:\n\t"
       "#movl $107, %0:\n\n\t"
       "movl $0, %0\n\n\t"
       
       "Return:\n\t"
       
       "popl %%edx\n\t"
       "popl %%ecx\n\t"
       "popl %%ebx\n\t"
       : "=a" (rval)
       /* no input, no scratch */
       );

  /* Return */
  return(rval);
}
#else /* use_mmx */
int
mm_support()
{
  return 0;
}
#endif /* use_mmx */

/*	Function to test if mmx instructions are supported...
*/
int
mmx_ok(void)
{
	/* Returns 1 if MMX instructions are supported, 0 otherwise */
	return ( mm_support() & 0x1 );
}

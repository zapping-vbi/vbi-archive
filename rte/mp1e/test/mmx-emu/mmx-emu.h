
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <asm/sigcontext.h>
#include <linux/version.h>

#ifdef MMX_DEBUG
#define mmx_printf(args...) fprintf(stderr ,## args)
#else
#define mmx_printf(args...)
#endif

#define ISTF2(x) #x
#define ISTF1(x) ISTF2(x)

#define FAIL(why, args...)						\
do {									\
	fprintf(stderr,	"mmx-emu:" __FILE__ ":" ISTF1(__LINE__) ": "	\
		why "\n" ,## args);					\
	exit(EXIT_FAILURE);						\
} while (0)

/* Linux interface */

#if LINUX_VERSION_CODE < 2 * 65536 + 1 * 256 + 1
typedef struct sigcontext_struct sigcontext_t;
#else
typedef struct sigcontext sigcontext_t;
#endif

/* Address of GP or MMX register */

#define GP_REG(reg) ((&(context->eax)) - (reg))
#define MMX_REG(reg) (&(context->fpstate->_st[reg]))

/* Cyrix implied MMX register: IMPL_REG(MMX_REG(reg)) == MMX_REG(reg ^ 1) */

#define IMPL_REG(regp) MMX_REG((((struct _fpreg *)(regp)) - MMX_REG(0)) ^ 1)

/* CPU classification */

typedef enum {
	CPU_UNKNOWN,		/* no MMX */
	CPU_PENTIUM_MMX,	/* MMX; late P5 core */
	CPU_PENTIUM_II,		/* MMX, CMOV; any P6 core */
	CPU_PENTIUM_III,	/* MMX, CMOV, SSE; any P6 core and Itanium x86 */
	CPU_PENTIUM_4,		/* MMX, CMOV, SSE, SSE2; any P8 core */
	CPU_K6_2,		/* MMX, 3DNOW; K6-2/K6-III */
	CPU_ATHLON,		/* MMX, 3DNOW, AMD 3DNOW ext, CMOV, SSE int; K7 core */
	CPU_CYRIX_MII,		/* MMX, CMOV */
	CPU_CYRIX_III,		/* MMX, Cyrix MMX ext, 3DNOW, CMOV */
	/* AMD Hammer family, presumably Athlon + SSE2; K8 core */
} cpu_class;

/* Math stuff */

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static inline int
saturate(int val, int min, int max)
{
#ifdef __i686__
	if (val < min)
		val = min;
	if (val > max)
		val = max;
#else
	if (val < min)
		val = min;
	else if (val > max)
		val = max;
#endif
	return val;
}

static inline unsigned int
nbabs(register int n)
{
	register int t = n;

        t >>= 31;
	n ^= t;
	n -= t;

	return n;
}

#define SATURATE(TYPE, VALUE_MAX, VALUE_MIN)				\
static inline TYPE saturate_##TYPE (int a) {				\
	return saturate(a, VALUE_MIN, VALUE_MAX);			\
}

SATURATE(int8_t,   0x7F,   0x80)
SATURATE(uint8_t,  0xFF,   0x0)
SATURATE(int16_t,  0x7FFF, 0x8000)
SATURATE(uint16_t, 0xFFFF, 0x0)

/* Global */

typedef void (ifunc)(void *, void *, ...);

extern void mmx_emu_configure(cpu_class cpu);
extern int mmx_emu_decode(sigcontext_t *context);

extern void mmx_emu_ni(void);


#define DD mmx_printf(__FUNCTION__ " called\n")

#define MMX_EMU_STORE_MACRO(NAME, TYPE, LOOP)				\
void NAME (void *src, void *dest) {					\
	TYPE *d = (TYPE *) src; /* memory (r/m) */			\
	TYPE *s = (TYPE *) dest; /* mmx or xmm (reg) */			\
	int i;								\
									\
	for (i = 0; i < LOOP; i++)					\
		d[i] = s[i];						\
	DD;								\
}

#define MMX_EMU_UOP_MACRO(NAME, TYPE, LOOP, OP)				\
void NAME (void *src, void *dest) {					\
	TYPE *d = (TYPE *) dest; /* mmx or xmm (reg) */			\
	TYPE *s = (TYPE *) src;	/* mmx, xmm or memory (r/m) */		\
	int i;								\
									\
	for (i = 0; i < LOOP; i++)					\
		d[i] = OP;						\
	DD;								\
}

#define MMX_EMU_SAT_MACRO(NAME, TYPE, LOOP, SYMB, SATURATION)		\
void NAME (void *src, void *dest) {					\
	TYPE *d = (TYPE *) dest;					\
	TYPE *s = (TYPE *) src;						\
	int i;								\
									\
	for (i = 0; i < LOOP; i++)					\
		d[i] = saturate_##SATURATION(				\
			(int) d[i] SYMB (int) s[i]);			\
	DD;								\
}

#define MMX_EMU_SHIFT_MACRO(NAME, TYPE, LOOP, SYMB, SIGN)		\
void NAME (void *src, void *dest) {					\
        TYPE *d = (TYPE *) dest;					\
        uint32_t s = * (uint32_t *) src; /* XXX */			\
        int i;								\
									\
	if (s >= sizeof(TYPE) * 8)					\
		for (i = 0; i < LOOP; i++)				\
    			d[i] = SIGN;					\
	else								\
		for (i = 0; i < LOOP; i++)				\
			d[i] = d[i] SYMB s;				\
	DD;								\
}

#define MMX_EMU_UNPCKL_MACRO(NAME, TYPE, LOOP)				\
void NAME (void *src, void *dest) {					\
	TYPE *d = (TYPE *) dest;					\
	TYPE *s = (TYPE *) src;						\
	int i;								\
									\
	for (i = LOOP - 1; i >= 0; i--) {				\
		d[2 * i + 1] = s[i];					\
		d[2 * i + 0] = d[i];					\
	}								\
									\
	DD;								\
}

#define MMX_EMU_UNPCKH_MACRO(NAME, TYPE, LOOP)				\
void NAME (void *src, void *dest) {					\
	TYPE *d = (TYPE *) dest;					\
	TYPE *s = (TYPE *) src;						\
	int i;								\
									\
	for (i = 0; i < LOOP; i++) {					\
		d[2 * i + 0] = d[i + LOOP];				\
		d[2 * i + 1] = s[i + LOOP];				\
	}								\
									\
	DD;								\
}

#define MMX_EMU_NOP_MACRO(NAME)						\
void NAME (void *src, void *dest) {					\
	DD;								\
}

#define MMX_EMU_NI_MACRO(NAME)						\
void NAME (void *src, void *dest) {					\
	DD;								\
	mmx_emu_ni();							\
}

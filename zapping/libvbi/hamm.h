#ifndef HAMM_H
#define HAMM_H

int hamm8(u8 *p, int *err);
int hamm16(u8 *p, int *err);
int hamm24(u8 *p, int *err);
int chk_parity(u8 *p, int n);
extern char hamm24par[3][256];

extern int		hamm24a(u8 *p);

extern unsigned char	bit_reverse[256];
extern char		hamm8a[256];

static inline int
parity(int c)
{
	if (hamm24par[0][c] & 32)
		return c & 0x7F;
	else
		return -1;
}

static inline int
hamm16a(unsigned char *p)
{
	return hamm8a[p[0]] | (hamm8a[p[1]] << 4);
}

#endif

#ifndef HAMM_H
#define HAMM_H

int hamm8(u8 *p, int *err);
int hamm16(u8 *p, int *err);
int hamm24(u8 *p, int *err);
int chk_parity(u8 *p, int n);

extern unsigned char	bit_reverse[256];
extern char		hamm8a[256];

#endif

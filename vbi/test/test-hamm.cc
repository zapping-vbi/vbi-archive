/*
 *  libzvbi test
 *  Copyright (C) 2003 Michael H. Schimek
 */

/* $Id: test-hamm.cc,v 1.1.2.2 2003-12-28 15:24:32 mschimek Exp $ */

#include <iostream>
#include <iomanip>

#include <assert.h>

#include "libzvbi.h"

namespace vbi {
  static inline unsigned int rev8 (uint8_t c)
    { return vbi_rev8 (c); };
  static inline unsigned int rev8 (const uint8_t* p)
    { return vbi_rev8 (*p); };
  static inline unsigned int rev16 (uint16_t c)
    { return vbi_rev16 (c); }
  static inline unsigned int rev16 (const uint8_t* p)
    { return vbi_rev16p (p); };

  static inline unsigned int fpar8 (uint8_t c)
    { return vbi_fpar8 (c); };
  static inline void fpar (uint8_t* p, unsigned int n)
    { vbi_fpar (p, n); };
  static inline void fpar (uint8_t* begin, uint8_t* end)
    { vbi_fpar (begin, end - begin); };

  static inline int ipar8 (uint8_t c)
    { return vbi_ipar8 (c); };
  static inline int ipar (uint8_t* p, unsigned int n)
    { return vbi_ipar (p, n); };
  static inline int ipar (uint8_t* begin, uint8_t* end)
    { return vbi_ipar (begin, end - begin); };

  static inline unsigned int fham8 (unsigned int c)
    { return vbi_fham8 (c); };
  static inline void fham16 (uint8_t* p, uint8_t c)
    {
      p[0] = vbi_fham8 (c);
      p[1] = vbi_fham8 (c >> 4);
    }

  static inline int iham8 (uint8_t c)
    { return vbi_iham8 (c); };
  static inline int iham16 (uint16_t c)
    { return ((int) vbi_hamm8_inv[c & 255])
	| ((int) vbi_hamm8_inv[c >> 8] << 4); };
  static inline int iham16 (uint8_t* p)
    { return vbi_iham16p (p); };
  static inline int iham24 (uint8_t* p)
    { return vbi_iham24p (p); };
};

static unsigned int
parity				(unsigned int		n)
{
	unsigned int sh;

	for (sh = sizeof (n) * 8 / 2; sh > 0; sh >>= 1)
		n ^= n >> sh;

	return n & 1;
}

#define BC(n) ((n) * (unsigned int) 0x0101010101010101ULL)

static unsigned int
population_count		(unsigned int		n)
{
	n -= (n >> 1) & BC (0x55);
	n = (n & BC (0x33)) + ((n >> 2) & BC (0x33));
	n = (n + (n >> 4)) & BC (0x0F);

	return (n * BC (0x01)) >> (sizeof (unsigned int) * 8 - 8);
}

unsigned int
hamming_distance		(unsigned int		a,
				 unsigned int		b)
{
	return population_count (a ^ b);
}

int
main				(int			argc,
				 char **		argv)
{
	unsigned int i;

	for (i = 0; i < 10000; ++i) {
		unsigned int n = (i < 256) ? i : mrand48 ();
		uint8_t buf[4] = { n, n >> 8, n >> 16 };
		unsigned int r;
		unsigned int j;

		for (r = 0, j = 0; j < 8; ++j)
			if (n & (0x01 << j))
				r |= 0x80 >> j;

		assert (r == vbi::rev8 (n));
		assert (vbi::rev8 (n) == vbi::rev8 (buf));
		assert (vbi::rev16 (n) == vbi::rev16 (buf));

		if (parity (n & 0xFF))
			assert (vbi::ipar8 (n) == (int)(n & 127));
		else
			assert (vbi::ipar8 (n) == -1);

		assert (vbi::ipar8 (vbi::fpar8 (n)) >= 0);

		vbi::fpar (buf, sizeof (buf));
		assert (vbi::ipar (buf, sizeof (buf)) >= 0);
	}

	for (i = 0; i < 10000; ++i) {
		unsigned int n = (i < 256) ? i : mrand48 ();
		uint8_t buf[4] = { n, n >> 8, n >> 16 };
		unsigned int A, B, C, D;
		int d;

		A = parity (n & 0xA3);
		B = parity (n & 0x8E);
		C = parity (n & 0x3A);
		D = parity (n & 0xFF);

		d = (+ ((n & 0x02) >> 1)
		     + ((n & 0x08) >> 2)
		     + ((n & 0x20) >> 3)
		     + ((n & 0x80) >> 4));

		if (A && B && C) {
			unsigned int nn;

			nn = D ? n : (n ^ 0x40);

			assert (vbi::fham8 (d) == (nn & 255));
			assert (vbi::iham8 (nn) == d);
		} else if (!D) {
			unsigned int nn;
			int dd;

			dd = vbi::iham8 (n);
			assert (dd >= 0 && dd <= 15);

			nn = vbi::fham8 (dd);
			assert (hamming_distance (n & 255, nn) == 1);
		} else {
			assert (vbi::iham8 (n) == -1);
		}

		vbi::fham16 (buf, n);
		assert (vbi::iham16 (buf) == (int)(n & 255));
	}

	for (i = 0; i < (1 << 24); ++i) {
		uint8_t buf[4] = { i, i >> 8, i >> 16 };
		unsigned int A, B, C, D, E, F;
		int d;

		A = parity (i & 0x555555);
		B = parity (i & 0x666666);
		C = parity (i & 0x787878);
		D = parity (i & 0x007F80);
		E = parity (i & 0x7F8000);
		F = parity (i & 0xFFFFFF);

		d = (+ ((i & 0x000004) >> (3 - 1))
		     + ((i & 0x000070) >> (5 - 2))
		     + ((i & 0x007F00) >> (9 - 5))
		     + ((i & 0x7F0000) >> (17 - 12)));
		
		if (A && B && C && D && E) {
			assert (vbi::iham24 (buf) == d);
		} else if (F) {
			assert (vbi::iham24 (buf) < 0);
		} else {
			unsigned int err;
			unsigned int ii;

			err = ((E << 4) | (D << 3)
			       | (C << 2) | (B << 1) | A) ^ 0x1F;

			assert (err > 0);

			if (err >= 24) {
				assert (vbi::iham24 (buf) < 0);
				continue;
			}

			ii = i ^ (1 << (err - 1));

			A = parity (ii & 0x555555);
			B = parity (ii & 0x666666);
			C = parity (ii & 0x787878);
			D = parity (ii & 0x007F80);
			E = parity (ii & 0x7F8000);
			F = parity (ii & 0xFFFFFF);

			assert (A && B && C && D && E && F);

			d = (+ ((ii & 0x000004) >> (3 - 1))
			     + ((ii & 0x000070) >> (5 - 2))
			     + ((ii & 0x007F00) >> (9 - 5))
			     + ((ii & 0x7F0000) >> (17 - 12)));

			assert (vbi::iham24 (buf) == d);
		}
	}

	return 0;
}

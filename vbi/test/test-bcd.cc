/*
 *  libzvbi test
 *  Copyright (C) 2003 Michael H. Schimek
 */

/* $Id: test-bcd.cc,v 1.1.2.5 2004-07-09 16:10:55 mschimek Exp $ */

#include <iostream>
#include <iomanip>

#include <assert.h>

#include "src/zvbi.h"

#define INT_BITS (sizeof (int) * 8)

namespace vbi {
  class Bcd {
    int	value;

  public:
    Bcd () {};
    Bcd (int n) : value (n) {};

    operator int () const
      { return value; };

    int dec (void)
      { return vbi_bcd2dec (value); };
    Bcd& bcd (int n)
      { value = vbi_dec2bcd (n); return *this; };

    Bcd operator+ ()
      { return *this; };
    Bcd operator- ()
      { Bcd t (vbi_neg_bcd (value)); return t; };

    Bcd& operator+= (const Bcd& other)
      { value = vbi_add_bcd (value, other.value); return *this; };
    Bcd& operator++ ()
      { value = vbi_add_bcd (value, 0x1); return *this; };
    Bcd operator++ (int)
      { Bcd t = *this; ++*this; return t; };

    Bcd& operator-= (const Bcd& other)
      { value = vbi_sub_bcd (value, other.value); return *this; };
    Bcd& operator-- ()
      { value = vbi_sub_bcd (value, 0x1); return *this; };
    Bcd operator-- (int)
      { Bcd t = *this; --*this; return t; };

    bool operator== (int n) const
      { return value == n; };
    bool operator== (const Bcd& other) const
      { return value == other.value; };
    bool operator!= (int n) const
      { return value != n; };
    bool operator!= (const Bcd& other) const
      { return value != other.value; };

    bool valid () const
      { return vbi_is_bcd (value); };
  };

  static inline Bcd operator+ (Bcd a, const Bcd& b)
    { a += b; return a; }
  static inline Bcd operator- (Bcd a, const Bcd& b)
    { a -= b; return a; }
  static inline
    std::ostream& operator<<	(std::ostream&		stream,
				 const Bcd&		b)
    {
      std::ios::fmtflags flags = stream.flags ();

      stream << std::hex << (int) b;
      stream.flags (flags);
      return stream;
    }

  typedef Bcd Pgno;
  typedef Bcd Subno;

  static const Bcd any_subno (VBI_ANY_SUBNO);
  static const Bcd no_subno (VBI_NO_SUBNO);
};

static int
modulo				(int			n)
{
	static const int m = -VBI_BCD_DEC_MIN;

	if (n == VBI_BCD_DEC_MIN)
		return n;
	else if (n < 0)
		return -((-n) % m);
	else
		return n % m;
}

static int
long_random			()
{
	int r = mrand48 ();

	if (INT_BITS > 32)
		r ^= mrand48 () << (INT_BITS - 32);

	return r;
}

static vbi_bool
digits_greater			(unsigned int		a,
				 unsigned int		b)
{
	unsigned int j;
	for (j = 0; j < (INT_BITS - 4); j += 4)
		if (((a >> j) & 0xF) > ((b >> j) & 0xF))
			return TRUE;

	return FALSE;
}

int
main				(void)
{
	int i;
	int x, y, z;
	vbi::Bcd a, b, c;
	vbi::Bcd d (0x1234);

	assert (0x1234 == d);

	assert (0x3F7F == vbi::any_subno);
	assert (0x3F7F == vbi::no_subno);

	assert (VBI_BCD_DEC_MIN == vbi_bcd2dec (VBI_BCD_MIN));
	assert (VBI_BCD_DEC_MAX == vbi_bcd2dec (VBI_BCD_MAX));

	for (i = -70000; i < 110000; ++i) {
		switch (i / 10000) {
		case 7:
			x = VBI_BCD_DEC_MAX;
			break;
		case 8:
			x = 0;
			break;
		case 9:
			x = VBI_BCD_DEC_MIN;
			break;
		case 10:
			x = modulo (long_random ());
			break;
		default:
			x = i;
			break;
		}

		y = vbi_dec2bcd (x);
		z = vbi_bcd2dec (y);
		assert (x == z);

		a.bcd (x);

		assert (y == (int) a);
		assert (x == a.dec ());

		assert (a.valid ());

		b = y;
		c.bcd (b.dec ());
		assert ((c == b) == b.valid ());

		b = -a;
		c = -b;
		assert (modulo (-x) == b.dec ());
		assert (modulo (x) == c.dec ());

		switch (i & 0xF00) {
		case 0x200:
			y = VBI_BCD_DEC_MAX;
			break;
		case 0x700:
			y = 0;
			break;
		case 0xB00:
			y = VBI_BCD_DEC_MIN;
			break;
		default:
			y = modulo (long_random ());
			break;
		}

		a.bcd (x);
		b.bcd (y);
		c = a; c += b;
		assert (modulo (x + y) == c.dec ());
		c = a + b;
		assert (modulo (x + y) == c.dec ());
		c = a; ++c;
		assert (modulo (x + 1) == c.dec ());
		c = a++;
		assert (modulo (x + 0) == c.dec ());
		assert (modulo (x + 1) == a.dec ());

		assert (a != c);
		assert (a == a);

		a.bcd (x);
		b.bcd (y);
		c = a; c -= b;
		assert (modulo (x - y) == c.dec ());
		c = a - b;
		assert (modulo (x - y) == c.dec ());
		c = a; --c;
		assert (modulo (x - 1) == c.dec ());
		c = a--;
		assert (modulo (x - 0) == c.dec ());
		assert (modulo (x - 1) == a.dec ());

		a.bcd (x);
		b.bcd (y);
		c = a + (-b);
		assert (modulo (x - y) == c.dec ());
		c = a - (-b);
		assert (modulo (x + y) == c.dec ());

		if (x > 0) {
			const unsigned int d = (1 << (INT_BITS - 4)) - 1;

			a.bcd (x);
			b.bcd (y);

			assert (digits_greater (d & (int) b, (int) a)
				== vbi_bcd_digits_greater
				(d & (int) b, (int) a));

			y = d & random();

			assert (digits_greater (y, (int) a)
				== vbi_bcd_digits_greater (y, (int) a));
		}
	}

	return 0;
}

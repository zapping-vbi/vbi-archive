/*
 *  libzvbi test
 *
 *  Copyright (C) 2002-2003 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: cpptest.cc,v 1.1.2.1 2003-05-02 10:44:26 mschimek Exp $ */

#include <assert.h>
#include "libzvbi.h"

#include <iostream>
#include <iomanip>

namespace vbi {

  class Bcd {
    int			value;

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
mod				(int			n)
{
	if (n < 0)
		return (10000 + n) % 10000;
	else
		return n % 10000;
}

static void
bcd_test			(void)
{
	int x, y, z;
	vbi::Bcd a, b, c;
	vbi::Bcd d(0x1234);

	assert (d == 0x1234);

	for (x = 0; x < 10000; ++x) {
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
		assert (mod (-x) == b.dec ());
		assert (mod (x) == c.dec ());

		y = rand () % 10000;

		a.bcd (x);
		b.bcd (y);
		c = a; c += b;
		assert (mod (x + y) == c.dec ());
		c = a + b;
		assert (mod (x + y) == c.dec ());
		c = a; ++c;
		assert (mod (x + 1) == c.dec ());
		c = a++;
		assert (mod (x + 0) == c.dec ());
		assert (mod (x + 1) == a.dec ());

		assert (a != c);
		assert (a == a);

		a.bcd (x);
		b.bcd (y);
		c = a; c -= b;
		assert (mod (x - y) == c.dec ());
		c = a - b;
		assert (mod (x - y) == c.dec ());
		c = a; --c;
		assert (mod (x - 1) == c.dec ());
		c = a--;
		assert (mod (x - 0) == c.dec ());
		assert (mod (x - 1) == a.dec ());

		a.bcd (x);
		b.bcd (y);
		c = a + (-b);
		assert (mod (x - y) == c.dec ());
		c = a - (-b);
		assert (mod (x + y) == c.dec ());
	}
}

int
main				(int			argc,
				 char **		argv)
{
	bcd_test ();

	return 0;
}

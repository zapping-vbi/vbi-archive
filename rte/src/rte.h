/*
 *  Real Time Encoder
 *
 *  Copyright (C) 2000-2001 Iñaki García Etxebarria
 *  Modified 2001 Michael H. Schimek
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
#ifndef __RTE_H__
#define __RTE_H__

/*
  A nice set of HTML rendered docs can be found here:
  http://zapping.sf.net/docs/rte/index.html
  FIXME: Upload docs before the release.
*/

#include "rte-enums.h"
#include "rte-types.h"
#include "rte-version.h"

/**
 * rte_init:
 *
 * Checks things (testing gtkdoc).
 * Return value: %TRUE if the library is usable.
 **/
rte_bool rte_init( void );
rte_context *rte_context_new ();

#endif /* rte.h */

/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2001 Iñaki García Etxebarria
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

typedef enum {
  CPU_UNKNOWN,
  CPU_PENTIUM_MMX,
  CPU_PENTIUM_II,
  CPU_PENTIUM_III,
  CPU_PENTIUM_4,
  CPU_K6_2,
  CPU_ATHLON,
  CPU_CYRIX_MII,
  CPU_CYRIX_III,
} cpu_type;

extern cpu_type cpu_detection(void);

/*
 * RTE (Real time encoder) front end for Zapping
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
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

/* $Id: schedule.c,v 1.3 2003-11-29 19:43:22 mschimek Exp $ */

#include "plugin_common.h"

#ifdef HAVE_LIBRTE

#include <math.h>
#include <glade/glade.h>
#include "mpeg.h"

/* future */

/*

 * Schedule dialog:
  - Set End date same as Start when entered, time blank, accept syntax +hh:mm
  - Code sets Station, Start and End, VPS/PDC defaults to Start time (hhmm),
    rest same as last entered
  - Add option Skip Commercials when plugin available (more?)
  - Add file name?
  - Permit overlapping times on a first served basis, consider future support
    for multiple devices
 * TTX context on program pages (VPT): Add to schedule. Callback? Event?
 * XDS program info pages (context or else?): Add to schedule. Callback? Event?
 * Rec window: add Record this until..., Record next until...
   VPS/PDC/XDS program end signal, +minutes or end time
 * zconf: custom extension to channel properties may be useful, cf.
   GTK object data, for Video+ channel codes and other channel related
   info a plugin gathers.
 * Configuration to be discussed:
  - Zapping running in background on the desktop, disadvantage: X11 has to
    be active 24/7. What can we use as alarm clock, is GTK reliable? Need
    failsave to record even when critical code (X11, GTK, Zapping) crashed,
    eg. fork off the timer process, auto-restart on fatal error or similar
    measures.
  - Little "daemon". Basically tveng, libvbi, rte plus some glue code,
    difficulty is seamless interoperation with full Zapping, see below.
  - other?
 * Modes of operation: A)ctive (displaying TV as usual), S)tandby,
   R)ecording a)active or in b)ackground. No automatic A -> Ra except warning
   when the scheduled program starts. A -> Ra -> A by user as usual. Ra -> Rb
   and A -> S to be defined. User requested Rb -> A shall enter capture mode,
   recording (rec status window) continue without interruption. S -> A must
   stop vbi monitoring, otherwise standard startup. S <-> Rb automatic,
   display disabled. S <-> Ra or A <-> Rb not permitted.
  - Z shouldn't consume resources (devices, memory, CPU etc) while in S. When
    we want VPS/PDC/XDS VCR triggers, vbi monitoring (vbi events available
    to plugins) and tuning is necessary, but only around the scheduled day.
    Take possible future EPG daemon into account.
  - Display can/must be disabled in Rb, the mmap-memcpy in tveng_read_frame
    becomes redundant. Examine reversed allocation.
 * Keep multiple devices and time shifting / playback in mind.

 */

#endif /* HAVE_LIBRTE */

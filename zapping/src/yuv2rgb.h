/* 
 *
 *  yuv2rgb.h, Software YUV to RGB coverter
 *
 *
 *  Copyright (C) 1999, Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *  All Rights Reserved. 
 * 
 *  Functions broken out from display_x11.c and several new modes
 *  added by Håkan Hjort <d95hjort@dtek.chalmers.se>
 * 
 *  15 & 16 bpp support by Franck Sicard <Franck.Sicard@solsoft.fr>
 *
 *  This file is part of mpeg2dec, a free MPEG-2 video decoder
 *	
 *  mpeg2dec is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  mpeg2dec is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 */

#ifndef __YUV2RGB_H__
#define __YUV2RGB_H__

void startup_yuv2rgb (void);
void shutdown_yuv2rgb (void);

#endif

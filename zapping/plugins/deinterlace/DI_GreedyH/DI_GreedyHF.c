/////////////////////////////////////////////////////////////////////////////
// $Id: DI_GreedyHF.c,v 1.1 2005-01-08 14:54:23 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2001 Tom Barry.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//	This file is subject to the terms of the GNU General Public License as
//	published by the Free Software Foundation.  A copy of this license is
//	included with this software distribution in the file COPYING.  If you
//	do not have a copy, you may obtain a copy by writing to the Free
//	Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//	This software is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details
//
/////////////////////////////////////////////////////////////////////////////
// Change Log
//
// Date          Developer             Changes
//
// 01 Feb 2001   Tom Barry		       New Greedy (High Motion)Deinterlace method
//
// 29 Jul 2001   Tom Barry		       Move CPU dependent code to DI_GreedyHF.asm
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.6  2001/07/30 21:50:32  trbarry
// Use weave chroma for reduced chroma jitter. Fix DJR bug again.
// Turn off Greedy Pulldown default.
//
// Revision 1.5  2001/07/30 18:18:59  trbarry
// Fix new DJR bug
//
// Revision 1.4  2001/07/30 17:56:26  trbarry
// Add Greedy High Motion MMX, K6-II, K6-III, and Celeron support.
// Tweak defaults.
//
// Revision 1.3  2001/07/28 18:47:24  trbarry
// Fix Sharpness with Median Filter
// Increase Sharpness default to make obvious
// Adjust deinterlace defaults for less jitter
//
// Revision 1.2  2001/07/25 12:04:31  adcockj
// Moved Control stuff into DS_Control.h
// Added $Id and $Log to comment blocks as per standards
//
/////////////////////////////////////////////////////////////////////////////

// This is the first version of the Greedy High Motion Deinterlace method I wrote (and kept). 
// It doesn't have many of the fancier options but I left it in because it's faster. It runs with 
// a field delay of 1 in a single pass with no call needed to UpdateFieldStore. It will be called
// if no special options are needed. The logic is somewhat different than the other rtns.  TRB 7/2001
//
// This version has been modified to be compatible with other DScaler functions such as Auto Pulldown.
// It will now automatically be call if none of the UI check boxes are check and also if we are not
// running on an SSE capable machine (Athlon, Duron, P-III, fast Celeron).


#include "windows.h"
#include "DS_Deinterlace.h"
#include "DI_GreedyHM.h"


#define IS_SSE
#define SSE_TYPE SSE
#define FUNCT_NAME DI_GreedyHF_SSE
#include "DI_GreedyHF.asm"
#undef SSE_TYPE
#undef IS_SSE
#undef FUNCT_NAME

#define IS_3DNOW
#define FUNCT_NAME DI_GreedyHF_3DNOW
#define SSE_TYPE 3DNOW
#include "DI_GreedyHF.asm"
#undef SSE_TYPE
#undef IS_3DNOW
#undef FUNCT_NAME

#define IS_3DNOW
#define SSE_TYPE MMX
#define FUNCT_NAME DI_GreedyHF_MMX
#include "DI_GreedyHF.asm"
#undef SSE_TYPE
#undef IS_3DNOW
#undef FUNCT_NAME


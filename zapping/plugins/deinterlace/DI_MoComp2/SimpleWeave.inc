/////////////////////////////////////////////////////////////////////////////
// $Id: SimpleWeave.inc,v 1.1 2005-01-08 14:54:24 mschimek Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2003 Tom Barry & John Adcock.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.  If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
//
//  (From Tom Barry)
//  Also, this program is "Philanthropy-Ware".  That is, if you like it and 
//  feel the need to reward or inspire the author then please feel free (but
//  not obligated) to consider joining or donating to the Electronic Frontier
//  Foundation. This will help keep cyber space free of barbed wire and bullsh*t.  
//  See www.eff.org for details
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: not supported by cvs2svn $
// Revision 1.2  2003/05/26 15:54:36  adcockj
// Improved Performance of motion detection
// Fixed bug with pBobP
//
// Revision 1.1  2003/02/11 15:41:04  adcockj
// New implementation of StrangeBob based off paper
// Tidy up sources
//
// Revision 1.1  2003/01/02 13:15:01  adcockj
// Added new plug-ins ready for developement by copying TomsMoComp and Gamma
//
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Just gets the values for a simple weave
/////////////////////////////////////////////////////////////////////////////

"    ## at the start of this function\n"
"    ## mm0 = 0\n"
"    ## mm6 = Bob pixels          \n"
"\n"
"    movq mm1, qword ptr[edi+ecx]\n"
"    movq mm2, qword ptr[esi+ecx]\n"
"    movq mm3, mm1			/* another copy of our pixel1 value */ \n"
"    movq mm4, mm2			/* another copy of our pixel2 value */ \n"
"\n"
    PABS(mm3, mm4, mm5)
"\n"
    V_PAVGB (mm1, mm2, mm4, ShiftMask) /* avg of 2 pixels */ 
"\n"
"    ## these must be the values as we exit\n"
"    ## mm0 = 0\n"
"    ## mm1 = weave pixels\n"
"    ## mm3 = \"movement\" in the centre\n"
"    ## mm6 = Bob pixels          \n"


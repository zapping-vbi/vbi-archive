

#ifdef IS_SSE2
"##sse2 code deleted for now\n"
#else
"\n"
"## Version for non-SSE2\n"
"\n"
#ifdef SKIP_SEARCH
"        movq    mm0, mm6            ## just use the results of our wierd bob\n"
#else
"\n"
"\n"
"## JA 9/Dec/2002\n"
"## failed experiment\n"
"## but leave in placeholder for me to play about\n"
#ifdef DONT_USE_STRANGE_BOB
"        ## Use the best weave if diffs less than 10 as that\n"
"        ## means the image is still or moving cleanly\n"
"        ## if there is motion we will clip which will catch anything\n"
"        psubusb mm7, FOURS          ## sets bits to zero if weave diff < 4\n"
"        pxor    mm0, mm0\n"
"        pcmpeqb mm7, mm0            ## all ff where weave better, else 00\n"
"        pcmpeqb mm0, mm7            ## all ff where bob better, else 00\n"
"        pand    mm0, mm6            ## use bob for these pixel values\n"
"        pand    mm7, mm5            ## use weave for these\n"
"        por     mm0, mm7            ## combine both\n"
#else
"        ## Use the better of bob or weave\n"
"##      pminub  mm4, TENS           ## the most we care about\n"
        V_PMINUB (mm4, TENS, mm0)   // the most we care about
"\n"
"        psubusb mm7, mm4            ## foregive that much from weave est?\n"
"        psubusb mm7, FOURS          ## bias it a bit toward weave\n"
"        pxor    mm0, mm0\n"
"        pcmpeqb mm7, mm0            ## all ff where weave better, else 00\n"
"        pcmpeqb mm0, mm7            ## all ff where bob better, else 00\n"
"        pand    mm0, mm6            ## use bob for these pixel values\n"
"        pand    mm7, mm5            ## use weave for these\n"
"        por     mm0, mm7            ## combine both\n"
#endif
"\n"
"\n"
"##      pminub  mm0, Max_Vals       ## but clip to catch the stray error\n"
        V_PMINUB (mm0, %[Max_Vals], mm1) // but clip to catch the stray error
"##      pmaxub  mm0, Min_Vals\n"
        V_PMAXUB (mm0, %[Min_Vals])
"\n"
#endif
"\n"
"\n"
"        mov     eax, %[pDest]\n"
"\n"
#ifdef USE_VERTICAL_FILTER
"        movq    mm1, mm0\n"
"##      pavgb   mm0, qword ptr[ebx]\n"
        V_PAVGB (mm0, qword ptr[ebx], mm2, ShiftMask)
"##      movntq  qword ptr[eax+edx], mm0\n"
        V_MOVNTQ (qword ptr[eax+edx], mm0)
"##      pavgb   mm1, qword ptr[ebx+ecx]\n"
        V_PAVGB (mm1, qword ptr[ebx+ecx], mm2, ShiftMask)
"        add     eax, %[dst_pitchw]\n"
"##      movntq  qword ptr[eax+edx], mm1\n"
        V_MOVNTQ (qword ptr[eax+edx], mm1)
#else
"\n"
"##      movntq  qword ptr[eax+edx], mm0\n"
        V_MOVNTQ (qword ptr[eax+edx], mm0)
#endif
"\n"
"        lea     edx, [edx+8]        ## bump offset pointer\n"
"        cmp     edx, %[Last8]          ## done with line?\n"
"        jb      1b # LoopQ               ## y\n"
#endif
"        \n"
_asm_end,
_m_array(Min_Vals), _m_array(Max_Vals),
_m(pDest), _m(Last8), _m(pSrc), _m(pSrcP),
_m(pBob), _m(pBobP), _m(src_pitch2),
/* Static consts gcc must not optimize away. */
_m(ShiftMask), _m_array(YMask), _m_array(UVMask), _m_array(DiffThres),
_m_array(ONES), _m_array(TENS), _m_array(FOURS), _m_array(Max_Mov)
: "eax", "ecx", "edx", "esi", "edi");

        // adjust for next line
        pSrc  += src_pitch2;
        pSrcP += src_pitch2;
        pDest += dst_pitch2;
        pBob += src_pitch2;
        pBobP += src_pitch2;
    }
    
    return 0;

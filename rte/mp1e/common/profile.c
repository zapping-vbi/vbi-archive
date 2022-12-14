/*
 *  MPEG Real Time Encoder
 *  Simple Profiling
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: profile.c,v 1.6 2005-02-25 18:30:47 mschimek Exp $ */

#include "site_def.h"

#ifdef PROFILING

#include <stdio.h>

typedef long long int tsc_t;

// Keep in mind TSC counts real time, not process time

tsc_t rdtsc(void)
{
    tsc_t tsc;

    asm ("\trdtsc\n" : "=A" (tsc));

    return tsc;
}                                                                                         

#define COUNTERS 80

static char *labels[COUNTERS];
static tsc_t start[COUNTERS];
static long long sum[COUNTERS];
static int count[COUNTERS];

void
pr_start(int n, char *label)
{
	if (n > COUNTERS)
		return;

	labels[n] = label;
	start[n] = rdtsc();
}

void
pr_event(int n, char *label)
{
	if (n > COUNTERS)
		return;

	labels[n] = label;
	count[n]++;
}

void
pr_end(int n)
{
	if (n > COUNTERS)
		return;

	sum[n] += rdtsc() - start[n];
	count[n]++;
}

void
pr_report(void)
{
	int i;

	for (i = 0; i < COUNTERS; i++)
		if (count[i]) {
			if (sum[i] > 0)
				fprintf(stderr, "%25s %02d: %10lld cycles %8d iterations (%4lld %06lld)\n",
					labels[i], i, sum[i] / count[i], count[i], sum[i] / 1000000, sum[i] % 1000000);
			else
				fprintf(stderr, "%25s %02d:                   %8d iterations\n", labels[i], i, count[i]);
		}
}

#endif /* PROFILING */

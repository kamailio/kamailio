/*
 * $Id: benchmark.h 825 2007-02-16 13:04:16Z bastian $
 *
 * Benchmarking module for OpenSER
 *
 * Copyright (C) 2007 Collax GmbH
 *                    (Bastian Friedrich <bastian.friedrich@collax.com>)
 * Copyright (C) 2007 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _BENCHMARK_MOD_H_
#define _BENCHMARK_MOD_H_

#include <sys/time.h>
#include <time.h>

#include "benchmark_api.h"

#define BM_NAME_LEN	32

#ifdef BM_CLOCK_REALTIME
/* nano seconds */
typedef struct timespec bm_timeval_t;
#else
/* micro seconds */
typedef struct timeval bm_timeval_t;
#endif

typedef struct benchmark_timer
{
	char name[BM_NAME_LEN];
	unsigned int id;
	int enabled;
	bm_timeval_t *start;    /* Current timer run */
	long long calls;		/* Number of runs of this timer */
	long long sum;			/* Accumulated runtime of this timer */
	long long last_sum;		/* Accumulated runtime since last logging */
	long long last_max;		/* Minimum in current period (between
							   granularity) */
	long long last_min;		/* Maximum ... */
	long long global_max;	/* Global minimum, since start */
	long long global_min;	/* ...    maximum ... */
	struct benchmark_timer *next;
} benchmark_timer_t;

inline int bm_get_time(bm_timeval_t *t)
{
#ifdef BM_CLOCK_REALTIME
	if(clock_gettime(CLOCK_REALTIME, t)!=0)
#else
	if(gettimeofday(t, NULL))
#endif
	{
		LM_ERR("error getting current time\n");
		return -1;
	}

	return 0;
}

inline unsigned long long bm_diff_time(bm_timeval_t *t1, bm_timeval_t *t2)
{
	unsigned long long tdiff;

	tdiff = t1->tv_sec - t1->tv_sec;
	
#ifdef BM_CLOCK_REALTIME
	tdiff = tdiff*1000000000 + t2->tv_nsec - t1->tv_nsec;
#else
	tdiff = tdiff*1000000 + t2->tv_usec - t1->tv_usec;
#endif

	return tdiff;
}

#endif /* _BENCHMARK_MOD_H_ */

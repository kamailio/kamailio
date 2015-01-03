/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifndef _T_STATS_H
#define _T_STATS_H

/* if defined even more stats are produced */
#define TM_MORE_STATS

#include "defs.h"

#include "../../rpc.h"
#include "../../pt.h"


typedef unsigned long stat_counter;

struct t_proc_stats {
	/* number of transactions in wait state */
	stat_counter waiting;
	/* number of server transactions */
	stat_counter transactions;
	/* number of UAC transactions (part of transactions) */
	stat_counter client_transactions;
	/* number of transactions which completed with this status */
	stat_counter completed_3xx, completed_4xx, completed_5xx, 
		completed_6xx, completed_2xx;
	stat_counter replied_locally;
	stat_counter deleted;
#ifdef TM_MORE_STATS
	/* number of created transactions */
	stat_counter t_created;
	/* number of freed transactions */
	stat_counter t_freed;
	/* number of transactions for which free was deleted */
	stat_counter delayed_free;
#endif /* TM_MORE_STATS */
};

union t_stats{
	struct t_proc_stats s;
	char _pad[256]; /* pad at least to cache line size 
	                    athlon=64, p4=128, some sparcs=256 */
};
extern union t_stats *tm_stats;

#ifdef TM_MORE_STATS 
inline void static t_stats_created(void)
{
	/* keep it in process's piece of shmem */
	tm_stats[process_no].s.t_created++;
}

inline void static t_stats_freed(void)
{
	/* keep it in process's piece of shmem */
	tm_stats[process_no].s.t_freed++;
}

inline void static t_stats_delayed_free(void)
{
	/* keep it in process's piece of shmem */
	tm_stats[process_no].s.delayed_free++;
}
#else /* TM_MORE_STATS  */
/* do nothing */
#define t_stats_created()  do{}while(0)
#define t_stats_freed()  do{}while(0)
#define t_stats_delayed_free()  do{}while(0)

#endif /* TM_MORE_STATS */

inline void static t_stats_new(int local)
{
	/* keep it in process's piece of shmem */
	tm_stats[process_no].s.transactions++;
	if(local) tm_stats[process_no].s.client_transactions++;
}

inline void static t_stats_wait(void)
{
	/* keep it in process's piece of shmem */
	tm_stats[process_no].s.waiting++;
}

inline void static t_stats_deleted( int local )
{
	tm_stats[process_no].s.deleted++;
}

inline static void update_reply_stats( int code ) {
	if (code>=600) {
		tm_stats[process_no].s.completed_6xx++;
	} else if (code>=500) {
		tm_stats[process_no].s.completed_5xx++;
	} else if (code>=400) {
		tm_stats[process_no].s.completed_4xx++;
	} else if (code>=300) {
		tm_stats[process_no].s.completed_3xx++;
	} else if (code>=200) {
		tm_stats[process_no].s.completed_2xx++;
	}
}


inline void static t_stats_replied_locally(void)
{
	tm_stats[process_no].s.replied_locally++;
}



int init_tm_stats(void);
int init_tm_stats_child(void);
void free_tm_stats(void);

void tm_rpc_stats(rpc_t* rpc, void* c);

void tm_rpc_hash_stats(rpc_t* rpc, void* c);

typedef int (*tm_get_stats_f)(struct t_proc_stats *all);
int tm_get_stats(struct t_proc_stats *all);

#endif

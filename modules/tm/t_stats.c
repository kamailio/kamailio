/*
 *
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * History:
 * --------
 *  2003-06-27  tm_stats & friends freed on exit only if non-null (andrei)
 */


#include "defs.h"


#include <stdio.h>
#include "t_stats.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../pt.h"

union t_stats *tm_stats=0;

int init_tm_stats(void)
{
	     /* Delay initialization of tm_stats  to
	      * init_tm_stats_child which gets called from child_init,
	      * in mod_init function other modules can increase the value of
	      * estimated_process_count and thus we do not know about processes created
	      * from modules which get loaded after tm and thus their mod_init
	      * functions will be called after tm mod_init function finishes
	      */
	return 0;
}


int init_tm_stats_child(void)
{
	int size;

	/* We are called from child_init, estimated_process_count has definitive
	 * value now and thus we can safely allocate the variables
	 */
	if (tm_stats==0){
		size=sizeof(*tm_stats) * get_max_procs();
		tm_stats=shm_malloc(size);
		if (tm_stats == 0) {
			ERR("No mem for stats\n");
			goto error;
		}
		memset(tm_stats, 0, size);
	}
	
	return 0;
error:
	return -1;
}



void free_tm_stats()
{
	if (tm_stats == 0) return;
	shm_free(tm_stats);
	tm_stats=0;
}



const char* tm_rpc_stats_doc[2] = {
	"Print transaction statistics.",
	0
};


/* res=s1+s2 */
#define tm_proc_stats_add_base(res, s1, s2) \
	do{\
		(res)->waiting=(s1)->waiting+(s2)->waiting; \
		(res)->transactions=(s1)->transactions+(s2)->transactions; \
		(res)->client_transactions=(s1)->client_transactions+\
									(s2)->client_transactions; \
		(res)->completed_3xx=(s1)->completed_3xx+(s2)->completed_3xx; \
		(res)->completed_4xx=(s1)->completed_4xx+(s2)->completed_4xx; \
		(res)->completed_5xx=(s1)->completed_5xx+(s2)->completed_5xx; \
		(res)->completed_6xx=(s1)->completed_6xx+(s2)->completed_6xx; \
		(res)->completed_2xx=(s1)->completed_2xx+(s2)->completed_2xx; \
		(res)->replied_locally=(s1)->replied_locally+(s2)->replied_locally; \
		(res)->deleted=(s1)->deleted+(s2)->deleted; \
	}while(0)


#ifdef TM_MORE_STATS
#define tm_proc_stats_add(res, s1, s2) \
	do{\
		tm_proc_stats_add_base(res, s1, s2); \
		(res)->t_created=(s1)->t_created+(s2)->t_created; \
		(res)->t_freed=(s1)->t_freed+(s2)->t_freed; \
		(res)->delayed_free=(s1)->delayed_free+(s2)->delayed_free; \
	}while(0)
#else
#define tm_proc_stats_add(res, s1, s2) tm_proc_stats_add_base(res, s1, s2)
#endif



/* we don't worry about locking data during reads (unlike
 * setting values which always happens from some locks) 
 */
void tm_rpc_stats(rpc_t* rpc, void* c)
{
	void* st;
	unsigned long current, waiting;
	struct t_proc_stats all;
	int i, pno;

	pno = get_max_procs();
	memset(&all, 0, sizeof(all));
	for(i = 0;i < pno; i++) {
		tm_proc_stats_add(&all, &all, &tm_stats[i].s);
	}
	current = all.transactions - all.deleted;
	waiting = all.waiting - all.deleted;

	if (rpc->add(c, "{", &st) < 0) return;

	rpc->struct_add(st, "dd", "current", (unsigned) current, "waiting",
										 (unsigned) waiting);
	rpc->struct_add(st, "d", "total", (unsigned) all.transactions);
	rpc->struct_add(st, "d", "total_local", (unsigned)all.client_transactions);
	rpc->struct_add(st, "d", "replied_locally", (unsigned)all.replied_locally);
	rpc->struct_add(st, "ddddd", 
			"6xx", (unsigned int)all.completed_6xx,
			"5xx", (unsigned int)all.completed_5xx,
			"4xx", (unsigned int)all.completed_4xx,
			"3xx", (unsigned int)all.completed_3xx,
			"2xx", (unsigned int)all.completed_2xx);
#ifdef TM_MORE_STATS 
	rpc->struct_add(st, "dd", "created", (unsigned int)all.t_created, "freed",
						(unsigned int)all.t_freed);
	rpc->struct_add(st, "d", "delayed_free", (unsigned int)all.delayed_free);
#endif
	/* rpc->fault(c, 100, "Trying"); */
}

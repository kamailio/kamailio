/*
 *
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


#include "defs.h"


#include <stdio.h>
#include "t_stats.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../pt.h"
#ifdef TM_HASH_STATS
#include "h_table.h"
#endif

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

int tm_get_stats(struct t_proc_stats *all)
{
	int i, pno;
	if(all==NULL)
		return -1;

	pno = get_max_procs();
	memset(all, 0, sizeof(struct t_proc_stats));
	for(i = 0; i < pno; i++) {
		tm_proc_stats_add(all, all, &tm_stats[i].s);
	}
	return 0;
}


/*  hash statistics */
void tm_rpc_hash_stats(rpc_t* rpc, void* c)
{
#ifdef TM_HASH_STATS
	void* st;
	unsigned long acc_min, acc_max, acc_zeroes, acc_dev_no;
	unsigned long crt_min, crt_max, crt_zeroes, crt_dev_no;
	unsigned long crt_count, acc_count;
	double acc_average, acc_dev, acc_d;
	double crt_average, crt_dev, crt_d;
	unsigned long acc, crt;
	int r;
	
	acc_count=0;
	acc_min=(unsigned long)(-1);
	acc_max=0;
	acc_zeroes=0;
	acc_dev_no=0;
	acc_dev=0;
	crt_count=0;
	crt_min=(unsigned long)(-1);
	crt_max=0;
	crt_zeroes=0;
	crt_dev_no=0;
	crt_dev=0;
	for (r=0; r<TABLE_ENTRIES; r++){
		acc=_tm_table->entries[r].acc_entries;
		crt=_tm_table->entries[r].cur_entries;
		
		acc_count+=acc;
		if (acc<acc_min) acc_min=acc;
		if (acc>acc_max) acc_max=acc;
		if (acc==0) acc_zeroes++;
		
		crt_count+=crt;
		if (crt<crt_min) crt_min=crt;
		if (crt>crt_max) crt_max=crt;
		if (crt==0) crt_zeroes++;
	}
	acc_average=acc_count/(double)TABLE_ENTRIES;
	crt_average=crt_count/(double)TABLE_ENTRIES;
	
	for (r=0; r<TABLE_ENTRIES; r++){
		acc=_tm_table->entries[r].acc_entries;
		crt=_tm_table->entries[r].cur_entries;
		
		acc_d=acc-acc_average;
		/* instead of fabs() which requires -lm */
		if (acc_d<0) acc_d=-acc_d;
		if (acc_d>1) acc_dev_no++;
		acc_dev+=acc_d*acc_d;
		crt_d=crt-crt_average;
		/* instead of fabs() which requires -lm */
		if (crt_d<0) crt_d=-crt_d;
		if (crt_d>1) crt_dev_no++;
		crt_dev+=crt_d*crt_d;
	}
	
	if (rpc->add(c, "{", &st) < 0) return;
	rpc->struct_add(st, "d", "hash_size", (unsigned) TABLE_ENTRIES);
	rpc->struct_add(st, "d", "crt_transactions", (unsigned)crt_count);
	rpc->struct_add(st, "f", "crt_target_per_cell", crt_average);
	rpc->struct_add(st, "dd", "crt_min", (unsigned)crt_min,
							  "crt_max", (unsigned) crt_max);
	rpc->struct_add(st, "d", "crt_worst_case_extra_cells", 
							(unsigned)(crt_max-(unsigned)crt_average));
	rpc->struct_add(st, "d", "crt_no_zero_cells", (unsigned)crt_zeroes);
	rpc->struct_add(st, "d", "crt_no_deviating_cells", crt_dev_no);
	rpc->struct_add(st, "f", "crt_deviation_sq_sum", crt_dev);
	rpc->struct_add(st, "d", "acc_transactions", (unsigned)acc_count);
	rpc->struct_add(st, "f", "acc_target_per_cell", acc_average);
	rpc->struct_add(st, "dd", "acc_min", (unsigned)acc_min,
							  "acc_max", (unsigned) acc_max);
	rpc->struct_add(st, "d", "acc_worst_case_extra_cells", 
							(unsigned)(acc_max-(unsigned)acc_average));
	rpc->struct_add(st, "d", "acc_no_zero_cells", (unsigned)acc_zeroes);
	rpc->struct_add(st, "d", "acc_no_deviating_cells", acc_dev_no);
	rpc->struct_add(st, "f", "acc_deviation_sq_sum", acc_dev);
#else /* TM_HASH_STATS */
	rpc->fault(c, 500, "Hash statistics not supported (try"
				"recompiling with -DTM_HASH_STATS)");
#endif /* TM_HASH_STATS */
}

/*
 * $Id$
 */

#include "timer.h"
#include "dprint.h"
#include "error.h"
#include "config.h"
#include "mem/mem.h"
#ifdef SHM_MEM
#include "mem/shm_mem.h"
#endif

#include <stdlib.h>


struct sr_timer* timer_list=0;

static int* jiffies=0;
static int timer_id=0;



/* ret 0 on success, <0 on error*/
int init_timer()
{
#ifdef SHM_MEM
	jiffies=shm_malloc(sizeof(int));
#else
	/* in this case get_ticks won't work! */
	LOG(L_INFO, "WARNING: no shared memory support compiled in"
				" get_ticks won't work\n");
	jiffies=malloc(sizeof(int));
#endif
	if (jiffies==0){
		LOG(L_CRIT, "ERROR: init_timer: could not init jiffies\n");
		return E_OUT_OF_MEM;
	}
	*jiffies=0;
	return 0;
}



	
/*register a periodic timer;
 * ret: <0 on error*/
int register_timer(timer_function f, void* param, unsigned int interval)
{
	struct sr_timer* t;

	t=malloc(sizeof(struct sr_timer));
	if (t==0){
		LOG(L_ERR, "ERROR: register_timer: out of memory\n");
		goto error;
	}
	t->id=timer_id++;
	t->timer_f=f;
	t->t_param=param;
	t->interval=interval;
	t->expires=*jiffies+interval;
	/* insert it into the list*/
	t->next=timer_list;
	timer_list=t;
	return t->id;

error:
	return E_OUT_OF_MEM;
}



void timer_ticker()
{
	struct sr_timer* t;
	unsigned int prev_jiffies;
	
	prev_jiffies=*jiffies;
	*jiffies+=TIMER_TICK;
	/* test for overflow (if tick= 1s =>overflow in 136 years)*/
	if (*jiffies<prev_jiffies){ 
		/*force expire & update every timer, a little buggy but it 
		 * happens once in 136 years :) */
		for(t=timer_list;t;t=t->next){
			t->expires=*jiffies+t->interval;
			t->timer_f(*jiffies, t->t_param);
		}
		return;
	}
	
	for (t=timer_list;t; t=t->next){
		if (*jiffies>=t->expires){
			t->expires=*jiffies+t->interval;
			t->timer_f(*jiffies, t->t_param);
		}
	}
}



unsigned int get_ticks()
{
	if (jiffies==0){
		LOG(L_CRIT, "BUG: get_ticks: jiffies not intialized\n");
		return 0;
	}
#ifndef SHM_MEM
	LOG(L_CRIT, "WARNING: get_ticks: no shared memory support compiled in"
			", returning 0 (probably wrong)");
	return 0;
#endif
	DBG("- get_ticks:returning %d\n", *jiffies);
	return *jiffies;
}

/*
 * $Id$
 */

#include "timer.h"
#include "dprint.h"
#include "error.h"
#include "config.h"

#include <stdlib.h>


struct sr_timer* timer_list=0;

static int jiffies=0;
static int timer_id=0;

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
	t->expires=jiffies+interval;
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
	
	prev_jiffies=jiffies;
	jiffies+=TIMER_TICK;
	/* test for overflow (if tick= 1s =>overflow in 136 years)*/
	if (jiffies<prev_jiffies){ 
		/*force expire & update every timer, a little buggy but it 
		 * happens once in 136 years :) */
		for(t=timer_list;t;t=t->next){
			t->expires=jiffies+t->interval;
			t->timer_f(jiffies, t->t_param);
		}
		return;
	}
	
	for (t=timer_list;t; t=t->next){
		if (jiffies>=t->expires){
			t->expires=jiffies+t->interval;
			t->timer_f(jiffies, t->t_param);
		}
	}
}

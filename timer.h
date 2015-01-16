/*
 * timer related functions (public interface)
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
/**
 * @file
 * @brief Kamailio core :: timer related functions (public interface)
 * @ingroup core
 *
 * Module: \ref core
 *
 * - \ref TimerDoc
 */


/**
 * @page TimerDoc Kamailio's timer documentation
 * @verbinclude timers.txt
 *
 */



#ifndef timer_h
#define timer_h

#define USE_SLOW_TIMER /* use another process to run the timer handlers 
						  marked "slow" */
/*#define TIMER_DEBUG -- compile with -DTIMER_DEBUG*/

#include "clist.h"
#include "dprint.h"
#include "timer_ticks.h"

#ifdef USE_SLOW_TIMER
#include <sys/types.h>

typedef unsigned short slow_idx_t; /* type fot the slow index */
extern pid_t slow_timer_pid;
#endif





/* deprecated, old, kept for compatibility */
typedef void (timer_function)(unsigned int ticks, void* param);
/* deprecated, old, kept for compatibility 
	get_ticks()*TIMER_TICK used to be the time in s
	for new code, use get_ticks_raw() and one of the macros defined in
	timer_ticks.h (.e.g TICKS_TO_S(tick) to convert to s or ms )*/
#define TIMER_TICK 1 /* 1 s, kept for compatibility */

/*function prototype to execute on mili-second based basic timers */
typedef void (utimer_function)(unsigned int uticks, void* param);

struct timer_ln; /* forward decl */
/* new 
 * params:
 *         - handle pointer to the corresponding struct timer_ln
 * return: 0 if the timer is one shot, new expire interval if not, -1
 *         if periodic
 * e.g.:   - a periodic timer would return: (ticks_t)(-1) or
 *            ((struct timer_ln*)handle)->initial_timeout
 *         - a timer which wants to expire again in x ms would return:
 *             (x * TICKS_HZ + 999)/1000
 */
typedef ticks_t (timer_handler_f)(ticks_t t, struct timer_ln* tl,
									void* data);


/* timer flags */
#define F_TIMER_FAST	1
#define F_TIMER_ON_SLOW_LIST	0x100
#define F_TIMER_ACTIVE	0x200 /* timer is running or has run and expired
								 (one shot) */
#ifdef TIMER_DEBUG
#define F_TIMER_DELETED	0x400 
#endif

struct timer_ln{ /* timer_link already used in tm */
	struct timer_ln* next;
	struct timer_ln* prev;
	ticks_t expire; 
	ticks_t initial_timeout;
	void* data;
	timer_handler_f* f; 
	volatile unsigned short flags;
#ifdef USE_SLOW_TIMER
	volatile slow_idx_t slow_idx;
#else
	unsigned short reserved;
#endif
#ifdef TIMER_DEBUG
	unsigned int expires_no; /* timer handler calls */
	const char* add_file;
	const char* add_func;
	unsigned add_line;
	unsigned add_calls;
	const char* del_file;
	const char* del_func;
	unsigned del_line;
	unsigned int del_calls;
	unsigned int init; /* how many times was init/re-init */
#endif
};



void timer_main(void); /* timer main loop, never exists */


int init_timer(void);
int arm_timer(void);
void destroy_timer(void);

#ifdef USE_SLOW_TIMER
int arm_slow_timer(void);
void slow_timer_main(void);
#endif


struct timer_ln* timer_alloc(void);
void timer_free(struct timer_ln* t);

#ifdef TIMER_DEBUG
/* use for a deleted/expired timer that you want to add again */
#define timer_reinit(tl) \
	do{ \
		(tl)->flags&=~((unsigned short)(F_TIMER_ON_SLOW_LIST | \
											  F_TIMER_ACTIVE));\
		(tl)->init++; \
	}while(0)
#else
/* use for a deleted/expired timer that you want to add again */
#define timer_reinit(tl) \
	(tl)->flags&=~((unsigned short)(F_TIMER_ON_SLOW_LIST | \
										  F_TIMER_ACTIVE))
#endif

#define timer_init(tl, fun, param, flgs) \
	do{ \
		memset((tl), 0, sizeof(struct timer_ln)); \
		(tl)->f=(fun); \
		(tl)->data=(param); \
		(tl)->flags=(flgs); \
		timer_reinit(tl); \
	}while(0)

#ifdef TIMER_DEBUG
int timer_add_safe(struct timer_ln *tl, ticks_t delta, 
					const char*, const char*, unsigned);
int timer_del_safe(struct timer_ln *tl,
					const char*, const char*, unsigned);
#define timer_add(tl, d) \
	timer_add_safe((tl), (d), __FILE__, __FUNCTION__, __LINE__)
#define timer_del(tl) \
	timer_del_safe((tl), __FILE__, __FUNCTION__, __LINE__)
#else
int timer_add_safe(struct timer_ln *tl, ticks_t delta);
int timer_del_safe(struct timer_ln *tl);
#define timer_add timer_add_safe
#define timer_del timer_del_safe
#endif

void timer_allow_del(void);

/* old timer compatibility functions & structure */

struct sr_timer{
	struct timer_ln tl;
	int id;
	timer_function* timer_f;
	void* t_param;
};


/*register a periodic timer;
 * ret: <0 on error*/
int register_timer(timer_function f, void* param, unsigned int interval);
ticks_t get_ticks(void);
ticks_t get_ticks_raw(void);

#endif

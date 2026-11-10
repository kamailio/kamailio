/*
 * timer related functions (public interface)
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
						*  marked "slow" */
/*#define TIMER_DEBUG -- compile with -DTIMER_DEBUG*/

#include "clist.h"
#include "dprint.h"
#include "timer_ticks.h"

#include <signal.h>
#include <sys/time.h>

#ifdef USE_SLOW_TIMER
#include <sys/types.h>

typedef unsigned int slow_idx_t; /* type for the slow index */
#endif


/* deprecated, old, kept for compatibility */
typedef void(timer_function)(unsigned int ticks, void *param);
typedef void(timer_function_w)(unsigned int ticks, int worker, void *param);
/* deprecated, old, kept for compatibility
	get_ticks()*TIMER_TICK used to be the time in s
	for new code, use get_ticks_raw() and one of the macros defined in
	timer_ticks.h (.e.g TICKS_TO_S(tick) to convert to s or ms )*/
#define TIMER_TICK 1 /* 1 s, kept for compatibility */

/*function prototype to execute on millisecond based basic timers */
typedef void(utimer_function)(unsigned int uticks, void *param);
typedef void(utimer_function_w)(unsigned int uticks, int worker, void *param);

struct timer_ln; /* forward decl */
struct timer_head;
struct timer_lists;
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
typedef ticks_t(timer_handler_f)(ticks_t t, struct timer_ln *tl, void *data);


/* timer flags */
#define F_TIMER_FAST 1
#define F_TIMER_ON_SLOW_LIST 0x100
#define F_TIMER_ACTIVE \
	0x200 /* timer is running or has run and expired
								 * (one shot) */
#ifdef TIMER_DEBUG
#define F_TIMER_DELETED 0x400
#endif

struct timer_ln
{ /* timer_link already used in tm */
	struct timer_ln *next;
	struct timer_ln *prev;
	ticks_t expire;
	ticks_t initial_timeout;
	void *data;
	timer_handler_f *f;
	volatile unsigned int flags;
#ifdef USE_SLOW_TIMER
	volatile slow_idx_t slow_idx;
#endif
#ifdef TIMER_DEBUG
	unsigned int expires_no; /* timer handler calls */
	const char *add_file;
	const char *add_func;
	unsigned add_line;
	unsigned add_calls;
	const char *del_file;
	const char *del_func;
	unsigned del_line;
	unsigned int del_calls;
	unsigned int init; /* how many times was init/re-init */
#endif
};


/** State belonging to one fast/slow core timer instance.
 *
 * An instance must be initialized before the processes using it are forked.
 * Since ITIMER_REAL and SIGALRM are process-wide, only one instance can be
 * armed in a process at a time.
 */
typedef struct timer_ctx
{
	ticks_t *ticks;
	ticks_t last_ticks;
	ticks_t last_adj_check;
	ticks_t prev_ticks;
	struct timeval last_time;
	struct timeval start_time;
	volatile sig_atomic_t run_timer;
	int timer_id;
	void *timer_lock;
	struct timer_ln *volatile *running_timer;
	int in_timer;
	struct timer_lists *timer_lst;
#ifdef USE_SLOW_TIMER
	void *slow_timer_lock;
	struct timer_head *slow_timer_lists;
	volatile unsigned short *t_idx;
	volatile unsigned short *s_idx;
	struct timer_ln *volatile *running_timer2;
	sigset_t slow_timer_sset;
	pid_t slow_timer_pid;
	int in_slow_timer;
#endif
} ksr_timer_ctx_t;

extern ksr_timer_ctx_t ksr_timer_ctx_core;


void timer_main_ctx(ksr_timer_ctx_t *td); /* timer main loop, never exits */

int init_timer_ctx(ksr_timer_ctx_t *td);
int arm_timer_ctx(ksr_timer_ctx_t *td);
void destroy_timer_ctx(ksr_timer_ctx_t *td);

#ifdef USE_SLOW_TIMER
int arm_slow_timer_ctx(ksr_timer_ctx_t *td);
void slow_timer_main_ctx(ksr_timer_ctx_t *td);
#endif


#define timer_main() timer_main_ctx(&ksr_timer_ctx_core)
#define init_timer() init_timer_ctx(&ksr_timer_ctx_core)
#define arm_timer() arm_timer_ctx(&ksr_timer_ctx_core)
#define destroy_timer() destroy_timer_ctx(&ksr_timer_ctx_core)

#ifdef USE_SLOW_TIMER
#define arm_slow_timer() arm_slow_timer_ctx(&ksr_timer_ctx_core)
#define slow_timer_main() slow_timer_main_ctx(&ksr_timer_ctx_core)
#endif

struct timer_ln *timer_alloc(void);
void timer_free(struct timer_ln *t);

#ifdef TIMER_DEBUG
/* use for a deleted/expired timer that you want to add again */
#define timer_reinit(tl)                                         \
	do {                                                         \
		(tl)->flags &= ~(F_TIMER_ON_SLOW_LIST | F_TIMER_ACTIVE); \
		(tl)->init++;                                            \
	} while(0)
#else
/* use for a deleted/expired timer that you want to add again */
#define timer_reinit(tl) (tl)->flags &= ~(F_TIMER_ON_SLOW_LIST | F_TIMER_ACTIVE)
#endif

#define timer_init(tl, fun, param, flgs)          \
	do {                                          \
		memset((tl), 0, sizeof(struct timer_ln)); \
		(tl)->f = (fun);                          \
		(tl)->data = (param);                     \
		(tl)->flags = (flgs);                     \
		timer_reinit(tl);                         \
	} while(0)

#ifdef TIMER_DEBUG
int timer_add_safe_ctx(ksr_timer_ctx_t *td, struct timer_ln *tl, ticks_t delta,
		const char *, const char *, unsigned);
int timer_del_safe_ctx(ksr_timer_ctx_t *td, struct timer_ln *tl, const char *,
		const char *, unsigned);
#define timer_add_safe(tl, d, file, func, line) \
	timer_add_safe_ctx(&ksr_timer_ctx_core, (tl), (d), (file), (func), (line))
#define timer_del_safe(tl, file, func, line) \
	timer_del_safe_ctx(&ksr_timer_ctx_core, (tl), (file), (func), (line))
#define timer_add_ctx(td, tl, d) \
	timer_add_safe_ctx((td), (tl), (d), __FILE__, __FUNCTION__, __LINE__)
#define timer_del_ctx(td, tl) \
	timer_del_safe_ctx((td), (tl), __FILE__, __FUNCTION__, __LINE__)
#define timer_add(tl, d) \
	timer_add_safe_ctx(  \
			&ksr_timer_ctx_core, (tl), (d), __FILE__, __FUNCTION__, __LINE__)
#define timer_del(tl)   \
	timer_del_safe_ctx( \
			&ksr_timer_ctx_core, (tl), __FILE__, __FUNCTION__, __LINE__)
#else
int timer_add_safe_ctx(ksr_timer_ctx_t *td, struct timer_ln *tl, ticks_t delta);
int timer_del_safe_ctx(ksr_timer_ctx_t *td, struct timer_ln *tl);
#define timer_add_safe(tl, d) timer_add_safe_ctx(&ksr_timer_ctx_core, (tl), (d))
#define timer_del_safe(tl) timer_del_safe_ctx(&ksr_timer_ctx_core, (tl))
#define timer_add_ctx(td, tl, d) timer_add_safe_ctx((td), (tl), (d))
#define timer_del_ctx(td, tl) timer_del_safe_ctx((td), (tl))
#define timer_add(tl, d) timer_add_safe_ctx(&ksr_timer_ctx_core, (tl), (d))
#define timer_del(tl) timer_del_safe_ctx(&ksr_timer_ctx_core, (tl))
#endif

void timer_allow_del_ctx(ksr_timer_ctx_t *td);
#define timer_allow_del() timer_allow_del_ctx(&ksr_timer_ctx_core)

/* old timer compatibility functions & structure */

struct sr_timer
{
	struct timer_ln tl;
	ksr_timer_ctx_t *timer_data;
	int id;
	timer_function *timer_f;
	void *t_param;
};


/*register a periodic timer;
 * ret: <0 on error*/
int register_timer_ctx(ksr_timer_ctx_t *td, timer_function f, void *param,
		unsigned int interval);
ticks_t get_ticks_ctx(ksr_timer_ctx_t *td);
ticks_t get_ticks_raw_ctx(ksr_timer_ctx_t *td);

#define register_timer(f, param, interval) \
	register_timer_ctx(&ksr_timer_ctx_core, (f), (param), (interval))
#define get_ticks() get_ticks_ctx(&ksr_timer_ctx_core)
#define get_ticks_raw() get_ticks_raw_ctx(&ksr_timer_ctx_core)

#endif

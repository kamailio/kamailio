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

/**
 * @file
 * @brief Kamailio core :: Timer
 * @ingroup core
 * Module: @ref core
 */

#include "timer.h"
#include "timer_funcs.h"
#include "timer_ticks.h"
#include "dprint.h"
#include <time.h>     /* gettimeofday */
#include <sys/time.h> /* setitimer, gettimeofday */
#include <signal.h>   /* SIGALRM */
#include <errno.h>
#include <unistd.h> /* pause() */
#include <stdlib.h> /* random, debugging only */
#include "error.h"
#include "signals.h"
/*
#include "config.h"
*/
#include "globals.h"
#include "mem/mem.h"
#ifdef SHM_MEM
#include "mem/shm_mem.h"
#endif
#include "locking.h"
#include "sched_yield.h"
#include "cfg/cfg_struct.h"


/* how often will the timer handler be called (in ticks) */
#define TIMER_HANDLER_INTERVAL	1U
/* how often to try to re-adjust the ticks */
#define TIMER_RESYNC_TICKS	(TIMER_TICKS_HZ*5U)  /* each 5 s */
#define TIMER_MAX_DRIFT	(TIMER_TICKS_HZ/10U) /* if drift > 0.1s adjust */



static ticks_t* ticks=0;
static ticks_t last_ticks; /* last time we adjusted the time */
static ticks_t last_adj_check; /* last time we ran adjust_ticks */
static ticks_t prev_ticks; /* last time we ran the timer, also used as
							  "current" ticks when running the timer for
							  "skipped" ticks */

static struct timeval last_time;
static struct timeval start_time; /* for debugging */

static volatile int run_timer=0;
static int timer_id=0;

static gen_lock_t* timer_lock=0;
static struct timer_ln* volatile* running_timer=0;/* running timer handler */
static int in_timer=0;

#define IS_IN_TIMER() (in_timer)

#define LOCK_TIMER_LIST()		lock_get(timer_lock)
#define UNLOCK_TIMER_LIST()		lock_release(timer_lock)

/* we can get away without atomic_set/atomic_cmp and write barriers because we
 * always call SET_RUNNING and IS_RUNNING while holding the timer lock
 * => it's implicitly atomic and the lock acts as write barrier */
#define SET_RUNNING(t)		(*running_timer=(t))
#define IS_RUNNING(t)		(*running_timer==(t))
#define UNSET_RUNNING()		(*running_timer=0)

#ifdef USE_SLOW_TIMER

#define SLOW_TIMER_SIG	SIGUSR2
/* timer flags checks */
#define IS_FAST_TIMER(t)	(t->flags&F_TIMER_FAST)
#define SET_SLOW_LIST(t)	(t->flags|=F_TIMER_ON_SLOW_LIST)
#define RESET_SLOW_LIST(t)	(t->flags&=~F_TIMER_ON_SLOW_LIST)
#define IS_ON_SLOW_LIST(t)	(t->flags&F_TIMER_ON_SLOW_LIST)

#define SLOW_LISTS_NO	1024U  /* slow lists number, 2^k recommended */


static gen_lock_t*  slow_timer_lock; /* slow timer lock */
static struct timer_head* slow_timer_lists; 
static volatile unsigned short* t_idx; /* "main" timer index in slow_lists[] */
static volatile unsigned short* s_idx; /* "slow" timer index in slow_lists[] */
static struct timer_ln* volatile* running_timer2=0; /* timer handler running
													     in the "slow" timer */
static sigset_t slow_timer_sset;
pid_t slow_timer_pid;
static int in_slow_timer=0;

#define IS_IN_TIMER_SLOW() (in_slow_timer)
#define SET_RUNNING_SLOW(t)		(*running_timer2=(t))
#define IS_RUNNING_SLOW(t)		(*running_timer2==(t))
#define UNSET_RUNNING_SLOW()	(*running_timer2=0)

#define LOCK_SLOW_TIMER_LIST()		lock_get(slow_timer_lock)
#define UNLOCK_SLOW_TIMER_LIST()	lock_release(slow_timer_lock)


#endif


struct timer_lists* timer_lst=0;

void sig_timer(int signo)
{
	(*ticks)++;
	if (( *ticks % TIMER_HANDLER_INTERVAL)==0){
		/* set a flag to run the handler */
		run_timer=1;
	}
}



void destroy_timer()
{
	struct itimerval it;
	
	/* disable timer */
	memset(&it, 0, sizeof(it));
	setitimer(ITIMER_REAL, &it, 0); 
	set_sig_h(SIGALRM, SIG_IGN);
	if (timer_lock){
		lock_destroy(timer_lock);
		lock_dealloc(timer_lock);
		timer_lock=0;
	}
	if (ticks){
#ifdef SHM_MEM
		shm_free(ticks);
#else
		pkg_free(ticks);
#endif
		ticks=0;
	}
	if (timer_lst){
#ifdef SHM_MEM
		shm_free(timer_lst);
#else
		pkg_free(timer_lst);
#endif
		timer_lst=0;
	}
	if (running_timer){
		shm_free((void*)running_timer);
		running_timer=0;
	}
#ifdef USE_SLOW_TIMER
	if (slow_timer_lock){
		lock_destroy(slow_timer_lock);
		lock_dealloc(slow_timer_lock);
		slow_timer_lock=0;
	}
	if (slow_timer_lists){
		shm_free((void*)slow_timer_lists);
		slow_timer_lists=0;
	}
	if (t_idx){
		shm_free((void*)t_idx);
		t_idx=0;
	}
	if (s_idx){
		shm_free((void*)s_idx);
		s_idx=0;
	}
	if(running_timer2){
		shm_free((void*)running_timer2);
		running_timer2=0;
	}
#endif
}



/* ret 0 on success, <0 on error*/
int init_timer()
{
	int r;
	int ret;
	
	ret=-1;
	
	/* init the locks */
	timer_lock=lock_alloc();
	if (timer_lock==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
	if (lock_init(timer_lock)==0){
		lock_dealloc(timer_lock);
		timer_lock=0;
		ret=-1;
		goto error;
	}
	/* init the shared structs */
#ifdef SHM_MEM
	ticks=shm_malloc(sizeof(ticks_t));
	timer_lst=shm_malloc(sizeof(struct timer_lists));
#else
	/* in this case get_ticks won't work! */
	LM_WARN("no shared memory support compiled in get_ticks won't work\n");
	ticks=pkg_malloc(sizeof(ticks_t));
	timer_lst=pkg_malloc(sizeof(struct timer_lists));
#endif
	if (ticks==0){
		LM_CRIT("out of shared memory (ticks)\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}
	if (timer_lst==0){
		LM_CRIT("out of shared memory (timer_lst)\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}
	running_timer=shm_malloc(sizeof(struct timer_ln*));
	if (running_timer==0){
		LM_CRIT("out of memory (running_timer)\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}

	/* initial values */
	memset(timer_lst, 0, sizeof(struct timer_lists));
	*ticks=random(); /* random value for start, for debugging */
	prev_ticks=last_ticks=last_adj_check=*ticks;
	*running_timer=0;
	if (gettimeofday(&start_time, 0)<0){
		LM_ERR("gettimeofday failed: %s [%d]\n", strerror(errno), errno);
		ret=-1;
		goto error;
	}
	last_time=start_time;
	LM_DBG("starting with *ticks=%u\n", (unsigned) *ticks);
	
	/* init timer structures */
	for (r=0; r<H0_ENTRIES; r++)
		_timer_init_list(&timer_lst->h0[r]);
	for (r=0; r<H1_ENTRIES; r++)
		_timer_init_list(&timer_lst->h1[r]);
	for (r=0; r<H2_ENTRIES; r++)
		_timer_init_list(&timer_lst->h2[r]);
	_timer_init_list(&timer_lst->expired);
	
#ifdef USE_SLOW_TIMER
	
	/* init the locks */
	slow_timer_lock=lock_alloc();
	if (slow_timer_lock==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
	if (lock_init(slow_timer_lock)==0){
		lock_dealloc(slow_timer_lock);
		slow_timer_lock=0;
		ret=-1;
		goto error;
	}
	t_idx=shm_malloc(sizeof(*t_idx));
	s_idx=shm_malloc(sizeof(*s_idx));
	slow_timer_lists=shm_malloc(sizeof(struct timer_head)*SLOW_LISTS_NO);
	running_timer2=shm_malloc(sizeof(struct timer_ln*));
	if ((t_idx==0)||(s_idx==0) || (slow_timer_lists==0) ||(running_timer2==0)){
		LM_ERR("out of shared memory (slow)\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}
	*t_idx=*s_idx=0;
	*running_timer2=0;
	for (r=0; r<SLOW_LISTS_NO; r++)
		_timer_init_list(&slow_timer_lists[r]);
	
#endif
	
	LM_DBG("timer_list between %p and %p\n",
			&timer_lst->h0[0], &timer_lst->h2[H2_ENTRIES]);
	return 0;
error:
	destroy_timer();
	return ret;
}



#ifdef USE_SLOW_TIMER
/* arm the "slow" timer ( start it) 
 * returns -1 on error
 * WARNING: use it in the same process as the timer
 *  (the one using pause(); timer_handler()) or
 *  change run_timer to a pointer in shared mem */
int arm_slow_timer()
{
	sigemptyset(&slow_timer_sset);
	sigaddset(&slow_timer_sset, SLOW_TIMER_SIG);
again:
	if (sigprocmask(SIG_BLOCK, &slow_timer_sset, 0)==-1){
		if (errno==EINTR) goto again;
		LM_ERR("sigprocmask failed: %s [%d]}n", strerror(errno), errno);
		goto error;
	}
#ifdef __OS_darwin
	/* workaround for darwin sigwait bug, see slow_timer_main() for more
	   info (or grep __OS_darwin) */
	/* keep in sync wih main.c: sig_usr() - signals we are interested in */
	sigaddset(&slow_timer_sset, SIGINT);
	sigaddset(&slow_timer_sset, SIGTERM);
	sigaddset(&slow_timer_sset, SIGUSR1);
	sigaddset(&slow_timer_sset, SIGHUP);
	sigaddset(&slow_timer_sset, SIGCHLD);
	sigaddset(&slow_timer_sset, SIGALRM);
#endif
	/* initialize the config framework */
	if (cfg_child_init()) goto error;

	return 0;
error:
	return -1;
}
#endif




/* arm the timer ( start it) 
 * returns -1 on error
 * WARNING: use it in the same process as the timer
 *  (the one using pause(); timer_handler()) or
 *  change run_timer to a pointer in shared mem */
int arm_timer()
{
	struct itimerval it;
	/* init signal generation */
	it.it_interval.tv_sec=0;
	it.it_interval.tv_usec=1000000/TIMER_TICKS_HZ;
	it.it_value=it.it_interval;
	/* install the signal handler */
	if (set_sig_h(SIGALRM, sig_timer) == SIG_ERR ){
		LM_CRIT("SIGALRM signal handler cannot be installed: %s [%d]\n",
					strerror(errno), errno);
		return -1;
	}
	if (setitimer(ITIMER_REAL, &it, 0) == -1){
		LM_CRIT("setitimer failed: %s [%d]\n", strerror(errno), errno);
		return -1;
	}
	if (gettimeofday(&last_time, 0)<0){
		LM_ERR("gettimeofday failed: %s [%d]\n", strerror(errno), errno);
		return -1;
	}
	/* initialize the config framework */
	if (cfg_child_init()) return -1;

	return 0;
}



#ifdef DBG_ser_time
/* debugging  only */
void check_ser_drift();
#endif /* DBG_set_time */



/* adjust the timer using the "real" time, each TIMER_RESYNC_TICKS, but only
 * if timer drift > TIMER_MAX_DRIFT
 * NOTES: - it will adjust time within  TIMER_MAX_DRIFT from the "real"
 *          elapsed time
 *        - it will never decrease the *ticks, only increase it (monotonic)
 *        - it works ok as long as the adjustment interval < MAX_TICKS_T
 * -- andrei
 */
inline static void adjust_ticks(void)
{
	struct timeval crt_time;
	long long diff_time;
	ticks_t diff_time_ticks;
	ticks_t diff_ticks_raw;
	s_ticks_t delta;
	
	/* fix ticks if necessary */
	if ((*ticks-last_adj_check)>=(ticks_t)TIMER_RESYNC_TICKS){
#ifdef DBG_ser_time
		check_ser_drift();
#endif /* DBG_ser_time */
		last_adj_check=*ticks;
		if (gettimeofday(&crt_time, 0)<0){
			LM_ERR("gettimeofday failed: %s [%d]\n", strerror(errno), errno);
			return; /* ignore */
		}
		diff_time=(long long)crt_time.tv_sec*1000000+crt_time.tv_usec-
					((long long) last_time.tv_sec*1000000+last_time.tv_usec);
		if (diff_time<0){
			LM_WARN("time changed backwards %ld ms ignoring...\n",
						(long)(diff_time/1000));
			last_time=crt_time;
			last_ticks=*ticks;
		}else{
			diff_ticks_raw=*ticks-last_ticks;
			diff_time_ticks=(ticks_t)((diff_time*TIMER_TICKS_HZ)/1000000LL);
			delta=(s_ticks_t)(diff_time_ticks-diff_ticks_raw);
			if (delta<-1){
				LM_WARN("our timer runs faster then real-time"
						" (%lu ms / %u ticks our time .->"
						 " %lu ms / %u ticks real time)\n", 
						(unsigned long)(diff_ticks_raw*1000L/TIMER_TICKS_HZ),
						diff_ticks_raw,
						(unsigned long)(diff_time/1000), diff_time_ticks);
				last_time=crt_time;
				last_ticks=*ticks;
			}else{
				/* fix the ticks */
				if (delta>(s_ticks_t)TIMER_MAX_DRIFT){
#ifndef TIMER_DEBUG
					if (delta > 2*(s_ticks_t)TIMER_MAX_DRIFT+1)
#endif
						LM_DBG("adjusting timer ticks (%lu) with %ld ms"
								" (%ld ticks)\n",
								(unsigned long)*ticks,
							(long)(delta*1000)/TIMER_TICKS_HZ, (long)delta);
					*ticks+=(ticks_t)delta;
				}else{
					/*LM_DBG("incredible, but our timer is in sync with"
							" real time (%lu)\n", (unsigned long)*ticks);
					*/
				}
			}
		}
	}
}



/* time(2) equivalent, using ser internal timers (faster then a syscall) */
time_t ser_time(time_t *t)
{
	if (likely(t==0))
		return last_time.tv_sec+TICKS_TO_S(*ticks-last_ticks);
	*t=last_time.tv_sec+TICKS_TO_S(*ticks-last_ticks);
	return *t;
}



/* gettimeofday(2) equivalent, using ser internal timers (faster 
 * but more imprecise)
 * WARNING: ignores tz (it's obsolete anyway)*/
int ser_gettimeofday(struct timeval* tv, struct timezone* tz)
{
	if (likely(tv!=0)){
		tv->tv_sec=last_time.tv_sec+TICKS_TO_S(*ticks-last_ticks);
		tv->tv_usec=last_time.tv_usec+
					(TICKS_TO_MS(*ticks-last_ticks)%1000)*1000;
	}
	return 0;
}



#ifdef DBG_ser_time
/* debugging  only, remove */
void check_ser_drift()
{
	time_t t1, t2;
	struct timeval tv1, tv2;
	int r;
	
	t1=time(0);
	t2=ser_time(0);
	if (t1!=t2)
		BUG("time(0)!=ser_time(0) : %d != %d \n", (unsigned)t1, (unsigned)t2);
	
	r=gettimeofday(&tv1, 0);
	ser_gettimeofday(&tv2, 0);
	if (tv1.tv_sec!=tv2.tv_sec)
		BUG("gettimeofday seconds!=ser_gettimeofday seconds : %d != %d \n",
				(unsigned)tv1.tv_sec, (unsigned)tv2.tv_sec);
	else if ((tv1.tv_usec > tv2.tv_usec) && 
				(unsigned)(tv1.tv_usec-tv2.tv_usec)>100000)
		BUG("gettimeofday usecs > ser_gettimeofday with > 0.1s : %d ms\n",
			(unsigned)(tv1.tv_usec-tv2.tv_usec)/1000);
	else if ((tv1.tv_usec < tv2.tv_usec) && 
				(unsigned)(tv2.tv_usec-tv1.tv_usec)>100000)
		BUG("gettimeofday usecs < ser_gettimeofday with > 0.1s : %d ms\n",
			(unsigned)(tv2.tv_usec-tv1.tv_usec)/1000);
}
#endif /* DBG_ser_time */



struct timer_ln* timer_alloc()
{
	return shm_malloc(sizeof(struct timer_ln));
}

void timer_free(struct timer_ln* t)
{
	shm_free(t);
}


/* unsafe (no lock ) timer add function
 * t = current ticks
 * tl must be filled (the intial_timeout and flags must be set)
 * returns -1 on error, 0 on success */
static inline int _timer_add(ticks_t t, struct timer_ln* tl)
{
	ticks_t delta;

#ifdef USE_SLOW_TIMER
	tl->flags&=~((unsigned short)F_TIMER_ON_SLOW_LIST);
	tl->slow_idx=0;
#endif
	delta=tl->initial_timeout;
	tl->expire=t+delta;
	return _timer_dist_tl(tl, delta);
}



/* "public", safe timer add functions
 * adds a timer at delta ticks from the current time
 * returns -1 on error, 0 on success
 * WARNING: to re-add an expired or deleted timer you must call
 *          timer_reinit(tl) prior to timer_add
 *          The default behaviour allows timer_add to add a timer only if it
 *          has never been added before.
 */
#ifdef TIMER_DEBUG
int timer_add_safe(struct timer_ln* tl, ticks_t delta,
					const char* file, const char* func, unsigned line)
#else
int timer_add_safe(struct timer_ln* tl, ticks_t delta)
#endif
{
	int ret;
	
	LOCK_TIMER_LIST();
	if (tl->flags & F_TIMER_ACTIVE){
#ifdef TIMER_DEBUG
		LOG(timerlog, "timer_add called on an active timer %p (%p, %p),"
					" flags %x\n", tl, tl->next, tl->prev, tl->flags);
		LOG(timerlog, "WARN: -timer_add-; called from %s(%s):%d\n",
					func, file, line);
		LOG(timerlog, "WARN: -timer_add-: added %d times"
					", last from: %s(%s):%d, deleted %d times"
					", last from: %s(%s):%d, init %d times, expired %d \n",
					tl->add_calls, tl->add_func, tl->add_file, tl->add_line,
					tl->del_calls, tl->del_func, tl->del_file, tl->del_line,
					tl->init, tl->expires_no);
#else
		LM_DBG("timer_add called on an active timer %p (%p, %p),"
					" flags %x\n", tl, tl->next, tl->prev, tl->flags);
#endif
		ret=-1; /* refusing to add active or non-reinit. timer */
		goto error;
	}
	tl->initial_timeout=delta;
	if ((tl->next!=0) || (tl->prev!=0)){
		LM_CRIT("timer_add: called with linked timer: %p (%p, %p)\n",
				tl, tl->next, tl->prev);
		ret=-1;
		goto error;
	}
	tl->flags|=F_TIMER_ACTIVE;
#ifdef TIMER_DEBUG
	tl->add_file=file;
	tl->add_func=func;
	tl->add_line=line;
	tl->add_calls++;
#endif
	ret=_timer_add(*ticks, tl);
error:
	UNLOCK_TIMER_LIST();
	return ret;
}



/* safe timer delete
 * deletes tl and inits the list pointer to 0
 * returns  <0 on error (-1 if timer not active/already deleted and -2 if 
 *           delete attempted from the timer handler) and 0 on success
 */
#ifdef TIMER_DEBUG
int timer_del_safe(struct timer_ln* tl,
					const char* file, const char* func, unsigned line)
#else
int timer_del_safe(struct timer_ln* tl)
#endif
{
	int ret;
	
	ret=-1;
again:
	/* quick exit if timer inactive */
	if ( !(tl->flags & F_TIMER_ACTIVE)){
#ifdef TIMER_DEBUG
		LOG(timerlog, "timer_del called on an inactive timer %p (%p, %p),"
					" flags %x\n", tl, tl->next, tl->prev, tl->flags);
		LOG(timerlog, "WARN: -timer_del-; called from %s(%s):%d\n",
					func, file, line);
		LOG(timerlog, "WARN: -timer_del-: added %d times"
					", last from: %s(%s):%d, deleted %d times"
					", last from: %s(%s):%d, init %d times, expired %d \n",
					tl->add_calls, tl->add_func, tl->add_file, tl->add_line,
					tl->del_calls, tl->del_func, tl->del_file, tl->del_line,
					tl->init, tl->expires_no);
#else
/*
		LM_DBG("called on an inactive timer %p (%p, %p),"
					" flags %x\n", tl, tl->next, tl->prev, tl->flags);
*/
#endif
		return -1;
	}
#ifdef USE_SLOW_TIMER
		if (IS_ON_SLOW_LIST(tl) && (tl->slow_idx!=*t_idx)){
			LOCK_SLOW_TIMER_LIST();
			if (!IS_ON_SLOW_LIST(tl) || (tl->slow_idx==*t_idx)){
				UNLOCK_SLOW_TIMER_LIST();
				goto again;
			}
			if (IS_RUNNING_SLOW(tl)){
				UNLOCK_SLOW_TIMER_LIST();
				if (IS_IN_TIMER_SLOW()){
					/* if somebody tries to shoot himself in the foot,
					 * warn him and ignore the delete */
					LM_CRIT("timer handle %p (s) tried to delete"
							" itself\n", tl);
#ifdef TIMER_DEBUG
					LOG(timerlog, "WARN: -timer_del-: called from %s(%s):%d\n",
									func, file, line);
					LOG(timerlog, "WARN: -timer_del-: added %d times"
						", last from: %s(%s):%d, deleted %d times"
						", last from: %s(%s):%d, init %d times, expired %d \n",
						tl->add_calls, tl->add_func, tl->add_file,
						tl->add_line, tl->del_calls, tl->del_func, 
						tl->del_file, tl->del_line, tl->init, tl->expires_no);
#endif
					return -2; /* do nothing */
				}
				sched_yield(); /* wait for it to complete */
				goto again;
			}
			if (tl->next!=0){
				_timer_rm_list(tl); /* detach */
				tl->next=tl->prev=0;
				ret=0;
#ifdef TIMER_DEBUG
				tl->del_file=file;
				tl->del_func=func;
				tl->del_line=line;
				tl->flags|=F_TIMER_DELETED;
#endif
			}else{
#ifdef TIMER_DEBUG
				LOG(timerlog, "timer_del: (s) timer %p (%p, %p) flags %x "
							"already detached\n",
							tl, tl->next, tl->prev, tl->flags);
				LOG(timerlog, "WARN: -timer_del-: @%d tl=%p "
					"{ %p, %p, %d, %d, %p, %p, %04x, -}\n", get_ticks_raw(), 
					tl,  tl->next, tl->prev, tl->expire, tl->initial_timeout,
					tl->data, tl->f, tl->flags);
				LOG(timerlog, "WARN: -timer_del-; called from %s(%s):%d\n",
						func, file, line);
				LOG(timerlog, "WARN: -timer_del-: added %d times"
						", last from: %s(%s):%d, deleted %d times"
						", last from: %s(%s):%d, init %d times, expired %d \n",
						tl->add_calls,
						tl->add_func, tl->add_file, tl->add_line,
						tl->del_calls,
						tl->del_func, tl->del_file, tl->del_line,
						tl->init, tl->expires_no);
#else
/*
				LM_DBG("(s) timer %p (%p, %p) flags %x "
							"already detached\n",
							tl, tl->next, tl->prev, tl->flags);
*/
#endif
				ret=-1;
			}
			UNLOCK_SLOW_TIMER_LIST();
		}else{
#endif
			LOCK_TIMER_LIST();
#ifdef USE_SLOW_TIMER
			if (IS_ON_SLOW_LIST(tl) && (tl->slow_idx!=*t_idx)){
				UNLOCK_TIMER_LIST();
				goto again;
			}
#endif
			if (IS_RUNNING(tl)){
				UNLOCK_TIMER_LIST();
				if (IS_IN_TIMER()){
					/* if somebody tries to shoot himself in the foot,
					 * warn him and ignore the delete */
					LM_CRIT("timer handle %p tried to delete"
							" itself\n", tl);
#ifdef TIMER_DEBUG
					LOG(timerlog, "WARN: -timer_del-: called from %s(%s):%d\n",
									func, file, line);
					LOG(timerlog, "WARN: -timer_del-: added %d times"
						", last from: %s(%s):%d, deleted %d times"
						", last from: %s(%s):%d, init %d times, expired %d \n",
						tl->add_calls, tl->add_func, tl->add_file,
						tl->add_line, tl->del_calls, tl->del_func, 
						tl->del_file, tl->del_line, tl->init, tl->expires_no);
#endif
					return -2; /* do nothing */
				}
				sched_yield(); /* wait for it to complete */
				goto again;
			}
			if ((tl->next!=0)&&(tl->prev!=0)){
				_timer_rm_list(tl); /* detach */
				tl->next=tl->prev=0;
				ret=0;
#ifdef TIMER_DEBUG
				tl->del_file=file;
				tl->del_func=func;
				tl->del_line=line;
				tl->flags|=F_TIMER_DELETED;
#endif
			}else{
#ifdef TIMER_DEBUG
				LOG(timerlog, "timer_del: (f) timer %p (%p, %p) flags %x "
							"already detached\n",
							tl, tl->next, tl->prev, tl->flags);
				LOG(timerlog, "WARN: -timer_del-: @%d tl=%p "
					"{ %p, %p, %d, %d, %p, %p, %04x, -}\n", get_ticks_raw(), 
					tl,  tl->next, tl->prev, tl->expire, tl->initial_timeout,
					tl->data, tl->f, tl->flags);
				LOG(timerlog, "WARN: -timer_del-; called from %s(%s):%d\n",
						func, file, line);
				LOG(timerlog, "WARN: -timer_del-: added %d times"
						", last from: %s(%s):%d, deleted %d times"
						", last from: %s(%s):%d, init %d times, expired %d \n",
						tl->add_calls,
						tl->add_func, tl->add_file, tl->add_line,
						tl->del_calls,
						tl->del_func, tl->del_file, tl->del_line,
						tl->init, tl->expires_no);
#else
/*
				LM_DBG("(f) timer %p (%p, %p) flags %x "
							"already detached\n",
							tl, tl->next, tl->prev, tl->flags);
*/
#endif
				ret=-1;
			}
			UNLOCK_TIMER_LIST();
#ifdef USE_SLOW_TIMER
		}
#endif
return ret;
}



/* marks a timer as "to be deleted when the handler ends", usefull when
 * the timer handler knows it won't prolong the timer anymore (it will 
 * return 0) and will do some time consuming work. Calling this function
 * will cause simultaneous timer_dels to return immediately (they won't 
 * wait anymore for the timer handle to finish). It will also allow 
 * self-deleting from the timer handle without bug reports.
 * WARNING: - if you rely on timer_del to know when the timer handle execution
 *            finishes (e.g. to free resources used in the timer handle), don't
 *            use this function.
 *          - this function can be called only from a timer handle (in timer
 *            context), all other calls will have no effect and will log a
 *            bug message
 */
void timer_allow_del(void)
{
	if (IS_IN_TIMER() ){
			UNSET_RUNNING();
	}else
#ifdef USE_SLOW_TIMER
	if (IS_IN_TIMER_SLOW()){
			UNSET_RUNNING_SLOW();
	}else 
#endif
		LM_CRIT("timer_allow_del called outside a timer handle\n");
}


/* called from timer_handle, must be called with the timer lock held
 * WARNING: expired one shot timers are _not_ automatically reinit
 *          (because they could have been already freed from the timer
 *           handler so a reinit would not be safe!) */
inline static void timer_list_expire(ticks_t t, struct timer_head* h
#ifdef USE_SLOW_TIMER
										, struct timer_head* slow_l,
										slow_idx_t slow_mark
#endif
																	)
{
	struct timer_ln * tl;
	ticks_t ret;
#ifdef TIMER_DEBUG
	struct timer_ln* first;
	int i=0;
	
	first=h->next;
#endif
	
	/*LM_DBG("@ ticks = %lu, list =%p\n",
			(unsigned long) *ticks, h);
	*/
	while(h->next!=(struct timer_ln*)h){
		tl=h->next;
#ifdef TIMER_DEBUG /* FIXME: replace w/ EXTRA_DEBUG */
		if (tl==0){
			LM_CRIT("timer_list_expire: tl=%p, h=%p {%p, %p}\n",
					tl, h, h->next, h->prev);
			abort();
		}else if((tl->next==0) || (tl->prev==0)){
			LM_CRIT("timer_list_expire: @%d tl=%p "
					"{ %p, %p, %d, %d, %p, %p, %04x, -},"
					" h=%p {%p, %p}\n", t, 
					tl,  tl->next, tl->prev, tl->expire, tl->initial_timeout,
					tl->data, tl->f, tl->flags, 
					h, h->next, h->prev);
			LM_CRIT("-timer_list_expire-: cycle %d, first %p,"
						"running %p\n", i, first, *running_timer);
			LM_CRIT("-timer_list_expire-: added %d times"
						", last from: %s(%s):%d, deleted %d times"
						", last from: %s(%s):%d, init %d times, expired %d \n",
						tl->add_calls,
						tl->add_func, tl->add_file, tl->add_line,
						tl->del_calls,
						tl->del_func, tl->del_file, tl->del_line,
						tl->init, tl->expires_no);
			abort();
		}
		i++;
#endif
		_timer_rm_list(tl); /* detach */
#ifdef USE_SLOW_TIMER
		if (IS_FAST_TIMER(tl)){
#endif
		/* if fast timer */
			SET_RUNNING(tl);
			tl->next=tl->prev=0; /* debugging */
#ifdef TIMER_DEBUG
			tl->expires_no++;
#endif
			UNLOCK_TIMER_LIST(); /* acts also as write barrier */ 
				ret=tl->f(t, tl, tl->data);
				/* reset the configuration group handles */
				cfg_reset_all();
				if (ret==0){
					UNSET_RUNNING();
					LOCK_TIMER_LIST();
				}else{
					/* not one-shot, re-add it */
					LOCK_TIMER_LIST();
					if (ret!=(ticks_t)-1) /* ! periodic */
						tl->initial_timeout=ret;
					_timer_add(t, tl);
					UNSET_RUNNING();
				}
#ifdef USE_SLOW_TIMER
		}else{
			/* slow timer */
			SET_SLOW_LIST(tl);
			tl->slow_idx=slow_mark; /* current index */
			/* overflow check in timer_handler*/
			_timer_add_list(slow_l, tl);
			
		}
#endif
	}
}



/* "main" timer routine
 * WARNING: it should never be called twice for the same *ticks value
 * (it could cause too fast expires for long timers), *ticks must be also
 *  always increasing */
static void timer_handler(void)
{
	ticks_t saved_ticks;
#ifdef USE_SLOW_TIMER
	int run_slow_timer;
	int i;
	
	run_slow_timer=0;
	i=(slow_idx_t)(*t_idx%SLOW_LISTS_NO);
#endif
	
	/*LM_DBG("called, ticks=%lu, prev_ticks=%lu\n",
			(unsigned long)*ticks, (unsigned long)prev_ticks);
	*/
	run_timer=0; /* reset run_timer */
	adjust_ticks();
	LOCK_TIMER_LIST();
	do{
		saved_ticks=*ticks; /* protect against time running backwards */
		if (prev_ticks>=saved_ticks){
			LM_CRIT("backwards or still time\n");
			/* try to continue */
			prev_ticks=saved_ticks-1;
			break;
		}
		/* go through all the "missed" ticks, taking a possible overflow
		 * into account */
		for (prev_ticks=prev_ticks+1; prev_ticks!=saved_ticks; prev_ticks++) 
			timer_run(prev_ticks);
		timer_run(prev_ticks); /* do it for saved_ticks too */
	}while(saved_ticks!=*ticks); /* in case *ticks changed */
#ifdef USE_SLOW_TIMER
	timer_list_expire(*ticks, &timer_lst->expired, &slow_timer_lists[i],
						*t_idx);
#else
	timer_list_expire(*ticks, &timer_lst->expired);
#endif
	/* WARNING: add_timer(...,0) must go directly to expired list, since
	 * otherwise there is a race between timer running and adding it
	 * (it could expire it H0_ENTRIES ticks later instead of 'now')*/
#ifdef USE_SLOW_TIMER
	if (slow_timer_lists[i].next!=(struct timer_ln*)&slow_timer_lists[i]){
		run_slow_timer=1;
		if ((slow_idx_t)(*t_idx-*s_idx) < (SLOW_LISTS_NO-1U))
			(*t_idx)++;
		else{
			LM_WARN("slow timer too slow: overflow (%d - %d = %d)\n",
					*t_idx, *s_idx, *t_idx-*s_idx);
			/* trying to continue */
		}
	}
#endif
	UNLOCK_TIMER_LIST();
#ifdef USE_SLOW_TIMER
	/* wake up the "slow" timer */
	if (run_slow_timer)
		kill(slow_timer_pid, SLOW_TIMER_SIG);
#endif
}



/* main timer function, never exists */
void timer_main()
{
	in_timer=1; /* mark this process as the fast timer */
	while(1){
		if (run_timer){
			/* update the local cfg if needed */
			cfg_update();

			timer_handler();
		}
		pause();
	}
}



/* generic call back for the old style timer functions */
static ticks_t compat_old_handler(ticks_t ti, struct timer_ln* tl,
									void * data)
{
	struct sr_timer* t;
	
#ifdef TIMER_DEBUG
	LM_DBG("calling, ticks=%u/%u, tl=%p, t=%p\n",
			prev_ticks, (unsigned)*ticks, tl, data);
#endif
	t=(struct sr_timer*)data;
	t->timer_f(TICKS_TO_S(*ticks), t->t_param);
	return (ticks_t)-1; /* periodic */
}



/* register a periodic timer;
 * compatibility mode.w/ the old timer interface...
 * ret: <0 on error
 * Hint: if you need it in a module, register it from mod_init or it 
 * won't work otherwise*/
int register_timer(timer_function f, void* param, unsigned int interval)
{
	struct sr_timer* t;

	t=shm_malloc(sizeof(struct sr_timer));
	if (t==0){
		LM_ERR("out of memory\n");
		goto error;
	}
	t->id=timer_id++;
	t->timer_f=f;
	t->t_param=param;
	
	timer_init(&t->tl, compat_old_handler, t, 0); /* is slow */
	if (timer_add(&t->tl, S_TO_TICKS(interval))!=0){
		LM_ERR("timer_add failed\n");
		return -1;
	}
	
	return t->id;

error:
	return E_OUT_OF_MEM;
}



ticks_t get_ticks_raw()
{
#ifndef SHM_MEM
	LM_CRIT("no shared memory support compiled in"
			", returning 0 (probably wrong)");
	return 0;
#endif
	return *ticks;
}



/* returns tick in s (for compatibility with the old code) */
ticks_t get_ticks()
{
#ifndef SHM_MEM
	LM_CRIT("no shared memory support compiled in"
			", returning 0 (probably wrong)");
	return 0;
#endif
	return TICKS_TO_S(*ticks);
}


#ifdef USE_SLOW_TIMER


/* slow timer main function, never exists
 * This function is intended to be executed in a special separated process
 * (the "slow" timer) which will run the timer handlers of all the registered
 * timers not marked as "fast". The ideea is to execute the fast timers in the
 * "main" timer process, as accurate as possible and defer the execution of the
 * timers marked as "slow" to the "slow" timer.
 * Implementation details:
 *  - it waits for a signal and then wakes up and processes
 *    all the lists in slow_timer_lists from [s_idx, t_idx). It will
 *   -it  increments *s_idx (at the end it will be == *t_idx)
 *   -all list operations are protected by the "slow" timer lock
 */
#ifdef __OS_darwin
extern void sig_usr(int signo);
#endif

void slow_timer_main()
{
	int n;
	ticks_t ret;
	struct timer_ln* tl;
	unsigned short i;
#ifdef USE_SIGWAIT
	int sig;
#endif
	
	in_slow_timer=1; /* mark this process as the slow timer */
	while(1){
#ifdef USE_SIGWAIT
		n=sigwait(&slow_timer_sset, &sig);
#else
		n=sigwaitinfo(&slow_timer_sset, 0);
#endif
		if (n==-1){
			if (errno==EINTR) continue; /* some other signal, ignore it */
			LM_ERR("sigwaitinfo failed: %s [%d]\n", strerror(errno), errno);
			sleep(1);
			/* try to continue */
		}
#ifdef USE_SIGWAIT
	if (sig!=SLOW_TIMER_SIG){
#ifdef __OS_darwin
		/* on darwin sigwait is buggy: it will cause extreme slow down
		   on signal delivery for the signals it doesn't wait on
		   (on darwin 8.8.0, g4 1.5Ghz I've measured a 36s delay!).
		  To work arround this bug, we sigwait() on all the signals we
		  are interested in ser and manually call the master signal handler 
		  if the signal!= slow timer signal -- andrei */
		sig_usr(sig);
#endif
		continue;
	}
#endif
		/* update the local cfg if needed */
		cfg_update();
		
		LOCK_SLOW_TIMER_LIST();
		while(*s_idx!=*t_idx){
			i= *s_idx%SLOW_LISTS_NO;
			while(slow_timer_lists[i].next!=
					(struct timer_ln*)&slow_timer_lists[i]){
				tl=slow_timer_lists[i].next;
				_timer_rm_list(tl);
				tl->next=tl->prev=0;
#ifdef TIMER_DEBUG
				tl->expires_no++;
#endif
				SET_RUNNING_SLOW(tl);
				UNLOCK_SLOW_TIMER_LIST();
					ret=tl->f(*ticks, tl, tl->data);
					/* reset the configuration group handles */
					cfg_reset_all();
					if (ret==0){
						/* one shot */
						UNSET_RUNNING_SLOW();
						LOCK_SLOW_TIMER_LIST();
					}else{
						/* not one shot, re-add it */
						LOCK_TIMER_LIST(); /* add it to the "main"  list */
							RESET_SLOW_LIST(tl);
							if (ret!=(ticks_t)-1) /* != periodic */
								tl->initial_timeout=ret;
							_timer_add(*ticks, tl);
						UNLOCK_TIMER_LIST();
						LOCK_SLOW_TIMER_LIST();
						UNSET_RUNNING_SLOW();
					}
			}
			(*s_idx)++;
		}
		UNLOCK_SLOW_TIMER_LIST();
	}
	
}

#endif

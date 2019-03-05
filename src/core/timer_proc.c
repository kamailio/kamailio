/*
 * Copyright (C) 2009 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief Kamailio core ::  timer - separate process timers
 *
 *  (unrelated to the main fast and slow timers)
 *
 * @ingroup core
 * Module: @ref core
 */

#include "timer_proc.h"
#include "cfg/cfg_struct.h"
#include "pt.h"
#include "ut.h"
#include "mem/shm_mem.h"

#include <unistd.h>


/**
 * \brief update internal counters for running new basic sec. timers
 * @param timers number of basic timer processes
 * @return 0 on success; -1 on error
 */
int register_basic_timers(int timers)
{
	if(register_procs(timers)<0)
		return -1;
	cfg_register_child(timers);
	return 0;
}

/**
 * \brief Forks a separate simple sleep() periodic timer
 *
 * Forks a very basic periodic timer process, that just sleep()s for
 * the specified interval and then calls the timer function.
 * The new "basic timer" process execution start immediately, the sleep()
 * is called first (so the first call to the timer function will happen
 * \<interval\> seconds after the call to fork_basic_timer)
 * @param child_id  @see fork_process()
 * @param desc      @see fork_process()
 * @param make_sock @see fork_process()
 * @param f         timer function/callback
 * @param param     parameter passed to the timer function
 * @param interval  interval in seconds.
 * @return pid of the new process on success, -1 on error
 * (doesn't return anything in the child process)
 */
int fork_basic_timer(int child_id, char* desc, int make_sock,
						timer_function* f, void* param, int interval)
{
	int pid;

	pid=fork_process(child_id, desc, make_sock);
	if (pid<0) return -1;
	if (pid==0){
		/* child */
		if (cfg_child_init()) return -1;
		for(;;){
			sleep(interval);
			cfg_update();
			f(get_ticks(), param); /* ticks in s for compatibility with old
									* timers */
		}
	}
	/* parent */
	return pid;
}

int fork_basic_timer_w(int child_id, char* desc, int make_sock,
						timer_function_w* f, int worker, void* param, int interval)
{
	int pid;

	pid=fork_process(child_id, desc, make_sock);
	if (pid<0) return -1;
	if (pid==0){
		/* child */
		if (cfg_child_init()) return -1;
		for(;;){
			sleep(interval);
			cfg_update();
			f(get_ticks(), worker, param); /* ticks in s for compatibility with old
									* timers */
		}
	}
	/* parent */
	return pid;
}

/**
 * \brief Forks a separate simple microsecond-sleep() periodic timer
 *
 * Forks a very basic periodic timer process, that just us-sleep()s for
 * the specified interval and then calls the timer function.
 * The new "basic timer" process execution start immediately, the us-sleep()
 * is called first (so the first call to the timer function will happen
 * \<interval\> microseconds after the call to fork_basic_utimer)
 * @param child_id  @see fork_process()
 * @param desc      @see fork_process()
 * @param make_sock @see fork_process()
 * @param f         timer function/callback
 * @param param     parameter passed to the timer function
 * @param uinterval  interval in micro-seconds.
 * @return pid of the new process on success, -1 on error
 * (doesn't return anything in the child process)
 */
int fork_basic_utimer(int child_id, char* desc, int make_sock,
						utimer_function* f, void* param, int uinterval)
{
	int pid;
	ticks_t ts;

	pid=fork_process(child_id, desc, make_sock);
	if (pid<0) return -1;
	if (pid==0){
		/* child */
		if (cfg_child_init()) return -1;
		for(;;){
			sleep_us(uinterval);
			cfg_update();
			ts = get_ticks_raw();
			f(TICKS_TO_MS(ts), param); /* ticks in mili-seconds */
		}
	}
	/* parent */
	return pid;
}

int fork_basic_utimer_w(int child_id, char* desc, int make_sock,
						utimer_function_w* f, int worker, void* param, int uinterval)
{
	int pid;
	ticks_t ts;

	pid=fork_process(child_id, desc, make_sock);
	if (pid<0) return -1;
	if (pid==0){
		/* child */
		if (cfg_child_init()) return -1;
		for(;;){
			sleep_us(uinterval);
			cfg_update();
			ts = get_ticks_raw();
			f(TICKS_TO_MS(ts), worker, param); /* ticks in mili-seconds */
		}
	}
	/* parent */
	return pid;
}


/**
 * \brief Forks a timer process based on the local timer
 *
 * Forks a separate timer process running a local_timer.h type of timer
 * A pointer to the local_timer handle (allocated in shared memory) is
 * returned in lt_h. It can be used to add/delete more timers at runtime
 * (via local_timer_add()/local_timer_del() a.s.o).
 * If timers are added from separate processes, some form of locking must be
 * used (all the calls to local_timer* must be enclosed by locks if it
 * cannot be guaranteed that they cannot execute in the same time)
 * The timer "engine" must be run manually from the child process. For
 * example a very simple local timer process that just runs a single
 * periodic timer can be started in the following way:
 * struct local_timer* lt_h;
 *
 * pid=fork_local_timer_process(...., &lt_h);
 * if (pid==0){
 *          timer_init(&my_timer, my_timer_f, 0, 0);
 *          local_timer_add(&lt_h, &my_timer, S_TO_TICKS(10), get_ticks_raw());
 *          while(1) { sleep(1); local_timer_run(lt, get_ticks_raw()); }
 * }
 *
 * @param child_id  @see fork_process()
 * @param desc      @see fork_process()
 * @param make_sock @see fork_process()
 * @param lt_h      local_timer handler
 * @return pid to the parent, 0 to the child, -1 if error.
 */
int fork_local_timer_process(int child_id, char* desc, int make_sock,
						struct local_timer** lt_h)
{
	int pid;
	struct local_timer* lt;

	lt=shm_malloc(sizeof(*lt));
	if (lt==0) goto error;
	if (init_local_timer(lt, get_ticks_raw())<0) goto error;
	pid=fork_process(child_id, desc, make_sock);
	if (pid<0) goto error;
	*lt_h=lt;
	return pid;
error:
	if (lt) shm_free(lt);
	return -1;
}

/**
 * \brief update internal counters for running new sync sec. timers
 * @param timers number of basic timer processes
 * @return 0 on success; -1 on error
 */
int register_sync_timers(int timers)
{
	if(register_procs(timers)<0)
		return -1;
	cfg_register_child(timers);
	return 0;
}

/**
 * \brief Forks a separate simple sleep() -&- sync periodic timer
 *
 * Forks a very basic periodic timer process, that just sleep()s for
 * the specified interval and then calls the timer function.
 * The new "sync timer" process execution start immediately, the sleep()
 * is called first (so the first call to the timer function will happen
 * \<interval\> seconds after the call to fork_sync_timer)
 * @param child_id  @see fork_process()
 * @param desc      @see fork_process()
 * @param make_sock @see fork_process()
 * @param f         timer function/callback
 * @param param     parameter passed to the timer function
 * @param interval  interval in seconds.
 * @return pid of the new process on success, -1 on error
 * (doesn't return anything in the child process)
 */
int fork_sync_timer(int child_id, char* desc, int make_sock,
						timer_function* f, void* param, int interval)
{
	int pid;
	ticks_t ts1 = 0;
	ticks_t ts2 = 0;

	pid=fork_process(child_id, desc, make_sock);
	if (pid<0) return -1;
	if (pid==0){
		/* child */
		interval *= 1000;  /* miliseconds */
		ts2 = interval;
		if (cfg_child_init()) return -1;
		for(;;){
			if (ts2>interval)
				sleep_us(1000);    /* 1 milisecond sleep to catch up */
			else
				sleep_us(ts2*1000); /* microseconds sleep */
			ts1 = get_ticks_raw();
			cfg_update();
			f(TICKS_TO_S(ts1), param); /* ticks in sec for compatibility with old
										* timers */
			/* adjust the next sleep duration */
			ts2 = interval - TICKS_TO_MS(get_ticks_raw()) + TICKS_TO_MS(ts1);
		}
	}
	/* parent */
	return pid;
}


/**
 * \brief Forks a separate simple microsecond-sleep() -&- sync periodic timer
 *
 * Forks a very basic periodic timer process, that just us-sleep()s for
 * the specified interval and then calls the timer function.
 * The new "sync timer" process execution start immediately, the us-sleep()
 * is called first (so the first call to the timer function will happen
 * \<interval\> microseconds after the call to fork_basic_utimer)
 * @param child_id  @see fork_process()
 * @param desc      @see fork_process()
 * @param make_sock @see fork_process()
 * @param f         timer function/callback
 * @param param     parameter passed to the timer function
 * @param uinterval  interval in micro-seconds.
 * @return pid of the new process on success, -1 on error
 * (doesn't return anything in the child process)
 */
int fork_sync_utimer(int child_id, char* desc, int make_sock,
						utimer_function* f, void* param, int uinterval)
{
	int pid;
	ticks_t ts1 = 0;
	ticks_t ts2 = 0;

	pid=fork_process(child_id, desc, make_sock);
	if (pid<0) return -1;
	if (pid==0){
		/* child */
		ts2 = uinterval;
		if (cfg_child_init()) return -1;
		for(;;){
			if(ts2>uinterval)
				sleep_us(1);
			else
				sleep_us(ts2);
			ts1 = get_ticks_raw();
			cfg_update();
			f(TICKS_TO_MS(ts1), param); /* ticks in mili-seconds */
			ts2 = uinterval - get_ticks_raw() + ts1;
		}
	}
	/* parent */
	return pid;
}


/* number of slots in the wheel timer */
#define SR_WTIMER_SIZE	16

typedef struct sr_wtimer_node {
	struct sr_wtimer_node *next;
	uint32_t interval;  /* frequency of execution (secs) */
	uint32_t steps;     /* init: interval = loops * SR_WTIMER_SIZE + steps */
	uint32_t loops;
	uint32_t eloop;
	timer_function* f;
	void* param;
} sr_wtimer_node_t;

typedef struct sr_wtimer {
	uint32_t itimer;
	sr_wtimer_node_t *wlist[SR_WTIMER_SIZE];
} sr_wtimer_t;

static sr_wtimer_t *_sr_wtimer = NULL;;

/**
 *
 */
int sr_wtimer_init(void)
{
	if(_sr_wtimer!=NULL)
		return 0;
	_sr_wtimer = (sr_wtimer_t *)pkg_malloc(sizeof(sr_wtimer_t));
	if(_sr_wtimer==NULL) {
		PKG_MEM_ERROR;
		return -1;
	}

	memset(_sr_wtimer, 0, sizeof(sr_wtimer_t));
	register_sync_timers(1);
	return 0;
}

/**
 *
 */
int sr_wtimer_add(timer_function* f, void* param, int interval)
{
	sr_wtimer_node_t *wt;
	if(_sr_wtimer==NULL) {
		LM_ERR("wtimer not initialized\n");
		return -1;
	}

	wt = (sr_wtimer_node_t*)pkg_malloc(sizeof(sr_wtimer_node_t));
	if(wt==NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(wt, 0, sizeof(sr_wtimer_node_t));
	wt->f = f;
	wt->param = param;
	wt->interval = interval;
	wt->steps = interval % SR_WTIMER_SIZE;
	wt->loops = interval / SR_WTIMER_SIZE;
	wt->eloop = wt->loops;
	wt->next = _sr_wtimer->wlist[wt->steps];
	_sr_wtimer->wlist[wt->steps] = wt;

	return 0;
}

/**
 *
 */
int sr_wtimer_reinsert(uint32_t cs, sr_wtimer_node_t *wt)
{
	uint32_t ts;

	ts = (cs + wt->interval) % SR_WTIMER_SIZE;
	wt->eloop = wt->interval / SR_WTIMER_SIZE;
	wt->next = _sr_wtimer->wlist[ts];
	_sr_wtimer->wlist[ts] = wt;

	return 0;
}

/**
 *
 */
void sr_wtimer_exec(unsigned int ticks, void *param)
{
	sr_wtimer_node_t *wt;
	sr_wtimer_node_t *wn;
	sr_wtimer_node_t *wp;
	uint32_t cs;

	if(_sr_wtimer==NULL) {
		LM_ERR("wtimer not initialized\n");
		return;
	}

	_sr_wtimer->itimer++;
	cs = _sr_wtimer->itimer % SR_WTIMER_SIZE;
	/* uint32_t cl;
	cl = _sr_wtimer->itimer / SR_WTIMER_SIZE;
	LM_DBG("wtimer - loop: %u - slot: %u\n", cl, cs); */

	wp = NULL;
	wt=_sr_wtimer->wlist[cs];
	while(wt) {
		wn = wt->next;
		if(wt->eloop==0) {
			/* execute timer callback function */
			wt->f(ticks, wt->param);
			/* extract and reinsert timer item */
			if(wp==NULL) {
				_sr_wtimer->wlist[cs] = wn;
			} else {
				wp->next = wn;
			}
			sr_wtimer_reinsert(cs, wt);
		} else {
			wt->eloop--;
			wp = wt;
		}
		wt = wn;
	}
}

/**
 *
 */
int sr_wtimer_start(void)
{
	if(_sr_wtimer==NULL) {
		LM_ERR("wtimer not initialized\n");
		return -1;
	}

	if(fork_sync_timer(-1 /*PROC_TIMER*/, "secondary timer", 1,
				sr_wtimer_exec, NULL, 1)<0) {
		LM_ERR("wtimer starting failed\n");
		return -1;
	}

	return 0;
}

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */

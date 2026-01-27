/**
 * Copyright (C) 2025 Tyler Moore (dOpenSource)
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/route.h"
#include "../../core/receive.h"
#include "../../core/action.h"
#include "../../core/dset.h"
#include "../../core/pt.h"
#include "../../core/script_cb.h"
#include "../../core/parser/parse_param.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/fmsg.h"
#include "../../core/kemi.h"
#include "../../core/mod_fix.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

MODULE_VERSION

/* clang-format off */
/* module defines */
#define PTIMER_MAKE_SOCK		0

#define PTIMER_PRECISION_SEC	0
#define PTIMER_PRECISION_MSEC	1
#define PTIMER_PRECISION_USEC	2
#define PTIMER_PRECISION_NSEC	3

#define PTIMER_TYPE_BASIC		0
#define PTIMER_TYPE_SYNC		1
#define PTIMER_TYPE_SLICE		2
#define PTIMER_TYPE_SSLICE		3

#define PTIMER_STATE_PAUSED		(1 << 0)
#define PTIMER_STATE_START		(2 << 0)
#define PTIMER_STATE_LOOP		(3 << 0)
#define PTIMER_STATE_END		(4 << 0)

#define PTIMER_FROM_PKG			0
#define PTIMER_FROM_SHM			1

#define S_TO_MS(t)       ((t) * UINT64_C(1000))
#define S_TO_US(t)       ((t) * UINT64_C(1000000))
#define S_TO_NS(t)       ((t) * UINT64_C(1000000000))
#define MS_TO_S(t)       ((t) / UINT64_C(1000))
#define MS_TO_US(t)      ((t) * UINT64_C(1000))
#define MS_TO_NS(t)      ((t) * UINT64_C(1000000))
#define US_TO_S(t)       ((t) / UINT64_C(1000000))
#define US_TO_MS(t)      ((t) / UINT64_C(1000))
#define US_TO_NS(t)      ((t) * UINT64_C(1000))
#define NS_TO_S(t)       ((t) / UINT64_C(1000000000))
#define NS_TO_MS(t)      ((t) / UINT64_C(1000000))
#define NS_TO_US(t)      ((t) / UINT64_C(1000))
#define MS_TO_S_MOD(t)   ((t) % UINT64_C(1000))
#define US_TO_S_MOD(t)   ((t) % UINT64_C(1000000))
#define US_TO_MS_MOD(t)  ((t) % UINT64_C(1000))
#define NS_TO_S_MOD(t)   ((t) % UINT64_C(1000000000))
#define NS_TO_MS_MOD(t)  ((t) % UINT64_C(1000000))
#define NS_TO_US_MOD(t)  ((t) % UINT64_C(1000))

// integer division ceiling
#define DIV_CEIL(a,b) ((a) + (b) - 1) / (b)
// integer interval splitting
#define INTERVAL_SPLIT(i,n,k) ((((i) + 1) * (n) / (k)) - ((i) * (n) / (k)));

#define handle_pending_state(s)				\
	do {									\
		switch((s)) {						\
			case PTIMER_STATE_PAUSED:		\
				goto pause;					\
			case PTIMER_STATE_START:		\
				goto start;					\
			case PTIMER_STATE_LOOP:			\
				goto loop;					\
			case PTIMER_STATE_END:			\
				goto end;					\
		}									\
	} while(0)

// WARNING: literal const char[], do not write if assigned to char*
#define state_to_route(s) (					\
	((s) & PTIMER_STATE_START) ? "start" :	\
	((s) & PTIMER_STATE_LOOP) ? "loop" :	\
	((s) & PTIMER_STATE_END) ? "end" :		\
	"")

/* module types */
typedef struct pt_route
{
	str				name;		/* name of cfg route */
	unsigned int	route;		/* route number in cfg */
} pt_route_t;

typedef struct pt_worker
{
	unsigned int			worker;		/* worker ID (idx in timer->workers) */
	int						pid;		/* worker process ID */
	unsigned int			ntasks;		/* total tasks scaled per interval */
	unsigned int			tasks;		/* number of tasks in current slice */
	unsigned int			nslices;	/* total number of task slices */
	unsigned int			slice;		/* current slice timer is working on */
	uint64_t				tnow;		/* time at start of loop route */
	uint64_t				tnext;		/* target time to execute loop route */
	uint8_t					cstate;		/* current execution state of worker */
	volatile uint8_t		pstate;		/* pending execution state of worker */
} pt_worker_t;

typedef struct pt_timer
{
	str				name;		/* timer name */
	unsigned int	type;		/* type of timer executing route */
	str				interval;	/* interval timer executes route */
	unsigned int	iprec;		/* interval precision part (s/ms/us) */
	unsigned int	ival;		/* interval numeric part */
	unsigned int	nworkers;	/* total number of worker procs */
	unsigned int	ntasks;		/* total number of tasks to process */
	unsigned int	nslices;	/* total number of task slices */
	pt_route_t		*start;		/* route to execute before the loop */
	pt_route_t		*loop;		/* route to execute in the loop */
	pt_route_t		*end;		/* route to execute after the loop */
	pt_worker_t		*workers;	/* list of worker states */
	struct pt_timer	*next;		/* next timer in list */
} pt_timer_t;

typedef uint64_t (timer_time_t)(void);
typedef unsigned int (timer_sleep_t)(uint64_t ival);
typedef int (timer_run_t)(pt_timer_t *timer, unsigned int worker,
	timer_sleep_t *sleep_fn, timer_time_t *time_fn);
typedef int (exec_route_t)(pt_route_t *rt, sip_msg_t *fmsg,
	run_act_ctx_t *ctx, sr_kemi_eng_t *keng);

/* module globals */
static unsigned int		default_type		= PTIMER_TYPE_BASIC;
static str				default_interval	= str_init("1s");
static unsigned int		default_iprec		= PTIMER_PRECISION_SEC;
static unsigned int		default_ival		= 1;
static unsigned int		default_nworkers	= 1;
static str				ptimer_evname		= str_init("ptimer");
static pt_timer_t		*pkg_timer_list		= NULL;
static pt_timer_t		*timer_list			= NULL;
static pt_timer_t		*current_timer		= NULL;
static pt_worker_t		*current_worker		= NULL;

/* module exports */
static int param_parse_type(modparam_t type, void *val);
static int param_parse_interval(modparam_t type, void *val);
static int param_parse_nworkers(modparam_t type, void *val);
static int param_parse_timer(modparam_t type, void *val);
static int param_parse_start(modparam_t type, void *val);
static int param_parse_loop(modparam_t type, void *val);
static int param_parse_end(modparam_t type, void *val);
static int pv_parse_ptimer(pv_spec_p sp, str *in);
static int pv_get_ptimer(
	struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int pv_set_ptimer(
	struct sip_msg *msg, pv_param_t *param, int op, pv_value_t *val);
static const char *rpc_list_timers_doc[2] = {
	"Show the current timers", 0};
static void rpc_list_timers(rpc_t *rpc, void *ctx);
static const char *rpc_pause_timer_doc[2] = {
	"Pause execution of timer worker(s)", 0};
static void rpc_pause_timer(rpc_t *rpc, void *ctx);
static const char *rpc_continue_timer_doc[2] = {
	"Continue execution of timer worker(s)", 0};
static void rpc_continue_timer(rpc_t *rpc, void *ctx);
static const char *rpc_start_timer_doc[2] = {
	"Jump execution of timer worker(s) to start route", 0};
static void rpc_start_timer(rpc_t *rpc, void *ctx);
static const char *rpc_loop_timer_doc[2] = {
	"Jump execution of timer worker(s) to loop route", 0};
static void rpc_loop_timer(rpc_t *rpc, void *ctx);
static const char *rpc_end_timer_doc[2] = {
	"Jump execution of timer worker(s) to end route", 0};
static void rpc_end_timer(rpc_t *rpc, void *ctx);
static int mod_init(void);
static int child_init(int rank);
static void destroy_mod(void);

static pv_export_t mod_pvs[] = {
	{{"ptimer", (sizeof("ptimer") - 1)}, PVT_OTHER, pv_get_ptimer, pv_set_ptimer, pv_parse_ptimer, 0, 0, 0},
	{{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

static param_export_t mod_params[] = {
	{"default_type", PARAM_INT | PARAM_USE_FUNC, (void *)param_parse_type},
	{"default_interval", PARAM_STR | PARAM_USE_FUNC, (void *)param_parse_interval},
	{"default_nworkers", PARAM_INT | PARAM_USE_FUNC, (void *)param_parse_nworkers},
	{"timer", PARAM_STRING | PARAM_USE_FUNC, (void *)param_parse_timer},
	{"start", PARAM_STRING | PARAM_USE_FUNC, (void *)param_parse_start},
	{"loop", PARAM_STRING | PARAM_USE_FUNC, (void *)param_parse_loop},
	{"end", PARAM_STRING | PARAM_USE_FUNC, (void *)param_parse_end},
	{0, 0, 0}
};

static rpc_export_t rpc_cmds[] = {
	{"ptimer.list",   rpc_list_timers, rpc_list_timers_doc, 0},
	{"ptimer.pause",   rpc_pause_timer, rpc_pause_timer_doc, 0},
	{"ptimer.continue", rpc_continue_timer, rpc_continue_timer_doc, 0},
	{"ptimer.start", rpc_start_timer, rpc_start_timer_doc, 0},
	{"ptimer.loop", rpc_loop_timer, rpc_loop_timer_doc, 0},
	{"ptimer.end",   rpc_end_timer, rpc_end_timer_doc, 0},
	{0, 0, 0, 0}
};

struct module_exports exports = {
	"ptimer",				/* module name */
	DEFAULT_DLFLAGS,		/* dlopen flags */
	0,					/* exported functions */
	mod_params,					/* exported parameters */
	0,				/* RPC methods (moved to mod_init) */
	mod_pvs,			/* exported pseudo-variables */
	0,					/* response handling function */
	mod_init,			/* module initialization function */
	child_init,					/* per-child init function */
	destroy_mod					/* module destroy function */
};
/* clang-format on */

/* module functions */
static inline uint64_t time_to_ns(const unsigned int t, unsigned int iprec)
{
	switch(iprec) {
		case PTIMER_PRECISION_SEC:
			return S_TO_NS(t);
		case PTIMER_PRECISION_MSEC:
			return MS_TO_NS(t);
		case PTIMER_PRECISION_USEC:
			return US_TO_NS(t);
		default:
			return (uint64_t)t;
	}
}

static unsigned int sleep_sec(uint64_t secs)
{
	return sleep((unsigned int)secs);
}

static unsigned int sleep_msec(uint64_t msecs)
{
	struct timespec ts;
	ts.tv_sec = MS_TO_S(msecs);
	ts.tv_nsec = MS_TO_NS(MS_TO_S_MOD(msecs));
	nanosleep(&ts, NULL);
	return 0;
}

static unsigned int sleep_usec(uint64_t usecs)
{
	struct timespec ts;
	ts.tv_sec = US_TO_S(usecs);
	ts.tv_nsec = US_TO_NS(US_TO_S_MOD(usecs));
	nanosleep(&ts, NULL);
	return 0;
}

static unsigned int sleep_nsec(uint64_t nsecs)
{
	struct timespec ts;
	ts.tv_sec = NS_TO_S(nsecs);
	ts.tv_nsec = NS_TO_S_MOD(nsecs);
	nanosleep(&ts, NULL);
	return 0;
}

static uint64_t time_nsec(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return S_TO_NS(ts.tv_sec) + ts.tv_nsec;
}

static int exec_sr_route(pt_route_t *rt, sip_msg_t *fmsg, run_act_ctx_t *ctx,
		sr_kemi_eng_t *keng)
{
	return run_top_route(main_rt.rlist[rt->route], fmsg, ctx);
}

static int exec_kemi_route(pt_route_t *rt, sip_msg_t *fmsg, run_act_ctx_t *ctx,
		sr_kemi_eng_t *keng)
{
	return sr_kemi_ctx_route(
			keng, ctx, fmsg, EVENT_ROUTE, &rt->name, &ptimer_evname);
}

static int register_timers(int timers)
{
	if(register_procs(timers) < 0) {
		return -1;
	}
	cfg_register_child(timers);

	return 0;
}

static int fork_timer(int child_id, char *desc, pt_timer_t *timer,
		unsigned int worker, timer_run_t *timer_fn, timer_sleep_t *sleep_fn,
		timer_time_t *time_fn)
{
	int pid;

	pid = fork_process(child_id, desc, PTIMER_MAKE_SOCK);
	if(pid < 0) {
		return -1;
	}
	if(pid == 0) {
		/* child */
		return timer_fn(timer, worker, sleep_fn, time_fn);
	}
	/* parent */
	return pid;
}

static int basic_timer(pt_timer_t *timer, unsigned int worker,
		timer_sleep_t *sleep_fn, timer_time_t *time_fn)
{
	sip_msg_t *fmsg;
	sr_kemi_eng_t *keng;
	exec_route_t *route_fn;
	run_act_ctx_t ctx;

	/* init section */
	if(cfg_child_init()) {
		return -1;
	}
	current_timer = timer;
	current_worker = &current_timer->workers[worker];
	set_route_type(REQUEST_ROUTE);
	keng = sr_kemi_eng_get();
	if(keng == NULL) {
		route_fn = exec_sr_route;
	} else {
		route_fn = exec_kemi_route;
	}

start:
	current_worker->cstate = PTIMER_STATE_START;
	current_worker->pstate = 0;
	init_run_actions_ctx(&ctx);
	fmsg = faked_msg_next();
	if(exec_pre_script_cb(fmsg, REQUEST_CB_TYPE) == 0) {
		LM_ERR("failed pre-routing, pausing timer\n");
		goto pause;
	}
	if(current_timer->start != NULL) {
		cfg_update();
		if(route_fn(current_timer->start, fmsg, &ctx, keng) < 0) {
			LM_DBG("start route returned < 0, jumping to end\n");
			goto end;
		}
	}
	handle_pending_state(current_worker->pstate);

loop:
	current_worker->cstate = PTIMER_STATE_LOOP;
	current_worker->pstate = 0;
	for(;;) {
		sleep_fn(current_timer->ival);
		cfg_update();
		if(unlikely(route_fn(current_timer->loop, fmsg, &ctx, keng) < 0)) {
			LM_DBG("loop route returned < 0, jumping to end\n");
			goto end;
		}
		if(unlikely(current_worker->pstate != 0)) {
			handle_pending_state(current_worker->pstate);
		}
	}

end:
	current_worker->cstate = PTIMER_STATE_END;
	current_worker->pstate = 0;
	if(current_timer->end != NULL) {
		cfg_update();
		route_fn(current_timer->end, fmsg, &ctx, keng);
	}
	exec_post_script_cb(fmsg, REQUEST_CB_TYPE);
	ksr_msg_env_reset();
	handle_pending_state(current_worker->pstate);

pause:
	current_worker->cstate |= PTIMER_STATE_PAUSED;
	current_worker->pstate = 0;
	if(kill(current_worker->pid, SIGSTOP) != 0) {
		LM_ERR("could not pause timer worker\n");
		return -1;
	}
	handle_pending_state(current_worker->pstate);

	/*
	 * if we got here external SIGCONT was sent w/o setting pstate
	 * ignore and go back to paused section
	 */
	goto pause;
}

static int sync_timer(pt_timer_t *timer, unsigned int worker,
		timer_sleep_t *sleep_fn, timer_time_t *time_fn)
{
	sip_msg_t *fmsg;
	sr_kemi_eng_t *keng;
	exec_route_t *route_fn;
	run_act_ctx_t ctx;
	uint64_t iscaled;

	/* init section */
	if(cfg_child_init()) {
		return -1;
	}
	current_timer = timer;
	current_worker = &current_timer->workers[worker];
	set_route_type(REQUEST_ROUTE);
	keng = sr_kemi_eng_get();
	if(keng == NULL) {
		route_fn = exec_sr_route;
	} else {
		route_fn = exec_kemi_route;
	}

start:
	current_worker->cstate = PTIMER_STATE_START;
	current_worker->pstate = 0;
	init_run_actions_ctx(&ctx);
	fmsg = faked_msg_next();
	if(exec_pre_script_cb(fmsg, REQUEST_CB_TYPE) == 0) {
		LM_ERR("failed pre-routing, pausing timer\n");
		goto pause;
	}
	if(current_timer->start != NULL) {
		cfg_update();
		if(route_fn(current_timer->start, fmsg, &ctx, keng) < 0) {
			LM_DBG("start route returned < 0, jumping to end\n");
			goto end;
		}
	}
	iscaled = time_to_ns(current_timer->ival, current_timer->iprec);
	handle_pending_state(current_worker->pstate);

loop:
	current_worker->cstate = PTIMER_STATE_LOOP;
	current_worker->pstate = 0;
	current_worker->tnext = time_fn() + iscaled;
	for(;;) {
		cfg_update();
		if(unlikely(route_fn(current_timer->loop, fmsg, &ctx, keng) < 0)) {
			LM_DBG("loop route returned < 0, jumping to end\n");
			goto end;
		}
		if(unlikely(current_worker->pstate != 0)) {
			handle_pending_state(current_worker->pstate);
		}
		current_worker->tnow = time_fn();
		if(current_worker->tnow < current_worker->tnext) {
			sleep_fn(current_worker->tnext - current_worker->tnow);
		} else {
			current_worker->tnext = current_worker->tnow;
		}
		current_worker->tnext += iscaled;
	}

end:
	current_worker->cstate = PTIMER_STATE_END;
	current_worker->pstate = 0;
	if(current_timer->end != NULL) {
		cfg_update();
		route_fn(current_timer->end, fmsg, &ctx, keng);
	}
	exec_post_script_cb(fmsg, REQUEST_CB_TYPE);
	ksr_msg_env_reset();
	handle_pending_state(current_worker->pstate);

pause:
	current_worker->cstate |= PTIMER_STATE_PAUSED;
	current_worker->pstate = 0;
	if(kill(current_worker->pid, SIGSTOP) != 0) {
		LM_ERR("could not pause timer worker\n");
		return -1;
	}
	handle_pending_state(current_worker->pstate);

	/*
	 * if we got here external SIGCONT was sent w/o setting pstate
	 * ignore and go back to paused section
	 */
	goto pause;
}

static int slice_timer(pt_timer_t *timer, unsigned int worker,
		timer_sleep_t *sleep_fn, timer_time_t *time_fn)
{
	sip_msg_t *fmsg;
	sr_kemi_eng_t *keng;
	exec_route_t *route_fn;
	run_act_ctx_t ctx;

	/* init section */
	if(cfg_child_init()) {
		return -1;
	}
	current_timer = timer;
	current_worker = &current_timer->workers[worker];
	set_route_type(REQUEST_ROUTE);
	keng = sr_kemi_eng_get();
	if(keng == NULL) {
		route_fn = exec_sr_route;
	} else {
		route_fn = exec_kemi_route;
	}

start:
	current_worker->cstate = PTIMER_STATE_START;
	current_worker->pstate = 0;
	init_run_actions_ctx(&ctx);
	fmsg = faked_msg_next();
	if(exec_pre_script_cb(fmsg, REQUEST_CB_TYPE) == 0) {
		LM_ERR("failed pre-routing, pausing timer\n");
		goto pause;
	}
	current_worker->slice = 0;
	if(current_timer->start != NULL) {
		cfg_update();
		if(route_fn(current_timer->start, fmsg, &ctx, keng) < 0) {
			LM_DBG("start route returned < 0, jumping to end\n");
			goto end;
		}
	}
	handle_pending_state(current_worker->pstate);

loop:
	current_worker->cstate = PTIMER_STATE_LOOP;
	current_worker->pstate = 0;
	for(;;) {
		sleep_fn(current_timer->ival);
		current_worker->slice =
				(current_worker->slice + 1) % current_worker->nslices;
		current_worker->tasks = INTERVAL_SPLIT(current_worker->slice,
				current_worker->ntasks, current_worker->nslices);
		cfg_update();
		if(unlikely(route_fn(current_timer->loop, fmsg, &ctx, keng) < 0)) {
			LM_DBG("loop route returned < 0, jumping to end\n");
			goto end;
		}
		if(unlikely(current_worker->pstate != 0)) {
			handle_pending_state(current_worker->pstate);
		}
	}

end:
	current_worker->cstate = PTIMER_STATE_END;
	current_worker->pstate = 0;
	if(current_timer->end != NULL) {
		cfg_update();
		route_fn(current_timer->end, fmsg, &ctx, keng);
	}
	exec_post_script_cb(fmsg, REQUEST_CB_TYPE);
	ksr_msg_env_reset();
	handle_pending_state(current_worker->pstate);

pause:
	current_worker->cstate |= PTIMER_STATE_PAUSED;
	current_worker->pstate = 0;
	if(kill(current_worker->pid, SIGSTOP) != 0) {
		LM_ERR("could not pause timer worker\n");
		return -1;
	}
	handle_pending_state(current_worker->pstate);

	/*
	 * if we got here external SIGCONT was sent w/o setting pstate
	 * ignore and go back to paused section
	 */
	goto pause;
}

static int synced_slice_timer(pt_timer_t *timer, unsigned int worker,
		timer_sleep_t *sleep_fn, timer_time_t *time_fn)
{
	sip_msg_t *fmsg;
	sr_kemi_eng_t *keng;
	exec_route_t *route_fn;
	run_act_ctx_t ctx;
	uint64_t iscaled;

	/* init section */
	if(cfg_child_init()) {
		return -1;
	}
	current_timer = timer;
	current_worker = &current_timer->workers[worker];
	set_route_type(REQUEST_ROUTE);
	keng = sr_kemi_eng_get();
	if(keng == NULL) {
		route_fn = exec_sr_route;
	} else {
		route_fn = exec_kemi_route;
	}

start:
	current_worker->cstate = PTIMER_STATE_START;
	current_worker->pstate = 0;
	init_run_actions_ctx(&ctx);
	fmsg = faked_msg_next();
	if(exec_pre_script_cb(fmsg, REQUEST_CB_TYPE) == 0) {
		LM_ERR("failed pre-routing, pausing timer\n");
		goto pause;
	}
	current_worker->slice = 0;
	if(current_timer->start != NULL) {
		cfg_update();
		if(route_fn(current_timer->start, fmsg, &ctx, keng) < 0) {
			LM_DBG("start route returned < 0, jumping to end\n");
			goto end;
		}
	}
	iscaled = time_to_ns(current_timer->ival, current_timer->iprec);
	handle_pending_state(current_worker->pstate);

loop:
	current_worker->cstate = PTIMER_STATE_LOOP;
	current_worker->pstate = 0;
	// account for first iteration (even if we jumped here)
	current_worker->slice = current_worker->slice - 1U;
	current_worker->tnext = time_fn() + iscaled;
	for(;;) {
		current_worker->slice =
				(current_worker->slice + 1) % current_worker->nslices;
		current_worker->tasks = INTERVAL_SPLIT(current_worker->slice,
				current_worker->ntasks, current_worker->nslices);
		cfg_update();
		if(unlikely(route_fn(current_timer->loop, fmsg, &ctx, keng) < 0)) {
			LM_DBG("loop route returned < 0, jumping to end\n");
			goto end;
		}
		if(unlikely(current_worker->pstate != 0)) {
			handle_pending_state(current_worker->pstate);
		}
		current_worker->tnow = time_fn();
		if(current_worker->tnow < current_worker->tnext) {
			sleep_fn(current_worker->tnext - current_worker->tnow);
		} else {
			current_worker->tnext = current_worker->tnow;
		}
		current_worker->tnext += iscaled;
	}

end:
	current_worker->cstate = PTIMER_STATE_END;
	current_worker->pstate = 0;
	if(current_timer->end != NULL) {
		cfg_update();
		route_fn(current_timer->end, fmsg, &ctx, keng);
	}
	exec_post_script_cb(fmsg, REQUEST_CB_TYPE);
	ksr_msg_env_reset();
	handle_pending_state(current_worker->pstate);

pause:
	current_worker->cstate |= PTIMER_STATE_PAUSED;
	current_worker->pstate = 0;
	if(kill(current_worker->pid, SIGSTOP) != 0) {
		LM_ERR("could not pause timer worker\n");
		return -1;
	}
	handle_pending_state(current_worker->pstate);

	/*
	 * if we got here external SIGCONT was sent w/o setting pstate
	 * ignore and go back to paused section
	 */
	goto pause;
}

static inline pt_timer_t *get_timer(str *name, unsigned short memloc)
{
	pt_timer_t *pit = NULL;

	switch(memloc) {
		case PTIMER_FROM_PKG:
			pit = pkg_timer_list;
			break;
		case PTIMER_FROM_SHM:
			pit = timer_list;
			break;
		default:
			LM_CRIT("invalid timer memloc %d", memloc);
			return pit;
	}
	while(pit != NULL) {
		if(pit->name.len == name->len
				&& strncmp(pit->name.s, name->s, name->len) == 0) {
			break;
		}
		pit = pit->next;
	}

	return pit;
}

static inline short is_valid_type(const unsigned int type)
{
	switch(type) {
		case PTIMER_TYPE_BASIC:
		case PTIMER_TYPE_SYNC:
		case PTIMER_TYPE_SLICE:
		case PTIMER_TYPE_SSLICE:
			return 1;
		default:
			LM_ERR("invalid type: %d\n", type);
			return 0;
	}
}

static inline short is_valid_ival(const unsigned int ival)
{
	if(ival == 0) {
		LM_ERR("interval can not be 0\n");
		return 0;
	}
	return 1;
}

static inline short is_valid_nworkers(const unsigned int nworkers)
{
	if(nworkers == 0) {
		LM_ERR("nworkers can not be 0\n");
		return 0;
	}
	return 1;
}

static inline short is_valid_nslices(const unsigned int nslices)
{
	if(nslices == 0) {
		LM_ERR("nslices can not be 0\n");
		return 0;
	}
	return 1;
}

static inline short parse_interval(
		const str *interval, unsigned int *ival, unsigned int *iprec)
{
	unsigned int _iprec = PTIMER_PRECISION_SEC;
	unsigned int _ival;
	str _interval = STR_NULL;

	if(interval == NULL || interval->len == 0 || interval->s == NULL) {
		LM_ERR("invalid interval\n");
		goto err1;
	}

	if(pkg_str_dup(&_interval, interval) < 0) {
		LM_ERR("could not allocate pkgmem for interval\n");
		goto err1;
	}

	if(_interval.s[_interval.len - 1] == 's') {
		_interval.len--;
		_iprec = PTIMER_PRECISION_SEC;
	}
	if(_interval.len > 0) {
		if(_interval.s[_interval.len - 1] == 'm') {
			_interval.len--;
			_iprec = PTIMER_PRECISION_MSEC;
		} else if(_interval.s[_interval.len - 1] == 'u') {
			_interval.len--;
			_iprec = PTIMER_PRECISION_USEC;
		}
	}
	if(str2int(&_interval, &_ival) < 0) {
		LM_ERR("invalid interval: %.*s\n", _interval.len, _interval.s);
		goto err2;
	}
	if(!is_valid_ival(_ival)) {
		goto err2;
	}
	*iprec = _iprec;
	*ival = _ival;
	pkg_free(_interval.s);
	return 1;

err2:
	pkg_free(_interval.s);
err1:
	return 0;
}

static int param_parse_type(modparam_t type, void *val)
{
	if(val == NULL) {
		LM_ERR("invalid type\n");
		return -1;
	}
	if(!is_valid_type((unsigned int)(long)(int *)val)) {
		return -1;
	}
	default_type = (unsigned int)(long)(int *)val;

	return 0;
}

static int param_parse_interval(modparam_t type, void *val)
{
	if(!parse_interval(val, &default_ival, &default_iprec)) {
		return -1;
	}
	default_interval = *((str *)val);

	return 0;
}

static int param_parse_nworkers(modparam_t type, void *val)
{
	if(val == NULL) {
		LM_ERR("invalid nworkers\n");
		return -1;
	}
	if(!is_valid_nworkers((unsigned int)(long)(int *)val)) {
		return -1;
	}
	default_nworkers = (unsigned int)(long)(int *)val;

	return 0;
}

static int param_parse_timer(modparam_t type, void *val)
{
	param_t *params_list = NULL;
	param_hooks_t phooks;
	param_t *pit = NULL;
	pt_timer_t tmp = {.name = STR_NULL,
			.type = default_type,
			.interval = STR_NULL,
			.iprec = default_iprec,
			.ival = default_ival,
			.nworkers = default_nworkers,
			.ntasks = 0,
			.nslices = 1,
			.start = NULL,
			.loop = NULL,
			.end = NULL,
			.workers = NULL,
			.next = NULL};
	pt_timer_t *nt;
	str s;
	str *sp1 = NULL;
	str *sp2 = &default_interval;

	if(val == NULL) {
		goto err1;
	}

	s.s = (char *)val;
	s.len = strlen(s.s);
	if(s.s[s.len - 1] == ';') {
		s.len--;
	}
	if(parse_params(&s, CLASS_ANY, &phooks, &params_list) < 0) {
		goto err1;
	}

	for(pit = params_list; pit; pit = pit->next) {
		switch(pit->name.len) {
			case 4:
				if(strncmp(pit->name.s, "name", 4) == 0) {
					sp1 = &pit->body;
				} else if(strncmp(pit->name.s, "type", 4) == 0) {
					if(str2int(&pit->body, &tmp.type) < 0) {
						LM_ERR("invalid type: %.*s\n", pit->body.len,
								pit->body.s);
						goto err2;
					}
				}
				break;
			case 6:
				if(strncmp(pit->name.s, "ntasks", 6) == 0) {
					if(str2int(&pit->body, &tmp.ntasks) < 0) {
						LM_ERR("invalid ntasks: %.*s\n", pit->body.len,
								pit->body.s);
						goto err2;
					}
				}
				break;
			case 7:
				if(strncmp(pit->name.s, "nslices", 7) == 0) {
					if(str2int(&pit->body, &tmp.nslices) < 0) {
						LM_ERR("invalid nslices: %.*s\n", pit->body.len,
								pit->body.s);
						goto err2;
					}
				}
				break;
			case 8:
				if(strncmp(pit->name.s, "interval", 8) == 0) {
					if(!parse_interval(&pit->body, &tmp.ival, &tmp.iprec)) {
						goto err2;
					}
					sp2 = &pit->body;
				} else if(strncmp(pit->name.s, "nworkers", 8) == 0) {
					if(str2int(&pit->body, &tmp.nworkers) < 0) {
						LM_ERR("invalid nworkers: %.*s\n", pit->body.len,
								pit->body.s);
						goto err2;
					}
				}
				break;
			default:
				LM_WARN("invalid timer attribute [%.*s] ignored\n",
						pit->body.len, pit->body.s);
				break;
		}
	}

	/* required, no default, not set */
	if(sp1 == NULL) {
		LM_ERR("invalid timer name\n");
		goto err2;
	}
	/* intentionally set, invalid value */
	if(!is_valid_nworkers(tmp.nworkers)) {
		goto err2;
	}
	if(!is_valid_nslices(tmp.nslices)) {
		goto err2;
	}
	if(!is_valid_type(tmp.type)) {
		goto err2;
	}

	/* check for same timer */
	nt = pkg_timer_list;
	while(nt) {
		if(nt->name.len == sp1->len
				&& strncmp(nt->name.s, sp1->s, sp1->len) == 0) {
			break;
		}
		nt = nt->next;
	}
	if(nt != NULL) {
		LM_ERR("duplicate timer with same name: %.*s\n", sp1->len, sp1->s);
		goto err2;
	}
	if(pkg_str_dup(&tmp.name, sp1) < 0) {
		LM_ERR("could not allocate pkgmem for name\n");
		goto err2;
	}
	if(pkg_str_dup(&tmp.interval, sp2) < 0) {
		LM_ERR("could not allocate pkgmem for interval\n");
		goto err2;
	}

	nt = (pt_timer_t *)pkg_malloc(sizeof(pt_timer_t));
	if(nt == NULL) {
		PKG_MEM_ERROR;
		goto err2;
	}
	memcpy(nt, &tmp, sizeof(pt_timer_t));
	nt->next = pkg_timer_list;
	pkg_timer_list = nt;
	LM_INFO("created timer: name=%.*s type=%d interval=%.*s nworkers=%d "
			"ntasks=%d nslices=%d\n",
			tmp.name.len, tmp.name.s, tmp.type, tmp.interval.len,
			tmp.interval.s, tmp.nworkers, tmp.ntasks, tmp.nslices);

	free_params(params_list);
	return 0;

err2:
	if(tmp.name.s != NULL) {
		pkg_free(tmp.name.s);
	}
	if(tmp.interval.s != NULL) {
		pkg_free(tmp.interval.s);
	}
	free_params(params_list);
err1:
	return -1;
}

static inline int parse_route_param(
		modparam_t type, void *val, unsigned short which)
{
	param_t *params_list = NULL;
	param_hooks_t phooks;
	param_t *pit = NULL;
	pt_route_t tmp;
	pt_route_t *rt = NULL;
	pt_timer_t *nt;
	str s1;
	str s2 = STR_NULL;
	str s3 = STR_NULL;

	if(val == NULL) {
		goto err1;
	}

	s1.s = (char *)val;
	s1.len = strlen(s1.s);
	if(s1.s[s1.len - 1] == ';') {
		s1.len--;
	}
	if(parse_params(&s1, CLASS_ANY, &phooks, &params_list) < 0) {
		goto err1;
	}

	memset(&tmp, 0, sizeof(pt_route_t));
	for(pit = params_list; pit; pit = pit->next) {
		if(pit->name.len == 5 && strncmp(pit->name.s, "timer", 5) == 0) {
			s2 = pit->body;
		} else if(pit->name.len == 5 && strncmp(pit->name.s, "route", 5) == 0) {
			s3 = pit->body;
		}
	}
	if(s2.s == NULL) {
		LM_ERR("invalid timer name\n");
		goto err2;
	}
	if(s3.s == NULL) {
		LM_ERR("invalid route name\n");
		goto err2;
	}

	nt = get_timer(&s2, PTIMER_FROM_PKG);
	if(nt == NULL) {
		LM_ERR("timer not found - name: %.*s\n", s2.len, s2.s);
		goto err2;
	}
	if(pkg_str_dup(&tmp.name, &s3) < 0) {
		LM_ERR("could not allocate pkgmem for name\n");
		goto err2;
	}

	rt = (pt_route_t *)pkg_malloc(sizeof(pt_route_t));
	if(rt == 0) {
		PKG_MEM_ERROR;
		goto err3;
	}
	memcpy(rt, &tmp, sizeof(pt_route_t));
	switch(which) {
		case 0:
			nt->start = rt;
			break;
		case 1:
			nt->loop = rt;
			break;
		case 2:
			nt->end = rt;
			break;
		default:
			LM_CRIT("parsing route param type=%d is undefined\n", which);
			pkg_free(rt);
			goto err3;
	}

	free_params(params_list);
	return 0;

err3:
	pkg_free(tmp.name.s);
err2:
	free_params(params_list);
err1:
	return -1;
}

static int param_parse_start(modparam_t type, void *val)
{
	return parse_route_param(type, val, 0);
}

static int param_parse_loop(modparam_t type, void *val)
{
	return parse_route_param(type, val, 1);
}

static int param_parse_end(modparam_t type, void *val)
{
	return parse_route_param(type, val, 2);
}

static int pv_parse_ptimer(pv_spec_p sp, str *in)
{
	if(sp == NULL || in == NULL || in->len <= 0) {
		return -1;
	}

	switch(in->len) {
		case 3:
			if(strncmp(in->s, "pid", 3) == 0) {
				sp->pvp.pvn.u.isname.name.n = 6;
			} else {
				goto err;
			}
			break;
		case 4:
			if(strncmp(in->s, "name", 4) == 0) {
				sp->pvp.pvn.u.isname.name.n = 1;
			} else if(strncmp(in->s, "type", 4) == 0) {
				sp->pvp.pvn.u.isname.name.n = 2;
			} else {
				goto err;
			}
			break;
		case 5:
			if(strncmp(in->s, "tasks", 5) == 0) {
				sp->pvp.pvn.u.isname.name.n = 8;
			} else if(strncmp(in->s, "slice", 5) == 0) {
				sp->pvp.pvn.u.isname.name.n = 10;
			} else {
				goto err;
			}
			break;
		case 6:
			if(strncmp(in->s, "worker", 6) == 0) {
				sp->pvp.pvn.u.isname.name.n = 5;
			} else if(strncmp(in->s, "ntasks", 6) == 0) {
				sp->pvp.pvn.u.isname.name.n = 7;
			} else {
				goto err;
			}
			break;
		case 7:
			if(strncmp(in->s, "nslices", 7) == 0) {
				sp->pvp.pvn.u.isname.name.n = 9;
			} else {
				goto err;
			}
			break;
		case 8:
			if(strncmp(in->s, "nworkers", 8) == 0) {
				sp->pvp.pvn.u.isname.name.n = 4;
			} else if(strncmp(in->s, "interval", 8) == 0) {
				sp->pvp.pvn.u.isname.name.n = 3;
			} else {
				goto err;
			}
			break;
		default:
			goto err;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

err:
	LM_ERR("unknown PV name %.*s\n", in->len, in->s);
	return -1;
}

static int pv_get_ptimer(
		struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	if(param == NULL) {
		LM_ERR("ptimer PV requires a target variable name to access\n");
		return -1;
	}

	if(current_timer == NULL) {
		LM_ERR("ptimer PV is not available outside of ptimer executed route\n");
		return -1;
	}

	switch(param->pvn.u.isname.name.n) {
		case 1:
			return pv_get_strval(msg, param, res, &current_timer->name);
		case 2:
			return pv_get_uintval(msg, param, res, current_timer->type);
		case 3:
			return pv_get_strval(msg, param, res, &current_timer->interval);
		case 4:
			return pv_get_uintval(msg, param, res, current_timer->nworkers);
		case 5:
			return pv_get_uintval(msg, param, res, current_worker->worker);
		case 6:
			return pv_get_sintval(msg, param, res, current_worker->pid);
		case 7:
			return pv_get_uintval(msg, param, res, current_worker->ntasks);
		case 8:
			return pv_get_uintval(msg, param, res, current_worker->tasks);
		case 9:
			return pv_get_uintval(msg, param, res, current_worker->nslices);
		case 10:
			return pv_get_uintval(msg, param, res, current_worker->slice);
		default:
			return pv_get_null(msg, param, res);
	}
}

static int pv_set_ptimer(
		struct sip_msg *msg, pv_param_t *param, int op, pv_value_t *val)
{
	str s = STR_NULL;
	char *cp = NULL;

	if(param == NULL) {
		LM_ERR("ptimer PV requires a target variable name to access\n");
		return -1;
	}

	if(current_timer == NULL) {
		LM_ERR("ptimer PV is not available outside of ptimer executed route\n");
		return -1;
	}

	if(!(current_worker->cstate & PTIMER_STATE_START)) {
		LM_ERR("ptimer PV variables can only be assigned inside start route\n");
		return -1;
	}

	switch(param->pvn.u.isname.name.n) {
		case 3:
			if(val == NULL || val->flags & PV_VAL_NULL) {
				if(shm_str_dup(&s, &default_interval) < 0) {
					LM_ERR("could not allocate shmem for interval\n");
					goto err;
				}
				current_timer->iprec = default_iprec;
				current_timer->ival = default_ival;
				cp = current_timer->interval.s;
				current_timer->interval = s;
				shm_free(cp);
			} else if(val->flags & PV_VAL_STR) {
				if(shm_str_dup(&s, &val->rs) < 0) {
					LM_ERR("could not allocate shmem for interval\n");
					goto err;
				}
				if(!parse_interval(
						   &s, &current_timer->iprec, &current_timer->ival)) {
					goto err;
				}
				cp = current_timer->interval.s;
				current_timer->interval = s;
				shm_free(cp);
			} else {
				LM_WARN("ptimer(interval) can only be set to str or null\n");
				goto err;
			}
			break;
		case 7:
			if(val == NULL || val->flags & PV_VAL_NULL) {
				current_worker->ntasks = 0;
			} else if(val->flags & PV_VAL_INT) {
				current_worker->ntasks = val->ri;
			} else {
				LM_WARN("ptimer(ntasks) can only be set to an integer\n");
				goto err;
			}
			break;
		case 8:
			if(val == NULL || val->flags & PV_VAL_NULL) {
				current_worker->tasks = 0;
			} else if(val->flags & PV_VAL_INT) {
				current_worker->tasks = val->ri;
			} else {
				LM_WARN("ptimer(tasks) can only be set to an integer\n");
				goto err;
			}
			break;
		case 9:
			if(val == NULL || val->flags & PV_VAL_NULL) {
				current_worker->nslices = 0;
			} else if(val->flags & PV_VAL_INT) {
				current_worker->nslices = val->ri;
			} else {
				LM_WARN("ptimer(nslices) can only be set to an integer\n");
				goto err;
			}
			break;
		case 10:
			if(val == NULL || val->flags & PV_VAL_NULL) {
				current_worker->slice = 0;
			} else if(val->flags & PV_VAL_INT) {
				current_worker->slice = val->ri;
			} else {
				LM_WARN("ptimer(slice) can only be set to an integer\n");
				goto err;
			}
			break;
		default:
			LM_ERR("ptimer(%.*s) is read only\n",
					param->pvn.u.isname.name.s.len,
					param->pvn.u.isname.name.s.s);
			goto err;
			break;
	}

	return 0;

err:
	if(s.s != NULL) {
		shm_free(s.s);
	}
	return -1;
}

static void rpc_list_timers(rpc_t *rpc, void *ctx)
{
	unsigned int i;
	char *s;
	void *rh;
	void *th;
	void *wh;
	void *wch;
	pt_timer_t *pit;
	str empty = str_init("");

	if(rpc->add(ctx, "{", &rh) < 0) {
		rpc->fault(ctx, 500, "Internal error root structure");
		return;
	}

	for(pit = timer_list; pit; pit = pit->next) {
		if(rpc->struct_add(rh, "{", pit->name.s, &th) < 0) {
			rpc->fault(ctx, 500, "Internal error timer structure");
			return;
		}
		if(rpc->struct_add(th, "dSdddSSS[", "type", pit->type, "interval",
				   &pit->interval, "nworkers", pit->nworkers, "ntasks",
				   pit->ntasks, "nslices", pit->nslices, "start",
				   pit->start != NULL ? &pit->start->name : &empty, "loop",
				   &pit->loop->name, "end",
				   pit->end != NULL ? &pit->end->name : &empty, "workers", &wh)
				< 0) {
			rpc->fault(ctx, 500, "Internal error timer structure");
			return;
		}
		for(i = 0; i < pit->nworkers; i++) {
			if(rpc->array_add(wh, "{", &wch) < 0) {
				rpc->fault(ctx, 500, "Internal error workers structure");
				return;
			}
			s = (char *)state_to_route(pit->workers[i].cstate);
			if(rpc->struct_add(wch, "ddddddds", "worker",
					   pit->workers[i].worker, "pid", pit->workers[i].pid,
					   "ntasks", pit->workers[i].ntasks, "tasks",
					   pit->workers[i].tasks, "nslices",
					   pit->workers[i].nslices, "slice", pit->workers[i].slice,
					   "paused", pit->workers[i].cstate & PTIMER_STATE_PAUSED,
					   "croute", s)
					< 0) {
				rpc->fault(ctx, 500, "Internal error workers structure");
				return;
			}
		}
	}
}

static void rpc_pause_timer(rpc_t *rpc, void *ctx)
{
	int nargs;
	short sigfail;
	unsigned int i;
	unsigned int worker;
	str name;
	pt_timer_t *timer;

	nargs = rpc->scan(ctx, "S*.d", &name, &worker);
	if(nargs < 1) {
		rpc->fault(ctx, 400, "Invalid arguments (see help)");
		return;
	}
	if(name.len <= 0 || name.s == NULL) {
		rpc->fault(ctx, 400, "Invalid timer name");
		return;
	}

	timer = get_timer(&name, PTIMER_FROM_SHM);
	if(timer == NULL) {
		rpc->fault(ctx, 404, "Could not find timer %.*s", name.len, name.s);
		return;
	}

	sigfail = 0;
	if(nargs == 1) {
		for(i = 0; i < timer->nworkers; i++) {
			timer->workers[i].pstate = PTIMER_STATE_PAUSED;
			if(kill(timer->workers[i].pid, SIGCONT) != 0) {
				sigfail = 1;
			}
		}
	} else {
		timer->workers[worker].pstate = PTIMER_STATE_PAUSED;
		if(kill(timer->workers[worker].pid, SIGCONT) != 0) {
			sigfail = 1;
		}
	}
	if(sigfail > 0) {
		rpc->fault(ctx, 500, "Failed pausing some workers (signal failure)");
	}

	rpc->rpl_printf(ctx, "Successfully paused timer workers");
}

static void rpc_continue_timer(rpc_t *rpc, void *ctx)
{
	int nargs;
	short sigfail;
	unsigned int i;
	unsigned int worker;
	str name;
	pt_timer_t *timer;

	nargs = rpc->scan(ctx, "S*.d", &name, &worker);
	if(nargs < 1) {
		rpc->fault(ctx, 400, "Invalid arguments (see help)");
		return;
	}
	if(name.len <= 0 || name.s == NULL) {
		rpc->fault(ctx, 400, "Invalid timer name");
		return;
	}

	timer = get_timer(&name, PTIMER_FROM_SHM);
	if(timer == NULL) {
		rpc->fault(ctx, 404, "Could not find timer %.*s", name.len, name.s);
		return;
	}

	sigfail = 0;
	if(nargs == 1) {
		for(i = 0; i < timer->nworkers; i++) {
			timer->workers[i].pstate = timer->workers[i].cstate;
			if(kill(timer->workers[i].pid, SIGCONT) != 0) {
				sigfail = 1;
			}
		}
	} else {
		timer->workers[worker].pstate = timer->workers[worker].cstate;
		if(kill(timer->workers[worker].pid, SIGCONT) != 0) {
			sigfail = 1;
		}
	}
	if(sigfail > 0) {
		rpc->fault(ctx, 500, "Failed continuing some workers (signal failure)");
	}

	rpc->rpl_printf(ctx, "Successfully continued timer workers");
}

static void rpc_start_timer(rpc_t *rpc, void *ctx)
{
	int nargs;
	short sigfail;
	unsigned int i;
	unsigned int worker;
	str name;
	pt_timer_t *timer;

	nargs = rpc->scan(ctx, "S*.d", &name, &worker);
	if(nargs < 1) {
		rpc->fault(ctx, 400, "Invalid arguments (see help)");
		return;
	}
	if(name.len <= 0 || name.s == NULL) {
		rpc->fault(ctx, 400, "Invalid timer name");
		return;
	}

	timer = get_timer(&name, PTIMER_FROM_SHM);
	if(timer == NULL) {
		rpc->fault(ctx, 404, "Could not find timer %.*s", name.len, name.s);
		return;
	}

	sigfail = 0;
	if(nargs == 1) {
		for(i = 0; i < timer->nworkers; i++) {
			timer->workers[i].pstate = PTIMER_STATE_START;
			if(kill(timer->workers[i].pid, SIGCONT) != 0) {
				sigfail = 1;
			}
		}
	} else {
		timer->workers[worker].pstate = PTIMER_STATE_START;
		if(kill(timer->workers[worker].pid, SIGCONT) != 0) {
			sigfail = 1;
		}
	}
	if(sigfail > 0) {
		rpc->fault(ctx, 500, "Failed starting some workers (signal failure)");
	}

	rpc->rpl_printf(ctx, "Successfully jumped execution to start route");
}

static void rpc_loop_timer(rpc_t *rpc, void *ctx)
{
	int nargs;
	short sigfail;
	unsigned int i;
	unsigned int worker;
	str name;
	pt_timer_t *timer;

	nargs = rpc->scan(ctx, "S*.d", &name, &worker);
	if(nargs < 1) {
		rpc->fault(ctx, 400, "Invalid arguments (see help)");
		return;
	}
	if(name.len <= 0 || name.s == NULL) {
		rpc->fault(ctx, 400, "Invalid timer name");
		return;
	}

	timer = get_timer(&name, PTIMER_FROM_SHM);
	if(timer == NULL) {
		rpc->fault(ctx, 404, "Could not find timer %.*s", name.len, name.s);
		return;
	}

	sigfail = 0;
	if(nargs == 1) {
		for(i = 0; i < timer->nworkers; i++) {
			timer->workers[i].pstate = PTIMER_STATE_LOOP;
			if(kill(timer->workers[i].pid, SIGCONT) != 0) {
				sigfail = 1;
			}
		}
	} else {
		timer->workers[worker].pstate = PTIMER_STATE_LOOP;
		if(kill(timer->workers[worker].pid, SIGCONT) != 0) {
			sigfail = 1;
		}
	}
	if(sigfail > 0) {
		rpc->fault(ctx, 500, "Failed looping some workers (signal failure)");
	}

	rpc->rpl_printf(ctx, "Successfully jumped execution to loop route");
}

static void rpc_end_timer(rpc_t *rpc, void *ctx)
{
	int nargs;
	short sigfail;
	unsigned int i;
	unsigned int worker;
	str name;
	pt_timer_t *timer;

	nargs = rpc->scan(ctx, "S*.d", &name, &worker);
	if(nargs < 1) {
		rpc->fault(ctx, 400, "Invalid arguments (see help)");
		return;
	}
	if(name.len <= 0 || name.s == NULL) {
		rpc->fault(ctx, 400, "Invalid timer name");
		return;
	}

	timer = get_timer(&name, PTIMER_FROM_SHM);
	if(timer == NULL) {
		rpc->fault(ctx, 404, "Could not find timer %.*s", name.len, name.s);
		return;
	}

	sigfail = 0;
	if(nargs == 1) {
		for(i = 0; i < timer->nworkers; i++) {
			timer->workers[i].pstate = PTIMER_STATE_END;
			if(kill(timer->workers[i].pid, SIGCONT) != 0) {
				sigfail = 1;
			}
		}
	} else {
		timer->workers[worker].pstate = PTIMER_STATE_END;
		if(kill(timer->workers[worker].pid, SIGCONT) != 0) {
			sigfail = 1;
		}
	}
	if(sigfail > 0) {
		rpc->fault(ctx, 500, "Failed ending some workers (signal failure)");
	}

	rpc->rpl_printf(ctx, "Successfully jumped execution to end route");
}

/* module setup and teardown functions */
static int mod_init(void)
{
	unsigned int i;
	pt_timer_t *pit, *pnxt, *pnew;

	if(rpc_register_array(rpc_cmds) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	// no timers defined
	if(pkg_timer_list == NULL) {
		return 0;
	}

	/*
	 * routes associated here in case they are declared after modparam in cfg
	 * allows explicit verification of routes being declared when required
	 */
	for(pit = pkg_timer_list; pit; pit = pit->next) {
		if(pit->start != NULL) {
			pit->start->route = route_lookup(&main_rt, pit->start->name.s);
			if(pit->start->route == -1) {
				LM_ERR("invalid start route: %.*s\n", pit->start->name.len,
						pit->start->name.s);
				return -1;
			}
		}
		pit->loop->route = route_lookup(&main_rt, pit->loop->name.s);
		if(pit->loop->route == -1) {
			LM_ERR("invalid loop route: %.*s\n", pit->loop->name.len,
					pit->loop->name.s);
			return -1;
		}
		if(pit->end != NULL) {
			pit->end->route = route_lookup(&main_rt, pit->end->name.s);
			if(pit->end->route == -1) {
				LM_ERR("invalid end route: %.*s\n", pit->end->name.len,
						pit->end->name.s);
				return -1;
			}
		}
	}

	/* init faked sip msg */
	if(faked_msg_init() < 0) {
		LM_ERR("failed to init timer local sip msg\n");
		return -1;
	}

	/* copy timers to shmem and free pkgmem */
	pit = pkg_timer_list;
	while(pit != NULL) {
		pnxt = pit->next;

		pnew = (pt_timer_t *)shm_malloc(sizeof(pt_timer_t));
		if(pnew == NULL) {
			SHM_MEM_ERROR;
			return -1;
		}
		memset(pnew, 0, sizeof(pt_timer_t));

		if(shm_str_dup(&pnew->name, &pit->name) < 0) {
			LM_ERR("could not allocate shmem for name\n");
			return -1;
		}
		pkg_free(pit->name.s);

		pnew->type = pit->type;

		if(shm_str_dup(&pnew->interval, &pit->interval) < 0) {
			LM_ERR("could not allocate shmem for interval\n");
			return -1;
		}
		pkg_free(pit->interval.s);

		pnew->iprec = pit->iprec;
		pnew->ival = pit->ival;
		pnew->nworkers = pit->nworkers;
		pnew->ntasks = pit->ntasks;
		pnew->nslices = pit->nslices;

		if(pit->start != NULL) {
			pnew->start = (pt_route_t *)shm_malloc(sizeof(pt_route_t));
			if(pnew->start == NULL) {
				SHM_MEM_ERROR;
				return -1;
			}
			if(shm_str_dup(&pnew->start->name, &pit->start->name) < 0) {
				LM_ERR("could not allocate shmem for route name\n");
				return -1;
			}
			pnew->start->route = pit->start->route;
			pkg_free(pit->start->name.s);
			pkg_free(pit->start);
		}

		pnew->loop = (pt_route_t *)shm_malloc(sizeof(pt_route_t));
		if(pnew->loop == NULL) {
			SHM_MEM_ERROR;
			return -1;
		}
		if(shm_str_dup(&pnew->loop->name, &pit->loop->name) < 0) {
			LM_ERR("could not allocate shmem for route name\n");
			return -1;
		}
		pnew->loop->route = pit->loop->route;
		pkg_free(pit->loop->name.s);
		pkg_free(pit->loop);

		if(pit->end != NULL) {
			pnew->end = (pt_route_t *)shm_malloc(sizeof(pt_route_t));
			if(pnew->end == NULL) {
				SHM_MEM_ERROR;
				return -1;
			}
			if(shm_str_dup(&pnew->end->name, &pit->end->name) < 0) {
				LM_ERR("could not allocate shmem for route name\n");
				return -1;
			}
			pnew->end->route = pit->end->route;
			pkg_free(pit->end->name.s);
			pkg_free(pit->end);
		}

		pnew->workers =
				(pt_worker_t *)shm_malloc(sizeof(pt_worker_t) * pnew->nworkers);
		if(pnew->workers == NULL) {
			SHM_MEM_ERROR;
			return -1;
		}
		for(i = 0; i < pnew->nworkers; i++) {
			pnew->workers[i] = (pt_worker_t){.worker = i,
					.pid = -1,
					.ntasks = pnew->ntasks,
					.tasks = pnew->ntasks,
					.nslices = pnew->nslices,
					.slice = 0,
					.tnow = 0,
					.tnext = 0,
					.cstate = 0,
					.pstate = 0};
		}

		pnew->next = timer_list;
		timer_list = pnew;

		pkg_free(pit);
		pit = pnxt;
	}
	pkg_timer_list = NULL;

	/* register timers */
	for(pit = timer_list; pit; pit = pit->next) {
		if(register_timers(pit->nworkers) < 0) {
			LM_ERR("failed registering timer %.*s\n", pit->name.len,
					pit->name.s);
			return -1;
		}
	}

	return 0;
}

static int child_init(int rank)
{
	pt_timer_t *pit;
	unsigned int i;
	int pid;
	char si_desc[MAX_PT_DESC];
	timer_sleep_t *sleep_fn;
	timer_time_t *time_fn;

	if(rank != PROC_MAIN) {
		return 0;
	}

	if(timer_list == NULL) {
		return 0;
	}

	for(pit = timer_list; pit; pit = pit->next) {
		switch(pit->type) {
			case PTIMER_TYPE_BASIC:
				switch(pit->iprec) {
					case PTIMER_PRECISION_SEC:
						sleep_fn = sleep_sec;
						break;
					case PTIMER_PRECISION_MSEC:
						sleep_fn = sleep_msec;
						break;
					case PTIMER_PRECISION_USEC:
						sleep_fn = sleep_usec;
						break;
					default:
						LM_CRIT("logic error, invalid iprec [%d] should have "
								"been validated\n",
								pit->iprec);
						return -1;
				}
				for(i = 0; i < pit->nworkers; i++) {
					snprintf(si_desc, MAX_PT_DESC,
							"PTIMER EXEC child=%d timer=%.*s", i, pit->name.len,
							pit->name.s);
					pid = fork_timer(PROC_TIMER, si_desc, pit, i, basic_timer,
							sleep_fn, NULL);
					if(pid < 0) {
						LM_ERR("failed to start timer routine as process\n");
						return -1;
					}
					pit->workers[i].pid = pid;
				}
				break;
			case PTIMER_TYPE_SYNC:
				switch(pit->iprec) {
					case PTIMER_PRECISION_SEC:
					case PTIMER_PRECISION_MSEC:
					case PTIMER_PRECISION_USEC:
						sleep_fn = sleep_nsec, time_fn = time_nsec;
						break;
					default:
						LM_CRIT("logic error, invalid iprec [%d] should have "
								"been validated\n",
								pit->iprec);
						return -1;
				}
				for(i = 0; i < pit->nworkers; i++) {
					snprintf(si_desc, MAX_PT_DESC,
							"PTIMER EXEC child=%d timer=%.*s", i, pit->name.len,
							pit->name.s);
					pid = fork_timer(PROC_TIMER, si_desc, pit, i, sync_timer,
							sleep_fn, time_fn);
					if(pid < 0) {
						LM_ERR("failed to start timer routine as process\n");
						return -1;
					}
					pit->workers[i].pid = pid;
				}
				break;
			case PTIMER_TYPE_SLICE:
				switch(pit->iprec) {
					case PTIMER_PRECISION_SEC:
						sleep_fn = sleep_sec;
						break;
					case PTIMER_PRECISION_MSEC:
						sleep_fn = sleep_msec;
						break;
					case PTIMER_PRECISION_USEC:
						sleep_fn = sleep_usec;
						break;
					default:
						LM_CRIT("logic error, invalid iprec [%d] should have "
								"been validated\n",
								pit->iprec);
						return -1;
				}
				for(i = 0; i < pit->nworkers; i++) {
					snprintf(si_desc, MAX_PT_DESC,
							"PTIMER EXEC child=%d timer=%.*s", i, pit->name.len,
							pit->name.s);
					pid = fork_timer(PROC_TIMER, si_desc, pit, i, slice_timer,
							sleep_fn, NULL);
					if(pid < 0) {
						LM_ERR("failed to start timer routine as process\n");
						return -1;
					}
					pit->workers[i].pid = pid;
				}
				break;
			case PTIMER_TYPE_SSLICE:
				switch(pit->iprec) {
					case PTIMER_PRECISION_SEC:
					case PTIMER_PRECISION_MSEC:
					case PTIMER_PRECISION_USEC:
						sleep_fn = sleep_nsec, time_fn = time_nsec;
						break;
					default:
						LM_CRIT("logic error, invalid iprec [%d] should have "
								"been validated\n",
								pit->iprec);
						return -1;
				}
				for(i = 0; i < pit->nworkers; i++) {
					snprintf(si_desc, MAX_PT_DESC,
							"PTIMER EXEC child=%d timer=%.*s", i, pit->name.len,
							pit->name.s);
					pid = fork_timer(PROC_TIMER, si_desc, pit, i,
							synced_slice_timer, sleep_fn, time_fn);
					if(pid < 0) {
						LM_ERR("failed to start timer routine as process\n");
						return -1;
					}
					pit->workers[i].pid = pid;
				}
				break;
			default:
				LM_CRIT("logic error, invalid type [%d] should have been "
						"validated\n",
						pit->type);
				return -1;
		}
	}

	return 0;
}

static void destroy_mod(void)
{
	unsigned int i;
	pt_timer_t *pit, *pnxt;

	/*
	 *	TODO:	implement solution for cleaning up "nicer" (SIGUSR2 handler?)
	 *			we want to execute the end route and post script callbacks
	 *			would require syncing between parent and child as end route is
	 *			being executing, shm can not be freed until end route is done
	 */
	for(pit = timer_list; pit; pit = pit->next) {
		for(i = 0; i < pit->nworkers; i++) {
			kill(pit->workers[i].pid, SIGTERM);
		}
	}

	pit = timer_list;
	while(pit != NULL) {
		pnxt = pit->next;

		if(pit->name.s != NULL) {
			shm_free(pit->name.s);
		}
		if(pit->interval.s != NULL) {
			shm_free(pit->name.s);
		}
		if(pit->start != NULL) {
			shm_free(pit->start->name.s);
			shm_free(pit->start);
		}
		if(pit->loop != NULL) {
			shm_free(pit->loop->name.s);
			shm_free(pit->loop);
		}
		if(pit->end != NULL) {
			shm_free(pit->end->name.s);
			shm_free(pit->end);
		}
		if(pit->workers != NULL) {
			shm_free(pit->workers);
		}

		shm_free(pit);
		pit = pnxt;
	}
	timer_list = NULL;
}

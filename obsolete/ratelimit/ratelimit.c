/*
 * $Id$
 *
 * ratelimit module
 *
 * Copyright (C) 2006 Hendrik Scholz <hscholz@raisdorf.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>

#include "../../mem/shm_mem.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../timer.h"
#include "../../ut.h"

MODULE_VERSION

/* use RED queuing algorithm? */
#define RL_WITH_RED	1
/* timer interval length in seconds, tunable via modparam */
#define RL_TIMER_INTERVAL 10

#ifndef rpc_lf
#define rpc_lf(rpc, c)  rpc->add(c, "s","")
#endif

/* globally visible parameters tunable via modparam and RPC interface */
int *invite_limit, *register_limit, *subscribe_limit = NULL;

/* storage for initial modparam values */
int invite_limit_mp = 0;
int register_limit_mp = 0;
int subscribe_limit_mp = 0;
int timer_interval = RL_TIMER_INTERVAL;	/* in seconds */

/* internal counters */
int *invite_counter = NULL;
int *register_counter = NULL;
int *subscribe_counter = NULL;

#if defined(RL_WITH_RED)
/* load levels for Random Early Detection algorithm */
int *invite_load = NULL;
int *register_load = NULL;
int *subscribe_load = NULL;
#endif

/** module functions */
static int mod_init(void);
static int child_init(int);
static int rl_check(struct sip_msg*, char *, char *);
#if defined (RL_WITH_RED)
static int rl_limit_check(int, int, int);
#else
static int rl_limit_check(int, int);
#endif
static void timer(unsigned int, void *);
static void destroy(void);

static rpc_export_t rpc_methods[];

static cmd_export_t cmds[]={
	{"rl_check", rl_check, 0, 0, REQUEST_ROUTE},
	{0,0,0,0,0}
};

static param_export_t params[]={
	{"invite_limit",	PARAM_INT, &invite_limit_mp},
	{"register_limit",	PARAM_INT, &register_limit_mp},
	{"subscribe_limit",	PARAM_INT, &subscribe_limit_mp},
	{"timer_interval",	PARAM_INT, &timer_interval},
	{0,0,0}
};

/** module exports */
struct module_exports exports= {
	"ratelimit",
	cmds,
	rpc_methods,
	params,	
	mod_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) destroy,
	0,
	child_init  /* per-child init function */
};

/* initialize ratelimit module */
static int mod_init(void)
{
	DBG("RATELIMIT: initializing ...\n");

	/* register timer to reset counters */
	if (register_timer(timer, 0, timer_interval) < 0) {
		LOG(L_ERR, "RATELIMIT:ERROR: could not register timer function\n");
		return -1;
	}

	invite_counter = shm_malloc(sizeof(int));
	register_counter = shm_malloc(sizeof(int));
	subscribe_counter = shm_malloc(sizeof(int));
	if (!invite_counter || !register_counter || !subscribe_counter) {
		LOG(L_ERR, "RATELIMIT:ERROR: no memory for counters\n");
		return -1;
	}
	*invite_counter = 0;
	*register_counter = 0;
	*subscribe_counter = 0;

	invite_limit = shm_malloc(sizeof(int));
	register_limit = shm_malloc(sizeof(int));
	subscribe_limit = shm_malloc(sizeof(int));
	if (!invite_limit || !register_limit || !subscribe_limit) {
		LOG(L_ERR, "RATELIMIT:ERROR: no memory for limit settings\n");
		return -1;
	}
	/* obtain limits from modparam */
	*invite_limit = invite_limit_mp;
	*register_limit = register_limit_mp;
	*subscribe_limit = subscribe_limit_mp;


#if defined (RL_WITH_RED)
	/* these are only needed when using RED */
	invite_load = shm_malloc(sizeof(int));
	register_load = shm_malloc(sizeof(int));
	subscribe_load = shm_malloc(sizeof(int));
	if (!invite_load || !register_load || !subscribe_load) {
		LOG(L_ERR, "RATELIMIT:ERROR: no memory for load levels\n");
		return -1;
	}
	*invite_load = -1; /* -1 = first run identifier */
	*register_load = -1;
	*subscribe_load = -1;
#endif

	return 0;
}

/* generic SER module functions */
static int child_init(int rank)
{
	DBG("RATELIMIT:init_child #%d / pid <%d>\n", rank, getpid());
	return 0;
}
static void destroy(void)
{
	DBG("RATELIMIT: destroy module ...\n");
}

/* ratelimit check 
 *
 * return values:
 *  -1: over limit (aka too many messages of that request type)
 *   0: internal error (not a request)
 *   1: within limit (let message through)
 */

static int rl_check(struct sip_msg* msg, char *_foo, char *_bar) {

	DBG("RATELIMIT:rl_check:invoked\n");

	if (msg->first_line.type != SIP_REQUEST) {
		DBG("RATELIMIT:rl_check:not a request\n");
		return 0;
	}

	if (msg->first_line.u.request.method_value == METHOD_INVITE) {
		if (*invite_limit == 0) 
			return 1;
		*invite_counter = *invite_counter + 1;
#if defined(RL_WITH_RED)
		return rl_limit_check(*invite_counter, *invite_limit, *invite_load);
#else
		return rl_limit_check(*invite_counter, *invite_limit);
#endif
	} else if (msg->first_line.u.request.method_value == METHOD_REGISTER) {
		if (*register_limit == 0) 
			return 1;
		*register_counter = *register_counter + 1;
#if defined(RL_WITH_RED)
		return rl_limit_check(*register_counter, *register_limit,
			*register_load);
#else
		return rl_limit_check(*register_counter, *register_limit);
#endif
	} else if (msg->first_line.u.request.method_value == METHOD_SUBSCRIBE) {
		if (*subscribe_limit == 0) 
			return 1;
		*subscribe_counter = *subscribe_counter + 1;
#if defined(RL_WITH_RED)
		return rl_limit_check(*subscribe_counter, *subscribe_limit,
			*subscribe_load);
#else
		return rl_limit_check(*subscribe_counter, *subscribe_limit);
#endif
	} else {
		return 0;
	}
	return -1;
}


/* timer housekeeping, invoked each timer interval to reset counters */
static void timer(unsigned int ticks, void *param) {
    DBG("RATELIMIT:timer:invoked\n");

#if defined(RL_WITH_RED)
	/* calculate load levels for RED */
	if (*invite_limit > 0) {
		if (*invite_counter < *invite_limit)
			*invite_load = 0;
		else 
			*invite_load = (int) (*invite_counter / *invite_limit);
	}
	if (*register_limit > 0) {
		if (*register_counter < *register_limit)
			*register_load = 0;
		else 
			*register_load = (int) (*register_counter / *register_limit);
	}
	if (*subscribe_limit > 0) {
		if (*subscribe_counter < *subscribe_limit)
			*subscribe_load = 0;
		else 
			*subscribe_load = (int) (*subscribe_counter / *subscribe_limit);
	}
#endif

	/* clear counters */
	if (*invite_limit > 0) *invite_counter = 0;
	if (*register_limit > 0) *register_counter = 0;
	if (*subscribe_limit > 0) *subscribe_counter = 0;
}

#if defined(RL_WITH_RED)

/* rl_limit_check() RED implementation
 *
 * RED (Random Early Detection) is a queue management algorithm that tries
 * to counter the usual tail drop issues as seen in the trivial implementation
 * below.
 *
 * We monitor the load level and start dropping messages early on to achieve
 * an even utilization throughout the timer interval.
 *
 * The algorithm has no load indication during the first interval so we revert
 * to 'tail drop' for a simple start.
 *
 */

static int rl_limit_check(int cnt, int limit, int load) {

	DBG("RATELIMIT:rl_limit_check: invoked\n");

	/* first run? */
	if (load == -1) return (cnt > limit) ? -1 : 1;

	/* low load, no drops */
	if (load <= 1) return 1;

	/* RED implementation, every load'th packet is let through */
	return (!(cnt % load)) ? 1 : -1;
}

#else

/*
 * rl_limit_check() trivial implementation (aka tail drop)
 *
 * check if counter is above limit and in that case return -1.
 * Returns 1 if still within limit.
 *
 * Caveats:
 *
 *  If the timer interval is too large this 'algorithm' might cause
 *  end device synchronization and bad traffic patterns, i.e. full traffic/load
 *  in the beginning of the timer interval and no traffic at all once
 *  the limit has been reached for the remaining interval.
 *
 */

static int rl_limit_check(int cnt, int limit) {
	DBG("RATELIMIT:rl_limit_check: invoked\n");
	return (cnt > limit) ? -1 : 1;
}

#endif /* queue management algorithm selection */

/*
 * RPC functions
 *
 * rpc_stats() dumps the current config/statistics
 * rpc_{invite|register|subscribe}() set the limits
 * rpc_timer() sets the timer interval length
 *
 */

/* rpc function documentation */
static const char *rpc_stats_doc[2] = {
	"Print ratelimit statistics", 
	0
};

static const char *rpc_invite_doc[2] = {
	"Set INVITEs per timer interval limit",
	0
};
static const char *rpc_register_doc[2] = {
	"Set REGISTERs per timer interval limit",
	0
};
static const char *rpc_subscribe_doc[2] = {
	"Set SUBSCRIBEs per timer interval limit",
	0
};
static const char *rpc_timer_doc[2] = {
	"Set the ratelimit timer_interval length",
	0
};

/* rpc function implementations */
static void rpc_stats(rpc_t *rpc, void *c) {

#if defined(RL_WITH_RED)
	if (rpc->printf(c, "   INVITE: %d/%d (drop rate: %d)", *invite_counter,
		*invite_limit, *invite_load) < 0) return;
	rpc_lf(rpc, c);
	if (rpc->printf(c, " REGISTER: %d/%d (drop rate: %d)", *register_counter,
		*register_limit, *register_load) < 0) return;
	rpc_lf(rpc, c);
	if (rpc->printf(c, "SUBSCRIBE: %d/%d (drop rate: %d)", *subscribe_counter,
		*subscribe_limit, *subscribe_load) < 0) return;
	rpc_lf(rpc, c);
#else
	if (rpc->printf(c, "   INVITE: %d/%d", *invite_counter,
		*invite_limit) < 0) return;
	rpc_lf(rpc, c);
	if (rpc->printf(c, " REGISTER: %d/%d", *register_counter,
		*register_limit) < 0) return;
	rpc_lf(rpc, c);
	if (rpc->printf(c, "SUBSCRIBE: %d/%d", *subscribe_counter,
		*subscribe_limit) < 0) return;
	rpc_lf(rpc, c);
#endif

}
static void rpc_invite(rpc_t *rpc, void *c) {

	int limit;
	if (rpc->scan(c, "d", &limit) < 1) {
		rpc->fault(c, 400, "Limit expected");
		return;
	}
	if (limit < 0) {
		rpc->fault(c, 400, "limit must be >= 0 (0 = unlimited)");
		return;
	}
	DBG("RATELIMIT:setting INVITE limit to %d messages\n", limit);
	*invite_limit = limit;
}
static void rpc_register(rpc_t *rpc, void *c) {

	int limit;
	if (rpc->scan(c, "d", &limit) < 1) {
		rpc->fault(c, 400, "Limit expected");
		return;
	}
	if (limit < 0) {
		rpc->fault(c, 400, "limit must be >= 0 (0 = unlimited)");
		return;
	}
	DBG("RATELIMIT:setting REGISTER limit to %d messages\n", limit);
	*register_limit = limit;
}
static void rpc_subscribe(rpc_t *rpc, void *c) {

	int limit;
	if (rpc->scan(c, "d", &limit) < 1) {
		rpc->fault(c, 400, "Limit expected");
		return;
	}
	if (limit < 0) {
		rpc->fault(c, 400, "limit must be >= 0 (0 = unlimited)");
		return;
	}
	DBG("RATELIMIT:setting SUBSCRIBE limit to %d messages\n", limit);
	*subscribe_limit = limit;
}
static void rpc_timer(rpc_t *rpc, void *c) {
	rpc->fault(c, 400, "Not yet implemented");
}

static rpc_export_t rpc_methods[] = {
	{"rl.stats",			rpc_stats,		rpc_stats_doc,		0},
	{"rl.invite_limit",		rpc_invite,		rpc_invite_doc,		0},
	{"rl.register_limit",	rpc_register,	rpc_register_doc,	0},
	{"rl.subscribe_limit",	rpc_subscribe,	rpc_subscribe_doc,	0},
	{"rl.timer_interval",	rpc_timer,		rpc_timer_doc,	0},
	{0, 0, 0, 0}
};

/**
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/pvar.h"
#include "../../core/timer_proc.h"
#include "../../core/route_struct.h"
#include "../../core/async_task.h"
#include "../../core/kemi.h"
#include "../../modules/tm/tm_load.h"

#include "async_sleep.h"

MODULE_VERSION

static int async_workers = 1;

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int w_async_sleep(sip_msg_t *msg, char *sec, char *str2);
static int fixup_async_sleep(void **param, int param_no);
static int w_async_route(sip_msg_t *msg, char *rt, char *sec);
static int fixup_async_route(void **param, int param_no);
static int w_async_task_route(sip_msg_t *msg, char *rt, char *p2);
static int fixup_async_task_route(void **param, int param_no);

/* tm */
struct tm_binds tmb;

/* clang-format off */
static cmd_export_t cmds[]={
	{"async_route", (cmd_function)w_async_route, 2, fixup_async_route,
		0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"async_sleep", (cmd_function)w_async_sleep, 1, fixup_async_sleep,
		0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"async_task_route", (cmd_function)w_async_task_route, 1, fixup_async_task_route,
		0, REQUEST_ROUTE|FAILURE_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"workers",     INT_PARAM,   &async_workers},
	{0, 0, 0}
};

struct module_exports exports = {
	"async",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,
	0,              /* exported MI functions */
	0,              /* exported pseudo-variables */
	0,              /* extra processes */
	mod_init,       /* module initialization function */
	0,              /* response function */
	mod_destroy,    /* destroy function */
	child_init      /* per child init function */
};
/* clang-format on */


/**
 * init module function
 */
static int mod_init(void)
{
	if(load_tm_api(&tmb) == -1) {
		LM_ERR("cannot load the TM-functions. Missing TM module?\n");
		return -1;
	}

	if(async_workers <= 0)
		return 0;

	if(async_init_timer_list() < 0) {
		LM_ERR("cannot initialize internal structure\n");
		return -1;
	}

	register_basic_timers(async_workers);

	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	int i;

	if(rank != PROC_MAIN)
		return 0;

	if(async_workers <= 0)
		return 0;

	for(i = 0; i < async_workers; i++) {
		if(fork_basic_timer(PROC_TIMER, "ASYNC MOD TIMER", 1 /*socks flag*/,
				   async_timer_exec, NULL, 1 /*sec*/)
				< 0) {
			LM_ERR("failed to register timer routine as process (%d)\n", i);
			return -1; /* error */
		}
	}

	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	async_destroy_timer_list();
}

/**
 *
 */
static int w_async_sleep(sip_msg_t *msg, char *sec, char *str2)
{
	int s;
	async_param_t *ap;

	if(msg == NULL)
		return -1;

	if(async_workers <= 0) {
		LM_ERR("no async mod timer workers (modparam missing?)\n");
		return -1;
	}

	ap = (async_param_t *)sec;
	if(fixup_get_ivalue(msg, ap->pinterval, &s) != 0) {
		LM_ERR("no async sleep time value\n");
		return -1;
	}
	if(ap->type == 0) {
		if(ap->u.paction == NULL || ap->u.paction->next == NULL) {
			LM_ERR("cannot be executed as last action in a route block\n");
			return -1;
		}
		if(async_sleep(msg, s, ap->u.paction->next, NULL) < 0)
			return -1;
		/* force exit in config */
		return 0;
	}

	return -1;
}

/**
 *
 */
static int fixup_async_sleep(void **param, int param_no)
{
	async_param_t *ap;
	if(param_no != 1)
		return 0;
	ap = (async_param_t *)pkg_malloc(sizeof(async_param_t));
	if(ap == NULL) {
		LM_ERR("no more pkg memory available\n");
		return -1;
	}
	memset(ap, 0, sizeof(async_param_t));
	ap->u.paction = get_action_from_param(param, param_no);
	if(fixup_igp_null(param, param_no) < 0) {
		pkg_free(ap);
		return -1;
	}
	ap->pinterval = (gparam_t *)(*param);
	*param = (void *)ap;
	return 0;
}

/**
 *
 */
int ki_async_route(sip_msg_t *msg, str *rn, int s)
{
	cfg_action_t *act = NULL;
	int ri;
	sr_kemi_eng_t *keng = NULL;

	keng = sr_kemi_eng_get();
	if(keng == NULL) {
		ri = route_lookup(&main_rt, rn->s);
		if(ri >= 0) {
			act = main_rt.rlist[ri];
			if(act == NULL) {
				LM_ERR("empty action lists in route block [%.*s]\n", rn->len,
						rn->s);
				return -1;
			}
		} else {
			LM_ERR("route block not found: %.*s\n", rn->len, rn->s);
			return -1;
		}
	}

	if(async_sleep(msg, s, act, rn) < 0)
		return -1;
	/* force exit in config */
	return 0;
}

/**
 *
 */
static int w_async_route(sip_msg_t *msg, char *rt, char *sec)
{
	int s;
	str rn;

	if(msg == NULL)
		return -1;

	if(async_workers <= 0) {
		LM_ERR("no async mod timer workers\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t *)rt, &rn) != 0) {
		LM_ERR("no async route block name\n");
		return -1;
	}

	if(fixup_get_ivalue(msg, (gparam_t *)sec, &s) != 0) {
		LM_ERR("no async interval value\n");
		return -1;
	}
	return ki_async_route(msg, &rn, s);
}

/**
 *
 */
static int fixup_async_route(void **param, int param_no)
{
	if(param_no == 1) {
		if(fixup_spve_null(param, 1) < 0)
			return -1;
		return 0;
	} else if(param_no == 2) {
		if(fixup_igp_null(param, 1) < 0)
			return -1;
	}
	return 0;
}

/**
 *
 */
int ki_async_task_route(sip_msg_t *msg, str *rn)
{
	cfg_action_t *act = NULL;
	int ri;
	sr_kemi_eng_t *keng = NULL;

	keng = sr_kemi_eng_get();
	if(keng == NULL) {
		ri = route_lookup(&main_rt, rn->s);
		if(ri >= 0) {
			act = main_rt.rlist[ri];
			if(act == NULL) {
				LM_ERR("empty action lists in route block [%.*s]\n", rn->len,
						rn->s);
				return -1;
			}
		} else {
			LM_ERR("route block not found: %.*s\n", rn->len, rn->s);
			return -1;
		}
	}

	if(async_send_task(msg, act, rn) < 0)
		return -1;
	/* force exit in config */
	return 0;
}

/**
 *
 */
static int w_async_task_route(sip_msg_t *msg, char *rt, char *sec)
{
	str rn;

	if(msg == NULL)
		return -1;

	if(fixup_get_svalue(msg, (gparam_t *)rt, &rn) != 0) {
		LM_ERR("no async route block name\n");
		return -1;
	}
	return ki_async_task_route(msg, &rn);
}

/**
 *
 */
static int fixup_async_task_route(void **param, int param_no)
{
	if(!async_task_initialized()) {
		LM_ERR("async task framework was not initialized"
			   " - set async_workers parameter in core\n");
		return -1;
	}

	if(param_no == 1) {
		if(fixup_spve_null(param, 1) < 0)
			return -1;
		return 0;
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_async_exports[] = {
	{ str_init("async"), str_init("route"),
		SR_KEMIP_INT, ki_async_route,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("async"), str_init("task_route"),
		SR_KEMIP_INT, ki_async_task_route,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_async_exports);
	return 0;
}

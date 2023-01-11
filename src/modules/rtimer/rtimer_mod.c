/**
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../core/sr_module.h"
#include "../../core/timer.h"
#include "../../core/dprint.h"
#include "../../core/route.h"
#include "../../core/receive.h"
#include "../../core/action.h"
#include "../../core/socket_info.h"
#include "../../core/dset.h"
#include "../../core/pt.h"
#include "../../core/timer_proc.h"
#include "../../core/script_cb.h"
#include "../../core/parser/parse_param.h"
#include "../../core/fmsg.h"
#include "../../core/kemi.h"


MODULE_VERSION

#define RTIMER_ROUTE_NAME_SIZE  64

typedef struct _stm_route {
	str timer;
	unsigned int route;
	char route_name_buf[RTIMER_ROUTE_NAME_SIZE];
	str route_name;
	struct _stm_route *next;
} stm_route_t;

typedef struct _stm_timer {
	str name;
	unsigned int mode;
	unsigned int flags;
	unsigned int interval;
	stm_route_t *rt;
	struct _stm_timer *next;
} stm_timer_t;

#define RTIMER_INTERVAL_USEC	(1<<0)

stm_timer_t *_stm_list = NULL;

/** module functions */
static int mod_init(void);
static int child_init(int);

int stm_t_param(modparam_t type, void* val);
int stm_e_param(modparam_t type, void* val);
void stm_timer_exec(unsigned int ticks, int worker, void *param);
void stm_main_timer_exec(unsigned int ticks, void *param);
int stm_get_worker(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);

static int default_interval = 120;

static pv_export_t rtimer_pvs[] = {
	{{"rtimer_worker", (sizeof("rtimer_worker")-1)}, PVT_OTHER, stm_get_worker, 0,	0, 0, 0, 0},
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static param_export_t params[]={
	{"default_interval",       INT_PARAM, &default_interval},
	{"timer",             PARAM_STRING|USE_FUNC_PARAM, (void*)stm_t_param},
	{"exec",              PARAM_STRING|USE_FUNC_PARAM, (void*)stm_e_param},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"rtimer",
	DEFAULT_DLFLAGS, /* dlopen flags */
	0,
	params,
	0,           /* exported RPC methods */
	rtimer_pvs,  /* exported pseudo-variables */
	0,
	mod_init,    /* module initialization function */
	child_init,  /* per-child init function */
	0
};


/**
 * init module function
 */
static int mod_init(void)
{
	stm_timer_t *it;
	if(_stm_list==NULL)
		return 0;

	/* init faked sip msg */
	if(faked_msg_init()<0)
	{
		LM_ERR("failed to init timer local sip msg\n");
		return -1;
	}

	/* register timers */
	it = _stm_list;
	while(it)
	{
		if(it->mode==0)
		{
			if(register_timer(stm_main_timer_exec, (void*)it, it->interval)<0)
			{
				LM_ERR("failed to register timer function\n");
				return -1;
			}
		} else {
			register_basic_timers(it->mode);
		}
		it = it->next;
	}

	return 0;
}

static int child_init(int rank)
{
	stm_timer_t *it;
	int i;
	char si_desc[MAX_PT_DESC];

	if(_stm_list==NULL)
		return 0;

	if (rank!=PROC_MAIN)
		return 0;

	it = _stm_list;
	while(it)
	{
		for(i=0; i<it->mode; i++)
		{
			snprintf(si_desc, MAX_PT_DESC, "RTIMER EXEC child=%d timer=%.*s",
			         i, it->name.len, it->name.s);
			if(it->flags & RTIMER_INTERVAL_USEC)
			{
				if(fork_basic_utimer_w(PROC_TIMER, si_desc, 1 /*socks flag*/,
								stm_timer_exec, i, (void*)it, it->interval
								/*usec*/)<0) {
					LM_ERR("failed to start utimer routine as process\n");
					return -1; /* error */
				}
			} else {
				if(fork_basic_timer_w(PROC_TIMER, si_desc, 1 /*socks flag*/,
								stm_timer_exec, i, (void*)it, it->interval
								/*sec*/)<0) {
					LM_ERR("failed to start timer routine as process\n");
					return -1; /* error */
				}
			}
		}
		it = it->next;
	}

	return 0;
}

int rt_worker = 0;

void stm_main_timer_exec(unsigned int ticks, void *param)
{
	stm_timer_exec(ticks, 0, param);
}

void stm_timer_exec(unsigned int ticks, int worker, void *param)
{
	stm_timer_t *it;
	stm_route_t *rt;
	sip_msg_t *fmsg;
	sr_kemi_eng_t *keng = NULL;
	str evname = str_init("rtimer");
	rt_worker = worker;

	if(param==NULL)
		return;
	it = (stm_timer_t*)param;
	if(it->rt==NULL)
		return;

	for(rt=it->rt; rt; rt=rt->next)
	{
		fmsg = faked_msg_next();
		if (exec_pre_script_cb(fmsg, REQUEST_CB_TYPE)==0 )
			continue; /* drop the request */
		set_route_type(REQUEST_ROUTE);
		keng = sr_kemi_eng_get();
		if(keng==NULL) {
			run_top_route(main_rt.rlist[rt->route], fmsg, 0);
		} else {
			if(sr_kemi_route(keng, fmsg, EVENT_ROUTE, &rt->route_name, &evname)<0) {
				LM_ERR("error running event route kemi callback [%.*s]\n",
						rt->route_name.len, rt->route_name.s);
			}
		}
		exec_post_script_cb(fmsg, REQUEST_CB_TYPE);
		ksr_msg_env_reset();
	}
}

int stm_t_param(modparam_t type, void *val)
{
	param_t* params_list = NULL;
	param_hooks_t phooks;
	param_t *pit=NULL;
	stm_timer_t tmp;
	stm_timer_t *nt;
	str s;

	if(val==NULL)
		return -1;
	s.s = (char*)val;
	s.len = strlen(s.s);
	if(s.s[s.len-1]==';')
		s.len--;
	if (parse_params(&s, CLASS_ANY, &phooks, &params_list)<0)
		return -1;
	memset(&tmp, 0, sizeof(stm_timer_t));
	for (pit = params_list; pit; pit=pit->next)
	{
		if (pit->name.len==4
				&& strncasecmp(pit->name.s, "name", 4)==0) {
			tmp.name = pit->body;
		} else if(pit->name.len==4
				&& strncasecmp(pit->name.s, "mode", 4)==0) {
			if(tmp.mode==0) {
				if (str2int(&pit->body, &tmp.mode) < 0) {
					LM_ERR("invalid mode: %.*s\n", pit->body.len, pit->body.s);
					return -1;
				}
			}
		}  else if(pit->name.len==8
				&& strncasecmp(pit->name.s, "interval", 8)==0) {
			if(pit->body.s[pit->body.len-1]=='u'
					|| pit->body.s[pit->body.len-1]=='U') {
				pit->body.len--;
				tmp.flags |= RTIMER_INTERVAL_USEC;
				if (tmp.mode==0) {
					tmp.mode = 1;
				}
			}
			if (str2int(&pit->body, &tmp.interval) < 0) {
				LM_ERR("invalid interval: %.*s\n", pit->body.len, pit->body.s);
					return -1;
			}
		}
	}
	if(tmp.name.s==NULL)
	{
		LM_ERR("invalid timer name\n");
		free_params(params_list);
		return -1;
	}
	/* check for same timer */
	nt = _stm_list;
	while(nt) {
		if(nt->name.len==tmp.name.len
				&& strncasecmp(nt->name.s, tmp.name.s, tmp.name.len)==0)
			break;
		nt = nt->next;
	}
	if(nt!=NULL)
	{
		LM_ERR("duplicate timer with same name: %.*s\n",
				tmp.name.len, tmp.name.s);
		free_params(params_list);
		return -1;
	}
	if(tmp.interval==0)
		tmp.interval = default_interval;

	nt = (stm_timer_t*)pkg_malloc(sizeof(stm_timer_t));
	if(nt==0)
	{
		LM_ERR("no more pkg memory\n");
		free_params(params_list);
		return -1;
	}
	memcpy(nt, &tmp, sizeof(stm_timer_t));
	nt->next = _stm_list;
	_stm_list = nt;
	free_params(params_list);
	LM_INFO("created rtimer name=%.*s interval=%d mode=%d\n", tmp.name.len, tmp.name.s, tmp.interval, tmp.mode);
	return 0;
}

int stm_e_param(modparam_t type, void *val)
{
	param_t* params_list = NULL;
	param_hooks_t phooks;
	param_t *pit=NULL;
	stm_route_t tmp;
	stm_route_t *rt;
	stm_timer_t *nt;
	str s;
	char c;

	if(val==NULL)
		return -1;
	s.s = (char*)val;
	s.len = strlen(s.s);
	if(s.s[s.len-1]==';')
		s.len--;
	if (parse_params(&s, CLASS_ANY, &phooks, &params_list)<0)
		return -1;
	memset(&tmp, 0, sizeof(stm_route_t));
	for (pit = params_list; pit; pit=pit->next)
	{
		if (pit->name.len==5
				&& strncasecmp(pit->name.s, "timer", 5)==0) {
			tmp.timer = pit->body;
		} else if(pit->name.len==5
				&& strncasecmp(pit->name.s, "route", 5)==0) {
			s = pit->body;
		}
	}
	if(tmp.timer.s==NULL)
	{
		LM_ERR("invalid timer name\n");
		free_params(params_list);
		return -1;
	}
	/* get the timer */
	nt = _stm_list;
	while(nt) {
		if(nt->name.len==tmp.timer.len
				&& strncasecmp(nt->name.s, tmp.timer.s, tmp.timer.len)==0)
			break;
		nt = nt->next;
	}
	if(nt==NULL)
	{
		LM_ERR("timer not found - name: %.*s\n",
				tmp.timer.len, tmp.timer.s);
		free_params(params_list);
		return -1;
	}
	c = s.s[s.len];
	s.s[s.len] = '\0';
	if(s.len>=RTIMER_ROUTE_NAME_SIZE-1) {
		LM_ERR("route block name is too long [%.*s] (%d)\n", s.len, s.s, s.len);
		free_params(params_list);
		return -1;
	}
	tmp.route = route_get(&main_rt, s.s);
	memcpy(tmp.route_name_buf, s.s, s.len);
	tmp.route_name_buf[s.len] = '\0';
	tmp.route_name.s = tmp.route_name_buf;
	tmp.route_name.len = s.len;
	s.s[s.len] = c;
	if(tmp.route == -1)
	{
		LM_ERR("invalid route: %.*s\n", s.len, s.s);
		free_params(params_list);
		return -1;
	}

	rt = (stm_route_t*)pkg_malloc(sizeof(stm_route_t));
	if(rt==0)
	{
		LM_ERR("no more pkg memory\n");
		free_params(params_list);
		return -1;
	}
	memcpy(rt, &tmp, sizeof(stm_route_t));
	rt->route_name.s = rt->route_name_buf;
	rt->next = nt->rt;
	nt->rt = rt;
	free_params(params_list);
	return 0;
}

int stm_get_worker(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	return pv_get_sintval(msg, param, res, rt_worker);
}

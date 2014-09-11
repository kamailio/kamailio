/**
 * $Id$
 *
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

#include "../../sr_module.h"
#include "../../timer.h"
#include "../../dprint.h"
#include "../../route.h"
#include "../../receive.h"
#include "../../action.h"
#include "../../socket_info.h"
#include "../../dset.h"
#include "../../pt.h"
#include "../../usr_avp.h"
#include "../../xavp.h"
#include "../../timer_proc.h"
#include "../../script_cb.h"
#include "../../parser/parse_param.h"
#include "../../lib/kcore/faked_msg.h"


MODULE_VERSION

typedef struct _stm_route {
	str timer;
	unsigned int route;
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
void stm_timer_exec(unsigned int ticks, void *param);


static param_export_t params[]={
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
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	0,
	child_init  /* per-child init function */
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
			if(register_timer(stm_timer_exec, (void*)it, it->interval)<0)
			{
				LM_ERR("failed to register timer function\n");
				return -1;
			}
		} else {
			register_basic_timers(1);
		}
		it = it->next;
	}

	return 0;
}

static int child_init(int rank)
{
	stm_timer_t *it;
	if(_stm_list==NULL)
		return 0;

	if (rank!=PROC_MAIN)
		return 0;

	it = _stm_list;
	while(it)
	{
		if(it->mode!=0)
		{
			if(it->flags & RTIMER_INTERVAL_USEC)
			{
				if(fork_basic_utimer(PROC_TIMER, "RTIMER USEC EXEC", 1 /*socks flag*/,
								stm_timer_exec, (void*)it, it->interval
								/*usec*/)<0) {
					LM_ERR("failed to start utimer routine as process\n");
					return -1; /* error */
				}
			} else {
				if(fork_basic_timer(PROC_TIMER, "RTIMER SEC EXEC", 1 /*socks flag*/,
								stm_timer_exec, (void*)it, it->interval
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

void stm_timer_exec(unsigned int ticks, void *param)
{
	stm_timer_t *it;
	stm_route_t *rt;
	sip_msg_t *fmsg;

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
		run_top_route(main_rt.rlist[rt->route], fmsg, 0);
		exec_post_script_cb(fmsg, REQUEST_CB_TYPE);
		reset_avps();
#ifdef WITH_XAVP
		xavp_reset_list();
#endif
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
			if(tmp.mode==0)
				str2int(&pit->body, &tmp.mode);
		}  else if(pit->name.len==8
				&& strncasecmp(pit->name.s, "interval", 8)==0) {
			if(pit->body.s[pit->body.len-1]=='u'
					|| pit->body.s[pit->body.len-1]=='U') {
				pit->body.len--;
				tmp.flags |= RTIMER_INTERVAL_USEC;
				tmp.mode = 1;
			}
			str2int(&pit->body, &tmp.interval);
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
		tmp.interval = 120;

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
	tmp.route = route_get(&main_rt, s.s);
	s.s[s.len] = c;
	if(tmp.route == -1)
	{
		LM_ERR("invalid route: %.*s\n",
				s.len, s.s);
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
	rt->next = nt->rt;
	nt->rt = rt;
	free_params(params_list);
	return 0;
}


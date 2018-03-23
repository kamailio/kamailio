/**
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
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
#include <unistd.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/route.h"
#include "../../core/receive.h"
#include "../../core/action.h"
#include "../../core/pt.h"
#include "../../core/ut.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/parser/parse_param.h"
#include "../../core/kemi.h"
#include "../../core/fmsg.h"


MODULE_VERSION

typedef struct evrexec_task {
	str ename;
	int rtid;
	unsigned int wait;
	unsigned int workers;
	struct evrexec_task *next;
} evrexec_task_t;

evrexec_task_t *_evrexec_list = NULL;

/** module functions */
static int mod_init(void);
static int child_init(int);

int evrexec_param(modparam_t type, void* val);
void evrexec_process(evrexec_task_t *it, int idx);


static param_export_t params[]={
	{"exec",  PARAM_STRING|USE_FUNC_PARAM, (void*)evrexec_param},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"evrexec",
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
	evrexec_task_t *it;
	if(_evrexec_list==NULL)
		return 0;

	/* init faked sip msg */
	if(faked_msg_init()<0) {
		LM_ERR("failed to init evrexec local sip msg\n");
		return -1;
	}

	/* register additional processes */
	it = _evrexec_list;
	while(it) {
		register_procs(it->workers);
		it = it->next;
	}

	return 0;
}

/**
 *
 */
static int child_init(int rank)
{
	evrexec_task_t *it;
	int i;
	int pid;
	char si_desc[MAX_PT_DESC];

	if(_evrexec_list==NULL)
		return 0;

	if (rank!=PROC_MAIN)
		return 0;

	it = _evrexec_list;
	while(it) {
		for(i=0; i<it->workers; i++) {
			snprintf(si_desc, MAX_PT_DESC, "EVREXEC child=%d exec=%.*s",
					i, it->ename.len, it->ename.s);
			pid=fork_process(PROC_RPC, si_desc, 1);
			if (pid<0)
				return -1; /* error */
			if(pid==0){
				/* child */
				/* initialize the config framework */
				if (cfg_child_init())
					return -1;

				evrexec_process(it, i);
			}
		}
		it = it->next;
	}

	return 0;
}

/**
 *
 */
void evrexec_process(evrexec_task_t *it, int idx)
{
	sip_msg_t *fmsg;
	sr_kemi_eng_t *keng = NULL;
	str sidx = STR_NULL;

	if(it!=NULL) {
		fmsg = faked_msg_next();
		set_route_type(LOCAL_ROUTE);
		if(it->wait>0) sleep_us(it->wait);
		keng = sr_kemi_eng_get();
		if(keng==NULL) {
			if(it->rtid>=0 && event_rt.rlist[it->rtid]!=NULL) {
				run_top_route(event_rt.rlist[it->rtid], fmsg, 0);
			} else {
				LM_WARN("empty event route block [%.*s]\n",
						it->ename.len, it->ename.s);
			}
		} else {
			sidx.s = int2str(idx, &sidx.len);
			if(keng->froute(fmsg, EVENT_ROUTE,
						&it->ename, &sidx)<0) {
				LM_ERR("error running event route kemi callback\n");
			}
		}
	}
	/* avoid exiting the process */
	while(1) { sleep(3600); }
}

/**
 *
 */
int evrexec_param(modparam_t type, void *val)
{
	param_t* params_list = NULL;
	param_hooks_t phooks;
	param_t *pit=NULL;
	evrexec_task_t *it;
	evrexec_task_t tmp;
	sr_kemi_eng_t *keng = NULL;
	str s;

	if(val==NULL)
		return -1;
	s.s = (char*)val;
	s.len = strlen(s.s);
	if(s.s[s.len-1]==';')
		s.len--;
	if (parse_params(&s, CLASS_ANY, &phooks, &params_list)<0)
		return -1;
	memset(&tmp, 0, sizeof(evrexec_task_t));
	for (pit = params_list; pit; pit=pit->next) {
		if (pit->name.len==4
				&& strncasecmp(pit->name.s, "name", 4)==0) {
			tmp.ename = pit->body;
		} else if(pit->name.len==4
				&& strncasecmp(pit->name.s, "wait", 4)==0) {
			if(tmp.wait==0) {
				if (str2int(&pit->body, &tmp.wait) < 0) {
					LM_ERR("invalid wait: %.*s\n", pit->body.len, pit->body.s);
					return -1;
				}
			}
		} else if(pit->name.len==7
				&& strncasecmp(pit->name.s, "workers", 7)==0) {
			if(tmp.workers==0) {
				if (str2int(&pit->body, &tmp.workers) < 0) {
					LM_ERR("invalid workers: %.*s\n", pit->body.len, pit->body.s);
					return -1;
				}
			}
		} else {
			LM_ERR("invalid attribute: %.*s\n", pit->body.len, pit->body.s);
			return -1;
		}
	}
	if(tmp.ename.s==NULL || tmp.ename.len<=0) {
		LM_ERR("missing or invalid name attribute\n");
		free_params(params_list);
		return -1;
	}
	/* set '\0' at the end of route name */
	tmp.ename.s[tmp.ename.len] = '\0';
	keng = sr_kemi_eng_get();
	if(keng==NULL) {
		tmp.rtid = route_get(&event_rt, tmp.ename.s);
		if(tmp.rtid == -1) {
			LM_ERR("event route not found: %.*s\n", tmp.ename.len, tmp.ename.s);
			free_params(params_list);
			return -1;
		}
	} else {
		tmp.rtid = -1;
	}

	it = (evrexec_task_t*)pkg_malloc(sizeof(evrexec_task_t));
	if(it==0) {
		LM_ERR("no more pkg memory\n");
		free_params(params_list);
		return -1;
	}
	memcpy(it, &tmp, sizeof(evrexec_task_t));
	if(it->workers==0) it->workers=1;
	it->next = _evrexec_list;
	_evrexec_list = it;
	free_params(params_list);
	return 0;
}

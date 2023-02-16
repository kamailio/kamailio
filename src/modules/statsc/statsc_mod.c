/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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
#include <stdint.h>
#include <time.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/timer_proc.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/kemi.h"

#include "../../core/parser/parse_param.h"

MODULE_VERSION

int statsc_init(void);
void statsc_timer(unsigned int ticks, void *param);
int statsc_init_rpc(void);

static int statsc_interval = 540; /* 15 min */
static int statsc_items = 100;    /* history items */

int statsc_track_param(modparam_t type, void* val);

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);

static int w_statsc_reset(sip_msg_t* msg, char* p1, char* p2);

static cmd_export_t cmds[]={
	{"statsc_reset", (cmd_function)w_statsc_reset, 0,
		0, 0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"track",        PARAM_STRING|USE_FUNC_PARAM, (void*)statsc_track_param},
	{"interval",     INT_PARAM,   &statsc_interval},
	{"items",        INT_PARAM,   &statsc_items},
	{0, 0, 0}
};

struct module_exports exports = {
	"statsc",        /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd exports */
	params,          /* param exports */
	0,               /* exported RPC methods */
	0,               /* exported pseudo-variables */
	0,               /* response function */
	mod_init,        /* module initialization function */
	child_init,      /* per child init function */
	mod_destroy      /* destroy function */
};


typedef struct statsc_nmap {
	str sname;
	str rname;
	int64_t *vals;
	struct statsc_nmap *next;
} statsc_nmap_t;

typedef struct _statsc_info {
	uint64_t steps;
	uint32_t slots;
	uint32_t items;
	statsc_nmap_t *slist;
} statsc_info_t;

static statsc_info_t *_statsc_info = NULL;

/**
 * @brief Initialize statsc module function
 */
static int mod_init(void)
{
	if(statsc_init_rpc()<0) {
		LM_ERR("failed to register rpc commands\n");
		return -1;
	}
	if(sr_wtimer_add(statsc_timer, 0, statsc_interval)<0) {
		LM_ERR("failed to register timer routine\n");
		return -1;
	}
	if(_statsc_info==NULL) {
		if(statsc_init()<0) {
			LM_ERR("failed to initialize the stats collector structure\n");
			return -1;
		}
	} else {
		if(_statsc_info->items != (uint32_t)statsc_items) {
			LM_ERR("number of items set after tracking statistics were added\n");
			LM_ERR("set mod param 'items' before 'track'\n");
			return -1;
		}
	}
	return 0;
}

/**
 * @brief Initialize statsc module children
 */
static int child_init(int rank)
{
	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	return;
}

/**
 *
 */
static int ki_statsc_reset(sip_msg_t* msg)
{
	LM_ERR("not implemented yet\n");
	return 1;
}

/**
 *
 */
static int w_statsc_reset(sip_msg_t* msg, char* p1, char* p2)
{
	return ki_statsc_reset(msg);
}

int statsc_svalue(str *name, int64_t *res)
{
	stat_var       *stat;

	stat = get_stat(name);
	if(stat==NULL) {
		LM_ERR("statistic %.*s not found\n", name->len, name->s);
		*res = 0;
		return -1;
	}

	*res = (int64_t)get_stat_val(stat);

	return 0;
}

static statsc_nmap_t _statsc_nmap_default[] = {
	{ str_init("shm.free"),      str_init("free_size"), NULL, NULL}, /* shmem:free_size */
	{ str_init("shm.used"),      str_init("used_size"), NULL, NULL},
	{ str_init("shm.real_used"), str_init("real_used_size"), NULL, NULL},
	{ {NULL, 0},                 {NULL, 0}, NULL, NULL}
};

#define STRLEN_ROUNDUP(len)  ( ( ((size_t)len) / sizeof(void*) + 1 ) * sizeof(void*) )

int statsc_nmap_add(str *sname, str *rname)
{
	int sz;
	statsc_nmap_t *sm = NULL;
	statsc_nmap_t *sl = NULL;

	if(_statsc_info==NULL) {
		LM_ERR("root structure not initialize yet\n");
		return -1;
	}
	if(_statsc_info->items != (uint32_t)statsc_items) {
		LM_ERR("number of items set after tracking statistics were added\n");
		LM_ERR("set mod param 'items' before 'track'\n");
		return -1;
	}

	sz = sizeof(statsc_nmap_t) + statsc_items * sizeof(int64_t)
		+ STRLEN_ROUNDUP(sname->len) + STRLEN_ROUNDUP(rname->len);
	sm = shm_malloc(sz);
	if(sm==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	memset(sm, 0, sz);
	sm->sname.s = (char*)((char*)sm + sizeof(statsc_nmap_t));
	sm->sname.len = sname->len;
	sm->rname.s = (char*)((char*)sm->sname.s + STRLEN_ROUNDUP(sm->sname.len));
	sm->rname.len = rname->len;
	sm->vals = (int64_t*)((char*)sm->rname.s + STRLEN_ROUNDUP(sm->rname.len));
	memcpy(sm->sname.s, sname->s, sname->len);
	memcpy(sm->rname.s, rname->s, rname->len);

	LM_INFO("added stat mapping [%.*s] [%.*s]\n", sname->len, sname->s,
			 rname->len,  rname->s);
	if(_statsc_info->slist==NULL) {
		_statsc_info->slist = sm;
		_statsc_info->slots = 1;
		_statsc_info->items = (uint32_t)statsc_items;
		return 0;
	}
	sl = _statsc_info->slist;
	while(sl->next!=NULL) { sl = sl->next; }
	sl->next = sm;
	_statsc_info->slots++;
	return 0;
}

int statsc_init(void)
{
	int i;
	int sz;
	statsc_nmap_t *sm = NULL;

	if(_statsc_info!=NULL) {
		return 0;
	}

	_statsc_info = shm_malloc(sizeof(statsc_info_t));
	if(_statsc_info==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	memset(_statsc_info, 0, sizeof(statsc_info_t));

	/* first slot with timestamps */
	sz = sizeof(statsc_nmap_t) + statsc_items * sizeof(int64_t);
	sm = shm_malloc(sz);
	if(sm==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	memset(sm, 0, sz);
	sm->vals = (int64_t*)((char*)sm + sizeof(statsc_nmap_t));
	_statsc_info->slist = sm;
	_statsc_info->slots = 1;
	_statsc_info->items = (uint32_t)statsc_items;

	for(i=0; _statsc_nmap_default[i].sname.s!=0; i++) {
		if(statsc_nmap_add(&_statsc_nmap_default[i].sname,
					&_statsc_nmap_default[i].rname)<0) {
			LM_ERR("cannot enable tracking default statistics\n");
			return -1;
		}
	}

	return 0;
}


void statsc_timer(unsigned int ticks, void *param)
{
	statsc_nmap_t *sm = NULL;
	time_t tn;
	int n;
	int i;

	if(_statsc_info==NULL || _statsc_info->slist==NULL) {
		LM_ERR("statsc not initialized\n");
		return;
	}

	tn = time(NULL);
	n = _statsc_info->steps % statsc_items;
	_statsc_info->slist->vals[n] = (int64_t)tn;

	LM_DBG("statsc timer - time: %lu - ticks: %u - index: %d - steps: %llu\n",
			(unsigned long)tn, ticks, n, (unsigned long long)_statsc_info->steps);

	i = 0;
	for(sm=_statsc_info->slist->next; sm!=NULL; sm=sm->next) {
		LM_DBG("fetching value for: [%.*s] - index [%d]\n", sm->rname.len,
				sm->rname.s, i);
		statsc_svalue(&sm->rname, sm->vals + n);
		i++;
	}
	_statsc_info->steps++;
}


/**
 *
 */
int statsc_track_param(modparam_t type, void* val)
{
	param_t* params_list = NULL;
	param_hooks_t phooks;
	param_t *pit=NULL;
	str s;

	if(val==NULL) {
		return -1;
	}
	if(statsc_init()<0) {
		return -1;
	}
	s.s = (char*)val;
	s.len = strlen(s.s);
	if(s.s[s.len-1]==';') {
		s.len--;
	}
	if (parse_params(&s, CLASS_ANY, &phooks, &params_list)<0) {
		return -1;
	}
	for (pit = params_list; pit; pit=pit->next) {
		if(statsc_nmap_add(&pit->name, &pit->body)<0) {
			free_params(params_list);
			LM_ERR("cannot enable tracking statistics\n");
			return -1;
		}
	}
	free_params(params_list);
	return 0;
}


/**
 *
 */
static const char* statsc_rpc_report_doc[2] = {
	"Statistics collector control command",
	0
};

/**
 *
 */
static void statsc_rpc_report(rpc_t* rpc, void* ctx)
{
	statsc_nmap_t *sm = NULL;
	str cname;
	int cmode;
	str sname;
	int range;
	int k, m, n;
	int64_t v;
	time_t tn;
	void* th;
	void* ts;
	void* ti;
	void* ta;
	void* td;

	if(_statsc_info==NULL || _statsc_info->slist==NULL) {
		rpc->fault(ctx, 500, "Statistics collector not initialized");
		return;
	}
	if(_statsc_info->steps==0) {
		rpc->fault(ctx, 500, "Nothing collected yet - try later");
		return;
	}
	n = (_statsc_info->steps - 1) % statsc_items;

	cmode = 0;
	if(rpc->scan(ctx, "S", &cname) != 1) {
		rpc->fault(ctx, 500, "Missing command parameter");
		return;
	}

	if(cname.len==4 && strncmp(cname.s, "list", 4)==0) {
		cmode = 1;
	} else if(cname.len==4 && strncmp(cname.s, "diff", 4)==0) {
		cmode = 2;
	} else {
		rpc->fault(ctx, 500, "Invalid command");
		return;
	}

	range = 0;
	if(rpc->scan(ctx, "*S", &sname) != 1) {
		sname.len = 0;
		sname.s = NULL;
	} else {
		if(sname.len==3 && strncmp(sname.s, "all", 3)==0) {
			sname.len = 0;
			sname.s = NULL;
		}
		rpc->scan(ctx, "*d", &range);
		if(range<0 || range>statsc_items) {
			range = 0;
		}
	}

	tn = time(NULL);
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Error creating rpc (1)");
		return;
	}
	if(rpc->struct_add(th, "u[",
				"timestamp", (uint64_t)tn,
				"stats",     &ts)<0) {
		rpc->fault(ctx, 500, "Error creating rpc (2)");
		return;
	}
	for(sm=_statsc_info->slist->next; sm!=NULL; sm=sm->next) {
		if(sname.s==NULL ||
				(sname.len == sm->sname.len
				 && strncmp(sname.s, sm->sname.s, sname.len)==0)) {
			if(rpc->array_add(ts, "{", &ta)<0) {
				rpc->fault(ctx, 500, "Error creating rpc (3)");
				return;
			}
			if(rpc->struct_add(ta, "S[",
						"name", &sm->sname,
						"data", &td )<0) {
				rpc->fault(ctx, 500, "Error creating rpc (4)");
				return;
			}
			m = 0;
			for(k=n; k>=0; k--) {
				if(rpc->array_add(td, "{", &ti)<0) {
					rpc->fault(ctx, 500, "Error creating rpc (5)");
					return;
				}
				v = sm->vals[k];
				switch(cmode) {
					case 1:
						break;
					case 2:
						if((n==statsc_items-1) && k==0) {
							continue;
						}
						if(k==0) {
							v -= sm->vals[statsc_items-1];
						} else {
							v -= sm->vals[k-1];
						}
						break;
				}
				if(rpc->struct_add(ti, "uLd",
						"timestamp", (unsigned int)_statsc_info->slist->vals[k],
						"value", v,
						"index", m++)<0) {
					rpc->fault(ctx, 500, "Error creating rpc (6)");
					return;
				}
				if(range>0 && m>=range) {
					break;
				}
			}
			for(k=statsc_items-1; k>n; k--) {
				if(rpc->array_add(td, "{", &ti)<0) {
					rpc->fault(ctx, 500, "Error creating rpc (7)");
					return;
				}
				v = sm->vals[k];
				switch(cmode) {
					case 1:
						break;
					case 2:
						if(n==k-1) {
							continue;
						}
						v -= sm->vals[k-1];
						break;
				}
				if(rpc->struct_add(ti, "uLd",
						"timestamp", (unsigned int)_statsc_info->slist->vals[k],
						"value", v,
						"index", m++)<0) {
					rpc->fault(ctx, 500, "Error creating rpc (8)");
					return;
				}
				if(range>0 && m>=range) {
					break;
				}
			}
		}
	}
}

/**
 *
 */
rpc_export_t statsc_rpc[] = {
	{"statsc.report", statsc_rpc_report, statsc_rpc_report_doc, 0},
	{0, 0, 0, 0}
};

/**
 *
 */
int statsc_init_rpc(void)
{
	if (rpc_register_array(statsc_rpc)!=0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_statsc_exports[] = {
	{ str_init("statsc"), str_init("statsc_reset"),
		SR_KEMIP_INT, ki_statsc_reset,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
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
	sr_kemi_modules_add(sr_kemi_statsc_exports);
	return 0;
}

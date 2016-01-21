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
#include <time.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../timer_proc.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"

#include "../../lib/kcore/statistics.h"

MODULE_VERSION

int statsc_init(void);
void statsc_timer(unsigned int ticks, void *param);
int statsc_init_rpc(void);

static int statsc_interval = 540; /* 15 min */
static int statsc_items = 100;    /* history items */

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
	{"interval",     INT_PARAM,   &statsc_interval},
	{"items",        INT_PARAM,   &statsc_items},
	{0, 0, 0}
};

struct module_exports exports = {
	"statsc",
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
	if(statsc_init()<0) {
		LM_ERR("failed to initialize the stats collector structure\n");
		return -1;
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
static int w_statsc_reset(sip_msg_t* msg, char* p1, char* p2)
{
	return 1;
}

typedef int (*statsc_func_t)(void *p, int64_t *res);

typedef struct statsc_nmap {
	str sname;
	int sindex;
	statsc_func_t f;
	void *p;
} statsc_nmap_t;


int statsc_timestamp(void *p, int64_t *res)
{
	return 0;
}

int statsc_svalue(void *p, int64_t *res)
{
	stat_var       *stat;
	str name;

	name.s = (char*)p;
	name.len = strlen(name.s);

	stat = get_stat(&name);
	if(stat==NULL) {
		LM_ERR("statistic %.*s not found\n", name.len, name.s);
		return -1;
	}

	*res = (int64_t)get_stat_val(stat);

	return 0;
}

static statsc_nmap_t _statsc_nmap[] = {
	{ {"timestamp", 9},         0, statsc_timestamp, (void*)0},
	{ {"shm.free",  8},         1, statsc_svalue,  (void*)"free_size"}, /* shmem:free_size */
	{ {"shm.used",  8},         2, statsc_svalue,  (void*)"used_size"},
	{ {"shm.real_used",  13},   3, statsc_svalue,  (void*)"real_used_size"},
	{ {0, 0},                   0, 0}
};


int statsc_nmap_index(str *sn)
{
	int i;

	for(i=0; _statsc_nmap[i].sname.s!=0; i++) {
		if(sn->len==_statsc_nmap[i].sname.len
				&& strncmp(sn->s, _statsc_nmap[i].sname.s, sn->len)==0) {
			return i;
		}
	}
	return -1;
}

typedef struct _statsc_info {
	uint64_t steps;
	uint32_t slots;
	int64_t **stable;
} statsc_info_t;


static statsc_info_t *_statsc_info = NULL;

int statsc_init(void)
{
	int i;

	_statsc_info = shm_malloc(sizeof(statsc_info_t));
	if(_statsc_info==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	memset(_statsc_info, 0, sizeof(statsc_info_t));
	
	for(i=0; _statsc_nmap[i].sname.s!=0; i++);
	_statsc_info->slots = i;
	
	_statsc_info->stable = shm_malloc(_statsc_info->slots * sizeof(int64_t*));
	if(_statsc_info->stable==NULL) {
		LM_ERR("no more shared memory\n");
		shm_free(_statsc_info);
		_statsc_info=NULL;
		return -1;
	}
	memset(_statsc_info->stable, 0, _statsc_info->slots * sizeof(int64_t*));
	for(i=0; i<_statsc_info->slots; i++) {
		_statsc_info->stable[i] = shm_malloc(statsc_items * sizeof(int64_t));
		if(_statsc_info->stable[i]==NULL) {
			LM_ERR("no more shared memory\n");
			i--;
			while(i>=0) {
				shm_free(_statsc_info->stable[i]);
				i--;
			}
			shm_free(_statsc_info->stable);
			shm_free(_statsc_info);
			return -1;
		}
		memset(_statsc_info->stable[i], 0, statsc_items * sizeof(int64_t));
	}

	return 0;
}


void statsc_timer(unsigned int ticks, void *param)
{
	time_t tn;
	int i;
	int n;

	if(_statsc_info==NULL) {
		LM_ERR("statsc not initialized\n");
		return;
	}

	tn = time(NULL);
	n = _statsc_info->steps % statsc_items;
	_statsc_info->stable[0][n] = (int64_t)tn;

	LM_DBG("statsc timer - time: %lu - ticks: %u - index: %d - steps: %llu\n",
			(unsigned long)tn, ticks, n, _statsc_info->steps);

	for(i=1; i<_statsc_info->slots; i++) {
		_statsc_nmap[i].f(_statsc_nmap[i].p, _statsc_info->stable[i] + n);
	}
	_statsc_info->steps++;
}


/**
 *
 */
static const char* statsc_rpc_exec_doc[2] = {
	"Statistics collector control command",
	0
};

/**
 *
 */
static void statsc_rpc_exec(rpc_t* rpc, void* ctx)
{
	str cname;
	int cmode;
	str sname;
	int range;
	int sidx;
	int i, k, n, r, m, v;
	time_t tn;
	void* th;
	void* ts;
	void* ti;
	void* ta;
	void* td;

	if(_statsc_info==NULL) {
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
	sidx = -1;
	if(rpc->scan(ctx, "*S", &sname) != 1) {
		sname.len = 0;
		sname.s = NULL;
	} else {
		if(sname.len!=3 || strncmp(sname.s, "all", 3)!=0) {
			if((sidx = statsc_nmap_index(&sname))<0) {
				rpc->fault(ctx, 500, "Invalid statistic name");
				return;
			}
		}
		rpc->scan(ctx, "*d", &range);
		if(range<0 || range>statsc_items)
			range = 0;
	}

	tn = time(NULL);
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Error creating rpc (1)");
		return;
	}
	if(rpc->struct_add(th, "u[",
				"timestamp", (unsigned int)tn,
				"stats",     &ts )<0) {
		rpc->fault(ctx, 500, "Error creating rpc (2)");
		return;
	}
	for(i=1; i<_statsc_info->slots; i++) {
		if(sidx==-1 || sidx==i) {
			if(rpc->array_add(ts, "{", &ta)<0) {
				rpc->fault(ctx, 500, "Error creating rpc (3)");
				return;
			}
			if(rpc->struct_add(ta, "S[",
						"name", &_statsc_nmap[i].sname,
						"data", &td )<0) {
				rpc->fault(ctx, 500, "Error creating rpc (4)");
				return;
			}
			m = 0;
			r = range;
			for(k=n; k>=0; k--) {
				if(rpc->array_add(td, "{", &ti)<0) {
					rpc->fault(ctx, 500, "Error creating rpc (5)");
					return;
				}
				v = (int)_statsc_info->stable[i][k];
				switch(cmode) {
					case 1:
						break;
					case 2:
						if((n==statsc_items-1) && k==0) {
							continue;
						}
						if(k==0) {
							v -= (int)_statsc_info->stable[i][statsc_items-1];
						} else {
							v -= (int)_statsc_info->stable[i][k-1];
						}
						break;
				}
				if(rpc->struct_add(ti, "udd",
						"timestamp", (unsigned int)_statsc_info->stable[0][k],
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
				v = (int)_statsc_info->stable[i][k];
				switch(cmode) {
					case 1:
						break;
					case 2:
						if(n==k-1) {
							continue;
						}
						v -= (int)_statsc_info->stable[i][k-1];
						break;
				}
				if(rpc->struct_add(ti, "udd",
						"timestamp", (unsigned int)_statsc_info->stable[0][k],
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
	{"statsc.exec", statsc_rpc_exec, statsc_rpc_exec_doc, 0},
	{0, 0, 0, 0}
};

/**
 *
 */
int statsc_init_rpc(void)
{
	if (rpc_register_array(statsc_rpc)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}


/*
 * $Id: benchmark.c 941 2007-04-11 12:37:21Z bastian $
 *
 * Benchmarking module for Kamailio
 *
 * Copyright (C) 2007 Collax GmbH
 *                    (Bastian Friedrich <bastian.friedrich@collax.com>)
 * Copyright (C) 2007 Voice Sistem SRL
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
 *
 */

/*! \file
 * \brief Benchmark :: Module Core
 *
 * \ingroup benchmark
 * - Module: benchmark
 */

/*! \defgroup benchmark Benchmark :: Developer benchmarking module
 *
 * This module is for Kamailio developers, as well as admins. It gives
 * a possibility to clock certain critical paths in module code or 
 * configuration sections.
 * 
 */

#define _GNU_SOURCE
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../lib/kmi/mi.h"
#include "../../mem/mem.h"
#include "../../ut.h"

#include "benchmark.h"

#include "../../mem/shm_mem.h"


MODULE_VERSION

/* Exported functions */
int bm_start_timer(struct sip_msg* _msg, char* timer, char *foobar);
int bm_log_timer(struct sip_msg* _msg, char* timer, char* mystr);

/*
 * Module destroy function prototype
 */
static void destroy(void);


/*
 * Module initialization function prototype
 */
static int mod_init(void);


/*
 * Exported parameters
 * Copied to mycfg on module initialization
 */
static int bm_enable_global = 0;
static int bm_granularity = 1;
static int bm_loglevel = L_INFO;

static int _bm_last_time_diff = 0;

/*
 * Module setup
 */

typedef struct bm_cfg {
	int enable_global;
	int granularity;
	int loglevel;
	/* The internal timers */
	int nrtimers;
	benchmark_timer_t *timers;
	benchmark_timer_t **tindex;
} bm_cfg_t;

/*
 * The setup is located in shared memory so that
 * all instances can access this variable
 */

bm_cfg_t *bm_mycfg = 0;

static inline int fixup_bm_timer(void** param, int param_no);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{ "bm_start_timer", (cmd_function)bm_start_timer, 1, fixup_bm_timer, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE },
	{ "bm_log_timer",   (cmd_function)bm_log_timer, 1, fixup_bm_timer, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE },
	{"load_bm",         (cmd_function)load_bm, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0 }
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"enable",      INT_PARAM, &bm_enable_global},
	{"granularity", INT_PARAM, &bm_granularity},
	{"loglevel",    INT_PARAM, &bm_loglevel},
	{ 0, 0, 0 }
};


/*
 * Exported MI functions
 */
struct mi_root* mi_bm_enable_global(struct mi_root *cmd, void *param);
struct mi_root* mi_bm_enable_timer(struct mi_root *cmd, void *param);
struct mi_root* mi_bm_granularity(struct mi_root *cmd, void *param);
struct mi_root* mi_bm_loglevel(struct mi_root *cmd, void *param);

static mi_export_t mi_cmds[] = {
	{ "bm_enable_global", mi_bm_enable_global,  0,  0,  0  },
	{ "bm_enable_timer",  mi_bm_enable_timer,   0,  0,  0  },
	{ "bm_granularity",   mi_bm_granularity,    0,  0,  0  },
	{ "bm_loglevel",      mi_bm_loglevel,       0,  0,  0  },
	{ 0, 0, 0, 0, 0}
};

static int bm_get_time_diff(struct sip_msg *msg, pv_param_t *param, 
		pv_value_t *res);

static pv_export_t mod_items[] = {
	{ {"BM_time_diff", sizeof("BM_time_diff")-1}, PVT_OTHER, bm_get_time_diff, 0,
		0, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

/*
 * Module interface
 */
struct module_exports exports = {
	"benchmark", 
	DEFAULT_DLFLAGS,
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	0,          /* exported statistics */
	mi_cmds,    /* exported MI functions */
	mod_items,  /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,          /* response function */
	destroy,    /* destroy function */
	0           /* child initialization function */
};


/****************/


/*
 * mod_init
 * Called by Kamailio at init time
 */
static int mod_init(void) {
	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	bm_mycfg = (bm_cfg_t*)shm_malloc(sizeof(bm_cfg_t));
	memset(bm_mycfg, 0, sizeof(bm_cfg_t));
	bm_mycfg->enable_global = bm_enable_global;
	bm_mycfg->granularity   = bm_granularity;
	bm_mycfg->loglevel      = bm_loglevel;

	return 0;
}


/*
 * destroy
 * called by Kamailio at exit time
 */
static void destroy(void)
{
	benchmark_timer_t *bmt = 0;
	benchmark_timer_t *bmp = 0;

	if(bm_mycfg!=NULL) 
	{
		/* free timers list */
		bmt = bm_mycfg->timers;
		while(bmt)
		{
			bmp = bmt;
			bmt = bmt->next;
			shm_free(bmp);
		}
		if(bm_mycfg->tindex) shm_free(bm_mycfg->tindex);
		shm_free(bm_mycfg);
	}
}

void bm_reset_timer(int i)
{
	if(bm_mycfg==NULL || bm_mycfg->tindex[i]==NULL)
		return;
	bm_mycfg->tindex[i]->calls = 0;
	bm_mycfg->tindex[i]->sum = 0;
	bm_mycfg->tindex[i]->last_max = 0;
	bm_mycfg->tindex[i]->last_min = 0xffffffff;
	bm_mycfg->tindex[i]->last_sum = 0;
	bm_mycfg->tindex[i]->global_max = 0;
	bm_mycfg->tindex[i]->global_min = 0xffffffff;
}

void reset_timers(void)
{
	int i;
	if(bm_mycfg==NULL)
		return;

	for (i = 0; i < bm_mycfg->nrtimers; i++)
		bm_reset_timer(i);
}

/*! \brief
 * timer_active().
 *
 * Global enable mode can be:
 * -1 - All timing disabled
 *  0 - Timing enabled, watch for single timers enabled (default: off)
 *  1 - Timing enabled for all timers
 */

static inline int timer_active(unsigned int id)
{
	if (bm_mycfg->enable_global > 0 || bm_mycfg->timers[id].enabled > 0)
		return 1;
	else
		return 0;
}


/*! \brief
 * start_timer()
 */

int _bm_start_timer(unsigned int id)
{
	if (timer_active(id))
	{
		if(bm_get_time(bm_mycfg->tindex[id]->start)!=0)
		{
			LM_ERR("error getting current time\n");
			return -1;
		}
	}

	return 1;
}

int bm_start_timer(struct sip_msg* _msg, char* timer, char *foobar)
{
	return _bm_start_timer((unsigned int)(unsigned long)timer);
}


/*! \brief
 * log_timer()
 */

int _bm_log_timer(unsigned int id)
{
	/* BM_CLOCK_REALTIME */
	bm_timeval_t now;
	unsigned long long tdiff;

	if (!timer_active(id))
		return 1;

	if(bm_get_time(&now)<0)
	{
		LM_ERR("error getting current time\n");
		return -1;
	}
	
	tdiff = bm_diff_time(bm_mycfg->tindex[id]->start, &now);
	_bm_last_time_diff = (int)tdiff;

	/* What to do
	 * - update min, max, sum
	 * - if granularity hit: Log, reset min/max
	 */

	bm_mycfg->tindex[id]->sum += tdiff;
	bm_mycfg->tindex[id]->last_sum += tdiff;
	bm_mycfg->tindex[id]->calls++;
	
	if (tdiff < bm_mycfg->tindex[id]->last_min)
		bm_mycfg->tindex[id]->last_min = tdiff;

	if (tdiff > bm_mycfg->tindex[id]->last_max)
		bm_mycfg->tindex[id]->last_max = tdiff;

	if (tdiff < bm_mycfg->tindex[id]->global_min)
		bm_mycfg->tindex[id]->global_min = tdiff;

	if (tdiff > bm_mycfg->tindex[id]->global_max)
		bm_mycfg->tindex[id]->global_max = tdiff;


	if ((bm_mycfg->tindex[id]->calls % bm_mycfg->granularity) == 0)
	{
		LM_GEN1(bm_mycfg->loglevel, "benchmark (timer %s [%d]): %llu ["
			" msgs/total/min/max/avg - LR:"
			" %i/%llu/%llu/%llu/%f | GB: %llu/%llu/%llu/%llu/%f]\n",
			bm_mycfg->tindex[id]->name,
			id,
			tdiff,
			bm_mycfg->granularity,
			bm_mycfg->tindex[id]->last_sum,
			bm_mycfg->tindex[id]->last_min,
			bm_mycfg->tindex[id]->last_max,
			((double)bm_mycfg->tindex[id]->last_sum)/bm_mycfg->granularity,
			bm_mycfg->tindex[id]->calls,
			bm_mycfg->tindex[id]->sum,
			bm_mycfg->tindex[id]->global_min,
			bm_mycfg->tindex[id]->global_max,
			((double)bm_mycfg->tindex[id]->sum)/bm_mycfg->tindex[id]->calls);

		bm_mycfg->tindex[id]->last_sum = 0;
		bm_mycfg->tindex[id]->last_max = 0;
		bm_mycfg->tindex[id]->last_min = 0xffffffff;
	}

	return 1;
}	

int bm_log_timer(struct sip_msg* _msg, char* timer, char* mystr)
{
	return _bm_log_timer((unsigned int)(unsigned long)timer);
}

int _bm_register_timer(char *tname, int mode, unsigned int *id)
{
	benchmark_timer_t *bmt = 0;
	benchmark_timer_t **tidx = 0;

	if(tname==NULL || id==NULL || bm_mycfg==NULL || strlen(tname)==0
			|| strlen(tname)>BM_NAME_LEN-1)
		return -1;

	bmt = bm_mycfg->timers;
	while(bmt)
	{
		if(strcmp(bmt->name, tname)==0)
		{
			*id = bmt->id;
			return 0;
		}
		bmt = bmt->next;
	}
	if(mode==0)
		return -1;

	bmt = (benchmark_timer_t*)shm_malloc(sizeof(benchmark_timer_t));

	if(bmt==0)
	{
		LM_ERR("no more shm\n");
		return -1;
	}
	memset(bmt, 0, sizeof(benchmark_timer_t));

	/* private memory, otherwise we have races */
	bmt->start = (bm_timeval_t*)pkg_malloc(sizeof(bm_timeval_t)); 
	if(bmt->start == NULL)
	{
		shm_free(bmt);
		LM_ERR("no more pkg\n");
		return -1;
	}
	memset(bmt->start, 0, sizeof(bm_timeval_t)); 

	strcpy(bmt->name, tname);
	if(bm_mycfg->timers==0)
	{
		bmt->id = 0;
		bm_mycfg->timers = bmt;
	} else {
		bmt->id = bm_mycfg->timers->id+1;
		bmt->next = bm_mycfg->timers;
		bm_mycfg->timers = bmt;
	}

	/* do the indexing */
	if(bmt->id%10==0)
	{
		if(bm_mycfg->tindex!=NULL)
			tidx = bm_mycfg->tindex;
		bm_mycfg->tindex = (benchmark_timer_t**)shm_malloc((10+bmt->id)*
								sizeof(benchmark_timer_t*));
		if(bm_mycfg->tindex==0)
		{
			LM_ERR("no more share memory\n");
			if(tidx!=0)
				shm_free(tidx);
			return -1;
		}
		memset(bm_mycfg->tindex, 0, (10+bmt->id)*sizeof(benchmark_timer_t*));
		if(tidx!=0)
		{
			memcpy(bm_mycfg->tindex, tidx, bmt->id*sizeof(benchmark_timer_t*));
			shm_free(tidx);
		}
	}
	bm_mycfg->tindex[bmt->id] = bmt;
	bm_mycfg->nrtimers = bmt->id + 1;
	bm_reset_timer(bmt->id);
	*id = bmt->id;
	LM_DBG("timer [%s] added with index <%u>\n", bmt->name, bmt->id);

	return 0;
}

/*! \brief API Binding */
int load_bm( struct bm_binds *bmb)
{
	if(bmb==NULL)
		return -1;

	bmb->bm_register = _bm_register_timer;
	bmb->bm_start    = _bm_start_timer;
	bmb->bm_log	     = _bm_log_timer;

	return 1;
}


static inline char * pkg_strndup( char* _p, int _len)
{
	char *s;

	s = (char*)pkg_malloc(_len+1);
	if (s==NULL)
		return NULL;
	memcpy(s,_p,_len);
	s[_len] = 0;
	return s;
}


/*! \name TimerMIfunctions MI functions */
/*@{ */

/*! \brief
 * Expects 1 node: 0 for disable, 1 for enable
 */
struct mi_root* mi_bm_enable_global(struct mi_root *cmd, void *param)
{
	struct mi_node *node;

	char *p1, *e1;
	long int v1;

	node = cmd->node.kids;

	if ((node == NULL) || (node->next != NULL))
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	p1 = pkg_strndup(node->value.s, node->value.len);

	v1 = strtol(p1, &e1, 0);

	if ((*e1 != '\0') || (*p1 == '\0')) {
		pkg_free(p1);
		return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
	}

	if ((v1 < -1) || (v1 > 1)) {
		pkg_free(p1);
		return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
	}

	bm_mycfg->enable_global = v1;

	pkg_free(p1);
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}

struct mi_root* mi_bm_enable_timer(struct mi_root *cmd, void *param)
{
	struct mi_node *node;

	char *p1, *p2, *e2;
	long int v2;
	unsigned int id;

	node = cmd->node.kids;

	if ((node == NULL) || (node->next == NULL) || (node->next->next != NULL))
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	p1 = pkg_strndup(node->value.s, node->value.len);
	p2 = pkg_strndup(node->next->value.s, node->next->value.len);
	if(!p1 || !p2)
		goto error;

	if(_bm_register_timer(p1, 0, &id)!=0)
	{
		pkg_free(p1);
		pkg_free(p2);
		return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
	}
	v2 = strtol(p2, &e2, 0);
	
	pkg_free(p1);

	if (*e2 != '\0' || *p2 == '\0') {
		pkg_free(p2);
		return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
	}

	pkg_free(p2);
	if ((v2 < 0) || (v2 > 1))
		return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);

	bm_mycfg->timers[id].enabled = v2;

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
error:
	if(p1) pkg_free(p1);
	if(p2) pkg_free(p2);
	return init_mi_tree(500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
}

struct mi_root* mi_bm_granularity(struct mi_root *cmd, void *param)
{
	struct mi_node *node;

	char *p1, *e1;
	long int v1;

	node = cmd->node.kids;

	if ((node == NULL) || (node->next != NULL))
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	p1 = pkg_strndup(node->value.s, node->value.len);

	v1 = strtol(p1, &e1, 0);

	if ((*e1 != '\0') || (*p1 == '\0'))
	{
		pkg_free(p1);
		return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
	}

	pkg_free(p1);

	if (v1 < 1)
		return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);

	bm_mycfg->granularity = v1;

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}

struct mi_root* mi_bm_loglevel(struct mi_root *cmd, void *param)
{
	struct mi_node *node;

	char *p1, *e1;
	long int v1;

	node = cmd->node.kids;

	if ((node == NULL) || (node->next != NULL))
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	p1 = pkg_strndup(node->value.s, node->value.len);

	v1 = strtol(p1, &e1, 0);
	
	if ((*e1 != '\0') || (*p1 == '\0'))
	{
		pkg_free(p1);
		return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
	}

	pkg_free(p1);

	if ((v1 < -3) || (v1 > 4)) /* Maximum log levels */
		return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);

	bm_mycfg->enable_global = v1;

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}
/*@} */

/*! \brief PV get function for time diff */
static int bm_get_time_diff(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(msg==NULL)
		return -1;
	return pv_get_sintval(msg, param, res, _bm_last_time_diff);
}


static inline int fixup_bm_timer(void** param, int param_no)
{
	unsigned int tid = 0;
	if (param_no == 1)
	{
		if((_bm_register_timer((char*)(*param), 1, &tid))!=0)
		{
			LM_ERR("cannot register timer [%s]\n", (char*)(*param));
			return E_UNSPEC;
		}
		pkg_free(*param);
		*param = (void*)(unsigned long)tid;
	}
	return 0;
}

/* End of file */

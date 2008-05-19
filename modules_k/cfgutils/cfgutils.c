/*
 * $Id$
 *
 * Copyright (C) 2007 1&1 Internet AG
 * Copyright (C) 2007 BASIS AudioNet GmbH
 *
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2007-03-22  initial module created (Henning Westerholt)
 *  2007-03-29  adaption to openser 1.2 and some cleanups
 *  2007-04-20  rename to cfgutils, use pseudovariable for get_random_val
 *              add "rand_" prefix, add sleep and usleep functions
 *
 * cfgutils module: random probability functions for openser;
 * it provide functions to make a decision in the script
 * of the server based on a probability function.
 * The benefit of this module is the value of the probability function
 * can be manipulated by external applications such as web interface
 * or command line tools.
 * Furthermore it provides some functions to let the server wait a
 * specific time interval.
 * 
 */


/* FIFO action protocol names */
#define FIFO_SET_PROB   "rand_set_prob"
#define FIFO_RESET_PROB "rand_reset_prob"
#define FIFO_GET_PROB   "rand_get_prob"
#define FIFO_GET_HASH   "get_config_hash"
#define FIFO_CHECK_HASH "check_config_hash"

#include "../../sr_module.h"
#include "../../error.h"
#include "../../pvar.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../mi/mi.h"
#include "../../mod_fix.h"
#include "../../md5utils.h"
#include "../../globals.h"
#include <stdlib.h>
#include "shvar.h"

MODULE_VERSION

static int set_prob(struct sip_msg*, char *, char *);
static int reset_prob(struct sip_msg*, char *, char *);
static int get_prob(struct sip_msg*, char *, char *);
static int rand_event(struct sip_msg*, char *, char *);
static int m_sleep(struct sip_msg*, char *, char *);
static int m_usleep(struct sip_msg*, char *, char *);
static int dbg_abort(struct sip_msg*, char*,char*);
static int dbg_pkg_status(struct sip_msg*, char*,char*);
static int dbg_shm_status(struct sip_msg*, char*,char*);

static struct mi_root* mi_set_prob(struct mi_root* cmd, void* param );
static struct mi_root* mi_reset_prob(struct mi_root* cmd, void* param );
static struct mi_root* mi_get_prob(struct mi_root* cmd, void* param );
static struct mi_root* mi_get_hash(struct mi_root* cmd, void* param );
static struct mi_root* mi_check_hash(struct mi_root* cmd, void* param );

static int pv_get_random_val(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

static int fixup_prob( void** param, int param_no);

static int mod_init(void);
static void mod_destroy(void);

static int initial = 10;
static int *probability;

static char config_hash[MD5_LEN];
static char* hash_file = NULL;

static cmd_export_t cmds[]={
	{"rand_set_prob", /* action name as in scripts */
		(cmd_function)set_prob,  /* C function name */
		1,          /* number of parameters */
		fixup_prob, 0,         /* */
		/* can be applied to original/failed requests and replies */
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"rand_reset_prob", (cmd_function)reset_prob, 0, 0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"rand_get_prob",   (cmd_function)get_prob,   0, 0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"rand_event",      (cmd_function)rand_event, 0, 0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"sleep",  (cmd_function)m_sleep,  1, fixup_uint_null, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"usleep", (cmd_function)m_usleep, 1, fixup_uint_null, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"abort",      (cmd_function)dbg_abort,        0, 0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"pkg_status", (cmd_function)dbg_pkg_status,   0, 0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"shm_status", (cmd_function)dbg_shm_status,   0, 0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={ 
	{"initial_probability", INT_PARAM, &initial},
	{"hash_file",           STR_PARAM, &hash_file        },
	{"shvset",              STR_PARAM|USE_FUNC_PARAM, (void*)param_set_shvar },
	{"varset",              STR_PARAM|USE_FUNC_PARAM, (void*)param_set_var },
	{0,0,0}
};

static mi_export_t mi_cmds[] = {
	{ FIFO_SET_PROB,   mi_set_prob,   0,                 0,  0 },
	{ FIFO_RESET_PROB, mi_reset_prob, MI_NO_INPUT_FLAG,  0,  0 },
	{ FIFO_GET_PROB,   mi_get_prob,   MI_NO_INPUT_FLAG,  0,  0 },
	{ FIFO_GET_HASH,   mi_get_hash,   MI_NO_INPUT_FLAG,  0,  0 },
	{ FIFO_CHECK_HASH, mi_check_hash, MI_NO_INPUT_FLAG,  0,  0 },
	{ "shv_get",       mi_shvar_get,  0,                 0,  0 },
	{ "shv_set" ,      mi_shvar_set,  0,                 0,  0 },
	{ 0, 0, 0, 0, 0}
};

static pv_export_t mod_items[] = {
	{ {"RANDOM", sizeof("RANDOM")-1}, 1000, pv_get_random_val, 0,
		0, 0, 0, 0 },
	{ {"shv", (sizeof("shv")-1)}, 1001, pv_get_shvar,
		pv_set_shvar, pv_parse_shvar_name, 0, 0, 0},
	{ {"time", (sizeof("time")-1)}, 1002, pv_get_time,
		0, pv_parse_time_name, 0, 0, 0},

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

struct module_exports exports = {
	"cfgutils",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,        /* exported functions */
	params,      /* exported parameters */
	0,           /* exported statistics */
	mi_cmds,     /* exported MI functions */
	mod_items,   /* exported pseudo-variables */
	0,           /* extra processes */
	mod_init,    /* module initialization function */
	0,           /* response function*/
	mod_destroy, /* destroy function */
	0            /* per-child init function */
};


/**************************** fixup functions ******************************/
static int fixup_prob( void** param, int param_no)
{
	unsigned int myint;
	str param_str;

	/* we only fix the parameter #1 */
	if (param_no!=1)
		return 0;

	param_str.s=(char*) *param;
	param_str.len=strlen(param_str.s);
	str2int(&param_str, &myint);

	if (myint > 100) {
		LM_ERR("invalid probability <%d>\n", myint);
		return E_CFG;
	}

	pkg_free(*param);
	*param=(void *)(long)myint;
	return 0;
}

/************************** module functions **********************************/

static struct mi_root* mi_set_prob(struct mi_root* cmd, void* param )
{
	unsigned int percent;
	struct mi_node* node;

	node = cmd->node.kids;
	if(node == NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if( str2int( &node->value, &percent) <0)
		goto error;
	if (percent > 100) {
		LM_ERR("incorrect probability <%u>\n", percent);
		goto error;
	}
	*probability = percent;
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);

error:
	return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
}

static struct mi_root* mi_reset_prob(struct mi_root* cmd, void* param )
{

	*probability = initial;
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN );
}

static struct mi_root* mi_get_prob(struct mi_root* cmd, void* param )
{
	struct mi_root* rpl_tree= NULL;
	struct mi_node* node= NULL;
	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN );
	if(rpl_tree == NULL)
		return 0;
	node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "actual probability: %u percent\n",(*probability));
	if(node == NULL)
		goto error;
	
	return rpl_tree;

error:
	free_mi_tree(rpl_tree);
	return 0;
}

static struct mi_root* mi_get_hash(struct mi_root* cmd, void* param )
{
	struct mi_root* rpl_tree= NULL;
	struct mi_node* node= NULL;
	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN );
	if(rpl_tree == NULL)
		return 0;
	node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "%.*s\n", MD5_LEN, config_hash);
	if(node == NULL)
		goto error;
	
	return rpl_tree;

error:
	free_mi_tree(rpl_tree);
	return 0;
}

static struct mi_root* mi_check_hash(struct mi_root* cmd, void* param )
{
	struct mi_root* rpl_tree= NULL;
	struct mi_node* node= NULL;
	char tmp[MD5_LEN];
	memset(tmp, 0, MD5_LEN);

	if (MD5File(tmp, hash_file) != 0) {
		LM_ERR("could not hash the config file");
		rpl_tree = init_mi_tree( 500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN );
	}
	
	if (strncmp(config_hash, tmp, MD5_LEN) == 0) {
		rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN );
		if(rpl_tree == NULL)
			return 0;
		node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "The actual config file hash is identical to the stored one.\n");
	} else {
		rpl_tree = init_mi_tree( 400, "Error", 5 );
		if(rpl_tree == NULL)
			return 0;
		node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "The actual config file hash is not identical to the stored one.\n");
	}

	if(node == NULL)
		goto error;
	
	return rpl_tree;

error:
	free_mi_tree(rpl_tree);
	return 0;
}

static int set_prob(struct sip_msg *bar, char *percent_par, char *foo) 
{
	*probability=(int)(long)percent_par;
	return 1;
}
	
static int reset_prob(struct sip_msg *bar, char *percent_par, char *foo)
{
	*probability=initial;
	return 1;
}

static int get_prob(struct sip_msg *bar, char *foo1, char *foo2)
{
	return *probability;
}

static int rand_event(struct sip_msg *bar, char *foo1, char *foo2)
{
	double tmp = ((double) rand() / RAND_MAX);
	LM_DBG("generated random %f\n", tmp);
	LM_DBG("my pid is %d", getpid());
	if (tmp < ((double) (*probability) / 100)) {
		LM_DBG("return true\n");
		return 1;
	}
	else {
		LM_DBG("return false\n");
		return -1;
	}
}

static int pv_get_random_val(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	int n;
	int l = 0;
	char *ch;

	if(msg==NULL || res==NULL)
		return -1;
	n = rand();

	ch = int2str(n , &l);

	res->rs.s = ch;
	res->rs.len = l;

	res->ri = n;
	res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;

	return 0;
}

static int m_sleep(struct sip_msg *msg, char *time, char *str2)
{
	LM_DBG("sleep %lu seconds\n", (unsigned long)time);
	sleep((unsigned int)(unsigned long)time);
	return 1;
}

static int m_usleep(struct sip_msg *msg, char *time, char *str2)
{
	LM_DBG("sleep %lu microseconds\n", (unsigned long)time);
	sleep_us((unsigned int)(unsigned long)time);
	return 1;
}

static int dbg_abort(struct sip_msg* msg, char* foo, char* bar)
{
	LM_CRIT("abort called\n");
	abort();
	return 0;
}

static int dbg_pkg_status(struct sip_msg* msg, char* foo, char* bar)
{
	pkg_status();
	return 1;
}

static int dbg_shm_status(struct sip_msg* msg, char* foo, char* bar)
{
	shm_status();
	return 1;
}

static int mod_init(void)
{
	if (!hash_file)
		hash_file = cfg_file;

	if (MD5File(config_hash, hash_file) != 0) {
		LM_ERR("could not hash the config file");
		return -1;
	}
	LM_DBG("config file hash is %.*s", MD5_LEN, config_hash);

	if (initial > 100) {
		LM_ERR("invalid probability <%d>\n", initial);
		return -1;
	}
	LM_DBG("initial probability %d percent\n", initial);

	probability=(int *) shm_malloc(sizeof(int));

	if (!probability) {
		LM_ERR("no shmem available\n");
		return -1;
	}
	*probability = initial;

	if(init_shvars()<0)
	{
		LM_ERR("init shvars failed\n");
		shm_free(probability);
		return -1;
	}
	LM_INFO("module initialized, pid [%d]\n", getpid());

	return 0;
}


static void mod_destroy(void)
{
	if (probability)
		shm_free(probability);
	shvar_destroy_locks();
	destroy_shvars();
}


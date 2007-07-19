/*
 * $Id: gflags.c,v 1.1.1.1 2005/06/13 16:47:38 bogdan_iancu Exp $
 *
 * Copyright (C) 2007 1und1 Internet AG
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

#include "../../sr_module.h"
#include "../../error.h"
#include "../../items.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../mi/mi.h"
#include "../../ut.h"
#include "../../mod_fix.h"
#include <stdlib.h>

MODULE_VERSION

static int set_prob(struct sip_msg*, char *, char *);
static int reset_prob(struct sip_msg*, char *, char *);
static int get_prob(struct sip_msg*, char *, char *);
static int rand_event(struct sip_msg*, char *, char *);
static int m_sleep(struct sip_msg*, char *, char *);
static int m_usleep(struct sip_msg*, char *, char *);

static struct mi_root* mi_set_prob(struct mi_root* cmd, void* param );
static struct mi_root* mi_reset_prob(struct mi_root* cmd, void* param );
static struct mi_root* mi_get_prob(struct mi_root* cmd, void* param );

static int it_get_random_val(struct sip_msg *msg, xl_value_t *res, xl_param_t *param, int flags);

static int fixup_prob( void** param, int param_no);

static int mod_init(void);
static void mod_destroy(void);

static int initial = 10;
static int *probability;

static cmd_export_t cmds[]={
	{"rand_set_prob", /* action name as in scripts */
		set_prob,  /* C function name */
		1,          /* number of parameters */
		fixup_prob,          /* */
		/* can be applied to original/failed requests and replies */
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE}, 
	{"rand_reset_prob", reset_prob, 0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE}, 
	{"rand_get_prob", get_prob, 0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"rand_event", rand_event, 0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"sleep",    m_sleep,    1,      fixup_str2int, 
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"usleep",   m_usleep,   1,      fixup_str2int, 
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{0, 0, 0, 0, 0}
};

static param_export_t params[]={ 
	{"initial_probability", INT_PARAM, &initial},
	{0,0,0} 
};

static mi_export_t mi_cmds[] = {
	{ FIFO_SET_PROB,   mi_set_prob,   0,                 0,  0 },
	{ FIFO_RESET_PROB, mi_reset_prob, MI_NO_INPUT_FLAG,  0,  0 },
	{ FIFO_GET_PROB,   mi_get_prob,   MI_NO_INPUT_FLAG,  0,  0 },
	{ 0, 0, 0, 0, 0}
};

static item_export_t mod_items[] = {
	{ "RANDOM",    it_get_random_val,    100, {{0, 0}, 0, 0} },
	{ 0, 0, 0, {{0, 0}, 0, 0} }
};

struct module_exports exports = {
	"cfgutils",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,        /* exported functions */
	params,      /* exported parameters */
	0,           /* exported statistics */
	mi_cmds,     /* exported MI functions */
	mod_items,   /* exported pseudo-variables */
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

	if( strno2int( &node->value, &percent) <0)
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

static int set_prob(struct sip_msg *bar, char *percent_par, char *foo) 
{
	*probability=(int) percent_par;
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
	/* 
	 * use drand48(), as these generator provide much more
	 * uniformly distributed numbers as rand()
	 */
	double tmp = drand48();
	LM_DBG("generated random %f\n", tmp);
	if (tmp < ((double) (*probability) / 100)) {
		LM_DBG("return true\n");
		return 1;
	}
	else {
		LM_DBG("return false\n");
		return -1;
	}
}

static int it_get_random_val(struct sip_msg *msg, xl_value_t *res, xl_param_t *param, int flags)
{
	int n;
	int l = 0;
	char *ch;

	if(msg==NULL || res==NULL)
		return -1;
	n = lrand48();

	ch = int2str(n , &l);

	res->rs.s = ch;
	res->rs.len = l;

	res->ri = n;
	res->flags = XL_VAL_STR|XL_VAL_INT|XL_TYPE_INT;

	return 0;
}

static int m_sleep(struct sip_msg *msg, char *time, char *str2)
{
	LM_DBG("sleep %d seconds\n", time);
	sleep((int)time);
	return 1;
}

static int m_usleep(struct sip_msg *msg, char *time, char *str2)
{
	LM_DBG("sleep %d microseconds\n", time);
	sleep_us((int)time);
	return 1;
}

static int mod_init(void)
{
	// rand is seeded from urandom, so use this as seed for the rand48 PRNG
	srand48(rand());
	probability=(int *) shm_malloc(sizeof(int));
	if (!probability) {
		LM_ERR("no shmem available\n");
		return -1;
	}
	*probability = initial;
	if (initial > 100) {
		LM_ERR("invalid probability <%d>\n", initial);
		return -1;
	}
	LM_DBG("initial probability %d percent\n", initial);

	LM_INFO("module initialized, pid [%d]\n", getpid());

	return 0;
}


static void mod_destroy()
{
	if (probability)
		shm_free(probability);
}

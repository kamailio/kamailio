/*
 * utils Module
 *
 * Copyright (C) 2008 Juha Heinanen
 * Copyright (C) 2009 1&1 Internet AG
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2008-11-24: Introduced utils module and its first function: http_query.
 *
 */


#include <curl/curl.h>

#include "../../mod_fix.h"
#include "../../sr_module.h"
#include "../../ut.h"
#include "../../forward.h"
#include "../../resolve.h"
#include "../../locking.h"
#include "../../script_cb.h"
#include "../../mem/shm_mem.h"

#include "functions.h"
#include "conf.h"


MODULE_VERSION


/* module parameters */
static int forward_active = 0;
static int   mp_max_id = 0;
static char* mp_switch = "";
static char* mp_filter = "";
static char* mp_proxy  = "";

/* lock for configuration access */
static gen_lock_t *conf_lock = NULL;


/* FIFO interface functions */
static struct mi_root* forward_fifo_list(struct mi_root* cmd_tree, void *param);
static struct mi_root* forward_fifo_switch(struct mi_root* cmd_tree, void* param);
static struct mi_root* forward_fifo_filter(struct mi_root* cmd_tree, void* param);
static struct mi_root* forward_fifo_proxy(struct mi_root* cmd_tree, void* param);


/* Module management function prototypes */
static int mod_init(void);
static void destroy(void);


/* Module parameter variables */
int http_query_timeout = 4;


/* Fixup functions to be defined later */
static int fixup_http_query(void** param, int param_no);
static int fixup_free_http_query(void** param, int param_no);

/* forward function */
int utils_forward(struct sip_msg *msg, int id, int proto);

/* Exported functions */
static cmd_export_t cmds[] = {
    {"http_query", (cmd_function)http_query, 2, fixup_http_query,
     fixup_free_http_query,
     REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
    {0, 0, 0, 0, 0, 0}
};


/* Exported parameters */
static param_export_t params[] = {
    {"http_query_timeout", INT_PARAM, &http_query_timeout},
    {"forward_active",     INT_PARAM, &forward_active},
    {0, 0, 0}
};

static mi_export_t mi_cmds[] = {
	{ "forward_list",   forward_fifo_list,   MI_NO_INPUT_FLAG, 0,  0 },
	{ "forward_switch", forward_fifo_switch, 0, 0,  0 },
	{ "forward_filter", forward_fifo_filter, 0, 0,  0 },
	{ "forward_proxy",  forward_fifo_proxy,  0, 0,  0 },
	{ 0, 0, 0, 0, 0}
};

/* Module interface */
struct module_exports exports = {
    "utils",
    DEFAULT_DLFLAGS, /* dlopen flags */
    cmds,      /* Exported functions */
    params,    /* Exported parameters */
    0,         /* exported statistics */
    mi_cmds,   /* exported MI functions */
    0,         /* exported pseudo-variables */
    0,         /* extra processes */
    mod_init,  /* module initialization function */
    0,         /* response function*/
    destroy,   /* destroy function */
    0          /* per-child init function */
};


static int init_shmlock(void)
{
	conf_lock = lock_alloc();
	if (conf_lock == NULL) {
		LM_CRIT("cannot allocate memory for lock.\n");
		return -1;
	}
	if (lock_init(conf_lock) == 0) {
		LM_CRIT("cannot initialize lock.\n");
		return -1;
	}

	return 0;
}


static int pre_script_filter(struct sip_msg *msg, void *unused)
{
	/* use id 0 for pre script callback */
	utils_forward(msg, 0, PROTO_UDP);

	/* always return 1 so that routing skript is called for msg */
	return 1;
}


static void destroy_shmlock(void)
{
	if (conf_lock) {
		lock_destroy(conf_lock);
		lock_dealloc((void *)conf_lock);
		conf_lock = NULL;
	}
}


/* Module initialization function */
static int mod_init(void)
{
	/* Initialize curl */
	if (curl_global_init(CURL_GLOBAL_ALL)) {
		LM_ERR("curl_global_init failed\n");
		return -1;
	}


	if (init_shmlock() != 0) {
		LM_CRIT("cannot initialize shmlock.\n");
		return -1;
	}

	if (conf_init(mp_max_id) < 0) {
		LM_CRIT("cannot initialize configuration.\n");
		return -1;
	}

	/* read module parameters and update configuration structure */
	if (conf_parse_proxy(mp_proxy) < 0) {
		LM_CRIT("cannot parse proxy module parameter.\n");
		return -1;
	}
	if (conf_parse_filter(mp_filter) < 0) {
		LM_CRIT("cannot parse filter module parameter.\n");
		return -1;
	}
	if (conf_parse_switch(mp_switch) < 0) {
		LM_CRIT("cannot parse switch module parameter.\n");
		return -1;
	}

	if (forward_active == 1) {
		/* register callback for id 0 */
		if (register_script_cb(pre_script_filter, PRE_SCRIPT_CB|REQ_TYPE_CB, 0) < 0) {
			LM_CRIT("cannot register script callback for requests.\n");
			return -1;
		}
		if (register_script_cb(pre_script_filter, PRE_SCRIPT_CB|RPL_TYPE_CB, 0) < 0) {
			LM_CRIT("cannot register script callback for replies.\n");
			return -1;
		}
	} else {
		LM_INFO("forward functionality disabled");
	}
	return 0;
}


static void destroy(void)
{
	/* Cleanup curl */
	curl_global_cleanup();
	/* Cleanup forward */
	conf_destroy();
	destroy_shmlock();
}


/* Fixup functions */

/*
 * Fix http_query params: url (string that may contain pvars) and
 * result (writable pvar).
 */
static int fixup_http_query(void** param, int param_no)
{
    if (param_no == 1) {
	return fixup_spve_null(param, 1);
    }

    if (param_no == 2) {
	if (fixup_pvar(param) != 0) {
	    LM_ERR("failed to fixup result pvar\n");
	    return -1;
	}
	if (((pv_spec_t *)(*param))->setf == NULL) {
	    LM_ERR("result pvar is not writeble\n");
	    return -1;
	}
	LM_INFO("leaving fixup_http_query\n");
	return 0;
    }

    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

/*
 * Free http_query params.
 */
static int fixup_free_http_query(void** param, int param_no)
{
    if (param_no == 1) {
	LM_WARN("free function has not been defined for spve\n");
	return 0;
    }

    if (param_no == 2) {
	return fixup_free_pvar_null(param, 1);
    }
    
    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}


/*!
 * \brief checks precondition, switch, filter and forwards msg if necessary
 * \param msg the message to be forwarded
 * \param id use configuration with this ID when checking switch, filter, proxy.
 * \param proto protocol to be used. Should be PROTO_UDP.
 *  \return 0 on success, -1 otherwise
 */
int utils_forward(struct sip_msg *msg, int id, int proto)
{
	int ret = -1;
	struct dest_info dst;

	init_dest_info(&dst);
	dst.proto = proto;

	// critical section start:
	// avoids dirty reads when updating configuration.
	lock_get(conf_lock);

	struct proxy_l *proxy = conf_needs_forward(msg, id);

	if (proxy != NULL) {
		proxy2su(&dst.to, proxy);
		if (forward_request(msg, NULL, 0, &dst) < 0){
			LM_ERR("could not forward message\n");
		}
		ret = 0;
	}

	// critical section end
	lock_release(conf_lock);

	return ret;
}


/* FIFO functions */

/*!
 * \brief fifo command for listing configuration
 * \return pointer to the mi_root on success, 0 otherwise
 */
static struct mi_root* forward_fifo_list(struct mi_root* cmd_tree, void *param)
{
	struct mi_node *node = NULL;
	struct mi_root * ret = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	if(ret == NULL)
		return 0;

	node = addf_mi_node_child( &ret->node, 0, 0, 0, "Printing forwarding information:");
	if(node == NULL)
		goto error;

	// critical section start:
	// avoids dirty reads when updating configuration.
	lock_get(conf_lock);

	conf_show(ret);

	// critical section end
	lock_release(conf_lock);

	return ret;

error:
	free_mi_tree(ret);
	return 0;
}


/*!
 * \brief fifo command for configuring switch
 * \return pointer to the mi_root on success, 0 otherwise
 */
static struct mi_root* forward_fifo_switch(struct mi_root* cmd_tree, void* param)
{
	struct mi_node *node = NULL;
	int result;

	node = cmd_tree->node.kids;
	if (node==NULL || node->next!=NULL || node->value.s==NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	// critical section start:
	// avoids dirty reads when updating configuration.
	lock_get(conf_lock);

	result = conf_parse_switch(node->value.s);

	// critical section end
	lock_release(conf_lock);

	if (result < 0) {
		LM_ERR("cannot parse parameter\n");
		return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
	}
	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}


/*!
 * \brief fifo command for configuring filter
 * \return pointer to the mi_root on success, 0 otherwise
 */
static struct mi_root* forward_fifo_filter(struct mi_root* cmd_tree, void* param)
{
	struct mi_node *node = NULL;
	int result;

	node = cmd_tree->node.kids;
	if (node==NULL || node->next!=NULL || node->value.s==NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	// critical section start:
	//   avoids dirty reads when updating configuration.
	lock_get(conf_lock);

	result = conf_parse_filter(node->value.s);

	// critical section end
	lock_release(conf_lock);

	if (result < 0) {
		LM_ERR("cannot parse parameter\n");
		return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
	}
	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}


/*!
 * \brief fifo command for configuring proxy
 * \return pointer to the mi_root on success, 0 otherwise
 */
static struct mi_root* forward_fifo_proxy(struct mi_root* cmd_tree, void* param)
{
	struct mi_node *node = NULL;
	int result;

	node = cmd_tree->node.kids;
	if (node==NULL || node->next!=NULL || node->value.s==NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	// critical section start:
	//   avoids dirty reads when updating configuration.
	lock_get(conf_lock);

	result = conf_parse_proxy(node->value.s);

	// critical section end
	lock_release(conf_lock);

	if (result < 0) {
		LM_ERR("cannot parse parameter\n");
		return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
	}
	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}

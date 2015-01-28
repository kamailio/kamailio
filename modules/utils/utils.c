/*
 * utils Module
 *
 * Copyright (C) 2008 Juha Heinanen
 * Copyright (C) 2009 1&1 Internet AG
 * Copyright (C) 2013 Carsten Bock, ng-voice GmbH
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
 * \brief Kamailio utils :: Module core
 * \ingroup utils
 */

/*! \defgroup Utils Kamailio :: Various utilities
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
#include "../../lib/srdb1/db.h"

#include "functions.h"
#include "conf.h"
#include "xcap_auth.h"


MODULE_VERSION

#define XCAP_TABLE_VERSION 4

/* Module parameter variables */
int http_query_timeout = 4;
static int forward_active = 0;
static int   mp_max_id = 0;
static char* mp_switch = "";
static char* mp_filter = "";
static char* mp_proxy  = "";
str xcap_table= str_init("xcap");
str pres_db_url = {0, 0};

/* lock for configuration access */
static gen_lock_t *conf_lock = NULL;

/* FIFO interface functions */
static struct mi_root* forward_fifo_list(struct mi_root* cmd_tree, void *param);
static struct mi_root* forward_fifo_switch(struct mi_root* cmd_tree, void* param);
static struct mi_root* forward_fifo_filter(struct mi_root* cmd_tree, void* param);
static struct mi_root* forward_fifo_proxy(struct mi_root* cmd_tree, void* param);

/* Database connection */
db1_con_t *pres_dbh = NULL;
db_func_t pres_dbf;

/* Module management function prototypes */
static int mod_init(void);
static int child_init(int);
static void destroy(void);

/* Fixup functions to be defined later */
static int fixup_http_query_get(void** param, int param_no);
static int fixup_free_http_query_get(void** param, int param_no);
static int fixup_http_query_post(void** param, int param_no);
static int fixup_free_http_query_post(void** param, int param_no);

/* Wrappers for http_query to be defined later */
static int w_http_query(struct sip_msg* _m, char* _url, char* _result);
static int w_http_query_post(struct sip_msg* _m, char* _url, char* _post, char* _result);

/* forward function */
int utils_forward(struct sip_msg *msg, int id, int proto);

/* Exported functions */
static cmd_export_t cmds[] = {
    {"http_query", (cmd_function)w_http_query, 2, fixup_http_query_get,
     fixup_free_http_query_get,
     REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
    {"http_query", (cmd_function)w_http_query_post, 3, fixup_http_query_post,
     fixup_free_http_query_post,
     REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
    {"xcap_auth_status", (cmd_function)xcap_auth_status, 2, fixup_pvar_pvar,
     fixup_free_pvar_pvar, REQUEST_ROUTE},
    {0, 0, 0, 0, 0, 0}
};


/* Exported parameters */
static param_export_t params[] = {
    {"pres_db_url", PARAM_STR, &pres_db_url},
    {"xcap_table", PARAM_STR, &xcap_table},
    {"http_query_timeout", INT_PARAM, &http_query_timeout},
    {"forward_active", INT_PARAM, &forward_active},
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
    child_init /* per-child init function */
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


static int pre_script_filter(struct sip_msg *msg, unsigned int flags, void *unused)
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


static void pres_db_close(void) {
    if (pres_dbh) {
	pres_dbf.close(pres_dbh);
	pres_dbh = NULL;
    }
}

static int pres_db_init(void) {
    if (!pres_db_url.s || !pres_db_url.len) {
	LM_INFO("xcap_auth_status function is disabled\n");
	return 0;
    }
    if (db_bind_mod(&pres_db_url, &pres_dbf) < 0) {
	LM_ERR("can't bind database module\n");
	return -1;
    }
    if ((pres_dbh = pres_dbf.init(&pres_db_url)) == NULL) {
	LM_ERR("can't connect to database\n");
	return -1;
    }
    if (db_check_table_version(&pres_dbf, pres_dbh, &xcap_table,
			       XCAP_TABLE_VERSION) < 0) {
	LM_ERR("during table version check\n");
	pres_db_close();
	return -1;
    }
    pres_db_close();
    return 0;
}

static int pres_db_open(void) {
    if (!pres_db_url.s || !pres_db_url.len) {
	return 0;
    }
    if (pres_dbh) {
	pres_dbf.close(pres_dbh);
    }
    if ((pres_dbh = pres_dbf.init(&pres_db_url)) == NULL) {
	LM_ERR("can't connect to database\n");
	return -1;
    }
    if (pres_dbf.use_table(pres_dbh, &xcap_table) < 0) {
	LM_ERR("in use_table: %.*s\n", xcap_table.len, xcap_table.s);
	return -1;
    }
    return 0;
}


/* Module initialization function */
static int mod_init(void)
{
	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

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
		if (register_script_cb(pre_script_filter, PRE_SCRIPT_CB|ONREPLY_CB, 0) < 0) {
			LM_CRIT("cannot register script callback for requests.\n");
			return -1;
		}
		if (register_script_cb(pre_script_filter, PRE_SCRIPT_CB|ONREPLY_CB, 0) < 0) {
			LM_CRIT("cannot register script callback for replies.\n");
			return -1;
		}
	} else {
		LM_INFO("forward functionality disabled");
	}

	/* presence database */
	LM_DBG("pres_db_url=%s/%d/%p\n", ZSW(pres_db_url.s), pres_db_url.len,
	       pres_db_url.s);

	if(pres_db_init() < 0) {
	    return -1;
	}

	return 0;
}


/* Child initialization function */
static int child_init(int rank)
{	
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

    return pres_db_open();
}

static void destroy(void)
{
	/* Cleanup curl */
	curl_global_cleanup();
	/* Cleanup forward */
	conf_destroy();
	destroy_shmlock();
	/* Close pres db */
	pres_db_close();
}


/* Fixup functions */

/*
 * Fix http_query params: url (string that may contain pvars) and
 * result (writable pvar).
 */
static int fixup_http_query_get(void** param, int param_no)
{
    if (param_no == 1) {
	return fixup_spve_null(param, 1);
    }

    if (param_no == 2) {
	if (fixup_pvar_null(param, 1) != 0) {
	    LM_ERR("failed to fixup result pvar\n");
	    return -1;
	}
	if (((pv_spec_t *)(*param))->setf == NULL) {
	    LM_ERR("result pvar is not writeble\n");
	    return -1;
	}
	return 0;
    }

    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

/*
 * Free http_query params.
 */
static int fixup_free_http_query_get(void** param, int param_no)
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


/*
 * Fix http_query params: url (string that may contain pvars) and
 * result (writable pvar).
 */
static int fixup_http_query_post(void** param, int param_no)
{
    if ((param_no == 1) || (param_no == 2)) {
	return fixup_spve_null(param, 1);
    }

    if (param_no == 3) {
	if (fixup_pvar_null(param, 1) != 0) {
	    LM_ERR("failed to fixup result pvar\n");
	    return -1;
	}
	if (((pv_spec_t *)(*param))->setf == NULL) {
	    LM_ERR("result pvar is not writeble\n");
	    return -1;
	}
	return 0;
    }

    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

/*
 * Free http_query params.
 */
static int fixup_free_http_query_post(void** param, int param_no)
{
    if ((param_no == 1) || (param_no == 2)) {
	LM_WARN("free function has not been defined for spve\n");
	return 0;
    }

    if (param_no == 3) {
	return fixup_free_pvar_null(param, 1);
    }
    
    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

/*
 * Wrapper for HTTP-Query (GET)
 */
static int w_http_query(struct sip_msg* _m, char* _url, char* _result) {
	return http_query(_m, _url, _result, NULL);
}


/*
 * Wrapper for HTTP-Query (POST-Variant)
 */
static int w_http_query_post(struct sip_msg* _m, char* _url, char* _post, char* _result) {
	return http_query(_m, _url, _result, _post);
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

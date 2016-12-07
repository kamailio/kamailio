/*
 * xcap_client module - XCAP client for Kamailio
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <curl/curl.h>

#include "../../pt.h"
#include "../../lib/srdb1/db.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../lib/kmi/mi.h"
#include "../presence/utils_func.h"
#include "xcap_functions.h"
#include "xcap_client.h"

MODULE_VERSION

#define XCAP_TABLE_VERSION   4

static int mod_init(void);
static int child_init(int rank);
static void destroy(void);
struct mi_root* refreshXcapDoc(struct mi_root* cmd, void* param);
int get_auid_flag(str auid);
str xcap_db_table = str_init("xcap");
str xcap_db_url = str_init(DEFAULT_DB_URL);
xcap_callback_t* xcapcb_list= NULL;
int periodical_query= 1;
unsigned int query_period= 100;

str str_source_col = str_init("source");
str str_path_col = str_init("path");
str str_doc_col = str_init("doc");
str str_etag_col = str_init("etag");
str str_username_col = str_init("username");
str str_domain_col = str_init("domain");
str str_doc_type_col = str_init("doc_type");
str str_doc_uri_col = str_init("doc_uri");
str str_port_col = str_init("port");


/* database connection */
db1_con_t *xcap_db = NULL;
db_func_t xcap_dbf;

void query_xcap_update(unsigned int ticks, void* param);

static param_export_t params[]={
	{ "db_url",					PARAM_STR,         &xcap_db_url    },
	{ "xcap_table",				PARAM_STR,         &xcap_db_table  },
	{ "periodical_query",		INT_PARAM,         &periodical_query },
	{ "query_period",	       	INT_PARAM,         &query_period     },
	{    0,                     0,                      0            }
};


static cmd_export_t  cmds[]=
{	
	{"bind_xcap",  (cmd_function)bind_xcap,  1,    0, 0,            0},
	{    0,                     0,           0,    0, 0,           0}
};

static mi_export_t mi_cmds[] = {
	{ "refreshXcapDoc", refreshXcapDoc,      0,  0,  0},
	{ 0,                 0,                  0,  0,  0}
};

/** module exports */
struct module_exports exports= {
	"xcap_client",				/* module name */
	DEFAULT_DLFLAGS,			/* dlopen flags */
	cmds,  						/* exported functions */
	params,						/* exported parameters */
	0,      					/* exported statistics */
	mi_cmds,   					/* exported MI functions */
	0,							/* exported pseudo-variables */
	0,							/* extra processes */
	mod_init,					/* module initialization function */
	0,							/* response handling function */
	(destroy_function) destroy, /* destroy function */
	child_init					/* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}
	
	/* binding to mysql module  */
	if (db_bind_mod(&xcap_db_url, &xcap_dbf))
	{
		LM_ERR("Database module not found\n");
		return -1;
	}
	
	if (!DB_CAPABILITY(xcap_dbf, DB_CAP_ALL)) {
		LM_ERR("Database module does not implement all functions"
				" needed by the module\n");
		return -1;
	}

	xcap_db = xcap_dbf.init(&xcap_db_url);
	if (!xcap_db)
	{
		LM_ERR("while connecting to database\n");
		return -1;
	}

	if(db_check_table_version(&xcap_dbf, xcap_db, &xcap_db_table,
				XCAP_TABLE_VERSION) < 0) {
		LM_ERR("error during table version check.\n");
		return -1;
	}
	xcap_dbf.close(xcap_db);
	xcap_db = NULL;

	curl_global_init(CURL_GLOBAL_ALL);

	if(periodical_query)
	{
		register_timer(query_xcap_update, 0, query_period);
	}
	return 0;
}

static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if((xcap_db = xcap_dbf.init(&xcap_db_url))==NULL)
	{
		LM_ERR("cannot connect to db\n");
		return -1;
	}
	return 0;
}

static void destroy(void)
{
	curl_global_cleanup();
	if(xcap_db != NULL)
		xcap_dbf.close(xcap_db);
}

void query_xcap_update(unsigned int ticks, void* param)
{
	db_key_t query_cols[3], update_cols[3];
	db_val_t query_vals[3], update_vals[3];
	db_key_t result_cols[7];
	int n_result_cols = 0, n_query_cols= 0, n_update_cols= 0;
	db1_res_t* result= NULL;
	int user_col, domain_col, doc_type_col, etag_col, doc_uri_col, port_col; 
	db_row_t *row ;	
	db_val_t *row_vals ;
	unsigned int port;
	char* etag, *path, *new_etag= NULL, *doc= NULL;
	int u_doc_col, u_etag_col;
	str user, domain, uri;
	int i;

	/* query the ones I have to handle */
	query_cols[n_query_cols] = &str_source_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= XCAP_CL_MOD;
	n_query_cols++;

	query_cols[n_query_cols] = &str_path_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;

	update_cols[u_doc_col=n_update_cols] = &str_doc_col;
	update_vals[n_update_cols].type = DB1_STRING;
	update_vals[n_update_cols].nul = 0;
	n_update_cols++;

	update_cols[u_etag_col=n_update_cols] = &str_etag_col;
	update_vals[n_update_cols].type = DB1_STRING;
	update_vals[n_update_cols].nul = 0;
	n_update_cols++;

	result_cols[user_col= n_result_cols++]     = &str_username_col;
	result_cols[domain_col=n_result_cols++]    = &str_domain_col;
	result_cols[doc_type_col=n_result_cols++]  = &str_doc_type_col;
	result_cols[etag_col=n_result_cols++]      = &str_etag_col;
	result_cols[doc_uri_col= n_result_cols++]  = &str_doc_uri_col;
	result_cols[port_col= n_result_cols++]     = &str_port_col;
	
	if (xcap_dbf.use_table(xcap_db, &xcap_db_table) < 0) 
	{
		LM_ERR("in use_table-[table]= %.*s\n", xcap_db_table.len, xcap_db_table.s);
		goto error;
	}

	if(xcap_dbf.query(xcap_db, query_cols, 0, query_vals, result_cols, 1,
				n_result_cols, 0, &result)< 0)
	{
		LM_ERR("in sql query\n");
		goto error;
	}
	if(result== NULL)
	{
		LM_ERR("in sql query- null result\n");
		return;
	}
	if(result->n<= 0)
	{
		xcap_dbf.free_result(xcap_db, result);
		return;
	}
	n_query_cols++;
	
	/* ask if updated */
	for(i= 0; i< result->n; i++)
	{
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);
	
		path= (char*)row_vals[doc_uri_col].val.string_val;
		port= row_vals[port_col].val.int_val;
		etag= (char*)row_vals[etag_col].val.string_val;	

		user.s= (char*)row_vals[user_col].val.string_val;
		user.len= strlen(user.s);

		domain.s= (char*)row_vals[domain_col].val.string_val;
		domain.len= strlen(domain.s);

		/* send HTTP request */
		doc= send_http_get(path, port, etag, IF_NONE_MATCH, &new_etag);
		if(doc== NULL)
		{
			LM_DBG("document not update\n");
			continue;
		}
		if(new_etag== NULL)
		{
			LM_ERR("etag not found\n");
			pkg_free(doc);
			goto error;
		}
		/* update in xcap db table */
		update_vals[u_doc_col].val.string_val= doc;
		update_vals[u_etag_col].val.string_val= etag;
		
		if(xcap_dbf.update(xcap_db, query_cols, 0, query_vals, update_cols,
					update_vals, n_query_cols, n_update_cols)< 0)
		{
			LM_ERR("in sql update\n");
			pkg_free(doc);
			goto error;
		}
		/* call registered callbacks */
		if(uandd_to_uri(user, domain, &uri)< 0)
		{
			LM_ERR("converting user and domain to uri\n");
			pkg_free(doc);
			goto error;
		}
		run_xcap_update_cb(row_vals[doc_type_col].val.int_val, uri, doc);
		pkg_free(doc);

	}

	xcap_dbf.free_result(xcap_db, result);
	return;

error:
	if(result)
		xcap_dbf.free_result(xcap_db, result);
}

int parse_doc_url(str doc_url, char** serv_addr, xcap_doc_sel_t* doc_sel)
{
	char* sl, *str_type;	
	
	sl= strchr(doc_url.s, '/');
	*sl= '\0';
	*serv_addr= doc_url.s;
	
	sl++;
	doc_sel->auid.s= sl;
	sl= strchr(sl, '/');
	doc_sel->auid.len= sl- doc_sel->auid.s;
	
	sl++;
	str_type= sl;
	sl= strchr(sl, '/');
	*sl= '\0';

	if(strcasecmp(str_type, "users")== 0)
		doc_sel->type= USERS_TYPE;
	else
	if(strcasecmp(str_type, "group")== 0)
		doc_sel->type= GLOBAL_TYPE;

	sl++;

	return 0;

}
/*
 * mi cmd: refreshXcapDoc
 *			<document uri> 
 *			<xcap_port>
 * */

struct mi_root* refreshXcapDoc(struct mi_root* cmd, void* param)
{
	struct mi_node* node= NULL;
	str doc_url;
	xcap_doc_sel_t doc_sel;
	char* serv_addr;
	char* stream= NULL;
	int type;
	unsigned int xcap_port;
	char* etag= NULL;

	node = cmd->node.kids;
	if(node == NULL)
		return 0;

	doc_url = node->value;
	if(doc_url.s == NULL || doc_url.len== 0)
	{
		LM_ERR("empty uri\n");
		return init_mi_tree(404, "Empty document URL", 20);
	}
	node= node->next;
	if(node== NULL)
		return 0;
	if(node->value.s== NULL || node->value.len== 0)
	{
		LM_ERR("port number\n");
		return init_mi_tree(404, "Empty document URL", 20);
	}
	if(str2int(&node->value, &xcap_port)< 0)
	{
		LM_ERR("while converting string to int\n");
		goto error;
	}

	if(node->next!= NULL)
		return 0;

	/* send GET HTTP request to the server */
	stream=	send_http_get(doc_url.s, xcap_port, NULL, 0, &etag);
	if(stream== NULL)
	{
		LM_ERR("in http get\n");
		return 0;
	}
	
	/* call registered functions with document argument */
	if(parse_doc_url(doc_url, &serv_addr, &doc_sel)< 0)
	{
		LM_ERR("parsing document url\n");
		return 0;
	}

	type= get_auid_flag(doc_sel.auid);
	if(type< 0)
	{
		LM_ERR("incorect auid: %.*s\n",
				doc_sel.auid.len, doc_sel.auid.s);
		goto error;
	}

	run_xcap_update_cb(type, doc_sel.xid, stream);

	return init_mi_tree(200, "OK", 2);

error:
	if(stream)
		pkg_free(stream);
	return 0;
}

#define STR_MATCH(s1, s2)   ((s1).len==(s2).len && memcmp((s1).s, (s2).s, (s1).len)==0)

int get_auid_flag(str auid)
{
	static str pres_rules = str_init("pres-rules");
	static str rls_services = str_init("rls-services");

	if (STR_MATCH(auid, pres_rules))
		return PRES_RULES;
	else if (STR_MATCH(auid, rls_services))
		return RESOURCE_LIST;

	return -1;
}

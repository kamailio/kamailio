/**
 * $Id$
 *
 * dispatcher module -- stateless load balancing
 *
 * Copyright (C) 2004-2005 FhG Fokus
 * Copyright (C) 2006 Voice Sistem SRL
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
 * History
 * -------
 * 2004-07-31  first version, by daniel
 * 2007-01-11  Added a function to check if a specific gateway is in a group
 *				(carsten - Carsten Bock, BASIS AudioNet GmbH)
 * 2007-02-09  Added active probing of failed destinations and automatic
 *				re-enabling of destinations (carsten)
 * 2007-05-08  Ported the changes to SVN-Trunk and renamed ds_is_domain
 *				to ds_is_from_list.  (carsten)
 * 2007-07-18  Added support for load/reload groups from DB 
 * 			   reload triggered from ds_reload MI_Command (ancuta)
 */

/*! \file
 * \ingroup dispatcher
 * \brief Dispatcher :: Dispatch
 */

/*! \defgroup dispatcher Dispatcher :: Load balancing and failover module
 * 	The dispatcher module implements a set of functions for distributing SIP requests on a 
 *	set of servers, but also grouping of server resources.
 *
 *	- The module has an internal API exposed to other modules.
 *	- The module implements a couple of MI functions for managing the list of server resources
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../lib/kmi/mi.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../route.h"
#include "../../mem/mem.h"
#include "../../mod_fix.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"

#include "ds_ht.h"
#include "dispatch.h"
#include "config.h"
#include "api.h"

MODULE_VERSION

#define DS_SET_ID_COL			"setid"
#define DS_DEST_URI_COL			"destination"
#define DS_DEST_FLAGS_COL		"flags"
#define DS_DEST_PRIORITY_COL	"priority"
#define DS_DEST_ATTRS_COL		"attrs"
#define DS_TABLE_NAME			"dispatcher"

/** parameters */
char *dslistfile = CFG_DIR"dispatcher.list";
int  ds_force_dst   = 1;
int  ds_flags       = 0; 
int  ds_use_default = 0; 
static str dst_avp_param = {NULL, 0};
static str grp_avp_param = {NULL, 0};
static str cnt_avp_param = {NULL, 0};
static str dstid_avp_param = {NULL, 0};
static str attrs_avp_param = {NULL, 0};
str hash_pvar_param = {NULL, 0};

int_str dst_avp_name;
unsigned short dst_avp_type;
int_str grp_avp_name;
unsigned short grp_avp_type;
int_str cnt_avp_name;
unsigned short cnt_avp_type;
int_str dstid_avp_name;
unsigned short dstid_avp_type;
int_str attrs_avp_name;
unsigned short attrs_avp_type;

pv_elem_t * hash_param_model = NULL;

int probing_threshhold = 1; /* number of failed requests, before a destination
							   is taken into probing */
str ds_ping_method = {"OPTIONS",7};
str ds_ping_from   = {"sip:dispatcher@localhost", 24};
static int ds_ping_interval = 0;
int ds_probing_mode  = DS_PROBE_NONE;

static str ds_ping_reply_codes_str= {NULL, 0};
static int** ds_ping_reply_codes = NULL;
static int* ds_ping_reply_codes_cnt;

int ds_hash_size = 0;
int ds_hash_expire = 7200;
int ds_hash_initexpire = 7200;
int ds_hash_check_interval = 30;

str ds_outbound_proxy = {0, 0};

/* tm */
struct tm_binds tmb;

/*db */
str ds_db_url            = {NULL, 0};
str ds_set_id_col        = str_init(DS_SET_ID_COL);
str ds_dest_uri_col      = str_init(DS_DEST_URI_COL);
str ds_dest_flags_col    = str_init(DS_DEST_FLAGS_COL);
str ds_dest_priority_col = str_init(DS_DEST_PRIORITY_COL);
str ds_dest_attrs_col    = str_init(DS_DEST_ATTRS_COL);
str ds_table_name        = str_init(DS_TABLE_NAME);

str ds_setid_pvname   = {NULL, 0};
pv_spec_t ds_setid_pv;
str ds_attrs_pvname   = {NULL, 0};
pv_spec_t ds_attrs_pv;

/** module functions */
static int mod_init(void);
static int child_init(int);

static int ds_parse_reply_codes();
static int ds_init_rpc(void);

static int w_ds_select_dst(struct sip_msg*, char*, char*);
static int w_ds_select_domain(struct sip_msg*, char*, char*);
static int w_ds_next_dst(struct sip_msg*, char*, char*);
static int w_ds_next_domain(struct sip_msg*, char*, char*);
static int w_ds_mark_dst0(struct sip_msg*, char*, char*);
static int w_ds_mark_dst1(struct sip_msg*, char*, char*);
static int w_ds_load_unset(struct sip_msg*, char*, char*);
static int w_ds_load_update(struct sip_msg*, char*, char*);

static int w_ds_is_from_list0(struct sip_msg*, char*, char*);
static int w_ds_is_from_list1(struct sip_msg*, char*, char*);

static void destroy(void);

static int ds_warn_fixup(void** param, int param_no);

static struct mi_root* ds_mi_set(struct mi_root* cmd, void* param);
static struct mi_root* ds_mi_list(struct mi_root* cmd, void* param);
static struct mi_root* ds_mi_reload(struct mi_root* cmd_tree, void* param);
static int mi_child_init(void);

static cmd_export_t cmds[]={
	{"ds_select_dst",    (cmd_function)w_ds_select_dst,    2,
		fixup_igp_igp, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_select_domain", (cmd_function)w_ds_select_domain, 2,
		fixup_igp_igp, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_next_dst",      (cmd_function)w_ds_next_dst,      0,
		ds_warn_fixup, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_next_domain",   (cmd_function)w_ds_next_domain,   0,
		ds_warn_fixup, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"ds_mark_dst",      (cmd_function)w_ds_mark_dst0,     0,
		ds_warn_fixup, 0, REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"ds_mark_dst",      (cmd_function)w_ds_mark_dst1,     1,
		ds_warn_fixup, 0, REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"ds_is_from_list",  (cmd_function)w_ds_is_from_list0, 0,
		0, 0, REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"ds_is_from_list",  (cmd_function)w_ds_is_from_list1, 1,
		fixup_uint_null, 0, ANY_ROUTE},
	{"ds_load_unset",    (cmd_function)w_ds_load_unset,   0,
		0, 0, ANY_ROUTE},
	{"ds_load_update",   (cmd_function)w_ds_load_update,  0,
		0, 0, ANY_ROUTE},
	{"bind_dispatcher",   (cmd_function)bind_dispatcher,  0,
		0, 0, 0},
	{0,0,0,0,0,0}
};


static param_export_t params[]={
	{"list_file",       STR_PARAM, &dslistfile},
	{"db_url",		    STR_PARAM, &ds_db_url.s},
	{"table_name", 	    STR_PARAM, &ds_table_name.s},
	{"setid_col",       STR_PARAM, &ds_set_id_col.s},
	{"destination_col", STR_PARAM, &ds_dest_uri_col.s},
	{"flags_col",       STR_PARAM, &ds_dest_flags_col.s},
	{"priority_col",    STR_PARAM, &ds_dest_priority_col.s},
	{"attrs_col",       STR_PARAM, &ds_dest_attrs_col.s},
	{"force_dst",       INT_PARAM, &ds_force_dst},
	{"flags",           INT_PARAM, &ds_flags},
	{"use_default",     INT_PARAM, &ds_use_default},
	{"dst_avp",         STR_PARAM, &dst_avp_param.s},
	{"grp_avp",         STR_PARAM, &grp_avp_param.s},
	{"cnt_avp",         STR_PARAM, &cnt_avp_param.s},
	{"dstid_avp",       STR_PARAM, &dstid_avp_param.s},
	{"attrs_avp",       STR_PARAM, &attrs_avp_param.s},
	{"hash_pvar",       STR_PARAM, &hash_pvar_param.s},
	{"setid_pvname",    STR_PARAM, &ds_setid_pvname.s},
	{"attrs_pvname",    STR_PARAM, &ds_attrs_pvname.s},
	{"ds_probing_threshhold", INT_PARAM, &probing_threshhold},
	{"ds_ping_method",     STR_PARAM, &ds_ping_method.s},
	{"ds_ping_from",       STR_PARAM, &ds_ping_from.s},
	{"ds_ping_interval",   INT_PARAM, &ds_ping_interval},
	{"ds_ping_reply_codes", STR_PARAM, &ds_ping_reply_codes_str},
	{"ds_probing_mode",    INT_PARAM, &ds_probing_mode},
	{"ds_hash_size",       INT_PARAM, &ds_hash_size},
	{"ds_hash_expire",     INT_PARAM, &ds_hash_expire},
	{"ds_hash_initexpire", INT_PARAM, &ds_hash_initexpire},
	{"ds_hash_check_interval", INT_PARAM, &ds_hash_check_interval},
	{"outbound_proxy",  STR_PARAM, &ds_outbound_proxy.s},
	{0,0,0}
};


static mi_export_t mi_cmds[] = {
	{ "ds_set_state",   ds_mi_set,     0,                 0,  0            },
	{ "ds_list",        ds_mi_list,    MI_NO_INPUT_FLAG,  0,  0            },
	{ "ds_reload",      ds_mi_reload,  0,                 0,  mi_child_init},
	{ 0, 0, 0, 0, 0}
};


/** module exports */
struct module_exports exports= {
	"dispatcher",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,          /* exported statistics */
	mi_cmds,    /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	(destroy_function) destroy,
	child_init  /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	pv_spec_t avp_spec;

	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}
	if(ds_init_rpc()<0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if (dst_avp_param.s)
		dst_avp_param.len = strlen(dst_avp_param.s);
	if (grp_avp_param.s)
		grp_avp_param.len = strlen(grp_avp_param.s);
	if (cnt_avp_param.s)
		cnt_avp_param.len = strlen(cnt_avp_param.s);	
	if (dstid_avp_param.s)
		dstid_avp_param.len = strlen(dstid_avp_param.s);
	if (attrs_avp_param.s)
		attrs_avp_param.len = strlen(attrs_avp_param.s);
	if (hash_pvar_param.s)
		hash_pvar_param.len = strlen(hash_pvar_param.s);
	if (ds_setid_pvname.s)
		ds_setid_pvname.len = strlen(ds_setid_pvname.s);
	if (ds_attrs_pvname.s)
		ds_attrs_pvname.len = strlen(ds_attrs_pvname.s);
	if (ds_ping_from.s) ds_ping_from.len = strlen(ds_ping_from.s);
	if (ds_ping_method.s) ds_ping_method.len = strlen(ds_ping_method.s);
	if (ds_outbound_proxy.s) ds_outbound_proxy.len = strlen(ds_outbound_proxy.s);

	if(cfg_declare("dispatcher", dispatcher_cfg_def,
				&default_dispatcher_cfg, cfg_sizeof(dispatcher),
				&dispatcher_cfg)){
		LM_ERR("Fail to declare the configuration\n");
		return -1;
	}

	/* Initialize the counter */
	ds_ping_reply_codes = (int**)shm_malloc(sizeof(unsigned int*));
	*ds_ping_reply_codes = 0;
	ds_ping_reply_codes_cnt = (int*)shm_malloc(sizeof(int));
	*ds_ping_reply_codes_cnt = 0;
	if(ds_ping_reply_codes_str.s) {
		ds_ping_reply_codes_str.len = strlen(ds_ping_reply_codes_str.s);
		cfg_get(dispatcher, dispatcher_cfg, ds_ping_reply_codes_str)
			= ds_ping_reply_codes_str;
		if(ds_parse_reply_codes()< 0)
		{
			return -1;
		}
	}	
	/* Copy Threshhold to Config */
	cfg_get(dispatcher, dispatcher_cfg, probing_threshhold)
		= probing_threshhold;


	if(init_data()!= 0)
		return -1;

	if(ds_db_url.s)
	{
		ds_db_url.len     = strlen(ds_db_url.s);
		ds_table_name.len = strlen(ds_table_name.s);
		ds_set_id_col.len        = strlen(ds_set_id_col.s);
		ds_dest_uri_col.len      = strlen(ds_dest_uri_col.s);
		ds_dest_flags_col.len    = strlen(ds_dest_flags_col.s);
		ds_dest_priority_col.len = strlen(ds_dest_priority_col.s);
		ds_dest_attrs_col.len    = strlen(ds_dest_attrs_col.s);

		if(init_ds_db()!= 0)
		{
			LM_ERR("could not initiate a connect to the database\n");
			return -1;
		}
	} else {
		if(ds_load_list(dslistfile)!=0) {
			LM_ERR("no dispatching list loaded from file\n");
			return -1;
		} else {
			LM_DBG("loaded dispatching list\n");
		}
	}

	if (dst_avp_param.s && dst_avp_param.len > 0)
	{
		if (pv_parse_spec(&dst_avp_param, &avp_spec)==0
				|| avp_spec.type!=PVT_AVP)
		{
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					dst_avp_param.len, dst_avp_param.s);
			return -1;
		}

		if(pv_get_avp_name(0, &(avp_spec.pvp), &dst_avp_name, &dst_avp_type)!=0)
		{
			LM_ERR("[%.*s]- invalid AVP definition\n", dst_avp_param.len,
					dst_avp_param.s);
			return -1;
		}
	} else {
		dst_avp_name.n = 0;
		dst_avp_type = 0;
	}
	if (grp_avp_param.s && grp_avp_param.len > 0)
	{
		if (pv_parse_spec(&grp_avp_param, &avp_spec)==0
				|| avp_spec.type!=PVT_AVP)
		{
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					grp_avp_param.len, grp_avp_param.s);
			return -1;
		}

		if(pv_get_avp_name(0, &(avp_spec.pvp), &grp_avp_name, &grp_avp_type)!=0)
		{
			LM_ERR("[%.*s]- invalid AVP definition\n", grp_avp_param.len,
					grp_avp_param.s);
			return -1;
		}
	} else {
		grp_avp_name.n = 0;
		grp_avp_type = 0;
	}
	if (cnt_avp_param.s && cnt_avp_param.len > 0)
	{
		if (pv_parse_spec(&cnt_avp_param, &avp_spec)==0
				|| avp_spec.type!=PVT_AVP)
		{
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					cnt_avp_param.len, cnt_avp_param.s);
			return -1;
		}

		if(pv_get_avp_name(0, &(avp_spec.pvp), &cnt_avp_name, &cnt_avp_type)!=0)
		{
			LM_ERR("[%.*s]- invalid AVP definition\n", cnt_avp_param.len,
					cnt_avp_param.s);
			return -1;
		}
	} else {
		cnt_avp_name.n = 0;
		cnt_avp_type = 0;
	}
	if (dstid_avp_param.s && dstid_avp_param.len > 0)
	{
		if (pv_parse_spec(&dstid_avp_param, &avp_spec)==0
				|| avp_spec.type!=PVT_AVP)
		{
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					dstid_avp_param.len, dstid_avp_param.s);
			return -1;
		}

		if(pv_get_avp_name(0, &(avp_spec.pvp), &dstid_avp_name,
					&dstid_avp_type)!=0)
		{
			LM_ERR("[%.*s]- invalid AVP definition\n", dstid_avp_param.len,
					dstid_avp_param.s);
			return -1;
		}
	} else {
		dstid_avp_name.n = 0;
		dstid_avp_type = 0;
	}

	if (attrs_avp_param.s && attrs_avp_param.len > 0)
	{
		if (pv_parse_spec(&attrs_avp_param, &avp_spec)==0
				|| avp_spec.type!=PVT_AVP)
		{
			LM_ERR("malformed or non AVP %.*s AVP definition\n",
					attrs_avp_param.len, attrs_avp_param.s);
			return -1;
		}

		if(pv_get_avp_name(0, &(avp_spec.pvp), &attrs_avp_name,
					&attrs_avp_type)!=0)
		{
			LM_ERR("[%.*s]- invalid AVP definition\n", attrs_avp_param.len,
					attrs_avp_param.s);
			return -1;
		}
	} else {
		attrs_avp_name.n = 0;
		attrs_avp_type = 0;
	}

	if (hash_pvar_param.s && *hash_pvar_param.s) {
		if(pv_parse_format(&hash_pvar_param, &hash_param_model) < 0
				|| hash_param_model==NULL) {
			LM_ERR("malformed PV string: %s\n", hash_pvar_param.s);
			return -1;
		}		
	} else {
		hash_param_model = NULL;
	}

	if(ds_setid_pvname.s!=0)
	{
		if(pv_parse_spec(&ds_setid_pvname, &ds_setid_pv)==NULL
				|| !pv_is_w(&ds_setid_pv))
		{
			LM_ERR("[%s]- invalid setid_pvname\n", ds_setid_pvname.s);
			return -1;
		}
	}

	if(ds_attrs_pvname.s!=0)
	{
		if(pv_parse_spec(&ds_attrs_pvname, &ds_attrs_pv)==NULL
				|| !pv_is_w(&ds_attrs_pv))
		{
			LM_ERR("[%s]- invalid attrs_pvname\n", ds_attrs_pvname.s);
			return -1;
		}
	}

	if (dstid_avp_param.s && dstid_avp_param.len > 0)
	{
		if(ds_hash_size>0)
		{
			if(ds_hash_load_init(1<<ds_hash_size, ds_hash_expire,
						ds_hash_initexpire)<0)
				return -1;
			register_timer(ds_ht_timer, NULL, ds_hash_check_interval);
		} else {
			LM_ERR("call load dispatching DSTID_AVP set but no size"
					" for hash table\n");
			return -1;
		}
	}
	/* Only, if the Probing-Timer is enabled the TM-API needs to be loaded: */
	if (ds_ping_interval > 0)
	{
		/*****************************************************
		 * TM-Bindings
		 *****************************************************/
		if (load_tm_api( &tmb ) == -1)
		{
			LM_ERR("could not load the TM-functions - disable DS ping\n");
			return -1;
		}
		/*****************************************************
		 * Register the PING-Timer
		 *****************************************************/
		register_timer(ds_check_timer, NULL, ds_ping_interval);
	}

	return 0;
}

/*! \brief
 * Initialize children
 */
static int child_init(int rank)
{
	srand((11+rank)*getpid()*7);

	return 0;
}

static int mi_child_init(void)
{

	if(ds_db_url.s)
		return ds_connect_db();
	return 0;

}

/*! \brief
 * destroy function
 */
static void destroy(void)
{
	ds_destroy_list();
	if(ds_db_url.s)
		ds_disconnect_db();
	ds_hash_load_destroy();
	if(ds_ping_reply_codes)
		shm_free(ds_ping_reply_codes);	

}

/**
 *
 */
static int w_ds_select_dst(struct sip_msg* msg, char* set, char* alg)
{
	int a, s;

	if(msg==NULL)
		return -1;
	if(fixup_get_ivalue(msg, (gparam_p)set, &s)!=0)
	{
		LM_ERR("no dst set value\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_p)alg, &a)!=0)
	{
		LM_ERR("no alg value\n");
		return -1;
	}

	return ds_select_dst(msg, s, a, 0 /*set dst uri*/);
}

/**
 *
 */
static int w_ds_select_domain(struct sip_msg* msg, char* set, char* alg)
{
	int a, s;
	if(msg==NULL)
		return -1;

	if(fixup_get_ivalue(msg, (gparam_p)set, &s)!=0)
	{
		LM_ERR("no dst set value\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_p)alg, &a)!=0)
	{
		LM_ERR("no alg value\n");
		return -1;
	}

	return ds_select_dst(msg, s, a, 1/*set host port*/);
}

/**
 *
 */
static int w_ds_next_dst(struct sip_msg *msg, char *str1, char *str2)
{
	return ds_next_dst(msg, 0/*set dst uri*/);
}

/**
 *
 */
static int w_ds_next_domain(struct sip_msg *msg, char *str1, char *str2)
{
	return ds_next_dst(msg, 1/*set host port*/);
}

/**
 *
 */
static int w_ds_mark_dst0(struct sip_msg *msg, char *str1, char *str2)
{
	int state;

	state = DS_INACTIVE_DST;
	if (ds_probing_mode==DS_PROBE_ALL)
		state |= DS_PROBING_DST;

	return ds_mark_dst(msg, state);
}

/**
 *
 */
static int w_ds_mark_dst1(struct sip_msg *msg, char *str1, char *str2)
{
	int state;
	int len;

	if(str1==NULL)
		return w_ds_mark_dst0(msg, NULL, NULL);

	len = strlen(str1);
	state = 0;
	if (len>1 && (str1[1]=='p' || str1[1]=='P'))
		state |= DS_PROBING_DST;

	if(str1[0]=='i' || str1[0]=='I')
		state |= DS_INACTIVE_DST;
	else if(str1[0]=='t' || str1[0]=='T')
		state |= DS_TRYING_DST;
	else if(str1[0]=='d' || str1[0]=='D')
		state = DS_DISABLED_DST;
	else if(str1[0]=='p' || str1[0]=='P')
		state =  DS_INACTIVE_DST|DS_PROBING_DST;
	return ds_mark_dst(msg, state);
}


/**
 *
 */
static int w_ds_load_unset(struct sip_msg *msg, char *str1, char *str2)
{
	if(ds_load_unset(msg)<0)
		return -1;
	return 1;
}

/**
 *
 */
static int w_ds_load_update(struct sip_msg *msg, char *str1, char *str2)
{
	if(ds_load_update(msg)<0)
		return -1;
	return 1;
}

/**
 *
 */
static int ds_warn_fixup(void** param, int param_no)
{
	if(!dst_avp_param.s || !grp_avp_param.s || !cnt_avp_param.s)
	{
		LM_ERR("failover functions used, but AVPs paraamters required"
				" are NULL -- feature disabled\n");
	}
	return 0;
}

/************************** MI STUFF ************************/

static struct mi_root* ds_mi_set(struct mi_root* cmd_tree, void* param)
{
	str sp;
	int ret;
	unsigned int group, state;
	struct mi_node* node;

	node = cmd_tree->node.kids;
	if(node == NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
	sp = node->value;
	if(sp.len<=0 || !sp.s)
	{
		LM_ERR("bad state value\n");
		return init_mi_tree(500, "bad state value", 15);
	}

	state = 0;
	if(sp.s[0]=='0' || sp.s[0]=='I' || sp.s[0]=='i') {
		/* set inactive */
		state |= DS_INACTIVE_DST;
		if((sp.len>1) && (sp.s[1]=='P' || sp.s[1]=='p'))
			state |= DS_PROBING_DST;
	} else if(sp.s[0]=='1' || sp.s[0]=='A' || sp.s[0]=='a') {
		/* set active */
		if((sp.len>1) && (sp.s[1]=='P' || sp.s[1]=='p'))
			state |= DS_PROBING_DST;
	} else if(sp.s[0]=='2' || sp.s[0]=='D' || sp.s[0]=='d') {
		/* set disabled */
		state |= DS_DISABLED_DST;
	} else if(sp.s[0]=='3' || sp.s[0]=='T' || sp.s[0]=='t') {
		/* set trying */
		state |= DS_TRYING_DST;
		if((sp.len>1) && (sp.s[1]=='P' || sp.s[1]=='p'))
			state |= DS_PROBING_DST;
	} else {
		LM_ERR("unknow state value\n");
		return init_mi_tree(500, "unknown state value", 19);
	}
	node = node->next;
	if(node == NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
	sp = node->value;
	if(sp.s == NULL)
	{
		return init_mi_tree(500, "group not found", 15);
	}

	if(str2int(&sp, &group))
	{
		LM_ERR("bad group value\n");
		return init_mi_tree( 500, "bad group value", 16);
	}

	node= node->next;
	if(node == NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	sp = node->value;
	if(sp.s == NULL)
	{
		return init_mi_tree(500,"address not found", 18 );
	}

	ret = ds_reinit_state(group, &sp, state);

	if(ret!=0)
	{
		return init_mi_tree(404, "destination not found", 21);
	}

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}




static struct mi_root* ds_mi_list(struct mi_root* cmd_tree, void* param)
{
	struct mi_root* rpl_tree;

	rpl_tree = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==NULL)
		return 0;

	if( ds_print_mi_list(&rpl_tree->node)< 0 )
	{
		LM_ERR("failed to add node\n");
		free_mi_tree(rpl_tree);
		return 0;
	}

	return rpl_tree;
}

#define MI_ERR_RELOAD 			"ERROR Reloading data"
#define MI_ERR_RELOAD_LEN 		(sizeof(MI_ERR_RELOAD)-1)
#define MI_NOT_SUPPORTED		"DB mode not configured"
#define MI_NOT_SUPPORTED_LEN 	(sizeof(MI_NOT_SUPPORTED)-1)
#define MI_ERR_DSLOAD			"No reload support for call load dispatching"
#define MI_ERR_DSLOAD_LEN		(sizeof(MI_ERR_DSLOAD)-1)

static struct mi_root* ds_mi_reload(struct mi_root* cmd_tree, void* param)
{
	if(!ds_db_url.s) {
		if (ds_load_list(dslistfile)!=0)
			return init_mi_tree(500, MI_ERR_RELOAD, MI_ERR_RELOAD_LEN);
	} else {
		if(ds_load_db()<0)
			return init_mi_tree(500, MI_ERR_RELOAD, MI_ERR_RELOAD_LEN);
	}
	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}


static int w_ds_is_from_list0(struct sip_msg *msg, char *str1, char *str2)
{
	return ds_is_from_list(msg, -1);
}


static int w_ds_is_from_list1(struct sip_msg *msg, char *set, char *str2)
{
	return ds_is_from_list(msg, (int)(long)set);
}

static int ds_parse_reply_codes() {
	param_t* params_list = NULL;
	param_t *pit=NULL;
	int list_size = 0;
	int i = 0;
	int pos = 0;
	int code = 0;
	str input = {0, 0};
	int* ds_ping_reply_codes_new = NULL;
	int* ds_ping_reply_codes_old = NULL;

	/* Validate String: */
	if (cfg_get(dispatcher, dispatcher_cfg, ds_ping_reply_codes_str).s == 0 
			|| cfg_get(dispatcher, dispatcher_cfg, ds_ping_reply_codes_str).len<=0)
		return 0;

	/* parse_params will modify the string pointer of .s, so we need to make a copy. */
	input.s = cfg_get(dispatcher, dispatcher_cfg, ds_ping_reply_codes_str).s;
	input.len = cfg_get(dispatcher, dispatcher_cfg, ds_ping_reply_codes_str).len;

	/* Parse the parameters: */
	if (parse_params(&input, CLASS_ANY, 0, &params_list)<0)
		return -1;

	/* Get the number of entries in the list */
	for (pit = params_list; pit; pit=pit->next)
	{
		if (pit->name.len==4
				&& strncasecmp(pit->name.s, "code", 4)==0) {
			str2sint(&pit->body, &code);
			if ((code >= 100) && (code < 700))
				list_size += 1;
		} else if (pit->name.len==5
				&& strncasecmp(pit->name.s, "class", 5)==0) {
			str2sint(&pit->body, &code);
			if ((code >= 1) && (code < 7))
				list_size += 100;
		}
	}
	LM_DBG("Should be %d Destinations.\n", list_size);

	if (list_size > 0) {
		/* Allocate Memory for the new list: */
		ds_ping_reply_codes_new = (int*)shm_malloc(list_size * sizeof(int));
		if(ds_ping_reply_codes_new== NULL)
		{
			free_params(params_list);
			LM_ERR("no more memory\n");
			return -1;
		}

		/* Now create the list of valid reply-codes: */
		for (pit = params_list; pit; pit=pit->next)
		{
			if (pit->name.len==4
					&& strncasecmp(pit->name.s, "code", 4)==0) {
				str2sint(&pit->body, &code);
				if ((code >= 100) && (code < 700))
					ds_ping_reply_codes_new[pos++] = code;
			} else if (pit->name.len==5
					&& strncasecmp(pit->name.s, "class", 5)==0) {
				str2sint(&pit->body, &code);
				if ((code >= 1) && (code < 7)) {
					/* Add every code from this class, e.g. 100 to 199 */
					for (i = (code*100); i <= ((code*100)+99); i++) 
						ds_ping_reply_codes_new[pos++] = i;
				}
			}
		}
	} else {
		ds_ping_reply_codes_new = 0;
	}
	free_params(params_list);

	/* More reply-codes? Change Pointer and then set number of codes. */
	if (list_size > *ds_ping_reply_codes_cnt) {
		// Copy Pointer
		ds_ping_reply_codes_old = *ds_ping_reply_codes;
		*ds_ping_reply_codes = ds_ping_reply_codes_new;
		// Done: Set new Number of entries:
		*ds_ping_reply_codes_cnt = list_size;
		// Free the old memory area:
		if(ds_ping_reply_codes_old)
			shm_free(ds_ping_reply_codes_old);	
		/* Less or equal? Set the number of codes first. */
	} else {
		// Done:
		*ds_ping_reply_codes_cnt = list_size;
		// Copy Pointer
		ds_ping_reply_codes_old = *ds_ping_reply_codes;
		*ds_ping_reply_codes = ds_ping_reply_codes_new;
		// Free the old memory area:
		if(ds_ping_reply_codes_old)
			shm_free(ds_ping_reply_codes_old);	
	}
	/* Print the list as INFO: */
	for (i =0; i< *ds_ping_reply_codes_cnt; i++)
	{
		LM_DBG("Dispatcher: Now accepting Reply-Code %d (%d/%d) as valid\n",
				(*ds_ping_reply_codes)[i], (i+1), *ds_ping_reply_codes_cnt);
	}
	return 0;
}

int ds_ping_check_rplcode(int code)
{
	int i;

	for (i =0; i< *ds_ping_reply_codes_cnt; i++)
	{
		if((*ds_ping_reply_codes)[i] == code)
			return 1;
	}

	return 0;
}

void ds_ping_reply_codes_update(str* gname, str* name){
	ds_parse_reply_codes();
}

/*** RPC implementation ***/

static const char* dispatcher_rpc_reload_doc[2] = {
	"Reload dispatcher destination sets",
	0
};


/*
 * RPC command to reload dispatcher destination sets
 */
static void dispatcher_rpc_reload(rpc_t* rpc, void* ctx)
{
	if(!ds_db_url.s) {
		if (ds_load_list(dslistfile)!=0) {
			rpc->fault(ctx, 500, "Reload Failed");
			return;
		}
	} else {
		if(ds_load_db()<0) {
			rpc->fault(ctx, 500, "Reload Failed");
			return;
		}
	}
	return;
}



static const char* dispatcher_rpc_list_doc[2] = {
	"Return the content of dispatcher sets",
	0
};


/*
 * RPC command to print dispatcher destination sets
 */
static void dispatcher_rpc_list(rpc_t* rpc, void* ctx)
{
	void* th;
	void* ih;
	void* vh;
	int j;
	char c[3];
	str data = {"", 0};
	ds_set_t *ds_list;
	int ds_list_nr;
	ds_set_t *list;

	ds_list = ds_get_list();
	ds_list_nr = ds_get_list_nr();

	if(ds_list==NULL || ds_list_nr<=0)
	{
		LM_ERR("no destination sets\n");
		rpc->fault(ctx, 500, "No Destination Sets");
		return;
	}

	/* add entry node */
	if (rpc->add(ctx, "{", &th) < 0)
	{
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}
	if(rpc->struct_add(th, "d{",
				"SET_NO", ds_list_nr,
				"SET",  &ih)<0)
	{
		rpc->fault(ctx, 500, "Internal error set structure");
		return;
	}


	for(list = ds_list; list!= NULL; list= list->next)
	{
		if(rpc->struct_add(ih, "d",
					"SET_ID", list->id)<0)
		{
			rpc->fault(ctx, 500, "Internal error creating set id");
			return;
		}

		for(j=0; j<list->nr; j++)
		{
			if(rpc->struct_add(ih, "{",
						"DEST", &vh)<0)
			{
				rpc->fault(ctx, 500, "Internal error creating dest");
				return;
			}

			memset(&c, 0, sizeof(c));
			if (list->dlist[j].flags & DS_INACTIVE_DST)
				c[0] = 'I';
			else if (list->dlist[j].flags & DS_DISABLED_DST)
				c[0] = 'D';
			else if (list->dlist[j].flags & DS_TRYING_DST)
				c[0] = 'T';
			else
				c[0] = 'A';

			if (list->dlist[j].flags & DS_PROBING_DST)
				c[1] = 'P';
			else
				c[1] = 'X';

			if(rpc->struct_add(vh, "SsdS",
						"URI", &list->dlist[j].uri,
						"FLAGS", c,
						"PRIORITY", list->dlist[j].priority,
						"ATTRS", (list->dlist[j].attrs.body.s)?
						&(list->dlist[j].attrs.body):&data)<0)
			{
				rpc->fault(ctx, 500, "Internal error creating dest struct");
				return;
			}
		}
	}

	return;
}


static const char* dispatcher_rpc_set_state_doc[2] = {
	"Set the state of a destination address",
	0
};


/*
 * RPC command to set the state of a destination address
 */
static void dispatcher_rpc_set_state(rpc_t* rpc, void* ctx)
{
	int group;
	str dest;
	str state;
	int stval;

	if(rpc->scan(ctx, ".SdS", &state, &group, &dest)<3)
	{
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	if(state.len<=0 || state.s==NULL)
	{
		LM_ERR("bad state value\n");
		rpc->fault(ctx, 500, "Invalid State Parameter");
		return;
	}

	stval = 0;
	if(state.s[0]=='0' || state.s[0]=='I' || state.s[0]=='i') {
		/* set inactive */
		stval |= DS_INACTIVE_DST;
		if((state.len>1) && (state.s[1]=='P' || state.s[1]=='p'))
			stval |= DS_PROBING_DST;
	} else if(state.s[0]=='1' || state.s[0]=='A' || state.s[0]=='a') {
		/* set active */
		if((state.len>1) && (state.s[1]=='P' || state.s[1]=='p'))
			stval |= DS_PROBING_DST;
	} else if(state.s[0]=='2' || state.s[0]=='D' || state.s[0]=='d') {
		/* set disabled */
		stval |= DS_DISABLED_DST;
	} else if(state.s[0]=='3' || state.s[0]=='T' || state.s[0]=='t') {
		/* set trying */
		stval |= DS_TRYING_DST;
		if((state.len>1) && (state.s[1]=='P' || state.s[1]=='p'))
			stval |= DS_PROBING_DST;
	} else {
		LM_ERR("unknow state value\n");
		rpc->fault(ctx, 500, "Unknown State Value");
		return;
	}

	if(ds_reinit_state(group, &dest, stval)<0)
	{
		rpc->fault(ctx, 500, "State Update Failed");
		return;
	}

	return;
}


rpc_export_t dispatcher_rpc_cmds[] = {
	{"dispatcher.reload", dispatcher_rpc_reload,
		dispatcher_rpc_reload_doc, 0},
	{"dispatcher.list",   dispatcher_rpc_list,
		dispatcher_rpc_list_doc,   0},
	{"dispatcher.set_state",   dispatcher_rpc_set_state,
		dispatcher_rpc_set_state_doc,   0},
	{0, 0, 0, 0}
};

/**
 * register RPC commands
 */
static int ds_init_rpc(void)
{
	if (rpc_register_array(dispatcher_rpc_cmds)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

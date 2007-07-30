/**
 * $Id$
 *
 * dispatcher module -- stateless load balancing
 *
 * Copyright (C) 2004-2005 FhG Fokus
 * Copyright (C) 2006 Voice Sistem SRL
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../mi/mi.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../route.h"
#include "../../mem/mem.h"

#include "dispatch.h"

MODULE_VERSION

#define DS_SET_ID_COL		"setid"
#define DS_DEST_URI_COL		"destination"
#define DS_TABLE_NAME 		"dispatcher"

/** parameters */
char *dslistfile = CFG_DIR"dispatcher.list";
int  ds_force_dst   = 0;
int  ds_flags       = 0; 
int  ds_use_default = 0; 
char*  dst_avp_param     = NULL;
char*  grp_avp_param     = NULL;
char*  cnt_avp_param     = NULL;
int_str dst_avp_name;
unsigned short dst_avp_type;
int_str grp_avp_name;
unsigned short grp_avp_type;
int_str cnt_avp_name;
unsigned short cnt_avp_type;

int probing_threshhold = 3; /* number of failed requests, before a destination
							   is taken into probing */
str ds_ping_method = {"OPTIONS",7};
str ds_ping_from   = {"sip:dispatcher@localhost", 24};
static int ds_ping_interval = 0;

/*db */
char* ds_db_url		  = NULL;
char* ds_set_id_col	  = DS_SET_ID_COL;
char* ds_dest_uri_col = DS_DEST_URI_COL;
char* ds_table_name   = DS_TABLE_NAME;


/** module functions */
static int mod_init(void);
static int child_init(int);

static int w_ds_select_dst(struct sip_msg*, char*, char*);
static int w_ds_select_domain(struct sip_msg*, char*, char*);
static int w_ds_next_dst(struct sip_msg*, char*, char*);
static int w_ds_next_domain(struct sip_msg*, char*, char*);
static int w_ds_mark_dst0(struct sip_msg*, char*, char*);
static int w_ds_mark_dst1(struct sip_msg*, char*, char*);

static int w_ds_is_from_list0(struct sip_msg*, char*, char*);
static int w_ds_is_from_list1(struct sip_msg*, char*, char*);
static int fixstring2int(void **, int);

void destroy(void);

static int ds_fixup(void** param, int param_no);
static int ds_warn_fixup(void** param, int param_no);

struct mi_root* ds_mi_set(struct mi_root* cmd, void* param);
struct mi_root* ds_mi_list(struct mi_root* cmd, void* param);
struct mi_root* ds_mi_reload(struct mi_root* cmd_tree, void* param);

static cmd_export_t cmds[]={
	{"ds_select_dst",    w_ds_select_dst,    2, ds_fixup, REQUEST_ROUTE},
	{"ds_select_domain", w_ds_select_domain, 2, ds_fixup, REQUEST_ROUTE},
	{"ds_next_dst",      w_ds_next_dst,      0, ds_warn_fixup, FAILURE_ROUTE},
	{"ds_next_domain",   w_ds_next_domain,   0, ds_warn_fixup, FAILURE_ROUTE},
	{"ds_mark_dst",      w_ds_mark_dst0,     0, ds_warn_fixup, FAILURE_ROUTE},
	{"ds_mark_dst",      w_ds_mark_dst1,     1, ds_warn_fixup, FAILURE_ROUTE},
	{"ds_is_from_list",  w_ds_is_from_list0, 0, 0, REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"ds_is_from_list",  w_ds_is_from_list1, 1, fixstring2int, REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{0,0,0,0,0}
};


static param_export_t params[]={
	{"list_file",       STR_PARAM, &dslistfile},
	{"db_url",		    STR_PARAM, &ds_db_url},
	{"table_name", 	    STR_PARAM, &ds_table_name},
	{"setid_col",       STR_PARAM, &ds_set_id_col},
	{"destination_col", STR_PARAM, &ds_dest_uri_col},
	{"force_dst",       INT_PARAM, &ds_force_dst},
	{"flags",           INT_PARAM, &ds_flags},
	{"use_default",     INT_PARAM, &ds_use_default},
	{"dst_avp",         STR_PARAM, &dst_avp_param},
	{"grp_avp",         STR_PARAM, &grp_avp_param},
	{"cnt_avp",         STR_PARAM, &cnt_avp_param},
	{"ds_probing_threshhold", INT_PARAM, &probing_threshhold},
	{"ds_ping_method",     STR_PARAM, &ds_ping_method},
	{"ds_ping_from",       STR_PARAM, &ds_ping_from},
	{"ds_ping_interval",   INT_PARAM, &ds_ping_interval},
	{0,0,0}
};


static mi_export_t mi_cmds[] = {
	{ "ds_set_state",   ds_mi_set,   0,                 0,  0 },
	{ "ds_list",        ds_mi_list,  MI_NO_INPUT_FLAG,  0,  0 },
	{ "ds_reload",		ds_mi_reload, 0,				0,	0},
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
	mod_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) destroy,
	child_init  /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	xl_spec_t avp_spec;
	LM_DBG("initializing ...\n");

	if(init_data()!= 0)
		return -1;

	if(ds_db_url!= NULL)
	{
		if(ds_load_db()!= 0)
		{
			LM_ERR("no dispatching list loaded from database\n");
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
	
	if (dst_avp_param && *dst_avp_param)
	{
		if (xl_parse_spec(dst_avp_param, &avp_spec,
					XL_THROW_ERROR|XL_DISABLE_MULTI|XL_DISABLE_COLORS)==0
				|| avp_spec.type!=XL_AVP)
		{
			LM_ERR("malformed or non AVP %s AVP definition\n", dst_avp_param);
			return -1;
		}

		if(xl_get_avp_name(0, &avp_spec, &dst_avp_name, &dst_avp_type)!=0)
		{
			LM_ERR("[%s]- invalid AVP definition\n", dst_avp_param);
			return -1;
		}
	} else {
		dst_avp_name.n = 0;
		dst_avp_type = 0;
	}
	if (grp_avp_param && *grp_avp_param)
	{
		if (xl_parse_spec(grp_avp_param, &avp_spec,
					XL_THROW_ERROR|XL_DISABLE_MULTI|XL_DISABLE_COLORS)==0
				|| avp_spec.type!=XL_AVP)
		{
			LM_ERR("malformed or non AVP %s AVP definition\n", grp_avp_param);
			return -1;
		}

		if(xl_get_avp_name(0, &avp_spec, &grp_avp_name, &grp_avp_type)!=0)
		{
			LM_ERR("[%s]- invalid AVP definition\n", grp_avp_param);
			return -1;
		}
	} else {
		grp_avp_name.n = 0;
		grp_avp_type = 0;
	}
	if (cnt_avp_param && *cnt_avp_param)
	{
		if (xl_parse_spec(cnt_avp_param, &avp_spec,
					XL_THROW_ERROR|XL_DISABLE_MULTI|XL_DISABLE_COLORS)==0
				|| avp_spec.type!=XL_AVP)
		{
			LM_ERR("malformed or non AVP %s AVP definition\n", cnt_avp_param);
			return -1;
		}

		if(xl_get_avp_name(0, &avp_spec, &cnt_avp_name, &cnt_avp_type)!=0)
		{
			LM_ERR("[%s]- invalid AVP definition\n", cnt_avp_param);
			return -1;
		}
	} else {
		cnt_avp_name.n = 0;
		cnt_avp_type = 0;
	}
	
	if (ds_ping_from.s) ds_ping_from.len = strlen(ds_ping_from.s);
	if (ds_ping_method.s) ds_ping_method.len = strlen(ds_ping_method.s);

	/* Only, if the Probing-Timer is enabled the TM-API needs to be loaded: */
	if (ds_ping_interval > 0)
	{
		/*****************************************************
		 * TM-Bindings
	  	 *****************************************************/
		load_tm_f load_tm;
		load_tm=(load_tm_f)find_export("load_tm", 0, 0);
	
		/* import the TM auto-loading function */
		if (load_tm)
		{
			/* let the auto-loading function load all TM stuff */
			if (load_tm( &tmb ) == -1)
			{
				LM_ERR("could not load the TM-functions - disable DS ping\n");
				return -1;
			}
			/*****************************************************
			 * Register the PING-Timer
	    	 *****************************************************/
			register_timer(ds_check_timer, NULL, ds_ping_interval);
		} else {
			LM_WARN("could not bind to the TM-Module, automatic"
					" re-activation disabled.\n");
		}
	}

	return 0;
}

/**
 * Initialize children
 */
static int child_init(int rank)
{
	LM_DBG(" #%d / pid <%d>\n", rank, getpid());
	srand((11+rank)*getpid()*7);
	return 0;
}

static inline int ds_get_ivalue(struct sip_msg* msg, ds_param_p dp, int *val)
{
	xl_value_t value;
	if(dp->type==0) {
		*val = dp->v.id;
		return 0;
	}
	
	LM_DBG("searching %d %d %d\n", dp->v.sp.type, dp->v.sp.p.ind,
			dp->v.sp.p.val.len);
	if(xl_get_spec_value(msg, &dp->v.sp, &value, 0)!=0 
			|| value.flags&XL_VAL_NULL || !(value.flags&XL_VAL_INT))
	{
		LM_ERR("no AVP found (error in scripts)\n");
		return -1;
	}
	*val = value.ri;
	return 0;
}

/**
 *
 */
static int w_ds_select_dst(struct sip_msg* msg, char* set, char* alg)
{
	int a, s;
	
	if(msg==NULL)
		return -1;
	if(ds_get_ivalue(msg, (ds_param_p)set, &s)!=0)
	{
		LM_ERR("no dst set value\n");
		return -1;
	}
	if(ds_get_ivalue(msg, (ds_param_p)alg, &a)!=0)
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

	if(ds_get_ivalue(msg, (ds_param_p)set, &s)!=0)
	{
		LM_ERR("no dst set value\n");
		return -1;
	}
	if(ds_get_ivalue(msg, (ds_param_p)alg, &a)!=0)
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
	return ds_mark_dst(msg, 0);
}

/**
 *
 */
static int w_ds_mark_dst1(struct sip_msg *msg, char *str1, char *str2)
{
	if(str1 && (str1[0]=='i' || str1[0]=='I' || str1[0]=='0'))
		return ds_mark_dst(msg, 0);
	else if(str1 && (str1[0]=='p' || str1[0]=='P' || str1[0]=='2'))
		return ds_mark_dst(msg, 2);
	else
		return ds_mark_dst(msg, 1);
}

/**
 * destroy function
 */
void destroy(void)
{
	LM_DBG("destroying module ...\n");
	ds_destroy_list();
}

/**
 *
 */
static int ds_fixup(void** param, int param_no)
{
	int err;
	ds_param_p dsp;
	
	if(param_no==1 || param_no==2)
	{
		dsp = (ds_param_p)pkg_malloc(sizeof(ds_param_t));
		if(dsp == NULL)
		{
			LM_ERR("no more memory\n");
			return E_UNSPEC;
		}
		memset(dsp, 0, sizeof(ds_param_t));
		if(((char*)(*param))[0]=='$')
		{
			dsp->type = 1;
			if(xl_parse_spec((char*)*param, &dsp->v.sp,
					XL_THROW_ERROR|XL_DISABLE_MULTI|XL_DISABLE_COLORS)==NULL
				|| dsp->v.sp.type!=XL_AVP)
			{
				LM_ERR("Unsupported User Field identifier\n");
				return E_UNSPEC;
			}
		} else {
			dsp->type = 0;
			dsp->v.id = str2s(*param, strlen(*param), &err);
			if (err == 0)
			{
				pkg_free(*param);
			} else {
				LM_ERR("Bad number <%s>\n",
				    (char*)(*param));
				return E_UNSPEC;
			}
		}
		*param = (void*)dsp;
	}
	return 0;
}

static int ds_warn_fixup(void** param, int param_no)
{
	if(dst_avp_param==NULL || grp_avp_param == NULL || cnt_avp_param == NULL)
	{
		LM_ERR("failover functions used, but AVPs paraamters required"
				" are NULL -- feature disabled\n");
	}
	return 0;
}

/************************** MI STUFF ************************/

struct mi_root* ds_mi_set(struct mi_root* cmd_tree, void* param)
{
	str sp;
	int ret;
	unsigned int group, state;
	struct mi_node* node;

	node = cmd_tree->node.kids;
	if(node == NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
	sp = node->value;
	if(sp.len<=0 || sp.s == NULL)
	{
		LM_ERR("bad state value\n");
		return init_mi_tree( 500, "bad state value", 15);
	}

	state = 1;
	if(sp.s[0]=='0' || sp.s[0]=='I' || sp.s[0]=='i')
		state = 0;
	node = node->next;
	if(node == NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
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

	if(state==1)
		ret = ds_set_state(group, &sp, DS_INACTIVE_DST, 0);
	else
		ret = ds_set_state(group, &sp, DS_INACTIVE_DST, 1);

	if(ret!=0)
	{
		return init_mi_tree(404, "destination not found", 21);
	}

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}




struct mi_root* ds_mi_list(struct mi_root* cmd_tree, void* param)
{
	struct mi_root* rpl_tree;

	rpl_tree = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==NULL)
		return 0;

	if( ds_print_mi_list(&rpl_tree->node)< 0 )
	{
		LOG(L_ERR,"ERROR:mi_ps: failed to add node\n");
		free_mi_tree(rpl_tree);
		return 0;
	}

	return rpl_tree;
}

#define MI_ERR_RELOAD 			"ERROR Reloading data"
#define MI_ERR_RELOAD_LEN 		(sizeof(MI_ERR_RELOAD)-1)
#define MI_NOT_SUPPORTED		"DB mode not configured"
#define MI_NOT_SUPPORTED_LEN 	(sizeof(MI_NOT_SUPPORTED)-1)

struct mi_root* ds_mi_reload(struct mi_root* cmd_tree, void* param)
{
	if(ds_db_url==NULL)
		return init_mi_tree(400, MI_NOT_SUPPORTED, MI_NOT_SUPPORTED_LEN);


	if(ds_load_db()<0)
		return init_mi_tree(500, MI_ERR_RELOAD, MI_ERR_RELOAD_LEN);

	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}


/**
 *
 */
static int w_ds_is_from_list0(struct sip_msg *msg, char *str1, char *str2)
{
	return ds_is_from_list(msg, -1);
}


/**
 *
 */
static int w_ds_is_from_list1(struct sip_msg *msg, char *set, char *str2)
{
	return ds_is_from_list(msg, (int)set);
}

/* 
 * Convert string parameter to integer for functions that expect an integer.
 * Taken from mediaproxy/LCR module.
 */
static int fixstring2int(void **param, int param_count)
{
	unsigned long number;
	int err;

	if (param_count == 1) {
		number = str2s(*param, strlen(*param), &err);
		if (err == 0) {
			pkg_free(*param);
			*param = (void*)number;
			return 0;
		} else {
			LM_ERR("ERROR: bad number `%s'\n", (char*)(*param));
			return E_CFG;
		}
	}
	return 0;
}

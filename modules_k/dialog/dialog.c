/*
 * $Id$
 *
 * dialog module - basic support for dialog tracking
 *
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
 * History:
 * --------
 *  2006-04-14 initial version (bogdan)
 *  2006-11-28 Added statistic support for the number of early and failed
 *              dialogs. (Jeffrey Magder - SOMA Networks) 
 *  2007-04-30 added dialog matching without DID (dialog ID), but based only
 *              on RFC3261 elements - based on an original patch submitted 
 *              by Michel Bensoussan <michel@extricom.com> (bogdan)
 *  2007-05-15 added saving dialogs' information to database (ancuta)
 *  2007-07-04 added saving dialog cseq, contact, record route 
 *              and bind_addresses(sock_info) for caller and callee (ancuta)
 *  2008-04-14 added new type of callback to be triggered when dialogs are 
 *              loaded from DB (bogdan)
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "../../sr_module.h"
#include "../../db/db.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../pvar.h"
#include "../../mod_fix.h"
#include "../../script_cb.h"
#include "../../script_var.h"
#include "../../mem/mem.h"
#include "../../mi/mi.h"
#include "../tm/tm_load.h"
#include "../rr/api.h"
#include "dlg_hash.h"
#include "dlg_timer.h"
#include "dlg_handlers.h"
#include "dlg_load.h"
#include "dlg_cb.h"
#include "dlg_db_handler.h"
#include "dlg_req_within.h"
#include "dlg_profile.h"

MODULE_VERSION


static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

/* module parameter */
static int dlg_hash_size = 4096;
static char* rr_param = "did";
static int dlg_flag = -1;
static str timeout_spec = {NULL, 0};
static int default_timeout = 60 * 60 * 12;  /* 12 hours */
static int seq_match_mode = SEQ_MATCH_STRICT_ID;
static char* profiles_wv_s = NULL;
static char* profiles_nv_s = NULL;
str dlg_extra_hdrs = {NULL,0};
static int db_fetch_rows = 200;


/* statistic variables */
int dlg_enable_stats = 1;
stat_var *active_dlgs = 0;
stat_var *processed_dlgs = 0;
stat_var *expired_dlgs = 0;
stat_var *failed_dlgs = 0;
stat_var *early_dlgs  = 0;

struct tm_binds d_tmb;
struct rr_binds d_rrb;
pv_spec_t timeout_avp;

/* db stuff */
static str db_url = str_init(DEFAULT_DB_URL);
static unsigned int db_update_period = DB_DEFAULT_UPDATE_PERIOD;

static int pv_get_dlg_count( struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

/* commands wrappers and fixups */
static int fixup_profile(void** param, int param_no);
static int fixup_get_profile2(void** param, int param_no);
static int fixup_get_profile3(void** param, int param_no);
static int w_set_dlg_profile(struct sip_msg*, char*, char*);
static int w_is_in_profile(struct sip_msg*, char*, char*);
static int w_get_profile_size(struct sip_msg*, char*, char*, char*);


static cmd_export_t cmds[]={
	{"set_dlg_profile", (cmd_function)w_set_dlg_profile,  1,fixup_profile,
			0, REQUEST_ROUTE| FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE },
	{"set_dlg_profile", (cmd_function)w_set_dlg_profile,  2,fixup_profile,
			0, REQUEST_ROUTE| FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE },
	{"is_in_profile", (cmd_function)w_is_in_profile,      1,fixup_profile,
			0, REQUEST_ROUTE| FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE },
	{"is_in_profile", (cmd_function)w_is_in_profile,      2,fixup_profile,
			0, REQUEST_ROUTE| FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE },
	{"get_profile_size",(cmd_function)w_get_profile_size, 2,fixup_get_profile2,
			0, REQUEST_ROUTE| FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE },
	{"get_profile_size",(cmd_function)w_get_profile_size, 3,fixup_get_profile3,
			0, REQUEST_ROUTE| FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE },
	{"load_dlg",  (cmd_function)load_dlg,   0, 0, 0, 0},
	{0,0,0,0,0,0}
};

static param_export_t mod_params[]={
	{ "enable_stats",          INT_PARAM, &dlg_enable_stats         },
	{ "hash_size",             INT_PARAM, &dlg_hash_size            },
	{ "rr_param",              STR_PARAM, &rr_param                 },
	{ "dlg_flag",              INT_PARAM, &dlg_flag                 },
	{ "timeout_avp",           STR_PARAM, &timeout_spec.s           },
	{ "default_timeout",       INT_PARAM, &default_timeout          },
	{ "dlg_extra_hdrs",        STR_PARAM, &dlg_extra_hdrs.s         },
	{ "dlg_match_mode",        INT_PARAM, &seq_match_mode           },
	{ "db_url",                STR_PARAM, &db_url.s                 },
	{ "db_mode",               INT_PARAM, &dlg_db_mode              },
	{ "table_name",            STR_PARAM, &dialog_table_name        },
	{ "call_id_column",        STR_PARAM, &call_id_column.s         },
	{ "from_uri_column",       STR_PARAM, &from_uri_column.s        },
	{ "from_tag_column",       STR_PARAM, &from_tag_column.s        },
	{ "to_uri_column",         STR_PARAM, &to_uri_column.s          },
	{ "to_tag_column",         STR_PARAM, &to_tag_column.s          },
	{ "h_id_column",           STR_PARAM, &h_id_column.s            },
	{ "h_entry_column",        STR_PARAM, &h_entry_column.s         },
	{ "state_column",          STR_PARAM, &state_column.s           },
	{ "start_time_column",     STR_PARAM, &start_time_column.s      },
	{ "timeout_column",        STR_PARAM, &timeout_column.s         },
	{ "to_cseq_column",        STR_PARAM, &to_cseq_column.s         },
	{ "from_cseq_column",      STR_PARAM, &from_cseq_column.s       },
	{ "to_route_column",       STR_PARAM, &to_route_column.s        },
	{ "from_route_column",     STR_PARAM, &from_route_column.s      },
	{ "to_contact_column",     STR_PARAM, &to_contact_column.s      },
	{ "from_contact_column",   STR_PARAM, &from_contact_column.s    },
	{ "to_sock_column",        STR_PARAM, &to_sock_column.s         },
	{ "from_sock_column",      STR_PARAM, &from_sock_column.s       },
	{ "db_update_period",      INT_PARAM, &db_update_period         },
	{ "db_fetch_rows",         INT_PARAM, &db_fetch_rows            },
	{ "profiles_with_value",   STR_PARAM, &profiles_wv_s            },
	{ "profiles_no_value",     STR_PARAM, &profiles_nv_s            },
	{ 0,0,0 }
};


static stat_export_t mod_stats[] = {
	{"active_dialogs" ,     STAT_NO_RESET,  &active_dlgs       },
	{"early_dialogs",       STAT_NO_RESET,  &early_dlgs        },
	{"processed_dialogs" ,  0,              &processed_dlgs    },
	{"expired_dialogs" ,    0,              &expired_dlgs      },
	{"failed_dialogs",      0,              &failed_dlgs       },
	{0,0,0}
};


static mi_export_t mi_cmds[] = {
	{ "dlg_list",           mi_print_dlgs,       0,  0,  0},
	{ "dlg_list_ctx",       mi_print_dlgs_ctx,   0,  0,  0},
	{ "dlg_end_dlg",        mi_terminate_dlg,    0,  0,  0},
	{ "profile_get_size",   mi_get_profile,      0,  0,  0},
	{ "profile_list_dlgs",  mi_profile_list,     0,  0,  0},
	{ 0, 0, 0, 0, 0}
};


static pv_export_t mod_items[] = {
	{ {"DLG_count",  sizeof("DLG_count")-1}, 1000,  pv_get_dlg_count,    0,
		0, 0, 0, 0 },
	{ {"DLG_lifetime",sizeof("DLG_lifetime")-1}, 1000, pv_get_dlg_lifetime, 0,
		0, 0, 0, 0 },
	{ {"DLG_status",  sizeof("DLG_status")-1}, 1000, pv_get_dlg_status, 0,
		0, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

struct module_exports exports= {
	"dialog",        /* module's name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions */
	mod_params,      /* param exports */
	mod_stats,       /* exported statistics */
	mi_cmds,         /* exported MI functions */
	mod_items,       /* exported pseudo-variables */
	0,               /* extra processes */
	mod_init,        /* module initialization function */
	0,               /* reply processing function */
	mod_destroy,
	child_init       /* per-child init function */
};


static int fixup_profile(void** param, int param_no)
{
	struct dlg_profile_table *profile;
	pv_elem_t *model=NULL;
	str s;

	s.s = (char*)(*param);
	s.len = strlen(s.s);
	if(s.len==0) {
		LM_ERR("param %d is empty string!\n", param_no);
		return E_CFG;
	}

	if (param_no==1) {
		profile = search_dlg_profile( &s );
		if (profile==NULL) {
			LM_CRIT("profile <%s> not definited\n",s.s);
			return E_CFG;
		}
		pkg_free(*param);
		*param = (void*)profile;
		return 0;
	} else if (param_no==2) {
		if(pv_parse_format(&s ,&model) || model==NULL) {
			LM_ERR("wrong format [%s] for value param!\n", s.s);
			return E_CFG;
		}
		*param = (void*)model;
	}
	return 0;
}


static int fixup_get_profile2(void** param, int param_no)
{
	pv_spec_t *sp;
	int ret;

	if (param_no==1) {
		return fixup_profile(param, 1);
	} else if (param_no==2) {
		ret = fixup_pvar(param);
		if (ret<0) return ret;
		sp = (pv_spec_t*)(*param);
		if (sp->type!=PVT_AVP && sp->type!=PVT_SCRIPTVAR) {
			LM_ERR("return must be an AVP or SCRIPT VAR!\n");
			return E_SCRIPT;
		}
	}
	return 0;
}


static int fixup_get_profile3(void** param, int param_no)
{
	if (param_no==1) {
		return fixup_profile(param, 1);
	} else if (param_no==2) {
		return fixup_profile(param, 2);
	} else if (param_no==3) {
		return fixup_get_profile2( param, 2);
	}
	return 0;
}



int load_dlg( struct dlg_binds *dlgb )
{
	dlgb->register_dlgcb = register_dlgcb;
	return 1;
}


static int pv_get_dlg_count(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	int n;
	int l;
	char *ch;

	if(msg==NULL || res==NULL)
		return -1;

	n = active_dlgs ? get_stat_val(active_dlgs) : 0;
	l = 0;
	ch = int2str( n, &l);

	res->rs.s = ch;
	res->rs.len = l;

	res->ri = n;
	res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;

	return 0;
}


static int mod_init(void)
{
	unsigned int n;

	LM_INFO("Dialog module - initializing\n");

	if (timeout_spec.s)
		timeout_spec.len = strlen(timeout_spec.s);

	db_url.len = strlen(db_url.s);
	call_id_column.len = strlen(call_id_column.s);
	from_uri_column.len = strlen(from_uri_column.s);
	from_tag_column.len = strlen(from_tag_column.s);
	to_uri_column.len = strlen(to_uri_column.s);
	to_tag_column.len = strlen(to_tag_column.s);
	h_id_column.len = strlen(h_id_column.s);
	h_entry_column.len = strlen(h_entry_column.s);
	state_column.len = strlen(state_column.s);
	start_time_column.len = strlen(start_time_column.s);
	timeout_column.len = strlen(timeout_column.s);
	to_cseq_column.len = strlen(to_cseq_column.s);
	from_cseq_column.len = strlen(from_cseq_column.s);
	to_route_column.len = strlen(to_route_column.s);
	from_route_column.len = strlen(from_route_column.s);
	to_contact_column.len = strlen(to_contact_column.s);
	from_contact_column.len = strlen(from_contact_column.s);
	to_sock_column.len = strlen(to_sock_column.s);
	from_sock_column.len = strlen(from_sock_column.s);
	dialog_table_name.len = strlen(dialog_table_name.s);

	/* param checkings */
	if (dlg_flag==-1) {
		LM_ERR("no dlg flag set!!\n");
		return -1;
	} else if (dlg_flag>MAX_FLAG) {
		LM_ERR("invalid dlg flag %d!!\n",dlg_flag);
		return -1;
	}

	if (rr_param==0 || rr_param[0]==0) {
		LM_ERR("empty rr_param!!\n");
		return -1;
	} else if (strlen(rr_param)>MAX_DLG_RR_PARAM_NAME) {
		LM_ERR("rr_param too long (max=%d)!!\n", MAX_DLG_RR_PARAM_NAME);
		return -1;
	}

	if (timeout_spec.s) {
		if ( pv_parse_spec(&timeout_spec, &timeout_avp)==0 
				&& (timeout_avp.type!=PVT_AVP)){
			LM_ERR("malformed or non AVP timeout "
				"AVP definition in '%.*s'\n", timeout_spec.len,timeout_spec.s);
			return -1;
		}
	}

	if (default_timeout<=0) {
		LM_ERR("0 default_timeout not accepted!!\n");
		return -1;
	}

	/* update the len of the extra headers */
	if (dlg_extra_hdrs.s)
		dlg_extra_hdrs.len = strlen(dlg_extra_hdrs.s);

	if (seq_match_mode!=SEQ_MATCH_NO_ID &&
	seq_match_mode!=SEQ_MATCH_FALLBACK &&
	seq_match_mode!=SEQ_MATCH_STRICT_ID ) {
		LM_ERR("invalid value %d for seq_match_mode param!!\n",seq_match_mode);
		return -1;
	}

	/* if statistics are disabled, prevent their registration to core */
	if (dlg_enable_stats==0)
		exports.stats = 0;

	/* create profile hashes */
	if (add_profile_definitions( profiles_nv_s, 0)!=0 ) {
		LM_ERR("failed to add profiles without value\n");
		return -1;
	}
	if (add_profile_definitions( profiles_wv_s, 1)!=0 ) {
		LM_ERR("failed to add profiles with value\n");
		return -1;
	}

	/* load the TM API */
	if (load_tm_api(&d_tmb)!=0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}

	/* load RR API also */
	if (load_rr_api(&d_rrb)!=0) {
		LM_ERR("can't load RR API\n");
		return -1;
	}

	/* register callbacks*/
	/* listen for all incoming requests  */
	if ( d_tmb.register_tmcb( 0, 0, TMCB_REQUEST_IN, dlg_onreq, 0 ) <=0 ) {
		LM_ERR("cannot register TMCB_REQUEST_IN callback\n");
		return -1;
	}

	/* listen for all routed requests  */
	if ( d_rrb.register_rrcb( dlg_onroute, 0 ) <0 ) {
		LM_ERR("cannot register RR callback\n");
		return -1;
	}

	if (register_script_cb( profile_cleanup, POST_SCRIPT_CB|REQ_TYPE_CB,0)<0) {
		LM_ERR("cannot regsiter script callback");
		return -1;
	}

	if ( register_timer( dlg_timer_routine, 0, 1)<0 ) {
		LM_ERR("failed to register timer \n");
		return -1;
	}

	/* init handlers */
	init_dlg_handlers( rr_param, dlg_flag,
		timeout_spec.s?&timeout_avp:0, default_timeout, seq_match_mode);

	/* init timer */
	if (init_dlg_timer(dlg_ontimeout)!=0) {
		LM_ERR("cannot init timer list\n");
		return -1;
	}

	/* initialized the hash table */
	for( n=0 ; n<(8*sizeof(n)) ; n++) {
		if (dlg_hash_size==(1<<n))
			break;
		if (dlg_hash_size<(1<<n)) {
			LM_WARN("hash_size is not a power "
				"of 2 as it should be -> rounding from %d to %d\n",
				dlg_hash_size, 1<<(n-1));
			dlg_hash_size = 1<<(n-1);
		}
	}

	if ( init_dlg_table(dlg_hash_size)<0 ) {
		LM_ERR("failed to create hash table\n");
		return -1;
	}

	/* if a database should be used to store the dialogs' information */
	if (dlg_db_mode==DB_MODE_NONE) {
		db_url.s = 0; db_url.len = 0;
	} else {
		if (dlg_db_mode!=DB_MODE_REALTIME &&
		dlg_db_mode!=DB_MODE_DELAYED && dlg_db_mode!=DB_MODE_SHUTDOWN ) {
			LM_ERR("unsupported db_mode %d\n", dlg_db_mode);
			return -1;
		}
		if ( !db_url.s || db_url.len==0 ) {
			LM_ERR("db_url not configured for db_mode %d\n", dlg_db_mode);
			return -1;
		}
		if (init_dlg_db(&db_url, dlg_hash_size, db_update_period,db_fetch_rows)!=0) {
			LM_ERR("failed to initialize the DB support\n");
			return -1;
		}
		run_load_callbacks();
	}

	destroy_dlg_callbacks( DLGCB_LOADED );

	return 0;
}


static int child_init(int rank)
{
	if ( (dlg_db_mode==DB_MODE_REALTIME && (rank>0 || rank==PROC_TIMER)) ||
	(dlg_db_mode==DB_MODE_SHUTDOWN && (rank==PROC_MAIN)) ||
	(dlg_db_mode==DB_MODE_DELAYED && (rank==PROC_MAIN || rank==PROC_TIMER ||
	rank>0) )){
		if ( dlg_connect_db(&db_url) ) {
			LM_ERR("failed to connect to database (rank=%d)\n",rank);
			return -1;
		}
	}

	/* in DB_MODE_SHUTDOWN only PROC_MAIN will do a DB dump at the end, so
	 * for the rest of the processes will be the same as DB_MODE_NONE */
	if (dlg_db_mode==DB_MODE_SHUTDOWN && rank!=PROC_MAIN)
		dlg_db_mode = DB_MODE_NONE;

	return 0;
}


static void mod_destroy(void)
{
	if(dlg_db_mode == DB_MODE_DELAYED || dlg_db_mode == DB_MODE_SHUTDOWN) {
		dialog_update_db(0, 0);
		destroy_dlg_db();
	}
	/* no DB interaction from now on */
	dlg_db_mode = DB_MODE_NONE;
	destroy_dlg_table();
	destroy_dlg_timer();
	destroy_dlg_callbacks( DLGCB_CREATED|DLGCB_LOADED );
	destroy_dlg_handlers();
	destroy_dlg_profiles();
}



static int w_set_dlg_profile(struct sip_msg *msg, char *profile, char *value)
{
	pv_elem_t *pve;
	str val_s;

	pve = (pv_elem_t *)value;

	if (((struct dlg_profile_table*)profile)->has_value) {
		if ( pve==NULL || pv_printf_s(msg, pve, &val_s)!=0 || 
		val_s.len == 0 || val_s.s == NULL) {
			LM_WARN("cannot get string for value\n");
			return -1;
		}
		if ( set_dlg_profile( msg, &val_s,
		(struct dlg_profile_table*)profile) < 0 ) {
			LM_ERR("failed to set profile");
			return -1;
		}
	} else {
		if ( set_dlg_profile( msg, NULL,
		(struct dlg_profile_table*)profile) < 0 ) {
			LM_ERR("failed to set profile");
			return -1;
		}
	}
	return 1;
}


static int w_is_in_profile(struct sip_msg *msg, char *profile, char *value)
{
	pv_elem_t *pve;
	str val_s;

	pve = (pv_elem_t *)value;

	if ( pve!=NULL && ((struct dlg_profile_table*)profile)->has_value) {
		if ( pv_printf_s(msg, pve, &val_s)!=0 || 
		val_s.len == 0 || val_s.s == NULL) {
			LM_WARN("cannot get string for value\n");
			return -1;
		}
		return is_dlg_in_profile( msg, (struct dlg_profile_table*)profile,
			&val_s);
	} else {
		return is_dlg_in_profile( msg, (struct dlg_profile_table*)profile,
			NULL);
	}
}


static int w_get_profile_size(struct sip_msg *msg, char *profile, 
													char *value, char *result)
{
	pv_elem_t *pve;
	str val_s;
	pv_spec_t *sp_dest;
	unsigned int size;
	int_str res;
	int_str avp_name;
	unsigned short avp_type;
	script_var_t * sc_var;

	pve = (pv_elem_t *)value;
	sp_dest = (pv_spec_t *)result;

	if ( pve!=NULL && ((struct dlg_profile_table*)profile)->has_value) {
		if ( pv_printf_s(msg, pve, &val_s)!=0 || 
		val_s.len == 0 || val_s.s == NULL) {
			LM_WARN("cannot get string for value\n");
			return -1;
		}
		size = get_profile_size( (struct dlg_profile_table*)profile ,&val_s );
	} else {
		size = get_profile_size( (struct dlg_profile_table*)profile, NULL );
	}

	switch (sp_dest->type) {
		case PVT_AVP:
			if (pv_get_avp_name( msg, &(sp_dest->pvp), &avp_name,
			&avp_type)!=0){
				LM_CRIT("BUG in getting AVP name\n");
				return -1;
			}
			res.n = size;
			if (add_avp(avp_type, avp_name, res)<0){
				LM_ERR("cannot add AVP\n");
				return -1;
			}
			break;

		case PVT_SCRIPTVAR:
			if(sp_dest->pvp.pvn.u.dname == 0){
				LM_ERR("cannot find svar name\n");
				return -1;
			}
			res.n = size;
			sc_var = (script_var_t *)sp_dest->pvp.pvn.u.dname;
			if(!set_var_value(sc_var, &res, VAR_VAL_STR)){
				LM_ERR("cannot set svar\n");
				return -1;
			}
			break;

		default:
			LM_CRIT("BUG: invalid pvar type\n");
			return -1;
	}

	return 1;
}



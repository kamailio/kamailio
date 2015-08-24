/*
 * dialog module - basic support for dialog tracking
 *
 * Copyright (C) 2006 Voice Sistem SRL
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
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

/*!
 * \file
 * \brief Module interface
 * \ingroup dialog
 * Module: \ref dialog
 */

/**
 * @defgroup dialog dialog :: Kamailio dialog module
 * @brief Kamailio dialog module
 *
 * The dialog module provides dialog awareness to the Kamailio proxy. Its
 * functionality is to keep track of the current dialogs, to offer
 * information about them (like how many dialogs are active) or to manage
 * them. The module exports several functions that could be used directly
 * from scripts.
 * The module, via an internal API, also provide the foundation to build
 * on top of it more complex dialog-based functionalities via other
 * Kamailio modules.                       
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "../../sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../pvar.h"
#include "../../mod_fix.h"
#include "../../script_cb.h"
#include "../../lib/kcore/faked_msg.h"
#include "../../hashes.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../mem/mem.h"
#include "../../lib/kmi/mi.h"
#include "../../timer_proc.h"
#include "../../lvalue.h"
#include "../../parser/parse_to.h"
#include "../../modules/tm/tm_load.h"
#include "../../rpc_lookup.h"
#include "../rr/api.h"
#include "dlg_hash.h"
#include "dlg_timer.h"
#include "dlg_handlers.h"
#include "dlg_load.h"
#include "dlg_cb.h"
#include "dlg_db_handler.h"
#include "dlg_req_within.h"
#include "dlg_profile.h"
#include "dlg_var.h"
#include "dlg_transfer.h"
#include "dlg_cseq.h"

MODULE_VERSION


#define RPC_DATE_BUF_LEN 21

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
int initial_cbs_inscript = 1;
int dlg_wait_ack = 1;
static int dlg_timer_procs = 0;
static int _dlg_track_cseq_updates = 0;

int dlg_event_rt[DLG_EVENTRT_MAX];

str dlg_bridge_controller = str_init("sip:controller@kamailio.org");

str dlg_bridge_contact = str_init("sip:controller@kamailio.org:5060");

str ruri_pvar_param = str_init("$ru");
pv_elem_t * ruri_param_model = NULL;
str empty_str = STR_NULL;

/* statistic variables */
int dlg_enable_stats = 1;
int active_dlgs_cnt = 0;
int early_dlgs_cnt = 0;
int detect_spirals = 1;
int dlg_send_bye = 0;
int dlg_timeout_noreset = 0;
stat_var *active_dlgs = 0;
stat_var *processed_dlgs = 0;
stat_var *expired_dlgs = 0;
stat_var *failed_dlgs = 0;
stat_var *early_dlgs  = 0;

struct tm_binds d_tmb;
struct rr_binds d_rrb;
pv_spec_t timeout_avp;

int dlg_db_mode_param = DB_MODE_NONE;

str dlg_xavp_cfg = {0};
int dlg_ka_timer = 0;
int dlg_ka_interval = 0;
int dlg_clean_timer = 90;

str dlg_lreq_callee_headers = {0};

/* db stuff */
static str db_url = str_init(DEFAULT_DB_URL);
static unsigned int db_update_period = DB_DEFAULT_UPDATE_PERIOD;

static int pv_get_dlg_count( struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

void dlg_ka_timer_exec(unsigned int ticks, void* param);
void dlg_clean_timer_exec(unsigned int ticks, void* param);

/* commands wrappers and fixups */
static int fixup_profile(void** param, int param_no);
static int fixup_get_profile2(void** param, int param_no);
static int fixup_get_profile3(void** param, int param_no);
static int w_set_dlg_profile(struct sip_msg*, char*, char*);
static int w_unset_dlg_profile(struct sip_msg*, char*, char*);
static int w_is_in_profile(struct sip_msg*, char*, char*);
static int w_get_profile_size2(struct sip_msg*, char*, char*);
static int w_get_profile_size3(struct sip_msg*, char*, char*, char*);
static int w_dlg_isflagset(struct sip_msg *msg, char *flag, str *s2);
static int w_dlg_resetflag(struct sip_msg *msg, char *flag, str *s2);
static int w_dlg_setflag(struct sip_msg *msg, char *flag, char *s2);
static int w_dlg_set_property(struct sip_msg *msg, char *prop, char *s2);
static int w_dlg_manage(struct sip_msg*, char*, char*);
static int w_dlg_bye(struct sip_msg*, char*, char*);
static int w_dlg_refer(struct sip_msg*, char*, char*);
static int w_dlg_bridge(struct sip_msg*, char*, char*, char*);
static int w_dlg_set_timeout(struct sip_msg*, char*, char*, char*);
static int w_dlg_set_timeout_by_profile2(struct sip_msg *, char *, char *);
static int w_dlg_set_timeout_by_profile3(struct sip_msg *, char *, char *, 
					char *);
static int fixup_dlg_bye(void** param, int param_no);
static int fixup_dlg_refer(void** param, int param_no);
static int fixup_dlg_bridge(void** param, int param_no);
static int w_dlg_get(struct sip_msg*, char*, char*, char*);
static int w_is_known_dlg(struct sip_msg *);

static int w_dlg_remote_profile(sip_msg_t *msg, char *cmd, char *pname,
		char *pval, char *puid, char *expires);
static int fixup_dlg_remote_profile(void** param, int param_no);

static cmd_export_t cmds[]={
	{"dlg_manage", (cmd_function)w_dlg_manage,            0,0,
			0, REQUEST_ROUTE },
	{"set_dlg_profile", (cmd_function)w_set_dlg_profile,  1,fixup_profile,
			0, ANY_ROUTE },
	{"set_dlg_profile", (cmd_function)w_set_dlg_profile,  2,fixup_profile,
			0, ANY_ROUTE },
	{"unset_dlg_profile", (cmd_function)w_unset_dlg_profile,  1,fixup_profile,
			0, REQUEST_ROUTE| FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE },
	{"unset_dlg_profile", (cmd_function)w_unset_dlg_profile,  2,fixup_profile,
			0, REQUEST_ROUTE| FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE },
	{"is_in_profile", (cmd_function)w_is_in_profile,      1,fixup_profile,
			0, ANY_ROUTE },
	{"is_in_profile", (cmd_function)w_is_in_profile,      2,fixup_profile,
			0, ANY_ROUTE },
	{"get_profile_size",(cmd_function)w_get_profile_size2, 2,fixup_get_profile2,
			0, ANY_ROUTE },
	{"get_profile_size",(cmd_function)w_get_profile_size3, 3,fixup_get_profile3,
			0, ANY_ROUTE },
	{"dlg_setflag", (cmd_function)w_dlg_setflag,          1,fixup_igp_null,
			0, ANY_ROUTE },
	{"dlg_resetflag", (cmd_function)w_dlg_resetflag,      1,fixup_igp_null,
			0, ANY_ROUTE },
	{"dlg_isflagset", (cmd_function)w_dlg_isflagset,      1,fixup_igp_null,
			0, ANY_ROUTE },
	{"dlg_bye",(cmd_function)w_dlg_bye,                   1,fixup_dlg_bye,
			0, ANY_ROUTE },
	{"dlg_refer",(cmd_function)w_dlg_refer,               2,fixup_dlg_refer,
			0, ANY_ROUTE },
	{"dlg_bridge",(cmd_function)w_dlg_bridge,             3,fixup_dlg_bridge,
			0, ANY_ROUTE },
	{"dlg_get",(cmd_function)w_dlg_get,                   3,fixup_dlg_bridge,
			0, ANY_ROUTE },
	{"is_known_dlg", (cmd_function)w_is_known_dlg,        0, NULL,
			0, ANY_ROUTE },
	{"dlg_set_timeout", (cmd_function)w_dlg_set_timeout,  1,fixup_igp_null,
			0, ANY_ROUTE },
	{"dlg_set_timeout", (cmd_function)w_dlg_set_timeout,  3,fixup_igp_all,
			0, ANY_ROUTE },
	{"dlg_set_timeout_by_profile", 
		(cmd_function) w_dlg_set_timeout_by_profile2, 2, fixup_profile,
			0, ANY_ROUTE },
	{"dlg_set_timeout_by_profile", 
		(cmd_function) w_dlg_set_timeout_by_profile3, 3, fixup_profile,
			0, ANY_ROUTE },
	{"dlg_set_property", (cmd_function)w_dlg_set_property,1,fixup_spve_null,
			0, ANY_ROUTE },
	{"dlg_remote_profile", (cmd_function)w_dlg_remote_profile, 5, fixup_dlg_remote_profile,
			0, ANY_ROUTE },
	{"load_dlg",  (cmd_function)load_dlg,   0, 0, 0, 0},
	{0,0,0,0,0,0}
};

static param_export_t mod_params[]={
	{ "enable_stats",          INT_PARAM, &dlg_enable_stats         },
	{ "hash_size",             INT_PARAM, &dlg_hash_size            },
	{ "rr_param",              PARAM_STRING, &rr_param                 },
	{ "dlg_flag",              INT_PARAM, &dlg_flag                 },
	{ "timeout_avp",           PARAM_STR, &timeout_spec           },
	{ "default_timeout",       INT_PARAM, &default_timeout          },
	{ "dlg_extra_hdrs",        PARAM_STR, &dlg_extra_hdrs         },
	{ "dlg_match_mode",        INT_PARAM, &seq_match_mode           },
	{ "detect_spirals",        INT_PARAM, &detect_spirals,          },
	{ "db_url",                PARAM_STR, &db_url                 },
	{ "db_mode",               INT_PARAM, &dlg_db_mode_param        },
	{ "table_name",            PARAM_STR, &dialog_table_name        },
	{ "call_id_column",        PARAM_STR, &call_id_column         },
	{ "from_uri_column",       PARAM_STR, &from_uri_column        },
	{ "from_tag_column",       PARAM_STR, &from_tag_column        },
	{ "to_uri_column",         PARAM_STR, &to_uri_column          },
	{ "to_tag_column",         PARAM_STR, &to_tag_column          },
	{ "h_id_column",           PARAM_STR, &h_id_column            },
	{ "h_entry_column",        PARAM_STR, &h_entry_column         },
	{ "state_column",          PARAM_STR, &state_column           },
	{ "start_time_column",     PARAM_STR, &start_time_column      },
	{ "timeout_column",        PARAM_STR, &timeout_column         },
	{ "to_cseq_column",        PARAM_STR, &to_cseq_column         },
	{ "from_cseq_column",      PARAM_STR, &from_cseq_column       },
	{ "to_route_column",       PARAM_STR, &to_route_column        },
	{ "from_route_column",     PARAM_STR, &from_route_column      },
	{ "to_contact_column",     PARAM_STR, &to_contact_column      },
	{ "from_contact_column",   PARAM_STR, &from_contact_column    },
	{ "to_sock_column",        PARAM_STR, &to_sock_column         },
	{ "from_sock_column",      PARAM_STR, &from_sock_column       },
	{ "sflags_column",         PARAM_STR, &sflags_column          },
	{ "toroute_name_column",   PARAM_STR, &toroute_name_column    },

	{ "vars_table_name",       PARAM_STR, &dialog_vars_table_name   },
	{ "vars_h_id_column",      PARAM_STR, &vars_h_id_column       },
	{ "vars_h_entry_column",   PARAM_STR, &vars_h_entry_column    },
	{ "vars_key_column",       PARAM_STR, &vars_key_column        },
	{ "vars_value_column",     PARAM_STR, &vars_value_column      },

	{ "db_update_period",      INT_PARAM, &db_update_period         },
	{ "db_fetch_rows",         INT_PARAM, &db_fetch_rows            },
	{ "profiles_with_value",   PARAM_STRING, &profiles_wv_s            },
	{ "profiles_no_value",     PARAM_STRING, &profiles_nv_s            },
	{ "bridge_controller",     PARAM_STR, &dlg_bridge_controller  },
	{ "bridge_contact",        PARAM_STR, &dlg_bridge_contact       },
	{ "ruri_pvar",             PARAM_STR, &ruri_pvar_param        },
	{ "initial_cbs_inscript",  INT_PARAM, &initial_cbs_inscript     },
	{ "send_bye",              INT_PARAM, &dlg_send_bye             },
	{ "wait_ack",              INT_PARAM, &dlg_wait_ack             },
	{ "xavp_cfg",              PARAM_STR, &dlg_xavp_cfg           },
	{ "ka_timer",              INT_PARAM, &dlg_ka_timer             },
	{ "ka_interval",           INT_PARAM, &dlg_ka_interval          },
	{ "timeout_noreset",       INT_PARAM, &dlg_timeout_noreset      },
	{ "timer_procs",           PARAM_INT, &dlg_timer_procs          },
	{ "track_cseq_updates",    PARAM_INT, &_dlg_track_cseq_updates  },
	{ "lreq_callee_headers",   PARAM_STR, &dlg_lreq_callee_headers  },
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

struct mi_root * mi_dlg_bridge(struct mi_root *cmd_tree, void *param);

static mi_export_t mi_cmds[] = {
	{ "dlg_list",           mi_print_dlgs,       0,  0,  0},
	{ "dlg_list_ctx",       mi_print_dlgs_ctx,   0,  0,  0},
	{ "dlg_end_dlg",        mi_terminate_dlg,    0,  0,  0},
	{ "dlg_terminate_dlg",  mi_terminate_dlgs,   0,  0,  0},
	{ "profile_get_size",   mi_get_profile,      0,  0,  0},
	{ "profile_list_dlgs",  mi_profile_list,     0,  0,  0},
	{ "dlg_bridge",         mi_dlg_bridge,       0,  0,  0},
	{ 0, 0, 0, 0, 0}
};

static rpc_export_t rpc_methods[];

static pv_export_t mod_items[] = {
	{ {"DLG_count",  sizeof("DLG_count")-1}, PVT_OTHER,  pv_get_dlg_count,    0,
		0, 0, 0, 0 },
	{ {"DLG_lifetime",sizeof("DLG_lifetime")-1}, PVT_OTHER, pv_get_dlg_lifetime, 0,
		0, 0, 0, 0 },
	{ {"DLG_status",  sizeof("DLG_status")-1}, PVT_OTHER, pv_get_dlg_status, 0,
		0, 0, 0, 0 },
	{ {"dlg_ctx",  sizeof("dlg_ctx")-1}, PVT_OTHER, pv_get_dlg_ctx,
		pv_set_dlg_ctx, pv_parse_dlg_ctx_name, 0, 0, 0 },
	{ {"dlg",  sizeof("dlg")-1}, PVT_OTHER, pv_get_dlg,
		0, pv_parse_dlg_name, 0, 0, 0 },
	{ {"dlg_var", sizeof("dlg_var")-1}, PVT_OTHER, pv_get_dlg_variable,
		pv_set_dlg_variable,    pv_parse_dialog_var_name, 0, 0, 0},
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
			LM_CRIT("profile <%s> not defined\n",s.s);
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
		ret = fixup_pvar_null(param, 1);
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
	dlgb->terminate_dlg = dlg_bye_all;
	dlgb->set_dlg_var = set_dlg_variable;
	dlgb->get_dlg_var = get_dlg_variable;
	dlgb->get_dlg = dlg_get_msg_dialog;
	dlgb->release_dlg = dlg_release;
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

	if(dlg_ka_interval!=0 && dlg_ka_interval<30) {
		LM_ERR("ka interval too low (%d), has to be at least 30\n",
				dlg_ka_interval);
		return -1;
	}

	dlg_event_rt[DLG_EVENTRT_START] = route_lookup(&event_rt, "dialog:start");
	dlg_event_rt[DLG_EVENTRT_END] = route_lookup(&event_rt, "dialog:end");
	dlg_event_rt[DLG_EVENTRT_FAILED] = route_lookup(&event_rt, "dialog:failed");

#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats( exports.name, mod_stats)!=0 ) {
		LM_ERR("failed to register %s statistics\n", exports.name);
		return -1;
	}
#endif

	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	if (rpc_register_array(rpc_methods)!=0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(faked_msg_init()<0)
		return -1;

	if(dlg_bridge_init_hdrs()<0)
		return -1;

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

	if (ruri_pvar_param.s==NULL || ruri_pvar_param.len<=0) {
		LM_ERR("invalid r-uri PV string\n");
		return -1;
	}

	if(pv_parse_format(&ruri_pvar_param, &ruri_param_model) < 0
				|| ruri_param_model==NULL) {
		LM_ERR("malformed r-uri PV string: %s\n", ruri_pvar_param.s);
		return -1;
	}

	if (initial_cbs_inscript != 0 && initial_cbs_inscript != 1) {
		LM_ERR("invalid parameter for running initial callbacks in-script"
				" (must be either 0 or 1)\n");
		return -1;
	}

	if (seq_match_mode!=SEQ_MATCH_NO_ID &&
	seq_match_mode!=SEQ_MATCH_FALLBACK &&
	seq_match_mode!=SEQ_MATCH_STRICT_ID ) {
		LM_ERR("invalid value %d for seq_match_mode param!!\n",seq_match_mode);
		return -1;
	}

	if (detect_spirals != 0 && detect_spirals != 1) {
		LM_ERR("invalid value %d for detect_spirals param!!\n",detect_spirals);
		return -1;
	}

	if (dlg_timeout_noreset != 0 && dlg_timeout_noreset != 1) {
		LM_ERR("invalid value %d for timeout_noreset param!!\n",
				dlg_timeout_noreset);
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
	if ( d_tmb.register_tmcb( 0, 0, TMCB_REQUEST_IN, dlg_onreq, 0, 0 ) <=0 ) {
		LM_ERR("cannot register TMCB_REQUEST_IN callback\n");
		return -1;
	}

	/* listen for all routed requests  */
	if ( d_rrb.register_rrcb( dlg_onroute, 0 ) <0 ) {
		LM_ERR("cannot register RR callback\n");
		return -1;
	}

	if (register_script_cb( profile_cleanup, POST_SCRIPT_CB|REQUEST_CB,0)<0) {
		LM_ERR("cannot register script callback");
		return -1;
	}
	if (register_script_cb(dlg_cfg_cb,
				PRE_SCRIPT_CB|REQUEST_CB,0)<0)
	{
		LM_ERR("cannot register pre-script ctx callback\n");
		return -1;
	}
	if (register_script_cb(dlg_cfg_cb,
				POST_SCRIPT_CB|REQUEST_CB,0)<0)
	{
		LM_ERR("cannot register post-script ctx callback\n");
		return -1;
	}

	if (register_script_cb(spiral_detect_reset,POST_SCRIPT_CB|REQUEST_CB,0)<0) {
		LM_ERR("cannot register req pre-script spiral detection reset callback\n");
		return -1;
	}

	if (register_script_cb(cb_dlg_locals_reset,POST_SCRIPT_CB|ONREPLY_CB,0)<0) {
		LM_ERR("cannot register reply post-script dlg locals reset callback\n");
		return -1;
	}

	if (register_script_cb(cb_dlg_locals_reset,POST_SCRIPT_CB|FAILURE_CB,0)<0) {
		LM_ERR("cannot register failure post-script dlg locals reset callback\n");
		return -1;
	}

	if(dlg_timer_procs<=0) {
		if ( register_timer( dlg_timer_routine, 0, 1)<0 ) {
			LM_ERR("failed to register timer \n");
			return -1;
		}
	} else {
		register_sync_timers(1);
	}

	/* init handlers */
	init_dlg_handlers( rr_param, dlg_flag,
		timeout_spec.s?&timeout_avp:0, default_timeout, seq_match_mode);

	/* init timer */
	if (init_dlg_timer(dlg_ontimeout)!=0) {
		LM_ERR("cannot init timer list\n");
		return -1;
	}

	/* sanitize dlg_hash_zie */
	if (dlg_hash_size < 1){
		LM_WARN("hash_size is smaller "
				"then 1  -> rounding from %d to 1\n",
				dlg_hash_size);
		dlg_hash_size = 1;
	}
	/* initialized the hash table */
	for( n=0 ; n<(8*sizeof(n)) ; n++) {
		if (dlg_hash_size==(1<<n))
			break;
		if (n && dlg_hash_size<(1<<n)) {
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
	dlg_db_mode = dlg_db_mode_param;
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

	/* timer process to send keep alive requests */
	if(dlg_ka_timer>0 && dlg_ka_interval>0)
		register_sync_timers(1);

	/* timer process to clean old unconfirmed dialogs */
	register_sync_timers(1);

	if(_dlg_track_cseq_updates!=0)
		dlg_register_cseq_callbacks();

	return 0;
}


static int child_init(int rank)
{
	dlg_db_mode = dlg_db_mode_param;

	if(rank==PROC_MAIN) {
		if(dlg_timer_procs>0) {
			if(fork_sync_timer(PROC_TIMER, "Dialog Main Timer", 1 /*socks flag*/,
					dlg_timer_routine, NULL, 1 /*every sec*/)<0) {
				LM_ERR("failed to start main timer routine as process\n");
				return -1; /* error */
			}
		}

		if(dlg_ka_timer>0 && dlg_ka_interval>0) {
			if(fork_sync_timer(PROC_TIMER, "Dialog KA Timer", 1 /*socks flag*/,
					dlg_ka_timer_exec, NULL, dlg_ka_timer /*sec*/)<0) {
				LM_ERR("failed to start ka timer routine as process\n");
				return -1; /* error */
			}
		}

		if(fork_sync_timer(PROC_TIMER, "Dialog Clean Timer", 1 /*socks flag*/,
					dlg_clean_timer_exec, NULL, dlg_clean_timer /*sec*/)<0) {
			LM_ERR("failed to start clean timer routine as process\n");
			return -1; /* error */
		}
	}

	if (rank==1) {
		if_update_stat(dlg_enable_stats, active_dlgs, active_dlgs_cnt);
		if_update_stat(dlg_enable_stats, early_dlgs, early_dlgs_cnt);
	}

	if ( ((dlg_db_mode==DB_MODE_REALTIME || dlg_db_mode==DB_MODE_DELAYED) &&
	(rank>0 || rank==PROC_TIMER)) ||
	(dlg_db_mode==DB_MODE_SHUTDOWN && (rank==PROC_MAIN)) ) {
		if ( dlg_connect_db(&db_url) ) {
			LM_ERR("failed to connect to database (rank=%d)\n",rank);
			return -1;
		}
	}

	/* in DB_MODE_SHUTDOWN only PROC_MAIN will do a DB dump at the end, so
	 * for the rest of the processes will be the same as DB_MODE_NONE */
	if (dlg_db_mode==DB_MODE_SHUTDOWN && rank!=PROC_MAIN)
		dlg_db_mode = DB_MODE_NONE;
	/* in DB_MODE_REALTIME and DB_MODE_DELAYED the PROC_MAIN have no DB handle */
	if ( (dlg_db_mode==DB_MODE_REALTIME || dlg_db_mode==DB_MODE_DELAYED) &&
			rank==PROC_MAIN)
		dlg_db_mode = DB_MODE_NONE;

	return 0;
}


static void mod_destroy(void)
{
	if(dlg_db_mode == DB_MODE_DELAYED || dlg_db_mode == DB_MODE_SHUTDOWN) {
		dialog_update_db(0, 0);
		destroy_dlg_db();
	}
	dlg_bridge_destroy_hdrs();
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



static int w_unset_dlg_profile(struct sip_msg *msg, char *profile, char *value)
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
		if ( unset_dlg_profile( msg, &val_s,
		(struct dlg_profile_table*)profile) < 0 ) {
			LM_ERR("failed to unset profile");
			return -1;
		}
	} else {
		if ( unset_dlg_profile( msg, NULL,
		(struct dlg_profile_table*)profile) < 0 ) {
			LM_ERR("failed to unset profile");
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


/**
 * get dynamic name profile size
 */
static int w_get_profile_size3(struct sip_msg *msg, char *profile,
		char *value, char *result)
{
	pv_elem_t *pve;
	str val_s;
	pv_spec_t *sp_dest;
	unsigned int size;
	pv_value_t val;

	if(result!=NULL)
	{
		pve = (pv_elem_t *)value;
		sp_dest = (pv_spec_t *)result;
	} else {
		pve = NULL;
		sp_dest = (pv_spec_t *)value;
	}
	if ( pve!=NULL && ((struct dlg_profile_table*)profile)->has_value) {
		if ( pv_printf_s(msg, pve, &val_s)!=0 || 
		val_s.len == 0 || val_s.s == NULL) {
			LM_WARN("cannot get string for value\n");
			return -1;
		}
		size = get_profile_size( (struct dlg_profile_table*)profile, &val_s );
	} else {
		size = get_profile_size( (struct dlg_profile_table*)profile, NULL );
	}

	memset(&val, 0, sizeof(pv_value_t));
	val.flags = PV_VAL_INT|PV_TYPE_INT;
	val.ri = (int)size;

	if(sp_dest->setf(msg, &sp_dest->pvp, (int)EQ_T, &val)<0)
	{
		LM_ERR("setting profile PV failed\n");
		return -1;
	}

	return 1;
}


/**
 * get static name profile size
 */
static int w_get_profile_size2(struct sip_msg *msg, char *profile, char *result)
{
	return w_get_profile_size3(msg, profile, result, NULL);
}


static int w_dlg_setflag(struct sip_msg *msg, char *flag, char *s2)
{
	dlg_ctx_t *dctx;
	dlg_cell_t *d;
	int val;

	if(fixup_get_ivalue(msg, (gparam_p)flag, &val)!=0)
	{
		LM_ERR("no flag value\n");
		return -1;
	}
	if(val<0 || val>31)
		return -1;
	if ( (dctx=dlg_get_dlg_ctx())==NULL )
		return -1;

	dctx->flags |= 1<<val;
	d = dlg_get_by_iuid(&dctx->iuid);
	if(d!=NULL) {
		d->sflags |= 1<<val;
		dlg_release(d);
	}
	return 1;
}


static int w_dlg_resetflag(struct sip_msg *msg, char *flag, str *s2)
{
	dlg_ctx_t *dctx;
	dlg_cell_t *d;
	int val;

	if(fixup_get_ivalue(msg, (gparam_p)flag, &val)!=0)
	{
		LM_ERR("no flag value\n");
		return -1;
	}
	if(val<0 || val>31)
		return -1;

	if ( (dctx=dlg_get_dlg_ctx())==NULL )
		return -1;

	dctx->flags &= ~(1<<val);
	d = dlg_get_by_iuid(&dctx->iuid);
	if(d!=NULL) {
		d->sflags &= ~(1<<val);
		dlg_release(d);
	}
	return 1;
}


static int w_dlg_isflagset(struct sip_msg *msg, char *flag, str *s2)
{
	dlg_ctx_t *dctx;
	dlg_cell_t *d;
	int val;
	int ret;

	if(fixup_get_ivalue(msg, (gparam_p)flag, &val)!=0)
	{
		LM_ERR("no flag value\n");
		return -1;
	}
	if(val<0 || val>31)
		return -1;

	if ( (dctx=dlg_get_dlg_ctx())==NULL )
		return -1;

	d = dlg_get_by_iuid(&dctx->iuid);
	if(d!=NULL) {
		ret = (d->sflags&(1<<val))?1:-1;
		dlg_release(d);
		return ret;
	}
	return (dctx->flags&(1<<val))?1:-1;
}

/**
 *
 */
static int w_dlg_manage(struct sip_msg *msg, char *s1, char *s2)
{
	return dlg_manage(msg);
}

static int w_dlg_bye(struct sip_msg *msg, char *side, char *s2)
{
	dlg_cell_t *dlg = NULL;
	int n;

	dlg = dlg_get_ctx_dialog();
	if(dlg==NULL)
		return -1;
	
	n = (int)(long)side;
	if(n==1)
	{
		if(dlg_bye(dlg, NULL, DLG_CALLER_LEG)!=0)
			goto error;
		goto done;
	} else if(n==2) {
		if(dlg_bye(dlg, NULL, DLG_CALLEE_LEG)!=0)
			goto error;
		goto done;
	} else {
		if(dlg_bye_all(dlg, NULL)!=0)
			goto error;
		goto done;
	}

done:
	dlg_release(dlg);
	return 1;

error:
	dlg_release(dlg);
	return -1;
}

static int w_dlg_refer(struct sip_msg *msg, char *side, char *to)
{
	dlg_cell_t *dlg;
	int n;
	str st = {0,0};

	dlg = dlg_get_ctx_dialog();
	if(dlg==NULL)
		return -1;
	
	n = (int)(long)side;

	if(fixup_get_svalue(msg, (gparam_p)to, &st)!=0)
	{
		LM_ERR("unable to get To\n");
		goto error;
	}
	if(st.s==NULL || st.len == 0)
	{
		LM_ERR("invalid To parameter\n");
		goto error;
	}
	if(n==1)
	{
		if(dlg_transfer(dlg, &st, DLG_CALLER_LEG)!=0)
			goto error;
	} else {
		if(dlg_transfer(dlg, &st, DLG_CALLEE_LEG)!=0)
			goto error;
	}

	dlg_release(dlg);
	return 1;

error:
	dlg_release(dlg);
	return -1;
}

static int w_dlg_bridge(struct sip_msg *msg, char *from, char *to, char *op)
{
	str sf = {0,0};
	str st = {0,0};
	str so = {0,0};

	if(from==0 || to==0 || op==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)from, &sf)!=0)
	{
		LM_ERR("unable to get From\n");
		return -1;
	}
	if(sf.s==NULL || sf.len == 0)
	{
		LM_ERR("invalid From parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)to, &st)!=0)
	{
		LM_ERR("unable to get To\n");
		return -1;
	}
	if(st.s==NULL || st.len == 0)
	{
		LM_ERR("invalid To parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)op, &so)!=0)
	{
		LM_ERR("unable to get OP\n");
		return -1;
	}

	if(dlg_bridge(&sf, &st, &so, NULL)!=0)
		return -1;
	return 1;
}

/**
 *
 */
static int w_dlg_set_timeout(struct sip_msg *msg, char *pto, char *phe, char *phi)
{
	int to = 0;
	unsigned int he = 0;
	unsigned int hi = 0;
	dlg_cell_t *dlg = NULL;

	if(fixup_get_ivalue(msg, (gparam_p)pto, &to)!=0)
	{
		LM_ERR("no timeout value\n");
		return -1;
	}
	if(phe!=NULL)
	{
		if(fixup_get_ivalue(msg, (gparam_p)phe, (int*)&he)!=0)
		{
			LM_ERR("no hash entry value value\n");
			return -1;
		}
		if(fixup_get_ivalue(msg, (gparam_p)phi, (int*)&hi)!=0)
		{
			LM_ERR("no hash id value value\n");
			return -1;
		}
		dlg = dlg_lookup(he, hi);
	} else {
		dlg = dlg_get_msg_dialog(msg);
	}

	if(dlg==NULL)
	{
		LM_DBG("no dialog found\n");
		return -1;
	}

	if(update_dlg_timeout(dlg, to) != 0) 
		return -1;

	return 1;
}

static int w_dlg_set_property(struct sip_msg *msg, char *prop, char *s2)
{
	dlg_ctx_t *dctx;
	dlg_cell_t *d;
	str val;

	if(fixup_get_svalue(msg, (gparam_t*)prop, &val)!=0)
	{
		LM_ERR("no property value\n");
		return -1;
	}
	if(val.len<=0)
	{
		LM_ERR("empty property value\n");
		return -1;
	}
	if ( (dctx=dlg_get_dlg_ctx())==NULL )
		return -1;

	if(val.len==6 && strncmp(val.s, "ka-src", 6)==0) {
		dctx->iflags |= DLG_IFLAG_KA_SRC;
		d = dlg_get_by_iuid(&dctx->iuid);
		if(d!=NULL) {
			d->iflags |= DLG_IFLAG_KA_SRC;
			dlg_release(d);
		}
	} else if(val.len==6 && strncmp(val.s, "ka-dst", 6)==0) {
		dctx->iflags |= DLG_IFLAG_KA_DST;
		d = dlg_get_by_iuid(&dctx->iuid);
		if(d!=NULL) {
			d->iflags |= DLG_IFLAG_KA_DST;
			dlg_release(d);
		}
	} else if(val.len==15 && strncmp(val.s, "timeout-noreset", 15)==0) {
		dctx->iflags |= DLG_IFLAG_TIMER_NORESET;
		d = dlg_get_by_iuid(&dctx->iuid);
		if(d!=NULL) {
			d->iflags |= DLG_IFLAG_TIMER_NORESET;
			dlg_release(d);
		}
	} else {
		LM_ERR("unknown property value [%.*s]\n", val.len, val.s);
		return -1;
	}

	return 1;
}

static int w_dlg_set_timeout_by_profile3(struct sip_msg *msg, char *profile,
					char *value, char *timeout_str) 
{
	pv_elem_t *pve = NULL;
	str val_s;

	pve = (pv_elem_t *) value;

	if(pve != NULL && ((struct dlg_profile_table *) profile)->has_value) {
		if(pv_printf_s(msg,pve, &val_s) != 0 || 
		   !val_s.s || val_s.len == 0) {
			LM_WARN("cannot get string for value\n");
			return -1;
		}
	}

	if(dlg_set_timeout_by_profile((struct dlg_profile_table *) profile,
				   &val_s, atoi(timeout_str)) != 0)
		return -1;

	return 1;
}

static int w_dlg_set_timeout_by_profile2(struct sip_msg *msg, 
					 char *profile, char *timeout_str)
{
	return w_dlg_set_timeout_by_profile3(msg, profile, NULL, timeout_str);
}

void dlg_ka_timer_exec(unsigned int ticks, void* param)
{
	dlg_ka_run(ticks);
}

void dlg_clean_timer_exec(unsigned int ticks, void* param)
{
	dlg_clean_run(ticks);
	remove_expired_remote_profiles(time(NULL));
}

static int fixup_dlg_bye(void** param, int param_no)
{
	char *val;
	int n = 0;

	if (param_no==1) {
		val = (char*)*param;
		if (strcasecmp(val,"all")==0) {
			n = 0;
		} else if (strcasecmp(val,"caller")==0) {
			n = 1;
		} else if (strcasecmp(val,"callee")==0) {
			n = 2;
		} else {
			LM_ERR("invalid param \"%s\"\n", val);
			return E_CFG;
		}
		pkg_free(*param);
		*param=(void*)(long)n;
	} else {
		LM_ERR("called with parameter != 1\n");
		return E_BUG;
	}
	return 0;
}

static int fixup_dlg_refer(void** param, int param_no)
{
	char *val;
	int n = 0;

	if (param_no==1) {
		val = (char*)*param;
		if (strcasecmp(val,"caller")==0) {
			n = 1;
		} else if (strcasecmp(val,"callee")==0) {
			n = 2;
		} else {
			LM_ERR("invalid param \"%s\"\n", val);
			return E_CFG;
		}
		pkg_free(*param);
		*param=(void*)(long)n;
	} else if (param_no==2) {
		return fixup_spve_null(param, 1);
	} else {
		LM_ERR("called with parameter idx %d\n", param_no);
		return E_BUG;
	}
	return 0;
}

static int fixup_dlg_bridge(void** param, int param_no)
{
	if (param_no>=1 && param_no<=3) {
		return fixup_spve_null(param, 1);
	} else {
		LM_ERR("called with parameter idx %d\n", param_no);
		return E_BUG;
	}
	return 0;
}

static int w_dlg_get(struct sip_msg *msg, char *ci, char *ft, char *tt)
{
	dlg_cell_t *dlg = NULL;
	str sc = {0,0};
	str sf = {0,0};
	str st = {0,0};
	unsigned int dir = 0;

	if(ci==0 || ft==0 || tt==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)ci, &sc)!=0)
	{
		LM_ERR("unable to get Call-ID\n");
		return -1;
	}
	if(sc.s==NULL || sc.len == 0)
	{
		LM_ERR("invalid Call-ID parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)ft, &sf)!=0)
	{
		LM_ERR("unable to get From tag\n");
		return -1;
	}
	if(sf.s==NULL || sf.len == 0)
	{
		LM_ERR("invalid From tag parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)tt, &st)!=0)
	{
		LM_ERR("unable to get To Tag\n");
		return -1;
	}
	if(st.s==NULL || st.len == 0)
	{
		LM_ERR("invalid To tag parameter\n");
		return -1;
	}

	dlg = get_dlg(&sc, &sf, &st, &dir);
	if(dlg==NULL)
		return -1;
    /* set shorcut to dialog internal unique id */
	_dlg_ctx.iuid.h_entry = dlg->h_entry;
	_dlg_ctx.iuid.h_id = dlg->h_id;
	_dlg_ctx.dir = dir;
	dlg_release(dlg);
	return 1;
}

/**
 *
 */
static int w_dlg_remote_profile(sip_msg_t *msg, char *cmd, char *pname,
		char *pval, char *puid, char *expires)
{
	str scmd;
	str sname;
	str sval;
	str suid;
	int ival;
	int ret;

	if(fixup_get_svalue(msg, (gparam_t*)cmd, &scmd)!=0) {
		LM_ERR("unable to get command\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pname, &sname)!=0) {
		LM_ERR("unable to get profile name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pval, &sval)!=0) {
		LM_ERR("unable to get profile value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)puid, &suid)!=0) {
		LM_ERR("unable to get profile uid\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t*)expires, &ival)!=0) {
		LM_ERR("no hash entry value value\n");
		return -1;
	}

	ret = dlg_cmd_remote_profile(&scmd, &sname, &sval, &suid, (time_t)ival, 0);
	if(ret==0)
		return 1;
	return ret;
}

static int fixup_dlg_remote_profile(void** param, int param_no)
{
	if(param_no>=1 && param_no<=4)
		return fixup_spve_null(param, 1);
	if(param_no==5)
		return fixup_igp_null(param, 1);
	return 0;
}

struct mi_root * mi_dlg_bridge(struct mi_root *cmd_tree, void *param)
{
	str from = {0,0};
	str to = {0,0};
	str op = {0,0};
	str bd = {0,0};
	struct mi_node* node;

	node = cmd_tree->node.kids;
	if(node == NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
	from = node->value;
	if(from.len<=0 || from.s==NULL)
	{
		LM_ERR("bad From value\n");
		return init_mi_tree( 500, "Bad From value", 14);
	}

	node = node->next;
	if(node == NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
	to = node->value;
	if(to.len<=0 || to.s == NULL)
	{
		return init_mi_tree(500, "Bad To value", 12);
	}

	node= node->next;
	if(node != NULL)
	{
		op = node->value;
		if(op.len<=0 || op.s==NULL)
		{
			return init_mi_tree(500, "Bad OP value", 12);
		}
		if(op.len==1 && *op.s=='.')
		{
			op.s = NULL;
			op.len = 0;
		}
		node= node->next;
		if(node != NULL)
		{
			bd = node->value;
			if(bd.len<=0 || bd.s==NULL)
			{
				return init_mi_tree(500, "Bad SDP value", 13);
			}
		}
	}

	if(dlg_bridge(&from, &to, &op, &bd)!=0)
		return init_mi_tree(500, MI_INTERNAL_ERR_S,  MI_INTERNAL_ERR_LEN);

	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}

/**************************** RPC functions ******************************/
/*!
 * \brief Helper method that outputs a dialog via the RPC interface
 * \see rpc_print_dlg
 * \param rpc RPC node that should be filled
 * \param c RPC void pointer
 * \param dlg printed dialog
 * \param with_context if 1 then the dialog context will be also printed
 * \return 0 on success, -1 on failure
 */
static inline void internal_rpc_print_dlg(rpc_t *rpc, void *c, dlg_cell_t *dlg,
		int with_context)
{
	rpc_cb_ctx_t rpc_cb;
	void *h, *sh, *ssh;
	dlg_profile_link_t *pl;
	dlg_var_t *var;

	if (rpc->add(c, "{", &h) < 0) goto error;

	rpc->struct_add(h, "ddSSSdddddddd",
		"h_entry", dlg->h_entry,
		"h_id", dlg->h_id,
		"call-id", &dlg->callid,
		"from_uri", &dlg->from_uri,
		"to_uri", &dlg->to_uri,
		"state", dlg->state,
		"start_ts", dlg->start_ts,
		"init_ts", dlg->init_ts,
		"timeout", dlg->tl.timeout ? time(0) + dlg->tl.timeout - get_ticks() : 0,
		"lifetime", dlg->lifetime,
		"dflags", dlg->dflags,
		"sflags", dlg->sflags,
		"iflags", dlg->iflags);

	if (rpc->struct_add(h, "{", "caller", &sh) < 0) goto error;
	rpc->struct_add(sh, "SSSSS",
		"tag", &dlg->tag[DLG_CALLER_LEG],
		"contact", &dlg->contact[DLG_CALLER_LEG],
		"cseq", &dlg->cseq[DLG_CALLER_LEG],
		"route_set", &dlg->route_set[DLG_CALLER_LEG],
		"socket", dlg->bind_addr[DLG_CALLER_LEG] ? &dlg->bind_addr[DLG_CALLER_LEG]->sock_str : &empty_str);

	if (rpc->struct_add(h, "{", "callee", &sh) < 0) goto error;
	rpc->struct_add(sh, "SSSSS",
		"tag", &dlg->tag[DLG_CALLEE_LEG],
		"contact", &dlg->contact[DLG_CALLEE_LEG],
		"cseq", &dlg->cseq[DLG_CALLEE_LEG],
		"route_set", &dlg->route_set[DLG_CALLEE_LEG],
		"socket", dlg->bind_addr[DLG_CALLEE_LEG] ? &dlg->bind_addr[DLG_CALLEE_LEG]->sock_str : &empty_str);

	if (rpc->struct_add(h, "[", "profiles", &sh) < 0) goto error;
	for (pl = dlg->profile_links ; pl ; pl=pl->next) {
		if (pl->profile->has_value) {
			rpc->array_add(sh, "{", &ssh);
			rpc->struct_add(ssh, "S", pl->profile->name.s, &pl->hash_linker.value);
		} else {
			rpc->array_add(sh, "S", &pl->profile->name);
		}
	}

	if (rpc->struct_add(h, "[", "variables", &sh) < 0) goto error;
	for(var=dlg->vars ; var ; var=var->next) {
		rpc->array_add(sh, "{", &ssh);
		rpc->struct_add(ssh, "S", var->key.s, &var->value);
	}

	if (with_context) {
		rpc_cb.rpc = rpc;
		rpc_cb.c = h;
		run_dlg_callbacks( DLGCB_RPC_CONTEXT, dlg, NULL, NULL, DLG_DIR_NONE, (void *)&rpc_cb);
	}

	return;
error:
	LM_ERR("Failed to add item to RPC response\n");
	return;
}

/*!
 * \brief Helper function that outputs all dialogs via the RPC interface
 * \see rpc_print_dlgs
 * \param rpc RPC node that should be filled
 * \param c RPC void pointer
 * \param with_context if 1 then the dialog context will be also printed
 */
static void internal_rpc_print_dlgs(rpc_t *rpc, void *c, int with_context)
{
	dlg_cell_t *dlg;
	unsigned int i;

	for( i=0 ; i<d_table->size ; i++ ) {
		dlg_lock( d_table, &(d_table->entries[i]) );

		for( dlg=d_table->entries[i].first ; dlg ; dlg=dlg->next ) {
			internal_rpc_print_dlg(rpc, c, dlg, with_context);
		}
		dlg_unlock( d_table, &(d_table->entries[i]) );
	}
}

/*!
 * \brief Helper function that outputs a dialog via the RPC interface
 * \see rpc_print_dlgs
 * \param rpc RPC node that should be filled
 * \param c RPC void pointer
 * \param with_context if 1 then the dialog context will be also printed
 */
static void internal_rpc_print_single_dlg(rpc_t *rpc, void *c, int with_context) {
	str callid, from_tag;
	dlg_entry_t *d_entry;
	dlg_cell_t *dlg;
	unsigned int h_entry;

	if (rpc->scan(c, ".S.S", &callid, &from_tag) < 2) return;

	h_entry = core_hash( &callid, 0, d_table->size);
	d_entry = &(d_table->entries[h_entry]);
	dlg_lock( d_table, d_entry);
	for( dlg = d_entry->first ; dlg ; dlg = dlg->next ) {
		if (match_downstream_dialog( dlg, &callid, &from_tag)==1) {
			internal_rpc_print_dlg(rpc, c, dlg, with_context);
		}
	}
	dlg_unlock( d_table, d_entry);
}

/*!
 * \brief Helper function that outputs the size of a given profile via the RPC interface
 * \see rpc_profile_get_size
 * \see rpc_profile_w_value_get_size
 * \param rpc RPC node that should be filled
 * \param c RPC void pointer
 * \param profile_name the given profile
 * \param value the given profile value
 */
static void internal_rpc_profile_get_size(rpc_t *rpc, void *c, str *profile_name,
		str *value) {
	unsigned int size;
	dlg_profile_table_t *profile;

	profile = search_dlg_profile( profile_name );
	if (!profile) {
		rpc->fault(c, 404, "Profile not found: %.*s",
			profile_name->len, profile_name->s);
		return;
	}
	size = get_profile_size(profile, value);
	rpc->add(c, "d", size);
	return;
}

/*!
 * \brief Helper function that outputs the dialogs belonging to a given profile via the RPC interface
 * \see rpc_profile_print_dlgs
 * \see rpc_profile_w_value_print_dlgs
 * \param rpc RPC node that should be filled
 * \param c RPC void pointer
 * \param profile_name the given profile
 * \param value the given profile value
 * \param with_context if 1 then the dialog context will be also printed
 */
static void internal_rpc_profile_print_dlgs(rpc_t *rpc, void *c, str *profile_name,
		str *value) {
	dlg_profile_table_t *profile;
	dlg_profile_hash_t *ph;
	unsigned int i;

	profile = search_dlg_profile( profile_name );
	if (!profile) {
		rpc->fault(c, 404, "Profile not found: %.*s",
			profile_name->len, profile_name->s);
		return;
	}

	/* go through the hash and print the dialogs */
	if (profile->has_value==0)
		value=NULL;

	lock_get( &profile->lock );
	for ( i=0 ; i< profile->size ; i++ ) {
		ph = profile->entries[i].first;
		if(ph) {
			do {
				if ((!value || (STR_EQ(*value, ph->value))) && ph->dlg) {
					/* print dialog */
					internal_rpc_print_dlg(rpc, c, ph->dlg, 0);
				}
				/* next */
				ph=ph->next;
			}while(ph!=profile->entries[i].first);
		}
		lock_release(&profile->lock);
	}
}

/*
 * Wrapper around is_known_dlg().
 */

static int w_is_known_dlg(sip_msg_t *msg) {
	return	is_known_dlg(msg);
}

static const char *rpc_print_dlgs_doc[2] = {
	"Print all dialogs", 0
};
static const char *rpc_print_dlgs_ctx_doc[2] = {
	"Print all dialogs with associated context", 0
};
static const char *rpc_print_dlg_doc[2] = {
	"Print dialog based on callid and fromtag", 0
};
static const char *rpc_print_dlg_ctx_doc[2] = {
	"Print dialog with associated context based on callid and fromtag", 0
};
static const char *rpc_end_dlg_entry_id_doc[2] = {
	"End a given dialog based on [h_entry] [h_id]", 0
};
static const char *rpc_profile_get_size_doc[2] = {
	"Returns the number of dialogs belonging to a profile", 0
};
static const char *rpc_profile_print_dlgs_doc[2] = {
	"Lists all the dialogs belonging to a profile", 0
};
static const char *rpc_dlg_bridge_doc[2] = {
	"Bridge two SIP addresses in a call using INVITE(hold)-REFER-BYE mechanism:\
 to, from, [outbound SIP proxy]", 0
};


static void rpc_print_dlgs(rpc_t *rpc, void *c) {
	internal_rpc_print_dlgs(rpc, c, 0);
}
static void rpc_print_dlgs_ctx(rpc_t *rpc, void *c) {
	internal_rpc_print_dlgs(rpc, c, 1);
}
static void rpc_print_dlg(rpc_t *rpc, void *c) {
	internal_rpc_print_single_dlg(rpc, c, 0);
}
static void rpc_print_dlg_ctx(rpc_t *rpc, void *c) {
	internal_rpc_print_single_dlg(rpc, c, 1);
}
static void rpc_end_dlg_entry_id(rpc_t *rpc, void *c) {
	unsigned int h_entry, h_id;
	dlg_cell_t * dlg = NULL;
	str rpc_extra_hdrs = {NULL,0};
	int n;

	n = rpc->scan(c, "dd", &h_entry, &h_id);
	if (n < 2) {
		LM_ERR("unable to read the parameters (%d)\n", n);
		rpc->fault(c, 500, "Invalid parameters");
		return;
	}
	if(rpc->scan(c, "*S", &rpc_extra_hdrs)<1)
	{
		rpc_extra_hdrs.s = NULL;
		rpc_extra_hdrs.len = 0;
	}

	dlg = dlg_lookup(h_entry, h_id);
	if(dlg==NULL) {
		rpc->fault(c, 404, "Dialog not found");
		return;
	}

	dlg_bye_all(dlg, (rpc_extra_hdrs.len>0)?&rpc_extra_hdrs:NULL);
	dlg_release(dlg);
}
static void rpc_profile_get_size(rpc_t *rpc, void *c) {
	str profile_name = {NULL,0};
	str value = {NULL,0};

	if (rpc->scan(c, ".S", &profile_name) < 1) return;
	if (rpc->scan(c, "*.S", &value) > 0) {
		internal_rpc_profile_get_size(rpc, c, &profile_name, &value);
	} else {
		internal_rpc_profile_get_size(rpc, c, &profile_name, NULL);
	}
	return;
}
static void rpc_profile_print_dlgs(rpc_t *rpc, void *c) {
	str profile_name = {NULL,0};
	str value = {NULL,0};

	if (rpc->scan(c, ".S", &profile_name) < 1) return;
	if (rpc->scan(c, "*.S", &value) > 0) {
		internal_rpc_profile_print_dlgs(rpc, c, &profile_name, &value);
	} else {
		internal_rpc_profile_print_dlgs(rpc, c, &profile_name, NULL);
	}
	return;
}
static void rpc_dlg_bridge(rpc_t *rpc, void *c) {
	str from = {NULL,0};
	str to = {NULL,0};
	str op = {NULL,0};
	str bd = {NULL,0};
	int n;

	n = rpc->scan(c, "SS", &from, &to);
	if (n< 2) {
		LM_ERR("unable to read the parameters (%d)\n", n);
		rpc->fault(c, 500, "Invalid parameters");
		return;
	}
	if(rpc->scan(c, "*S", &op)<1) {
		op.s = NULL;
		op.len = 0;
	} else {
		if(op.len==1 && *op.s=='.') {
			op.s = NULL;
			op.len = 0;
		}
		if(rpc->scan(c, "*S", &bd)<1) {
			bd.s = NULL;
			bd.len = 0;
		}
	}

	dlg_bridge(&from, &to, &op, &bd);
}

static rpc_export_t rpc_methods[] = {
	{"dlg.list", rpc_print_dlgs, rpc_print_dlgs_doc, RET_ARRAY},
	{"dlg.list_ctx", rpc_print_dlgs_ctx, rpc_print_dlgs_ctx_doc, RET_ARRAY},
	{"dlg.dlg_list", rpc_print_dlg, rpc_print_dlg_doc, 0},
	{"dlg.dlg_list_ctx", rpc_print_dlg_ctx, rpc_print_dlg_ctx_doc, 0},
	{"dlg.end_dlg", rpc_end_dlg_entry_id, rpc_end_dlg_entry_id_doc, 0},
	{"dlg.profile_get_size", rpc_profile_get_size, rpc_profile_get_size_doc, 0},
	{"dlg.profile_list", rpc_profile_print_dlgs, rpc_profile_print_dlgs_doc, RET_ARRAY},
	{"dlg.bridge_dlg", rpc_dlg_bridge, rpc_dlg_bridge_doc, 0},
	{0, 0, 0, 0}
};

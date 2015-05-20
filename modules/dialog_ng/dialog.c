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
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../mem/mem.h"
#include "../../lib/kmi/mi.h"
#include "../../lvalue.h"
#include "../../parser/parse_to.h"
#include "../../modules/tm/tm_load.h"
#include "../../rpc_lookup.h"
#include "../../modules/rr/api.h"

#include "dlg_hash.h"
#include "dlg_timer.h"
#include "dlg_handlers.h"
#include "dlg_load.h"
#include "dlg_cb.h"
#include "dlg_profile.h"
#include "dlg_var.h"
#include "dlg_req_within.h"
#include "dlg_db_handler.h"
#include "dlg_ng_stats.h"

MODULE_VERSION


static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

/* module parameter */
static int dlg_hash_size = 4096;

static char* rr_param = "did";
static int dlg_flag = -1;


static str timeout_spec = {NULL, 0};
static int default_timeout = 60 * 60 * 12; /* 12 hours */
static int seq_match_mode = SEQ_MATCH_STRICT_ID;
static char* profiles_wv_s = NULL;
static char* profiles_nv_s = NULL;
int detect_spirals = 1;
str dlg_extra_hdrs = {NULL, 0};
int initial_cbs_inscript = 1;

str dlg_bridge_controller = str_init("sip:controller@kamailio.org");

str ruri_pvar_param = str_init("$ru");
pv_elem_t * ruri_param_model = NULL;

struct tm_binds d_tmb;
struct rr_binds d_rrb;
pv_spec_t timeout_avp;

/* db stuff */
int dlg_db_mode_param = DB_MODE_NONE;
static int db_fetch_rows = 200;
static str db_url = str_init(DEFAULT_DB_URL);
static unsigned int db_update_period = DB_DEFAULT_UPDATE_PERIOD;

/* commands wrappers and fixups */
static int fixup_profile(void** param, int param_no);
static int fixup_get_profile2(void** param, int param_no);
static int fixup_get_profile3(void** param, int param_no);
static int fixup_dlg_bridge(void** param, int param_no);
static int fixup_dlg_terminate(void** param, int param_no);
static int w_set_dlg_profile(struct sip_msg*, char*, char*);
static int w_unset_dlg_profile(struct sip_msg*, char*, char*);
static int w_is_in_profile(struct sip_msg*, char*, char*);
static int w_get_profile_size2(struct sip_msg*, char*, char*);
static int w_get_profile_size3(struct sip_msg*, char*, char*, char*);
static int w_dlg_isflagset(struct sip_msg *msg, char *flag, str *s2);
static int w_dlg_resetflag(struct sip_msg *msg, char *flag, str *s2);
static int w_dlg_setflag(struct sip_msg *msg, char *flag, char *s2);
static int w_dlg_terminate(struct sip_msg*, char*, char*);
static int w_dlg_get(struct sip_msg*, char*, char*, char*);
static int w_is_known_dlg(struct sip_msg *);

static cmd_export_t cmds[] = {
    {"set_dlg_profile", (cmd_function) w_set_dlg_profile, 1, fixup_profile,
        0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},
    {"set_dlg_profile", (cmd_function) w_set_dlg_profile, 2, fixup_profile,
        0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},
    {"unset_dlg_profile", (cmd_function) w_unset_dlg_profile, 1, fixup_profile,
        0, FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},
    {"unset_dlg_profile", (cmd_function) w_unset_dlg_profile, 2, fixup_profile,
        0, FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},
    {"is_in_profile", (cmd_function) w_is_in_profile, 1, fixup_profile,
        0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},
    {"is_in_profile", (cmd_function) w_is_in_profile, 2, fixup_profile,
        0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},
    {"get_profile_size", (cmd_function) w_get_profile_size2, 2, fixup_get_profile2,
        0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},
    {"get_profile_size", (cmd_function) w_get_profile_size3, 3, fixup_get_profile3,
        0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},
    {"dlg_setflag", (cmd_function) w_dlg_setflag, 1, fixup_igp_null,
        0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},
    {"dlg_resetflag", (cmd_function) w_dlg_resetflag, 1, fixup_igp_null,
        0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},
    {"dlg_isflagset", (cmd_function) w_dlg_isflagset, 1, fixup_igp_null,
        0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},
    {"dlg_terminate", (cmd_function) w_dlg_terminate, 1, fixup_dlg_terminate,
        0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},
    {"dlg_terminate", (cmd_function) w_dlg_terminate, 2, fixup_dlg_terminate,
        0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},
   {"dlg_get", (cmd_function)w_dlg_get, 3, fixup_dlg_bridge, 0, ANY_ROUTE },
    {"is_known_dlg", (cmd_function) w_is_known_dlg, 0, NULL,
        0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE},
    {"load_dlg", (cmd_function) load_dlg, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0}
};

static param_export_t mod_params[] = {
    { "hash_size", INT_PARAM, &dlg_hash_size},
    { "rr_param", PARAM_STRING, &rr_param},
    { "dlg_flag", INT_PARAM, &dlg_flag},
    { "timeout_avp", PARAM_STR, &timeout_spec},
    { "default_timeout", INT_PARAM, &default_timeout},
    { "dlg_extra_hdrs", PARAM_STR, &dlg_extra_hdrs},
    //In this new dialog module we always match using DID
    //{ "dlg_match_mode", INT_PARAM, &seq_match_mode},

    { "db_url",				PARAM_STR, &db_url 				},
    { "db_mode",			INT_PARAM, &dlg_db_mode_param		},
    { "db_update_period",	INT_PARAM, &db_update_period		},
    { "db_fetch_rows",		INT_PARAM, &db_fetch_rows			}
    ,
    { "detect_spirals",		INT_PARAM, &detect_spirals			},
    { "profiles_with_value",PARAM_STRING, &profiles_wv_s			},
    { "profiles_no_value",	PARAM_STRING, &profiles_nv_s			},
    { "bridge_controller",	PARAM_STR, &dlg_bridge_controller	},
    { "ruri_pvar",			PARAM_STR, &ruri_pvar_param		},

    { 0, 0, 0}
};

static mi_export_t mi_cmds[] = {
    { "dlg_list", mi_print_dlgs, 0, 0, 0},
    { "dlg_terminate_dlg", mi_terminate_dlg, 0, 0, 0},
    { 0, 0, 0, 0, 0}
    /* TODO: restore old dialog functionality later - also expose dialoig_out cmds, possibly*/
};

static rpc_export_t rpc_methods[];

struct module_exports exports = {
    "dialog_ng", /* module's name */
    DEFAULT_DLFLAGS, /* dlopen flags */
    cmds, /* exported functions */
    mod_params, /* param exports */
    0, /* exported statistics */
    mi_cmds, /* exported MI functions */
    0, /* exported pseudo-variables */
    0, /* extra processes */
    mod_init, /* module initialization function */
    0, /* reply processing function */
    mod_destroy,
    child_init /* per-child init function */
};

static int fixup_profile(void** param, int param_no) {
    struct dlg_profile_table *profile;
    pv_elem_t *model = NULL;
    str s;

    s.s = (char*) (*param);
    s.len = strlen(s.s);
    if (s.len == 0) {
        LM_ERR("param %d is empty string!\n", param_no);
        return E_CFG;
    }

    if (param_no == 1) {
        profile = search_dlg_profile(&s);
        if (profile == NULL) {
            LM_CRIT("profile <%s> not definited\n", s.s);
            return E_CFG;
        }
        pkg_free(*param);
        *param = (void*) profile;
        return 0;
    } else if (param_no == 2) {
        if (pv_parse_format(&s, &model) || model == NULL) {
            LM_ERR("wrong format [%s] for value param!\n", s.s);
            return E_CFG;
        }
        *param = (void*) model;
    }
    return 0;
}

static int fixup_get_profile2(void** param, int param_no) {
    pv_spec_t *sp;
    int ret;

    if (param_no == 1) {
        return fixup_profile(param, 1);
    } else if (param_no == 2) {
        ret = fixup_pvar_null(param, 1);
        if (ret < 0) return ret;
        sp = (pv_spec_t*) (*param);
        if (sp->type != PVT_AVP && sp->type != PVT_SCRIPTVAR) {
            LM_ERR("return must be an AVP or SCRIPT VAR!\n");
            return E_SCRIPT;
        }
    }
    return 0;
}

static int fixup_get_profile3(void** param, int param_no) {
    if (param_no == 1) {
        return fixup_profile(param, 1);
    } else if (param_no == 2) {
        return fixup_profile(param, 2);
    } else if (param_no == 3) {
        return fixup_get_profile2(param, 2);
    }
    return 0;
}

static int fixup_dlg_terminate(void** param, int param_no) {
    char *val;
    int n = 0;

    if (param_no == 1) {
        val = (char*) *param;
        if (strcasecmp(val, "all") == 0) {
            n = 2;
        } else if (strcasecmp(val, "caller") == 0) {
            n = 0;
        } else if (strcasecmp(val, "callee") == 0) {
            n = 1;
        } else {
            LM_ERR("invalid param \"%s\"\n", val);
            return E_CFG;
        }
        pkg_free(*param);
        *param = (void*) (long) n;
    } else if (param_no == 2) {
        //fixup str
        return fixup_str_12(param, param_no);
    } else {
        LM_ERR("called with parameter != 1\n");
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
	struct dlg_cell *dlg = NULL;
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
	
	/* 
		note: we should unref the dlg here (from get_dlg). BUT, because we are setting the current dialog
		we can ignore the unref... instead of unreffing and reffing again for the set_current_dialog. NB.
		this function is generally called from the cfg file. If used via API, remember to unref the dlg
		afterwards
	*/	

	set_current_dialog(msg, dlg);
    _dlg_ctx.dlg = dlg;
    _dlg_ctx.dir = dir;
	return 1;
}

int load_dlg(struct dlg_binds *dlgb) {

    dlgb->register_dlgcb = register_dlgcb;
    dlgb->register_dlgcb_nodlg = register_dlgcb_nodlg;
    dlgb->set_dlg_var = api_set_dlg_variable;
    dlgb->get_dlg_var = api_get_dlg_variable;
    dlgb->terminate_dlg = w_api_terminate_dlg;
    dlgb->lookup_terminate_dlg = w_api_lookup_terminate_dlg;
    dlgb->get_dlg_expires = api_get_dlg_expires;
    dlgb->get_dlg = dlg_get_msg_dialog;
    dlgb->release_dlg = dlg_release;

    return 1;
}

static int mod_init(void) {
    unsigned int n;

    if (register_mi_mod(exports.name, mi_cmds) != 0) {
        LM_ERR("failed to register MI commands\n");
        return -1;
    }

    if (rpc_register_array(rpc_methods) != 0) {
        LM_ERR("failed to register RPC commands\n");
        return -1;
    }
    
    if (dialog_ng_stats_init() != 0) {
	LM_ERR("Failed to register dialog_ng counters\n");
	return -1;
    }

    if (faked_msg_init() < 0)
        return -1;

    if (timeout_spec.s)
        timeout_spec.len = strlen(timeout_spec.s);

    dlg_bridge_controller.len = strlen(dlg_bridge_controller.s);


    /* param checkings */
    if (dlg_flag == -1) {
        LM_ERR("no dlg flag set!!\n");
        return -1;
    } else if (dlg_flag > MAX_FLAG) {
        LM_ERR("invalid dlg flag %d!!\n", dlg_flag);
        return -1;
    }

    if (rr_param == 0 || rr_param[0] == 0) {
        LM_ERR("empty rr_param!!\n");
        return -1;
    } else if (strlen(rr_param) > MAX_DLG_RR_PARAM_NAME) {
        LM_ERR("rr_param too long (max=%d)!!\n", MAX_DLG_RR_PARAM_NAME);
        return -1;
    }

    if (timeout_spec.s) {
        if (pv_parse_spec(&timeout_spec, &timeout_avp) == 0
                && (timeout_avp.type != PVT_AVP)) {
            LM_ERR("malformed or non AVP timeout "
                    "AVP definition in '%.*s'\n", timeout_spec.len, timeout_spec.s);
            return -1;
        }
    }

    if (default_timeout <= 0) {
        LM_ERR("0 default_timeout not accepted!!\n");
        return -1;
    }

    if (ruri_pvar_param.s == NULL || *ruri_pvar_param.s == '\0') {
        LM_ERR("invalid r-uri PV string\n");
        return -1;
    }
    ruri_pvar_param.len = strlen(ruri_pvar_param.s);
    if (pv_parse_format(&ruri_pvar_param, &ruri_param_model) < 0
            || ruri_param_model == NULL) {
        LM_ERR("malformed r-uri PV string: %s\n", ruri_pvar_param.s);
        return -1;
    }

    /* update the len of the extra headers */
    if (dlg_extra_hdrs.s)
        dlg_extra_hdrs.len = strlen(dlg_extra_hdrs.s);

    if (seq_match_mode != SEQ_MATCH_NO_ID &&
            seq_match_mode != SEQ_MATCH_FALLBACK &&
            seq_match_mode != SEQ_MATCH_STRICT_ID) {
        LM_ERR("invalid value %d for seq_match_mode param!!\n", seq_match_mode);
        return -1;
    }

    if (detect_spirals != 0 && detect_spirals != 1) {
        LM_ERR("invalid value %d for detect_spirals param!!\n", detect_spirals);
        return -1;
    }

    /* create profile hashes */
    if (add_profile_definitions(profiles_nv_s, 0) != 0) {
        LM_ERR("failed to add profiles without value\n");
        return -1;
    }
    if (add_profile_definitions(profiles_wv_s, 1) != 0) {
        LM_ERR("failed to add profiles with value\n");
        return -1;
    }

    /* load the TM API */
    if (load_tm_api(&d_tmb) != 0) {
        LM_ERR("can't load TM API\n");
        return -1;
    }

    /* load RR API also */
    if (load_rr_api(&d_rrb) != 0) {
        LM_ERR("can't load RR API\n");
        return -1;
    }

    /* register callbacks*/
    /* listen for all incoming requests  */
    if (d_tmb.register_tmcb(0, 0, TMCB_REQUEST_IN, dlg_onreq, 0, 0) <= 0) {
        LM_ERR("cannot register TMCB_REQUEST_IN callback\n");
        return -1;
    }

    /* listen for all routed requests  */
    if (d_rrb.register_rrcb(dlg_onroute, 0) < 0) {
        LM_ERR("cannot register RR callback\n");
        return -1;
    }

    if (register_script_cb(profile_cleanup, POST_SCRIPT_CB | REQUEST_CB, 0) < 0) {
        LM_ERR("cannot regsiter script callback");
        return -1;
    }
    if (register_script_cb(dlg_cfg_cb,
            PRE_SCRIPT_CB | REQUEST_CB, 0) < 0) {
        LM_ERR("cannot regsiter pre-script ctx callback\n");
        return -1;
    }
    if (register_script_cb(dlg_cfg_cb,
            POST_SCRIPT_CB | REQUEST_CB, 0) < 0) {
        LM_ERR("cannot regsiter post-script ctx callback\n");
        return -1;
    }

    if (register_script_cb(spiral_detect_reset, POST_SCRIPT_CB | REQUEST_CB, 0) < 0) {
        LM_ERR("cannot register req pre-script spiral detection reset callback\n");
        return -1;
    }

    if (register_timer(dlg_timer_routine, 0, 1) < 0) {
        LM_ERR("failed to register timer \n");
        return -1;
    }

    /*for testing only!!!! setup timer to call print all dlg every 10 seconds!*/
    if (register_timer(print_all_dlgs, 0, 10) < 0) {
        LM_ERR("failed to register timer \n");
        return -1;
    }

    /* init handlers */
    init_dlg_handlers(rr_param, dlg_flag,
            timeout_spec.s ? &timeout_avp : 0, default_timeout, seq_match_mode);

    /* init timer */
    if (init_dlg_timer(dlg_ontimeout) != 0) {
        LM_ERR("cannot init timer list\n");
        return -1;
    }

    /* sanitize dlg_hash_zie */
    if (dlg_hash_size < 1) {
        LM_WARN("hash_size is smaller "
                "then 1  -> rounding from %d to 1\n",
                dlg_hash_size);
        dlg_hash_size = 1;
    }

    /* initialized the hash table */
    for (n = 0; n < (8 * sizeof (n)); n++) {
        if (dlg_hash_size == (1 << n))
            break;
        if (n && dlg_hash_size < (1 << n)) {
            LM_WARN("hash_size is not a power "
                    "of 2 as it should be -> rounding from %d to %d\n",
                    dlg_hash_size, 1 << (n - 1));
            dlg_hash_size = 1 << (n - 1);
        }
    }

    if (init_dlg_table(dlg_hash_size) < 0) {
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
		if (init_dlg_db(&db_url, dlg_hash_size, db_update_period, db_fetch_rows)!=0) {
			LM_ERR("failed to initialize the DB support\n");
			return -1;
		}
		run_load_callbacks();
	}

    destroy_dlg_callbacks(DLGCB_LOADED);

    return 0;
}

static int child_init(int rank) {
	dlg_db_mode = dlg_db_mode_param;

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

static void mod_destroy(void) {
	if(dlg_db_mode == DB_MODE_DELAYED || dlg_db_mode == DB_MODE_SHUTDOWN) {
		dialog_update_db(0, 0);
		destroy_dlg_db();
	}

    destroy_dlg_table();
    destroy_dlg_timer();
    destroy_dlg_callbacks(DLGCB_CREATED | DLGCB_LOADED);
    destroy_dlg_handlers();
    destroy_dlg_profiles();
    dialog_ng_stats_destroy();
}

static int w_set_dlg_profile(struct sip_msg *msg, char *profile, char *value) {
    pv_elem_t *pve;
    str val_s;

    pve = (pv_elem_t *) value;

    if (((struct dlg_profile_table*) profile)->has_value) {
        if (pve == NULL || pv_printf_s(msg, pve, &val_s) != 0 ||
                val_s.len == 0 || val_s.s == NULL) {
            LM_WARN("cannot get string for value\n");
            return -1;
        }
        if (set_dlg_profile(msg, &val_s,
                (struct dlg_profile_table*) profile) < 0) {
            LM_ERR("failed to set profile");
            return -1;
        }
    } else {
        if (set_dlg_profile(msg, NULL,
                (struct dlg_profile_table*) profile) < 0) {
            LM_ERR("failed to set profile");
            return -1;
        }
    }
    return 1;
}

static int w_unset_dlg_profile(struct sip_msg *msg, char *profile, char *value) {
    pv_elem_t *pve;
    str val_s;

    pve = (pv_elem_t *) value;

    if (((struct dlg_profile_table*) profile)->has_value) {
        if (pve == NULL || pv_printf_s(msg, pve, &val_s) != 0 ||
                val_s.len == 0 || val_s.s == NULL) {
            LM_WARN("cannot get string for value\n");
            return -1;
        }
        if (unset_dlg_profile(msg, &val_s,
                (struct dlg_profile_table*) profile) < 0) {
            LM_ERR("failed to unset profile");
            return -1;
        }
    } else {
        if (unset_dlg_profile(msg, NULL,
                (struct dlg_profile_table*) profile) < 0) {
            LM_ERR("failed to unset profile");
            return -1;
        }
    }
    return 1;
}

static int w_is_in_profile(struct sip_msg *msg, char *profile, char *value) {
    pv_elem_t *pve;
    str val_s;

    pve = (pv_elem_t *) value;

    if (pve != NULL && ((struct dlg_profile_table*) profile)->has_value) {
        if (pv_printf_s(msg, pve, &val_s) != 0 ||
                val_s.len == 0 || val_s.s == NULL) {
            LM_WARN("cannot get string for value\n");
            return -1;
        }
        return is_dlg_in_profile(msg, (struct dlg_profile_table*) profile,
                &val_s);
    } else {
        return is_dlg_in_profile(msg, (struct dlg_profile_table*) profile,
                NULL);
    }
}

/**
 * get dynamic name profile size
 */
static int w_get_profile_size3(struct sip_msg *msg, char *profile,
        char *value, char *result) {
    pv_elem_t *pve;
    str val_s;
    pv_spec_t *sp_dest;
    unsigned int size;
    pv_value_t val;

    if (result != NULL) {
        pve = (pv_elem_t *) value;
        sp_dest = (pv_spec_t *) result;
    } else {
        pve = NULL;
        sp_dest = (pv_spec_t *) value;
    }
    if (pve != NULL && ((struct dlg_profile_table*) profile)->has_value) {
        if (pv_printf_s(msg, pve, &val_s) != 0 ||
                val_s.len == 0 || val_s.s == NULL) {
            LM_WARN("cannot get string for value\n");
            return -1;
        }
        size = get_profile_size((struct dlg_profile_table*) profile, &val_s);
    } else {
        size = get_profile_size((struct dlg_profile_table*) profile, NULL);
    }

    memset(&val, 0, sizeof (pv_value_t));
    val.flags = PV_VAL_INT | PV_TYPE_INT;
    val.ri = (int) size;

    if (sp_dest->setf(msg, &sp_dest->pvp, (int) EQ_T, &val) < 0) {
        LM_ERR("setting profile PV failed\n");
        return -1;
    }

    return 1;
}

/**
 * get static name profile size
 */
static int w_get_profile_size2(struct sip_msg *msg, char *profile, char *result) {
    return w_get_profile_size3(msg, profile, result, NULL);
}

static int w_dlg_setflag(struct sip_msg *msg, char *flag, char *s2) {
    dlg_ctx_t *dctx;
    int val;

    if (fixup_get_ivalue(msg, (gparam_p) flag, &val) != 0) {
        LM_ERR("no flag value\n");
        return -1;
    }
    if (val < 0 || val > 31)
        return -1;
    if ((dctx = dlg_get_dlg_ctx()) == NULL)
        return -1;

    dctx->flags |= 1 << val;
    if (dctx->dlg)
        dctx->dlg->sflags |= 1 << val;
    return 1;
}

static int w_dlg_resetflag(struct sip_msg *msg, char *flag, str *s2) {
    dlg_ctx_t *dctx;
    int val;

    if (fixup_get_ivalue(msg, (gparam_p) flag, &val) != 0) {
        LM_ERR("no flag value\n");
        return -1;
    }
    if (val < 0 || val > 31)
        return -1;

    if ((dctx = dlg_get_dlg_ctx()) == NULL)
        return -1;

    dctx->flags &= ~(1 << val);
    if (dctx->dlg)
        dctx->dlg->sflags &= ~(1 << val);
    return 1;
}

static int w_dlg_isflagset(struct sip_msg *msg, char *flag, str *s2) {
    dlg_ctx_t *dctx;
    int val;

    if (fixup_get_ivalue(msg, (gparam_p) flag, &val) != 0) {
        LM_ERR("no flag value\n");
        return -1;
    }
    if (val < 0 || val > 31)
        return -1;

    if ((dctx = dlg_get_dlg_ctx()) == NULL)
        return -1;

    if (dctx->dlg)
        return (dctx->dlg->sflags & (1 << val)) ? 1 : -1;
    return (dctx->flags & (1 << val)) ? 1 : -1;
}

static int w_dlg_terminate(struct sip_msg *msg, char *side, char *r) {
    struct dlg_cell *dlg;
    str reason = {0, 0};

    int n = (int) (long) side;

    //check if a reason was given
    if (r) {
        if (get_str_fparam(&reason, msg, (fparam_t *) r) < 0) {
            LM_ERR("failed to recover reason parameter\n");
            return -1;
        }
    }

    dlg = get_current_dialog(msg);
    //dlg_get_ctx_dialog();
    if (!dlg) {
        LM_DBG("Unable to find dialog for terminate\n");
        return -1;
    }

    if (!dlg_terminate(dlg, msg, &reason, n, NULL)) {
        LM_DBG("Failed to terminate dialog\n");
        return -1;
    }

    return 1;
}

/*
 * Wrapper around is_known_dlg().
 */

static int w_is_known_dlg(struct sip_msg *msg) {
    return is_known_dlg(msg);
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
static inline void internal_rpc_print_dlg(rpc_t *rpc, void *c, struct dlg_cell *dlg, void *dh)
{
	void* dlg_outs_h;
	struct dlg_cell_out* dlg_out;

	rpc->struct_add(dh, "dd", "Entry", dlg->h_entry, "Id", dlg->h_id);
	rpc->struct_add(dh, "SSSSSSSsd{",
			"RURI", &dlg->req_uri,
			"From", &dlg->from_uri,
			"Call-ID", &dlg->callid,
			"Caller Contact", &dlg->caller_contact,
			"Caller Route Set", &dlg->caller_route_set,
			"Dialog-ID", &dlg->did,
			"From Tag", &dlg->from_tag,
			"State", state_to_char(dlg->state),
			"Ref", dlg->ref,
			"dlg_outs", &dlg_outs_h
			);

	lock_get(dlg->dlg_out_entries_lock);

	dlg_out = dlg->dlg_entry_out.first;
	while (dlg_out) {
		rpc->struct_add(dlg_outs_h, "dd", "Entry", dlg_out->h_entry, "Id", dlg_out->h_id);
		dlg_out = dlg_out->next;
	}

	lock_release(dlg->dlg_out_entries_lock);

	/*now traverse dlg_outs*/

}

/*!
 * \brief Helper function that outputs all dialogs via the RPC interface
 * \see rpc_print_dlgs
 * \param rpc RPC node that should be filled
 * \param c RPC void pointer
 * \param with_context if 1 then the dialog context will be also printed
 */
static void internal_rpc_print_dlgs(rpc_t *rpc, void *c)
{
	struct dlg_cell *dlg;
	unsigned int i;

	void *ah;
	void *dh;		/*beginning struct holding dialogs*/

	if (rpc->add(c, "{", &ah) < 0) {
		rpc->fault(c, 500, "Internal error creating top rpc");
		return;
	}
	if (rpc->struct_add(ah, "d{", "Size", (int) d_table->size, "Dialogs", &dh) < 0) {
		rpc->fault(c, 500, "Internal error creating inner struct");
		return;
	}

	for( i=0 ; i<d_table->size ; i++ ) {
		dlg_lock( d_table, &(d_table->entries[i]) );

		for( dlg=d_table->entries[i].first ; dlg ; dlg=dlg->next ) {
			internal_rpc_print_dlg(rpc, c, dlg, dh);
		}
		dlg_unlock( d_table, &(d_table->entries[i]) );
	}
}

static const char *rpc_print_dlgs_doc[2] = {
	"Print all dialogs", 0
};

static void rpc_print_dlgs(rpc_t *rpc, void *c) {
	internal_rpc_print_dlgs(rpc, c);
}

/*static const char *rpc_end_dlg_entry_id_doc[2] = {
    "End a given dialog based on [h_entry] [h_id]", 0
};


static void rpc_end_dlg_entry_id(rpc_t *rpc, void *c) {
    unsigned int h_entry, h_id;
    struct dlg_cell * dlg = NULL;
    str rpc_extra_hdrs = {NULL, 0};

    if (rpc->scan(c, "ddS", &h_entry, &h_id, &rpc_extra_hdrs) < 2) return;

    dlg = lookup_dlg(h_entry, h_id);
    if (dlg) {
        //dlg_bye_all(dlg, (rpc_extra_hdrs.len>0)?&rpc_extra_hdrs:NULL);
    unref_dlg(dlg, 1);
}
}*/

static const char *rpc_end_dlg_entry_id_doc[2] = {
    "End a given dialog based on [h_entry] [h_id]", 0
};





/* Wrapper for terminating dialog from API - from other modules */
static void rpc_end_dlg_entry_id(rpc_t *rpc, void *c) {
    unsigned int h_entry, h_id;
    struct dlg_cell * dlg = NULL;
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

    dlg = lookup_dlg(h_entry, h_id);//increments ref count!
    if(dlg==NULL) {
	    rpc->fault(c, 404, "Dialog not found");
	    return;
    }

    unref_dlg(dlg, 1);

    dlg_terminate(dlg, NULL, NULL/*reason*/, 2, NULL);

}


static rpc_export_t rpc_methods[] = {
	{"dlg2.list", rpc_print_dlgs, rpc_print_dlgs_doc, 0},
        {"dlg2.end_dlg", rpc_end_dlg_entry_id, rpc_end_dlg_entry_id_doc, 0},
    {0, 0, 0, 0}
};


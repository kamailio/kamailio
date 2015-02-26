/*
 * mod.c
 *
 *  Created on: 21 Feb 2013
 *      Author: jaybeepee
 */

#include "mod.h"
#include "../../sr_module.h"
#include "../../modules/dialog_ng/dlg_load.h"
#include "../../modules/dialog_ng/dlg_hash.h"
#include "../cdp/cdp_load.h"
#include "../cdp_avp/mod_export.h"
#include "../../parser/parse_to.h"
#include "ro_timer.h"
#include "ro_session_hash.h"
#include "ims_ro.h"
#include "config.h"
#include "dialog.h"
#include "../ims_usrloc_scscf/usrloc.h"
#include "../../lib/ims/ims_getters.h"
#include "ro_db_handler.h"
#include "ims_charging_stats.h"

MODULE_VERSION

/* parameters */
char* ro_destination_host_s = "hss.ims.smilecoms.com";
char* ro_service_context_id_root_s = "32260@3gpp.org";
char* ro_service_context_id_ext_s = "ext";
char* ro_service_context_id_mnc_s = "01";
char* ro_service_context_id_mcc_s = "001";
char* ro_service_context_id_release_s = "8";
static int ro_session_hash_size = 4096;
int ro_timer_buffer = 5;
int interim_request_credits = 30;

int voice_service_identifier = 1000;
int voice_rating_group = 100;

int video_service_identifier = 1001;
int video_rating_group = 200;


/* DB params */
static str db_url = str_init(DEFAULT_DB_URL);
static unsigned int db_update_period = DB_DEFAULT_UPDATE_PERIOD;
int ro_db_mode_param = DB_MODE_NONE;
static int db_fetch_rows = 200;
int ro_db_mode = DB_MODE_NONE;

char *domain = "location";

client_ro_cfg cfg = { str_init("scscf.ims.smilecoms.com"),
    str_init("ims.smilecoms.com"),
    str_init("ims.smilecoms.com"),
    0
};

struct cdp_binds cdpb;
struct dlg_binds dlgb;
cdp_avp_bind_t *cdp_avp;
struct tm_binds tmb;

usrloc_api_t ul; /*!< Structure containing pointers to usrloc functions*/

char* rx_dest_realm_s = "ims.smilecoms.com";
str rx_dest_realm;
/* Only used if we want to force the Ro peer usually this is configured at a stack level and the first request uses realm routing */
//char* rx_forced_peer_s = "";
str ro_forced_peer;
int ro_auth_expiry = 7200;
int cdp_event_latency = 1; /*flag: report slow processing of CDP callback events or not - default enabled */
int cdp_event_threshold = 500; /*time in ms above which we should report slow processing of CDP callback event - default 500ms*/
int cdp_event_latency_loglevel = 0; /*log-level to use to report slow processing of CDP callback event - default ERROR*/
int single_ro_session_per_dialog = 0; /*whether to to have 1 ro_session per dialog or let user decide from config - default is an ro session every time Ro_CCR called from config file*/

/** module functions */
static int mod_init(void);
static int mod_child_init(int);
static void mod_destroy(void);

static int w_ro_ccr(struct sip_msg *msg, char* route_name, char* direction, int reservation_units, char* incoming_trunk_id, char* outgoing_trunk_id);
//void ro_session_ontimeout(struct ro_tl *tl);

static int ro_fixup(void **param, int param_no);

static cmd_export_t cmds[] = {
		{ "Ro_CCR", 	(cmd_function) w_ro_ccr, 5, ro_fixup, 0, REQUEST_ROUTE },
		{ 0, 0, 0, 0, 0, 0 }
};

static param_export_t params[] = {
		{ "hash_size", 				INT_PARAM,			&ro_session_hash_size 		},
		{ "interim_update_credits",	INT_PARAM,			&interim_request_credits 	},
		{ "timer_buffer", 			INT_PARAM,			&ro_timer_buffer 			},
		{ "ro_forced_peer", 		PARAM_STR, 			&ro_forced_peer 			},
		{ "ro_auth_expiry",			INT_PARAM, 			&ro_auth_expiry 			},
		{ "cdp_event_latency", 		INT_PARAM,			&cdp_event_latency 			}, /*flag: report slow processing of CDP
																						callback events or not */
		{ "cdp_event_threshold", 	INT_PARAM, 			&cdp_event_threshold 		}, /*time in ms above which we should
																						report slow processing of CDP callback event*/
		{ "cdp_event_latency_log", 	INT_PARAM, 			&cdp_event_latency_loglevel },/*log-level to use to report
																						slow processing of CDP callback event*/
		{ "single_ro_session_per_dialog", 	INT_PARAM, 			&single_ro_session_per_dialog },
		{ "origin_host", 			PARAM_STR, 			&cfg.origin_host 			},
		{ "origin_realm", 			PARAM_STR,			&cfg.origin_realm 			},
		{ "destination_realm", 		PARAM_STR,			&cfg.destination_realm 	},
		{ "destination_host", 		PARAM_STRING,			&ro_destination_host_s 		}, /* Unused parameter? */
		{ "service_context_id_root",PARAM_STRING,			&ro_service_context_id_root_s 	},
		{ "service_context_id_ext", PARAM_STRING,			&ro_service_context_id_ext_s 	},
		{ "service_context_id_mnc", PARAM_STRING,			&ro_service_context_id_mnc_s 	},
		{ "service_context_id_mcc", PARAM_STRING,			&ro_service_context_id_mcc_s 	},
		{ "service_context_id_release",	PARAM_STRING,			&ro_service_context_id_release_s},
		{ "voice_service_identifier", 	INT_PARAM, 			&voice_service_identifier },/*service id for voice*/
		{ "voice_rating_group", 	INT_PARAM, 			&voice_rating_group },/*rating group for voice*/
		{ "video_service_identifier", 	INT_PARAM, 			&video_service_identifier },/*service id for voice*/
		{ "video_rating_group", 	INT_PARAM, 			&video_rating_group },/*rating group for voice*/
		{ "db_mode",			INT_PARAM,			&ro_db_mode_param		},
		{ "db_url",			PARAM_STRING,			&db_url 			},
		{ "db_update_period",		INT_PARAM,			&db_update_period		},
		{ 0, 0, 0 }
};

/** module exports */
struct module_exports exports = { MOD_NAME, DEFAULT_DLFLAGS, /* dlopen flags */
		cmds, 		/* Exported functions */
		params, 	/* Exported params */
		0,		/* exported statistics */
		0, 			/* exported MI functions */
		0, 			/* exported pseudo-variables */
		0, 			/* extra processes */
		mod_init, 	/* module initialization function */
		0,
		mod_destroy, 	/* module destroy functoin */
		mod_child_init 	/* per-child init function */
};

int fix_parameters() {
	cfg.service_context_id = shm_malloc(sizeof(str));
	if (!cfg.service_context_id) {
		LM_ERR("fix_parameters:not enough shm memory\n");
		return 0;
	}
	cfg.service_context_id->len = strlen(ro_service_context_id_ext_s)
			+ strlen(ro_service_context_id_mnc_s)
			+ strlen(ro_service_context_id_mcc_s)
			+ strlen(ro_service_context_id_release_s)
			+ strlen(ro_service_context_id_root_s) + 5;
	cfg.service_context_id->s =
			pkg_malloc(cfg.service_context_id->len * sizeof (char));
	if (!cfg.service_context_id->s) {
		LM_ERR("fix_parameters: not enough memory!\n");
		return 0;
	}
	cfg.service_context_id->len = sprintf(cfg.service_context_id->s,
			"%s.%s.%s.%s.%s", ro_service_context_id_ext_s,
			ro_service_context_id_mnc_s, ro_service_context_id_mcc_s,
			ro_service_context_id_release_s, ro_service_context_id_root_s);
	if (cfg.service_context_id->len < 0) {
		LM_ERR("fix_parameters: error while creating service_context_id\n");
		return 0;
	}

	return 1;
}

static int mod_init(void) {
	int n;
	load_dlg_f load_dlg;
	load_tm_f load_tm;
	bind_usrloc_t bind_usrloc;

	if (!fix_parameters()) {
		LM_ERR("unable to set Ro configuration parameters correctly\n");
		goto error;
	}

	/* bind to the tm module */
	if (!(load_tm = (load_tm_f) find_export("load_tm", NO_SCRIPT, 0))) {
		LM_ERR("Can not import load_tm. This module requires tm module\n");
		goto error;
	}
	if (load_tm(&tmb) == -1)
		goto error;

	if (!(load_dlg = (load_dlg_f) find_export("load_dlg", 0, 0))) { /* bind to dialog module */
		LM_ERR("can not import load_dlg. This module requires Kamailio dialog module.\n");
	}
	if (load_dlg(&dlgb) == -1) {
		goto error;
	}

	if (load_cdp_api(&cdpb) != 0) { /* load the CDP API */
		LM_ERR("can't load CDP API\n");
		goto error;
	}

	if (load_dlg_api(&dlgb) != 0) { /* load the dialog API */
		LM_ERR("can't load Dialog API\n");
		goto error;
	}

	cdp_avp = load_cdp_avp(); /* load CDP_AVP API */
	if (!cdp_avp) {
		LM_ERR("can't load CDP_AVP API\n");
		goto error;
	}

	/* init timer lists*/
	if (init_ro_timer(ro_session_ontimeout) != 0) {
		LM_ERR("cannot init timer list\n");
		return -1;
	}

	/* initialized the hash table */
	for (n = 0; n < (8 * sizeof(n)); n++) {
		if (ro_session_hash_size == (1 << n))
			break;
		if (ro_session_hash_size < (1 << n)) {
			LM_WARN("hash_size is not a power of 2 as it should be -> rounding from %d to %d\n", ro_session_hash_size, 1 << (n - 1));
			ro_session_hash_size = 1 << (n - 1);
		}
	}

	if (init_ro_session_table(ro_session_hash_size) < 0) {
		LM_ERR("failed to create ro session hash table\n");
		return -1;
	}

	/* register global timer */
	if (register_timer(ro_timer_routine, 0/*(void*)ro_session_list*/, 1) < 0) {
		LM_ERR("failed to register timer \n");
		return -1;
	}

	bind_usrloc = (bind_usrloc_t) find_export("ul_bind_usrloc", 1, 0);
	if (!bind_usrloc) {
	    LM_ERR("can't bind usrloc\n");
	    return -1;
	}
	
	if (bind_usrloc(&ul) < 0) {
	    return -1;
	}

	/*Register for callback of URECORD being deleted - so we can send a SAR*/

	if (ul.register_ulcb == NULL) {
	    LM_ERR("Could not import ul_register_ulcb\n");
	    return -1;
	}
	
	if (ims_charging_init_counters() != 0) {
	    LM_ERR("Failed to register counters for ims_charging module\n");
	    return -1;
	}
	
	/* if a database should be used to store the dialogs' information */
	ro_db_mode = ro_db_mode_param;
	if (ro_db_mode == DB_MODE_NONE) {
	    db_url.s = 0;
	    db_url.len = 0;
	} else {
	    if (ro_db_mode != DB_MODE_REALTIME && ro_db_mode != DB_MODE_SHUTDOWN) {
		LM_ERR("unsupported db_mode %d\n", ro_db_mode);
		return -1;
	    }
	    if (!db_url.s || db_url.len == 0) {
		LM_ERR("db_url not configured for db_mode %d\n", ro_db_mode);
		return -1;
	    }
	    if (init_ro_db(&db_url, ro_session_hash_size, db_update_period, db_fetch_rows) != 0) {
		LM_ERR("failed to initialize the DB support\n");
		return -1;
	    }
//	    run_load_callbacks();
	}

	return 0;

error:
	LM_ERR("Failed to initialise ims_charging module\n");
	return RO_RETURN_FALSE;

}

static int mod_child_init(int rank) {
    ro_db_mode = ro_db_mode_param;

    if (((ro_db_mode == DB_MODE_REALTIME) && (rank > 0 || rank == PROC_TIMER)) ||
	    (ro_db_mode == DB_MODE_SHUTDOWN && (rank == PROC_MAIN))) {
	if (ro_connect_db(&db_url)) {
	    LM_ERR("failed to connect to database (rank=%d)\n", rank);
	    return -1;
	}
    }

    /* in DB_MODE_SHUTDOWN only PROC_MAIN will do a DB dump at the end, so
     * for the rest of the processes will be the same as DB_MODE_NONE */
    if (ro_db_mode == DB_MODE_SHUTDOWN && rank != PROC_MAIN)
	ro_db_mode = DB_MODE_NONE;
    /* in DB_MODE_REALTIME and DB_MODE_DELAYED the PROC_MAIN have no DB handle */
    if ((ro_db_mode == DB_MODE_REALTIME) && rank == PROC_MAIN)
	ro_db_mode = DB_MODE_NONE;
    
    return 0;
}

static void mod_destroy(void) {

}

static int w_ro_ccr(struct sip_msg *msg, char* c_route_name, char* c_direction, int reservation_units, char* c_incoming_trunk_id, char* c_outgoing_trunk_id) {
	/* PSEUDOCODE/NOTES
	 * 1. What mode are we in - terminating or originating
	 * 2. We assume this is SCUR in this module for now - can see event based charging in another component instead (AS for SMS for example, etc)
	 * 4. Check a dialog exists for call, if not we fail
	 * 5. make sure we dont already have an Ro Session for this dialog
	 * 6. create new Ro Session
	 * 7. register for DLG callback passing new Ro session as parameter - (if dlg torn down we know which Ro session it is associated with)
	 *
	 *
	 */
	int ret = RO_RETURN_TRUE;
	int dir = 0;
	str identity = {0, 0},
	pani = {0,0},
	contact = {0, 0};
	struct hdr_field *h=0;
	
	cfg_action_t* cfg_action;
	tm_cell_t *t;
	unsigned int tindex = 0,
				 tlabel = 0;
	struct impu_data *impu_data;
	char *p;
	struct dlg_cell* dlg;
	unsigned int len;
	struct ro_session *ro_session = 0;
	int free_contact = 0;
	
	str s_route_name, s_direction, s_incoming_trunk_id, s_outgoing_trunk_id;
	
	if (get_str_fparam(&s_route_name, msg, (fparam_t*) c_route_name) < 0) {
	    LM_ERR("failed to get s_route_name\n");
	    return RO_RETURN_ERROR;
	}
	if (get_str_fparam(&s_direction, msg, (fparam_t*) c_direction) < 0) {
	    LM_ERR("failed to get s_direction\n");
	    return RO_RETURN_ERROR;
	}
	if (get_str_fparam(&s_incoming_trunk_id, msg, (fparam_t*) c_incoming_trunk_id) < 0) {
	    LM_ERR("failed to get s_incoming_trunk_id\n");
	    return RO_RETURN_ERROR;
	}
	if (get_str_fparam(&s_outgoing_trunk_id, msg, (fparam_t*) c_outgoing_trunk_id) < 0) {
	    LM_ERR("failed to get s_outgoing_trunk_id\n");
	    return RO_RETURN_ERROR;
	}
	
	LM_DBG("Ro CCR initiated: direction:%.*s, reservation_units:%i, route_name:%.*s, incoming_trunk_id:%.*s outgoing_trunk_id:%.*s\n",
			s_direction.len, s_direction.s,
			reservation_units,
			s_route_name.len, s_route_name.s,
			s_incoming_trunk_id.len, s_incoming_trunk_id.s,
			s_outgoing_trunk_id.len, s_outgoing_trunk_id.s);
	

	if (msg->first_line.type != SIP_REQUEST) {
	    LM_ERR("Ro_CCR() called from SIP reply.");
	    return RO_RETURN_ERROR;;
	}
	
	//make sure we can get the dialog! if not, we can't continue
	
	dlg = dlgb.get_dlg(msg);
	if (!dlg) {
		LM_ERR("Unable to find dialog and cannot do Ro charging without it\n");
		return RO_RETURN_ERROR;
	}
	
	dir = get_direction_as_int(&s_direction);
	
	if (dir == RO_ORIG_DIRECTION) {
		//get caller IMPU from asserted identity
		if ((identity = cscf_get_asserted_identity(msg, 0)).len == 0) {
			LM_DBG("No P-Asserted-Identity hdr found. Using From hdr for asserted_identity");
			identity = dlg->from_uri;
		}
		//get caller contact from contact header - if not present then skip this
		if ((contact = cscf_get_contact(msg)).len == 0) {
		    LM_WARN("Can not get contact from message - will not get callbacks if this IMPU is removed to terminate call");
			goto send_ccr;
		}
		
		pani = cscf_get_access_network_info(msg, &h);
	} else if (dir == RO_TERM_DIRECTION){
		//get callee IMPU from called part id - if not present then skip this
		if ((identity = cscf_get_public_identity_from_called_party_id(msg, &h)).len == 0) {
			LM_WARN("No P-Called-Identity hdr found - will not get callbacks if this IMPU is removed to terminate call");
			goto send_ccr;
		}
		//get callee contact from request URI
		contact = cscf_get_contact_from_requri(msg);
		free_contact = 1;
	    
	} else {
	    LM_CRIT("don't know what to do in unknown mode - should we even get here\n");
	    ret = RO_RETURN_ERROR;
	    goto done;
	}
	
	LM_DBG("IMPU data to pass to usrloc:  contact <%.*s> identity <%.*s>\n", contact.len, contact.s, identity.len, identity.s);
	
	//create impu_data_parcel
	len = identity.len + contact.len +  sizeof (struct impu_data);
	impu_data = (struct impu_data*) shm_malloc(len);
	if (!impu_data) {
	    LM_ERR("Unable to allocate memory for impu_data, trying to send CCR\n");
	    ret = RO_RETURN_ERROR;
	    goto done;
	}
	memset(impu_data, 0, len);
	
	p = (char*) (impu_data + 1);
	impu_data->identity.s = p;
	impu_data->identity.len = identity.len;
	memcpy(p, identity.s, identity.len);
	p += identity.len;

	impu_data->contact.s = p;
	impu_data->contact.len = contact.len;
	memcpy(p, contact.s, contact.len);
	p += contact.len;
	
	if (p != (((char*) impu_data) + len)) {
	    LM_ERR("buffer overflow creating impu data, trying to send CCR\n");
	    shm_free(impu_data);
	    ret = RO_RETURN_ERROR;
	    goto done;
	}
	
	
	//reg for callbacks on confirmed and terminated
	if (dlgb.register_dlgcb(dlg, /* DLGCB_RESPONSE_FWDED */ DLGCB_CONFIRMED, add_dlg_data_to_contact, (void*)impu_data ,NULL ) != 0) {
	    LM_CRIT("cannot register callback for dialog confirmation\n");
	    ret = RO_RETURN_ERROR;
	    goto done;
	}

	if (dlgb.register_dlgcb(dlg, DLGCB_TERMINATED | DLGCB_FAILED | DLGCB_EXPIRED /*| DLGCB_DESTROY */, remove_dlg_data_from_contact, (void*)impu_data, NULL ) != 0) {
	    LM_CRIT("cannot register callback for dialog termination\n");
	    ret = RO_RETURN_ERROR;
	    goto done;
	}
	
send_ccr:

	//check if we need to send_ccr - 
	//we get the ro_session based on dlg->h_id and dlg->h_entry and direction 0 (so get any ro_session)
	//if it already exists then we go to done
	if (single_ro_session_per_dialog && (ro_session = lookup_ro_session(dlg->h_entry, &dlg->callid, 0, 0))) {
	    LM_DBG("single_ro_session_per_dialog = 1 and ro_session already exists for this dialog -so we don't need to send another one\n");
	    unref_ro_session(ro_session,1);//for the lookup ro session ref
	    goto done;
	}
	
	LM_DBG("Looking for route block [%.*s]\n", s_route_name.len, s_route_name.s);

	int ri = route_get(&main_rt, s_route_name.s);
	if (ri < 0) {
		LM_ERR("unable to find route block [%.*s]\n", s_route_name.len, s_route_name.s);
		ret = RO_RETURN_ERROR;
		goto done;
	}
	
	cfg_action = main_rt.rlist[ri];
	if (!cfg_action) {
		LM_ERR("empty action lists in route block [%.*s]\n", s_route_name.len, s_route_name.s);
		ret = RO_RETURN_ERROR;
		goto done;
	}

	//before we send lets suspend the transaction
	t = tmb.t_gett();
	if (t == NULL || t == T_UNDEFINED) {
		if (tmb.t_newtran(msg) < 0) {
			LM_ERR("cannot create the transaction for CCR async\n");
			ret = RO_RETURN_ERROR;
			goto done;
		}
		t = tmb.t_gett();
		if (t == NULL || t == T_UNDEFINED) {
			LM_ERR("cannot lookup the transaction\n");
			ret = RO_RETURN_ERROR;
			goto done;
		}
	}

	LM_DBG("Suspending SIP TM transaction\n");
	if (tmb.t_suspend(msg, &tindex, &tlabel) < 0) {
		LM_ERR("failed to suspend the TM processing\n");
		ret =  RO_RETURN_ERROR;
		goto done;
	}
	
	ret = Ro_Send_CCR(msg, dlg, dir, reservation_units, &s_incoming_trunk_id, &s_outgoing_trunk_id, &pani, cfg_action, tindex, tlabel);
	
	if(ret < 0){
	    LM_ERR("Failed to send CCR\n");
		tmb.t_cancel_suspend(tindex, tlabel);
	}
    
done:
	if(free_contact)  shm_free(contact.s);// shm_malloc in cscf_get_public_identity_from_requri	
	return ret;
}

static int ro_fixup(void **param, int param_no) {
	str s;
	unsigned int num;

	if ( (param_no > 0 && param_no <= 2) || (param_no >= 4 && param_no <= 6)) {
		return fixup_var_str_12(param, param_no);
	} else if (param_no == 3) {
		/*convert to int */
		s.s = (char*)*param;
		s.len = strlen(s.s);
		if (str2int(&s, &num)==0) {
			pkg_free(*param);
			*param = (void*)(unsigned long)num;
			return 0;
		}
		LM_ERR("Bad reservation units: <%s>n", (char*)(*param));
		return E_CFG;
	}
	
	return 0;
}

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

MODULE_VERSION

/* parameters */
char* ro_origin_host_s = "scscf.ims.smilecoms.com";
char* ro_origin_realm_s = "ims.smilecoms.com";
char* ro_destination_realm_s = "ims.smilecoms.com";
char* ro_destination_host_s = "hss.ims.smilecoms.com";
char* ro_service_context_id_root_s = "32260@3gpp.org";
char* ro_service_context_id_ext_s = "ext";
char* ro_service_context_id_mnc_s = "01";
char* ro_service_context_id_mcc_s = "001";
char* ro_service_context_id_release_s = "8";
static int ro_session_hash_size = 4096;
int ro_timer_buffer = 5;
int interim_request_credits = 30;
client_ro_cfg cfg;

struct cdp_binds cdpb;
struct dlg_binds dlgb;
cdp_avp_bind_t *cdp_avp;
struct tm_binds tmb;

char* rx_dest_realm_s = "ims.smilecoms.com";
str rx_dest_realm;
/* Only used if we want to force the Ro peer usually this is configured at a stack level and the first request uses realm routing */
//char* rx_forced_peer_s = "";
str ro_forced_peer;
int ro_auth_expiry = 7200;
int cdp_event_latency = 1; /*flag: report slow processing of CDP callback events or not - default enabled */
int cdp_event_threshold = 500; /*time in ms above which we should report slow processing of CDP callback event - default 500ms*/
int cdp_event_latency_loglevel = 0; /*log-level to use to report slow processing of CDP callback event - default ERROR*/

/** module functions */
static int mod_init(void);
static int mod_child_init(int);
static void mod_destroy(void);
static int w_ro_ccr(struct sip_msg *msg, str* direction, str* charge_type, str* unit_type, int reservation_units);
//void ro_session_ontimeout(struct ro_tl *tl);

static int ro_fixup(void **param, int param_no);

static cmd_export_t cmds[] = {
		{ "Ro_CCR", 	(cmd_function) w_ro_ccr, 4, ro_fixup, 0, REQUEST_ROUTE },
		{ 0, 0, 0, 0, 0, 0 }
};

static param_export_t params[] = {
		{ "hash_size", 				INT_PARAM,			&ro_session_hash_size 		},
		{ "interim_update_credits",	INT_PARAM,			&interim_request_credits 	},
		{ "timer_buffer", 			INT_PARAM,			&ro_timer_buffer 			},
		{ "ro_forced_peer", 		STR_PARAM, 			&ro_forced_peer.s 			},
		{ "ro_auth_expiry",			INT_PARAM, 			&ro_auth_expiry 			},
		{ "cdp_event_latency", 		INT_PARAM,			&cdp_event_latency 			}, /*flag: report slow processing of CDP
																						callback events or not */
		{ "cdp_event_threshold", 	INT_PARAM, 			&cdp_event_threshold 		}, /*time in ms above which we should
																						report slow processing of CDP callback event*/
		{ "cdp_event_latency_log", 	INT_PARAM, 			&cdp_event_latency_loglevel },/*log-level to use to report
																						slow processing of CDP callback event*/
		{ "origin_host", 			STR_PARAM, 			&ro_origin_host_s 			},
		{ "origin_realm", 			STR_PARAM,			&ro_origin_realm_s 			},
		{ "destination_realm", 		STR_PARAM,			&ro_destination_realm_s 	},
		{ "destination_host", 		STR_PARAM,			&ro_destination_host_s 		},
		{ "service_context_id_root",STR_PARAM,			&ro_service_context_id_root_s 	},
		{ "service_context_id_ext", STR_PARAM,			&ro_service_context_id_ext_s 	},
		{ "service_context_id_mnc", STR_PARAM,			&ro_service_context_id_mnc_s 	},
		{ "service_context_id_mcc", STR_PARAM,			&ro_service_context_id_mcc_s 	},
		{ "service_context_id_release",	STR_PARAM, 		&ro_service_context_id_release_s},
		{ 0, 0, 0 }
};

stat_export_t mod_stats[] = {
		/*{"ccr_avg_response_time" ,  STAT_IS_FUNC, 	(stat_var**)get_avg_ccr_response_time	},*/
		/*{"ccr_timeouts" ,  			0, 				(stat_var**)&stat_ccr_timeouts  		},*/
		{ 0, 0, 0 }
};

/** module exports */
struct module_exports exports = { "ims_charging", DEFAULT_DLFLAGS, /* dlopen flags */
		cmds, 		/* Exported functions */
		params, 	/* Exported params */
		0, 			/* exported statistics */
		0, 			/* exported MI functions */
		0, 			/* exported pseudo-variables */
		0, 			/* extra processes */
		mod_init, 	/* module initialization function */
		0,
		mod_destroy, 	/* module destroy functoin */
		mod_child_init 	/* per-child init function */
};

int fix_parameters() {
	cfg.origin_host.s = ro_origin_host_s;
	cfg.origin_host.len = strlen(ro_origin_host_s);

	cfg.origin_realm.s = ro_origin_realm_s;
	cfg.origin_realm.len = strlen(ro_origin_realm_s);

	cfg.destination_realm.s = ro_destination_realm_s;
	cfg.destination_realm.len = strlen(ro_destination_realm_s);

	cfg.destination_host.s = ro_destination_host_s;
	cfg.destination_host.len = strlen(ro_destination_host_s);

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

	/* init timer lists*/
	if (init_ro_timer(ro_session_ontimeout) != 0) {
		LM_ERR("cannot init timer list\n");
		return -1;
	}


	/* bind to dialog module */
	if (!(load_dlg = (load_dlg_f) find_export("load_dlg", 0, 0))) {
		LM_ERR("mod_init: can not import load_dlg. This module requires Kamailio dialog moduile.\n");
	}

	if (load_dlg(&dlgb) == -1) {
		goto error;
	}

	return 0;

	error:
	LM_ERR("Failed to initialise ims_qos module\n");
	return RO_RETURN_FALSE;

}

static int mod_child_init(int rank) {
	return 0;
}

static void mod_destroy(void) {

}

static int w_ro_ccr(struct sip_msg *msg, str* direction, str* charge_type, str* unit_type, int reservation_units) {
	/* PSEUDOCODE/NOTES
	 * 1. What mode are we in - terminating or originating
	 * 2. check request type - 	IEC - Immediate Event Charging
	 * 							ECUR - Event Charging with Unit Reservation
	 * 							SCUR - Session Charging with Unit Reservation
	 * 3. probably only do SCUR in this module for now - can see event based charging in another component instead (AS for SMS for example, etc)
	 * 4. Check a dialog exists for call, if not we fail
	 * 5. make sure we dont already have an Ro Session for this dialog
	 * 6. create new Ro Session
	 * 7. register for DLG callback passing new Ro session as parameter - (if dlg torn down we know which Ro session it is associated with)
	 *
	 *
	 */
	LM_DBG("Ro CCR initiated: direction:%.*s, charge_type:%.*s, unit_type:%.*s, reservation_units:%i",
			direction->len, direction->s,
			charge_type->len, charge_type->s,
			unit_type->len, unit_type->s,
			reservation_units);

//	if (msg->REQ_METHOD != METHOD_INVITE)
//		return RO_RETURN_FALSE;
//
	int ret = Ro_Send_CCR(msg, direction, charge_type, unit_type, reservation_units);

	if (ret != 0) {
		LM_DBG("RO_CCR failed\n");
		return RO_RETURN_FALSE;
	} else {
		LM_DBG("RO_CCR_success\n");
		return RO_RETURN_TRUE;
	}

	return RO_RETURN_FALSE;
}

static int ro_fixup(void **param, int param_no) {
	str s;
	unsigned int num;

	if (param_no > 0 && param_no <= 3) {
		return fixup_var_str_12(param, param_no);
	} else if (param_no == 4) {
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

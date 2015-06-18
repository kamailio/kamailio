/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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
#include "../../data_lump.h"
#include "../../ip_addr.h"
#include "../../ut.h"
#include "../../sr_module.h"
#include "../../timer.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../socket_info.h"
#include "../../pvar.h"
#include "../ims_usrloc_scscf/usrloc.h"
#include "../../lib/kcore/statistics.h"
#include "../../modules/sl/sl.h"
#include "../../mod_fix.h"
#include "../../cfg/cfg_struct.h"

#include "save.h"
#include "api.h"
#include "lookup.h"
#include "regpv.h"
#include "reply.h"
#include "reg_mod.h"
#include "config.h"
#include "server_assignment.h"
#include "usrloc_cb.h"
#include "userdata_parser.h"
#include "cxdx_sar.h"
#include "cxdx_callbacks.h"
#include "registrar_notify.h"
#include "../cdp_avp/mod_export.h"

MODULE_VERSION

extern gen_lock_t* process_lock; /* lock on the process table */

int * callback_singleton; /**< Cx callback singleton 								*/


struct tm_binds tmb;

struct cdp_binds cdpb;
cdp_avp_bind_t *cdp_avp;


usrloc_api_t ul; /*!< Structure containing pointers to usrloc functions*/

char *scscf_user_data_dtd = 0; /* Path to "CxDataType.dtd" */
char *scscf_user_data_xsd = 0; /* Path to "CxDataType_Rel6.xsd" or "CxDataType_Rel7.xsd" */
int scscf_support_wildcardPSI = 0;
int store_data_on_dereg = 0; /**< should we store SAR data on de-registration  */

int ue_unsubscribe_on_dereg = 0;  /*many UEs do not unsubscribe on de reg - therefore we should remove their subscription and not send a notify
				   Some UEs do unsubscribe then everything is fine*/

int user_data_always = 0; /* Always Reports that user data is missing to HSS */

/* parameters storage */
str cxdx_dest_realm = str_init("ims.smilecoms.com");

//Only used if we want to force the Rx peer
//Usually this is configured at a stack level and the first request uses realm routing
str cxdx_forced_peer = {0,0};

str scscf_name_str = str_init("sip:scscf2.ims.smilecoms.com:6060"); /* default scscf_name - actual should be set via parameter*/
str scscf_serviceroute_uri_str; /* Service Route URI */

char *domain = "location";  ///TODO should be configurable mod param

/*! \brief Module init & destroy function */
static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);
static int w_save(struct sip_msg* _m, char * _route, char* _d, char* mode, char* _cflags);
static int w_assign_server_unreg(struct sip_msg* _m, char* _route, char* _d, char* _direction);
static int w_lookup(struct sip_msg* _m, char* _d, char* _p2);
static int w_lookup_path_to_contact(struct sip_msg* _m, char* contact_uri);

/*! \brief Fixup functions */
static int domain_fixup(void** param, int param_no);
static int assign_save_fixup3_async(void** param, int param_no);
static int unreg_fixup(void** param, int param_no);
static int fetchc_fixup(void** param, int param_no);
/*! \brief Functions */
static int add_sock_hdr(struct sip_msg* msg, char *str, char *foo);

AAAMessage* callback_cdp_request(AAAMessage *request, void *param);

int tcp_persistent_flag = -1; /*!< if the TCP connection should be kept open */
int method_filtering = 0; /*!< if the looked up contacts should be filtered based on supported methods */
int path_enabled = 0; /*!< if the Path HF should be handled */
int path_mode = PATH_MODE_STRICT; /*!< if the Path HF should be inserted in the reply.
 			*   - STRICT (2): always insert, error if no support indicated in request
 			*   - LAZY   (1): insert only if support indicated in request
 			*   - OFF    (0): never insert */

int path_use_params = 0; /*!< if the received- and nat-parameters of last Path uri should be used
 						 * to determine if UAC is nat'ed */

char *aor_avp_param = 0; /*!< if instead of extacting the AOR from the request, it should be
 						 * fetched via this AVP ID */
unsigned short aor_avp_type = 0;
int_str aor_avp_name;

/* Populate this AVP if testing for specific registration instance. */
char *reg_callid_avp_param = 0;
unsigned short reg_callid_avp_type = 0;
int_str reg_callid_avp_name;

char* rcv_avp_param = 0;
unsigned short rcv_avp_type = 0;
int_str rcv_avp_name;

int sock_flag = -1;
str sock_hdr_name = {0, 0};

int subscription_default_expires = 3600; /**< the default value for expires if none found*/
int subscription_min_expires = 10; /**< minimum subscription expiration time 		*/
int subscription_max_expires = 1000000; /**< maximum subscription expiration time 		*/
int subscription_expires_range = 0;

int notification_list_size_threshold = 0; /**Threshold for size of notification list after which a warning is logged */


extern reg_notification_list *notification_list; /**< list of notifications for reg to be sent			*/

#define RCV_NAME "received"
str rcv_param = str_init(RCV_NAME);

stat_var *accepted_registrations;
stat_var *rejected_registrations;
stat_var *max_expires_stat;
stat_var *max_contacts_stat;
stat_var *default_expire_stat;
stat_var *default_expire_range_stat;
/** SL API structure */
sl_api_t slb;

/*! \brief
 * Exported PV
 */
static pv_export_t mod_pvs[] = {
    {
        {"ulc", sizeof ("ulc") - 1}, PVT_OTHER, pv_get_ulc, pv_set_ulc,
        pv_parse_ulc_name, pv_parse_index, 0, 0
    },
    {
        {0, 0}, 0, 0, 0, 0, 0, 0, 0
    }
};


/*! \brief
 * Exported functions
 */
static cmd_export_t cmds[] = {
    {"save", (cmd_function) w_save, 2, assign_save_fixup3_async, 0, REQUEST_ROUTE | ONREPLY_ROUTE},
    {"lookup", (cmd_function) w_lookup, 1, domain_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
    {"lookup_path_to_contact", (cmd_function) w_lookup_path_to_contact, 1, fixup_var_str_12, 0, REQUEST_ROUTE},
    {"term_impu_registered", (cmd_function) term_impu_registered, 1, domain_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
    {"term_impu_has_contact", (cmd_function) term_impu_has_contact, 1, domain_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
    {"impu_registered", (cmd_function) impu_registered, 1, domain_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
    {"assign_server_unreg", (cmd_function) w_assign_server_unreg, 3, assign_save_fixup3_async, 0, REQUEST_ROUTE},
    {"add_sock_hdr", (cmd_function) add_sock_hdr, 1, fixup_str_null, 0, REQUEST_ROUTE},
    {"unregister", (cmd_function) unregister, 2, unreg_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
    {"reg_fetch_contacts", (cmd_function) pv_fetch_contacts, 3, fetchc_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
    {"reg_free_contacts", (cmd_function) pv_free_contacts, 1, fixup_str_null, 0, REQUEST_ROUTE | FAILURE_ROUTE},
    {"can_subscribe_to_reg", (cmd_function) can_subscribe_to_reg, 1, domain_fixup, 0, REQUEST_ROUTE},
    {"subscribe_to_reg", (cmd_function) subscribe_to_reg, 1, domain_fixup, 0, REQUEST_ROUTE},
    {"can_publish_reg", (cmd_function) can_publish_reg, 1, domain_fixup, 0, REQUEST_ROUTE},
    {"publish_reg", (cmd_function) publish_reg, 1, domain_fixup, 0, REQUEST_ROUTE},
    //{"bind_registrar", (cmd_function) bind_registrar, 0, 0, 0, 0},  TODO put this back in !
    {0, 0, 0, 0, 0, 0}
};


/*! \brief
 * Exported parameters
 */
static param_export_t params[] = {
    {"default_expires", INT_PARAM, &default_registrar_cfg.default_expires},
    {"default_expires_range", INT_PARAM, &default_registrar_cfg.default_expires_range},
    {"min_expires", INT_PARAM, &default_registrar_cfg.min_expires},
    {"max_expires", INT_PARAM, &default_registrar_cfg.max_expires},
    {"em_default_expires", INT_PARAM, &default_registrar_cfg.em_default_expires},
    {"em_min_expires", INT_PARAM, &default_registrar_cfg.em_max_expires},
    {"em_min_expires", INT_PARAM, &default_registrar_cfg.em_min_expires},

    {"default_q", INT_PARAM, &default_registrar_cfg.default_q},
    {"append_branches", INT_PARAM, &default_registrar_cfg.append_branches},
    {"case_sensitive", INT_PARAM, &default_registrar_cfg.case_sensitive},
    {"realm_prefix", PARAM_STRING, &default_registrar_cfg.realm_pref},

    {"received_param", PARAM_STR, &rcv_param},
    {"received_avp", PARAM_STRING, &rcv_avp_param},
    {"aor_avp", PARAM_STRING, &aor_avp_param},
    {"reg_callid_avp", PARAM_STRING, &reg_callid_avp_param},
    {"max_contacts", INT_PARAM, &default_registrar_cfg.max_contacts},
    {"retry_after", INT_PARAM, &default_registrar_cfg.retry_after},
    {"sock_flag", INT_PARAM, &sock_flag},
    {"sock_hdr_name", PARAM_STR, &sock_hdr_name},
    {"method_filtering", INT_PARAM, &method_filtering},
    {"use_path", INT_PARAM, &path_enabled},
    {"path_mode", INT_PARAM, &path_mode},
    {"path_use_received", INT_PARAM, &path_use_params},
    {"user_data_dtd", PARAM_STRING, &scscf_user_data_dtd},
    {"user_data_xsd", PARAM_STRING, &scscf_user_data_xsd},
    {"support_wildcardPSI", INT_PARAM, &scscf_support_wildcardPSI},
    {"scscf_name", PARAM_STR, &scscf_name_str}, //TODO: need to set this to default
    {"store_profile_dereg", INT_PARAM, &store_data_on_dereg},
    {"cxdx_forced_peer", PARAM_STR, &cxdx_forced_peer},
    {"cxdx_dest_realm", PARAM_STR, &cxdx_dest_realm},

    {"subscription_default_expires", INT_PARAM, &subscription_default_expires},
    {"subscription_min_expires", INT_PARAM, &subscription_min_expires},
    {"subscription_max_expires", INT_PARAM, &subscription_max_expires},
    {"ue_unsubscribe_on_dereg", INT_PARAM, &ue_unsubscribe_on_dereg},
    {"subscription_expires_range", INT_PARAM, &subscription_expires_range},
    {"user_data_always", INT_PARAM, &user_data_always},
    {"notification_list_size_threshold", INT_PARAM, &notification_list_size_threshold},

    {0, 0, 0}
};

/*! \brief We expose internal variables via the statistic framework below.*/
stat_export_t mod_stats[] = {
    {"max_expires", STAT_NO_RESET, &max_expires_stat},
    {"max_contacts", STAT_NO_RESET, &max_contacts_stat},
    {"default_expire", STAT_NO_RESET, &default_expire_stat},
    {"default_expires_range", STAT_NO_RESET, &default_expire_range_stat},
    {"accepted_regs", 0, &accepted_registrations},
    {"rejected_regs", 0, &rejected_registrations},
    {"sar_avg_response_time", STAT_IS_FUNC, (stat_var**) get_avg_sar_response_time},
    {"sar_timeouts", 0, (stat_var**) & stat_sar_timeouts},
    {0, 0, 0}
};

/*! \brief
 * Module exports structure
 */
struct module_exports exports = {
    "ims_registrar_scscf",
    DEFAULT_DLFLAGS, /* dlopen flags */
    cmds, /* Exported functions */
    params, /* Exported parameters */
    mod_stats, /* exported statistics */
    0, /* exported MI functions */
    mod_pvs, /* exported pseudo-variables */
    0, /* extra processes */
    mod_init, /* module initialization function */
    0,
    mod_destroy, /* destroy function */
    child_init, /* Per-child init function */
};

static str orig_prefix = {"sip:orig@", 9};

/*! \brief
 * Initialize parent
 */

static int mod_init(void) {
    pv_spec_t avp_spec;
    str s;
    bind_usrloc_t bind_usrloc;
    qvalue_t dq;
    
    callback_singleton = shm_malloc(sizeof (int));
    *callback_singleton = 0;

    /*build the required strings */
    scscf_serviceroute_uri_str.s =
            (char*) pkg_malloc(orig_prefix.len + scscf_name_str.len);

    if (!scscf_serviceroute_uri_str.s) {
        LM_ERR("Unable to allocate memory for service route uri\n");
        return -1;
    }

    memcpy(scscf_serviceroute_uri_str.s, orig_prefix.s, orig_prefix.len);
    scscf_serviceroute_uri_str.len = orig_prefix.len;
    if (scscf_name_str.len > 4
            && strncasecmp(scscf_name_str.s, "sip:", 4) == 0) {
        memcpy(scscf_serviceroute_uri_str.s + scscf_serviceroute_uri_str.len,
                scscf_name_str.s + 4, scscf_name_str.len - 4);
        scscf_serviceroute_uri_str.len += scscf_name_str.len - 4;
    } else {
        memcpy(scscf_serviceroute_uri_str.s + scscf_serviceroute_uri_str.len,
                scscf_name_str.s, scscf_name_str.len);
        scscf_serviceroute_uri_str.len += scscf_name_str.len;
    }

    /* </build required strings> */

#ifdef STATISTICS
    /* register statistics */
    if (register_module_stats(exports.name, mod_stats) != 0) {
        LM_ERR("failed to register core statistics\n");
        return -1;
    }
    if (!register_stats()) {
        LM_ERR("Unable to register statistics\n");
        return -1;
    }
#endif

    /*register space for notification processor*/
    register_procs(1);

    /* bind the SL API */
    if (sl_load_api(&slb) != 0) {
        LM_ERR("cannot bind to SL API\n");
        return -1;
    }

    /* load the TM API */
    if (load_tm_api(&tmb) != 0) {
        LM_ERR("can't load TM API\n");
        return -1;
    }

    /* load the CDP API */
    if (load_cdp_api(&cdpb) != 0) {
        LM_ERR("can't load CDP API\n");
        return -1;
    }

    cdp_avp = load_cdp_avp();
    if (!cdp_avp) {
        LM_ERR("can't load CDP_AVP API\n");
        return -1;
    }

    if (cfg_declare("registrar", registrar_cfg_def, &default_registrar_cfg,
            cfg_sizeof(registrar), &registrar_cfg)) {
        LM_ERR("Fail to declare the configuration\n");
        return -1;
    }

    if (rcv_avp_param && *rcv_avp_param) {
        s.s = rcv_avp_param;
        s.len = strlen(s.s);
        if (pv_parse_spec(&s, &avp_spec) == 0 || avp_spec.type != PVT_AVP) {
            LM_ERR("malformed or non AVP %s AVP definition\n", rcv_avp_param);
            return -1;
        }

        if (pv_get_avp_name(0, &avp_spec.pvp, &rcv_avp_name, &rcv_avp_type)
                != 0) {
            LM_ERR("[%s]- invalid AVP definition\n", rcv_avp_param);
            return -1;
        }
    } else {
        rcv_avp_name.n = 0;
        rcv_avp_type = 0;
    }
    if (aor_avp_param && *aor_avp_param) {
        s.s = aor_avp_param;
        s.len = strlen(s.s);
        if (pv_parse_spec(&s, &avp_spec) == 0 || avp_spec.type != PVT_AVP) {
            LM_ERR("malformed or non AVP %s AVP definition\n", aor_avp_param);
            return -1;
        }

        if (pv_get_avp_name(0, &avp_spec.pvp, &aor_avp_name, &aor_avp_type)
                != 0) {
            LM_ERR("[%s]- invalid AVP definition\n", aor_avp_param);
            return -1;
        }
    } else {
        aor_avp_name.n = 0;
        aor_avp_type = 0;
    }

    if (reg_callid_avp_param && *reg_callid_avp_param) {
        s.s = reg_callid_avp_param;
        s.len = strlen(s.s);
        if (pv_parse_spec(&s, &avp_spec) == 0 || avp_spec.type != PVT_AVP) {
            LM_ERR("malformed or non AVP %s AVP definition\n", reg_callid_avp_param);
            return -1;
        }

        if (pv_get_avp_name(0, &avp_spec.pvp, &reg_callid_avp_name,
                &reg_callid_avp_type) != 0) {
            LM_ERR("[%s]- invalid AVP definition\n", reg_callid_avp_param);
            return -1;
        }
    } else {
        reg_callid_avp_name.n = 0;
        reg_callid_avp_type = 0;
    }

    bind_usrloc = (bind_usrloc_t) find_export("ul_bind_usrloc", 1, 0);
    if (!bind_usrloc) {
        LM_ERR("can't bind usrloc\n");
        return -1;
    }

    /* Normalize default_q parameter */
    dq = cfg_get(registrar, registrar_cfg, default_q);
    if (dq != Q_UNSPECIFIED) {
        if (dq > MAX_Q) {
            LM_DBG("default_q = %d, lowering to MAX_Q: %d\n", dq, MAX_Q);
            dq = MAX_Q;
        } else if (dq < MIN_Q) {
            LM_DBG("default_q = %d, raising to MIN_Q: %d\n", dq, MIN_Q);
            dq = MIN_Q;
        }
    }
    cfg_get(registrar, registrar_cfg, default_q) = dq;

    if (bind_usrloc(&ul) < 0) {
        return -1;
    }

    /*Register for callback of URECORD being deleted - so we can send a SAR*/

    if (ul.register_ulcb == NULL) {
        LM_ERR("Could not import ul_register_ulcb\n");
        return -1;
    }
    
    if (ul.register_ulcb(0, 0, UL_IMPU_INSERT, ul_impu_inserted, 0) < 0) {
        LM_ERR("can not register callback for insert\n");
        return -1;
    }

    if (sock_hdr_name.s) {
        if (sock_hdr_name.len == 0 || sock_flag == -1) {
            LM_WARN("empty sock_hdr_name or sock_flag no set -> reseting\n");
            sock_hdr_name.len = 0;
            sock_flag = -1;
        }
    } else if (sock_flag != -1) {
        LM_WARN("sock_flag defined but no sock_hdr_name -> reseting flag\n");
        sock_flag = -1;
    }

    /* fix the flags */
    sock_flag = (sock_flag != -1) ? (1 << sock_flag) : 0;
    tcp_persistent_flag =
            (tcp_persistent_flag != -1) ? (1 << tcp_persistent_flag) : 0;

    /* init the registrar notifications */
    if (!notify_init()) return -1;

    /* register the registrar notifications timer */
    //Currently we do not use this - we send notifies immediately
    //if (register_timer(notification_timer, notification_list, 5) < 0) return -1;

    return 0;
}

static int child_init(int rank) {
    LM_DBG("Initialization of module in child [%d] \n", rank);
    int pid;
    
    if (rank == PROC_MAIN) {
        pid = fork_process(PROC_MIN, "sip_notification_event_process", 1);
        if (pid < 0)
            return -1; //error
        if (pid == 0) {
            if (cfg_child_init())
                return -1; //error
            notification_event_process();
        }
    }
    
    
    if (rank == PROC_MAIN || rank == PROC_TCP_MAIN)
        return 0;
    if (rank == 1) {
        /* init stats */
        //TODO if parameters are modified via cfg framework do i change them?
        update_stat(max_expires_stat, default_registrar_cfg.max_expires);
        update_stat(max_contacts_stat, default_registrar_cfg.max_contacts);
        update_stat(default_expire_stat, default_registrar_cfg.default_expires);
    }
    /* don't do anything for main process and TCP manager process */

    /* don't do anything for main process and TCP manager process */
    if (rank == PROC_MAIN || rank == PROC_TCP_MAIN)
        return 0;

    /* Init the user data parser */
    if (!parser_init(scscf_user_data_dtd, scscf_user_data_xsd))
        return -1;
    
    
    lock_get(process_lock);
    if ((*callback_singleton) == 0) {
        *callback_singleton = 1;
        cdpb.AAAAddRequestHandler(callback_cdp_request, NULL);
    }
    lock_release(process_lock);

    return 0;
}


/**
 * Handler for incoming Diameter requests.
 * @param request - the received request
 * @param param - generic pointer
 * @returns the answer to this request
 */
AAAMessage* callback_cdp_request(AAAMessage *request, void *param) {
    if (is_req(request)) {

        switch (request->applicationId) {
            case IMS_Cx:
            //case IMS_Dx:  IMS_Cx is same as IMS_Dx 16777216
                switch (request->commandCode) {
                    case IMS_RTR:
                        LM_INFO("Cx/Dx request handler():- Received an IMS_RTR \n");
                        return cxdx_process_rtr(request);
                        break;
                    default:
                        LM_ERR("Cx/Dx request handler(): - Received unknown request for Cx/Dx command %d, flags %#1x endtoend %u hopbyhop %u\n", request->commandCode, request->flags, request->endtoendId, request->hopbyhopId);
                        return 0;
                        break;
                }
                break;
            default:
                LM_ERR("Cx/Dx request handler(): - Received unknown request for app %d command %d\n", request->applicationId, request->commandCode);
                return 0;
                break;
        }
    }
    return 0;
}


/*! \brief
 * Wrapper to save(location)
 */
static int w_save(struct sip_msg* _m, char* _route, char* _d, char* mode, char* _cflags) {
    return save(_m, _d, _route);
}

static int w_assign_server_unreg(struct sip_msg* _m, char* _route, char* _d, char* _direction) {
    str direction;

    direction.s = _direction;
    direction.len = strlen(_direction);
    return assign_server_unreg(_m, _d, &direction, _route);

}

static int w_lookup_path_to_contact(struct sip_msg* _m, char* contact_uri) {
    return lookup_path_to_contact(_m, contact_uri);
}

/*! \brief
 * Wrapper to lookup(location)
 */
static int w_lookup(struct sip_msg* _m, char* _d, char* _p2) {
    return lookup(_m, (udomain_t*) _d);
}

/*! \brief
 * Convert char* parameter to udomain_t* pointer
 */
static int domain_fixup(void** param, int param_no) {
    udomain_t* d;

    if (param_no == 1) {
        if (ul.register_udomain((char*) *param, &d) < 0) {
            LM_ERR("failed to register domain\n");
            return E_UNSPEC;
        }

        *param = (void*) d;
    }
    return 0;
}

/*! \brief
 * Convert char* parameter to udomain_t* pointer
 * Convert char* parameter to pv_elem_t* pointer
 */
static int unreg_fixup(void** param, int param_no) {
    if (param_no == 1) {
        return domain_fixup(param, 1);
    } else if (param_no == 2) {
        return fixup_spve_null(param, 1);
    }
    return 0;
}

/*
 * Convert the char* parameters
 */
static int assign_save_fixup3_async(void** param, int param_no) {

    if (strlen((char*) *param) <= 0) {
        LM_ERR("empty parameter %d not allowed\n", param_no);
        return -1;
    }

    if (param_no == 1) {        //route name - static or dynamic string (config vars)
        if (fixup_spve_null(param, param_no) < 0)
            return -1;
        return 0;
    } else if (param_no == 2) {
        udomain_t* d;

        if (ul.register_udomain((char*) *param, &d) < 0) {
            LM_ERR("Error doing fixup on assign save");
            return -1;
        }
        *param = (void*) d;
    }

    return 0;
}

/*! \brief
 * Convert char* parameter to udomain_t* pointer
 * Convert char* parameter to pv_elem_t* pointer
 * Convert char* parameter to str* pointer
 */
static int fetchc_fixup(void** param, int param_no) {
    if (param_no == 1) {
        return domain_fixup(param, 1);
    } else if (param_no == 2) {
        return fixup_spve_null(param, 1);
    } else if (param_no == 3) {
        return fixup_str_null(param, 1);
    }
    return 0;
}

static void mod_destroy(void) {
    free_p_associated_uri_buf();
    free_expired_contact_buf();
}

static int add_sock_hdr(struct sip_msg* msg, char *name, char *foo) {
    struct socket_info* si;
    struct lump* anchor;
    str *hdr_name;
    str hdr;
    char *p;

    hdr_name = (str*) name;
    si = msg->rcv.bind_address;

    if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
        LM_ERR("failed to parse message\n");
        goto error;
    }

    anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
    if (anchor == 0) {
        LM_ERR("can't get anchor\n");
        goto error;
    }

    hdr.len = hdr_name->len + 2 + si->sock_str.len + CRLF_LEN;
    if ((hdr.s = (char*) pkg_malloc(hdr.len)) == 0) {
        LM_ERR("no more pkg mem\n");
        goto error;
    }

    p = hdr.s;
    memcpy(p, hdr_name->s, hdr_name->len);
    p += hdr_name->len;
    *(p++) = ':';
    *(p++) = ' ';

    memcpy(p, si->sock_str.s, si->sock_str.len);
    p += si->sock_str.len;

    memcpy(p, CRLF, CRLF_LEN);
    p += CRLF_LEN;

    if (p - hdr.s != hdr.len) {
        LM_CRIT("buffer overflow (%d!=%d)\n", (int) (long) (p - hdr.s), hdr.len);
        goto error1;
    }

    if (insert_new_lump_before(anchor, hdr.s, hdr.len, 0) == 0) {
        LM_ERR("can't insert lump\n");
        goto error1;
    }

    return 1;
error1:
    pkg_free(hdr.s);
error:
    return -1;
}

void default_expires_stats_update(str* gname, str* name) {
    update_stat(default_expire_stat, cfg_get(registrar, registrar_cfg, default_expires));
}

void max_expires_stats_update(str* gname, str* name) {
    update_stat(max_expires_stat, cfg_get(registrar, registrar_cfg, max_expires));
}

void default_expires_range_update(str* gname, str* name) {
    update_stat(default_expire_range_stat, cfg_get(registrar, registrar_cfg, default_expires_range));
}

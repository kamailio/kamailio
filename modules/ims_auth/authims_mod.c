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
#include <string.h>
#include "stats.h"
#include "../../sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../mod_fix.h"
#include "../../trim.h"
#include "../../mem/mem.h"
#include "../../modules/sl/sl.h"
#include "../cdp/cdp_load.h"
#include "../tm/tm_load.h"
#include "authorize.h"
#include "authims_mod.h"
#include "cxdx_mar.h"
#include "../../lib/ims/useful_defs.h"

MODULE_VERSION

static void destroy(void);
static int mod_init(void);

static int auth_fixup(void** param, int param_no);
static int auth_fixup_async(void** param, int param_no);
static int challenge_fixup_async(void** param, int param_no);

struct cdp_binds cdpb;

/*! API structures */
struct tm_binds tmb; /**< Structure with pointers to tm funcs 				*/

extern auth_hash_slot_t *auth_data; /**< authentication vectors hast table 					*/

int auth_data_hash_size = 1024; /**< the size of the hash table 							*/
int auth_vector_timeout = 60; /**< timeout for a sent auth vector to expire in sec 		*/
int auth_used_vector_timeout = 3600; /**< timeout for a used auth vector to expire in sec 		*/
int max_nonce_reuse = 0; /**< how many times a nonce can be reused (provided nc is incremented)	*/
int auth_data_timeout = 60; /**< timeout for a hash entry to expire when empty in sec 	*/
int add_authinfo_hdr = 1; /**< should an Authentication-Info header be added on 200 OK responses? 	*/
int av_request_at_once = 1; /**< how many auth vectors to request in a MAR 				*/
int av_request_at_sync = 1; /**< how many auth vectors to request in a sync MAR 		*/
static str registration_qop = str_init("auth,auth-int"); /**< the qop options to put in the authorization challenges */
str registration_qop_str = STR_NULL; /**< the qop options to put in the authorization challenges */
int av_check_only_impu = 0; /**< Should we check IMPU (0) or IMPU and IMPI (1), when searching for authentication vectors? */
static str s_qop_s = str_init(", qop=\"");
static str s_qop_e = str_init("\"");

static str registration_default_algorithm = str_init("AKAv1-MD5"); /**< default algorithm for registration (if none present)*/
unsigned char registration_default_algorithm_type = 1; /**< fixed default algorithm for registration (if none present)	 */

str cxdx_dest_realm = str_init("ims.smilecoms.com");

//Only used if we want to force the Rx peer
//Usually this is configured at a stack level and the first request uses realm routing
str cxdx_forced_peer = str_init("");


/* fixed parameter storage */
str scscf_name_str = str_init("sip:scscf.ims.smilecoms.com:6060"); /**< fixed name of the S-CSCF 							*/

/* used mainly in testing - load balancing with SIPP where we don't want to worry about auth */
int ignore_failed_auth = 0;

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    {"ims_www_authenticate", (cmd_function) www_authenticate, 1, auth_fixup, 0, REQUEST_ROUTE},
    {"ims_www_challenge", (cmd_function) www_challenge2, 2, challenge_fixup_async, 0, REQUEST_ROUTE},
    {"ims_www_challenge", (cmd_function) www_challenge3, 3, challenge_fixup_async, 0, REQUEST_ROUTE},
    {"ims_www_resync_auth", (cmd_function) www_resync_auth, 2, challenge_fixup_async, 0, REQUEST_ROUTE},
    {"ims_proxy_authenticate", (cmd_function) proxy_authenticate, 1, auth_fixup, 0, REQUEST_ROUTE},
    {"ims_proxy_challenge", (cmd_function) proxy_challenge, 2, auth_fixup_async, 0, REQUEST_ROUTE},
    {"bind_ims_auth", (cmd_function) bind_ims_auth, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
    {"name", PARAM_STR, &scscf_name_str},
    {"auth_data_hash_size", INT_PARAM, &auth_data_hash_size},
    {"auth_vector_timeout", INT_PARAM, &auth_vector_timeout},
    {"auth_used_vector_timeout", INT_PARAM, &auth_used_vector_timeout},
    {"auth_data_timeout", INT_PARAM, &auth_data_timeout},
    {"max_nonce_reuse", INT_PARAM, &max_nonce_reuse},
    {"add_authinfo_hdr", INT_PARAM, &add_authinfo_hdr},
    {"av_request_at_once", INT_PARAM, &av_request_at_once},
    {"av_request_at_sync", INT_PARAM, &av_request_at_sync},
    {"registration_default_algorithm", PARAM_STR, &registration_default_algorithm},
    {"registration_qop", PARAM_STR, &registration_qop},
    {"ignore_failed_auth", INT_PARAM, &ignore_failed_auth},
    {"av_check_only_impu", INT_PARAM, &av_check_only_impu},
    {"cxdx_forced_peer", PARAM_STR, &cxdx_forced_peer},
    {"cxdx_dest_realm", PARAM_STR, &cxdx_dest_realm},
    {0, 0, 0}
};

stat_export_t mod_stats[] = {
	{"mar_avg_response_time" ,  STAT_IS_FUNC, 	(stat_var**)get_avg_mar_response_time	},
	{"mar_timeouts" ,  			0, 				(stat_var**)&stat_mar_timeouts  		},
	{0,0,0}
};

/*
 * Module interface
 */
struct module_exports exports = {
    "ims_auth",
    DEFAULT_DLFLAGS, /* dlopen flags */
    cmds, /* Exported functions */
    params, /* Exported parameters */
    0, /* exported statistics */
    0, /* exported MI functions */
    0, /* exported pseudo-variables */
    0, /* extra processes */
    mod_init, /* module initialization function */
    0, /* response function */
    destroy, /* destroy function */
    0 /* child initialization function */
};

static int mod_init(void) {
    registration_default_algorithm_type = get_algorithm_type(registration_default_algorithm);

#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats( exports.name, mod_stats)!=0 ) {
		LM_ERR("failed to register core statistics\n");
		return -1;
	}

	if (!register_stats()){
		LM_ERR("Unable to register statistics\n");
		return -1;
	}
#endif

    /* check the max_nonce_reuse param */
    if (auth_used_vector_timeout < 0) {
        LM_WARN("bad value for auth_used_vector_timeout parameter (=%d), must be positive. Fixed to 3600\n", auth_used_vector_timeout);
        auth_used_vector_timeout = 3600;
    }

    /* check the max_nonce_reuse param */
    if (max_nonce_reuse < 0) {
        LM_WARN("bad value for max_nonce_reuse parameter (=%d), must be positive. Fixed to 0\n", max_nonce_reuse);
        max_nonce_reuse = 0;
    }

    /* load the CDP API */
    if (load_cdp_api(&cdpb) != 0) {
        LM_ERR("can't load CDP API\n");
        return -1;
    }

    /* load the TM API */
    if (load_tm_api(&tmb) != 0) {
        LM_ERR("can't load TM API\n");
        return -1;
    }

    /* Init the authorization data storage */
    if (!auth_data_init(auth_data_hash_size)) {
        LM_ERR("Unable to init auth data\n");
        return -1;
    }

    /* set default qop */
    if (registration_qop.s && registration_qop.len > 0) {
        registration_qop_str.len = s_qop_s.len + registration_qop.len
                + s_qop_e.len;
        registration_qop_str.s = pkg_malloc(registration_qop_str.len);
        if (!registration_qop_str.s) {
            LM_ERR("Error allocating %d bytes\n", registration_qop_str.len);
            registration_qop_str.len = 0;
            return 0;
        }
        registration_qop_str.len = 0;
        STR_APPEND(registration_qop_str, s_qop_s);
        memcpy(registration_qop_str.s + registration_qop_str.len,
            registration_qop.s, registration_qop.len);
        registration_qop_str.len += registration_qop.len;
        STR_APPEND(registration_qop_str, s_qop_e);
    } else {
        registration_qop_str.len = 0;
        registration_qop_str.s = 0;
    }

    /* Register the auth vector timer */
    if (register_timer(reg_await_timer, auth_data, 10) < 0) {
        LM_ERR("Unable to register auth vector timer\n");
        return -1;
    }

    return 0;
}

static void destroy(void) {
    auth_data_destroy();
}

/*
 * Convert the char* parameters
 */
static int challenge_fixup_async(void** param, int param_no) {

    if (strlen((char*) *param) <= 0) {
        LM_ERR("empty parameter %d not allowed\n", param_no);
        return -1;
    }

    if (param_no == 1) {        //route name - static or dynamic string (config vars)
        if (fixup_spve_null(param, param_no) < 0)
            return -1;
        return 0;
    } else if (param_no == 2) {
        if (fixup_var_str_12(param, 1) == -1) {
            LM_ERR("Error doing fixup on challenge");
            return -1;
        }
    } else if (param_no == 3) /* algorithm */ {
	if (fixup_var_str_12(param, 1) == -1) {
            LM_ERR("Error doing fixup on challenge");
            return -1;
        }
    }

    return 0;
}

/*
 * Convert the char* parameters
 */
static int auth_fixup(void** param, int param_no) {
    if (strlen((char*) *param) <= 0) {
        LM_ERR("empty parameter %d not allowed\n", param_no);
        return -1;
    }

    if (param_no == 1) {
        if (fixup_var_str_12(param, 1) == -1) {
            LM_ERR("Erroring doing fixup on auth");
            return -1;
        }
    }

    return 0;
}

/*
 * Convert the char* parameters
 */
static int auth_fixup_async(void** param, int param_no) {
    if (strlen((char*) *param) <= 0) {
        LM_ERR("empty parameter %d not allowed\n", param_no);
        return -1;
    }

    if (param_no == 1) {        //route name - static or dynamic string (config vars)
        if (fixup_spve_null(param, param_no) < 0)
            return -1;
        return 0;
    } else if (param_no == 2) {
        if (fixup_var_str_12(param, 1) == -1) {
            LM_ERR("Erroring doing fixup on auth");
            return -1;
        }
    }

    return 0;
}

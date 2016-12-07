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

#include "../../timer.h"
#include "../../modules/sl/sl.h"

#include "mod.h"
#include "scscf_list.h"
#include "cxdx_uar.h"
#include "cxdx_lir.h"
#include "db.h"
#include "nds.h"

MODULE_VERSION

struct tm_binds tmb;
struct cdp_binds cdpb;
cdp_avp_bind_t *cdp_avp;

/** SL API structure */
sl_api_t slb;


//module parameters
char* ims_icscf_db_url="mysql://icscf:heslo@localhost/hssdata";     /**< DB URL */
char* ims_icscf_db_nds_table="nds_trusted_domains";                         /**< NDS table in DB */
char* ims_icscf_db_scscf_table="s_cscf";                                            /**< S-CSCF table in db */
char* ims_icscf_db_capabilities_table="s_cscf_capabilities";        /**< S-CSCF capabilities table in db */

int ims_icscf_hash_size = 128;
int scscf_entry_expiry = 300;

/* parameters storage */
char* cxdx_dest_realm_s = "ims.smilecoms.com";
str cxdx_dest_realm;

str preferred_scscf_uri = str_init("sip:scscf.ims.smilecoms.com:4060");
int use_preferred_scscf_uri = 0;

//Only used if we want to force the Rx peer
//Usually this is configured at a stack level and the first request uses realm routing
char* cxdx_forced_peer_s = "";
str cxdx_forced_peer;

/** 
 * Name of the route, if a user is not found in the HSS (LIR)
 */
char* route_lir_user_unknown=0; /* default is the main route */
/** 
 * Number of the route, if a user is not found in the HSS (LIR)
 */
int route_lir_user_unknown_no=-1;

/** 
 * Name of the route, if a user is not found in the HSS (UAR)
 */
char* route_uar_user_unknown=0; /* default is the main route */
/** 
 * Number of the route, if a user is not found in the HSS (UAR)
 */
int route_uar_user_unknown_no=-1;


/** module functions */
static int mod_init(void);
static int fixup_uar(void** param, int param_no);
static int fixup_lir(void** param, int param_no);

static cmd_export_t cmds[] = {
    {"I_perform_user_authorization_request", (cmd_function) I_perform_user_authorization_request, 2, fixup_uar, 0, REQUEST_ROUTE},
    {"I_perform_location_information_request", (cmd_function) I_perform_location_information_request, 2, fixup_lir, 0, REQUEST_ROUTE},
    {"I_scscf_select", (cmd_function) I_scscf_select, 1, 0, 0, REQUEST_ROUTE | FAILURE_ROUTE},
    {"I_scscf_drop", (cmd_function) I_scscf_drop, 0, 0, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    { 0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
    {"route_lir_user_unknown", PARAM_STRING, &route_lir_user_unknown},
    {"route_uar_user_unknown", PARAM_STRING, &route_uar_user_unknown},
    {"scscf_entry_expiry", INT_PARAM, &scscf_entry_expiry},
    {"db_url", 					PARAM_STRING, &ims_icscf_db_url},
    {"db_nds_table", 			PARAM_STRING, &ims_icscf_db_nds_table},
    {"db_scscf_table", 			PARAM_STRING, &ims_icscf_db_scscf_table},
    {"db_capabilities_table", 	PARAM_STRING, &ims_icscf_db_capabilities_table},
    {"cxdx_forced_peer", PARAM_STR, &cxdx_forced_peer},
    {"cxdx_dest_realm", PARAM_STR, &cxdx_dest_realm},
    {"preferred_scscf_uri",	PARAM_STR, &preferred_scscf_uri},
    {"use_preferred_scscf_uri", INT_PARAM, &use_preferred_scscf_uri},
    {0, 0, 0}
};

stat_export_t mod_stats[] = {
	{"uar_avg_response_time" ,  	STAT_IS_FUNC, 	(stat_var**)get_avg_uar_response_time	},
	{"lir_avg_response_time" ,  	STAT_IS_FUNC, 	(stat_var**)get_avg_lir_response_time	},
	{"uar_timeouts" ,  				0, 				(stat_var**)&stat_uar_timeouts  		},
	{"lir_timeouts" ,  				0, 				(stat_var**)&stat_lir_timeouts  		},
	{0,0,0}
};

/** module exports */
struct module_exports exports = {
    "ims_icscf",
    DEFAULT_DLFLAGS, /* dlopen flags */
    cmds, /* Exported functions */
    params,
    mod_stats, /* exported statistics */
    0, /* exported MI functions */
    0, /* exported pseudo-variables */
    0, /* extra processes */
    mod_init, /* module initialization function */
    0,
    0,
    0 /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void) {
	cdp_avp = 0;
	int route_no;

#ifdef STATISTICS
	if (register_module_stats( exports.name, mod_stats) != 0) {
		LM_ERR("failed to register core statistics\n");
		goto error;
	}
	if (!register_stats()){
		LM_ERR("Unable to register statistics\n");
		goto error;
	}
#endif

	/* initialising hash table*/
	if (!i_hash_table_init(ims_icscf_hash_size)) {
		LM_ERR("Error initializing the Hash Table for stored S-CSCF lists\n");
		goto error;
	}

	/* load the TM API */
	if (load_tm_api(&tmb) != 0) {
		LM_ERR("can't load TM API\n");
		goto error;
	}

	/* load the CDP API */
	if (load_cdp_api(&cdpb) != 0) {
		LM_ERR("can't load CDP API\n");
		goto error;
	}

	cdp_avp = load_cdp_avp();
	if (!cdp_avp) {
		LM_ERR("can't load CDP_AVP API\n");
		goto error;
	}

	/* cache the trusted domain names and capabilities */
	/* bind to the db module */
	if (ims_icscf_db_bind(ims_icscf_db_url) < 0)
		goto error;

	/* bind the SL API */
	if (sl_load_api(&slb) != 0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	ims_icscf_db_init(ims_icscf_db_url, ims_icscf_db_nds_table, ims_icscf_db_scscf_table,
			ims_icscf_db_capabilities_table);

	I_NDS_get_trusted_domains();
	I_get_capabilities();

	ims_icscf_db_close();

	if (!i_hash_table_init(ims_icscf_hash_size)) {
		LOG(
				L_ERR,
				"ERR"M_NAME":mod_init: Error initializing the Hash Table for stored S-CSCF lists\n");
		goto error;
	}

	/* register global timer used to get rid of stale scscf_lists*/
	if (register_timer(ims_icscf_timer_routine, 0, 60) < 0) {
		LM_ERR("failed to register timer \n");
		return -1;
	}
	
	/* try to fix the xmlrpc route */
	if (route_lir_user_unknown){
		route_no=route_get(&main_rt, route_lir_user_unknown);
		if (route_no==-1){
			ERR("ims_icscf: failed to fix route \"%s\": route_get() failed\n",
					route_lir_user_unknown);
			return -1;
		}
		if (main_rt.rlist[route_no]==0){
			WARN("ims_icscf: ims_icscf route \"%s\" is empty / doesn't exist\n",
					route_lir_user_unknown);
		}
		route_lir_user_unknown_no=route_no;
	}
	/* try to fix the xmlrpc route */
	if (route_uar_user_unknown){
		route_no=route_get(&main_rt, route_uar_user_unknown);
		if (route_no==-1){
			ERR("ims_icscf: failed to fix route \"%s\": route_get() failed\n",
					route_uar_user_unknown);
			return -1;
		}
		if (main_rt.rlist[route_no]==0){
			WARN("ims_icscf: ims_icscf route \"%s\" is empty / doesn't exist\n",
					route_uar_user_unknown);
		}
		route_uar_user_unknown_no=route_no;
	}

	LM_DBG("ims_icscf module successfully initialised\n");

	return 0;
	error:
	LM_ERR("Failed to initialise ims_icscf module\n");
	return -1;
}

static int fixup_uar(void** param, int param_no)
{
    if (strlen((char*) *param) <= 0) {
        LM_ERR("empty parameter %d not allowed\n", param_no);
        return -1;
    }

    if (param_no == 1) {        //route name - static or dynamic string (config vars)
        if (fixup_spve_null(param, param_no) < 0){
            LM_ERR("fixup spve failed on %d\n", param_no);
            return -1;
        }
        return 0;
    }
    return 0;
    
}

static int fixup_lir(void** param, int param_no)
{
	if (strlen((char*) *param) <= 0) {
        LM_ERR("empty parameter %d not allowed\n", param_no);
        return -1;
        }

        if (param_no == 1) {        //route name - static or dynamic string (config vars)
            if (fixup_spve_null(param, param_no) < 0)
                return -1;
            return 0;
        } 
        return 0;
    
}


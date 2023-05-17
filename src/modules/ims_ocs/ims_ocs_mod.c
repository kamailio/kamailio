/*
 * Copyright (C) 2016 ng-voice GmbH, carsten@ng-voice.com
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

#include "../../core/sr_module.h"
#include "../../core/route.h"
#include "../cdp/cdp_load.h"
#include "../cdp_avp/cdp_avp_mod.h"
#include "../../core/parser/msg_parser.h"
#include "ims_ocs_mod.h"
#include "msg_faker.h"
#include "ocs_avp_helper.h"

MODULE_VERSION

extern gen_lock_t *process_lock; /* lock on the process table */

struct cdp_binds cdpb;

cdp_avp_bind_t *cdp_avp;

/** module functions */
static int mod_init(void);
static int mod_child_init(int);
static void mod_destroy(void);

int *callback_singleton; /*< Callback singleton */

int result_code = 0;
int granted_units = 0;
int final_unit = 0;

int event_route_ccr_orig = 0;
int event_route_ccr_term = 0;

static int w_ccr_result(
		struct sip_msg *msg, char *result, char *grantedunits, char *final);

static cmd_export_t cmds[] = {{"ccr_result", (cmd_function)w_ccr_result, 3,
									  fixup_var_pve_str_12, 0, REQUEST_ROUTE},
		{0, 0, 0, 0, 0, 0}};

static param_export_t params[] = {{0, 0, 0}};


/** module exports */
struct module_exports exports = {"ims_ocs", DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			/* Exported functions */
		params, 0,		/* exported RPC methods */
		0,				/* exported pseudo-variables */
		0,				/* response handling function */
		mod_init,		/* module initialization function */
		mod_child_init, /* per-child init function */
		mod_destroy};

/**
 * init module function
 */
static int mod_init(void)
{
	LM_DBG("Loading...\n");
	event_route_ccr_orig = route_get(&event_rt, "ocs:ccr-orig");
	if(event_route_ccr_orig < 0) {
		LM_ERR("No ocs:ccr-orig event route found\n");
		goto error;
	}
	LM_DBG("Found Route ocs:ccr-orig: %i\n", event_route_ccr_orig);

	event_route_ccr_term = route_get(&event_rt, "ocs:ccr-term");
	if(event_route_ccr_term < 0) {
		LM_INFO("No ocs:ccr-term event route found\n");
	}
	LM_DBG("Found Route ocs:ccr-term: %i\n", event_route_ccr_term);


	callback_singleton = shm_malloc(sizeof(int));
	*callback_singleton = 0;

	cdp_avp = 0;
	/* load the CDP API */
	if(load_cdp_api(&cdpb) != 0) {
		LM_ERR("can't load CDP API\n");
		goto error;
	}

	cdp_avp = load_cdp_avp();
	if(!cdp_avp) {
		LM_ERR("can't load CDP_AVP API\n");
		goto error;
	}

	return 0;
error:
	LM_ERR("Failed to initialise ims_ocs module\n");
	return -1;
}

/**
 * Initializes the module in child.
 */
static int mod_child_init(int rank)
{
	LM_DBG("Initialization of module in child [%d] \n", rank);

	/* don't do anything for main process and TCP manager process */
	if(rank == PROC_MAIN || rank == PROC_TCP_MAIN) {
		return 0;
	}

	lock_get(process_lock);
	if((*callback_singleton) == 0) {
		*callback_singleton = 1;
		cdpb.AAAAddRequestHandler(callback_cdp_request, NULL);
	}
	lock_release(process_lock);

	return 0;
}


static void mod_destroy(void)
{
}

static int w_ccr_result(
		struct sip_msg *msg, char *result, char *grantedunits, char *final)
{
	str s_result_code, s_granted_units, s_final_unit;
	if(get_str_fparam(&s_result_code, msg, (fparam_t *)result) < 0) {
		LM_ERR("failed to get Result\n");
		return -1;
	}
	if(str2sint(&s_result_code, &result_code) != 0) {
		LM_DBG("Invalid result-code (%.*s)\n", s_result_code.len,
				s_result_code.s);
	}
	LM_DBG("Got result: %i (%.*s)\n", result_code, s_result_code.len,
			s_result_code.s);

	if(get_str_fparam(&s_granted_units, msg, (fparam_t *)grantedunits) < 0) {
		LM_ERR("failed to get Granted Units\n");
		return -1;
	}
	if(str2sint(&s_granted_units, &granted_units) != 0) {
		LM_DBG("Invalid Granted Units (%.*s)\n", s_granted_units.len,
				s_granted_units.s);
	}
	LM_DBG("Got Granted Units: %i, %.*s\n", granted_units, s_granted_units.len,
			s_granted_units.s);

	if(get_str_fparam(&s_final_unit, msg, (fparam_t *) final) < 0) {
		LM_ERR("failed to get Final Unit\n");
		return -1;
	}
	if(str2sint(&s_final_unit, &final_unit) != 0) {
		LM_DBG("Invalid Granted Units (%.*s)\n", s_final_unit.len,
				s_final_unit.s);
	}
	LM_DBG("Got Final Unit: %i, %.*s\n", final_unit, s_final_unit.len,
			s_final_unit.s);
	return 1;
}

AAAMessage *process_ccr(AAAMessage *ccr)
{
	int backup_rt;
	struct run_act_ctx ctx;
	struct sip_msg *msg;

	// Initialize values:
	result_code = 0;
	granted_units = 0;

	LM_DBG("Processing CCR");

	if((isOrig(ccr) != 0) && (event_route_ccr_term < 0)) {
		result_code = DIAMETER_SUCCESS;
		granted_units = 3600;
		final_unit = 0;
	} else {
		if(faked_aaa_msg(ccr, &msg) != 0) {
			LM_ERR("Failed to build Fake-Message\n");
		}

		backup_rt = get_route_type();
		set_route_type(REQUEST_ROUTE);
		init_run_actions_ctx(&ctx);
		if(isOrig(ccr) != 0) {
			run_top_route(event_rt.rlist[event_route_ccr_term], msg, 0);
		} else {
			run_top_route(event_rt.rlist[event_route_ccr_orig], msg, 0);
		}

		set_route_type(backup_rt);

		free_sip_msg(msg);
	}

	LM_DBG("Result-Code is %i, Granted Units %i (Final: %i)\n", result_code,
			granted_units, final_unit);

	if(result_code == 0) {
		LM_ERR("event_route did not set Result-Code, aborting\n");
		result_code = DIAMETER_UNABLE_TO_COMPLY;
		granted_units = 0;
		final_unit = 0;
	}

	AAAMessage *cca;
	cca = cdpb.AAACreateResponse(ccr);
	if(!cca)
		return 0;

	ocs_build_answer(ccr, cca, result_code, granted_units, final_unit);

	return cca;
}

/**
 * Handler for incoming Diameter requests.
 * @param request - the received request
 * @param param - generic pointer
 * @returns the answer to this request
 */
AAAMessage *callback_cdp_request(AAAMessage *request, void *param)
{
	if(is_req(request)) {

		switch(request->applicationId) {
			case IMS_Ro:
				switch(request->commandCode) {
					case IMS_CCR:
						return process_ccr(request);
						break;
					default:
						LM_ERR("Ro request handler(): - Received unknown "
							   "request for Ro command %d, flags %#1x endtoend "
							   "%u hopbyhop %u\n",
								request->commandCode, request->flags,
								request->endtoendId, request->hopbyhopId);
						return 0;
						break;
				}
				break;
			default:
				LM_ERR("Ro request handler(): - Received unknown request for "
					   "app %d command %d\n",
						request->applicationId, request->commandCode);
				return 0;
				break;
		}
	}
	return 0;
}

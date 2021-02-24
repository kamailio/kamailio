/*
 * Copyright (C) 2016-2017 ng-voice GmbH, carsten@ng-voice.com
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
#include "ims_diameter_server.h"
#include "avp_helper.h"
#include "../../core/fmsg.h"

MODULE_VERSION

extern gen_lock_t* process_lock; /* lock on the process table */

struct cdp_binds cdpb;
cdp_avp_bind_t *cdp_avp;

AAAMessage *request;
str responsejson;
str requestjson;

struct cdp_binds cdpb;

cdp_avp_bind_t *cdp_avp;

/** module functions */
static int mod_init(void);
static int mod_child_init(int);
static void mod_destroy(void);

int * callback_singleton; /*< Callback singleton */

int event_route_diameter = 0;
int event_route_diameter_response = 0;

static int diameter_request(struct sip_msg * msg, char* peer, char* appid, char* commandcode, char* message, int async);
static int w_diameter_request(struct sip_msg * msg, char* appid, char* commandcode, char* message);
static int w_diameter_request_peer(struct sip_msg *msg, char* peer, char* appid, char* commandcode, char* message);
static int w_diameter_request_async(struct sip_msg * msg, char* appid, char* commandcode, char* message);
static int w_diameter_request_peer_async(struct sip_msg *msg, char* peer, char* appid, char* commandcode, char* message);


static cmd_export_t cmds[] = {
	{"diameter_request", (cmd_function)w_diameter_request, 3, fixup_var_pve_str_12, 0, ANY_ROUTE},
	{"diameter_request", (cmd_function)w_diameter_request_peer, 4, fixup_var_pve_str_12, 0, ANY_ROUTE},
	{"diameter_request_async", (cmd_function)w_diameter_request_async, 3, fixup_var_pve_str_12, 0, ANY_ROUTE},
	{"diameter_request_async", (cmd_function)w_diameter_request_peer_async, 4, fixup_var_pve_str_12, 0, ANY_ROUTE},
	{ 0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
    { 0, 0, 0}
};

static pv_export_t mod_pvs[] = {
	{ {"diameter_command", sizeof("diameter_command")-1}, PVT_OTHER, pv_get_command, 0, 0, 0, 0, 0 },
	{ {"diameter_application", sizeof("diameter_application")-1}, PVT_OTHER, pv_get_application, 0, 0, 0, 0, 0 },
	{ {"diameter_request", sizeof("diameter_request")-1}, PVT_OTHER, pv_get_request, 0, 0, 0, 0, 0 },
	{ {"diameter_response", sizeof("diameter_response")-1}, PVT_OTHER, pv_get_response, pv_set_response, 0, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

/** module exports */
struct module_exports exports = {
	"ims_diameter_server", 
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds, 		 /* Exported functions */
	params,  	 /* exported statistics */
	0, 		 /* exported RPC methods */
	mod_pvs, 	 /* exported pseudo-variables */
	0, 		 /* response handling function */
	mod_init, 	 /* module initialization function */
	mod_child_init,  /* per-child init function */
	mod_destroy
};

/**
 * init module function
 */
static int mod_init(void) {
	LM_DBG("Loading...\n");

	request = 0;
	responsejson.s = 0;
	responsejson.len = 0;
	requestjson.s = 0;
	requestjson.len = 0;

	callback_singleton = shm_malloc(sizeof (int));
	*callback_singleton = 0;

	cdp_avp = 0;
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

	event_route_diameter = route_get(&event_rt, "diameter:request");
	if (event_route_diameter < 0) {
		LM_ERR("No diameter:request event route found\n");
		goto error;
	}
	LM_DBG("Found Route diameter:request: %i\n", event_route_diameter);

	event_route_diameter_response = route_get(&event_rt, "diameter:response");
	if (event_route_diameter_response < 0) {
		LM_WARN("No diameter:response event route found, asynchronous operations disabled.\n");
	} else {
		LM_DBG("Found Route diameter:response: %i\n", event_route_diameter_response);
	}

	return 0;
error:
	LM_ERR("Failed to initialise ims_diameter_server module\n");
	return -1;
}

/**
 * Initializes the module in child.
 */
static int mod_child_init(int rank) {
    LM_DBG("Initialization of module in child [%d] \n", rank);

    /* don't do anything for main process and TCP manager process */
    if (rank == PROC_MAIN || rank == PROC_TCP_MAIN) {
        return 0;
    }

    lock_get(process_lock);
    if ((*callback_singleton) == 0) {
        *callback_singleton = 1;
        cdpb.AAAAddRequestHandler(callback_cdp_request, NULL);
    }
    lock_release(process_lock);

    return 0;
}


static void mod_destroy(void) {

}

/**
 * Handler for incoming Diameter requests.
 * @param request - the received request
 * @param param - generic pointer
 * @returns the answer to this request
 */
AAAMessage* callback_cdp_request(AAAMessage *request_in, void *param) {
	struct sip_msg *fmsg;
	int backup_rt;
	struct run_act_ctx ctx;
	AAAMessage *response;

	LM_DBG("Got DIAMETER-Request!\n");

	if (is_req(request_in)) {
		LM_DBG("is request!\n");
		LM_DBG("Found Route diameter:request: %i\n", event_route_diameter);

		request = request_in;
		response = cdpb.AAACreateResponse(request_in);
		if (!response) return 0;

		backup_rt = get_route_type();
		set_route_type(REQUEST_ROUTE);
		init_run_actions_ctx(&ctx);
		fmsg = faked_msg_next();
		responsejson.s = 0;
		responsejson.len = 0;

		run_top_route(event_rt.rlist[event_route_diameter], fmsg, &ctx);

		set_route_type(backup_rt);
		LM_DBG("Processed Event-Route!\n");

		if (addAVPsfromJSON(response, NULL)) {
			return response;
		} else {
			return 0;
		}
	}
	return 0;
}

int w_diameter_request(struct sip_msg * msg, char* appid, char* commandcode, char* message) {
	return diameter_request(msg, 0, appid, commandcode, message, 0);
}

static int w_diameter_request_peer(struct sip_msg *msg, char* peer, char* appid, char* commandcode, char* message) {
	return diameter_request(msg, peer, appid, commandcode, message, 0);
}

static int w_diameter_request_async(struct sip_msg * msg, char* appid, char* commandcode, char* message) {
	return diameter_request(msg, 0, appid, commandcode, message, 1);
}

static int w_diameter_request_peer_async(struct sip_msg *msg, char* peer, char* appid, char* commandcode, char* message) {
	return diameter_request(msg, peer, appid, commandcode, message, 1);
}

void async_cdp_diameter_callback(int is_timeout, void *request, AAAMessage *response, long elapsed_msecs) {
	struct sip_msg *fmsg;
	int backup_rt;
	struct run_act_ctx ctx;

	if (is_timeout != 0) {
		LM_ERR("Error timeout when sending message via CDP\n");
		goto error;
	}

	if (!response) {
		LM_ERR("Error sending message via CDP\n");
		goto error;
	}
	if (AAAmsg2json(response, &responsejson) != 1) {
		LM_ERR("Failed to convert response to JSON\n");
	}
	request = (AAAMessage*)request;

	backup_rt = get_route_type();
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);
	fmsg = faked_msg_next();

	run_top_route(event_rt.rlist[event_route_diameter_response], fmsg, &ctx);

	set_route_type(backup_rt);
	LM_DBG("Processed Event-Route!\n");
error:
	//free memory
	if (response) cdpb.AAAFreeMessage(&response);
}


int diameter_request(struct sip_msg * msg, char* peer, char* appid, char* commandcode, char* message, int async) {
	str s_appid, s_commandcode, s_peer, s_message;
	AAAMessage *req = 0;
	AAASession *session = 0;
	AAAMessage *resp = 0;

	unsigned int i_appid, i_commandcode;

	if (async && (event_route_diameter_response < 0)) {
		LM_ERR("Asynchronous operations disabled\n");
		return -1;
	}

	if (peer) {
		if (get_str_fparam(&s_peer, msg, (fparam_t*)peer) < 0) {
		    LM_ERR("failed to get Peer\n");
		    return -1;
		}
		LM_DBG("Peer %.*s\n", s_peer.len, s_peer.s);
	}
	if (get_str_fparam(&s_appid, msg, (fparam_t*)appid) < 0) {
		LM_ERR("failed to get App-ID\n");
		return -1;
	}
	if (str2int(&s_appid, &i_appid) != 0) {
		LM_ERR("Invalid App-ID (%.*s)\n", s_appid.len, s_appid.s);
		return -1;
	}
	LM_DBG("App-ID %i\n", i_appid);
	if (get_str_fparam(&s_commandcode, msg, (fparam_t*)commandcode) < 0) {
		LM_ERR("failed to get Command-Code\n");
		return -1;
	}
	if (str2int(&s_commandcode, &i_commandcode) != 0) {
		LM_ERR("Invalid Command-Code (%.*s)\n", s_commandcode.len, s_commandcode.s);
		return -1;
	}
	LM_DBG("Command-Code %i\n", i_commandcode);
	if (get_str_fparam(&s_commandcode, msg, (fparam_t*)commandcode) < 0) {
		LM_ERR("failed to get Command-Code\n");
		return -1;
	}

	session = cdpb.AAACreateSession(0);

	req = cdpb.AAACreateRequest(i_appid, i_commandcode, Flag_Proxyable, session);
	if (!req) goto error1;

	if (addAVPsfromJSON(req, &s_message)) {
		LM_ERR("Failed to parse JSON Request\n");
		return -1;
	}


	if (peer && (s_peer.len > 0)) {
		if (async) {
			cdpb.AAASendMessageToPeer(req, &s_peer, (void*) async_cdp_diameter_callback, req);
			LM_DBG("Successfully sent async diameter\n");
			return 0;
		} else {
			resp = cdpb.AAASendRecvMessageToPeer(req, &s_peer);
			LM_DBG("Successfully sent diameter\n");
			if (resp && AAAmsg2json(resp, &responsejson) == 1) {
				return 1;
			} else {
				LM_ERR("Failed to convert response to JSON\n");
				return -1;
			}
		}
	} else {
		if (async) {
			cdpb.AAASendMessage(req, (void*) async_cdp_diameter_callback, req);
			LM_DBG("Successfully sent async diameter\n");
			return 0;
		} else {
			resp = cdpb.AAASendRecvMessage(req);
			LM_DBG("Successfully sent diameter\n");
			if (resp && AAAmsg2json(resp, &responsejson) == 1) {
				return 1;
			} else {
				LM_ERR("Failed to convert response to JSON\n");
				return -1;
			}
		}
	}
error1:
	//Only free UAR IFF it has not been passed to CDP
	if (req) cdpb.AAAFreeMessage(&req);
	LM_ERR("Error occurred trying to send request\n");
	return -1;
}



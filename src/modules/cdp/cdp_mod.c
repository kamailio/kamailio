/*
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

#include "cdp_mod.h"

#include "../../core/sr_module.h"
#include "../../core/globals.h"

#include "diameter_peer.h"
#include "config.h"
#include "cdp_load.h"
#include "cdp_rpc.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/cfg/cfg_struct.h"
#include "cdp_stats.h"
#include "cdp_functions.h"
#include "../../core/mod_fix.h"

MODULE_VERSION

char *config_file =
		"DiameterPeer.xml"; /**< default DiameterPeer configuration filename */
unsigned int latency_threshold =
		500; /**< default threshold for Diameter calls (ms) */
unsigned int *latency_threshold_p = &latency_threshold;
unsigned int workerq_latency_threshold =
		100; /**< default threshold for putting a task into worker queue (ms) */
unsigned int workerq_length_threshold_percentage =
		0; /**< default threshold for worker queue length, percentage of max queue length - by default disabled */
unsigned int debug_heavy = 0;

extern dp_config *config; /**< DiameterPeer configuration structure */

static int w_cdp_check_peer(sip_msg_t *msg, char *peer, char *p2);
static int w_cdp_has_app(sip_msg_t *msg, char *appid, char *param);
static int w_cdp_has_app2(sip_msg_t *msg, char *vendor, char *appid);
static int w_cdp_has_app(sip_msg_t *msg, char *appid, char *param);


#define EXP_FUNC(NAME) {#NAME, (cmd_function)NAME, NO_SCRIPT, 0, 0},
/**
 * Exported functions. This is the API available for use from other SER modules.
 * If you require more, please add them here.
 * <p>
 * - load_cdp() - find and load the CDiameterPeer function bindings
 * <p>
 * - AAACreateRequest() - create a diameter request #AAAMessage
 * - AAACreateResponse() - create a diameter response #AAAMessage
 * - AAAFreeMessage() - free up the memory used in a Diameter message
 * <p>
 * - AAASendMessage() - asynchronously send a message
 * - AAASendMessageToPeer() - asynchronously send a message to a forced peer
 * - AAASendRecvMessage() - synchronously send a message and get the response
 * - AAASendRecvMessageToPeer() - synchronously send a message and get the response to a forced peer
 * <p>
 * - AAACreateSession() - create a diameter #AAASessionId
 * - AAADropSession() - drop a diameter #AAASessionId
 * <p>
 * - AAACreateTransaction() - create a diameter #AAATransaction
 * - AAADropTransaction() - drop a diameter #AAATransaction
 * <p>
 * - AAACreateAVP() - create an #AAA_AVP
 * - AAAAddAVPToMessage() - add an #AAA_AVP to a #AAAMessage
 * - AAAFindMatchingAVP() - find an #AAA_AVP inside a #AAAMessage
 * - AAAGetNextAVP() - get the next #AAA_AVP from the #AAAMessage
 * - AAAFreeAVP() - free the memory taken by the #AAA_AVP
 * - AAAGroupAVPS() - group a #AAA_AVP_LIST of #AAA_AVP into a grouped #AAA_AVP
 * - AAAUngroupAVPS() - ungroup a grouped #AAA_AVP into a #AAA_AVP_LIST of #AAA_AVP
 * - AAAFindMatchingAVPList() - find an #AAA_AVP inside a #AAA_AVP_LIST
 * - AAAFreeAVPList() - free the memory taken by all the members of #AAA_AVP_LIST
 * <p>
 * - AAAAddRequestHandler() - add a #AAARequestHandler_f callback to request being received
 * - AAAAddResponseHandler() - add a #AAAResponseHandler_f callback to responses being received
 */
static cmd_export_t cdp_cmds[] = {
		{"cdp_check_peer", (cmd_function)w_cdp_check_peer, 1, fixup_spve_null,
				0, ANY_ROUTE},
		{"cdp_has_app", (cmd_function)w_cdp_has_app, 1, fixup_igp_null, 0,
				ANY_ROUTE},
		{"cdp_has_app", (cmd_function)w_cdp_has_app2, 2, fixup_igp_igp, 0,
				ANY_ROUTE},
		{"load_cdp", (cmd_function)load_cdp, NO_SCRIPT, 0, 0},

		EXP_FUNC(AAACreateRequest) EXP_FUNC(AAACreateResponse) EXP_FUNC(
				AAAFreeMessage)


				EXP_FUNC(AAACreateAVP) EXP_FUNC(AAAAddAVPToMessage) EXP_FUNC(
						AAAAddAVPToList) EXP_FUNC(AAAFindMatchingAVP) EXP_FUNC(AAAFindMatchingAVPList)
						EXP_FUNC(AAAGetNextAVP) EXP_FUNC(AAAFreeAVP) EXP_FUNC(
								AAAFreeAVPList) EXP_FUNC(AAAGroupAVPS) EXP_FUNC(AAAUngroupAVPS)

								EXP_FUNC(AAASendMessage) EXP_FUNC(AAASendMessageToPeer) EXP_FUNC(
										AAASendRecvMessage) EXP_FUNC(AAASendRecvMessageToPeer)


										EXP_FUNC(AAAAddRequestHandler) EXP_FUNC(
												AAAAddResponseHandler)


												EXP_FUNC(AAACreateTransaction) EXP_FUNC(
														AAADropTransaction)


														EXP_FUNC(AAACreateSession) EXP_FUNC(
																AAAMakeSession) EXP_FUNC(AAAGetSession)
																EXP_FUNC(AAADropSession) EXP_FUNC(
																		AAASessionsLock) EXP_FUNC(AAASessionsUnlock)

																		EXP_FUNC(AAACreateClientAuthSession) EXP_FUNC(
																				AAACreateServerAuthSession)
																				EXP_FUNC(AAAGetAuthSession) EXP_FUNC(
																						AAADropAuthSession) EXP_FUNC(AAATerminateAuthSession)

																						EXP_FUNC(AAACreateCCAccSession) EXP_FUNC(
																								AAAStartChargingCCAccSession)
																								EXP_FUNC(AAAGetCCAccSession) EXP_FUNC(
																										AAADropCCAccSession) EXP_FUNC(AAATerminateCCAccSession)

																										{0, 0, 0,
																												0,
																												0}};


/**
 * Exported parameters.
 * - config_file - Configuration filename. See configdtd.h for the structure and ConfigExample.xml.
 */
static param_export_t cdp_params[] = {
		{"config_file", PARAM_STRING,
				&config_file}, /**< configuration filename */
		{"latency_threshold", PARAM_INT,
				&latency_threshold}, /**<threshold above which we will log*/
		{"workerq_latency_threshold", PARAM_INT,
				&workerq_latency_threshold}, /**<time threshold putting job into queue*/
		{"workerq_length_threshold_percentage", PARAM_INT,
				&workerq_length_threshold_percentage}, /**<queue length threshold - percentage of max queue length*/
		{"debug_heavy", PARAM_INT, &debug_heavy}, {0, 0, 0}};

/**
 * Exported module interface
 */
struct module_exports exports = {"cdp", DEFAULT_DLFLAGS,
		cdp_cmds,		/**< Exported functions */
		cdp_params,		/**< Exported parameters */
		0,				/**< RPC cmds */
		0,				/**< pseudovariables */
		0, cdp_init,	/**< Module initialization function */
		cdp_child_init, /**< per-child init function */
		cdp_exit};

/**
 * Module init function.
 *
 * - Initializes the diameter peer using the provided configuration file.
 * - Registers with pt the required number of processes.
 */
static int cdp_init(void)
{
	if(rpc_register_array(cdp_rpc) != 0) {
		LM_ERR("failed to register RPC commands for CDP module\n");
		return -1;
	}

	if(cdp_init_counters() != 0) {
		LM_ERR("Failed to register counters for CDP modules\n");
		return -1;
	}

	if(!diameter_peer_init(config_file)) {
		LM_ERR("error initializing the diameter peer\n");
		return 1;
	}
	register_procs(2 + config->workers + 2 * config->peers_cnt);
	cfg_register_child(2 + config->workers + 2 * config->peers_cnt);
	return 0;
}

/**
 *	Child init function.
 * - starts the DiameterPeer by forking the processes
 * @param rank - id of the child
 */
static int cdp_child_init(int rank)
{
	if(rank == PROC_MAIN) {
		LM_INFO("CDiameterPeer child starting ...\n");
		diameter_peer_start(0);
		LM_INFO("... CDiameterPeer child started\n");
	}

	return 0;
}


/**
 *	Module termination function.
 * - stop the DiameterPeer processes in a civilized manner
 */
static void cdp_exit(void)
{
	LM_INFO("CDiameterPeer child stopping ...\n");
	diameter_peer_destroy();
	LM_INFO("... CDiameterPeer child stopped\n");
	return;
}

int w_cdp_check_peer(sip_msg_t *msg, char *peer, char *p2)
{
	str s;
	if(fixup_get_svalue(msg, (gparam_p)peer, &s) < 0) {
		LM_ERR("cannot get the peer\n");
		return -1;
	}
	if(s.len > 0) {
		return check_peer(&s);
	}
	return -1;
}

static int w_cdp_has_app(sip_msg_t *msg, char *appid, char *param)
{
	unsigned int app_flags;
	str app_s = STR_NULL;
	int a;
	if(msg == NULL)
		return -1;

	if(get_is_fparam(&a, &app_s, msg, (fparam_t *)appid, &app_flags) != 0) {
		LM_ERR("no Vendor-ID\n");
		return -1;
	}
	if(!(app_flags & PARAM_INT)) {
		if(app_flags & PARAM_STR)
			LM_ERR("unable to get app from [%.*s]\n", app_s.len, app_s.s);
		else
			LM_ERR("unable to get app\n");
		return -1;
	}
	return check_application(-1, a);
}

static int w_cdp_has_app2(sip_msg_t *msg, char *vendor, char *appid)
{
	unsigned int vendor_flags, app_flags;
	str vendor_s = STR_NULL;
	str app_s = STR_NULL;
	int v, a;
	if(msg == NULL)
		return -1;

	if(get_is_fparam(&v, &vendor_s, msg, (fparam_t *)vendor, &vendor_flags)
			!= 0) {
		LM_ERR("no Vendor-ID\n");
		return -1;
	}
	if(!(vendor_flags & PARAM_INT)) {
		if(vendor_flags & PARAM_STR)
			LM_ERR("unable to get vendor from [%.*s]\n", vendor_s.len,
					vendor_s.s);
		else
			LM_ERR("unable to get vendor\n");
		return -1;
	}
	if(get_is_fparam(&a, &app_s, msg, (fparam_t *)appid, &app_flags) != 0) {
		LM_ERR("no Vendor-ID\n");
		return -1;
	}
	if(!(app_flags & PARAM_INT)) {
		if(app_flags & PARAM_STR)
			LM_ERR("unable to get app from [%.*s]\n", app_s.len, app_s.s);
		else
			LM_ERR("unable to get app\n");
		return -1;
	}
	return check_application(v, a);
}

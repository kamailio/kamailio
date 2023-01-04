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
#include <stdlib.h>
#include <unistd.h>
#include "../../core/sr_module.h"
#include "../../core/events.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../lib/ims/ims_getters.h"
#include "../tm/tm_load.h"
#include "../../core/mod_fix.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_content.h"
#include "../ims_usrloc_pcscf/usrloc.h"
#include "../../modules/ims_dialog/dlg_load.h"
#include "../../modules/ims_dialog/dlg_hash.h"
#include "../cdp/cdp_load.h"
#include "../cdp_avp/cdp_avp_mod.h"
#include "../../core/cfg/cfg_struct.h"
#include "cdpeventprocessor.h"
#include "rx_authdata.h"
#include "rx_asr.h"
#include "rx_str.h"
#include "rx_aar.h"
#include "ims_qos_mod.h"
#include "../../core/parser/sdp/sdp.h"

#include "../../lib/ims/useful_defs.h"
#include "ims_qos_stats.h"

MODULE_VERSION

extern gen_lock_t* process_lock; /* lock on the process table */

stat_var* stat_aar_timeouts;
stat_var* aar_replies_received;
stat_var* aar_replies_response_time;

str orig_session_key = str_init("originating");
str term_session_key = str_init("terminating");

int rx_auth_expiry = 7200;

int must_send_str = 1;

int authorize_video_flow = 1; //by default we authorize resources for video flow descriptions

struct tm_binds tmb;
struct cdp_binds cdpb;
ims_dlg_api_t dlgb;
bind_usrloc_t bind_usrloc;
cdp_avp_bind_t *cdp_avp;
usrloc_api_t ul;

int cdp_event_latency = 1; /*flag: report slow processing of CDP callback events or not - default enabled */
int cdp_event_threshold = 500; /*time in ms above which we should report slow processing of CDP callback event - default 500ms*/
int cdp_event_latency_loglevel = 0; /*log-level to use to report slow processing of CDP callback event - default ERROR*/

int audio_default_bandwidth = 64;
int video_default_bandwidth = 128;

//If set then any IP in SDP that does not match this regex then filter is added with any in flow description AVP
//Very useful for UEs that change ports mid way through call therefore breaking flow description filters
str regex_sdp_ip_prefix_to_maintain_in_fd = {0, 0};

//If set this will include an additional filter for all existing filters using the next odd port up - as this is the RTCP port
int include_rtcp_fd = 0;

int cdp_event_list_size_threshold = 0; /**Threshold for size of cdp event list after which a warning is logged */

stat_var *aars;
stat_var *strs;
stat_var *asrs;
stat_var *successful_aars;
stat_var *successful_strs;

static str identifier = {0, 0};
static int identifier_size = 0;

/** module functions */
static int mod_init(void);
static int mod_child_init(int);
static void mod_destroy(void);

static int fixup_aar_register(void** param, int param_no);
static int fixup_aar(void** param, int param_no);

int * callback_singleton; /*< Callback singleton */

int terminate_dialog_on_rx_failure = 1; //this specifies whether a dialog is torn down when a media rx session fails - in some cases you might not want the dialog torn down
int delete_contact_on_rx_failure = 1;  //If this is set we delete the contact if the associated signalling bearer is removed


str early_qosrelease_reason = {"QoS released", 12};
str confirmed_qosrelease_headers = {NULL, 0};


/* parameters storage */
str rx_dest_realm = str_init("ims.smilecoms.com");
/* Only used if we want to force the Rx peer usually this is configured at a stack level and the first request uses realm routing */
str rx_forced_peer = str_init("");

/* P-CSCF IP address to generate the flows for the UE<->PCSCF signaling path */
str af_signaling_ip = str_init("127.0.0.1");
/* P-CSCF IPv6 address to generate the flows for the UE<->PCSCF signaling path */
str af_signaling_ip6 = str_init("");

str component_media_type = str_init("control");
str flow_protocol = str_init("IP");
int omit_flow_ports = 0;
int rs_default_bandwidth = 0;
int rr_default_bandwidth = 0;

/* commands wrappers and fixups */
static int w_rx_aar(struct sip_msg *msg, char *route, char* dir, char *id, int id_type);
static int w_rx_aar_register(struct sip_msg *msg, char *route, char* str1, char *bar);

struct _pv_req_data {
		struct cell *T;
		struct sip_msg msg;
		struct sip_msg *tmsgp;
		char *buf;
		int buf_size;
  msg_ctx_id_t msg_ctx;
};

static struct _pv_req_data _pv_treq;

static void pv_tmx_data_init(void)
{
		memset(&_pv_treq, 0, sizeof(struct _pv_req_data));
}

static int pv_t_copy_msg(struct sip_msg *src, struct sip_msg *dst)
{
		dst->id = src->id;
		dst->rcv = src->rcv;
		dst->set_global_address = src->set_global_address;
		dst->set_global_port = src->set_global_port;
		dst->flags = src->flags;
		dst->fwd_send_flags = src->fwd_send_flags;
		dst->rpl_send_flags = src->rpl_send_flags;
		dst->force_send_socket = src->force_send_socket;

		if (parse_msg(dst->buf, dst->len, dst) != 0) {
				LM_ERR("parse msg failed\n");
				return -1;
		}
		return 0;
}


static cmd_export_t cmds[] = {
		{ "Rx_AAR", (cmd_function) w_rx_aar, 4, fixup_aar, 0, REQUEST_ROUTE | ONREPLY_ROUTE},
		{ "Rx_AAR_Register", (cmd_function) w_rx_aar_register, 2, fixup_aar_register, 0, REQUEST_ROUTE},
		{ 0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
		{ "rx_dest_realm", PARAM_STR, &rx_dest_realm},
		{ "rx_forced_peer", PARAM_STR, &rx_forced_peer},
		{ "rx_auth_expiry", INT_PARAM, &rx_auth_expiry},
		{ "af_signaling_ip", PARAM_STR, &af_signaling_ip}, /* IP of this P-CSCF, to be used in the flow for the AF-signaling */
		{ "af_signaling_ip6", PARAM_STR, &af_signaling_ip6}, /* IPv6 of this P-CSCF, to be used in the flow for the AF-signaling */
		{ "media_type", PARAM_STR, &component_media_type}, /*  */
		{ "flow_protocol", PARAM_STR, &flow_protocol}, /*  */
		{ "omit_flow_ports", INT_PARAM, &omit_flow_ports}, /*  */
		{ "rs_default_bandwidth", INT_PARAM, &rs_default_bandwidth}, /*  */
		{ "rr_default_bandwidth", INT_PARAM, &rr_default_bandwidth}, /*  */
		{ "cdp_event_latency", INT_PARAM, &cdp_event_latency}, /*flag: report slow processing of CDP callback events or not */
		{ "cdp_event_threshold", INT_PARAM, &cdp_event_threshold}, /*time in ms above which we should report slow processing of CDP callback event*/
		{ "cdp_event_latency_log", INT_PARAM, &cdp_event_latency_loglevel}, /*log-level to use to report slow processing of CDP callback event*/
		{ "authorize_video_flow", INT_PARAM, &authorize_video_flow}, /*whether or not we authorize resources for video flows*/
		{ "cdp_event_list_size_threshold", INT_PARAM, &cdp_event_list_size_threshold}, /**Threshold for size of cdp event list after which a warning is logged */
		{ "audio_default_bandwidth", INT_PARAM, &audio_default_bandwidth},
		{ "video_default_bandwidth", INT_PARAM, &video_default_bandwidth},
		{ "early_qosrelease_reason", PARAM_STR, &early_qosrelease_reason},
		{ "confirmed_qosrelease_headers", PARAM_STR, &confirmed_qosrelease_headers},
		{ "terminate_dialog_on_rx_failure", INT_PARAM, &terminate_dialog_on_rx_failure},
		{ "delete_contact_on_rx_failure", INT_PARAM, &delete_contact_on_rx_failure},
		{ "regex_sdp_ip_prefix_to_maintain_in_fd", PARAM_STR, &regex_sdp_ip_prefix_to_maintain_in_fd},
		{ "include_rtcp_fd", INT_PARAM, &include_rtcp_fd},
		{ 0, 0, 0}
};


/** module exports */
struct module_exports exports = {
	"ims_qos", 
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds, 		 /* Exported functions */
	params, 
	0, 		 /* exported RPC methods */
	0, 		 /* exported pseudo-variables */
	0, 		 /* response handling function */
	mod_init, 	 /* module initialization function */
	mod_child_init,	 /* per-child init function */
	mod_destroy
};

/**
 * init module function
 */
static int mod_init(void)
{

		callback_singleton = shm_malloc(sizeof(int));
		*callback_singleton = 0;

		/*register space for event processor*/
		register_procs(1);

		cdp_avp = 0;
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

		/* load the dialog API */
		if (load_ims_dlg_api(&dlgb) != 0) {
				LM_ERR("can't load Dialog API\n");
				goto error;
		}

		cdp_avp = load_cdp_avp();
		if (!cdp_avp) {
				LM_ERR("can't load CDP_AVP API\n");
				goto error;
		}

		/* load the usrloc API */
		bind_usrloc = (bind_usrloc_t) find_export("ul_bind_ims_usrloc_pcscf", 1, 0);
		if (!bind_usrloc) {
				LM_ERR("can't bind usrloc_pcscf\n");
				return CSCF_RETURN_FALSE;
		}

		if (bind_usrloc(&ul) < 0) {
				LM_ERR("can't bind to usrloc pcscf\n");
				return CSCF_RETURN_FALSE;
		}
		LM_DBG("Successfully bound to PCSCF Usrloc module\n");

		LM_DBG("Diameter RX interface successfully bound to TM, Dialog, Usrloc and CDP modules\n");

		/*init cdb cb event list*/
		if (!init_cdp_cb_event_list()) {
				LM_ERR("unable to initialise cdp callback event list\n");
				return -1;
		}

		if (ims_qos_init_counters() != 0) {
				LM_ERR("Failed to register counters for ims_qos module\n");
				return -1;
		}

		pv_tmx_data_init();

		return 0;
error:
		LM_ERR("Failed to initialise ims_qos module\n");
		return CSCF_RETURN_FALSE;
}

/**
 * Initializes the module in child.
 */
static int mod_child_init(int rank)
{
		LM_DBG("Initialization of module in child [%d] \n", rank);

		if (rank == PROC_MAIN) {
				int pid = fork_process(PROC_MIN, "Rx Event Processor", 1);
				if (pid < 0)
						return -1; //error
				if (pid == 0) {
						if (cfg_child_init())
								return -1; //error
						cdp_cb_event_process();
				}
		}
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

static void mod_destroy(void)
{
		if (identifier_size > 0 && identifier.s) {
				pkg_free(identifier.s);
		}
}

/*callback of CDP session*/
void callback_for_cdp_session(int event, void *session)
{
		rx_authsessiondata_t* p_session_data = 0;
		AAASession *x = session;

		str* rx_session_id = (str*) & x->id;
		p_session_data = (rx_authsessiondata_t*) x->u.auth.generic_data;

		if (!rx_session_id || rx_session_id->len <= 0 || !rx_session_id->s) {
				LM_ERR("Invalid Rx session id");
				return;
		}

		if (!p_session_data) {
				LM_ERR("Invalid associated session data\n");
				return;
		}

		//only put the events we care about on the event stack
		if (event == AUTH_EV_SESSION_TIMEOUT ||
				event == AUTH_EV_SESSION_GRACE_TIMEOUT ||
				event == AUTH_EV_RECV_ASR ||
				event == AUTH_EV_SERVICE_TERMINATED) {

				LOG(L_DBG, "callback_for_cdp session(): called with event %d and session id [%.*s]\n", event, rx_session_id->len, rx_session_id->s);

				//create new event to process async
				cdp_cb_event_t *new_event = new_cdp_cb_event(event, rx_session_id, p_session_data);
				if (!new_event) {
						LM_ERR("Unable to create event for cdp callback\n");
						return;
				}
				//push the new event onto the stack (FIFO)
				push_cdp_cb_event(new_event);
		} else {
				LM_DBG("Ignoring event [%d] from CDP session\n", event);
		}
}

/**
 * Handler for incoming Diameter requests.
 * @param request - the received request
 * @param param - generic pointer
 * @returns the answer to this request
 */
AAAMessage* callback_cdp_request(AAAMessage *request, void *param)
{
		if (is_req(request)) {

				switch (request->applicationId) {
				case IMS_Rx:
				case IMS_Gq:
						switch (request->commandCode) {
						case IMS_RAR:
								LM_INFO("Rx request handler():- Received an IMS_RAR \n");
								/* TODO: Add support for Re-Auth Requests */
								return 0;
								break;
						case IMS_ASR:
								LM_INFO("Rx request handler(): - Received an IMS_ASR \n");
								return rx_process_asr(request);
								break;
						default:
								LM_ERR("Rx request handler(): - Received unknown request for Rx/Gq command %d, flags %#1x endtoend %u hopbyhop %u\n", request->commandCode, request->flags, request->endtoendId, request->hopbyhopId);
								return 0;
								break;
						}
						break;
				default:
						LM_ERR("Rx request handler(): - Received unknown request for app %d command %d\n", request->applicationId, request->commandCode);
						return 0;
						break;
				}
		}
		return 0;
}

const str match_cseq_method = {"INVITE", 6};

void callback_dialog(struct dlg_cell* dlg, int type, struct dlg_cb_params * params)
{

		struct sip_msg* msg = params->rpl;
		struct cseq_body *parsed_cseq;
		str *rx_session_id;
		rx_session_id = (str*) * params->param;
		AAASession *auth = 0;
		rx_authsessiondata_t* p_session_data = 0;
		flow_description_t *current_fd = 0;
		flow_description_t *new_fd = 0;
		int current_has_video = 0;
		int new_has_video = 0;
		int must_unlock_aaa = 1;

		if (rx_session_id == 0) {
				LM_WARN("Strange... no rx session ID in callback.... why?\n");
				return;
		}
		//getting session data

		LM_DBG("Dialog callback of type %d received\n", type);

		if (type == DLGCB_TERMINATED || type == DLGCB_DESTROY || type == DLGCB_EXPIRED || type == DLGCB_FAILED) {
				LM_DBG("Dialog has ended or failed - we need to terminate Rx bearer session\n");

				LM_DBG("Received notification of termination of dialog with Rx session ID: [%.*s]\n",
						rx_session_id->len, rx_session_id->s);

				LM_DBG("Sending STR\n");
				rx_send_str(rx_session_id);
		} else if (type == DLGCB_CONFIRMED) {

				LM_DBG("Callback for confirmed dialog - copy new flow description into current flow description\n");
				if (!rx_session_id || !rx_session_id->s || !rx_session_id->len) {
						LM_ERR("Dialog has no Rx session associated\n");
						goto error;
				}

				//getting auth session
				auth = cdpb.AAAGetAuthSession(*rx_session_id);
				if (!auth) {
						LM_DBG("Could not get Auth Session for session id: [%.*s]\n", rx_session_id->len, rx_session_id->s);
						goto error;
				}

				//getting session data
				p_session_data = (rx_authsessiondata_t*) auth->u.auth.generic_data;
				if (!p_session_data) {
						LM_DBG("Could not get session data on Auth Session for session id: [%.*s]\n", rx_session_id->len, rx_session_id->s);
						goto error;
				}

				//check if there is a new flow description - if there is then free the current flow description and replace it with the new flow description
				if (p_session_data->first_new_flow_description) {
						//free the current
						LM_DBG("Free-ing the current fd\n");
						free_flow_description(p_session_data, 1);
						//point the current to the new
						LM_DBG("Point the first current fd to the first new fd\n");
						p_session_data->first_current_flow_description = p_session_data->first_new_flow_description;
						//point the new to 0
						LM_DBG("Point the first new fd to 0\n");
						p_session_data->first_new_flow_description = 0;
				} else {
						LM_ERR("There is no new flow description - this shouldn't happen\n");
				}

				show_callsessiondata(p_session_data);
				if (auth) cdpb.AAASessionsUnlock(auth->hash);

		} else if (type == DLGCB_RESPONSE_WITHIN) {

				LM_DBG("Dialog has received a response to a request within dialog\n");
				if (!rx_session_id || !rx_session_id->s || !rx_session_id->len) {
						LM_ERR("Dialog has no Rx session associated\n");
						goto error;
				}

				//getting auth session
				auth = cdpb.AAAGetAuthSession(*rx_session_id);
				if (!auth) {
						LM_DBG("Could not get Auth Session for session id: [%.*s]\n", rx_session_id->len, rx_session_id->s);
						goto error;
				}

				//getting session data
				p_session_data = (rx_authsessiondata_t*) auth->u.auth.generic_data;

				if (!p_session_data) {
						LM_DBG("Could not get session data on Auth Session for session id: [%.*s]\n", rx_session_id->len, rx_session_id->s);
						goto error;
				}

				show_callsessiondata(p_session_data);

				if (msg->first_line.type == SIP_REPLY) {
						LM_DBG("This is a SIP REPLY\n");
						if (msg->cseq && (parsed_cseq = get_cseq(msg)) && memcmp(parsed_cseq->method.s, match_cseq_method.s, match_cseq_method.len) == 0) {
								LM_DBG("This response has a cseq method [%.*s]\n", match_cseq_method.len, match_cseq_method.s);

								if (msg->first_line.u.reply.statuscode == 200) {
										LM_DBG("Response is 200 - this is success\n");
										//check if there is a new flow description - if there is then free the current flow description and replace it with the new flow description
										if (p_session_data->first_new_flow_description) {
												//free the current
												free_flow_description(p_session_data, 1);
												//point the current to the new
												p_session_data->first_current_flow_description = p_session_data->first_new_flow_description;
												//point the new to 0
												p_session_data->first_new_flow_description = 0;
										} else {
												LM_DBG("There is no new flow description - duplicate dialog callback - we ignore.\n");
										}
								} else if (msg->first_line.u.reply.statuscode > 299) {
										LM_DBG("Response is more than 299 so this is an error code\n");

										new_fd = p_session_data->first_new_flow_description;
										//check if there is video in the new flow description
										while (new_fd) {
												if (strncmp(new_fd->media.s, "video", 5) == 0) {
														LM_DBG("The new flow has a video fd in it\n");
														new_has_video = 1;

												}
												new_fd = new_fd->next;
										}
										//check if there is video in the current flow description
										current_fd = p_session_data->first_current_flow_description;
										while (current_fd) {
												if (strncmp(current_fd->media.s, "video", 5) == 0) {
														LM_DBG("The current flow has a video fd in it\n");
														current_has_video = 1;

												}
												current_fd = current_fd->next;
										}
										if (new_has_video && !current_has_video) {
												LM_DBG("New flow description has video in it, and current does not - this means we added video and it failed further upstream - "
														"so we must remove the video\n");
												//We need to send AAR asynchronously with current fd
												rx_send_aar_update_no_video(auth);
												must_unlock_aaa = 0;

										}
										//free the new flow description
										free_flow_description(p_session_data, 0);
								}
						}
				}

				show_callsessiondata(p_session_data);

				if (must_unlock_aaa) {
						LM_DBG("Unlocking AAA session");
						cdpb.AAASessionsUnlock(auth->hash);
				}
		} else {
				LM_DBG("Callback type not supported - just returning");
		}

		return;

error:
		if (auth) cdpb.AAASessionsUnlock(auth->hash);
		return;
}

void callback_pcscf_contact_cb(struct pcontact *c, int type, void *param)
{
		LM_DBG("----------------------!\n");
		LM_DBG("PCSCF Contact Callback!\n");
		LM_DBG("Contact AOR: [%.*s]\n", c->aor.len, c->aor.s);
		LM_DBG("Callback type [%d]\n", type);


		if (type == PCSCF_CONTACT_EXPIRE || type == PCSCF_CONTACT_DELETE) {
				// we dont need to send STR if no QoS was ever successfully registered!
				if (must_send_str && (c->reg_state != PCONTACT_REG_PENDING) && (c->reg_state != PCONTACT_REG_PENDING_AAR)) {
						LM_DBG("Received notification of contact (in state [%d] deleted for signalling bearer with  with Rx session ID: [%.*s]\n",
								c->reg_state, c->rx_session_id.len, c->rx_session_id.s);
						LM_DBG("Sending STR\n");
						rx_send_str(&c->rx_session_id);
				}
		}
}

static int get_identifier(str* src)
{
		char *sep;

		if (src == 0 || src->len == 0) {
				return -1;
		}

		if (identifier_size <= src->len) {
				if (identifier.s) {
						pkg_free(identifier.s);
				}
				identifier.s = (char*) pkg_malloc(src->len + 1);
				if (!identifier.s) {
						LM_ERR("no more pkg mem\n");
						return -1;
				}
				memset(identifier.s, 0, src->len + 1);
				identifier_size = src->len + 1;
		}

		memcpy(identifier.s, src->s, src->len);
		identifier.len = src->len;
		sep = memchr(identifier.s, 59 /* ; */, identifier.len);

		if (sep) identifier.len = (int) (sep - identifier.s);

		return 0;
}

uint16_t check_ip_version(str ip)
{
		struct addrinfo hint, *res = NULL;
		memset(&hint, '\0', sizeof(hint));
		hint.ai_family = AF_UNSPEC;
		hint.ai_flags = AI_NUMERICHOST;
		int getaddrret = getaddrinfo(ip.s, NULL, &hint, &res);
		if (getaddrret) {
				LM_ERR("GetAddrInfo returned an error !\n");
				return 0;
		}
		if (res->ai_family == AF_INET) {
				freeaddrinfo(res);
				return AF_INET;
		} else if (res->ai_family == AF_INET6) {
				freeaddrinfo(res);
				return AF_INET6;
		} else {
				freeaddrinfo(res);
				LM_ERR("unknown IP format \n");
				return 0;
		}
}

/* Wrapper to send AAR from config file - this only allows for AAR for calls - not register, which uses r_rx_aar_register
 * return: 1 - success, <=0 failure. 2 - message not an AAR generating message (ie proceed without PCC if you wish)
 */
static int w_rx_aar(struct sip_msg *msg, char *route, char* dir, char *c_id, int id_type)
{

		int ret = CSCF_RETURN_ERROR;
		int result = CSCF_RETURN_ERROR;
		struct cell *t;
		struct sip_msg* orig_sip_request_msg = NULL;

		AAASession* auth_session = 0;
		rx_authsessiondata_t* rx_authdata_p = 0;
		str *rx_session_id = 0;
		str callid = {0, 0};
		str ftag = {0, 0};
		str ttag = {0, 0};

		str route_name;
		str ip, uri;
		int identifier_type;
		int ip_version = 0;
		sdp_session_cell_t* sdp_session;
		sdp_stream_cell_t* sdp_stream;
		str s_id;
		struct hdr_field *h = 0;
		struct dlg_cell* dlg = 0;

		cfg_action_t* cfg_action = 0;
		saved_transaction_t* saved_t_data = 0; //data specific to each contact's AAR async call
		char* direction = dir;
		if (fixup_get_svalue(msg, (gparam_t*) route, &route_name) != 0) {
				LM_ERR("no async route block for assign_server_unreg\n");
				return result;
		}

		if (get_str_fparam(&s_id, msg, (fparam_t*) c_id) < 0) {
				LM_ERR("failed to get s__id\n");
				return result;
		}

		LM_DBG("Looking for route block [%.*s]\n", route_name.len, route_name.s);
		int ri = route_get(&main_rt, route_name.s);
		if (ri < 0) {
				LM_ERR("unable to find route block [%.*s]\n", route_name.len, route_name.s);
				return result;
		}
		cfg_action = main_rt.rlist[ri];
		if (cfg_action == NULL) {
				LM_ERR("empty action lists in route block [%.*s]\n", route_name.len, route_name.s);
				return result;
		}

		LM_DBG("Rx AAR called\n");
		//create the default return code AVP
		create_return_code(ret);

		//We don't ever do AAR on request for calling scenario...
		if (msg->first_line.type != SIP_REPLY) {
				LM_DBG("Can't do AAR for call session in request\n");
				return result;
		}

		//is it appropriate to send AAR at this stage?
		t = tmb.t_gett();
		if (t == NULL || t == T_UNDEFINED) {
				LM_WARN("Cannot get transaction for AAR based on SIP Request\n");
				//goto aarna;
				return result;
		}

		if (t->uas.status >= 200) {
				LM_DBG("trasaction sent out a final response already - %d\n",
						t->uas.status);
				return result;
		}


		/*  we may need the request message from here on.. if there are headers we need that were not parsed in the original request
			(which we cannot assume) then we would pollute the shm_msg t->uas.request if we did any parsing on it. Instead, we need to 
			make a private copy of the message and free it when we are done 
		 */
		if (msg_ctx_id_match(t->uas.request, &_pv_treq.msg_ctx) != 1) {

				/* make a copy */
				if (_pv_treq.buf == NULL || _pv_treq.buf_size < t->uas.request->len + 1) {
						if (_pv_treq.buf != NULL)
								pkg_free(_pv_treq.buf);
						if (_pv_treq.tmsgp)
								free_sip_msg(&_pv_treq.msg);
						_pv_treq.tmsgp = NULL;
                                                _pv_treq.msg_ctx.msgid=0;
                                                _pv_treq.msg_ctx.pid=0;
						_pv_treq.T = NULL;
						_pv_treq.buf_size = t->uas.request->len + 1;
						_pv_treq.buf = (char*) pkg_malloc(_pv_treq.buf_size * sizeof(char));
						if (_pv_treq.buf == NULL) {
								LM_ERR("no more pkg\n");
								_pv_treq.buf_size = 0;
								return -1;
						}
				}
				if (_pv_treq.tmsgp)
						free_sip_msg(&_pv_treq.msg);
				memset(&_pv_treq.msg, 0, sizeof(struct sip_msg));
				memcpy(_pv_treq.buf, t->uas.request->buf, t->uas.request->len);
				_pv_treq.buf[t->uas.request->len] = '\0';
				_pv_treq.msg.len = t->uas.request->len;
				_pv_treq.msg.buf = _pv_treq.buf;
				_pv_treq.tmsgp = t->uas.request;
                                msg_ctx_id_set(t->uas.request, &_pv_treq.msg_ctx);
				_pv_treq.T = t;


				if (pv_t_copy_msg(t->uas.request, &_pv_treq.msg) != 0) {
						pkg_free(_pv_treq.buf);
						_pv_treq.buf_size = 0;
						_pv_treq.buf = NULL;
						_pv_treq.tmsgp = NULL;
						_pv_treq.T = NULL;
						return -1;
				}
		}

		orig_sip_request_msg = &_pv_treq.msg;


		//we dont apply QoS if its not a reply to an INVITE! or UPDATE or PRACK!
		if ((t->method.len == 5 && memcmp(t->method.s, "PRACK", 5) == 0)
				|| (t->method.len == 6 && (memcmp(t->method.s, "INVITE", 6) == 0
				|| memcmp(t->method.s, "UPDATE", 6) == 0))) {
				if (cscf_get_content_length(msg) == 0
						|| cscf_get_content_length(orig_sip_request_msg) == 0) {
						LM_DBG("No SDP offer answer -> therefore we can not do Rx AAR");
						//goto aarna; //AAR na if we dont have offer/answer pair
						return result;
				}
		} else {
				LM_DBG("Message is not response to INVITE, PRACK or UPDATE -> therefore we do not Rx AAR");
				return result;
		}

		/* get callid, from and to tags to be able to identify dialog */
		callid = cscf_get_call_id(msg, 0);
		if (callid.len <= 0 || !callid.s) {
				LM_ERR("unable to get callid\n");
				return result;
		}
		if (!cscf_get_from_tag(msg, &ftag)) {
				LM_ERR("Unable to get ftag\n");
				return result;
		}
		if (!cscf_get_to_tag(msg, &ttag)) {
				LM_ERR("Unable to get ttag\n");
				return result;
		}

		//check to see that this is not a result of a retransmission in reply route only
		if (msg->cseq == NULL
				&& ((parse_headers(msg, HDR_CSEQ_F, 0) == -1) || (msg->cseq == NULL))) {
				LM_ERR("No Cseq header found - aborting\n");
				return result;
		}

		saved_t_data = (saved_transaction_t*) shm_malloc(sizeof(saved_transaction_t));
		if (!saved_t_data) {
				LM_ERR("Unable to allocate memory for transaction data, trying to send AAR\n");
				return result;
		}
		memset(saved_t_data, 0, sizeof(saved_transaction_t));
		saved_t_data->act = cfg_action;
		//OTHER parms need after async response set here
		//store call id
		saved_t_data->callid.s = (char*) shm_malloc(callid.len + 1);
		if (!saved_t_data->callid.s) {
				LM_ERR("no more memory trying to save transaction state : callid\n");
				shm_free(saved_t_data);
				return result;
		}
		memset(saved_t_data->callid.s, 0, callid.len + 1);
		memcpy(saved_t_data->callid.s, callid.s, callid.len);
		saved_t_data->callid.len = callid.len;

		//store ttag
		saved_t_data->ttag.s = (char*) shm_malloc(ttag.len + 1);
		if (!saved_t_data->ttag.s) {
				LM_ERR("no more memory trying to save transaction state : ttag\n");
				shm_free(saved_t_data);
				return result;
		}
		memset(saved_t_data->ttag.s, 0, ttag.len + 1);
		memcpy(saved_t_data->ttag.s, ttag.s, ttag.len);
		saved_t_data->ttag.len = ttag.len;

		//store ftag
		saved_t_data->ftag.s = (char*) shm_malloc(ftag.len + 1);
		if (!saved_t_data->ftag.s) {
				LM_ERR("no more memory trying to save transaction state : ftag\n");
				shm_free(saved_t_data);
				return result;
		}
		memset(saved_t_data->ftag.s, 0, ftag.len + 1);
		memcpy(saved_t_data->ftag.s, ftag.s, ftag.len);
		saved_t_data->ftag.len = ftag.len;

		saved_t_data->aar_update = 0; //by default we say this is not an aar update - if it is we set it below

		//store branch
		int branch;
		if (tmb.t_check(msg, &branch) == -1) {
				LOG(L_ERR, "ERROR: t_suspend: failed find UAC branch\n");
				return result;
		}

		//Check that we dont already have an auth session for this specific dialog
		//if not we create a new one and attach it to the dialog (via session ID).
		enum dialog_direction dlg_direction = get_dialog_direction(direction);
		if (dlg_direction == DLG_MOBILE_ORIGINATING) {
				rx_session_id = dlgb.get_dlg_var(&callid, &ftag, &ttag,
						&orig_session_key);
		} else {
				rx_session_id = dlgb.get_dlg_var(&callid, &ftag, &ttag,
						&term_session_key);
		}
		if (rx_session_id && rx_session_id->len > 0 && rx_session_id->s) {
				auth_session = cdpb.AAAGetAuthSession(*rx_session_id);
				if (auth_session && auth_session->u.auth.state != AUTH_ST_OPEN) {
						LM_DBG("This session is not state open - so we will create a new session");
						if (auth_session) cdpb.AAASessionsUnlock(auth_session->hash);
						auth_session = 0;
				}
		}

		if (!auth_session) {
				LM_DBG("New AAR session for this dialog in mode %s\n", direction);


				//get ip and subscription_id and store them in the call session data

				//SUBSCRIPTION-ID

				//if subscription-id and identifier_type is passed from config file we use them - if not we use default behaviour of

				//if its mo we use p_asserted_identity in request - if that not there we use from_uri
				//if its mt we use p_asserted_identity in reply - if that not there we use to_uri
				if (s_id.len > 0 && id_type > -1) {
						get_identifier(&s_id);
						identifier_type = id_type;
						LM_DBG("Passed in subscription_id [%.*s] and subscription_id_type [%d]\n", identifier.len, identifier.s, identifier_type);
				} else {
						if (dlg_direction == DLG_MOBILE_ORIGINATING) {
								LM_DBG("originating direction\n");
								uri = cscf_get_asserted_identity(orig_sip_request_msg, 0);
								if (uri.len == 0) {
										LM_ERR("No P-Asserted-Identity hdr found in request. Using From hdr in req - we shouldn't have to do this");

										if (!cscf_get_from_uri(orig_sip_request_msg, &uri)) {
												LM_ERR("Error assigning P-Asserted-Identity using From hdr in req");
												goto error;
										}
										LM_DBG("going to remove parameters if any from identity: [%.*s]\n", uri.len, uri.s);
										get_identifier(&uri);
										LM_DBG("identifier from uri : [%.*s]\n", identifier.len, identifier.s);

								} else {
										get_identifier(&uri);
										//free this cscf_get_asserted_identity allocates it
										//                    pkg_free(uri.s);
								}
						} else {
								LM_DBG("terminating direction\n");
								uri = cscf_get_asserted_identity(msg, 0);
								if (uri.len == 0) {
										LM_DBG("No P-Asserted-Identity hdr found in response. Using Called party id in resp");
										//get identity from called party id
										//getting called asserted identity
										uri = cscf_get_public_identity_from_called_party_id(orig_sip_request_msg, &h);
										if (uri.len == 0) {
												LM_ERR("No P-Called-Party hdr found in response. Using req URI from dlg - we shouldn't have to do this");
												//get dialog and get the req URI from there
												dlg = dlgb.get_dlg(msg);
												if (!dlg) {
													if (!cscf_get_to_uri(orig_sip_request_msg, &uri)) {
														LM_ERR("Error assigning P-Asserted-Identity using To hdr in req");
														goto error;
													}
													LM_DBG("going to remove parameters if any from identity: [%.*s]\n", uri.len, uri.s);
													get_identifier(&uri);
													LM_DBG("identifier from uri : [%.*s]\n", identifier.len, identifier.s);
												}else{
													LM_DBG("dlg req uri : [%.*s] going to remove parameters if any\n", dlg->req_uri.len, dlg->req_uri.s);

													if (get_identifier(&dlg->req_uri) != 0) {
															dlgb.release_dlg(dlg);
															goto error;
													}
													dlgb.release_dlg(dlg);
													LM_DBG("identifier from dlg req uri : [%.*s]\n", identifier.len, identifier.s);
												}
										} else {
												get_identifier(&uri);
										}
								} else {
										get_identifier(&uri);
								}
						}
						if (strncasecmp(identifier.s, "tel:", 4) == 0) {
								identifier_type = AVP_Subscription_Id_Type_E164; //
						} else {
								identifier_type = AVP_Subscription_Id_Type_SIP_URI; //default is END_USER_SIP_URI
						}
				}
				//IP 
				//if its mo we use request SDP
				//if its mt we use reply SDP
				if (dlg_direction == DLG_MOBILE_ORIGINATING) {
						LM_DBG("originating direction\n");
						//get ip from request sdp (we use first SDP session)
						if (parse_sdp(orig_sip_request_msg) < 0) {
								LM_ERR("Unable to parse req SDP\n");
								goto error;
						}

						sdp_session = get_sdp_session(orig_sip_request_msg, 0);
						if (!sdp_session) {
								LM_ERR("Missing SDP session information from req\n");
								goto error;
						}
						ip = sdp_session->ip_addr;
						ip_version = sdp_session->pf;
						
						LM_DBG("IP retrieved from Request SDP to use for framed IP address: [%.*s]", ip.len, ip.s);
						
						if (ip.len <= 0) {
								LM_DBG("Request SDP connection IP could not be retrieved, so we use SDP stream IP");
								sdp_stream = get_sdp_stream(orig_sip_request_msg, 0, 0);
								if (!sdp_stream) {
										LM_ERR("Missing SDP stream information from request\n");
										goto error;
								}
								
								ip = sdp_stream->ip_addr;
								if (ip.len <= 0) {
										LM_ERR("Request SDP IP information could not be retrieved");
										goto error;
								}
								ip_version = check_ip_version(ip);
								if (!ip_version) {
										LM_ERR("check_ip_version returned 0 \n");
										goto error;
								}
								
						}
						
						free_sdp((sdp_info_t**) (void*) &t->uas.request->body);
						
				} else {
						LM_DBG("terminating direction\n");
						//get ip from reply sdp (we use first SDP session)
						if (parse_sdp(msg) < 0) {
								LM_ERR("Unable to parse reply SDP\n");
								goto error;
						}

						sdp_session = get_sdp_session(msg, 0);
						if (!sdp_session) {
								LM_ERR("Missing SDP session information from reply\n");
								goto error;
						}
						ip = sdp_session->ip_addr;
						ip_version = sdp_session->pf;
						
						LM_DBG("IP retrieved from Reply SDP to use for framed IP address: [%.*s]", ip.len, ip.s);
						
						if (ip.len <= 0) {
								LM_DBG("Reply SDP connection IP could not be retrieved, so we use SDP stream IP");
								sdp_stream = get_sdp_stream(msg, 0, 0);
								if (!sdp_stream) {
										LM_ERR("Missing SDP stream information from reply\n");
										goto error;
								}
								
								ip = sdp_stream->ip_addr;
								if (ip.len <= 0) {
										LM_ERR("Reply SDP IP information could not be retrieved");
										goto error;
								}
								ip_version = check_ip_version(ip);
								if (!ip_version) {
										LM_ERR("check_ip_version returned 0 \n");
										goto error;
								}
								
						}
						
						free_sdp((sdp_info_t**) (void*) &msg->body);
				}

				int ret = create_new_callsessiondata(&callid, &ftag, &ttag, &identifier, identifier_type, &ip, ip_version, &rx_authdata_p);
				if (!ret) {
						LM_DBG("Unable to create new media session data parcel\n");
						goto error;
				}

				//create new diameter auth session
				auth_session = cdpb.AAACreateClientAuthSession(1, callback_for_cdp_session, rx_authdata_p); //returns with a lock
				if (!auth_session) {
						LM_ERR("Rx: unable to create new Rx Media Session\n");
						if (auth_session) cdpb.AAASessionsUnlock(auth_session->hash);
						if (rx_authdata_p) {
								free_callsessiondata(rx_authdata_p);
						}
						goto error;
				}
				auth_session->u.auth.class = AUTH_CLASS_RXMEDIA;

				//attach new cdp auth session to dlg for this direction
				if (dlg_direction == DLG_MOBILE_ORIGINATING) {
						dlgb.set_dlg_var(&callid, &ftag, &ttag,
								&orig_session_key, &auth_session->id);
				} else {
						dlgb.set_dlg_var(&callid, &ftag, &ttag,
								&term_session_key, &auth_session->id);
				}
				LM_DBG("Attached CDP auth session [%.*s] for Rx to dialog in %s mode\n", auth_session->id.len, auth_session->id.s, direction);
		} else {
				LM_DBG("Update AAR session for this dialog in mode %s\n", direction);
				//check if this is triggered by a 183 - if so break here as its probably a re-transmit
				if ((msg->first_line).u.reply.statuscode == 183) {
						LM_DBG("Received a 183 for a diameter session that already exists - just going to ignore this\n");
						cdpb.AAASessionsUnlock(auth_session->hash);
						result = CSCF_RETURN_TRUE;
						goto ignore;
				}
				saved_t_data->aar_update = 1; //this is an update aar - we set this so on async_aar we know this is an update and act accordingly
		}

                dlg = dlgb.get_dlg(msg);
                if (!dlg) {
                    LM_ERR("Unable to find dialog and cannot do Rx without it\n");
                    goto error;
                }
                saved_t_data->dlg = dlg;

		LM_DBG("Suspending SIP TM transaction\n");
		if (tmb.t_suspend(msg, &saved_t_data->tindex, &saved_t_data->tlabel) != 0) {
				LM_ERR("failed to suspend the TM processing\n");
				if (auth_session) cdpb.AAASessionsUnlock(auth_session->hash);
				goto error;
		}

		LM_DBG("Sending Rx AAR");
		ret = rx_send_aar(orig_sip_request_msg, msg, auth_session, direction, saved_t_data);

		if (!ret) {
				LM_ERR("Failed to send AAR\n");
				tmb.t_cancel_suspend(saved_t_data->tindex, saved_t_data->tlabel);
				goto error;


		} else {
				LM_DBG("Successful async send of AAR\n");
				result = CSCF_RETURN_BREAK;
				return result; //on success we break - because rest of cfg file will be executed by async process
		}

error:
		LM_ERR("Error trying to send AAR (calling)\n");
		if (auth_session != NULL) {
				cdpb.AAASessionsUnlock(auth_session->hash);
				cdpb.AAADropAuthSession(auth_session);
		}
ignore:
		if (saved_t_data)
				free_saved_transaction_global_data(saved_t_data); //only free global data if no AARs were sent. if one was sent we have to rely on the callback (CDP) to free
		//otherwise the callback will segfault
		return result;
}

/* Wrapper to send AAR from config file - only used for registration */
static int w_rx_aar_register(struct sip_msg *msg, char* route, char* str1, char* bar)
{

		int ret = CSCF_RETURN_ERROR;
		struct pcontact_info ci;
		struct cell *t;
		contact_t* c;
		struct hdr_field* h;
		pcontact_t* pcontact;
		pcontact_info_t contact_info;
		contact_body_t* cb = 0;
		AAASession* auth;
		rx_authsessiondata_t* rx_regsession_data_p;
		cfg_action_t* cfg_action = 0;
		str route_name;
		char* p;
		int aar_sent = 0;
		saved_transaction_local_t* local_data = 0; //data to be shared across all async calls
		saved_transaction_t* saved_t_data = 0; //data specific to each contact's AAR async call
		str recv_ip;
		int recv_port;
		unsigned short recv_proto;
		uint16_t ip_version;

		struct via_body* vb;
		unsigned short via_port;
		unsigned short via_proto;

		if (fixup_get_svalue(msg, (gparam_t*) route, &route_name) != 0) {
				LM_ERR("no async route block for assign_server_unreg\n");
				return -1;
		}

		LM_DBG("Looking for route block [%.*s]\n", route_name.len, route_name.s);
		int ri = route_get(&main_rt, route_name.s);
		if (ri < 0) {
				LM_ERR("unable to find route block [%.*s]\n", route_name.len, route_name.s);
				return -1;
		}
		cfg_action = main_rt.rlist[ri];
		if (cfg_action == NULL) {
				LM_ERR("empty action lists in route block [%.*s]\n", route_name.len, route_name.s);
				return -1;
		}

		udomain_t* domain_t = (udomain_t*) str1;

		int is_rereg = 0; //is this a reg/re-reg

		LM_DBG("Rx AAR Register called\n");

		//create the default return code AVP
		create_return_code(ret);

		memset(&ci, 0, sizeof(struct pcontact_info));

		/** If this is a response then let's check the status before we try and do an AAR.
		 * We will only do AAR for register on success response and of course if message is register
		 */
		if (msg->first_line.type == SIP_REPLY) {
				//check this is a response to a register
				/* Get the SIP request from this transaction */
				t = tmb.t_gett();
				if (!t) {
						LM_ERR("Cannot get transaction for AAR based on SIP Request\n");
						goto error;
				}
				if ((strncmp(t->method.s, "REGISTER", 8) != 0)) {
						LM_ERR("Method is not a response to a REGISTER\n");
						goto error;
				}
				if (msg->first_line.u.reply.statuscode < 200
						|| msg->first_line.u.reply.statuscode >= 300) {
						LM_DBG("Message is not a 2xx OK response to a REGISTER\n");
						goto error;
				}
				tmb.t_release(msg);
		} else { //SIP Request
				/* in case of request make sure it is a REGISTER */
				if (msg->first_line.u.request.method_value != METHOD_REGISTER) {
						LM_DBG("This is not a register request\n");
						goto error;
				}

				if ((cscf_get_max_expires(msg, 0) == 0)) {
						//if ((cscf_get_expires(msg) == 0)) {
						LM_DBG("This is a de registration\n");
						LM_DBG("We ignore it as these are dealt with by usrloc callbacks \n");
						create_return_code(CSCF_RETURN_TRUE);
						return CSCF_RETURN_TRUE;
				}
		}

		vb = cscf_get_ue_via(msg);
		via_port = vb->port ? vb->port : 5060;
		via_proto = vb->proto;

		//before we continue, make sure we have a transaction to work with (viz. cdp async)
		t = tmb.t_gett();
		if (t == NULL || t == T_UNDEFINED) {
				if (tmb.t_newtran(msg) < 0) {
						LM_ERR("cannot create the transaction for UAR async\n");
						return CSCF_RETURN_ERROR;
				}
				t = tmb.t_gett();
				if (t == NULL || t == T_UNDEFINED) {
						LM_ERR("cannot lookup the transaction\n");
						return CSCF_RETURN_ERROR;
				}
		}

		saved_t_data = (saved_transaction_t*) shm_malloc(sizeof(saved_transaction_t));
		if (!saved_t_data) {
				LM_ERR("Unable to allocate memory for transaction data, trying to send AAR\n");
				return CSCF_RETURN_ERROR;
		}
		memset(saved_t_data, 0, sizeof(saved_transaction_t));
		saved_t_data->act = cfg_action;
		saved_t_data->domain = domain_t;
		saved_t_data->lock = lock_alloc();
		if (saved_t_data->lock == NULL) {
				LM_ERR("unable to allocate init lock for saved_t_transaction reply counter\n");
				return CSCF_RETURN_ERROR;
		}
		if (lock_init(saved_t_data->lock) == NULL) {
				LM_ERR("unable to init lock for saved_t_transaction reply counter\n");
				return CSCF_RETURN_ERROR;
		}

		LM_DBG("Suspending SIP TM transaction\n");
		if (tmb.t_suspend(msg, &saved_t_data->tindex, &saved_t_data->tlabel) != 0) {
				LM_ERR("failed to suspend the TM processing\n");
				free_saved_transaction_global_data(saved_t_data);
				return CSCF_RETURN_ERROR;
		}

		LM_DBG("Successfully suspended transaction\n");

		//now get the contacts in the REGISTER and do AAR for each one.
		cb = cscf_parse_contacts(msg);
		if (!cb || (!cb->contacts && !cb->star)) {
				LM_DBG("No contact headers in Register message\n");
				goto error;
		}

		//we use the received IP address for the framed_ip_address 
		recv_ip.s = ip_addr2a(&msg->rcv.src_ip);
		recv_ip.len = strlen(ip_addr2a(&msg->rcv.src_ip));

		ip_version = check_ip_version(recv_ip);
		if (!ip_version) {
				LM_ERR("check_ip_version returned 0 \n");
				goto error;
		}

		recv_port = msg->rcv.src_port;
		recv_proto = msg->rcv.proto;

		LM_DBG("Message received IP address is: [%.*s]\n", recv_ip.len, recv_ip.s);
		LM_DBG("Message via is [%d://%.*s:%d]\n", vb->proto, vb->host.len, vb->host.s, via_port);

		lock_get(saved_t_data->lock); //we lock here to make sure we send all requests before processing replies asynchronously
		for (h = msg->contact; h; h = h->next) {
				if (h->type == HDR_CONTACT_T && h->parsed) {
						for (c = ((contact_body_t*) h->parsed)->contacts; c; c = c->next) {
								ul.lock_udomain(domain_t, &vb->host, vb->port, vb->proto);
								memset(&contact_info, 0, sizeof(struct pcontact_info));
								contact_info.aor = c->uri;
								contact_info.via_host = vb->host;
								contact_info.via_port = vb->port;
								contact_info.via_prot = vb->proto;
								contact_info.searchflag = SEARCH_NORMAL;
								contact_info.received_host.s = 0;
								contact_info.received_host.len = 0;
								contact_info.reg_state = PCONTACT_ANY; //search for any state

								if (ul.get_pcontact(domain_t, &contact_info, &pcontact, 0) != 0) {
										LM_ERR("This contact does not exist in PCSCF usrloc - error in cfg file\n");
										ul.unlock_udomain(domain_t, &vb->host, vb->port, vb->proto);
										lock_release(saved_t_data->lock);
										goto error;
								} else if (pcontact->reg_state == PCONTACT_REG_PENDING
										|| pcontact->reg_state == PCONTACT_REGISTERED) { //NEW reg request
										LM_DBG("Contact [%.*s] exists and is in state PCONTACT_REG_PENDING or PCONTACT_REGISTERED\n"
												, pcontact->aor.len, pcontact->aor.s);

										//check for existing Rx session
										if (pcontact->rx_session_id.len > 0
												&& pcontact->rx_session_id.s
												&& (auth = cdpb.AAAGetAuthSession(pcontact->rx_session_id))) {
												LM_DBG("Rx session already exists for this user\n");
												if (memcmp(pcontact->rx_session_id.s, auth->id.s, auth->id.len) != 0) {
														LM_ERR("Rx session mismatch for rx_session_id [%.*s].......Aborting\n", pcontact->rx_session_id.len, pcontact->rx_session_id.s);
														if (auth) cdpb.AAASessionsUnlock(auth->hash);
														lock_release(saved_t_data->lock);
														goto error;
												}
												//re-registration - update auth lifetime
												auth->u.auth.lifetime = time(NULL) + rx_auth_expiry;
												is_rereg = 1;
										} else {
												LM_DBG("Creating new Rx session for contact <%.*s>\n", pcontact->aor.len, pcontact->aor.s);
												int ret = create_new_regsessiondata(domain_t->name, &pcontact->aor, &recv_ip, ip_version, recv_port, recv_proto, &vb->host, vb->port, vb->proto, &rx_regsession_data_p);
												if (!ret) {
														LM_ERR("Unable to create regsession data parcel for rx_session_id [%.*s]...Aborting\n", pcontact->rx_session_id.len, pcontact->rx_session_id.s);
														ul.unlock_udomain(domain_t, &vb->host, vb->port, vb->proto);
														if (rx_regsession_data_p) {
																shm_free(rx_regsession_data_p);
																rx_regsession_data_p = 0;
														}
														lock_release(saved_t_data->lock);
														goto error;
												}
												auth = cdpb.AAACreateClientAuthSession(1, callback_for_cdp_session, rx_regsession_data_p); //returns with a lock
												if (!auth) {
														LM_ERR("Rx: unable to create new Rx Reg Session for rx_session_id is [%.*s]\n", pcontact->rx_session_id.len, pcontact->rx_session_id.s);
														if (rx_regsession_data_p) {
																shm_free(rx_regsession_data_p);
																rx_regsession_data_p = 0;
														}
														ul.unlock_udomain(domain_t, &vb->host, via_port, via_proto);
														if (auth) cdpb.AAASessionsUnlock(auth->hash);
														if (rx_regsession_data_p) {
																shm_free(rx_regsession_data_p);
																rx_regsession_data_p = 0;
														}
														lock_release(saved_t_data->lock);
														goto error;
												}
												auth->u.auth.class = AUTH_CLASS_RXREG;
										}

										//we are ready to send the AAR async. lets save the local data data
										int local_data_len = sizeof(saved_transaction_local_t) + c->uri.len + auth->id.len + vb->host.len + recv_ip.len;
										local_data = shm_malloc(local_data_len);
										if (!local_data) {
												LM_ERR("unable to alloc memory for local data, trying to send AAR Register\n");
												lock_release(saved_t_data->lock);
												goto error;
										}
										memset(local_data, 0, local_data_len);

										local_data->is_rereg = is_rereg;
										local_data->global_data = saved_t_data;
										p = (char*) (local_data + 1);

										local_data->contact.s = p;
										local_data->contact.len = c->uri.len;
										memcpy(p, c->uri.s, c->uri.len);
										p += c->uri.len;

										local_data->auth_session_id.s = p;
										local_data->auth_session_id.len = auth->id.len;
										memcpy(p, auth->id.s, auth->id.len);
										p += auth->id.len;

										local_data->via_host.s = p;
										local_data->via_host.len = vb->host.len;
										memcpy(p, vb->host.s, vb->host.len);
										p += vb->host.len;

										local_data->recv_host.s = p;
										local_data->recv_host.len = recv_ip.len;
										memcpy(p, recv_ip.s, recv_ip.len);
										p += recv_ip.len;

										local_data->via_port = via_port;
										local_data->via_proto = via_proto;
										local_data->recv_port = recv_port;
										local_data->recv_proto = recv_proto;

										if (p != (((char*) local_data) + local_data_len)) {
												LM_CRIT("buffer overflow\n");
												free_saved_transaction_data(local_data);
												goto error;
										}

										LM_DBG("Calling send aar register");

										//TODOD remove - no longer user AOR parm
										//ret = rx_send_aar_register(msg, auth, &puri.host, &ip_version, &c->uri, local_data); //returns a locked rx auth object
										ret = rx_send_aar_register(msg, auth, local_data); //returns a locked rx auth object

										ul.unlock_udomain(domain_t, &vb->host, via_port, via_proto);

										if (!ret) {
												LM_ERR("Failed to send AAR\n");
												lock_release(saved_t_data->lock);
												free_saved_transaction_data(local_data); //free the local data becuase the CDP async request was not successful (we must free here)
												goto error;
										} else {
												aar_sent = 1;
												//before we send - bump up the reply counter
												saved_t_data->answers_not_received++; //we dont need to lock as we already hold the lock above
										}
								} else {
										//contact exists - this is a re-registration, for now we just ignore this
										LM_DBG("This contact exists and is not in state REGISTER PENDING - we assume re (or de) registration and ignore\n");
										ul.unlock_udomain(domain_t, &vb->host, via_port, via_proto);
										//now we loop for any other contacts.
								}
						}
				} else {
						if (h->type == HDR_CONTACT_T) { //means we couldnt parse the contact - this is an error
								LM_ERR("Failed to parse contact header\n");
								lock_release(saved_t_data->lock);
								goto error;
						}
				}
		}
		//all requests sent at this point - we can unlock the reply lock
		lock_release(saved_t_data->lock);

		/*if we get here, we have either:
		 * 1. Successfully sent AAR's for ALL contacts, or
		 * 2. haven't needed to send ANY AAR's for ANY contacts
		 */
		if (aar_sent) {
				LM_DBG("Successful async send of AAR\n");
				return CSCF_RETURN_BREAK; //on success we break - because rest of cfg file will be executed by async process
		} else {
				create_return_code(CSCF_RETURN_TRUE);
				tmb.t_cancel_suspend(saved_t_data->tindex, saved_t_data->tlabel);
				if (saved_t_data) {
						free_saved_transaction_global_data(saved_t_data); //no aar sent so we must free the global data
				}
				//return CSCF_RETURN_ERROR;
				return CSCF_RETURN_TRUE;
		}
error:
		LM_ERR("Error trying to send AAR\n");
		if (!aar_sent) {
				tmb.t_cancel_suspend(saved_t_data->tindex, saved_t_data->tlabel);
				if (saved_t_data) {

						free_saved_transaction_global_data(saved_t_data); //only free global data if no AARs were sent. if one was sent we have to rely on the callback (CDP) to free
						//otherwise the callback will segfault
				}
		}
		return CSCF_RETURN_ERROR;
		//return CSCF_RETURN_FALSE;
}

static int fixup_aar_register(void** param, int param_no)
{
		//    udomain_t* d;
		//    aar_param_t *ap;
		//
		//    if (param_no != 1)
		//        return 0;
		//    ap = (aar_param_t*) pkg_malloc(sizeof (aar_param_t));
		//    if (ap == NULL) {
		//        LM_ERR("no more pkg\n");
		//        return -1;
		//    }
		//    memset(ap, 0, sizeof (aar_param_t));
		//    ap->paction = get_action_from_param(param, param_no);
		//
		//    if (ul.register_udomain((char*) *param, &d) < 0) {
		//        LM_ERR("failed to register domain\n");
		//        return E_UNSPEC;
		//    }
		//    ap->domain = d;
		//
		//    *param = (void*) ap;
		//    return 0;
		if (strlen((char*) *param) <= 0) {
				LM_ERR("empty parameter %d not allowed\n", param_no);
				return -1;
		}

		if (param_no == 1) { //route name - static or dynamic string (config vars)
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

static int fixup_aar(void** param, int param_no)
{
		str s;
		int num;

		//param 3 can be empty
		if (param_no != 3 && strlen((char*) *param) <= 0) {
				LM_ERR("empty parameter %d not allowed\n", param_no);
				return -1;
		}

		if (param_no == 1) { //route name - static or dynamic string (config vars)
				if (fixup_spve_null(param, param_no) < 0)
						return -1;
				return 0;
		} else if (param_no == 3) {
				return fixup_var_str_12(param, param_no);
		} else if (param_no == 4) {
				/*convert to int */
				s.s = (char*) *param;
				s.len = strlen(s.s);
				if (str2sint(&s, &num) == 0) {
						pkg_free(*param);
						*param = (void*) (unsigned long) num;
						return 0;
				}
				LM_ERR("Bad subscription id: <%s>n", (char*) (*param));

				return E_CFG;
		}

		return 0;
}

/*create a return code to be passed back into config file*/
int create_return_code(int result)
{
		int rc;
		int_str avp_val, avp_name;
		avp_name.s.s = "aar_return_code";
		avp_name.s.len = 15;

		LM_DBG("Creating return code of [%d] for aar_return_code\n", result);
		//build avp spec for uaa_return_code
		avp_val.n = result;

		rc = add_avp(AVP_NAME_STR, avp_name, avp_val);

		if (rc < 0)
				LM_ERR("couldn't create [aar_return_code] AVP\n");
		else
				LM_DBG("created AVP successfully : [%.*s]\n", avp_name.s.len, avp_name.s.s);

		return rc;
}

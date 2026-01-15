/*
 *
 * Peerstate Module - Peer State Tracking
 *
 * Copyright (C) 2025 Serdar Gucluer (Netgsm ICT Inc. - www.netgsm.com.tr)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../core/mem/shm_mem.h"
#include "../../core/shm_init.h"
#include "../../core/mem/mem.h"
#include "../../core/script_cb.h"
#include "../../core/sr_module.h"
#include "../../core/parser/parse_expires.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_param.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/dprint.h"
#include "../../core/pt.h"
#include "../../core/ut.h"
#include "../../core/str.h"
#include "../../core/str_list.h"
#include "../../core/timer_proc.h"
#include "../../core/timer.h"
#include "../../core/utils/sruid.h"
#include "../dialog/dlg_load.h"
#include "../dialog/dlg_hash.h"
#include "../usrloc/usrloc.h"
#include "../../core/route.h"
#include "../../core/kemi.h"
#include "../../core/fmsg.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "peerstate.h"
#include "peerstate_cache.h"
#include "peerstate_cb.h"

MODULE_VERSION

/* Default module parameter values */
#define DEF_CALLER_ALWAYS_CONFIRMED 0
#define DEF_USE_AVPS 0
#define DEF_CALLER_AVP 0
#define DEF_CALLEE_AVP 0
#define DEF_REG_AVP 0

#define DEF_ENABLE_NOTIFY_ON_TRYING 0
#define DEF_DISABLE_CALLER_NOTIFY_FLAG -1
#define DEF_DISABLE_CALLEE_NOTIFY_FLAG -1
#define DEF_DISABLE_REG_NOTIFY -1
#define DEF_DISABLE_DLG_NOTIFY -1
#define DEF_CACHE_EXPIRE 3600
#define DEF_CACHE_CLEANUP_INTERVAL 300
#define DEF_CACHE_HASH_SIZE 4096

static struct dlg_binds dlg_api;
static usrloc_api_t ul_api;

avp_flags_t caller_avp_type;
avp_name_t caller_avp_name;
avp_flags_t callee_avp_type;
avp_name_t callee_avp_name;
avp_flags_t reg_avp_type;
avp_name_t reg_avp_name;

static char *DLG_VAR_SEP = ",";
static str caller_dlg_var = {"caller_var", strlen("caller_var")};
static str callee_dlg_var = {"callee_var", strlen("callee_var")};
static int peerstate_event_rt = -1;

/* Module parameter variables */
int use_avps = DEF_USE_AVPS;
int enable_notify_on_trying = DEF_ENABLE_NOTIFY_ON_TRYING;
int disable_caller_notify_flag = DEF_DISABLE_CALLER_NOTIFY_FLAG;
int disable_callee_notify_flag = DEF_DISABLE_CALLEE_NOTIFY_FLAG;
int disable_reg_notify = DEF_DISABLE_REG_NOTIFY;
int disable_dlg_notify = DEF_DISABLE_DLG_NOTIFY;
char *caller_avp = DEF_CALLER_AVP;
char *callee_avp = DEF_CALLEE_AVP;
char *reg_avp = DEF_REG_AVP;
int dialog_event_types = DLGCB_FAILED | DLGCB_CONFIRMED | DLGCB_TERMINATED
						 | DLGCB_EXPIRED | DLGCB_EARLY;
int cache_expire = DEF_CACHE_EXPIRE; /* Default: 1 hour */
int cache_cleanup_interval =
		DEF_CACHE_CLEANUP_INTERVAL;		   /* Default: 5 minutes */
int cache_hash_size = DEF_CACHE_HASH_SIZE; /* Default: 4096 entries */

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);
static inline void print_ucontact(ucontact_t *c);
static inline int should_notify(
		int disable_notify, int disable_flag, struct sip_msg *request);
static void process_dialog_peers(struct str_list *peers, ps_state_t new_state,
		int early_enabled, str *callid);
static struct str_list *peer_at_host_list(struct sip_uri *uri);
static inline void free_ps_dlginfo_cell(void *param);
static inline void free_str_list_all(struct str_list *del_current);
static struct str_list *get_avp_str_list(
		unsigned short avp_flags, int_str avp_name);
static inline int get_avp_first_str(
		unsigned short avp_flags, int_str avp_name, str *result);
static int set_dlg_var(struct dlg_cell *dlg, str *key, struct str_list *lst);
static int get_dlg_var(struct dlg_cell *dlg, str *key, struct str_list **lst);
static struct ps_dlginfo_cell *get_dialog_data(struct dlg_cell *dlg, int type,
		int disable_caller_notify, int disable_callee_notify);
static void rpc_dump(rpc_t *rpc, void *ctx);
static void rpc_list(rpc_t *rpc, void *ctx);
static void rpc_stats(rpc_t *rpc, void *ctx);
static void rpc_get_peer(rpc_t *rpc, void *ctx);


/* clang-format off */
static cmd_export_t cmds[] = {
	{"bind_peerstate", (cmd_function)bind_peerstate, 1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

static const char *rpc_dump_doc[2] = {
	"Dump all cache entries",
	0
};

static const char *rpc_get_peer_doc[2] = {
	"Get state information for specific peer",
	0
};

static const char *rpc_stats_doc[2] = {
	"Get cache statistics",
	0
};

static const char *rpc_list_doc[2] = {
	"List all peers in specific state (NOT_INUSE, INUSE, RINGING, UNAVAILABLE)",
	0
};

static rpc_export_t peerstate_rpc[] = {
	{"peerstate.get_peer", rpc_get_peer, rpc_get_peer_doc, 0},
	{"peerstate.stats", rpc_stats, rpc_stats_doc, 0},
	{"peerstate.list", rpc_list, rpc_list_doc, 0},
	{"peerstate.dump", rpc_dump, rpc_dump_doc, 0},
	{0, 0, 0, 0}
};

static param_export_t params[] = {
	{"use_avps", PARAM_INT, &use_avps},
	{"caller_avp", PARAM_STRING, &caller_avp},
	{"callee_avp", PARAM_STRING, &callee_avp},
	{"reg_avp", PARAM_STRING, &reg_avp},
	{"enable_notify_on_trying", PARAM_INT, &enable_notify_on_trying},
	{"disable_caller_notify_flag", PARAM_INT, &disable_caller_notify_flag},
	{"disable_callee_notify_flag", PARAM_INT, &disable_callee_notify_flag},
	{"disable_reg_notify", PARAM_INT, &disable_reg_notify},
	{"disable_dlg_notify", PARAM_INT, &disable_dlg_notify},
	{"cache_expire", PARAM_INT, &cache_expire},
	{"cache_cleanup_interval", PARAM_INT, &cache_cleanup_interval},
	{"cache_hash_size", PARAM_INT, &cache_hash_size},
	{0, 0, 0}
};

struct module_exports exports = {
	"peerstate", 			/* module name */
	DEFAULT_DLFLAGS,  		/* dlopen flags */
	cmds,			  	/* exported functions */
	params,			  	/* exported parameters */
	peerstate_rpc,			/* RPC method exports */
	0,			 	/* exported pseudo-variables */
	0,				/* response handling function */
	mod_init,		  	/* module initialization function */
	child_init,		  	/* per-child init function */
	mod_destroy		  	/* module destroy function */
};
/* clang-format on */

static void trigger_event_route(
		ps_state_t ps_state, ps_state_t prev_state, str *peer, str *uniq_id)
{
	struct run_act_ctx ra_ctx;
	struct sip_msg *fmsg = NULL;
	sr_kemi_eng_t *keng = NULL;
	str evname = str_init(PEERSTATE_EVENT_ROUTE);
	int_str avp_name, avp_val;

	if(peerstate_event_rt < 0)
		return;

	// Create fake message for event route execution
	fmsg = faked_msg_get_next();
	if(fmsg == NULL) {
		LM_ERR("failed to get faked message\n");
		return;
	}

	avp_name.s.s = "ps_peer";
	avp_name.s.len = 7;
	avp_val.s = *peer;
	if(add_avp(AVP_NAME_STR | AVP_VAL_STR, avp_name, avp_val) < 0) {
		LM_ERR("failed to set peer AVP\n");
		goto error;
	}

	avp_name.s.s = "ps_state";
	avp_name.s.len = 8;
	avp_val.s.s = (char *)PS_STATE_TO_STR(ps_state);
	avp_val.s.len = strlen(avp_val.s.s);
	if(add_avp(AVP_NAME_STR | AVP_VAL_STR, avp_name, avp_val) < 0) {
		LM_ERR("failed to set state AVP\n");
		goto error;
	}

	avp_name.s.s = "ps_prev_state";
	avp_name.s.len = 13;
	avp_val.s.s = (char *)PS_STATE_TO_STR(prev_state);
	avp_val.s.len = strlen(avp_val.s.s);
	if(add_avp(AVP_NAME_STR | AVP_VAL_STR, avp_name, avp_val) < 0) {
		LM_ERR("failed to set previous state AVP\n");
		goto error;
	}

	avp_name.s.s = "ps_uniq_id";
	avp_name.s.len = 10;
	avp_val.s = *uniq_id;
	if(add_avp(AVP_NAME_STR | AVP_VAL_STR, avp_name, avp_val) < 0) {
		LM_ERR("failed to set callid AVP\n");
		goto error;
	}

	keng = sr_kemi_eng_get();
	if(keng == NULL) {
		init_run_actions_ctx(&ra_ctx);
		run_top_route(event_rt.rlist[peerstate_event_rt], fmsg, &ra_ctx);
	} else if(keng->froute(fmsg, EVENT_ROUTE, &evname, &evname) < 0)
		LM_ERR("error executing event route kemi callback\n");

	LM_DBG("event_route[%s] executed: peer=%.*s, state=%s, id=%.*s\n",
			PEERSTATE_EVENT_ROUTE, peer->len, peer->s,
			PS_STATE_TO_STR(ps_state), uniq_id->len, uniq_id->s);
	return;

error:
	LM_ERR("failed to execute event route\n");
}

/**
 * @brief Helper to trigger callbacks with event type
 */
static inline void notify_with_callbacks(ps_state_t ps_state,
		ps_state_t prev_state, str *peer, str *uniq_id,
		peerstate_event_type_t event_type)
{
	peerstate_cb_ctx_t ctx;
	int call_count = 0, is_registered = 0;

	LM_DBG("[%.*s] notify for peer: %.*s with state: %s (event_type=%d)\n",
			uniq_id->len, uniq_id->s, peer->len, peer->s,
			PS_STATE_TO_STR(ps_state), event_type);

	/* Trigger event route */
	trigger_event_route(ps_state, prev_state, peer, uniq_id);
	/* Prepare callback context */
	memset(&ctx, 0, sizeof(peerstate_cb_ctx_t));
	ctx.peer = *peer;
	ctx.current_state = ps_state;
	ctx.previous_state = prev_state;
	ctx.uniq_id = *uniq_id;
	ctx.event_type = event_type;

	/* Get additional info from cache */
	if(get_peer_info(peer, NULL, &call_count, &is_registered) == 0) {
		ctx.call_count = call_count;
		ctx.is_registered = is_registered;
	} else {
		LM_WARN("[%.*s] failed to get peer info for callbacks: %.*s, check "
				"cache_expire time may be too short\n",
				uniq_id->len, uniq_id->s, peer->len, peer->s);
	}

	/* Trigger callbacks */
	trigger_peerstate_callbacks(&ctx);
}

static void __dialog_callback(
		struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
	struct ps_dlginfo_cell *dlginfo = (struct ps_dlginfo_cell *)*_params->param;
	struct sip_msg *request = _params->req;
	ps_state_t new_state;
	int notify_caller, notify_callee;
	const char *event_name;

	if(!dlg || !dlginfo)
		return;

	if(request && (request->REQ_METHOD & (METHOD_PRACK | METHOD_UPDATE)))
		return;

	if(disable_dlg_notify > 0)
		return;

	switch(type) {
		case DLGCB_FAILED:
		case DLGCB_TERMINATED:
		case DLGCB_EXPIRED:
			new_state = NOT_INUSE;
			event_name = "ENDED";
			break;

		case DLGCB_CONFIRMED:
		case DLGCB_REQ_WITHIN:
		case DLGCB_CONFIRMED_NA:
			new_state = INUSE;
			event_name = "CONFIRMED";
			break;

		case DLGCB_EARLY:
			new_state = RINGING;
			event_name = "EARLY";
			break;

		default:
			LM_DBG("[%.*s] Unhandled dialog type: %d\n", dlginfo->callid.len,
					dlginfo->callid.s, type);
			return;
	}

	LM_DBG("[%.*s] Dialog %s: %.*s -> %.*s\n", dlginfo->callid.len,
			dlginfo->callid.s, event_name, dlginfo->from->user.len,
			dlginfo->from->user.s, dlginfo->to->user.len, dlginfo->to->user.s);

	notify_caller = should_notify(dlginfo->disable_caller_notify,
			disable_caller_notify_flag, request);
	notify_callee = should_notify(dlginfo->disable_callee_notify,
			disable_callee_notify_flag, request);

	if(notify_caller && dlginfo->caller_peers)
		process_dialog_peers(dlginfo->caller_peers, new_state,
				enable_notify_on_trying, &dlginfo->callid);

	if(notify_callee && dlginfo->callee_peers)
		process_dialog_peers(dlginfo->callee_peers, new_state,
				enable_notify_on_trying, &dlginfo->callid);
}

static void __dialog_created(
		struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
	struct sip_msg *request = _params->req;
	struct ps_dlginfo_cell *dlginfo;

	int disable_caller_notify = 0;
	int disable_callee_notify = 0;

	if(request == NULL || request->REQ_METHOD != METHOD_INVITE)
		return;

	LM_DBG("new INVITE dialog created: from=%.*s\n", dlg->from_uri.len,
			dlg->from_uri.s);

	if(disable_caller_notify_flag != -1
			&& (request
					&& (request->flags & (1 << disable_caller_notify_flag))))
		disable_caller_notify = 1;

	if(disable_callee_notify_flag != -1
			&& (request
					&& (request->flags & (1 << disable_callee_notify_flag))))
		disable_callee_notify = 1;

	dlginfo = get_dialog_data(
			dlg, type, disable_caller_notify, disable_callee_notify);

	if(dlginfo == NULL) {
		LM_WARN("failed to get dialog data for new dialog\n");
		return;
	}
}

static void __dialog_loaded(
		struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
	struct sip_msg *request = _params->req;
	struct ps_dlginfo_cell *dlginfo;

	if(request == NULL || request->REQ_METHOD != METHOD_INVITE)
		return;

	LM_INFO("INVITE dialog loaded: from=%.*s\n", dlg->from_uri.len,
			dlg->from_uri.s);

	dlginfo = get_dialog_data(dlg, type, 0, 0);

	if(dlginfo == NULL) {
		LM_WARN("failed to load dialog data for existing dialog\n");
		return;
	}
}

static void __usrloc_changed(ucontact_t *c, int type, void *param)
{
	str peer = {NULL, 0};

	peer.len = c->aor->len;
	peer.s = c->aor->s;

	if(disable_reg_notify > 0) {
		LM_DBG("Reginfo notify disabled for peer(%.*s)\n", peer.len, peer.s);
		return;
	}

	int res;
	udomain_t *domain;
	urecord_t *record = NULL;
	int state_changed;
	char *state_meaning = NULL;
	ps_state_t prev_state;

	if(type & UL_CONTACT_INSERT)
		state_meaning = "UL_CONTACT_INSERT";
	else if(type & UL_CONTACT_EXPIRE)
		state_meaning = "UL_CONTACT_EXPIRE";
	else if(type & UL_CONTACT_UPDATE)
		state_meaning = "UL_CONTACT_UPDATE";
	else if(type & UL_CONTACT_DELETE)
		state_meaning = "UL_CONTACT_DELETE";
	else
		return;

	LM_DBG("Current Contact Ruid : %.*s, Peer: %.*s, State Meaning : %s\n",
			c->ruid.len, c->ruid.s, peer.len, peer.s,
			state_meaning ? state_meaning : "Unknown");

	/* Get the UDomain for this account */
	res = ul_api.get_udomain(c->domain->s, &domain);
	if(res < 0) {
		LM_ERR("no domain found\n");
		return;
	}

	/* Get the URecord for this AOR */
	res = ul_api.get_urecord(domain, &peer, &record);
	if(res > 0) {
		LM_ERR("' %.*s (%.*s)' Not found in usrloc\n", c->aor->len, c->aor->s,
				c->domain->len, c->domain->s);
		return;
	}

	if(!record)
		return;

	str cur_ruid = c->ruid;

	int valid_contact_counter = 0;
	time_t cur_time = time(0);
	ucontact_t *ptr;
	ptr = record->contacts;
	ps_state_t ps_state = NA;

	if(type & (UL_CONTACT_INSERT)) {
		while(ptr) {
			if(STR_EQ(c->ruid, ptr->ruid) || !(VALID_CONTACT(ptr, cur_time))) {
				// ptr null or itself or not valid contact
				ptr = ptr->next;
				continue;
			}

			print_ucontact(ptr);
			valid_contact_counter++;
			ptr = ptr->next;
		}
		ps_state = NOT_INUSE;
	} else if(type & (UL_CONTACT_UPDATE)) {
		while(ptr) {
			if(STR_EQ(c->ruid, ptr->ruid)) {
				if(!(VALID_CONTACT(ptr, cur_time))) {
					print_ucontact(ptr);
					valid_contact_counter++;
					ptr = ptr->next;
					continue;
				}
				ptr = ptr->next;
				continue;
			}

			if(!(VALID_CONTACT(ptr, cur_time))) {
				ptr = ptr->next;
				continue;
			}

			print_ucontact(ptr);
			valid_contact_counter++;
			ptr = ptr->next;
		}
		ps_state = NOT_INUSE;
	} else if(type & (UL_CONTACT_DELETE)) {
		while(ptr) {
			if(STR_EQ(c->ruid, ptr->ruid) || !(VALID_CONTACT(ptr, cur_time))) {
				ptr = ptr->next;
				continue;
			}

			print_ucontact(ptr);
			valid_contact_counter++;
			ptr = ptr->next;
		}
		ps_state = UNAVAILABLE;
	} else if(type & (UL_CONTACT_EXPIRE)) {
		while(ptr) {
			if(STR_EQ(c->ruid, ptr->ruid) || !(VALID_CONTACT(ptr, cur_time))) {
				ptr = ptr->next;
				continue;
			}

			print_ucontact(ptr);
			valid_contact_counter++;
			ptr = ptr->next;
		}
		ps_state = UNAVAILABLE;
	}

	if(use_avps && get_avp_first_str(reg_avp_type, reg_avp_name, &peer) != 0)
		LM_WARN("Failed to get reg_avp(%s) for peer(%.*s)\n", reg_avp, peer.len,
				peer.s);

	if(valid_contact_counter == 0) {
		state_changed = update_peer_state_usrloc(&peer, ps_state, &prev_state);
		if(state_changed > 0) {
			LM_INFO("Peer '%.*s' state changed from %s to %s\n", peer.len,
					peer.s, PS_STATE_TO_STR(prev_state),
					PS_STATE_TO_STR(ps_state));
			notify_with_callbacks(ps_state, prev_state, &peer, &cur_ruid,
					PEERSTATE_EVENT_REGISTRATION);
		}
	} else
		LM_DBG("Reginfo state(%s) notify disabled (multiple active contacts) "
			   "for peer(%.*s) with ruid(%.*s)\n",
				state_meaning ? state_meaning : "Unknown", peer.len, peer.s,
				cur_ruid.len, cur_ruid.s);

	ul_api.release_urecord(record);
}

static struct ps_dlginfo_cell *get_dialog_data(struct dlg_cell *dlg, int type,
		int disable_caller_notify, int disable_callee_notify)
{
	struct ps_dlginfo_cell *dlginfo;
	int len;

	// create dlginfo structure to store important data inside the module
	len = sizeof(struct ps_dlginfo_cell) + dlg->from_uri.len + dlg->to_uri.len
		  + dlg->callid.len;
	dlginfo = (struct ps_dlginfo_cell *)shm_malloc(len);

	if(dlginfo == 0) {
		SHM_MEM_ERROR;
		return NULL;
	}
	memset(dlginfo, 0, len);

	// copy from dlg structure to dlginfo structure
	dlginfo->disable_caller_notify = disable_caller_notify;
	dlginfo->disable_callee_notify = disable_callee_notify;
	dlginfo->callid.s = (char *)dlginfo + sizeof(struct ps_dlginfo_cell);
	dlginfo->callid.len = dlg->callid.len;

	memcpy(dlginfo->callid.s, dlg->callid.s, dlg->callid.len);

	struct sip_uri *furi = (struct sip_uri *)shm_malloc(sizeof(struct sip_uri));
	if(!furi) {
		LM_ERR("failed to allocate From URI\n");
		SHM_MEM_ERROR;
		free_ps_dlginfo_cell(dlginfo);
		return NULL;
	}

	if(parse_uri(dlg->from_uri.s, dlg->from_uri.len, furi) < 0) {
		LM_ERR("failed to parse From URI\n");
		free_ps_dlginfo_cell(dlginfo);
		shm_free(furi);
		return NULL;
	}

	struct sip_uri *turi = (struct sip_uri *)shm_malloc(sizeof(struct sip_uri));

	if(!turi) {
		LM_ERR("failed to allocate To URI\n");
		SHM_MEM_ERROR;
		free_ps_dlginfo_cell(dlginfo);
		shm_free(furi);
		return NULL;
	}

	if(parse_uri(dlg->to_uri.s, dlg->to_uri.len, turi) < 0) {
		LM_ERR("failed to parse To URI\n");
		free_ps_dlginfo_cell(dlginfo);
		shm_free(turi);
		shm_free(furi);
		return NULL;
	}

	dlginfo->from = furi;
	dlginfo->to = turi;

	if(type == DLGCB_CREATED) {
		if(use_avps) {
			struct str_list *caller_peers_from_avp =
					get_avp_str_list(caller_avp_type, caller_avp_name);
			struct str_list *callee_peers_from_avp =
					get_avp_str_list(callee_avp_type, callee_avp_name);

			if(caller_peers_from_avp)
				dlginfo->caller_peers = caller_peers_from_avp;
			else {
				LM_WARN("failed to get caller peers from AVP\n");
				// build "peer@host" from from/to uri
				dlginfo->caller_peers = peer_at_host_list(dlginfo->from);
				if(!dlginfo->caller_peers) {
					LM_ERR("failed to build caller peer@host\n");
					free_ps_dlginfo_cell(dlginfo);
					return NULL;
				}
			}

			if(callee_peers_from_avp)
				dlginfo->callee_peers = callee_peers_from_avp;
			else {
				LM_WARN("failed to get callee peers from AVP\n");
				// build "peer@host" from from/to uri
				dlginfo->callee_peers = peer_at_host_list(dlginfo->to);
				if(!dlginfo->callee_peers) {
					LM_ERR("failed to build callee peer@host\n");
					free_ps_dlginfo_cell(dlginfo);
					return NULL;
				}
			}
		} else {
			// build "peer@host" from from/to uri
			dlginfo->caller_peers = peer_at_host_list(dlginfo->from);
			if(!dlginfo->caller_peers) {
				LM_ERR("failed to build caller peer@host\n");
				free_ps_dlginfo_cell(dlginfo);
				return NULL;
			}

			dlginfo->callee_peers = peer_at_host_list(dlginfo->to);
			if(!dlginfo->callee_peers) {
				LM_ERR("failed to build callee peer@host\n");
				free_ps_dlginfo_cell(dlginfo);
				return NULL;
			}
		}

		if(caller_dlg_var.len > 0) {
			if(set_dlg_var(dlg, &caller_dlg_var, dlginfo->caller_peers) < 0) {
				free_str_list_all(dlginfo->caller_peers);
				dlginfo->caller_peers = NULL;
			}
		}

		if(callee_dlg_var.len > 0) {
			if(set_dlg_var(dlg, &callee_dlg_var, dlginfo->callee_peers) < 0) {
				free_str_list_all(dlginfo->callee_peers);
				dlginfo->callee_peers = NULL;
			}
		}
	} else {
		// no need to init caller_peers and callee_peers, will be taken from dlg var.
		if(caller_dlg_var.len > 0) {
			if(get_dlg_var(dlg, &caller_dlg_var, &dlginfo->caller_peers) < 0) {
				free_ps_dlginfo_cell(dlginfo);
				return NULL;
			}
		}

		if(callee_dlg_var.len > 0) {
			if(get_dlg_var(dlg, &callee_dlg_var, &dlginfo->callee_peers) < 0) {
				free_ps_dlginfo_cell(dlginfo);
				return NULL;
			}
		}
	}

	// register dialog callbacks to handle changing dialog state
	if(dlg_api.register_dlgcb(dlg, dialog_event_types, __dialog_callback,
			   dlginfo, free_ps_dlginfo_cell)
			!= 0) {
		LM_ERR("cannot register callback for interesting dialog types\n");
		free_ps_dlginfo_cell(dlginfo);
		return NULL;
	}

	return (dlginfo);
}

static inline void free_ps_dlginfo_cell(void *param)
{
	struct ps_dlginfo_cell *cell = NULL;

	if(param == NULL)
		return;

	cell = param;
	if(cell->from)
		shm_free(cell->from);
	if(cell->to)
		shm_free(cell->to);
	free_str_list_all(cell->caller_peers);
	free_str_list_all(cell->callee_peers);

	shm_free(param);
}

static inline void free_str_list_all(struct str_list *del_current)
{
	struct str_list *del_next;
	while(del_current) {
		del_next = del_current->next;

		if(del_current->s.s)
			shm_free(del_current->s.s);

		shm_free(del_current);
		del_current = del_next;
	}
}

static inline int get_avp_first_str(
		unsigned short avp_flags, int_str avp_name, str *result)
{
	int_str avp_value;
	struct search_state st;

	if(!result)
		return -1;

	LM_DBG("AVP Name received '%.*s'\n", avp_name.s.len, avp_name.s.s);

	if(!search_first_avp(avp_flags, avp_name, &avp_value, &st)) {
		return -1;
	}

	LM_DBG("AVP found '%.*s'\n", avp_value.s.len, avp_value.s.s);

	result->s = avp_value.s.s;
	result->len = avp_value.s.len;

	return 0;
}

static struct str_list *get_avp_str_list(
		unsigned short avp_flags, int_str avp_name)
{
	int_str avp_value;
	struct str_list *list_first = NULL;
	struct str_list *list_current = NULL;
	struct str_list *new_node = NULL;
	struct search_state st;

	LM_DBG("AVP Name received '%.*s'\n", avp_name.s.len, avp_name.s.s);

	if(!search_first_avp(avp_flags, avp_name, &avp_value, &st)) {
		LM_DBG("AVP '%.*s' not found\n", avp_name.s.len, avp_name.s.s);
		return NULL;
	}

	do {
		LM_DBG("AVP found '%.*s'\n", avp_value.s.len, avp_value.s.s);
		new_node = (struct str_list *)shm_malloc(sizeof(struct str_list));
		if(!new_node) {
			SHM_MEM_ERROR;
			free_str_list_all(list_first);
			return NULL;
		}

		memset(new_node, 0, sizeof(struct str_list));

		new_node->s.s = shm_str2char_dup(&avp_value.s);
		if(!new_node->s.s) {
			shm_free(new_node); /* Free the newly allocated node */
			free_str_list_all(list_first);
			return NULL;
		}

		new_node->s.len = avp_value.s.len;

		if(list_current)
			list_current->next = new_node;
		else
			list_first = new_node;

		list_current = new_node;
	} while(search_next_avp(&st, &avp_value));
	return list_first;
}

static int set_dlg_var(struct dlg_cell *dlg, str *key, struct str_list *lst)
{
	str buf = STR_NULL;
	struct str_list *it = lst;
	int res;
	int count = 0;

	if(!lst)
		return -1;

	buf.len = 0;
	while(it) {
		buf.len += it->s.len;
		count++;
		it = it->next;
	}
	if(count > 1)
		buf.len += (count - 1);

	buf.s = (char *)pkg_malloc(buf.len + 1);
	if(!buf.s) {
		PKG_MEM_ERROR;
		return -1;
	}

	it = lst;
	int pos = 0;
	while(it) {
		memcpy(buf.s + pos, it->s.s, it->s.len);
		pos += it->s.len;
		if(it->next)
			buf.s[pos++] = *DLG_VAR_SEP;
		it = it->next;
	}
	buf.s[buf.len] = '\0';

	res = dlg_api.set_dlg_var(dlg, key, &buf);
	pkg_free(buf.s);
	return res;
}

static int get_dlg_var(struct dlg_cell *dlg, str *key, struct str_list **lst)
{
	str dval = STR_NULL;
	str val = STR_NULL;
	struct str_list *it, *prev;
	char *sep, *ini, *end;

	if(dlg_api.get_dlg_varval(dlg, key, &dval) != 0 || dval.s == NULL)
		return 0;

	if(*lst)
		free_str_list_all(*lst);

	*lst = prev = NULL;
	ini = dval.s;
	end = dval.s + dval.len - 1;

	while(ini <= end) {
		sep = stre_search_strz(ini, end, DLG_VAR_SEP);

		if(!sep || sep > end) {
			val.s = ini;
			val.len = end - ini + 1;
		} else {
			val.s = ini;
			val.len = sep - ini;
		}

		if(val.len <= 0) {
			if(!sep || sep > end)
				break;
			ini = sep + 1;
			continue;
		}

		it = (struct str_list *)shm_malloc(sizeof(struct str_list));
		if(!it) {
			SHM_MEM_ERROR;
			free_str_list_all(*lst);
			return -1;
		}

		memset(it, 0, sizeof(struct str_list));
		it->s.s = shm_str2char_dup(&val);
		if(!it->s.s) {
			shm_free(it);
			free_str_list_all(*lst);
			return -1;
		}

		it->s.len = val.len;

		if(!*lst)
			*lst = prev = it;
		else {
			prev->next = it;
			prev = it;
		}

		if(!sep || sep > end)
			break;
		ini = sep + 1;
	}
	return 0;
}

/**
 * init module function
 */
static int mod_init(void)
{
	LM_DBG("mod_init worked!\n");
	str s;
	pv_spec_t avp_spec;
	bind_usrloc_t bind_usrloc;

	if(init_peerstate_callbacks() < 0) {
		LM_ERR("failed to initialize callback system\n");
		return -1;
	}

	if(init_peerstate_cache(
			   cache_expire, cache_cleanup_interval, cache_hash_size)
			< 0) {
		LM_ERR("failed to initialize peerstate cache\n");
		return -1;
	}
	LM_DBG("Peerstate cache enabled\n");

	if(caller_dlg_var.len <= 0)
		LM_WARN("pubruri_caller_dlg_var is not set - restore on restart "
				"disabled\n");

	if(callee_dlg_var.len <= 0)
		LM_WARN("pubruri_callee_dlg_var is not set - restore on restart "
				"disabled\n");

	/* bind to the dialog API */
	if(load_dlg_api(&dlg_api) != 0) {
		LM_ERR("failed to find dialog API - is dialog module loaded?\n");
		return -1;
	}

	bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);
	if(!bind_usrloc) {
		LM_ERR("Failed to bind usrloc - is usrloc module loaded?\n");
		return -1;
	}

	if(bind_usrloc(&ul_api) < 0) {
		LM_ERR("Failed to bind usrloc - is usrloc module loaded?\n");
		return -1;
	}

	peerstate_event_rt = route_lookup(&event_rt, PEERSTATE_EVENT_ROUTE);
	if(peerstate_event_rt < 0)
		LM_INFO("event_route[%s] not defined, notifications via event route "
				"disabled\n",
				PEERSTATE_EVENT_ROUTE);
	else
		LM_INFO("event_route[%s] registered\n", PEERSTATE_EVENT_ROUTE);

	if(disable_dlg_notify <= 0) {
		/* register dialog creation callback */
		if(dlg_api.register_dlgcb(
				   NULL, DLGCB_CREATED, __dialog_created, NULL, NULL)
				!= 0) {
			LM_ERR("cannot register callback for dialog creation\n");
			return -1;
		}

		/* register dialog loaded callback */
		if(dlg_api.register_dlgcb(
				   NULL, DLGCB_LOADED, __dialog_loaded, NULL, NULL)
				!= 0) {
			LM_ERR("cannot register callback for dialog loaded\n");
			return -1;
		}
	}

	if(disable_reg_notify <= 0) {
		if(ul_api.register_ulcb(UL_CONTACT_INSERT | UL_CONTACT_EXPIRE
										| UL_CONTACT_UPDATE | UL_CONTACT_DELETE,
				   __usrloc_changed, 0)
				< 0) {
			LM_ERR("cannot register usrloc callbacks\n");
			return -1;
		}
	}

	if(use_avps) {
		LM_INFO("configured to use avps for uri values\n");
		if((caller_avp == NULL || *caller_avp == 0)
				|| (callee_avp == NULL || *callee_avp == 0)) {
			LM_ERR("caller_avp and callee_avp must be set, if use_avps is "
				   "enabled\n");
			return -1;
		}

		s.s = caller_avp;
		s.len = strlen(s.s);
		if(pv_parse_spec(&s, &avp_spec) == 0 || avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non AVP %s AVP definition\n", caller_avp);
			return -1;
		}

		if(pv_get_avp_name(0, &avp_spec.pvp, &caller_avp_name, &caller_avp_type)
				!= 0) {
			LM_ERR("[%s]- invalid AVP definition\n", caller_avp);
			return -1;
		}

		s.s = callee_avp;
		s.len = strlen(s.s);
		if(pv_parse_spec(&s, &avp_spec) == 0 || avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non AVP %s AVP definition\n", callee_avp);
			return -1;
		}
		if(pv_get_avp_name(0, &avp_spec.pvp, &callee_avp_name, &callee_avp_type)
				!= 0) {
			LM_ERR("[%s]- invalid AVP definition\n", callee_avp);
			return -1;
		}

		s.s = reg_avp;
		s.len = strlen(s.s);
		if(pv_parse_spec(&s, &avp_spec) == 0 || avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non AVP %s AVP definition\n", reg_avp);
			return -1;
		}
		if(pv_get_avp_name(0, &avp_spec.pvp, &reg_avp_name, &reg_avp_type)
				!= 0) {
			LM_ERR("[%s]- invalid AVP definition\n", reg_avp);
			return -1;
		}
	} else
		LM_INFO("configured to use headers for uri values\n");

	return 0;
}

/**
 * @brief Initialize module children
 */
static int child_init(int rank)
{
	if(rank != PROC_MAIN) {
		return 0;
	}

	return 0;
}

/**
 * @brief Module cleanup function
 */
static void mod_destroy(void)
{
	LM_INFO("peerstate module cleanup started\n");
	destroy_peerstate_callbacks();
	destroy_peerstate_cache();
	LM_INFO("peerstate module cleanup completed\n");
}

static inline int should_notify(
		int disable_notify, int disable_flag, struct sip_msg *request)
{
	if(disable_notify)
		return 0;

	if(disable_flag == -1)
		return 1;

	if(request && (request->flags & (1 << disable_flag)))
		return 0;

	return 1;
}

static void process_dialog_peers(struct str_list *peers, ps_state_t new_state,
		int early_enabled, str *callid)
{
	struct str_list *peer = peers;
	ps_state_t prev_state;
	int state_changed;

	while(peer) {
		state_changed = update_peer_state_dialog(
				&peer->s, new_state, early_enabled, &prev_state);
		if(state_changed > 0) {
			LM_DBG("[%.*s] '%.*s': %s -> %s\n", callid->len, callid->s,
					peer->s.len, peer->s.s, PS_STATE_TO_STR(prev_state),
					PS_STATE_TO_STR(new_state));
			notify_with_callbacks(new_state, prev_state, &peer->s, callid,
					PEERSTATE_EVENT_DIALOG);
		}
		peer = peer->next;
	}
}

// helper: build a shm-allocated str_list node with "peer@host"
static struct str_list *peer_at_host_list(struct sip_uri *uri)
{
	struct str_list *n;
	int len;
	char *p;

	if(uri == NULL || uri->user.s == NULL || uri->user.len <= 0
			|| uri->host.s == NULL || uri->host.len <= 0)
		return NULL;

	n = (struct str_list *)shm_malloc(sizeof(struct str_list));
	if(!n) {
		SHM_MEM_ERROR;
		return NULL;
	}
	memset(n, 0, sizeof(struct str_list));

	len = uri->user.len + 1 + uri->host.len; /* peer + '@' + host */
	n->s.s = (char *)shm_malloc(len + 1);
	if(!n->s.s) {
		SHM_MEM_ERROR;
		shm_free(n);
		return NULL;
	}

	p = n->s.s;
	memcpy(p, uri->user.s, uri->user.len);
	p += uri->user.len;
	*p++ = '@';
	memcpy(p, uri->host.s, uri->host.len);
	p += uri->host.len;
	*p = '\0';

	n->s.len = len;
	n->next = NULL;
	return n;
}

static inline void print_ucontact(ucontact_t *c)
{
	time_t now = time(NULL);
	if(!c) {
		LM_ERR("Contact is NULL\n");
		return;
	}
	LM_DBG("CONTACT: aor=%.*s contact=%.*s ruid=%.*s expires=%lu "
		   "remaining=%ld(s) callid=%.*s ua=%.*s\n",
			c->aor ? c->aor->len : 0, c->aor ? c->aor->s : "", c->c.len, c->c.s,
			c->ruid.len, c->ruid.s, (unsigned long)c->expires,
			(c->expires > now) ? (long)(c->expires - now) : 0, c->callid.len,
			c->callid.s, c->user_agent.len, c->user_agent.s);
}

/**
 * @brief RPC: peerstate.get_peer <peer>
 * Get state info for specific peer
 */
static void rpc_get_peer(rpc_t *rpc, void *ctx)
{
	str peer;
	ps_state_t state;
	int call_count, is_registered;

	if(rpc->scan(ctx, "S", &peer) < 1) {
		rpc->fault(ctx, 400, "Peer parameter required");
		return;
	}

	if(get_peer_info(&peer, &state, &call_count, &is_registered) < 0) {
		rpc->fault(ctx, 404, "Peer not found");
		return;
	}

	if(rpc->add(ctx, "{", &ctx) < 0)
		return;
	rpc->struct_add(ctx, "Ssds", "peer", &peer, "state", PS_STATE_TO_STR(state),
			"call_count", call_count, "registered",
			is_registered ? "yes" : "no");
}

/**
 * @brief RPC: peerstate.stats
 * Get cache statistics
 */
static void rpc_stats(rpc_t *rpc, void *ctx)
{
	int total, incall, registered;

	get_cache_stats(&total, &incall, &registered);

	if(rpc->add(ctx, "{", &ctx) < 0)
		return;
	rpc->struct_add(ctx, "ddd", "total_peers", total, "incall_peers", incall,
			"registered_peers", registered);
}

static void rpc_list(rpc_t *rpc, void *ctx)
{
	str state_str;
	ps_state_t state;
	str *peers = NULL;
	int count, i;

	void *root_ctx;
	void *peers_obj_ctx;
	void *peer_arr_ctx;
	void *peer_ctx;

	ps_state_t ps_state;
	int call_count, is_registered;

	if(rpc->scan(ctx, "S", &state_str) < 1) {
		rpc->fault(ctx, 400,
				"State parameter required (NOT_INUSE, INUSE, RINGING, "
				"UNAVAILABLE)");
		return;
	}

	if(strncasecmp(state_str.s, "NOT_INUSE", 9) == 0)
		state = NOT_INUSE;
	else if(strncasecmp(state_str.s, "INUSE", 5) == 0)
		state = INUSE;
	else if(strncasecmp(state_str.s, "RINGING", 7) == 0)
		state = RINGING;
	else if(strncasecmp(state_str.s, "UNAVAILABLE", 11) == 0)
		state = UNAVAILABLE;
	else {
		rpc->fault(ctx, 400,
				"Invalid state (use: NOT_INUSE, INUSE, RINGING, UNAVAILABLE)");
		return;
	}

	if(get_peers_by_state(state, &peers, &count) < 0) {
		rpc->fault(ctx, 500, "Failed to get peers");
		return;
	}

	if(rpc->add(ctx, "{", &root_ctx) < 0)
		goto cleanup;

	rpc->struct_add(root_ctx, "{", "Peers", &peers_obj_ctx);

	rpc->struct_add(peers_obj_ctx, "dS", "count", count, "state", &state_str);

	rpc->struct_add(peers_obj_ctx, "[", "Peer", &peer_arr_ctx);

	for(i = 0; i < count; i++) {
		rpc->array_add(peer_arr_ctx, "{", &peer_ctx);

		if(get_peer_info(&peers[i], &ps_state, &call_count, &is_registered)
				< 0) {
			rpc->struct_add(peer_ctx, "Ssds", "name", &peers[i], "state",
					"UNKNOWN", "call_count", 0, "registered", "no");
		} else {
			rpc->struct_add(peer_ctx, "Ssds", "name", &peers[i], "state",
					PS_STATE_TO_STR(ps_state), "call_count", call_count,
					"registered", is_registered ? "yes" : "no");
		}
	}

cleanup:
	for(i = 0; i < count; i++)
		pkg_free(peers[i].s);
	pkg_free(peers);
}

/**
 * @brief RPC: peerstate.dump
 * List all peers
 */
static void rpc_dump(rpc_t *rpc, void *ctx)
{
	str *peers = NULL;
	int count, i;

	void *root_ctx;
	void *peers_obj_ctx;
	void *peer_arr_ctx;
	void *peer_ctx;

	ps_state_t state;
	int call_count, is_registered;

	if(get_all_peers(&peers, &count) < 0) {
		rpc->fault(ctx, 500, "Failed to get peers");
		return;
	}

	if(rpc->add(ctx, "{", &root_ctx) < 0)
		goto cleanup;

	rpc->struct_add(root_ctx, "{", "Peers", &peers_obj_ctx);

	rpc->struct_add(peers_obj_ctx, "d", "count", count);

	rpc->struct_add(peers_obj_ctx, "[", "Peer", &peer_arr_ctx);

	for(i = 0; i < count; i++) {
		rpc->array_add(peer_arr_ctx, "{", &peer_ctx);

		if(get_peer_info(&peers[i], &state, &call_count, &is_registered) < 0) {
			rpc->struct_add(peer_ctx, "Ssds", "name", &peers[i], "state",
					"UNKNOWN", "call_count", 0, "registered", "no");
		} else {
			rpc->struct_add(peer_ctx, "Ssds", "name", &peers[i], "state",
					PS_STATE_TO_STR(state), "call_count", call_count,
					"registered", is_registered ? "yes" : "no");
		}
	}

cleanup:
	for(i = 0; i < count; i++)
		pkg_free(peers[i].s);
	pkg_free(peers);
}
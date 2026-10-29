/*
 * ims_qos_n5 - P-CSCF media authorisation over N5 (Npcf_PolicyAuthorization)
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
 */

/*
 * The P-CSCF as an AF of the 5G core, talking N5 (TS 29.514
 * Npcf_PolicyAuthorization) to the PCF instead of Rx to a PCRF:
 *
 *  N5_create(direction[, subscription_id])   onreply_route of INVITE/UPDATE/
 *      PRACK with an SDP answer: creates the app-session (POST app-sessions)
 *      on the first call for a dialog and direction, updates it (PATCH)
 *      afterwards; the app-session is deleted (POST .../delete) when the
 *      dialog ends.
 *  N5_notification(path, body)   in event_route[nghttp2:request] of the
 *      nghttp2 module: handles the PCF's {notifUri}/terminate (the dialog is
 *      released, as ims_qos does on an Rx ASR) and {notifUri}/notify.
 *
 * The notifUri given to the PCF is <notif_uri>/<h_entry>-<h_id>-<o|t>, so a
 * callback identifies its dialog without module state.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/mod_fix.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/kemi.h"
#include "../../core/timer_proc.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_from.h"
#include "../../lib/ims/ims_getters.h"
#include "../tm/tm_load.h"
#include "../ims_dialog/dlg_load.h"
#include "../ims_dialog/dlg_hash.h"

#include "n5_http.h"
#include "n5_media.h"
#include "n5_oauth.h"

MODULE_VERSION

/* parameters */
static str n5_pcf_uri = STR_NULL;
static str n5_notif_uri = STR_NULL;
static str n5_af_app_id = str_init("IMS Services");
static str n5_dnn = STR_NULL;
static str n5_events =
		str_init("SUCCESSFUL_RESOURCES_ALLOCATION,FAILED_RESOURCES_ALLOCATION");
static str n5_release_headers = STR_NULL;
int n5_http_timeout = 2000;
int n5_http2 = 1;
int n5_audio_default_bw = 64;
int n5_video_default_bw = 128;
int n5_authorize_video = 1;
int n5_include_rtcp = 0;
/* OAuth2 (TS 29.500 clause 6.7): NRF registration and access tokens */
int n5_oauth = 0;
str n5_nrf_uri = STR_NULL;
str n5_nf_instance_id = STR_NULL;
str n5_nf_type = str_init("AF");
str n5_nf_ipv4 = STR_NULL;
int n5_heartbeat_interval = 30;

static struct tm_binds tmb;
static ims_dlg_api_t dlgb;

static str n5_key_orig = str_init("n5_app_orig");
static str n5_key_term = str_init("n5_app_term");

/* per dialog and direction: the app-session to delete when it ends */
typedef struct n5_sess
{
	int deleted;
	str uri; /* points behind the struct */
} n5_sess_t;

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);
static int w_n5_create1(struct sip_msg *msg, char *dir, char *p2);
static int w_n5_create2(struct sip_msg *msg, char *dir, char *sid);
static int w_n5_notification(struct sip_msg *msg, char *path, char *body);

/* clang-format off */
static cmd_export_t cmds[] = {
	{"N5_create", (cmd_function)w_n5_create1, 1, fixup_spve_null,
		fixup_free_spve_null, ONREPLY_ROUTE},
	{"N5_create", (cmd_function)w_n5_create2, 2, fixup_spve_spve,
		fixup_free_spve_spve, ONREPLY_ROUTE},
	{"N5_notification", (cmd_function)w_n5_notification, 2, fixup_spve_spve,
		fixup_free_spve_spve, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"pcf_uri", PARAM_STR, &n5_pcf_uri},
	{"notif_uri", PARAM_STR, &n5_notif_uri},
	{"af_app_id", PARAM_STR, &n5_af_app_id},
	{"dnn", PARAM_STR, &n5_dnn},
	{"events", PARAM_STR, &n5_events},
	{"release_headers", PARAM_STR, &n5_release_headers},
	{"http_timeout", PARAM_INT, &n5_http_timeout},
	{"http2", PARAM_INT, &n5_http2},
	{"audio_default_bandwidth", PARAM_INT, &n5_audio_default_bw},
	{"video_default_bandwidth", PARAM_INT, &n5_video_default_bw},
	{"authorize_video_flow", PARAM_INT, &n5_authorize_video},
	{"include_rtcp_fd", PARAM_INT, &n5_include_rtcp},
	{"oauth", PARAM_INT, &n5_oauth},
	{"nrf_uri", PARAM_STR, &n5_nrf_uri},
	{"nf_instance_id", PARAM_STR, &n5_nf_instance_id},
	{"nf_type", PARAM_STR, &n5_nf_type},
	{"nf_ipv4", PARAM_STR, &n5_nf_ipv4},
	{"heartbeat_interval", PARAM_INT, &n5_heartbeat_interval},
	{0, 0, 0}
};

struct module_exports exports = {
	"ims_qos_n5",    /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions */
	params,          /* exported parameters */
	0,               /* RPC method exports */
	0,               /* exported pseudo-variables */
	0,               /* response handling function */
	mod_init,        /* module initialization function */
	child_init,      /* per-child init function */
	mod_destroy      /* module destroy function */
};
/* clang-format on */

static int mod_init(void)
{
	if(n5_pcf_uri.len <= 0) {
		LM_ERR("pcf_uri is not set\n");
		return -1;
	}
	if(n5_notif_uri.len <= 0) {
		LM_ERR("notif_uri is not set\n");
		return -1;
	}
	while(n5_pcf_uri.len > 0 && n5_pcf_uri.s[n5_pcf_uri.len - 1] == '/')
		n5_pcf_uri.len--;
	while(n5_notif_uri.len > 0 && n5_notif_uri.s[n5_notif_uri.len - 1] == '/')
		n5_notif_uri.len--;
	if(load_tm_api(&tmb) != 0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}
	if(load_ims_dlg_api(&dlgb) != 0) {
		LM_ERR("can't load ims_dialog API\n");
		return -1;
	}
	if(n5_oauth_mod_init() < 0)
		return -1;
	/* NRF registration and heartbeat in their own process, so that SIP
	 * workers never wait for the NRF */
	if(n5_oauth)
		register_basic_timers(1);
	return n5_http_mod_init();
}

static int child_init(int rank)
{
	if(rank != PROC_MAIN || !n5_oauth)
		return 0;
	if(fork_basic_timer(PROC_TIMER, "IMS QOS N5 NRF", 1, n5_nrf_timer, NULL, 1)
			< 0) {
		LM_ERR("failed to start the NRF timer process\n");
		return -1;
	}
	return 0;
}

static void mod_destroy(void)
{
	n5_http_mod_destroy();
}

/* ---------------------------------------------------------------- helpers */

static char *n5_strdup0(str *s)
{
	char *p = pkg_malloc(s->len + 1);
	if(p == NULL) {
		PKG_MEM_ERROR;
		return NULL;
	}
	memcpy(p, s->s, s->len);
	p[s->len] = '\0';
	return p;
}

/* AfEventSubscription list from the "events" parameter */
static json_t *n5_ev_subsc(char *notif)
{
	json_t *evs, *sub;
	char *list, *tok, *save = NULL;

	if(n5_events.len <= 0)
		return NULL;
	list = n5_strdup0(&n5_events);
	if(list == NULL)
		return NULL;
	evs = json_array();
	for(tok = strtok_r(list, ", ", &save); tok;
			tok = strtok_r(NULL, ", ", &save)) {
		json_t *e = json_object();
		json_object_set_new(e, "event", json_string(tok));
		json_object_set_new(e, "notifMethod", json_string("EVENT_DETECTION"));
		json_array_append_new(evs, e);
	}
	pkg_free(list);
	sub = json_object();
	json_object_set_new(sub, "events", evs);
	json_object_set_new(sub, "notifUri", json_string(notif));
	return sub;
}

/* subscription id -> supi ("imsi-...") or gpsi ("msisdn-<digits>") */
static void n5_set_identity(json_t *asc, str *sid)
{
	char buf[64];
	int i, n = 0;

	if(sid == NULL || sid->len <= 0)
		return;
	if(sid->len > 5 && strncmp(sid->s, "imsi-", 5) == 0) {
		json_object_set_new(
				asc, "supi", json_stringn(sid->s, (size_t)sid->len));
		return;
	}
	if(sid->len > 7 && strncmp(sid->s, "msisdn-", 7) == 0) {
		json_object_set_new(
				asc, "gpsi", json_stringn(sid->s, (size_t)sid->len));
		return;
	}
	/* tel:+91..., sip:+91...@host, or bare digits */
	n = snprintf(buf, sizeof(buf), "msisdn-");
	for(i = 0; i < sid->len && n < (int)sizeof(buf) - 1; i++) {
		char c = sid->s[i];
		if(c == '@' || c == ';')
			break;
		if(c >= '0' && c <= '9')
			buf[n++] = c;
	}
	buf[n] = '\0';
	if(n > 7)
		json_object_set_new(asc, "gpsi", json_string(buf));
}

/* POST {uri}/delete; 404 means the PCF already removed it (e.g. after
 * /terminate), which is the outcome asked for. */
static void n5_delete(str *uri)
{
	char url[512];
	n5_http_res_t res;

	snprintf(url, sizeof(url), "%.*s/delete", uri->len, uri->s);
	if(n5_http_request("POST", url, NULL, NULL, n5_oauth_pcf_token(), &res)
			< 0) {
		LM_ERR("app-session delete not sent: %s\n", url);
		return;
	}
	if(res.code != 200 && res.code != 204 && res.code != 404)
		LM_ERR("app-session delete %s answered %ld\n", url, res.code);
	n5_http_res_free(&res);
}

static void n5_dlg_cb(struct dlg_cell *dlg, int type, struct dlg_cb_params *p)
{
	n5_sess_t *s;

	if(p == NULL || p->param == NULL)
		return;
	s = (n5_sess_t *)*p->param;
	if(s == NULL || s->deleted)
		return;
	s->deleted = 1;
	LM_DBG("dialog ended (type %d): deleting app-session %.*s\n", type,
			s->uri.len, s->uri.s);
	n5_delete(&s->uri);
}

static void n5_sess_free(void *param)
{
	if(param)
		shm_free(param);
}

/* --------------------------------------------------------------- N5_create */

static int n5_create(struct sip_msg *rpl, str *dir, str *sid)
{
	struct cell *t;
	struct sip_msg req;
	struct dlg_cell *dlg = NULL;
	char *reqbuf = NULL;
	int orig, from_caller, ret = -1;
	str ftag, rpl_ftag, rpl_ttag, callid, *key, *existing;
	json_t *comps = NULL, *doc = NULL, *asc;
	char ue_ip[64] = "", notif[256], url[512], *body = NULL;
	n5_http_res_t res;

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	if(dir->len >= 4 && strncasecmp(dir->s, "orig", 4) == 0)
		orig = 1;
	else if(dir->len >= 4 && strncasecmp(dir->s, "term", 4) == 0)
		orig = 0;
	else {
		LM_ERR("direction must be 'orig' or 'term', not '%.*s'\n", dir->len,
				dir->s);
		return -1;
	}
	if(rpl->first_line.type != SIP_REPLY)
		return -1;
	t = tmb.t_gett();
	if(t == NULL || t == T_UNDEFINED) {
		LM_WARN("no transaction for this reply\n");
		return -1;
	}
	if(!((t->method.len == 6
				 && (memcmp(t->method.s, "INVITE", 6) == 0
						 || memcmp(t->method.s, "UPDATE", 6) == 0))
			   || (t->method.len == 5
					   && memcmp(t->method.s, "PRACK", 5) == 0))) {
		LM_DBG("not a reply to INVITE, UPDATE or PRACK\n");
		return -1;
	}
	if(cscf_get_content_length(rpl) == 0) {
		LM_DBG("no SDP answer in the reply\n");
		return -1;
	}

	/* private copy of the request: parsing the shm copy (t->uas.request)
	 * would attach pkg-allocated header structures to shared memory */
	reqbuf = pkg_malloc(t->uas.request->len + 1);
	if(reqbuf == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(reqbuf, t->uas.request->buf, t->uas.request->len);
	reqbuf[t->uas.request->len] = '\0';
	req.buf = reqbuf;
	req.len = t->uas.request->len;
	req.rcv = t->uas.request->rcv;
	if(parse_msg(req.buf, req.len, &req) != 0) {
		LM_ERR("cannot parse the request of this transaction\n");
		goto done;
	}
	if(cscf_get_content_length(&req) == 0) {
		LM_DBG("no SDP offer in the request\n");
		goto done;
	}

	callid = cscf_get_call_id(rpl, 0);
	if(callid.len <= 0 || !cscf_get_from_tag(rpl, &rpl_ftag)
			|| !cscf_get_to_tag(rpl, &rpl_ttag)) {
		LM_ERR("cannot identify the dialog (Call-ID/tags)\n");
		goto done;
	}
	dlg = dlgb.get_dlg(rpl);
	if(dlg == NULL) {
		LM_ERR("no dialog: create it (dlg_manage) before N5_create\n");
		goto done;
	}

	/* Whose SDP is the UE's: the UE is the caller when originating, the
	 * callee when terminating; a request may come from either end. */
	from_caller = !cscf_get_from_tag(&req, &ftag) || dlg->from_tag.len == 0
				  || (ftag.len == dlg->from_tag.len
						  && memcmp(ftag.s, dlg->from_tag.s, ftag.len) == 0);
	if(orig == from_caller)
		comps = n5_build_med_components(&req, rpl, ue_ip, sizeof(ue_ip));
	else
		comps = n5_build_med_components(rpl, &req, ue_ip, sizeof(ue_ip));
	if(comps == NULL) {
		LM_WARN("no media component to authorise\n");
		goto done;
	}

	key = orig ? &n5_key_orig : &n5_key_term;
	existing = dlgb.get_dlg_var(&callid, &rpl_ftag, &rpl_ttag, key);

	if(existing && existing->len > 0) {
		/* update: AppSessionContextUpdateDataPatch (TS 29.514 4.2.3) */
		asc = json_object();
		json_object_set_new(asc, "medComponents", comps);
		comps = NULL;
		doc = json_object();
		json_object_set_new(doc, "ascReqData", asc);
		body = json_dumps(doc, JSON_COMPACT);
		snprintf(url, sizeof(url), "%.*s", existing->len, existing->s);
		if(body == NULL
				|| n5_http_request("PATCH", url, "application/merge-patch+json",
						   body, n5_oauth_pcf_token(), &res)
						   < 0)
			goto done;
		if(res.code == 200 || res.code == 204) {
			ret = 1;
		} else {
			LM_ERR("app-session update answered %ld: %.*s\n", res.code,
					res.body.len, res.body.s ? res.body.s : "");
		}
		goto done;
	}

	/* create: AppSessionContext (TS 29.514 4.2.2) */
	snprintf(notif, sizeof(notif), "%.*s/%u-%u-%c", n5_notif_uri.len,
			n5_notif_uri.s, dlg->h_entry, dlg->h_id, orig ? 'o' : 't');
	asc = json_object();
	json_object_set_new(asc, "afAppId",
			json_stringn(n5_af_app_id.s, (size_t)n5_af_app_id.len));
	json_object_set_new(asc, "notifUri", json_string(notif));
	json_object_set_new(asc, "suppFeat", json_string("0"));
	if(ue_ip[0])
		json_object_set_new(asc, "ueIpv4", json_string(ue_ip));
	if(n5_dnn.len > 0)
		json_object_set_new(
				asc, "dnn", json_stringn(n5_dnn.s, (size_t)n5_dnn.len));
	n5_set_identity(asc, sid);
	json_object_set_new(asc, "medComponents", comps);
	comps = NULL;
	{
		json_t *ev = n5_ev_subsc(notif);
		if(ev)
			json_object_set_new(asc, "evSubsc", ev);
	}
	doc = json_object();
	json_object_set_new(doc, "ascReqData", asc);
	body = json_dumps(doc, JSON_COMPACT);
	snprintf(url, sizeof(url), "%.*s/npcf-policyauthorization/v1/app-sessions",
			n5_pcf_uri.len, n5_pcf_uri.s);
	if(body == NULL
			|| n5_http_request("POST", url, "application/json", body,
					   n5_oauth_pcf_token(), &res)
					   < 0)
		goto done;
	if(res.code != 201 || res.location.len <= 0) {
		LM_ERR("app-session create answered %ld%s: %.*s\n", res.code,
				res.location.len ? "" : " without Location", res.body.len,
				res.body.s ? res.body.s : "");
		goto done;
	}

	/* remember it for updates and delete it when the dialog ends */
	if(dlgb.set_dlg_var(&callid, &rpl_ftag, &rpl_ttag, key, &res.location) != 0)
		LM_WARN("cannot store the app-session URI in the dialog\n");
	{
		n5_sess_t *s = shm_malloc(sizeof(n5_sess_t) + res.location.len);
		if(s == NULL) {
			SHM_MEM_ERROR;
			n5_delete(&res.location);
			goto done;
		}
		s->deleted = 0;
		s->uri.s = (char *)(s + 1);
		s->uri.len = res.location.len;
		memcpy(s->uri.s, res.location.s, res.location.len);
		if(dlgb.register_dlgcb(dlg,
				   DLGCB_TERMINATED | DLGCB_FAILED | DLGCB_EXPIRED
						   | DLGCB_DESTROY,
				   n5_dlg_cb, s, n5_sess_free)
				!= 0) {
			LM_ERR("cannot register dialog callback\n");
			n5_delete(&res.location);
			shm_free(s);
			goto done;
		}
	}
	LM_DBG("app-session %.*s created for %s\n", res.location.len,
			res.location.s, orig ? "orig" : "term");
	ret = 1;

done:
	if(body)
		free(body); /* jansson allocates with malloc */
	if(doc)
		json_decref(doc);
	if(comps)
		json_decref(comps);
	n5_http_res_free(&res);
	if(dlg)
		dlgb.release_dlg(dlg);
	if(reqbuf) {
		free_sip_msg(&req);
		pkg_free(reqbuf);
	}
	return ret;
}

static int w_n5_create1(struct sip_msg *msg, char *dir, char *p2)
{
	str d;
	if(fixup_get_svalue(msg, (gparam_t *)dir, &d) != 0)
		return -1;
	return n5_create(msg, &d, NULL);
}

static int w_n5_create2(struct sip_msg *msg, char *dir, char *sid)
{
	str d, s;
	if(fixup_get_svalue(msg, (gparam_t *)dir, &d) != 0
			|| fixup_get_svalue(msg, (gparam_t *)sid, &s) != 0)
		return -1;
	return n5_create(msg, &d, &s);
}

/* --------------------------------------------------------- N5_notification */

/* path: <anything>/<h_entry>-<h_id>-<o|t>/<notify|terminate>
 * returns 1 terminate handled, 2 notify accepted, -1 not ours or malformed,
 * -2 dialog gone */
static int n5_notification(str *ppath, str *body)
{
	str path = *ppath, op, id;
	unsigned int h_entry, h_id;
	char side, *p, *q;
	int i;

	/* strip a query string */
	for(i = 0; i < path.len; i++)
		if(path.s[i] == '?') {
			path.len = i;
			break;
		}
	/* last segment = operation, the one before = dialog id */
	p = path.s + path.len;
	while(p > path.s && p[-1] != '/')
		p--;
	op.s = p;
	op.len = (int)(path.s + path.len - p);
	if(p <= path.s + 1)
		return -1;
	q = p - 1;
	while(q > path.s && q[-1] != '/')
		q--;
	id.s = q;
	id.len = (int)(p - 1 - q);
	{
		char tmp[64];
		if(id.len <= 0 || id.len >= (int)sizeof(tmp))
			return -1;
		memcpy(tmp, id.s, id.len);
		tmp[id.len] = '\0';
		if(sscanf(tmp, "%u-%u-%c", &h_entry, &h_id, &side) != 3
				|| (side != 'o' && side != 't'))
			return -1;
	}

	if(op.len == 9 && strncmp(op.s, "terminate", 9) == 0) {
		/* TerminationInfo: the PCF ends the app-session (e.g. PDU session
		 * released); release the call as ims_qos does on an Rx ASR. The
		 * app-session is deleted from the dialog callback. */
		LM_INFO("PCF requested termination of %u-%u-%c: %.*s\n", h_entry, h_id,
				side, body->len, body->s);
		if(dlgb.lookup_terminate_dlg(h_entry, h_id, &n5_release_headers) < 0)
			return -2;
		return 1;
	}
	if(op.len == 6 && strncmp(op.s, "notify", 6) == 0) {
		LM_INFO("PCF event notification for %u-%u-%c: %.*s\n", h_entry, h_id,
				side, body->len, body->s);
		return 2;
	}
	return -1;
}

static int w_n5_notification(struct sip_msg *msg, char *ppath, char *pbody)
{
	str path, body;

	if(fixup_get_svalue(msg, (gparam_t *)ppath, &path) != 0
			|| fixup_get_svalue(msg, (gparam_t *)pbody, &body) != 0)
		return -1;
	return n5_notification(&path, &body);
}

/* ------------------------------------------------------------------- KEMI */

static int ki_n5_create(sip_msg_t *msg, str *dir)
{
	return n5_create(msg, dir, NULL);
}

static int ki_n5_create_sid(sip_msg_t *msg, str *dir, str *sid)
{
	return n5_create(msg, dir, sid);
}

static int ki_n5_notification(sip_msg_t *msg, str *path, str *body)
{
	return n5_notification(path, body);
}

/* clang-format off */
static sr_kemi_t sr_kemi_ims_qos_n5_exports[] = {
	{ str_init("ims_qos_n5"), str_init("N5_create"),
		SR_KEMIP_INT, ki_n5_create,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ims_qos_n5"), str_init("N5_create_sid"),
		SR_KEMIP_INT, ki_n5_create_sid,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ims_qos_n5"), str_init("N5_notification"),
		SR_KEMIP_INT, ki_n5_notification,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_ims_qos_n5_exports);
	return 0;
}

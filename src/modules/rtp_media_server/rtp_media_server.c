/*
 * Copyright (C) 2017-2018 Julien Chavanton jchavanton@gmail.com
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "rtp_media_server.h"
#include "../../core/fmsg.h"

MODULE_VERSION

rms_session_info_t *rms_session_list;
static int mod_init(void);
static void mod_destroy(void);
static int child_init(int);
str playback_fn = {0, 0};
str log_fn = {0, 0};
static char *rms_bridge_default_route = "rms:bridged";
static char *rms_answer_default_route = "rms:start";

int in_rms_process;
rms_t *rms;

static rms_session_info_t *rms_session_create_leg(rms_session_info_t *si, struct sip_msg *msg);
static int fixup_rms_action_play(void **param, int param_no);
static int fixup_rms_bridge(void **param, int param_no);
static int fixup_rms_answer(void **param, int param_no);
static int rms_hangup_call(rms_session_info_t *si);
static int rms_bridging_call(rms_session_info_t *si, rms_action_t *a);
static int rms_bridged_call(rms_session_info_t *si, rms_action_t *a);

static int rms_answer_f(struct sip_msg *, char *);
static int rms_sip_request_f(struct sip_msg *);
static int rms_action_play_f(struct sip_msg *, str *, str *);
static int rms_session_check_f(struct sip_msg *);
static int rms_hangup_f(struct sip_msg *);
static int rms_bridge_f(struct sip_msg *, char *, char *);

static int rms_update_media_sockets(struct sip_msg *msg,
		rms_session_info_t *si, rms_sdp_info_t *sdp_info);

static cmd_export_t cmds[] = {
		{"rms_answer", (cmd_function)rms_answer_f, 1, fixup_rms_answer, 0, EVENT_ROUTE},
		{"rms_sip_request", (cmd_function)rms_sip_request_f, 0, 0, 0,
				EVENT_ROUTE},
		{"rms_play", (cmd_function)rms_action_play_f, 2, fixup_rms_action_play,
				0, ANY_ROUTE},
		{"rms_session_check", (cmd_function)rms_session_check_f, 0, 0, 0,
				REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
		{"rms_hangup", (cmd_function)rms_hangup_f, 0, 0, 0, EVENT_ROUTE},
		{"rms_bridge", (cmd_function)rms_bridge_f, 2, fixup_rms_bridge, 0,
				ANY_ROUTE},
		{"rms_sessions_dump", (cmd_function)rms_sessions_dump_f, 0, 0, 0,
				ANY_ROUTE},
		{0, 0, 0, 0, 0, 0}};

static param_export_t mod_params[] = {
		{"log_file_name", PARAM_STR, &log_fn}, {0, 0, 0}};

struct module_exports exports = {
		"rtp_media_server", DEFAULT_DLFLAGS, /* dlopen flags */
		cmds, mod_params, 0,				 /* RPC export */
		0, 0, mod_init, child_init, mod_destroy,
};

static void run_action_route(rms_session_info_t *si, char *route)
{
	int rt, backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;

	if(route == NULL) {
		LM_ERR("bad route\n");
		return;
	}
	rt = -1;
	rt = route_lookup(&event_rt, route);
	if(rt < 0 || event_rt.rlist[rt] == NULL) {
		LM_DBG("route does not exist");
		return;
	}
	if(faked_msg_init() < 0) {
		LM_ERR("faked_msg_init() failed\n");
		return;
	}

	fmsg = faked_msg_next();

	{ // set the callid
		struct hdr_field callid;
		callid.body.s = si->callid.s;
		callid.body.len = si->callid.len;
		fmsg->callid = &callid;
	}
	{ // set the from tag
		struct hdr_field from;
		struct to_body from_parsed;
		from.parsed = &from_parsed;
		from_parsed.tag_value.len = si->remote_tag.len;
		from_parsed.tag_value.s = si->remote_tag.s;
		fmsg->from = &from;
	}
	//{ // set the to tag
	//	struct hdr_field to;
	//	struct to_body to_parsed;
	//	to.parsed = &to_parsed;
	//	to_parsed.tag_value.len = si->local_tag.len;
	//	to_parsed.tag_value.s = si->local_tag.s;
	//	fmsg->to = &to;
	//}

	backup_rt = get_route_type();
	set_route_type(EVENT_ROUTE);
	init_run_actions_ctx(&ctx);
	if(rt >= 0) {
		run_top_route(event_rt.rlist[rt], fmsg, 0);
	}
	set_route_type(backup_rt);
}


static int fixup_rms_bridge(void **param, int param_no)
{
	if(param_no == 1 || param_no == 2)
		return fixup_spve_null(param, 1);
	LM_ERR("invalid parameter count [%d]\n", param_no);
	return -1;
}

static int fixup_rms_answer(void **param, int param_no)
{
	if(param_no == 1 || param_no == 2)
		return fixup_spve_null(param, 1);
	LM_ERR("invalid parameter count [%d]\n", param_no);
	return -1;
}

static int fixup_rms_action_play(void **param, int param_no)
{
	if(param_no == 1 || param_no == 2 || param_no ==3)
		return fixup_spve_null(param, 1);
	LM_ERR("invalid parameter count [%d]\n", param_no);
	return -1;
}

/**
 * @return 0 to continue to load the OpenSER, -1 to stop the loading
 * and abort OpenSER.
 */
static int mod_init(void)
{
	LM_INFO("RTP media server module init\n");

	rms = shm_malloc(sizeof(rms_t));
	rms->udp_start_port = 50000;
	LM_INFO("RTP media server module init\n");
	rms->udp_end_port = 60000;
	rms->udp_last_port = 50000 + rand() % 10000;
	rms_media_init();

	if(!init_rms_session_list()) {
		LM_ERR("can't initialize rms_session_list !\n");
		return -1;
	}

	register_procs(1);
	if(load_tm_api(&tmb) != 0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}
	FILE *log_file = fopen(log_fn.s, "w+");
	if(log_file) {
		LM_INFO("ortp logs are redirected [%s]\n", log_fn.s);
	} else {
		log_file = stdout;
		LM_INFO("ortp can not open logs file [%s]\n", log_fn.s);
	}
	ortp_set_log_file(log_file);
	ortp_set_log_level_mask(
			NULL, ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR | ORTP_FATAL);
	return (0);
}

/**
 * Called only once when OpenSER is shuting down to clean up module
 * resources.
 */
static void mod_destroy()
{
	rms_media_destroy();
	LM_INFO("RTP media server module destroy\n");
	return;
}

void rms_signal_handler(int signum)
{
	LM_INFO("signal received [%d]\n", signum);
}


static rms_session_info_t *rms_stop(rms_session_info_t *si)
{
	LM_NOTICE("si[%p]\n", si);
	if(si->bridged_si) {
		rms_stop_bridge(&si->media, &si->bridged_si->media);
	} else {
		rms_stop_media(&si->media);
	}

	rms_session_info_t *tmp = si->prev;
	si->state = RMS_ST_DISCONNECTED;
	// keep it for a little while to deal with retransmissions ...
	//clist_rm(si, next, prev);
	//rms_session_free(si);
	si = tmp;
	return si;
}

static rms_session_info_t *rms_session_action_check(rms_session_info_t *si)
{
	rms_action_t *a;
		if (!si)
			LM_ERR("session  NULL\n");
	clist_foreach(&si->action, a, next)
	{
		if (!a)
			LM_ERR("session action NULL\n");

		if(a->type == RMS_HANGUP) {
			LM_INFO("session action RMS_HANGUP [%s]\n", si->callid.s);
			rms_hangup_call(si);
			if(si->bridged_si)
				rms_hangup_call(si->bridged_si);
			a->type = RMS_STOP;
			return si;
		} else if(a->type == RMS_BRIDGING) {
			LM_INFO("session action RMS_BRIDGING [%s]\n", si->callid.s);
			rms_bridging_call(si, a);
			a->type = RMS_NONE;
			shm_free(a->param.s);
			return si;
		} else if(a->type == RMS_BRIDGED) {
			LM_INFO("session action RMS_BRIDGED [%s]\n", si->callid.s);
			LM_INFO("si_1[%p]si_2[%p]\n", si, si->bridged_si);
			rms_bridged_call(si, a);
			a->type = RMS_NONE;
			return si;
		} else if(a->type == RMS_STOP) {
			LM_INFO("session action RMS_STOP [%s][%p|%p]\n", si->callid.s, si,
					si->prev);
			//if (si->bridged_si)
			//	rms_stop(si->bridged_si);
			si = rms_stop(si);
			a->type = RMS_NONE;
			// si->state = RMS_SSTATE_DISCONNECTED;
			return si;
		} else if(a->type == RMS_PLAY) {
			LM_INFO("session action RMS_PLAY [%s]\n", si->callid.s);
			rms_playfile(&si->media, a);
			a->type = RMS_NONE;
		} else if(a->type == RMS_DONE) {
			LM_INFO("session action RMS_DONE [%s][%s]\n", si->callid.s,
					a->route.s);
			if(a->route.s) {
				run_action_route(si, a->route.s);
				rms_action_t *tmp = a->prev;
				clist_rm(a, next, prev);
				shm_free(a);
				a = tmp;
			} else {
				a->type = RMS_HANGUP;
			}
			return si;
		} else if(a->type == RMS_START) {
			LM_INFO("session action RMS_START\n");
			create_call_leg_media(&si->media);
			LM_INFO("session action RMS_START [%s]\n", si->callid.s);
			rms_action_t *tmp = a->prev;
			clist_rm(a, next, prev);
			rms_start_media(&si->media, a->param.s);
			run_action_route(si, a->route.s);
			shm_free(a);
			a = tmp;
			LM_INFO("session action RMS_START[done]\n");
			return si;
		}
	}
	return si;
}

/**
 * Most interaction with the session and media streams that are controlled 
 * in this function this is safer in the event where a library is using non shared memory
 * all the mediastreamer2 ticker threads are spawned from here.
 */
static void rms_session_manage_loop()
{
	in_rms_process = 1;
	while(1) {
		lock(&session_list_mutex);
		rms_session_info_t *si;
		clist_foreach(rms_session_list, si, next)
		{
			si = rms_session_action_check(si);
			//LM_INFO("next ... si[%p]\n", si);
		}
		unlock(&session_list_mutex);
		usleep(10000);
	}
}

/**
 * The rank will be o for the main process calling this function,
 * or 1 through n for each listener process. The rank can have a negative
 * value if it is a special process calling the child init function.
 * Other then the listeners, the rank will equal one of these values:
 * PROC_MAIN      0  Main ser process
 * PROC_TIMER    -1  Timer attendant process
 * PROC_FIFO     -2  FIFO attendant process
 * PROC_TCP_MAIN -4  TCP main process
 * PROC_UNIXSOCK -5  Unix domain socket server processes
 *
 * If this function returns a nonzero value the loading of OpenSER will
 * stop.
 */
static int child_init(int rank)
{
	if(rank == PROC_MAIN) {
		int pid;
		pid = fork_process(PROC_XWORKER, "RTP_media_server", 1);
		if(pid < 0)
			return -1;
		if(pid == 0) {
			rms_session_manage_loop();
			return 0;
		}
	}
	int rtn = 0;
	return (rtn);
}

static int parse_from(struct sip_msg *msg, rms_session_info_t *si)
{
	struct to_body *from = get_from(msg);
	LM_DBG("from[%.*s]tag[%.*s]\n", from->uri.len, from->uri.s,
			from->tag_value.len, from->tag_value.s);
	rms_str_dup(&si->remote_tag, &from->tag_value, 1);
	return 1;
}

static int rms_sip_reply(
		struct cell *cell, rms_session_info_t *si, int code, char *_reason)
{
	if (si->state == RMS_ST_CONNECTED) {
		return 1;
	}
	str reason = str_init(_reason);
	if(si->remote_tag.len == 0) {
		LM_ERR("can not find from tag\n");
		return 0;
	}

	if(!cell)
		cell = tmb.t_gett();

	if(cell->uas.request) {
		if(!tmb.t_reply_with_body(
				   cell, code, &reason, NULL, NULL, &si->local_tag)) {
			LM_ERR("t_reply error");
			return 0;
		}
	} else {
		LM_INFO("no request found\n");
	}
	return 1;
}

static int rms_answer_call(
		struct cell *cell, rms_session_info_t *si, rms_sdp_info_t *sdp_info)
{
	char buffer[128];
	str reason = str_init("OK");
	str contact_hdr;
	if (si->state == RMS_ST_CONNECTED) {
		return 1;
	}

	LM_INFO("[%s][%d]\n", sdp_info->new_body.s, sdp_info->udp_local_port);

	if(si->remote_tag.len == 0) {
		LM_ERR("can not find from tag\n");
		return 0;
	}
	LM_INFO("ip[%s]\n", si->local_ip.s);
	sdp_info->local_ip.s = si->local_ip.s;
	sdp_info->local_ip.len = si->local_ip.len;

	snprintf(buffer, 128,
			"Contact: <sip:rms@%s:%d>\r\nContent-Type: application/sdp\r\n",
			si->local_ip.s, si->local_port);
	contact_hdr.len = strlen(buffer);
	contact_hdr.s = buffer;

	if(!cell)
		cell = tmb.t_gett();

	if(cell->uas.request) {
		if(!tmb.t_reply_with_body(cell, 200, &reason, &sdp_info->new_body,
				   &contact_hdr, &si->local_tag)) {
			LM_ERR("t_reply error");
			return 0;
		}
		LM_INFO("answered\n");
		si->state = RMS_ST_CONNECTED;
	} else {
		LM_INFO("no request found\n");
	}
	return 1;
}

// message originating leg is suspended, this is the callback function of the transaction with destination leg
static void forward_cb(struct cell *ptrans, int ntype, struct tmcb_params *pcbp)
{
	// struct sip_msg *msg = pcbp->rpl;
	str *reason = &pcbp->rpl->first_line.u.reply.reason;
	rms_action_t *a = (rms_action_t *)*pcbp->param;
	if(ntype == TMCB_ON_FAILURE) {
		LM_NOTICE("FAILURE [%d][%.*s]\n", pcbp->code, reason->len, reason->s);
		return;
	} else {
		LM_NOTICE("COMPLETE [%d][%.*s] RE-INVITE/UPDATE - TODO SDP "
				  "renegotiation ? \n",
				pcbp->code, reason->len, reason->s);
	}

	// rms_answer_call(a->cell, si, sdp_info);
	// if(!cell) cell = tmb.t_gett();
	if(a->cell->uas.request) {
		if(!tmb.t_reply_with_body(a->cell, pcbp->code, reason, NULL, NULL,
				   &a->si->local_tag)) {
			LM_ERR("t_reply error");
			return;
		}
		LM_INFO("replied\n");
	} else {
		LM_INFO("no request found\n");
	}
}


// legA is suspended, this is the callback function of the transaction with legB (the callee)
// response to legA will be not be done here but once the media negociation is completed by the RMS process.
static void bridge_cb(struct cell *ptrans, int ntype, struct tmcb_params *pcbp)
{
	struct sip_msg *msg = pcbp->rpl;
	if(ntype == TMCB_ON_FAILURE) {
		LM_NOTICE("FAILURE [%d]\n", pcbp->code);
		rms_action_t *a = (rms_action_t *)*pcbp->param;
		a->si->state = RMS_ST_DISCONNECTED;
		if(a->cell->uas.request) {
			str *reason = &pcbp->rpl->first_line.u.reply.reason;
			if(!tmb.t_reply_with_body(a->cell, pcbp->code, reason, NULL, NULL,
					   &a->si->local_tag)) {
				LM_ERR("t_reply error");
				return;
			}
			LM_INFO("failure replied\n");
			a->type = RMS_NONE;
		} else {
			LM_ERR("failure but no request found\n");
		}
		return;
	} else if(ntype == TMCB_LOCAL_COMPLETED) {
		LM_NOTICE("COMPLETED [%d]\n", pcbp->code);
	} else if(ntype == TMCB_LOCAL_RESPONSE_IN){
		LM_NOTICE("RESPONSE [%d]\n", pcbp->code);
		if (pcbp->code != 180 && pcbp->code != 183)
			return;
	} else {
		LM_NOTICE("TMCB_TYPE[%d][%d]\n", ntype, pcbp->code);
	}

	if(parse_to_header(msg) < 0) {
		LM_ERR("can not parse To header!\n");
		return;
	}
	struct to_body *to = get_to(msg);
	if(parse_from_header(msg) < 0) {
		LM_ERR("can not parse From header!\n");
		return;
	}
	struct to_body *from = get_from(msg);

	rms_action_t *a = (rms_action_t *)*pcbp->param;
	rms_session_info_t *bridged_si = a->si;
	rms_session_info_t *si = bridged_si->bridged_si;

	if(to->tag_value.len == 0) {
		LM_ERR("not to tag.\n");
		goto error;
	} else {
		rms_str_dup(&si->remote_tag, &to->tag_value, 1);
	}

	if(from->tag_value.len == 0) {
		LM_ERR("not from tag.\n");
		goto error;
	} else {
		rms_str_dup(&si->local_tag, &from->tag_value, 1);
	}

	LM_NOTICE("session updated [%s][%s][%s]\n", si->callid.s, si->remote_tag.s,
			si->local_tag.s);
	if (pcbp->code == 180 || pcbp->code == 183) {
		return; // early media not tested/handled properly
	}

	rms_sdp_info_t *sdp_info = &si->sdp_info_answer;
	if(!rms_get_sdp_info(sdp_info, msg)) {
		LM_ERR("can not get SDP information\n");
		goto error;
	}
	si->media.pt = rms_sdp_check_payload(sdp_info);
	rms_update_media_sockets(pcbp->rpl, si, &si->sdp_info_answer);
	LM_INFO("[%p][%s:%d]\n", si, sdp_info->local_ip.s,
			sdp_info->udp_local_port);
	a->type = RMS_BRIDGED;
	return;
error:
	LM_ERR("TODO: free and terminate!\n");
}


static int rms_bridged_call(rms_session_info_t *si, rms_action_t *a)
{
	rms_sdp_info_t *sdp_info = &si->bridged_si->sdp_info_answer;
	sdp_info->udp_local_port = si->media.local_port;
	LM_INFO("[%p][%s:%d]\n", si, sdp_info->local_ip.s,
			sdp_info->udp_local_port);
	sdp_info->local_ip.s = si->local_ip.s;
	sdp_info->local_ip.len = si->local_ip.len;

	rms_sdp_prepare_new_body(sdp_info, si->bridged_si->media.pt->type);
	rms_answer_call(a->cell, si, sdp_info);
	LM_NOTICE("si_1[%p] si_2[%p]\n", si, si->bridged_si);
	create_call_leg_media(&si->media);
	create_call_leg_media(&si->bridged_si->media);
	//	clist_append(rms_session_list, si, next, prev);
	rms_bridge(&si->media, &si->bridged_si->media);
	return 1;
}


static int rms_bridging_call(rms_session_info_t *si, rms_action_t *a)
{
	uac_req_t uac_r;
	int result;
	str method_invite = str_init("INVITE");
	str headers;

	struct sip_uri ruri_t;
	str *param_uri = &a->param;
	param_uri->len = strlen(param_uri->s);

	result = parse_uri(param_uri->s, param_uri->len, &ruri_t);
	LM_INFO("parsed[%.*s][%d]\n", param_uri->len, param_uri->s, result);
	char buff[1024];
	si->bridged_si->remote_uri.len = snprintf(buff, 1024,
			"<sip:%.*s@%.*s:%.*s>", ruri_t.user.len, ruri_t.user.s,
			ruri_t.host.len, ruri_t.host.s, ruri_t.port.len, ruri_t.port.s);
	si->bridged_si->remote_uri.s = rms_char_dup(buff, 1);
	if(!si->bridged_si->remote_uri.s) {
		LM_ERR("can not set remote uri !");
		goto error;
	}

	snprintf(buff, 256, "Max-Forwards: 70\r\nContact: "
						"<sip:rms@%s:%d>\r\nContent-Type: application/sdp\r\n",
			si->local_ip.s, si->local_port);
	headers.len = strlen(buff);
	headers.s = buff;
	LM_INFO("si[%p]call-id[%.*s]cseq[%d]ruri[%d|%s]remote_uri[%s]local_uri[%s]\n", si,
			si->callid.len, si->callid.s, si->bridged_si->cseq, param_uri->len, param_uri->s,
			si->bridged_si->remote_uri.s, si->bridged_si->local_uri.s);
	dlg_t *dialog = NULL;
	if(tmb.new_dlg_uac(&si->bridged_si->callid, &si->bridged_si->local_tag,
			   si->bridged_si->cseq, &si->bridged_si->local_uri,
			   &si->bridged_si->remote_uri, &dialog)
			< 0) {
		LM_ERR("error in tmb.new_dlg_uac\n");
		goto error;
	}
	dialog->rem_target.s = param_uri->s;
	dialog->rem_target.len = param_uri->len - 1;
	rms_sdp_info_t *sdp_info = &si->bridged_si->sdp_info_offer;

	set_uac_req(&uac_r, &method_invite, &headers, &sdp_info->new_body, dialog,
			TMCB_LOCAL_COMPLETED | TMCB_LOCAL_RESPONSE_IN | TMCB_ON_FAILURE, bridge_cb, a);
	result = tmb.t_request_within(&uac_r);
	si->bridged_si->cseq = dialog->loc_seq.value;
	if(result < 0) {
		LM_ERR("error in tmb.t_request\n");
		goto error;
	} else {
		LM_ERR("tmb.t_request_within ok\n");
	}
	return 1;
error:
	rms_sip_reply(a->cell, si, 503, "bridging error");
	rms_session_free(si->bridged_si);
	return -1;
}

static int rms_hangup_call(rms_session_info_t *si)
{
	uac_req_t uac_r;
	int result;
	str headers = str_init("Max-Forwards: 70" CRLF);
	str method_bye = str_init("BYE");

	LM_INFO("si[%p]callid[%.*s]cseq[%d]remote_uri[%s]local_uri[%s]\n", si, si->callid.len, si->callid.s,
			si->cseq, si->remote_uri.s, si->local_uri.s);
	LM_INFO("contact[%.*s]\n", si->contact_uri.len, si->contact_uri.s);
	dlg_t *dialog = NULL;

	if(tmb.new_dlg_uac(&si->callid, &si->local_tag, si->cseq, &si->local_uri,
			   &si->remote_uri, &dialog)
			< 0) {
		LM_ERR("error in tmb.new_dlg_uac\n");
		return -1;
	}
	dialog->id.rem_tag.s = si->remote_tag.s;
	dialog->id.rem_tag.len = si->remote_tag.len;
	dialog->rem_target.s = si->contact_uri.s;
	dialog->rem_target.len = si->contact_uri.len;
	set_uac_req(&uac_r, &method_bye, &headers, NULL, dialog,
			TMCB_LOCAL_COMPLETED, NULL, NULL);
	result = tmb.t_request_within(&uac_r);
	si->cseq = dialog->loc_seq.value;
	if(result < 0) {
		LM_ERR("error in tmb.t_request\n");
		return -1;
	} else {
		LM_ERR("tmb.t_request_within ok\n");
	}
	return 1;
}


/*
 * Create a new session info that will be used for bridging
 */
static rms_session_info_t *rms_session_create_leg(rms_session_info_t *si, struct sip_msg *msg)
{
	if(!si)
		return NULL;
	si->bridged_si = rms_session_new_bleg(msg);
	if(!si->bridged_si) {
		LM_ERR("can not allocate session info !\n");
		goto error;
	}
	memset(si->bridged_si, 0, sizeof(rms_session_info_t));
	si->bridged_si->bridged_si = si;
	if(!rms_str_dup(&si->bridged_si->callid, &si->callid, 1)) {
		LM_ERR("can not get callid .\n");
		goto error;
	}
	if(!rms_str_dup(&si->bridged_si->local_uri, &si->local_uri, 1))
		goto error;
	if(!rms_str_dup(&si->bridged_si->local_ip, &si->local_ip, 1))
		goto error;

	rms_update_media_sockets(msg, si->bridged_si, &si->bridged_si->sdp_info_offer);
	rms_sdp_prepare_new_body(&si->bridged_si->sdp_info_offer, si->media.pt->type);
	clist_init(&si->bridged_si->action, next, prev);
	return si->bridged_si;
error:
	rms_session_free(si);
	return NULL;
}


static int rms_get_udp_port(void)
{
	// RTP UDP port
	rms->udp_last_port += 3;
	if(rms->udp_last_port > rms->udp_end_port)
		rms->udp_last_port = rms->udp_start_port;
	LM_INFO("port[%d]\n", rms->udp_last_port);
	return rms->udp_last_port;
}



// update media IP and port
static int rms_update_media_sockets(struct sip_msg *msg, rms_session_info_t *si, rms_sdp_info_t *sdp_info)
{
	call_leg_media_t *m = &si->media;
	if(!m->local_port)
		m->local_port = rms_get_udp_port();
	sdp_info->udp_local_port = m->local_port;
	sdp_info->local_ip.s = si->local_ip.s;
	sdp_info->local_ip.len = si->local_ip.len;
	m->local_ip.s = si->local_ip.s;
	m->local_ip.len = si->local_ip.len;
	m->remote_port = sdp_info->remote_port;
	m->remote_ip.s = sdp_info->remote_ip.s;
	m->remote_ip.len = sdp_info->remote_ip.len;
	m->si = si;

	LM_INFO("remote_socket[%s:%d] local_socket[%s:%d]\n",
			sdp_info->remote_ip.s, sdp_info->remote_port, m->local_ip.s,
			m->local_port);
	return 1;
}

static int rms_create_trans(struct sip_msg *msg)
{
	int status = tmb.t_newtran(msg);
	LM_INFO("new transaction[%d]\n", status);
	if(status < 0) {
		LM_ERR("error creating transaction \n");
		return -1;
	} else if(status == 0) {
		LM_DBG("retransmission");
		return 0;
	}
	return 1;
}

static void rms_action_add(rms_session_info_t *si, rms_action_t *a)
{
	a->si = si;
	clist_append(&si->action, a, next, prev);
}

static void rms_action_add_sync(rms_session_info_t *si, rms_action_t *a)
{
	lock(&session_list_mutex);
	rms_action_add(si, a);
	unlock(&session_list_mutex);
}

// Called when receiving BYE
static int rms_disconnect(struct sip_msg *msg)
{
	int status = rms_create_trans(msg);
	if(status < 1)
		return status;

	rms_session_info_t *si;
	if(!msg || !msg->callid || !msg->callid->body.s) {
		LM_ERR("no callid\n");
		return -1;
	}
	si = rms_session_search_sync(msg);
	if(!si) {
		LM_INFO("session not found ci[%.*s]\n", msg->callid->body.len,
				msg->callid->body.s);
		if(!tmb.t_reply(msg, 481, "Call/Transaction Does Not Exist")) {
			return -1;
		}
		return 0;
	}
	rms_action_t *a = rms_action_new(RMS_STOP);
	if(!a)
		return -1;
	rms_action_add_sync(si, a);
	if(!tmb.t_reply(msg, 200, "OK")) {
		return -1;
	}
	return 0;
}

//static int rms_action_dtmf_f(struct sip_msg *msg, char dtmf, str *route)
//	rms_session_info_t *si =
//			rms_session_search(msg);
//	if(!si)
//		return -1;
//	rms_playfile();
//	return 0;
//}

static int rms_action_play_f(struct sip_msg *msg, str *playback_fn, str *route)
{
	rms_session_info_t *si = rms_session_search(msg);
	if(!si) {
		return -1;
	}
	LM_INFO("RTP session [%s:%d]<>[%s:%d]\n", si->media.local_ip.s,
			si->media.local_port, si->media.remote_ip.s, si->media.remote_port);

	rms_action_t *a = rms_action_new(RMS_PLAY);
	if(!a)
		return -1;
	a->param.len = playback_fn->len;
	a->param.s = playback_fn->s;
	a->route.len = route->len;
	a->route.s = route->s;
	rms_action_add(si, a);
	return 0;
}

static int rms_bridge_f(struct sip_msg *msg, char *_target, char *_route)
{
	str target = {NULL, 0};
	str route = {NULL, 0};
	rms_session_info_t *si = rms_session_search(msg);
	int status = 1;

	if(!rms_check_msg(msg))
		return -1;

	if (si) {
		if (si->state == RMS_ST_CONNECTED) {
			LM_INFO("already connected, bridging\n");
		} else {
			LM_ERR("Can not bridge an existing call leg that is not connected.\n");
			return -1;
		}
	} else {
		status = rms_create_trans(msg);
		if(status < 1)
			return status;
		// create a_leg session
		si = rms_session_new(msg);
		if(!si)
			return -1;
		si->local_port = msg->rcv.dst_port;
	}

	// parameter 1 : target URI
	if(get_str_fparam(&target, msg, (gparam_p)_target) != 0) {
		if (si->state != RMS_ST_CONNECTED) {
			LM_ERR("rms_bridge: missing target\n");
			tmb.t_reply(msg, 404, "Not found");
		}
		return -1;
	}
	// parameter 2 : route call-back
	if(get_str_fparam(&route, msg, (gparam_p)_route) != 0) {
		route.len = strlen(rms_bridge_default_route);
		route.s = rms_bridge_default_route;
	}

	LM_NOTICE("rms_bridge[%s][%d]\n", target.s, target.len);

	if (si->state == RMS_ST_DEFAULT) {
		str to_tag;
		parse_from(msg, si);
		tmb.t_get_reply_totag(msg, &to_tag);
		rms_str_dup(&si->local_tag, &to_tag, 1);
		LM_INFO("local_uri[%s]local_tag[%s]\n", si->local_uri.s,
				si->local_tag.s);
		rms_update_media_sockets(msg, si, &si->sdp_info_offer);
	}
	// Prepare the body of the SDP offer for the current Payload type
	// Both call legs will have the same offer.
	LM_NOTICE("payload[%d]\n", si->media.pt->type);
	rms_sdp_prepare_new_body(&si->sdp_info_offer, si->media.pt->type);

	// create b_leg
	si->bridged_si = rms_session_create_leg(si, msg);

	if(!si->bridged_si) {
		LM_ERR("can not create session b_leg !\n");
		goto error;
	}

	rms_action_t *a = rms_action_new(RMS_BRIDGING);
	if(!a)
		return -1;
	LM_NOTICE("remote target[%.*s]\n", target.len, target.s);
	LM_NOTICE("remote route[%.*s]\n", route.len, route.s);
	if(!rms_str_dup(&a->param, &target, 1)) {
		goto error;
	}

	a->route.len = route.len;
	a->route.s = route.s;

	if (si->state != RMS_ST_CONNECTED) { // a_leg: suspend transaction
		a->cell = tmb.t_gett();
		tmb.t_reply(msg, 100, "Trying");
		if(tmb.t_suspend(msg, &a->tm_info.hash_index, &a->tm_info.label) < 0) {
			LM_ERR("t_suspend() failed\n");
			goto error;
		}
		LM_INFO("transaction request[%p]\n", a->cell->uas.request);
	} else {
		a->cell = NULL;
	}
	LM_INFO("adding action\n");
	rms_action_add(si, a);

	LM_INFO("adding b_leg session\n");
	rms_session_add(si->bridged_si);
	LM_INFO("si_1[%p]si_2[%p]\n", si, si->bridged_si);
	if (si->state != RMS_ST_CONNECTED)
		rms_session_add(si);
	return 0;
error:
	rms_session_rm(si);
	rms_session_free(si);
	return -1;
}


static int rms_sip_cancel(struct sip_msg *msg, str *callid_s, str *cseq_s)
{
	tm_cell_t *trans;
	tm_cell_t *bkt;
	int bkb;
	struct cancel_info cancel_data;
	int fl = 0;
	int rcode = 0;

	if(rcode<100 || rcode>699)
		rcode = 0;

	bkt = tmb.t_gett();
	bkb = tmb.t_gett_branch();
	if (tmb.t_lookup_callid(&trans, *callid_s, *cseq_s) < 0 ) {
		LM_NOTICE("Lookup failed - no transaction [%s][%s]\n", callid_s->s, cseq_s->s);
		tmb.t_sett(bkt, bkb);
		if(!tmb.t_reply(msg, 481, "Call/Transaction Does Not Exist")) {
			return -1;
		}
		return 1;
	}

	if(trans->uas.request && fl>0 && fl<32)
		setflag(trans->uas.request, fl);
	init_cancel_info(&cancel_data);
	cancel_data.reason.cause = rcode;
	cancel_data.cancel_bitmap = 0;
	tmb.prepare_to_cancel(trans, &cancel_data.cancel_bitmap, 0);
	tmb.cancel_uacs(trans, &cancel_data, 0);

	tmb.t_sett(bkt, bkb);
	if(!tmb.t_reply(msg, 202, "cancelling")) {
		LM_ERR("can not reply cancelling ?\n");
		return -1;
	}
	LM_DBG("cancelling ...\n");
	return 1;
}


static int rms_sip_forward(
		rms_session_info_t *si, struct sip_msg *msg, str *method)
{
	uac_req_t uac_r;
	int result;
	str headers;
	char buff[1024];

	if(!rms_create_trans(msg))
		return 0;
	si->action.cell = tmb.t_gett();
	si->action.si = si;

	if (strncmp(method->s, "CANCEL", 6) == 0) {
		LM_INFO("[CANCEL][%s][%.*s]\n", si->bridged_si->remote_uri.s, si->remote_tag.len, si->remote_tag.s);
		str cseq;
		cseq.s = buff;
		cseq.len = snprintf(buff, 1024, "%d", si->bridged_si->cseq);
		rms_sip_cancel(msg, &si->callid, &cseq);
		return 1;
	}

	if(tmb.t_suspend(
			   msg, &si->action.tm_info.hash_index, &si->action.tm_info.label)
			< 0) {
		LM_ERR("t_suspend() failed\n");
		return 0;
	}

	snprintf(buff, 256, "Max-Forwards: 70\r\nContact: "
						"<sip:rms@%s:%d>\r\nContent-Type: application/sdp\r\n",
			si->local_ip.s, si->local_port);
	headers.len = strlen(buff);
	headers.s = buff;

	LM_INFO("si[%p]callid[%.*s]cseq[%d]ruri[%d|%s]remote_uri[%s]local_uri[%s]\n", si, si->callid.len,
			si->callid.s, si->bridged_si->cseq, si->bridged_si->contact_uri.len,
			si->bridged_si->contact_uri.s, si->bridged_si->remote_uri.s,
			si->bridged_si->local_uri.s);
	dlg_t *dialog = NULL;

	if(tmb.new_dlg_uac(&si->bridged_si->callid, &si->bridged_si->local_tag,
			   si->bridged_si->cseq, &si->bridged_si->local_uri,
			   &si->bridged_si->remote_uri, &dialog)
			< 0) {
		LM_ERR("error in tmb.new_dlg_uac\n");
		goto error;
	}
	dialog->id.rem_tag.s = si->bridged_si->remote_tag.s;
	dialog->id.rem_tag.len = si->bridged_si->remote_tag.len;

	dialog->rem_target.s = si->bridged_si->contact_uri.s;
	dialog->rem_target.len = si->bridged_si->contact_uri.len;

	set_uac_req(&uac_r, method, &headers, NULL, dialog,
			TMCB_LOCAL_COMPLETED | TMCB_ON_FAILURE, forward_cb, &si->action);
	result = tmb.t_request_within(&uac_r);
	si->bridged_si->cseq = dialog->loc_seq.value;
	if(result < 0) {
		LM_ERR("error in tmb.t_request\n");
		goto error;
	} else {
		LM_DBG("tmb.t_request_within ok\n");
	}
	return 1;
error:
	rms_session_free(si->bridged_si);
	return -1;
}

static int rms_sip_request_f(struct sip_msg *msg)
{
	if(msg->parsed_uri_ok) {
		LM_NOTICE("request[%s]\n", msg->parsed_uri.method_val.s);
	} else {
		LM_NOTICE("request[%.*s] not parsed\n",
				msg->first_line.u.request.method.len,
				msg->first_line.u.request.method.s);
	}
	str *method = &msg->first_line.u.request.method;
	rms_session_info_t *si = rms_session_search_sync(msg);
	if(!si) {
		rms_create_trans(msg);
		LM_INFO("session not found ci[%.*s]\n", msg->callid->body.len,
				msg->callid->body.s);
		if(!tmb.t_reply(msg, 481, "Call/Transaction Does Not Exist")) {
			return -1;
		}
		return 1;
	}

	if(si && strncmp(method->s, "BYE", 3) == 0) {
		if(si->state == RMS_ST_CONNECTED)
			si->state = RMS_ST_DISCONNECTING;
		rms_action_t *a = rms_action_new(RMS_STOP);
		if(!a)
			return -1;
		rms_action_add_sync(si, a);
		if(si->bridged_si) { // bridged
			LM_NOTICE("BYE in brigde mode\n");
		} else { // connected localy
			LM_NOTICE("BYE in local mode\n");
			rms_disconnect(msg);
			return 1;
		}
	}

	if(strncmp(method->s, "INVITE", 6) == 0 && si->remote_tag.len == 0) {
		LM_NOTICE("initial INVITE\n");
		return 1;
	} else {
		LM_NOTICE("in dialog message\n");
	}

	rms_sip_forward(si, msg, method);
	return 1;
}

static int rms_answer_f(struct sip_msg *msg, char * _route)
{
	str to_tag;
	str route;

	int status = rms_create_trans(msg);
	if(status < 1)
		return status;

	if(get_str_fparam(&route, msg, (gparam_p)_route) != 0) {
		route.len = strlen(rms_answer_default_route);
		route.s = rms_answer_default_route;
	}

	if(rms_session_search(msg))
		return -1;
	rms_session_info_t *si = rms_session_new(msg);
	if(!si)
		return -1;
	rms_session_add(si);
	rms_update_media_sockets(msg, si, &si->sdp_info_offer);
	parse_from(msg, si);
	tmb.t_get_reply_totag(msg, &to_tag);
	rms_str_dup(&si->local_tag, &to_tag, 1);
	LM_INFO("local_uri[%s]local_tag[%s]\n", si->local_uri.s, si->local_tag.s);
	if(!rms_sdp_prepare_new_body(&si->sdp_info_offer, si->media.pt->type)) {
		LM_ERR("error preparing SDP body\n");
		goto error;
	}

	si->local_port = msg->rcv.dst_port;
	if(rms_answer_call(NULL, si, &si->sdp_info_offer) < 1) {
		goto error;
	}
	LM_INFO("RTP session [%s:%d]<>[%s:%d]\n", si->media.local_ip.s,
			si->media.local_port, si->media.remote_ip.s, si->media.remote_port);

	rms_action_t *a = rms_action_new(RMS_START);
	if(!a)
		return -1;
	a->route.len = route.len;
	a->route.s = route.s;
	rms_action_add(si, a);
	return 1;
error:
	rms_session_rm(si);
	rms_session_free(si);
	return -1;
}

static int rms_session_check_f(struct sip_msg *msg)
{
	rms_session_info_t *si = rms_session_search_sync(msg);
	if(!si)
		return -1;
	return 1;
}

static int rms_hangup_f(struct sip_msg *msg)
{
	rms_session_info_t *si = rms_session_search(msg);
	if(!si)
		return -1;
	rms_action_t *a = rms_action_new(RMS_HANGUP);
	if(!a)
		return -1;
	rms_action_add(si, a);
	return 1;
}

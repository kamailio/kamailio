/*
 * Copyright (C) 2017-2019 Julien Chavanton jchavanton@gmail.com
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

extern rms_dialog_info_t *rms_dialog_list;
extern int in_rms_process;

gen_lock_t *dialog_list_mutex = NULL;

static void rms_action_free(rms_dialog_info_t *si)
{
	rms_action_t *a, *tmp;
	if (!si->action.prev) return;
	clist_foreach(&si->action, a, next)
	{
		tmp = a;
		a = a->prev;
		clist_rm(tmp, next, prev);
		shm_free(tmp);
	}
}

rms_action_t *rms_action_new(rms_action_type_t t)
{
	rms_action_t *a = shm_malloc(sizeof(rms_action_t));
	if(!a)
		return NULL;
	memset(a, 0, sizeof(rms_action_t));
	a->type = t;
	return a;
}

int rms_dialog_list_init()
{
	dialog_list_mutex = lock_alloc();
	if(!dialog_list_mutex)
		return 0;
	dialog_list_mutex = lock_init(dialog_list_mutex);
	rms_dialog_list = shm_malloc(sizeof(rms_dialog_info_t));
	if(!rms_dialog_list)
		return 0;
	clist_init(rms_dialog_list, next, prev);
	return 1;
}

void rms_dialog_list_free()
{
	lock_destroy(dialog_list_mutex);
	lock_dealloc((void*)dialog_list_mutex);
	shm_free(rms_dialog_list);
}

char *rms_dialog_state_toa(rms_dialog_state_t state) {
	if (state == 0) return "RMS_ST_DEFAULT";
	else if (state == 1) return "RMS_ST_CONNECTING";
	else if (state == 2) return "RMS_ST_CONNECTED";
	else if (state == 3) return "RMS_ST_CONNECTED_ACK";
	else if (state == 4) return "RMS_ST_DISCONNECTING";
	else if (state == 5) return "RMS_ST_DISCONNECTED";
	return "RMS_ST_UNKNOWN";
}

int rms_dialog_info_set_state(rms_dialog_info_t *di, rms_dialog_state_t state)
{
	if (state <= di->state) {
		LM_ERR("[%s] >> [%s] (invalid state transition) call-id[%s]\n", rms_dialog_state_toa(di->state), rms_dialog_state_toa(state), di->callid.s);
	} else {
		LM_NOTICE("[%s] >> [%s] call-id[%s]\n", rms_dialog_state_toa(di->state), rms_dialog_state_toa(state), di->callid.s);
		di->state = state;
	}
	return 1;
}

rms_dialog_info_t *rms_dialog_search(struct sip_msg *msg) // str *from_tag)
{
	rms_dialog_info_t *si;
	str callid = msg->callid->body;
	if(parse_from_header(msg) < 0) {
		LM_ERR("can not parse from header!\n");
		return NULL;
	}
	struct to_body *from = get_from(msg);
	clist_foreach(rms_dialog_list, si, next)
	{
		if(strncmp(callid.s, si->callid.s, callid.len) == 0) {
			LM_NOTICE("call-id[%s]tag[%s][%s]\n", si->callid.s, si->local_tag.s,
					si->remote_tag.s);
			if(si->remote_tag.s
					&& strncmp(from->tag_value.s, si->remote_tag.s,
							   from->tag_value.len)
							   == 0)
				return si;
			if(si->local_tag.s
					&& strncmp(from->tag_value.s, si->local_tag.s,
							   from->tag_value.len)
							   == 0)
				return si;
			LM_NOTICE("call-id found but tag not matching ? [%s][%.*s]\n",
					si->callid.s, from->tag_value.len, from->tag_value.s);
		}
	}
	return NULL;
}

rms_dialog_info_t *rms_dialog_search_sync(struct sip_msg *msg)
{
	lock(dialog_list_mutex);
	rms_dialog_info_t *si = rms_dialog_search(msg);
	unlock(dialog_list_mutex);
	return si;
}

void rms_dialog_add(rms_dialog_info_t *si)
{
	if (in_rms_process) {
		clist_append(rms_dialog_list, si, next, prev);
	} else {
		lock(dialog_list_mutex);
		clist_append(rms_dialog_list, si, next, prev);
		unlock(dialog_list_mutex);
	}
}

void rms_dialog_rm(rms_dialog_info_t *si)
{
	if (in_rms_process) {
		clist_append(rms_dialog_list, si, next, prev);
	} else {
		lock(dialog_list_mutex);
		clist_rm(si, next, prev);
		unlock(dialog_list_mutex);
	}
}

int rms_dialog_free(rms_dialog_info_t *si)
{
	rms_action_free(si);
	rms_sdp_info_free(&si->sdp_info_offer);
	rms_sdp_info_free(&si->sdp_info_answer);
	if(si->media.pt) {
		shm_free(si->media.pt); // TODO: should be destroyed in  compatible way from MS manager process
		si->media.pt = NULL;
	}
	if(si->callid.s) {
		shm_free(si->callid.s);
		si->callid.s = NULL;
	}
	if(si->contact_uri.s) {
		shm_free(si->contact_uri.s);
		si->contact_uri.s = NULL;
	}
	if(si->local_ip.s) {
		shm_free(si->local_ip.s);
		si->local_ip.s = NULL;
	}
	if(si->remote_uri.s) {
		shm_free(si->remote_uri.s);
		si->remote_uri.s = NULL;
	}
	if(si->local_uri.s) {
		shm_free(si->local_uri.s);
		si->local_uri.s = NULL;
	}
	shm_free(si);
	si = NULL;
	return 1;
}

int rms_check_msg(struct sip_msg *msg)
{
	if(!msg || !msg->callid || !msg->callid->body.s) {
		LM_INFO("no callid ?\n");
		return -1;
	}
	return 1;
}

rms_dialog_info_t *rms_dialog_new_bleg(struct sip_msg *msg)
{
	if(!rms_check_msg(msg))
		return NULL;
	rms_dialog_info_t *si = shm_malloc(sizeof(rms_dialog_info_t));
	if(!si) {
		LM_ERR("can not allocate dialog info !\n");
		goto error;
	}
	memset(si, 0, sizeof(rms_dialog_info_t));

	if(!rms_str_dup(&si->callid, &msg->callid->body, 1)) {
		LM_ERR("can not get callid .\n");
		goto error;
	}
	if(!rms_str_dup(&si->remote_uri, &msg->from->body, 1))
		goto error;
	str ip;
	ip.s = ip_addr2a(&msg->rcv.dst_ip);
	ip.len = strlen(ip.s);
	if(!rms_str_dup(&si->local_ip, &ip, 1))
		goto error;
	clist_init(&si->action, next, prev);
	return si;
error:
	LM_ERR("can not create dialog info.\n");
	rms_dialog_free(si);
	return NULL;
}

rms_dialog_info_t *rms_dialog_new(struct sip_msg *msg)
{
	struct hdr_field *hdr = NULL;

	if(!rms_check_msg(msg))
		return NULL;
	rms_dialog_info_t *si = shm_malloc(sizeof(rms_dialog_info_t));
	if(!si) {
		LM_ERR("can not allocate dialog info !\n");
		goto error;
	}
	memset(si, 0, sizeof(rms_dialog_info_t));

	if(!rms_str_dup(&si->callid, &msg->callid->body, 1)) {
		LM_ERR("can not get callid .\n");
		goto error;
	}
	if(!rms_str_dup(&si->remote_uri, &msg->from->body, 1))
		goto error;
	if(!rms_str_dup(&si->local_uri, &msg->to->body, 1))
		goto error;
	str ip;
	ip.s = ip_addr2a(&msg->rcv.dst_ip);
	ip.len = strlen(ip.s);
	if(!rms_str_dup(&si->local_ip, &ip, 1))
		goto error;
	hdr = msg->contact;
	if(parse_contact(hdr) < 0)
		goto error;
	contact_body_t *contact = hdr->parsed;
	if(!rms_str_dup(&si->contact_uri, &contact->contacts->uri, 1))
		goto error;
	LM_INFO("[contact offer] [%.*s]\n", si->contact_uri.len, si->contact_uri.s);
	si->cseq = atoi(msg->cseq->body.s);

	rms_sdp_info_t *sdp_info = &si->sdp_info_offer;
	if(!rms_get_sdp_info(sdp_info, msg))
		goto error;
	si->media.pt = rms_sdp_select_payload(sdp_info);
	if(!si->media.pt) {
		tmb.t_reply(msg, 488, "incompatible media format");
		goto error;
	}
	clist_init(&si->action, next, prev);
	return si;
error:
	LM_ERR("can not create dialog info.\n");
	rms_dialog_free(si);
	return NULL;
}

int rms_dialogs_dump_f(struct sip_msg *msg, char *param1, char *param2)
{
	int x = 1;
	rms_dialog_info_t *di;
	clist_foreach(rms_dialog_list, di, next)
	{
		LM_INFO("[%d]callid[%s]remote_tag[%s]local_tag[%s]cseq[%d]\n", x,
				di->callid.s, di->remote_tag.s, di->local_tag.s, di->cseq);
		x++;
	}
	return 1;
}

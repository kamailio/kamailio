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
extern rms_session_info_t *rms_session_list;
extern int in_rms_process;

static void rms_action_free(rms_session_info_t *si)
{
	rms_action_t *a, *tmp;
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

int init_rms_session_list()
{
	rms_session_list = shm_malloc(sizeof(rms_session_info_t));
	if(!rms_session_list)
		return 0;
	clist_init(rms_session_list, next, prev);
	return 1;
}

rms_session_info_t *rms_session_search(struct sip_msg *msg) // str *from_tag)
{
	rms_session_info_t *si;
	str callid = msg->callid->body;
	if(parse_from_header(msg) < 0) {
		LM_ERR("can not parse from header!\n");
		return NULL;
	}
	struct to_body *from = get_from(msg);
	clist_foreach(rms_session_list, si, next)
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

rms_session_info_t *rms_session_search_sync(struct sip_msg *msg)
{
	lock(&session_list_mutex);
	rms_session_info_t *si = rms_session_search(msg);
	unlock(&session_list_mutex);
	return si;
}

void rms_session_add(rms_session_info_t *si)
{
	if (in_rms_process) {
		clist_append(rms_session_list, si, next, prev);
	} else {
		lock(&session_list_mutex);
		clist_append(rms_session_list, si, next, prev);
		unlock(&session_list_mutex);
	}
}

void rms_session_rm(rms_session_info_t *si)
{
	if (in_rms_process) {
		clist_append(rms_session_list, si, next, prev);
	} else {
		lock(&session_list_mutex);
		clist_rm(si, next, prev);
		unlock(&session_list_mutex);
	}
}

int rms_session_free(rms_session_info_t *si)
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

rms_session_info_t *rms_session_new_bleg(struct sip_msg *msg)
{
	if(!rms_check_msg(msg))
		return NULL;
	rms_session_info_t *si = shm_malloc(sizeof(rms_session_info_t));
	if(!si) {
		LM_ERR("can not allocate session info !\n");
		goto error;
	}
	memset(si, 0, sizeof(rms_session_info_t));

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
	LM_ERR("can not create session.\n");
	rms_session_free(si);
	return NULL;
}

rms_session_info_t *rms_session_new(struct sip_msg *msg)
{
	struct hdr_field *hdr = NULL;

	if(!rms_check_msg(msg))
		return NULL;
	rms_session_info_t *si = shm_malloc(sizeof(rms_session_info_t));
	if(!si) {
		LM_ERR("can not allocate session info !\n");
		goto error;
	}
	memset(si, 0, sizeof(rms_session_info_t));

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
	si->media.pt = rms_sdp_check_payload(sdp_info);
	if(!si->media.pt) {
		tmb.t_reply(msg, 488, "incompatible media format");
		goto error;
	}
	clist_init(&si->action, next, prev);
	return si;
error:
	LM_ERR("can not create session.\n");
	rms_session_free(si);
	return NULL;
}

int rms_sessions_dump_f(struct sip_msg *msg, char *param1, char *param2)
{
	int x = 1;
	rms_session_info_t *si;
	clist_foreach(rms_session_list, si, next)
	{
		LM_INFO("[%d]callid[%s]remote_tag[%s]local_tag[%s]cseq[%d]\n", x,
				si->callid.s, si->remote_tag.s, si->local_tag.s, si->cseq);
		x++;
	}
	return 1;
}

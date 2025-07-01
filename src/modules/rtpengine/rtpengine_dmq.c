/*
 * Copyright (C) 2025 Viktor Litvinov
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "rtpengine.h"
#include "rtpengine_dmq.h"

static str rtpengine_dmq_content_type = str_init("application/json");
static str rtpengine_dmq_200_rpl = str_init("OK");
static str rtpengine_dmq_400_rpl = str_init("Bad Request");
static str rtpengine_dmq_500_rpl = str_init("Server Internal Error");

dmq_api_t rtpengine_dmqb;
dmq_peer_t *rtpengine_dmq_peer = NULL;

int rtpengine_dmq_request_sync();

/**
* @brief add rtpengine notification peer
*/
int rtpengine_dmq_init()
{
	dmq_peer_t not_peer;

	/* load the DMQ API */
	if(dmq_load_api(&rtpengine_dmqb) != 0) {
		LM_ERR("cannot load dmq api\n");
		return -1;
	} else {
		LM_DBG("loaded dmq api\n");
	}

	not_peer.callback = rtpengine_dmq_handle_msg;
	not_peer.init_callback = rtpengine_dmq_request_sync;
	not_peer.description.s = "rtpengine";
	not_peer.description.len = 9;
	not_peer.peer_id.s = "rtpengine";
	not_peer.peer_id.len = 9;
	rtpengine_dmq_peer = rtpengine_dmqb.register_dmq_peer(&not_peer);
	if(!rtpengine_dmq_peer) {
		LM_ERR("error in register_dmq_peer\n");
		return -1;
	} else {
		LM_DBG("dmq peer registered\n");
	}
	return 0;
}

int rtpengine_dmq_request_sync()
{
	return 0;
}

int rtpengine_dmq_handle_msg(
		struct sip_msg *msg, peer_reponse_t *resp, dmq_node_t *node)
{
	int content_length = 0;
	srjson_doc_t jdoc;
	srjson_t *it = NULL;
	str body = STR_NULL;

	rtpengine_dmq_action_t action = RTPENGINE_DMQ_NONE;
	struct rtpp_set *rtpp_list;
	unsigned int setid;
	struct rtpp_node *rtpp_node;
	str rtpengine_url = STR_NULL;
	str viabranch = STR_NULL;
	str callid = STR_NULL;
	struct rtpengine_hash_entry *entry = NULL;

	srjson_InitDoc(&jdoc, NULL);

	if(!msg->content_length) {
		LM_ERR("no content length header found\n");
		goto invalid;
	}
	content_length = get_content_length(msg);
	if(!content_length) {
		LM_DBG("content length is 0\n");
		goto invalid;
	}

	body.s = get_body(msg);
	body.len = content_length;

	if(!body.s) {
		LM_ERR("unable to get body\n");
		goto error;
	}

	/* parse body */
	LM_DBG("body: %.*s\n", body.len, body.s);

	jdoc.buf = body;

	if(jdoc.root == NULL) {
		jdoc.root = srjson_Parse(&jdoc, jdoc.buf.s);
		if(jdoc.root == NULL) {
			LM_ERR("invalid json doc [[%s]]\n", jdoc.buf.s);
			goto invalid;
		}
	}

	/* iterate over keys */
	for(it = jdoc.root->child; it; it = it->next) {
		LM_DBG("found field: %s\n", it->string);
		if(strcmp(it->string, "action") == 0) {
			action = SRJSON_GET_INT(it);
		} else if(strcmp(it->string, "setid") == 0) {
			setid = SRJSON_GET_INT(it);
		} else if(strcmp(it->string, "rtpengine_url") == 0) {
			rtpengine_url.s = it->valuestring;
			rtpengine_url.len = strlen(it->valuestring);
		} else if(strcmp(it->string, "callid") == 0) {
			callid.s = it->valuestring;
			callid.len = strlen(it->valuestring);
		} else if(strcmp(it->string, "viabranch") == 0) {
			viabranch.s = it->valuestring;
			viabranch.len = strlen(it->valuestring);
		} else {
			LM_ERR("unrecognized field in json object\n");
			goto invalid;
		}
	}

	switch(action) {
		case RTPENGINE_DMQ_INSERT:
			rtpp_list = get_rtpp_set(setid, 0);
			if(!rtpp_list) {
				LM_ERR("rtpengine dmq fail to get rtpp list for setid=%d\n",
						setid);
				goto error;
			}

			rtpp_node = get_rtpp_node(rtpp_list, &rtpengine_url);
			if(!rtpp_node) {
				LM_ERR("rtpengine dmq fail to get rtpp node for "
					   "rtpengine_url=%.*s\n",
						rtpengine_url.len, rtpengine_url.s);
				goto error;
			}

			entry = shm_malloc(sizeof(struct rtpengine_hash_entry));
			if(!entry) {
				LM_ERR("rtpengine hash table fail to create entry for "
					   "calllen=%d "
					   "callid=%.*s viabranch=%.*s\n",
						callid.len, callid.len, callid.s, viabranch.len,
						viabranch.s);
				goto error;
			}
			memset(entry, 0, sizeof(struct rtpengine_hash_entry));
			// fill the entry
			if(callid.s && callid.len > 0) {
				if(shm_str_dup(&entry->callid, &callid) < 0) {
					LM_ERR("rtpengine hash table fail to duplicate calllen=%d "
						   "callid=%.*s\n",
							callid.len, callid.len, callid.s);
					goto error;
				}
			}
			if(viabranch.s && viabranch.len > 0) {
				if(shm_str_dup(&entry->viabranch, &viabranch) < 0) {
					LM_ERR("rtpengine hash table fail to duplicate "
						   "viabranch=%.*s\n",
							viabranch.len, viabranch.s);
					goto error;
				}
			}
			entry->node = rtpp_node;
			entry->next = NULL;
			entry->tout = get_ticks() + hash_table_tout;

			if(!rtpengine_hash_table_insert(callid, viabranch, entry))
				goto error;
			break;
		case RTPENGINE_DMQ_REMOVE:
			if(!rtpengine_hash_table_remove(callid, viabranch, OP_DELETE))
				goto error;
			break;
		case RTPENGINE_DMQ_SYNC:
		case RTPENGINE_DMQ_NONE:
			break;
	}

	resp->reason = rtpengine_dmq_200_rpl;
	resp->resp_code = 200;
	goto cleanup;

invalid:
	resp->reason = rtpengine_dmq_400_rpl;
	resp->resp_code = 400;
	goto cleanup;

error:
	resp->reason = rtpengine_dmq_500_rpl;
	resp->resp_code = 500;

	if(entry)
		rtpengine_hash_table_free_entry(entry);

cleanup:
	srjson_DestroyDoc(&jdoc);

	return 0;
}

int rtpengine_dmq_send(str *body, dmq_node_t *node)
{
	if(!rtpengine_dmq_peer) {
		LM_ERR("rtpengine_dmq_peer is null!\n");
		return -1;
	}
	if(node) {
		LM_DBG("sending dmq message ...\n");
		rtpengine_dmqb.send_message(rtpengine_dmq_peer, body, node, NULL, 1,
				&rtpengine_dmq_content_type);
	} else {
		LM_DBG("sending dmq broadcast...\n");
		rtpengine_dmqb.bcast_message(rtpengine_dmq_peer, body, 0, NULL, 1,
				&rtpengine_dmq_content_type);
	}
	return 0;
}

int rtpengine_dmq_replicate_action(rtpengine_dmq_action_t action, str callid,
		str viabranch, struct rtpengine_hash_entry *entry, dmq_node_t *node)
{
	srjson_doc_t jdoc;
	unsigned int setid = -1;

	LM_DBG("replicating action to dmq peers...\n");

	srjson_InitDoc(&jdoc, NULL);

	jdoc.root = srjson_CreateObject(&jdoc);
	if(jdoc.root == NULL) {
		LM_ERR("cannot create json root\n");
		goto error;
	}

	srjson_AddNumberToObject(&jdoc, jdoc.root, "action", action);
	srjson_AddStrToObject(&jdoc, jdoc.root, "callid", callid.s, callid.len);
	if(viabranch.s != NULL && viabranch.len > 0) {
		srjson_AddStrToObject(
				&jdoc, jdoc.root, "viabranch", viabranch.s, viabranch.len);
	}
	if(entry && entry->node) {
		srjson_AddStrToObject(&jdoc, jdoc.root, "rtpengine_url",
				entry->node->rn_url.s, entry->node->rn_url.len);

		setid = get_rtpp_set_id_by_node(entry->node);
		if(setid != -1)
			srjson_AddNumberToObject(&jdoc, jdoc.root, "setid", setid);
	}

	jdoc.buf.s = srjson_PrintUnformatted(&jdoc, jdoc.root);
	if(jdoc.buf.s != NULL) {
		jdoc.buf.len = strlen(jdoc.buf.s);
		LM_DBG("sending serialized data %.*s\n", jdoc.buf.len, jdoc.buf.s);
		if(rtpengine_dmq_send(&jdoc.buf, 0) != 0) {
			goto error;
		}
		jdoc.free_fn(jdoc.buf.s);
		jdoc.buf.s = NULL;
	} else {
		LM_ERR("unable to serialize data\n");
		goto error;
	}

	srjson_DestroyDoc(&jdoc);
	return 0;

error:
	if(jdoc.buf.s != NULL) {
		jdoc.free_fn(jdoc.buf.s);
		jdoc.buf.s = NULL;
	}
	srjson_DestroyDoc(&jdoc);
	return -1;
}

int rtpengine_dmq_replicate_insert(
		str callid, str viabranch, struct rtpengine_hash_entry *entry)
{
	return rtpengine_dmq_replicate_action(
			RTPENGINE_DMQ_INSERT, callid, viabranch, entry, NULL);
}

int rtpengine_dmq_replicate_remove(str callid, str viabranch)
{
	return rtpengine_dmq_replicate_action(
			RTPENGINE_DMQ_REMOVE, callid, viabranch, NULL, NULL);
}

/**
*
* Copyright (C) 2018 Charles Chance (Sipcentric Ltd)
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

#include "presence.h"
#include "presence_dmq.h"

static str pres_dmq_content_type = str_init("application/json");
static str pres_dmq_200_rpl = str_init("OK");
static str pres_dmq_400_rpl = str_init("Bad Request");
static str pres_dmq_500_rpl = str_init("Server Internal Error");

static int *pres_dmq_proc_init = 0;
static int *pres_dmq_recv = 0;

dmq_api_t pres_dmqb;
dmq_peer_t *pres_dmq_peer = NULL;
dmq_resp_cback_t pres_dmq_resp_callback = {&pres_dmq_resp_callback_f, 0};

int pres_dmq_send_all_presentities();
int pres_dmq_request_sync();

/**
* @brief add notification peer
*/
int pres_dmq_initialize()
{
	dmq_peer_t not_peer;

	/* load the DMQ API */
	if(dmq_load_api(&pres_dmqb) != 0) {
		LM_ERR("cannot load dmq api\n");
		return -1;
	} else {
		LM_DBG("loaded dmq api\n");
	}

	not_peer.callback = pres_dmq_handle_msg;
	not_peer.init_callback = pres_dmq_request_sync;
	not_peer.description.s = "presence";
	not_peer.description.len = 8;
	not_peer.peer_id.s = "presence";
	not_peer.peer_id.len = 8;
	pres_dmq_peer = pres_dmqb.register_dmq_peer(&not_peer);
	if(!pres_dmq_peer) {
		LM_ERR("error in register_dmq_peer\n");
		goto error;
	} else {
		LM_DBG("dmq peer registered\n");
	}
	return 0;
error:
	return -1;
}

static int pres_dmq_init_proc()
{
	// TODO: tidy up

	if(!pres_dmq_proc_init) {
		LM_DBG("Initializing pres_dmq_proc_init for pid (%d)\n", my_pid());
		pres_dmq_proc_init = (int *)pkg_malloc(sizeof(int));
		if(!pres_dmq_proc_init) {
			LM_ERR("no more pkg memory\n");
			return -1;
		}
		*pres_dmq_proc_init = 0;
	}

	if(!pres_dmq_recv) {
		LM_DBG("Initializing pres_dmq_recv for pid (%d)\n", my_pid());
		pres_dmq_recv = (int *)pkg_malloc(sizeof(int));
		if(!pres_dmq_recv) {
			LM_ERR("no more pkg memory\n");
			return -1;
		}
		*pres_dmq_recv = 0;
	}

	if(pres_sruid.pid == 0) {
		LM_DBG("Initializing pres_sruid for pid (%d)\n", my_pid());
		if(sruid_init(&pres_sruid, '-', "pres", SRUID_INC) < 0) {
			return -1;
		}
	}

	if(publ_cache_enabled && subs_dbmode==NO_DB) {
		goto finish;
	}

	if(!pa_db) {
		LM_DBG("Initializing presence DB connection for pid (%d)\n", my_pid());

		if(pa_dbf.init == 0) {
			LM_ERR("database not bound\n");
			return -1;
		}

		/* Do not pool the connections where possible when running notifier
		* processes. */
		if(pres_notifier_processes > 0 && pa_dbf.init2)
			pa_db = pa_dbf.init2(&pres_db_url, DB_POOLING_NONE);
		else
			pa_db = pa_dbf.init(&pres_db_url);

		if(!pa_db) {
			LM_ERR("dmq_worker_init: unsuccessful database connection\n");
			return -1;
		}
	}

finish:
	*pres_dmq_proc_init = 1;

	LM_DBG("process initialization complete\n");

	return 0;
}

int pres_dmq_send(str *body, dmq_node_t *node)
{
	if(!pres_dmq_peer) {
		LM_ERR("pres_dmq_peer is null!\n");
		return -1;
	}
	if(node) {
		LM_DBG("sending dmq message ...\n");
		pres_dmqb.send_message(pres_dmq_peer, body, node,
				&pres_dmq_resp_callback, 1, &pres_dmq_content_type);
	} else {
		LM_DBG("sending dmq broadcast...\n");
		pres_dmqb.bcast_message(pres_dmq_peer, body, 0, &pres_dmq_resp_callback,
				1, &pres_dmq_content_type);
	}
	return 0;
}

/**
 * @brief extract presentity from json object
*/
presentity_t *pres_parse_json_presentity(srjson_t *in)
{

	int p_expires = 0, p_recv = 0;
	str p_domain = STR_NULL, p_user = STR_NULL, p_etag = STR_NULL,
		p_sender = STR_NULL, p_event_str = STR_NULL;
	srjson_t *p_it;
	pres_ev_t *p_event = NULL;
	presentity_t *presentity = NULL;

	LM_DBG("extracting presentity\n");

	for(p_it = in->child; p_it; p_it = p_it->next) {
		if(strcmp(p_it->string, "domain") == 0) {
			p_domain.s = p_it->valuestring;
			p_domain.len = strlen(p_it->valuestring);
		} else if(strcmp(p_it->string, "user") == 0) {
			p_user.s = p_it->valuestring;
			p_user.len = strlen(p_it->valuestring);
		} else if(strcmp(p_it->string, "etag") == 0) {
			p_etag.s = p_it->valuestring;
			p_etag.len = strlen(p_it->valuestring);
		} else if(strcmp(p_it->string, "expires") == 0) {
			p_expires = SRJSON_GET_INT(p_it);
		} else if(strcmp(p_it->string, "recv") == 0) {
			p_recv = SRJSON_GET_INT(p_it);
		} else if(strcmp(p_it->string, "sender") == 0) {
			p_sender.s = p_it->valuestring;
			p_sender.len = strlen(p_it->valuestring);
		} else if(strcmp(p_it->string, "event") == 0) {
			p_event_str.s = p_it->valuestring;
			p_event_str.len = strlen(p_it->valuestring);
			p_event = contains_event(&p_event_str, 0);
			if(!p_event) {
				LM_ERR("unsupported event %s\n", p_it->valuestring);
				return NULL;
			}
		} else {
			LM_ERR("unrecognized field in json object\n");
			return NULL;
		}
	}

	if(!p_event) {
		LM_ERR("presence event not found\n");
		return NULL;
	}

	LM_DBG("building presentity from domain: %.*s, user: %.*s, expires: %d, "
		   "event: %.*s, etag: %.*s, sender: %.*s",
			p_domain.len, p_domain.s, p_user.len, p_user.s, p_expires,
			p_event->name.len, p_event->name.s, p_etag.len, p_etag.s,
			p_sender.len, p_sender.s);

	presentity = new_presentity(
			&p_domain, &p_user, p_expires, p_event, &p_etag, &p_sender);

	if(!presentity)
		return NULL;

	if(p_recv > 0)
		presentity->received_time = p_recv;

	return presentity;
}

/**
* @brief presence dmq callback
*/
int pres_dmq_handle_msg(
		struct sip_msg *msg, peer_reponse_t *resp, dmq_node_t *node)
{
	int content_length = 0, t_new = 0, sent_reply = 0;
	str cur_etag = STR_NULL, body = STR_NULL, p_body = STR_NULL,
		ruid = STR_NULL;
	char *sphere = NULL;
	srjson_doc_t jdoc;
	srjson_t *it = NULL;
	presentity_t *presentity = NULL;

	pres_dmq_action_t action = PRES_DMQ_NONE;

	/* received dmq message */
	LM_DBG("dmq message received\n");

	if(!pres_dmq_proc_init && pres_dmq_init_proc() < 0) {
		return 0;
	}

	*pres_dmq_recv = 1;
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
		} else if(strcmp(it->string, "presentity") == 0) {
			presentity = pres_parse_json_presentity(it);
			if(!presentity) {
				LM_ERR("failed to construct presentity from json\n");
				goto invalid;
			}
		} else if(strcmp(it->string, "t_new") == 0) {
			t_new = SRJSON_GET_INT(it);
		} else if(strcmp(it->string, "cur_etag") == 0) {
			cur_etag.s = it->valuestring;
			cur_etag.len = strlen(it->valuestring);
		} else if(strcmp(it->string, "sphere") == 0) {
			sphere = it->valuestring;
		} else if(strcmp(it->string, "ruid") == 0) {
			ruid.s = it->valuestring;
			ruid.len = strlen(it->valuestring);
		} else if(strcmp(it->string, "body") == 0) {
			p_body.s = it->valuestring;
			p_body.len = strlen(it->valuestring);
		} else {
			LM_ERR("unrecognized field in json object\n");
			goto invalid;
		}
	}

	switch(action) {
		case PRES_DMQ_UPDATE_PRESENTITY:
			if(presentity == NULL
					|| update_presentity(NULL, presentity, &p_body, t_new,
							   &sent_reply, sphere, &cur_etag, &ruid, 0)
							   < 0) {
				goto error;
			}
			break;
		case PRES_DMQ_SYNC:
		case PRES_DMQ_NONE:
			break;
	}

	resp->reason = pres_dmq_200_rpl;
	resp->resp_code = 200;
	goto cleanup;

invalid:
	resp->reason = pres_dmq_400_rpl;
	resp->resp_code = 400;
	goto cleanup;

error:
	resp->reason = pres_dmq_500_rpl;
	resp->resp_code = 500;

cleanup:
	*pres_dmq_recv = 0;
	srjson_DestroyDoc(&jdoc);
	if(presentity)
		pkg_free(presentity);

	return 0;
}


int pres_dmq_request_sync()
{
	srjson_doc_t jdoc;

	LM_DBG("requesting sync from dmq peers\n");

	srjson_InitDoc(&jdoc, NULL);

	jdoc.root = srjson_CreateObject(&jdoc);
	if(jdoc.root == NULL) {
		LM_ERR("cannot create json root\n");
		goto error;
	}

	srjson_AddNumberToObject(&jdoc, jdoc.root, "action", PRES_DMQ_SYNC);
	jdoc.buf.s = srjson_PrintUnformatted(&jdoc, jdoc.root);
	if(jdoc.buf.s == NULL) {
		LM_ERR("unable to serialize data\n");
		goto error;
	}
	jdoc.buf.len = strlen(jdoc.buf.s);
	LM_DBG("sending serialized data %.*s\n", jdoc.buf.len, jdoc.buf.s);
	if(pres_dmq_send(&jdoc.buf, 0) != 0) {
		goto error;
	}

	jdoc.free_fn(jdoc.buf.s);
	jdoc.buf.s = NULL;
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


int pres_dmq_replicate_presentity(presentity_t *presentity, str *body,
		int t_new, str *cur_etag, char *sphere, str *ruid, dmq_node_t *node)
{

	srjson_doc_t jdoc;
	srjson_t *p_json;

	LM_DBG("replicating presentity record - old etag %.*s, new etag %.*s, ruid "
		   "%.*s\n",
			presentity->etag.len, presentity->etag.s, cur_etag->len,
			cur_etag->s, ruid->len, ruid->s);

	if(!pres_dmq_proc_init && pres_dmq_init_proc() < 0) {
		return -1;
	}

	if(*pres_dmq_recv) {
		return 0;
	}

	srjson_InitDoc(&jdoc, NULL);

	jdoc.root = srjson_CreateObject(&jdoc);
	if(jdoc.root == NULL) {
		LM_ERR("cannot create json root\n");
		goto error;
	}

	// action
	srjson_AddNumberToObject(
			&jdoc, jdoc.root, "action", PRES_DMQ_UPDATE_PRESENTITY);
	// presentity
	p_json = srjson_CreateObject(&jdoc);
	srjson_AddStrToObject(&jdoc, p_json, "domain", presentity->domain.s,
			presentity->domain.len);
	srjson_AddStrToObject(
			&jdoc, p_json, "user", presentity->user.s, presentity->user.len);
	srjson_AddStrToObject(
			&jdoc, p_json, "etag", presentity->etag.s, presentity->etag.len);
	srjson_AddNumberToObject(&jdoc, p_json, "expires", presentity->expires);
	srjson_AddNumberToObject(&jdoc, p_json, "recv", presentity->received_time);
	if(presentity->sender) {
		srjson_AddStrToObject(&jdoc, p_json, "sender", presentity->sender->s,
				presentity->sender->len);
	}
	srjson_AddStrToObject(&jdoc, p_json, "event", presentity->event->name.s,
			presentity->event->name.len);
	srjson_AddItemToObject(&jdoc, jdoc.root, "presentity", p_json);
	// t_new
	srjson_AddNumberToObject(&jdoc, jdoc.root, "t_new", t_new);
	// cur_etag
	if(cur_etag) {
		srjson_AddStrToObject(
				&jdoc, jdoc.root, "cur_etag", cur_etag->s, cur_etag->len);
	}
	// sphere
	if(sphere) {
		srjson_AddStringToObject(&jdoc, jdoc.root, "sphere", sphere);
	}
	// ruid
	if(ruid) {
		srjson_AddStrToObject(&jdoc, jdoc.root, "ruid", ruid->s, ruid->len);
	}
	// body
	if(body) {
		srjson_AddStrToObject(&jdoc, jdoc.root, "body", body->s, body->len);
	}

	jdoc.buf.s = srjson_PrintUnformatted(&jdoc, jdoc.root);
	if(jdoc.buf.s == NULL) {
		LM_ERR("unable to serialize data\n");
		goto error;
	}
	jdoc.buf.len = strlen(jdoc.buf.s);
	LM_DBG("sending serialized data %.*s\n", jdoc.buf.len, jdoc.buf.s);
	if(pres_dmq_send(&jdoc.buf, node) != 0) {
		goto error;
	}

	jdoc.free_fn(jdoc.buf.s);
	jdoc.buf.s = NULL;
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


int pres_dmq_send_all_presentities(dmq_node_t *dmq_node)
{
	// TODO: implement send all presentities

	return 0;
}


/**
* @brief dmq response callback
*/
int pres_dmq_resp_callback_f(
		struct sip_msg *msg, int code, dmq_node_t *node, void *param)
{
	LM_DBG("dmq response callback triggered [%p %d %p]\n", msg, code, param);
	return 0;
}

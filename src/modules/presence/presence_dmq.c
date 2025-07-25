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

int pres_dmq_send_all_presentities(dmq_node_t *dmq_node);
int pres_dmq_send_all_subscriptions(dmq_node_t *dmq_node);
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

	if(publ_cache_mode == PS_PCACHE_RECORD && pres_subs_dbmode == NO_DB) {
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
		pres_dmqb.send_message(
				pres_dmq_peer, body, node, NULL, 1, &pres_dmq_content_type);
	} else {
		LM_DBG("sending dmq broadcast...\n");
		pres_dmqb.bcast_message(
				pres_dmq_peer, body, 0, NULL, 1, &pres_dmq_content_type);
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
 * @brief extract subscription from json object
*/
subs_t *pres_parse_json_subscription(srjson_t *in)
{
	subs_t subscription;
	str s_event_str = STR_NULL;
	srjson_t *s_it;

	memset(&subscription, 0, sizeof(subs_t));
	LM_DBG("extracting subscription\n");

	for(s_it = in->child; s_it; s_it = s_it->next) {
		if(strcmp(s_it->string, "pres_uri") == 0) {
			subscription.pres_uri.s = s_it->valuestring;
			subscription.pres_uri.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "contact") == 0) {
			subscription.contact.s = s_it->valuestring;
			subscription.contact.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "local_contact") == 0) {
			subscription.local_contact.s = s_it->valuestring;
			subscription.local_contact.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "watcher_domain") == 0) {
			subscription.watcher_domain.s = s_it->valuestring;
			subscription.watcher_domain.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "watcher_user") == 0) {
			subscription.watcher_user.s = s_it->valuestring;
			subscription.watcher_user.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "from_domain") == 0) {
			subscription.from_domain.s = s_it->valuestring;
			subscription.from_domain.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "from_user") == 0) {
			subscription.from_user.s = s_it->valuestring;
			subscription.from_user.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "to_domain") == 0) {
			subscription.to_domain.s = s_it->valuestring;
			subscription.to_domain.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "to_user") == 0) {
			subscription.to_user.s = s_it->valuestring;
			subscription.to_user.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "from_tag") == 0) {
			subscription.from_tag.s = s_it->valuestring;
			subscription.from_tag.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "to_tag") == 0) {
			subscription.to_tag.s = s_it->valuestring;
			subscription.to_tag.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "user_agent") == 0) {
			subscription.user_agent.s = s_it->valuestring;
			subscription.user_agent.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "callid") == 0) {
			subscription.callid.s = s_it->valuestring;
			subscription.callid.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "reason") == 0) {
			subscription.reason.s = s_it->valuestring;
			subscription.reason.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "socket_info") == 0) {
			subscription.sockinfo_str.s = s_it->valuestring;
			subscription.sockinfo_str.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "record_route") == 0) {
			subscription.record_route.s = s_it->valuestring;
			subscription.record_route.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "event_id") == 0) {
			subscription.event_id.s = s_it->valuestring;
			subscription.event_id.len = strlen(s_it->valuestring);
		} else if(strcmp(s_it->string, "local_cseq") == 0) {
			subscription.local_cseq = SRJSON_GET_INT(s_it);
		} else if(strcmp(s_it->string, "remote_cseq") == 0) {
			subscription.remote_cseq = SRJSON_GET_INT(s_it);
		} else if(strcmp(s_it->string, "expires") == 0) {
			subscription.expires = SRJSON_GET_INT(s_it);
		} else if(strcmp(s_it->string, "flags") == 0) {
			subscription.flags = SRJSON_GET_INT(s_it);
		} else if(strcmp(s_it->string, "status") == 0) {
			subscription.status = SRJSON_GET_INT(s_it);
		} else if(strcmp(s_it->string, "version") == 0) {
			subscription.version = SRJSON_GET_INT(s_it);
		} else if(strcmp(s_it->string, "updated") == 0) {
			subscription.updated = SRJSON_GET_INT(s_it);
		} else if(strcmp(s_it->string, "updated_winfo") == 0) {
			subscription.updated_winfo = SRJSON_GET_INT(s_it);
		} else if(strcmp(s_it->string, "event") == 0) {
			s_event_str.s = s_it->valuestring;
			s_event_str.len = strlen(s_it->valuestring);
			subscription.event = contains_event(&s_event_str, 0);
			if(!subscription.event) {
				LM_ERR("unsupported event %s\n", s_it->valuestring);
				return NULL;
			}
		} else {
			LM_ERR("unrecognized field in json object\n");
			return NULL;
		}
	}

	if((pres_server_address.s) && (pres_server_address.len != 0)) {
		subscription.local_contact.s = pres_server_address.s;
		subscription.local_contact.len = pres_server_address.len;
	}

	if((pres_default_socket.s) && (pres_default_socket.len != 0)) {
		subscription.sockinfo_str.s = pres_default_socket.s;
		subscription.sockinfo_str.len = pres_default_socket.len;
	}

	return mem_copy_subs(&subscription, PKG_MEM_TYPE);
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
	subs_t *subscription = NULL;

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
		} else if(strcmp(it->string, "subscription") == 0) {
			subscription = pres_parse_json_subscription(it);
			if(!subscription) {
				LM_ERR("failed to construct subscription from json\n");
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
			if(p_body.len == 0) {
				p_body.s = NULL;
			}
		} else {
			LM_ERR("unrecognized field in json object\n");
			goto invalid;
		}
	}

	switch(action) {
		case PRES_DMQ_UPDATE_PRESENTITY:
			if(presentity == NULL
					|| update_presentity(NULL, presentity, &p_body, t_new,
							   &sent_reply, sphere, &cur_etag, &ruid,
							   presentity->event->evp->type == EVENT_DIALOG ? 0
																			: 1,
							   pres_skip_notify_dmq)
							   < 0) {
				goto error;
			}
			break;
		case PRES_DMQ_SYNC_PRESENTITY:
			*pres_dmq_recv = 0;
			if(pres_dmq_send_all_presentities(node) < 0) {
				goto error;
			}
			break;
		case PRES_DMQ_UPDATE_SUBSCRIPTION:
			if(subscription == NULL || replace_subscription(subscription) < 0) {
				goto error;
			}
			break;
		case PRES_DMQ_SYNC_SUBSCRIPTION:
			*pres_dmq_recv = 0;
			if(pres_dmq_send_all_subscriptions(node) < 0) {
				goto error;
			}
			break;
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
	if(subscription)
		pkg_free(subscription);

	return 0;
}


int pres_dmq_request_method_sync(int action)
{
	srjson_doc_t jdoc;

	LM_DBG("requesting sync from dmq peers\n");

	srjson_InitDoc(&jdoc, NULL);

	jdoc.root = srjson_CreateObject(&jdoc);
	if(jdoc.root == NULL) {
		LM_ERR("cannot create json root\n");
		goto error;
	}

	srjson_AddNumberToObject(&jdoc, jdoc.root, "action", action);
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


int pres_dmq_request_sync()
{
	if(pres_enable_pres_dmq > 0 && pres_enable_pres_sync_dmq > 0) {
		if(pres_dmq_request_method_sync(PRES_DMQ_SYNC_PRESENTITY) != 0) {
			LM_ERR("cannot send presence sync request\n");
			return -1;
		}
	}
	if(pres_enable_subs_dmq > 0 && pres_enable_subs_sync_dmq > 0) {
		if(pres_dmq_request_method_sync(PRES_DMQ_SYNC_SUBSCRIPTION) != 0) {
			LM_ERR("cannot send subscription sync request\n");
			return -1;
		}
	}
	return 0;
}


int pres_dmq_replicate_presentity(presentity_t *presentity, str *body,
		int t_new, str *cur_etag, char *sphere, str *ruid, dmq_node_t *node)
{

	srjson_doc_t jdoc;
	srjson_t *p_json;

	if(!pres_dmq_proc_init && pres_dmq_init_proc() < 0) {
		return -1;
	}

	if(*pres_dmq_recv) {
		return 0;
	}

	LM_DBG("replicating presentity record - old etag %.*s, new etag %.*s, ruid "
		   "%.*s\n",
			presentity->etag.len, presentity->etag.s, cur_etag->len,
			cur_etag->s, ruid->len, ruid->s);

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


int pres_dmq_replicate_subscription(subs_t *subscription, dmq_node_t *node)
{

	srjson_doc_t jdoc;
	srjson_t *s_json;
	LM_DBG("replicating subscription record - pres_uri %.*s, watcher_user "
		   "%.*s, watcher_domain %.*s\n",
			subscription->pres_uri.len, subscription->pres_uri.s,
			subscription->watcher_user.len, subscription->watcher_user.s,
			subscription->watcher_domain.len, subscription->watcher_domain.s);

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
			&jdoc, jdoc.root, "action", PRES_DMQ_UPDATE_SUBSCRIPTION);
	// subscription
	s_json = srjson_CreateObject(&jdoc);
	srjson_AddStrToObject(&jdoc, s_json, "pres_uri", subscription->pres_uri.s,
			subscription->pres_uri.len);
	srjson_AddStrToObject(&jdoc, s_json, "contact", subscription->contact.s,
			subscription->contact.len);
	srjson_AddStrToObject(&jdoc, s_json, "local_contact",
			subscription->local_contact.s, subscription->local_contact.len);
	srjson_AddStrToObject(&jdoc, s_json, "watcher_domain",
			subscription->watcher_domain.s, subscription->watcher_domain.len);
	srjson_AddStrToObject(&jdoc, s_json, "watcher_user",
			subscription->watcher_user.s, subscription->watcher_user.len);
	srjson_AddStrToObject(&jdoc, s_json, "from_domain",
			subscription->from_domain.s, subscription->from_domain.len);
	srjson_AddStrToObject(&jdoc, s_json, "from_user", subscription->from_user.s,
			subscription->from_user.len);
	srjson_AddStrToObject(&jdoc, s_json, "to_domain", subscription->to_domain.s,
			subscription->to_domain.len);
	srjson_AddStrToObject(&jdoc, s_json, "to_user", subscription->to_user.s,
			subscription->to_user.len);
	srjson_AddStrToObject(&jdoc, s_json, "from_tag", subscription->from_tag.s,
			subscription->from_tag.len);
	srjson_AddStrToObject(&jdoc, s_json, "to_tag", subscription->to_tag.s,
			subscription->to_tag.len);
	srjson_AddStrToObject(&jdoc, s_json, "user_agent",
			subscription->user_agent.s, subscription->user_agent.len);
	srjson_AddStrToObject(&jdoc, s_json, "callid", subscription->callid.s,
			subscription->callid.len);
	srjson_AddStrToObject(&jdoc, s_json, "reason", subscription->reason.s,
			subscription->reason.len);
	srjson_AddStrToObject(&jdoc, s_json, "socket_info",
			subscription->sockinfo_str.s, subscription->sockinfo_str.len);
	srjson_AddStrToObject(&jdoc, s_json, "record_route",
			subscription->record_route.s, subscription->record_route.len);
	srjson_AddStrToObject(&jdoc, s_json, "event_id", subscription->event_id.s,
			subscription->event_id.len);
	if(subscription->event)
		srjson_AddStrToObject(&jdoc, s_json, "event",
				subscription->event->name.s, subscription->event->name.len);
	srjson_AddNumberToObject(
			&jdoc, s_json, "local_cseq", subscription->local_cseq);
	srjson_AddNumberToObject(
			&jdoc, s_json, "remote_cseq", subscription->remote_cseq);
	if(subscription->expires != 0
			&& subscription->expires < ksr_time_sint(NULL, NULL)) {
		srjson_AddNumberToObject(&jdoc, s_json, "expires",
				subscription->expires + ksr_time_sint(NULL, NULL));
	} else {
		srjson_AddNumberToObject(
				&jdoc, s_json, "expires", subscription->expires);
	}
	srjson_AddNumberToObject(&jdoc, s_json, "flags", subscription->flags);
	srjson_AddNumberToObject(&jdoc, s_json, "status", subscription->status);
	srjson_AddNumberToObject(&jdoc, s_json, "version", subscription->version);
	srjson_AddNumberToObject(&jdoc, s_json, "updated", subscription->updated);
	srjson_AddNumberToObject(
			&jdoc, s_json, "updated_winfo", subscription->updated_winfo);

	srjson_AddItemToObject(&jdoc, jdoc.root, "subscription", s_json);

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
	LM_ERR("subscription replication failed\n");
	if(jdoc.buf.s != NULL) {
		jdoc.free_fn(jdoc.buf.s);
		jdoc.buf.s = NULL;
	}
	srjson_DestroyDoc(&jdoc);
	return -1;
}


int pres_dmq_cache_send_all_presentities(dmq_node_t *dmq_node)
{
	int i;
	ps_ptable_t *ps_ptable = NULL;
	ps_presentity_t *ptn = NULL;
	presentity_t *presentity = NULL;
	str pempty = str_init("");
	pres_ev_t *ev;

	LM_DBG("send_all_presentities from cache started\n");

	ps_ptable = ps_ptable_get();
	if(ps_ptable == NULL) {
		LM_ERR("can't find presence table\n");
		return -1;
	}

	for(i = 0; i < ps_ptable->ssize; i++) {
		lock_get(&ps_ptable->slots[i].lock);
		ptn = ps_ptable->slots[i].plist;
		while(ptn != NULL) {
			ev = contains_event(&ptn->event, NULL);
			presentity = new_presentity(&ptn->domain, &ptn->user,
					ptn->expires - ptn->received_time, ev, &ptn->etag,
					(ptn->sender.s) ? &ptn->sender : NULL);
			if(presentity != NULL) {
				presentity->priority = ptn->priority;
				presentity->received_time = ptn->received_time;
				pres_dmq_replicate_presentity(presentity,
						(ptn->body.s) ? &ptn->body : &pempty, 1, &pempty, NULL,
						(ptn->ruid.s) ? &ptn->ruid : &pempty, dmq_node);
				pkg_free(presentity);
			}
			ptn = ptn->next;
		}
		lock_release(&ps_ptable->slots[i].lock);
	}

	return 0;
}


int pres_dmq_send_all_presentities(dmq_node_t *dmq_node)
{
	if(publ_cache_mode == PS_PCACHE_RECORD) {
		return pres_dmq_cache_send_all_presentities(dmq_node);
	} else {
		// not implemented for db mode
		return 0;
	}
}


int pres_dmq_cache_send_all_subscriptions(dmq_node_t *dmq_node)
{
	int i;
	subs_t *s = NULL;

	LM_DBG("send_all_subscriptions from cache started\n");

	for(i = 0; i < shtable_size; i++) {
		lock_get(&subs_htable[i].lock);

		s = subs_htable[i].entries->next;

		while(s) {
			LM_DBG("send_all_subscriptions found subs to pres_uri %.*s with "
				   "callid %.*s\n",
					s->pres_uri.len, s->pres_uri.s, s->callid.len, s->callid.s);
			pres_dmq_replicate_subscription(s, dmq_node);
			s = s->next;
		}

		lock_release(&subs_htable[i].lock);
	}

	return 0;
}


int pres_dmq_send_all_subscriptions(dmq_node_t *dmq_node)
{
	if(pres_subs_dbmode != DB_ONLY) {
		return pres_dmq_cache_send_all_subscriptions(dmq_node);
	} else {
		// not implemented for db mode
		return 0;
	}
}

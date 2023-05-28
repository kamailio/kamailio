/*
 * Copyright (C) 2014 Andrey Rybkin <rybkin.a@bks.tv>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Edited :
 *
 * Copyright (C) 2017 Julien Chavanton, Flowroute
 */

#include "usrloc_sync.h"
#include "../usrloc/usrloc.h"
#include "../usrloc/ul_callback.h"
#include "../usrloc/dlist.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_addr_spec.h"

static str usrloc_dmq_content_type = str_init("application/json");
static str dmq_200_rpl = str_init("OK");
static str dmq_400_rpl = str_init("Bad Request");
static str dmq_500_rpl = str_init("Server Internal Error");

static int *usrloc_dmq_recv = 0;

dmq_api_t usrloc_dmqb;
dmq_peer_t *usrloc_dmq_peer = NULL;
dmq_resp_cback_t usrloc_dmq_resp_callback = {&usrloc_dmq_resp_callback_f, 0};

int usrloc_dmq_send_all();
int usrloc_dmq_request_sync();
int usrloc_dmq_send_contact(
		ucontact_t *ptr, str aor, int action, dmq_node_t *node);
int usrloc_dmq_send_multi_contact(
		ucontact_t *ptr, str aor, int action, dmq_node_t *node);
void usrloc_dmq_send_multi_contact_flush(dmq_node_t *node);

#define MAX_AOR_LEN 256

extern int _dmq_usrloc_sync;
extern int _dmq_usrloc_replicate_socket_info;
extern int _dmq_usrloc_batch_msg_contacts;
extern int _dmq_usrloc_batch_msg_size;
extern int _dmq_usrloc_batch_size;
extern int _dmq_usrloc_batch_usleep;
extern str _dmq_usrloc_domain;
extern int _dmq_usrloc_delete;

static int add_contact(str aor, ucontact_info_t *ci)
{
	urecord_t *r = NULL;
	udomain_t *_d;
	ucontact_t *c = NULL;
	str contact;
	int res;

	if(dmq_ul.get_udomain(_dmq_usrloc_domain.s, &_d) < 0) {
		LM_ERR("Failed to get domain\n");
		return -1;
	}

	LM_DBG("aor: %.*s\n", aor.len, aor.s);
	LM_DBG("ci->ruid: %.*s\n", ci->ruid.len, ci->ruid.s);
	LM_DBG("aorhash: %i\n", dmq_ul.get_aorhash(&aor));

	if(ci->ruid.len > 0) {
		// Search by ruid, if possible
		res = dmq_ul.get_urecord_by_ruid(
				_d, dmq_ul.get_aorhash(&aor), &ci->ruid, &r, &c);
		if(res == 0) {
			LM_DBG("Found contact\n");
			dmq_ul.update_ucontact(r, c, ci);
			LM_DBG("Release record\n");
			dmq_ul.release_urecord(r);
			LM_DBG("Unlock udomain\n");
			dmq_ul.unlock_udomain(_d, &aor);
			return 0;
		}
	}

	dmq_ul.lock_udomain(_d, &aor);
	res = dmq_ul.get_urecord(_d, &aor, &r);
	if(res < 0) {
		LM_ERR("failed to retrieve record from usrloc\n");
		goto error;
	} else if(res == 0) {
		LM_DBG("'%.*s' found in usrloc\n", aor.len, ZSW(aor.s));
		res = dmq_ul.get_ucontact(r, ci->c, ci->callid, ci->path, ci->cseq, &c);
		LM_DBG("get_ucontact = %d\n", res);
		if(res == -1) {
			LM_ERR("Invalid cseq\n");
			goto error;
		} else if(res > 0) {
			LM_DBG("Not found contact\n");
			contact.s = ci->c->s;
			contact.len = ci->c->len;
			dmq_ul.insert_ucontact(r, &contact, ci, &c);
		} else if(res == 0) {
			LM_DBG("Found contact\n");
			dmq_ul.update_ucontact(r, c, ci);
		}
	} else {
		LM_DBG("'%.*s' Not found in usrloc\n", aor.len, ZSW(aor.s));
		dmq_ul.insert_urecord(_d, &aor, &r);
		LM_DBG("Insert record\n");
		contact.s = ci->c->s;
		contact.len = ci->c->len;
		dmq_ul.insert_ucontact(r, &contact, ci, &c);
		LM_DBG("Insert ucontact\n");
	}

	LM_DBG("Release record\n");
	dmq_ul.release_urecord(r);
	LM_DBG("Unlock udomain\n");
	dmq_ul.unlock_udomain(_d, &aor);
	return 0;
error:
	dmq_ul.unlock_udomain(_d, &aor);
	return -1;
}

static int delete_contact(str aor, ucontact_info_t *ci)
{
	udomain_t *_d;
	urecord_t *r;
	ucontact_t *c;

	if(dmq_ul.get_udomain(_dmq_usrloc_domain.s, &_d) < 0) {
		LM_ERR("Failed to get domain\n");
		return -1;
	}

	/* it locks the udomain on success */
	if(dmq_ul.get_urecord_by_ruid(
			   _d, dmq_ul.get_aorhash(&aor), &ci->ruid, &r, &c)
			!= 0) {
		LM_DBG("AOR/Contact [%.*s] not found\n", aor.len, aor.s);
		return -1;
	}
	if(dmq_ul.delete_ucontact(r, c) != 0) {
		dmq_ul.unlock_udomain(_d, &aor);
		LM_WARN("could not delete contact\n");
		return -1;
	}
	dmq_ul.release_urecord(r);
	dmq_ul.unlock_udomain(_d, &aor);

	return 0;
}

#define dmq_usrloc_malloc malloc
#define dmq_usrloc_free free

void usrloc_get_all_ucontact(dmq_node_t *node)
{
	int rval, len = 0;
	void *buf, *cp;
	str c, recv;
	str path;
	str ruid;
	unsigned int aorhash;
	struct socket_info *send_sock;
	unsigned int flags;

	len = 0;
	buf = NULL;

	str aor;
	urecord_t *r;
	udomain_t *_d;
	ucontact_t *ptr = 0;
	int res;
	int n;

	if(dmq_ul.get_all_ucontacts == NULL) {
		LM_ERR("dmq_ul.get_all_ucontacts is NULL\n");
		goto done;
	}

	if(dmq_ul.get_udomain(_dmq_usrloc_domain.s, &_d) < 0) {
		LM_ERR("Failed to get domain\n");
		goto done;
	}

	rval = dmq_ul.get_all_ucontacts(buf, len, 0, 0, 1, 0);
	if(rval < 0) {
		LM_ERR("failed to fetch contacts\n");
		goto done;
	}
	if(rval > 0) {
		len = rval * 2;
		buf = dmq_usrloc_malloc(len);
		if(buf == NULL) {
			PKG_MEM_ERROR;
			goto done;
		}
		rval = dmq_ul.get_all_ucontacts(buf, len, 0, 0, 1, 0);
		if(rval != 0) {
			dmq_usrloc_free(buf);
			goto done;
		}
	}
	if(buf == NULL)
		goto done;
	cp = buf;
	n = 0;
	while(1) {
		memcpy(&(c.len), cp, sizeof(c.len));
		if(c.len == 0)
			break;
		c.s = (char *)cp + sizeof(c.len);
		cp = (char *)cp + sizeof(c.len) + c.len;
		memcpy(&(recv.len), cp, sizeof(recv.len));
		recv.s = (char *)cp + sizeof(recv.len);
		cp = (char *)cp + sizeof(recv.len) + recv.len;
		memcpy(&send_sock, cp, sizeof(send_sock));
		cp = (char *)cp + sizeof(send_sock);
		memcpy(&flags, cp, sizeof(flags));
		cp = (char *)cp + sizeof(flags);
		memcpy(&(path.len), cp, sizeof(path.len));
		path.s = path.len ? ((char *)cp + sizeof(path.len)) : NULL;
		cp = (char *)cp + sizeof(path.len) + path.len;
		memcpy(&(ruid.len), cp, sizeof(ruid.len));
		ruid.s = ruid.len ? ((char *)cp + sizeof(ruid.len)) : NULL;
		cp = (char *)cp + sizeof(ruid.len) + ruid.len;
		memcpy(&aorhash, cp, sizeof(aorhash));
		cp = (char *)cp + sizeof(aorhash);

		r = 0;
		ptr = 0;
		res = dmq_ul.get_urecord_by_ruid(_d, aorhash, &ruid, &r, &ptr);
		if(res < 0) {
			LM_DBG("'%.*s' Not found in usrloc\n", ruid.len, ZSW(ruid.s));
			continue;
		}
		aor = r->aor;
		LM_DBG("- AoR: %.*s  AoRhash=%d  Flags=%d\n", aor.len, aor.s, aorhash,
				flags);

		while(ptr) {
			if(_dmq_usrloc_batch_msg_contacts > 1) {
				usrloc_dmq_send_multi_contact(ptr, aor, DMQ_UPDATE, node);
			} else {
				usrloc_dmq_send_contact(ptr, aor, DMQ_UPDATE, node);
			}
			n++;
			ptr = ptr->next;
		}
		dmq_ul.release_urecord(r);
		dmq_ul.unlock_udomain(_d, &aor);
		if(_dmq_usrloc_batch_size > 0 && _dmq_usrloc_batch_usleep > 0) {
			if(n >= _dmq_usrloc_batch_size) {
				n = 0;
				sleep_us(_dmq_usrloc_batch_usleep);
			}
		}
	}
	dmq_usrloc_free(buf);
	usrloc_dmq_send_multi_contact_flush(node); // send any remaining contacts

done:
	c.s = "";
	c.len = 0;
}


int usrloc_dmq_initialize()
{
	dmq_peer_t not_peer;

	/* load the DMQ API */
	if(dmq_load_api(&usrloc_dmqb) != 0) {
		LM_ERR("cannot load dmq api\n");
		return -1;
	} else {
		LM_DBG("loaded dmq api\n");
	}
	not_peer.callback = usrloc_dmq_handle_msg;
	not_peer.init_callback = usrloc_dmq_request_sync;
	not_peer.description.s = "usrloc";
	not_peer.description.len = 6;
	not_peer.peer_id.s = "usrloc";
	not_peer.peer_id.len = 6;
	usrloc_dmq_peer = usrloc_dmqb.register_dmq_peer(&not_peer);
	if(!usrloc_dmq_peer) {
		LM_ERR("error in register_dmq_peer\n");
		goto error;
	} else {
		LM_DBG("dmq peer registered\n");
	}
	return 0;
error:
	return -1;
}


int usrloc_dmq_send(str *body, dmq_node_t *node)
{
	if(!usrloc_dmq_peer) {
		LM_ERR("dlg_dmq_peer is null!\n");
		return -1;
	}
	if(node) {
		LM_DBG("sending dmq message ...\n");
		usrloc_dmqb.send_message(usrloc_dmq_peer, body, node,
				&usrloc_dmq_resp_callback, 1, &usrloc_dmq_content_type);
	} else {
		LM_DBG("sending dmq broadcast...\n");
		usrloc_dmqb.bcast_message(usrloc_dmq_peer, body, 0,
				&usrloc_dmq_resp_callback, 1, &usrloc_dmq_content_type);
	}
	return 0;
}

static int usrloc_dmq_execute_action(srjson_t *jdoc_action, dmq_node_t *node)
{
	static ucontact_info_t ci;
	srjson_t *it = NULL;
	struct socket_info *sock = 0;
	unsigned int action, expires, cseq, flags, cflags, q, last_modified,
			methods, reg_id, server_id, port, proto;
	str aor = STR_NULL, ruid = STR_NULL, received = STR_NULL,
		instance = STR_NULL, sockname = STR_NULL;
	static str host = STR_NULL, c = STR_NULL, callid = STR_NULL,
			   path = STR_NULL, user_agent = STR_NULL;

	action = expires = cseq = flags = cflags = q = last_modified = methods =
			reg_id = server_id = port = proto = 0;

	for(it = jdoc_action; it; it = it->next) {
		if(it->string == NULL)
			continue;

		if(strcmp(it->string, "action") == 0) {
			action = SRJSON_GET_UINT(it);
		} else if(strcmp(it->string, "aor") == 0) {
			aor.s = it->valuestring;
			aor.len = strlen(aor.s);
		} else if(strcmp(it->string, "ruid") == 0) {
			ruid.s = it->valuestring;
			ruid.len = strlen(ruid.s);
		} else if(strcmp(it->string, "c") == 0) {
			c.s = it->valuestring;
			c.len = strlen(c.s);
		} else if(strcmp(it->string, "received") == 0) {
			received.s = it->valuestring;
			received.len = strlen(received.s);
		} else if(_dmq_usrloc_replicate_socket_info
						  == DMQ_USRLOC_REPLICATE_SOCKET
				  && strcmp(it->string, "sock") == 0) {
			if(parse_phostport(it->valuestring, &host.s, &host.len,
					   (int *)&port, (int *)&proto)
					!= 0) {
				LM_ERR("bad socket <%s>\n", it->valuestring);
				return 0;
			}
			sock = grep_sock_info(&host, (unsigned short)port, proto);
			if(sock == 0) {
				LM_DBG("non-local socket <%s>...ignoring\n", it->valuestring);
			} else {
				sock = grep_sock_info(&host, (unsigned short)port, proto);
				sock->sock_str.s = it->valuestring;
				sock->sock_str.len = strlen(sock->sock_str.s);
			}
		} else if(_dmq_usrloc_replicate_socket_info
						  == DMQ_USRLOC_REPLICATE_SOCKNAME
				  && strcmp(it->string, "sockname") == 0) {
			sockname.s = it->valuestring;
			sockname.len = strlen(sockname.s);
			sock = ksr_get_socket_by_name(&sockname);
			if(sock == 0) {
				LM_DBG("socket with name <%s> not known ... ignoring\n",
						it->valuestring);
			}
		} else if(strcmp(it->string, "path") == 0) {
			path.s = it->valuestring;
			path.len = strlen(path.s);
		} else if(strcmp(it->string, "callid") == 0) {
			callid.s = it->valuestring;
			callid.len = strlen(callid.s);
		} else if(strcmp(it->string, "user_agent") == 0) {
			user_agent.s = it->valuestring;
			user_agent.len = strlen(user_agent.s);
		} else if(strcmp(it->string, "instance") == 0) {
			instance.s = it->valuestring;
			instance.len = strlen(instance.s);
		} else if(strcmp(it->string, "expires") == 0) {
			expires = SRJSON_GET_UINT(it);
		} else if(strcmp(it->string, "cseq") == 0) {
			cseq = SRJSON_GET_UINT(it);
		} else if(strcmp(it->string, "flags") == 0) {
			flags = SRJSON_GET_UINT(it);
		} else if(strcmp(it->string, "cflags") == 0) {
			cflags = SRJSON_GET_UINT(it);
		} else if(strcmp(it->string, "q") == 0) {
			q = SRJSON_GET_UINT(it);
		} else if(strcmp(it->string, "last_modified") == 0) {
			last_modified = SRJSON_GET_UINT(it);
		} else if(strcmp(it->string, "methods") == 0) {
			methods = SRJSON_GET_UINT(it);
		} else if(strcmp(it->string, "reg_id") == 0) {
			reg_id = SRJSON_GET_UINT(it);
		} else if(strcmp(it->string, "server_id") == 0) {
			server_id = SRJSON_GET_UINT(it);
		} else {
			LM_ERR("unrecognized field in json object\n");
		}
	}
	memset(&ci, 0, sizeof(ucontact_info_t));
	ci.ruid = ruid;
	ci.c = &c;
	ci.received = received;
	if(_dmq_usrloc_replicate_socket_info
			& (DMQ_USRLOC_REPLICATE_SOCKET | DMQ_USRLOC_REPLICATE_SOCKNAME))
		ci.sock = sock;
	ci.path = &path;
	ci.expires = expires;
	ci.q = q;
	ci.callid = &callid;
	ci.cseq = cseq;
	ci.flags = flags;
	ci.cflags = cflags;
	ci.user_agent = &user_agent;
	ci.methods = methods;
	ci.instance = instance;
	ci.reg_id = reg_id;
	ci.server_id = server_id;
	ci.tcpconn_id = -1;
	ci.last_modified = last_modified;

	switch(action) {
		case DMQ_UPDATE:
			LM_DBG("Received DMQ_UPDATE. Update contact info...\n");
			add_contact(aor, &ci);
			break;
		case DMQ_RM:
			LM_DBG("Received DMQ_RM. Delete contact info...\n");
			delete_contact(aor, &ci);
			break;
		case DMQ_SYNC:
			LM_DBG("Received DMQ_SYNC. Sending all contacts...\n");
			usrloc_get_all_ucontact(node);
			break;
		case DMQ_NONE:
			LM_DBG("Received DMQ_NONE. Not used...\n");
			break;
		default:
			return 0;
	}
	return 1;
}

static int init_usrloc_dmq_recv()
{
	if(!usrloc_dmq_recv) {
		LM_DBG("Initializing usrloc_dmq_recv for pid (%d)\n", my_pid());
		usrloc_dmq_recv = (int *)pkg_malloc(sizeof(int));
		if(!usrloc_dmq_recv) {
			PKG_MEM_ERROR;
			return -1;
		}
		*usrloc_dmq_recv = 0;
	}
	return 0;
}

/**
 * @brief ht dmq callback
 */
int usrloc_dmq_handle_msg(
		struct sip_msg *msg, peer_reponse_t *resp, dmq_node_t *node)
{
	int content_length;
	str body;
	srjson_doc_t jdoc;

	if(!usrloc_dmq_recv && init_usrloc_dmq_recv() < 0) {
		return 0;
	}

	*usrloc_dmq_recv = 1;

	srjson_InitDoc(&jdoc, NULL);
	if(parse_from_header(msg) < 0) {
		LM_ERR("failed to parse from header\n");
		goto invalid;
	}
	body = ((struct to_body *)msg->from->parsed)->uri;

	LM_DBG("dmq message received from %.*s\n", body.len, body.s);

	if(parse_headers(msg, HDR_EOH_F, 0) < 0) {
		LM_ERR("failed to parse the headers\n");
		goto invalid;
	}
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

	jdoc.buf = body;
	if(jdoc.root == NULL) {
		jdoc.root = srjson_Parse(&jdoc, jdoc.buf.s);
		if(jdoc.root == NULL) {
			LM_ERR("invalid json doc [[%s]]\n", jdoc.buf.s);
			goto invalid;
		}
	}

	if(strcmp(jdoc.root->child->string, "multi") == 0) {
		LM_DBG("request [%s]\n", jdoc.root->child->string);
		srjson_t *jdoc_actions = jdoc.root->child->child;
		srjson_t *it = NULL;
		for(it = jdoc_actions; it; it = it->next) {
			LM_DBG("action [%s]\n", jdoc_actions->child->string);
			if(!usrloc_dmq_execute_action(it->child, node))
				goto invalid;
		}
	} else {
		if(!usrloc_dmq_execute_action(jdoc.root->child, node))
			goto invalid;
	}

	*usrloc_dmq_recv = 0;
	srjson_DestroyDoc(&jdoc);
	resp->reason = dmq_200_rpl;
	resp->resp_code = 200;
	return 0;

invalid:
	*usrloc_dmq_recv = 0;
	srjson_DestroyDoc(&jdoc);
	resp->reason = dmq_400_rpl;
	resp->resp_code = 400;
	return 0;

error:
	*usrloc_dmq_recv = 0;
	srjson_DestroyDoc(&jdoc);
	resp->reason = dmq_500_rpl;
	resp->resp_code = 500;
	return 0;
}


int usrloc_dmq_request_sync()
{
	srjson_doc_t jdoc;

	if(_dmq_usrloc_sync == 0)
		return 0;

	LM_DBG("requesting sync from dmq peers\n");
	srjson_InitDoc(&jdoc, NULL);

	jdoc.root = srjson_CreateObject(&jdoc);
	if(jdoc.root == NULL) {
		LM_ERR("cannot create json root\n");
		goto error;
	}

	srjson_AddNumberToObject(&jdoc, jdoc.root, "action", DMQ_SYNC);
	jdoc.buf.s = srjson_PrintUnformatted(&jdoc, jdoc.root);
	if(jdoc.buf.s == NULL) {
		LM_ERR("unable to serialize data\n");
		goto error;
	}
	jdoc.buf.len = strlen(jdoc.buf.s);
	LM_DBG("sending serialized data %.*s\n", jdoc.buf.len, jdoc.buf.s);
	if(usrloc_dmq_send(&jdoc.buf, 0) != 0) {
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

/* while prt append json string
 * */

/* Multi contacts */
typedef struct jdoc_contact_group
{
	int count;
	int size;
	srjson_doc_t jdoc;
	srjson_t *jdoc_contacts;
} jdoc_contact_group_t;

static jdoc_contact_group_t jdoc_contact_group;

static void usrloc_dmq_contacts_group_init(void)
{
	if(jdoc_contact_group.jdoc.root)
		return;
	jdoc_contact_group.count = 0;
	jdoc_contact_group.size = 12; // {"multi":{}}
	srjson_InitDoc(&jdoc_contact_group.jdoc, NULL);
	LM_DBG("init multi contacts batch. \n");
	jdoc_contact_group.jdoc.root =
			srjson_CreateObject(&jdoc_contact_group.jdoc);
	if(jdoc_contact_group.jdoc.root == NULL)
		LM_ERR("cannot create json root ! \n");
	jdoc_contact_group.jdoc_contacts =
			srjson_CreateObject(&jdoc_contact_group.jdoc);
	if(jdoc_contact_group.jdoc_contacts == NULL) {
		LM_ERR("cannot create json contacts ! \n");
		srjson_DestroyDoc(&jdoc_contact_group.jdoc);
	}
}

static void usrloc_dmq_contacts_group_send(dmq_node_t *node)
{
	if(jdoc_contact_group.count == 0)
		return;
	srjson_doc_t *jdoc = &jdoc_contact_group.jdoc;
	srjson_t *jdoc_contacts = jdoc_contact_group.jdoc_contacts;

	srjson_AddItemToObject(jdoc, jdoc->root, "multi", jdoc_contacts);

	LM_DBG("json[%s]\n", srjson_PrintUnformatted(jdoc, jdoc->root));
	jdoc->buf.s = srjson_PrintUnformatted(jdoc, jdoc->root);
	if(jdoc->buf.s == NULL) {
		LM_ERR("unable to serialize data\n");
		goto error;
	}
	jdoc->buf.len = strlen(jdoc->buf.s);

	LM_DBG("sending serialized data %.*s\n", jdoc->buf.len, jdoc->buf.s);
	if(usrloc_dmq_send(&jdoc->buf, node) != 0) {
		LM_ERR("unable to send data\n");
		goto error;
	}

	jdoc->free_fn(jdoc->buf.s);
	jdoc->buf.s = NULL;
	srjson_DestroyDoc(jdoc);
	return;

error:
	if(jdoc->buf.s != NULL) {
		jdoc->free_fn(jdoc->buf.s);
		jdoc->buf.s = NULL;
	}
	srjson_DestroyDoc(jdoc);
	return;
}

void usrloc_dmq_send_multi_contact_flush(dmq_node_t *node)
{
	usrloc_dmq_contacts_group_send(node);
	usrloc_dmq_contacts_group_init();
}

int usrloc_dmq_send_multi_contact(
		ucontact_t *ptr, str aor, int action, dmq_node_t *node)
{

	usrloc_dmq_contacts_group_init();

	srjson_doc_t *jdoc = &jdoc_contact_group.jdoc;
	srjson_t *jdoc_contacts = jdoc_contact_group.jdoc_contacts;

	srjson_t *jdoc_contact = srjson_CreateObject(jdoc);
	if(!jdoc_contact) {
		LM_ERR("cannot create json root\n");
		return -1;
	}
	LM_DBG("group size[%d]\n", jdoc_contact_group.size);
	jdoc_contact_group.size +=
			201; // json overhead ("":{"action":,"aor":"","ruid":"","c":""...)

	srjson_AddNumberToObject(jdoc, jdoc_contact, "action", action);
	jdoc_contact_group.size += snprintf(NULL, 0, "%d", action);

	srjson_AddStrToObject(jdoc, jdoc_contact, "aor", aor.s, aor.len);
	jdoc_contact_group.size += aor.len;
	srjson_AddStrToObject(
			jdoc, jdoc_contact, "ruid", ptr->ruid.s, ptr->ruid.len);
	jdoc_contact_group.size += ptr->ruid.len;
	srjson_AddStrToObject(jdoc, jdoc_contact, "c", ptr->c.s, ptr->c.len);
	jdoc_contact_group.size += ptr->c.len;
	srjson_AddStrToObject(
			jdoc, jdoc_contact, "received", ptr->received.s, ptr->received.len);
	jdoc_contact_group.size += ptr->received.len;
	if(_dmq_usrloc_replicate_socket_info == DMQ_USRLOC_REPLICATE_SOCKET
			&& ptr->sock != NULL && ptr->sock->sock_str.s != NULL) {
		srjson_AddStrToObject(jdoc, jdoc_contact, "sock", ptr->sock->sock_str.s,
				ptr->sock->sock_str.len);
		jdoc_contact_group.size += ptr->sock->sock_str.len;
	} else if(_dmq_usrloc_replicate_socket_info == DMQ_USRLOC_REPLICATE_SOCKNAME
			  && ptr->sock != NULL && ptr->sock->sockname.s != NULL) {
		srjson_AddStrToObject(jdoc, jdoc_contact, "sockname",
				ptr->sock->sockname.s, ptr->sock->sockname.len);
		jdoc_contact_group.size += ptr->sock->sockname.len;
	}
	srjson_AddStrToObject(
			jdoc, jdoc_contact, "path", ptr->path.s, ptr->path.len);
	jdoc_contact_group.size += ptr->path.len;
	srjson_AddStrToObject(
			jdoc, jdoc_contact, "callid", ptr->callid.s, ptr->callid.len);
	jdoc_contact_group.size += ptr->callid.len;
	srjson_AddStrToObject(jdoc, jdoc_contact, "user_agent", ptr->user_agent.s,
			ptr->user_agent.len);
	jdoc_contact_group.size += ptr->user_agent.len;
	srjson_AddStrToObject(
			jdoc, jdoc_contact, "instance", ptr->instance.s, ptr->instance.len);
	jdoc_contact_group.size += ptr->instance.len;
	srjson_AddNumberToObject(jdoc, jdoc_contact, "expires", ptr->expires);
	jdoc_contact_group.size += snprintf(NULL, 0, "%.0lf", (double)ptr->expires);
	srjson_AddNumberToObject(jdoc, jdoc_contact, "cseq", ptr->cseq);
	jdoc_contact_group.size += snprintf(NULL, 0, "%d", ptr->cseq);
	srjson_AddNumberToObject(jdoc, jdoc_contact, "flags", ptr->flags);
	jdoc_contact_group.size += snprintf(NULL, 0, "%d", ptr->flags);
	srjson_AddNumberToObject(jdoc, jdoc_contact, "cflags", ptr->cflags);
	jdoc_contact_group.size += snprintf(NULL, 0, "%d", ptr->cflags);
	srjson_AddNumberToObject(jdoc, jdoc_contact, "q", ptr->q);
	jdoc_contact_group.size += snprintf(NULL, 0, "%d", ptr->q);
	srjson_AddNumberToObject(
			jdoc, jdoc_contact, "last_modified", ptr->last_modified);
	jdoc_contact_group.size +=
			snprintf(NULL, 0, "%.0lf", (double)ptr->last_modified);
	srjson_AddNumberToObject(jdoc, jdoc_contact, "methods", ptr->methods);
	jdoc_contact_group.size += snprintf(NULL, 0, "%u", ptr->methods);
	srjson_AddNumberToObject(jdoc, jdoc_contact, "reg_id", ptr->reg_id);
	jdoc_contact_group.size += snprintf(NULL, 0, "%d", ptr->reg_id);
	srjson_AddNumberToObject(jdoc, jdoc_contact, "server_id", ptr->server_id);
	jdoc_contact_group.size += snprintf(NULL, 0, "%d", ptr->server_id);

	char idx[5];
	jdoc_contact_group.count++;
	jdoc_contact_group.size += snprintf(idx, 5, "%d", jdoc_contact_group.count);
	srjson_AddItemToObject(jdoc, jdoc_contacts, idx, jdoc_contact);

	if(jdoc_contact_group.count >= _dmq_usrloc_batch_msg_contacts
			|| jdoc_contact_group.size >= _dmq_usrloc_batch_msg_size) {
		LM_DBG("sending group count[%d]size[%d]", jdoc_contact_group.count,
				jdoc_contact_group.size);
		usrloc_dmq_contacts_group_send(node);
		usrloc_dmq_contacts_group_init();
	}

	return 0;
}


int usrloc_dmq_send_contact(
		ucontact_t *ptr, str aor, int action, dmq_node_t *node)
{
	srjson_doc_t jdoc;
	srjson_InitDoc(&jdoc, NULL);

	jdoc.root = srjson_CreateObject(&jdoc);
	if(!jdoc.root) {
		LM_ERR("cannot create json root\n");
		goto error;
	}

	srjson_AddNumberToObject(&jdoc, jdoc.root, "action", action);

	srjson_AddStrToObject(&jdoc, jdoc.root, "aor", aor.s, aor.len);
	srjson_AddStrToObject(&jdoc, jdoc.root, "ruid", ptr->ruid.s, ptr->ruid.len);
	srjson_AddStrToObject(&jdoc, jdoc.root, "c", ptr->c.s, ptr->c.len);
	srjson_AddStrToObject(
			&jdoc, jdoc.root, "received", ptr->received.s, ptr->received.len);
	if(_dmq_usrloc_replicate_socket_info == DMQ_USRLOC_REPLICATE_SOCKET
			&& ptr->sock != NULL) {
		srjson_AddStrToObject(&jdoc, jdoc.root, "sock", ptr->sock->sock_str.s,
				ptr->sock->sock_str.len);
	} else if(_dmq_usrloc_replicate_socket_info == DMQ_USRLOC_REPLICATE_SOCKNAME
			  && ptr->sock != NULL && ptr->sock->sockname.s != NULL) {
		srjson_AddStrToObject(&jdoc, jdoc.root, "sockname",
				ptr->sock->sockname.s, ptr->sock->sockname.len);
	}
	srjson_AddStrToObject(&jdoc, jdoc.root, "path", ptr->path.s, ptr->path.len);
	srjson_AddStrToObject(
			&jdoc, jdoc.root, "callid", ptr->callid.s, ptr->callid.len);
	srjson_AddStrToObject(&jdoc, jdoc.root, "user_agent", ptr->user_agent.s,
			ptr->user_agent.len);
	srjson_AddStrToObject(
			&jdoc, jdoc.root, "instance", ptr->instance.s, ptr->instance.len);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "expires", ptr->expires);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "cseq", ptr->cseq);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "flags", ptr->flags);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "cflags", ptr->cflags);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "q", ptr->q);
	srjson_AddNumberToObject(
			&jdoc, jdoc.root, "last_modified", ptr->last_modified);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "methods", ptr->methods);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "reg_id", ptr->reg_id);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "server_id", ptr->server_id);

	jdoc.buf.s = srjson_PrintUnformatted(&jdoc, jdoc.root);
	if(jdoc.buf.s == NULL) {
		LM_ERR("unable to serialize data\n");
		goto error;
	}
	jdoc.buf.len = strlen(jdoc.buf.s);

	LM_DBG("sending serialized data %.*s\n", jdoc.buf.len, jdoc.buf.s);
	if(usrloc_dmq_send(&jdoc.buf, node) != 0) {
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

int usrloc_dmq_resp_callback_f(
		struct sip_msg *msg, int code, dmq_node_t *node, void *param)
{
	LM_DBG("dmq response callback triggered [%p %d %p]\n", msg, code, param);
	return 0;
}

void dmq_ul_cb_contact(ucontact_t *ptr, int type, void *param)
{
	str aor;

	LM_DBG("Callback from usrloc with type=%d\n", type);
	aor.s = ptr->aor->s;
	aor.len = ptr->aor->len;

	if(!usrloc_dmq_recv && init_usrloc_dmq_recv() < 0) {
		return;
	}

	if(!*usrloc_dmq_recv) {
		LM_DBG("Replicating local update to other nodes...\n");
		switch(type) {
			case UL_CONTACT_INSERT:
				usrloc_dmq_send_contact(ptr, aor, DMQ_UPDATE, 0);
				break;
			case UL_CONTACT_UPDATE:
				usrloc_dmq_send_contact(ptr, aor, DMQ_UPDATE, 0);
				break;
			case UL_CONTACT_DELETE:
				if(_dmq_usrloc_delete >= 1) {
					usrloc_dmq_send_contact(ptr, aor, DMQ_RM, 0);
				}
				break;
			case UL_CONTACT_EXPIRE:
				//usrloc_dmq_send_contact(ptr, aor, DMQ_UPDATE);
				LM_DBG("Contact <%.*s> expired\n", aor.len, aor.s);
				break;
		}
	} else {
		LM_DBG("Contact received from DMQ... skip\n");
	}
}

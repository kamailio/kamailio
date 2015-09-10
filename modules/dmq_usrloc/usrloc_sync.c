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
*/

#include "usrloc_sync.h"
#include "../usrloc/usrloc.h"
#include "../usrloc/ul_callback.h"
#include "../usrloc/dlist.h"
#include "../../dprint.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_addr_spec.h"

static str usrloc_dmq_content_type = str_init("application/json");
static str dmq_200_rpl  = str_init("OK");
static str dmq_400_rpl  = str_init("Bad Request");
static str dmq_500_rpl  = str_init("Server Internal Error");

dmq_api_t usrloc_dmqb;
dmq_peer_t* usrloc_dmq_peer = NULL;
dmq_resp_cback_t usrloc_dmq_resp_callback = {&usrloc_dmq_resp_callback_f, 0};

int usrloc_dmq_send_all();
int usrloc_dmq_request_sync();
int usrloc_dmq_send_contact(ucontact_t* ptr, str aor, int action, dmq_node_t* node);

#define MAX_AOR_LEN 256

static int add_contact(str aor, ucontact_info_t* ci)
{
	urecord_t* r;
	udomain_t* _d;
	ucontact_t* c;
	str contact;
	int res;

        if (dmq_ul.get_udomain("location", &_d) < 0) {
                LM_ERR("Failed to get domain\n");
                return -1;
        }
	res = dmq_ul.get_urecord(_d, &aor, &r);
	if (res < 0) {
		LM_ERR("failed to retrieve record from usrloc\n");
		goto error;
	} else if ( res == 0) {
		LM_DBG("'%.*s' found in usrloc\n", aor.len, ZSW(aor.s));
		res = dmq_ul.get_ucontact(r, ci->c, ci->callid, ci->path, ci->cseq, &c);
		LM_DBG("get_ucontact = %d\n", res);
		if (res==-1) {
			LM_ERR("Invalid cseq\n");
			goto error;
		} else if (res > 0 ) {
			LM_DBG("Not found contact\n");
			contact.s = ci->c->s;
			contact.len = ci->c->len;
			dmq_ul.insert_ucontact(r, &contact, ci, &c);
		} else if (res == 0) {
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

static int delete_contact(str aor, ucontact_info_t* ci)
{
	udomain_t* _d;
	urecord_t* r;
	ucontact_t* c;

	if (dmq_ul.get_udomain("location", &_d) < 0) {
		LM_ERR("Failed to get domain\n");
		return -1;
	}

	if (dmq_ul.get_urecord_by_ruid(_d, dmq_ul.get_aorhash(&aor),
				&ci->ruid, &r, &c) != 0) {
		LM_WARN("AOR/Contact not found\n");
		return -1;
	}
	if (dmq_ul.delete_ucontact(r, c) != 0) {
		dmq_ul.unlock_udomain(_d, &aor);
		LM_WARN("could not delete contact\n");
		return -1;
	}
	dmq_ul.release_urecord(r);
	dmq_ul.unlock_udomain(_d, &aor);

	return 0;
}

void usrloc_get_all_ucontact(dmq_node_t* node)
{
 	int rval, len=0;
	void *buf, *cp;
	str c, recv;
	str path;
	str ruid;
	unsigned int aorhash;
	struct socket_info* send_sock;
	unsigned int flags;

	len = 0;
	buf = NULL;

  str aor;
  urecord_t* r;
  udomain_t* _d;
  ucontact_t* ptr = 0;
  int res;

  if (dmq_ul.get_all_ucontacts == NULL){
    LM_ERR("dmq_ul.get_all_ucontacts is NULL\n");
    goto done;
  }

	if (dmq_ul.get_udomain("location", &_d) < 0) {
		LM_ERR("Failed to get domain\n");
		goto done;
	}

	rval = dmq_ul.get_all_ucontacts(buf, len, 0, 0, 1);
	if (rval<0) {
		LM_ERR("failed to fetch contacts\n");
		goto done;
	}
	if (rval > 0) {
		if (buf != NULL)
			pkg_free(buf);
		len = rval * 2;
		buf = pkg_malloc(len);
		if (buf == NULL) {
			LM_ERR("out of pkg memory\n");
			goto done;
		}
		rval = dmq_ul.get_all_ucontacts(buf, len, 0, 0, 1);
		if (rval != 0) {
			pkg_free(buf);
			goto done;
		}
	}
	if (buf == NULL)
		goto done;
	  cp = buf;
    while (1) {
        memcpy(&(c.len), cp, sizeof(c.len));
        if (c.len == 0)
            break;
        c.s = (char*)cp + sizeof(c.len);
        cp =  (char*)cp + sizeof(c.len) + c.len;
        memcpy(&(recv.len), cp, sizeof(recv.len));
        recv.s = (char*)cp + sizeof(recv.len);
        cp =  (char*)cp + sizeof(recv.len) + recv.len;
        memcpy( &send_sock, cp, sizeof(send_sock));
        cp = (char*)cp + sizeof(send_sock);
        memcpy( &flags, cp, sizeof(flags));
        cp = (char*)cp + sizeof(flags);
        memcpy( &(path.len), cp, sizeof(path.len));
        path.s = path.len ? ((char*)cp + sizeof(path.len)) : NULL ;
        cp =  (char*)cp + sizeof(path.len) + path.len;
        memcpy( &(ruid.len), cp, sizeof(ruid.len));
        ruid.s = ruid.len ? ((char*)cp + sizeof(ruid.len)) : NULL ;
        cp =  (char*)cp + sizeof(ruid.len) + ruid.len;
        memcpy( &aorhash, cp, sizeof(aorhash));
        cp = (char*)cp + sizeof(aorhash);

        res = dmq_ul.get_urecord_by_ruid(_d, aorhash, &ruid, &r, &ptr);
        aor = r->aor;
        if (res > 0) {
            LM_DBG("'%.*s' Not found in usrloc\n", aor.len, ZSW(aor.s));
            dmq_ul.release_urecord(r);
            dmq_ul.unlock_udomain(_d, &aor);
            continue;
        }
        LM_DBG("- AoR: %.*s  AoRhash=%d  Flags=%d\n", aor.len, aor.s, aorhash, flags);

        while (ptr) {
            usrloc_dmq_send_contact(ptr, aor, DMQ_UPDATE, node);
            ptr = ptr->next;
        }
        dmq_ul.release_urecord(r);
        dmq_ul.unlock_udomain(_d, &aor);
    }
	pkg_free(buf);

done:
	c.s = ""; c.len = 0;
}


int usrloc_dmq_initialize()
{
	dmq_peer_t not_peer;

	/* load the DMQ API */
	if (dmq_load_api(&usrloc_dmqb)!=0) {
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


int usrloc_dmq_send(str* body, dmq_node_t* node) {
	if (!usrloc_dmq_peer) {
		LM_ERR("dlg_dmq_peer is null!\n");
		return -1;
	}
	if (node) {
		LM_DBG("sending dmq message ...\n");
		usrloc_dmqb.send_message(usrloc_dmq_peer, body, node, &usrloc_dmq_resp_callback, 1, &usrloc_dmq_content_type);
	} else {
		LM_DBG("sending dmq broadcast...\n");
		usrloc_dmqb.bcast_message(usrloc_dmq_peer, body, 0, &usrloc_dmq_resp_callback, 1, &usrloc_dmq_content_type);
	}
	return 0;
}


/**
* @brief ht dmq callback
*/
int usrloc_dmq_handle_msg(struct sip_msg* msg, peer_reponse_t* resp, dmq_node_t* node)
{
	int content_length;
	str body;
	srjson_doc_t jdoc;
	srjson_t *it = NULL;
	static ucontact_info_t ci;

	int action, expires, cseq, flags, cflags, q, last_modified, methods, reg_id;
	str aor, ruid, c, received, path, callid, user_agent, instance;

	parse_from_header(msg);
	body = ((struct to_body*)msg->from->parsed)->uri;

	LM_DBG("dmq message received from %.*s\n", body.len, body.s);

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

	if (!body.s) {
		LM_ERR("unable to get body\n");
		goto error;
	}

	srjson_InitDoc(&jdoc, NULL);
	jdoc.buf = body;
	if(jdoc.root == NULL) {
		jdoc.root = srjson_Parse(&jdoc, jdoc.buf.s);
		if(jdoc.root == NULL)
		{
			LM_ERR("invalid json doc [[%s]]\n", jdoc.buf.s);
			goto invalid;
		}
	}

	for(it=jdoc.root->child; it; it = it->next)
	{
		if (it->string == NULL) continue;

		if (strcmp(it->string, "action")==0) {
			action = it->valueint;
		} else if (strcmp(it->string, "aor")==0) {
			aor.s = it->valuestring;
			aor.len = strlen(aor.s);
		} else if (strcmp(it->string, "ruid")==0) {
			ruid.s = it->valuestring;
			ruid.len = strlen(ruid.s);
		} else if (strcmp(it->string, "c")==0) {
			c.s = it->valuestring;
			c.len = strlen(c.s);
		} else if (strcmp(it->string, "received")==0) {
			received.s = it->valuestring;
			received.len = strlen(received.s);
		} else if (strcmp(it->string, "path")==0) {
			path.s = it->valuestring;
			path.len = strlen(path.s);
		} else if (strcmp(it->string, "callid")==0) {
			callid.s = it->valuestring;
			callid.len = strlen(callid.s);
		} else if (strcmp(it->string, "user_agent")==0) {
			user_agent.s = it->valuestring;
			user_agent.len = strlen(user_agent.s);
		} else if (strcmp(it->string, "instance")==0) {
			instance.s = it->valuestring;
			instance.len = strlen(instance.s);
		} else if (strcmp(it->string, "expires")==0) { //
			expires = it->valueint;
		} else if (strcmp(it->string, "cseq")==0) {
			cseq = it->valueint;
		} else if (strcmp(it->string, "flags")==0) {
			flags = it->valueint;
		} else if (strcmp(it->string, "cflags")==0) {
			cflags = it->valueint;
		} else if (strcmp(it->string, "q")==0) {
			q = it->valueint;
		} else if (strcmp(it->string, "last_modified")==0) {
			last_modified = it->valueint;
		} else if (strcmp(it->string, "methods")==0) {
			methods = it->valueint;
		} else if (strcmp(it->string, "reg_id")==0) {
			reg_id = it->valueint;
		} else {
			LM_ERR("unrecognized field in json object\n");
		}
	}
	memset( &ci, 0, sizeof(ucontact_info_t));
	ci.ruid = ruid;
	ci.c = &c;
	ci.received = received;
	ci.path = &path;
	ci.expires = expires;
	ci.q = q;
	ci.callid = &callid;
	ci.cseq = cseq;
	ci.flags = flags;
	ci.flags |= FL_RPL;
	ci.cflags = cflags;
	ci.user_agent = &user_agent;
	ci.methods = methods;
	ci.instance = instance;
	ci.reg_id = reg_id;
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

		default:  goto invalid;
	}

	srjson_DestroyDoc(&jdoc);
	resp->reason = dmq_200_rpl;
	resp->resp_code = 200;
	return 0;

invalid:
	srjson_DestroyDoc(&jdoc);
	resp->reason = dmq_400_rpl;
	resp->resp_code = 400;
	return 0;

error:
	srjson_DestroyDoc(&jdoc);
	resp->reason = dmq_500_rpl;
	resp->resp_code = 500;
	return 0;
}


int usrloc_dmq_request_sync() {
	srjson_doc_t jdoc;
	LM_DBG("requesting sync from dmq peers\n");
	srjson_InitDoc(&jdoc, NULL);

	jdoc.root = srjson_CreateObject(&jdoc);
	if(jdoc.root==NULL) {
		LM_ERR("cannot create json root\n");
		goto error;
	}

	srjson_AddNumberToObject(&jdoc, jdoc.root, "action", DMQ_SYNC);
	jdoc.buf.s = srjson_PrintUnformatted(&jdoc, jdoc.root);
	if(jdoc.buf.s==NULL) {
		LM_ERR("unable to serialize data\n");
		goto error;
	}
	jdoc.buf.len = strlen(jdoc.buf.s);
	LM_DBG("sending serialized data %.*s\n", jdoc.buf.len, jdoc.buf.s);
	if (usrloc_dmq_send(&jdoc.buf, 0)!=0) {
		goto error;
	}

	jdoc.free_fn(jdoc.buf.s);
	jdoc.buf.s = NULL;
	srjson_DestroyDoc(&jdoc);
	return 0;

error:
	if(jdoc.buf.s!=NULL) {
		jdoc.free_fn(jdoc.buf.s);
		jdoc.buf.s = NULL;
	}
	srjson_DestroyDoc(&jdoc);
	return -1;
}

int usrloc_dmq_send_contact(ucontact_t* ptr, str aor, int action, dmq_node_t* node) {
	srjson_doc_t jdoc;
	srjson_InitDoc(&jdoc, NULL);

	int flags;

	jdoc.root = srjson_CreateObject(&jdoc);
	if(jdoc.root==NULL) {
		LM_ERR("cannot create json root\n");
		goto error;
	}

	flags = ptr->flags;
	flags &= ~FL_RPL;

	srjson_AddNumberToObject(&jdoc, jdoc.root, "action", action);

	srjson_AddStrToObject(&jdoc, jdoc.root, "aor", aor.s, aor.len);
	srjson_AddStrToObject(&jdoc, jdoc.root, "ruid", ptr->ruid.s, ptr->ruid.len);
	srjson_AddStrToObject(&jdoc, jdoc.root, "c", ptr->c.s, ptr->c.len);
	srjson_AddStrToObject(&jdoc, jdoc.root, "received", ptr->received.s, ptr->received.len);
	srjson_AddStrToObject(&jdoc, jdoc.root, "path", ptr->path.s, ptr->path.len);
	srjson_AddStrToObject(&jdoc, jdoc.root, "callid", ptr->callid.s, ptr->callid.len);
	srjson_AddStrToObject(&jdoc, jdoc.root, "user_agent", ptr->user_agent.s, ptr->user_agent.len);
	srjson_AddStrToObject(&jdoc, jdoc.root, "instance", ptr->instance.s, ptr->instance.len);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "expires", ptr->expires);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "cseq", ptr->cseq);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "flags", flags);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "cflags", ptr->cflags);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "q", ptr->q);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "last_modified", ptr->last_modified);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "methods", ptr->methods);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "reg_id", ptr->reg_id);

	jdoc.buf.s = srjson_PrintUnformatted(&jdoc, jdoc.root);
	if(jdoc.buf.s==NULL) {
		LM_ERR("unable to serialize data\n");
		goto error;
	}
	jdoc.buf.len = strlen(jdoc.buf.s);

	LM_DBG("sending serialized data %.*s\n", jdoc.buf.len, jdoc.buf.s);
	if (usrloc_dmq_send(&jdoc.buf, node)!=0) {
		goto error;
	}

	jdoc.free_fn(jdoc.buf.s);
	jdoc.buf.s = NULL;
	srjson_DestroyDoc(&jdoc);
	return 0;

error:
	if(jdoc.buf.s!=NULL) {
		jdoc.free_fn(jdoc.buf.s);
		jdoc.buf.s = NULL;
	}
	srjson_DestroyDoc(&jdoc);
	return -1;
}

int usrloc_dmq_resp_callback_f(struct sip_msg* msg, int code,
                            dmq_node_t* node, void* param)
{
	LM_DBG("dmq response callback triggered [%p %d %p]\n", msg, code, param);
	return 0;
}

void dmq_ul_cb_contact(ucontact_t* ptr, int type, void* param)
{
	str aor;

		LM_DBG("Callback from usrloc with type=%d\n", type);
		aor.s = ptr->aor->s;
		aor.len = ptr->aor->len;

		if (!(ptr->flags & FL_RPL)) {

			switch(type){
				case UL_CONTACT_INSERT:
											usrloc_dmq_send_contact(ptr, aor, DMQ_UPDATE, 0);
										break;
				case UL_CONTACT_UPDATE:
											usrloc_dmq_send_contact(ptr, aor, DMQ_UPDATE, 0);
										break;
				case UL_CONTACT_DELETE:
											usrloc_dmq_send_contact(ptr, aor, DMQ_RM, 0);
										break;
				case UL_CONTACT_EXPIRE:
											//usrloc_dmq_send_contact(ptr, aor, DMQ_UPDATE);
											LM_DBG("Contact <%.*s> expired\n", aor.len, aor.s);
										break;
			}
		} else {
			LM_DBG("Contact recieved from DMQ... skip\n");
		}
}

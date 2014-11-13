/**
 * 
 * Copyright (C) 2013 Charles Chance (Sipcentric Ltd)
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


#include "ht_dmq.h"
#include "ht_api.h"

static str ht_dmq_content_type = str_init("application/json");
static str dmq_200_rpl  = str_init("OK");
static str dmq_400_rpl  = str_init("Bad Request");
static str dmq_500_rpl  = str_init("Server Internal Error");

typedef struct _ht_dmq_repdata {
	int action;
	str htname;
	str cname;
	int type;
	int intval;
	str strval;
	int expire;
} ht_dmq_repdata_t;

dmq_api_t ht_dmqb;
dmq_peer_t* ht_dmq_peer = NULL;
dmq_resp_cback_t ht_dmq_resp_callback = {&ht_dmq_resp_callback_f, 0};

/**
 * @brief add notification peer
 */
int ht_dmq_initialize()
{
	dmq_peer_t not_peer;

        /* load the DMQ API */
        if (dmq_load_api(&ht_dmqb)!=0) {
                LM_ERR("cannot load dmq api\n");
                return -1;
        } else {
                LM_DBG("loaded dmq api\n");
        }

	not_peer.callback = ht_dmq_handle_msg;
	not_peer.init_callback = NULL;
	not_peer.description.s = "htable";
	not_peer.description.len = 6;
	not_peer.peer_id.s = "htable";
	not_peer.peer_id.len = 6;
	ht_dmq_peer = ht_dmqb.register_dmq_peer(&not_peer);
	if(!ht_dmq_peer) {
		LM_ERR("error in register_dmq_peer\n");
		goto error;
	} else {
		LM_DBG("dmq peer registered\n");
	}
	return 0;
error:
	return -1;
}

int ht_dmq_broadcast(str* body) {
        if (!ht_dmq_peer) {
                LM_ERR("ht_dmq_peer is null!\n");
                return -1;
        }
        LM_DBG("sending broadcast...\n");
        ht_dmqb.bcast_message(ht_dmq_peer, body, 0, &ht_dmq_resp_callback, 1, &ht_dmq_content_type);
        return 0;
}

/**
 * @brief ht dmq callback
 */
int ht_dmq_handle_msg(struct sip_msg* msg, peer_reponse_t* resp, dmq_node_t* dmq_node)
{
	int content_length;
	str body;
	ht_dmq_action_t action = HT_DMQ_NONE;
	str htname, cname;
	int type = 0, mode = 0;
	int_str val;
	srjson_doc_t jdoc;
	srjson_t *it = NULL;

	/* received dmq message */
	LM_DBG("dmq message received\n");
	
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

	/* parse body */
	LM_DBG("body: %.*s\n", body.len, body.s);	

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
		LM_DBG("found field: %s\n", it->string);
		if (strcmp(it->string, "action")==0) {
			action = it->valueint;
		} else if (strcmp(it->string, "htname")==0) {
			htname.s = it->valuestring;
			htname.len = strlen(htname.s);
		} else if (strcmp(it->string, "cname")==0) {
			cname.s = it->valuestring;
			cname.len = strlen(cname.s);
		} else if (strcmp(it->string, "type")==0) {
			type = it->valueint;
		} else if (strcmp(it->string, "strval")==0) {
			val.s.s = it->valuestring;
			val.s.len = strlen(val.s.s);
		} else if (strcmp(it->string, "intval")==0) {
			val.n = it->valueint;
		} else if (strcmp(it->string, "mode")==0) {
			mode = it->valueint;
		} else {
			LM_ERR("unrecognized field in json object\n");
			goto invalid;
		}
	}	

	if (ht_dmq_replay_action(action, &htname, &cname, type, &val, mode)!=0) {
		LM_ERR("failed to replay action\n");
		goto error;
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

int ht_dmq_replicate_action(ht_dmq_action_t action, str* htname, str* cname, int type, int_str* val, int mode) {

	srjson_doc_t jdoc;

        LM_DBG("replicating action to dmq peers...\n");

	srjson_InitDoc(&jdoc, NULL);

	jdoc.root = srjson_CreateObject(&jdoc);
	if(jdoc.root==NULL) {
		LM_ERR("cannot create json root\n");
		goto error;
	}

	srjson_AddNumberToObject(&jdoc, jdoc.root, "action", action);
	srjson_AddStrToObject(&jdoc, jdoc.root, "htname", htname->s, htname->len);
	if (cname!=NULL) {
		srjson_AddStrToObject(&jdoc, jdoc.root, "cname", cname->s, cname->len);
	}

	if (action==HT_DMQ_SET_CELL || action==HT_DMQ_SET_CELL_EXPIRE || action==HT_DMQ_RM_CELL_RE) {
		srjson_AddNumberToObject(&jdoc, jdoc.root, "type", type);
		if (type&AVP_VAL_STR) {
			srjson_AddStrToObject(&jdoc, jdoc.root, "strval", val->s.s, val->s.len);
		} else {
			srjson_AddNumberToObject(&jdoc, jdoc.root, "intval", val->n);
		}
	}

	srjson_AddNumberToObject(&jdoc, jdoc.root, "mode", mode);	

	jdoc.buf.s = srjson_PrintUnformatted(&jdoc, jdoc.root);
	if(jdoc.buf.s!=NULL) {
		jdoc.buf.len = strlen(jdoc.buf.s);
		LM_DBG("sending serialized data %.*s\n", jdoc.buf.len, jdoc.buf.s);
		if (ht_dmq_broadcast(&jdoc.buf)!=0) {
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
	if(jdoc.buf.s!=NULL) {
		jdoc.free_fn(jdoc.buf.s);
		jdoc.buf.s = NULL;
	}
	srjson_DestroyDoc(&jdoc);
	return -1;
}

int ht_dmq_replay_action(ht_dmq_action_t action, str* htname, str* cname, int type, int_str* val, int mode) {

	ht_t* ht;
	ht = ht_get_table(htname);
	if(ht==NULL) {
		LM_ERR("unable to get table\n");
		return -1;
	}

        LM_DBG("replaying action %d on %.*s=>%.*s...\n", action, htname->len, htname->s, cname->len, cname->s);

	if (action==HT_DMQ_SET_CELL) {
		return ht_set_cell(ht, cname, type, val, mode);
	} else if (action==HT_DMQ_SET_CELL_EXPIRE) {
		return ht_set_cell_expire(ht, cname, 0, val);
	} else if (action==HT_DMQ_DEL_CELL) {
		return ht_del_cell(ht, cname);
	} else if (action==HT_DMQ_RM_CELL_RE) {
		return ht_rm_cell_re(&val->s, ht, mode);
	} else {
		LM_ERR("unrecognized action");
		return -1;
	}
}

/**
 * @brief dmq response callback
 */
int ht_dmq_resp_callback_f(struct sip_msg* msg, int code,
		dmq_node_t* node, void* param)
{
	LM_DBG("dmq response callback triggered [%p %d %p]\n", msg, code, param);
	return 0;
}

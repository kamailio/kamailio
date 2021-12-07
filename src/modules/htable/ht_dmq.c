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

typedef struct _ht_dmq_repdata {
	int action;
	str htname;
	str cname;
	int type;
	int intval;
	str strval;
	int expire;
} ht_dmq_repdata_t;

typedef struct _ht_dmq_jdoc_cell_group {
	int count;
	int size;
	srjson_doc_t jdoc;
	srjson_t *jdoc_cells;
} ht_dmq_jdoc_cell_group_t;

static str ht_dmq_content_type = str_init("application/json");
static str dmq_200_rpl  = str_init("OK");
static str dmq_400_rpl  = str_init("Bad Request");
static str dmq_500_rpl  = str_init("Server Internal Error");
static int dmq_cell_group_empty_size = 12; // {"cells":[]}
static int dmq_cell_group_max_size = 60000;
static ht_dmq_jdoc_cell_group_t ht_dmq_jdoc_cell_group;
extern int ht_dmq_init_sync;

dmq_api_t ht_dmqb;
dmq_peer_t* ht_dmq_peer = NULL;
dmq_resp_cback_t ht_dmq_resp_callback = {&ht_dmq_resp_callback_f, 0};

int ht_dmq_send(str* body, dmq_node_t* node);
int ht_dmq_send_sync(dmq_node_t* node);
int ht_dmq_handle_sync(srjson_doc_t* jdoc);

static int ht_dmq_cell_group_init(void) {

	if (ht_dmq_jdoc_cell_group.jdoc.root)
		return 0; // already initialised

	ht_dmq_jdoc_cell_group.count = 0;
	ht_dmq_jdoc_cell_group.size = dmq_cell_group_empty_size;

	srjson_InitDoc(&ht_dmq_jdoc_cell_group.jdoc, NULL);

	ht_dmq_jdoc_cell_group.jdoc.root = srjson_CreateObject(&ht_dmq_jdoc_cell_group.jdoc);
	if (ht_dmq_jdoc_cell_group.jdoc.root==NULL) {
		LM_ERR("cannot create json root object! \n");
		return -1;
	}

	ht_dmq_jdoc_cell_group.jdoc_cells = srjson_CreateArray(&ht_dmq_jdoc_cell_group.jdoc);
	if (ht_dmq_jdoc_cell_group.jdoc_cells==NULL) {
		LM_ERR("cannot create json cells array! \n");
		srjson_DestroyDoc(&ht_dmq_jdoc_cell_group.jdoc);
		return -1;
	}

	return 0;
}

static int ht_dmq_cell_group_write(str* htname, ht_cell_t* ptr) {

	// jsonify cell and add to array

	str tmp;
	srjson_doc_t *jdoc = &ht_dmq_jdoc_cell_group.jdoc;
	srjson_t *jdoc_cells = ht_dmq_jdoc_cell_group.jdoc_cells;
	srjson_t * jdoc_cell = srjson_CreateObject(jdoc);

	if(!jdoc_cell) {
		LM_ERR("cannot create cell json root\n");
		return -1;
	}

	// add json overhead
	if(ptr->flags&AVP_VAL_STR) {
		ht_dmq_jdoc_cell_group.size += 54; // {"htname":"","cname":"","type":,"strval":"","expire":}
	} else {
		ht_dmq_jdoc_cell_group.size += 52; // {"htname":"","cname":"","type":,"intval":,"expire":}
	}

	srjson_AddStrToObject(jdoc, jdoc_cell, "htname", htname->s, htname->len);
	ht_dmq_jdoc_cell_group.size += htname->len;

	srjson_AddStrToObject(jdoc, jdoc_cell, "cname", ptr->name.s, ptr->name.len);
	ht_dmq_jdoc_cell_group.size += ptr->name.len;

	if (ptr->flags&AVP_VAL_STR) {
		srjson_AddNumberToObject(jdoc, jdoc_cell, "type", AVP_VAL_STR);
		ht_dmq_jdoc_cell_group.size += 1;
		srjson_AddStrToObject(jdoc, jdoc_cell, "strval", ptr->value.s.s, ptr->value.s.len);
		ht_dmq_jdoc_cell_group.size += ptr->value.s.len;
	} else {
		srjson_AddNumberToObject(jdoc, jdoc_cell, "type", 0);
		ht_dmq_jdoc_cell_group.size += 1;
		srjson_AddNumberToObject(jdoc, jdoc_cell, "intval", ptr->value.n);
		tmp.s = sint2str((long)ptr->value.n, &tmp.len);
		ht_dmq_jdoc_cell_group.size += tmp.len;
	}

	srjson_AddNumberToObject(jdoc, jdoc_cell, "expire", ptr->expire);
	tmp.s = sint2str((long)ptr->expire, &tmp.len);
	ht_dmq_jdoc_cell_group.size += tmp.len;

	srjson_AddItemToArray(jdoc, jdoc_cells, jdoc_cell);

	ht_dmq_jdoc_cell_group.count++;

	return 0;
}

static int ht_dmq_cell_group_flush(dmq_node_t* node) {

	srjson_doc_t *jdoc = &ht_dmq_jdoc_cell_group.jdoc;
	srjson_t *jdoc_cells = ht_dmq_jdoc_cell_group.jdoc_cells;
	int ret = 0;

	srjson_AddItemToObject(jdoc, jdoc->root, "cells", jdoc_cells);

	LM_DBG("jdoc size[%d]\n", ht_dmq_jdoc_cell_group.size);
	jdoc->buf.s = srjson_PrintUnformatted(jdoc, jdoc->root);
	if(jdoc->buf.s==NULL) {
		LM_ERR("unable to serialize data\n");
		ret = -1;
		goto cleanup;
	}
	jdoc->buf.len = strlen(jdoc->buf.s);

	LM_DBG("sending serialized data %.*s\n", jdoc->buf.len, jdoc->buf.s);
	if (ht_dmq_send(&jdoc->buf, node)!=0) {
		LM_ERR("unable to send data\n");
		ret = -1;
	}

cleanup:

	srjson_DeleteItemFromObject(jdoc, jdoc->root, "cells");
	ht_dmq_jdoc_cell_group.count = 0;
	ht_dmq_jdoc_cell_group.size = dmq_cell_group_empty_size;

	if(jdoc->buf.s!=NULL) {
		jdoc->free_fn(jdoc->buf.s);
		jdoc->buf.s = NULL;
	}

	ht_dmq_jdoc_cell_group.jdoc_cells = srjson_CreateArray(&ht_dmq_jdoc_cell_group.jdoc);
	if (ht_dmq_jdoc_cell_group.jdoc_cells==NULL) {
		LM_ERR("cannot re-create json cells array! \n");
		ret = -1;
	}

	return ret;
}

static void ht_dmq_cell_group_destroy() {

	srjson_doc_t *jdoc = &ht_dmq_jdoc_cell_group.jdoc;

	if(jdoc->buf.s!=NULL) {
		jdoc->free_fn(jdoc->buf.s);
		jdoc->buf.s = NULL;
	}
	srjson_DestroyDoc(jdoc);

}

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
	not_peer.init_callback = (ht_dmq_init_sync ? ht_dmq_request_sync : NULL);
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

int ht_dmq_send(str* body, dmq_node_t* node) {
	if (!ht_dmq_peer) {
		LM_ERR("ht_dmq_peer is null!\n");
		return -1;
	}
	if (node) {
		LM_DBG("sending dmq message ...\n");
		ht_dmqb.send_message(ht_dmq_peer, body, node,
				&ht_dmq_resp_callback, 1, &ht_dmq_content_type);
	} else {
		LM_DBG("sending dmq broadcast...\n");
		ht_dmqb.bcast_message(ht_dmq_peer, body, 0,
				&ht_dmq_resp_callback, 1, &ht_dmq_content_type);
	}
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

	if (!body.s) {
		LM_ERR("unable to get body\n");
		goto error;
	}

	/* parse body */
	LM_DBG("body: %.*s\n", body.len, body.s);

	jdoc.buf = body;

	if(jdoc.root == NULL) {
		jdoc.root = srjson_Parse(&jdoc, jdoc.buf.s);
		if(jdoc.root == NULL)
		{
			LM_ERR("invalid json doc [[%s]]\n", jdoc.buf.s);
			goto invalid;
		}
	}

	if (unlikely(strcmp(jdoc.root->child->string, "cells")==0)) {
		ht_dmq_handle_sync(&jdoc);
	} else {

		for(it=jdoc.root->child; it; it = it->next)
		{
			LM_DBG("found field: %s\n", it->string);
			if (strcmp(it->string, "action")==0) {
				action = SRJSON_GET_INT(it);
			} else if (strcmp(it->string, "htname")==0) {
				htname.s = it->valuestring;
				htname.len = strlen(htname.s);
			} else if (strcmp(it->string, "cname")==0) {
				cname.s = it->valuestring;
				cname.len = strlen(cname.s);
			} else if (strcmp(it->string, "type")==0) {
				type = SRJSON_GET_INT(it);
			} else if (strcmp(it->string, "strval")==0) {
				val.s.s = it->valuestring;
				val.s.len = strlen(val.s.s);
			} else if (strcmp(it->string, "intval")==0) {
				val.n = SRJSON_GET_INT(it);
			} else if (strcmp(it->string, "mode")==0) {
				mode = SRJSON_GET_INT(it);
			} else {
				LM_ERR("unrecognized field in json object\n");
				goto invalid;
			}
		}

		if (unlikely(action == HT_DMQ_SYNC)) {
			ht_dmq_send_sync(dmq_node);
		} else {
			if (ht_dmq_replay_action(action, &htname, &cname, type, &val, mode)!=0) {
				LM_ERR("failed to replay action\n");
				goto error;
			}
		}

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

int ht_dmq_replicate_action(ht_dmq_action_t action, str* htname, str* cname,
		int type, int_str* val, int mode)
{

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

	if (action==HT_DMQ_SET_CELL || action==HT_DMQ_SET_CELL_EXPIRE
			|| action==HT_DMQ_RM_CELL_RE || action==HT_DMQ_RM_CELL_SW) {
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
		if (ht_dmq_send(&jdoc.buf, 0)!=0) {
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

/* Replay DMQ action

Return 0 for non-error. Allt other returns are parsed as error.
*/
int ht_dmq_replay_action(ht_dmq_action_t action, str* htname, str* cname,
		int type, int_str* val, int mode) {

	ht_t* ht;
	ht = ht_get_table(htname);
	if(ht==NULL) {
		LM_ERR("unable to get table\n");
		return -1;
	}

	LM_DBG("replaying action %d on %.*s=>%.*s...\n", action,
			htname->len, htname->s, cname->len, cname->s);

	if (action==HT_DMQ_SET_CELL) {
		return ht_set_cell(ht, cname, type, val, mode);
	} else if (action==HT_DMQ_SET_CELL_EXPIRE) {
		return ht_set_cell_expire(ht, cname, 0, val);
	} else if (action==HT_DMQ_DEL_CELL) {
		return ht_del_cell(ht, cname);
	} else if (action==HT_DMQ_RM_CELL_RE) {
		return ht_rm_cell_re(&val->s, ht, mode);
	} else if (action==HT_DMQ_RM_CELL_SW) {
		return ht_rm_cell_op(&val->s, ht, mode, HT_RM_OP_SW);
	} else {
		LM_ERR("unrecognized action\n");
		return -1;
	}
}

int ht_dmq_request_sync() {

	srjson_doc_t jdoc;

	LM_DBG("requesting sync from dmq peers\n");
	srjson_InitDoc(&jdoc, NULL);

	jdoc.root = srjson_CreateObject(&jdoc);
	if(jdoc.root==NULL) {
		LM_ERR("cannot create json root\n");
		goto error;
	}

	srjson_AddNumberToObject(&jdoc, jdoc.root, "action", HT_DMQ_SYNC);
	jdoc.buf.s = srjson_PrintUnformatted(&jdoc, jdoc.root);
	if(jdoc.buf.s==NULL) {
		LM_ERR("unable to serialize data\n");
		goto error;
	}
	jdoc.buf.len = strlen(jdoc.buf.s);
	LM_DBG("sending serialized data %.*s\n", jdoc.buf.len, jdoc.buf.s);
	if (ht_dmq_send(&jdoc.buf, 0)!=0) {
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

int ht_dmq_send_sync(dmq_node_t* node) {
	ht_t *ht;
	ht_cell_t *it;
	time_t now;
	int i;

	ht = ht_get_root();
	if(ht==NULL)
	{
		LM_DBG("no htables to sync!\n");
		return 0;
	}

	if (ht_dmq_cell_group_init() < 0)
		return -1;

	now = time(NULL);

	while (ht != NULL)
	{
		if (!ht->dmqreplicate)
			goto skip;

		for(i=0; i<ht->htsize; i++)
		{
			ht_slot_lock(ht, i);
			it = ht->entries[i].first;
			while(it)
			{
				if(ht->htexpire > 0) {
					if (it->expire <= now) {
						LM_DBG("skipping expired entry\n");
						it = it->next;
						continue;
					}
				}

				if (ht_dmq_cell_group_write(&ht->name, it) < 0) {
					ht_slot_unlock(ht, i);
					goto error;
				}

				if (ht_dmq_jdoc_cell_group.size >= dmq_cell_group_max_size) {
					LM_DBG("sending group count[%d]size[%d]\n", ht_dmq_jdoc_cell_group.count, ht_dmq_jdoc_cell_group.size);
					if (ht_dmq_cell_group_flush(node) < 0) {
						ht_slot_unlock(ht, i);
						goto error;
					}
				}

				it = it->next;
			}
			ht_slot_unlock(ht, i);
		}

skip:
		ht = ht->next;
	}

	if (ht_dmq_cell_group_flush(node) < 0)
		goto error;

	ht_dmq_cell_group_destroy();
	return 0;

error:
	ht_dmq_cell_group_destroy();
	return -1;
}

int ht_dmq_handle_sync(srjson_doc_t* jdoc) {
	LM_DBG("handling sync\n");

	srjson_t* cells = NULL;
	srjson_t* cell = NULL;
	srjson_t* it = NULL;
	str htname = STR_NULL;
	str cname = STR_NULL;
	int type = 0;
	int_str val = {0};
	int expire = 0;
	ht_t* ht = NULL;
	time_t now = 0;

	cells = jdoc->root->child;
	cell = cells->child;

	now = time(NULL);

	while (cell) {
		for(it=cell->child; it; it = it->next) {
			if (strcmp(it->string, "htname")==0) {
				htname.s = it->valuestring;
				htname.len = strlen(htname.s);
			} else if (strcmp(it->string, "cname")==0) {
				cname.s = it->valuestring;
				cname.len = strlen(cname.s);
			} else if (strcmp(it->string, "type")==0) {
				type = SRJSON_GET_INT(it);
			} else if (strcmp(it->string, "strval")==0) {
				val.s.s = it->valuestring;
				val.s.len = strlen(val.s.s);
			} else if (strcmp(it->string, "intval")==0) {
				val.n = SRJSON_GET_INT(it);
			} else if (strcmp(it->string, "expire")==0) {
				expire = SRJSON_GET_INT(it);
			} else {
				LM_WARN("unrecognized field in json object\n");
			}
		}

		if(htname.s!=NULL && htname.len>0 && cname.s!=NULL
				&& cname.len>0) {
			ht = ht_get_table(&htname);
			if(ht==NULL) {
				LM_WARN("unable to get table %.*s\n",
						htname.len, (htname.s)?htname.s:"");
			} else {
				if (ht_set_cell_ex(ht, &cname, type, &val, 0, expire - now) < 0) {
					LM_WARN("unable to set cell %.*s in table %.*s\n",
							cname.len, cname.s, ht->name.len, ht->name.s);
				}
			}
		}

		cell = cell->next;
	}
	return 0;
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

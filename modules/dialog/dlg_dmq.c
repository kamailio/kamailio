/**
* 
* Copyright (C) 2014 Alex Hermann (SpeakUp BV)
* Based on ht_dmq.c Copyright (C) 2013 Charles Chance (Sipcentric Ltd)
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


#include "dlg_dmq.h"
#include "dlg_hash.h"
#include "dlg_profile.h"

static str dlg_dmq_content_type = str_init("application/json");
static str dmq_200_rpl  = str_init("OK");
static str dmq_400_rpl  = str_init("Bad Request");
static str dmq_500_rpl  = str_init("Server Internal Error");

dmq_api_t dlg_dmqb;
dmq_peer_t* dlg_dmq_peer = NULL;
dmq_resp_cback_t dlg_dmq_resp_callback = {&dlg_dmq_resp_callback_f, 0};

int dmq_send_all_dlgs();
int dlg_dmq_request_sync();


/**
* @brief add notification peer
*/
int dlg_dmq_initialize()
{
	dmq_peer_t not_peer;

	/* load the DMQ API */
	if (dmq_load_api(&dlg_dmqb)!=0) {
		LM_ERR("cannot load dmq api\n");
		return -1;
	} else {
		LM_DBG("loaded dmq api\n");
	}

	not_peer.callback = dlg_dmq_handle_msg;
	not_peer.init_callback = dlg_dmq_request_sync;
	not_peer.description.s = "dialog";
	not_peer.description.len = 6;
	not_peer.peer_id.s = "dialog";
	not_peer.peer_id.len = 6;
	dlg_dmq_peer = dlg_dmqb.register_dmq_peer(&not_peer);
	if(!dlg_dmq_peer) {
		LM_ERR("error in register_dmq_peer\n");
		goto error;
	} else {
		LM_DBG("dmq peer registered\n");
	}
	return 0;
error:
	return -1;
}


int dlg_dmq_broadcast(str* body) {
	if (!dlg_dmq_peer) {
		LM_ERR("dlg_dmq_peer is null!\n");
		return -1;
	}
	LM_DBG("sending broadcast...\n");
	dlg_dmqb.bcast_message(dlg_dmq_peer, body, 0, &dlg_dmq_resp_callback, 1, &dlg_dmq_content_type);
	return 0;
}


/**
* @brief ht dmq callback
*/
int dlg_dmq_handle_msg(struct sip_msg* msg, peer_reponse_t* resp)
{
	int content_length;
	str body;
	dlg_cell_t *dlg;
	int unref = 0;
	int ret;
	srjson_doc_t jdoc, prof_jdoc;
	srjson_t *it = NULL;

	dlg_dmq_action_t action = DLG_DMQ_NONE;
	dlg_iuid_t iuid;
	str profiles = {0, 0}, callid = {0, 0};
	str dummy = {0, 0};
	unsigned int init_ts = 0, start_ts = 0, lifetime = 0;
	unsigned int state = 1;

	/* received dmq message */
	LM_DBG("dmq message received\n");
	
	if(!msg->content_length) {
		LM_ERR("no content length header found\n");
		goto invalid2;
	}
	content_length = get_content_length(msg);
	if(!content_length) {
		LM_DBG("content length is 0\n");
		goto invalid2;
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
		} else if (strcmp(it->string, "h_entry")==0) {
			iuid.h_entry = it->valueint;
		} else if (strcmp(it->string, "h_id")==0) {
			iuid.h_id = it->valueint;
		} else if (strcmp(it->string, "init_ts")==0) {
			init_ts = it->valueint;
		} else if (strcmp(it->string, "start_ts")==0) {
			start_ts = it->valueint;
		} else if (strcmp(it->string, "state")==0) {
			state = it->valueint;
		} else if (strcmp(it->string, "lifetime")==0) {
			lifetime = it->valueint;
		} else if (strcmp(it->string, "callid")==0) {
			callid.s = it->valuestring;
			callid.len = strlen(callid.s);
		} else if (strcmp(it->string, "profiles")==0) {
			profiles.s = it->valuestring;
			profiles.len = strlen(profiles.s);
		} else {
			LM_ERR("unrecognized field in json object\n");
		}
	}

	dlg = dlg_get_by_iuid(&iuid);
	if (dlg) {
		LM_DBG("found dialog [%u:%u] at %p\n", iuid.h_entry, iuid.h_id, dlg);
		unref++;
	}

	switch(action) {
		case DLG_DMQ_UPDATE: 
			LM_DBG("Updating dlg [%u:%u] with callid [%.*s]\n", iuid.h_entry, iuid.h_id,
					callid.len, callid.s);
			if (!dlg) {
				dlg = build_new_dlg(&callid, &dummy, &dummy, &dummy, &dummy);
				if (!dlg) {
					LM_ERR("failed to build new dialog\n");
					goto error;
				}

				if(dlg->h_entry != iuid.h_entry){
					LM_ERR("inconsistent hash data from peer: "
						"make sure all Kamailio's use the same hash size\n");
					shm_free(dlg);
					goto error;
				}

				/* link the dialog */
				link_dlg(dlg, 0, 0);

				/* override generated h_id */
				dlg->h_id = iuid.h_id;
				/* prevent DB sync */
				dlg->dflags &= ~(DLG_FLAG_NEW|DLG_FLAG_CHANGED);
			} else {
				/* remove existing profiles */
				if (dlg->profile_links!=NULL) {
					destroy_linkers(dlg->profile_links);
					dlg->profile_links = NULL;
				}
			}

			dlg->init_ts = init_ts;
			dlg->start_ts = start_ts;

			/* add profiles */
			if(profiles.s!=NULL) {
				srjson_InitDoc(&prof_jdoc, NULL);
				prof_jdoc.buf = profiles;
				dlg_json_to_profiles(dlg, &prof_jdoc);
				srjson_DestroyDoc(&prof_jdoc);
			}
			if (state == dlg->state) {
				break;
			}
			/* intentional fallthrough */

		case DLG_DMQ_STATE:
			if (!dlg) {
				LM_ERR("dialog [%u:%u] not found\n", iuid.h_entry, iuid.h_id);
				goto error;
			}
			if (state < dlg->state) {
				LM_NOTICE("Ignoring backwards state change on dlg [%u:%u] with callid [%.*s] from state [%u] to state [%u]\n",
					iuid.h_entry, iuid.h_id,
					dlg->callid.len, dlg->callid.s, dlg->state, state);
				break;
			}
			LM_DBG("State update dlg [%u:%u] with callid [%.*s] from state [%u] to state [%u]\n", iuid.h_entry, iuid.h_id,
					dlg->callid.len, dlg->callid.s, dlg->state, state);
			switch (state) {
				case DLG_STATE_CONFIRMED:
					dlg->start_ts = start_ts;
					dlg->lifetime = lifetime;
					if (insert_dlg_timer( &dlg->tl, dlg->lifetime ) != 0) {
						LM_CRIT("Unable to insert dlg timer %p [%u:%u]\n",
							dlg, dlg->h_entry, dlg->h_id);
					} else {
						/* dialog pointer inserted in timer list */
						dlg_ref(dlg, 1);
					}
					break;
				case DLG_STATE_DELETED:
					if (dlg->state == DLG_STATE_CONFIRMED) {
						ret = remove_dialog_timer(&dlg->tl);
						if (ret == 0) {
							/* one extra unref due to removal from timer list */
							unref++;
						} else if (ret < 0) {
							LM_CRIT("unable to unlink the timer on dlg %p [%u:%u]\n",
								dlg, dlg->h_entry, dlg->h_id);
						}
					}
					/* prevent DB sync */
					dlg->dflags |= DLG_FLAG_NEW;
					unref++;
					break;
				default:
					LM_ERR("unhandled state update to state %u\n", state);
					dlg_unref(dlg, unref);
					goto error;
			}
			dlg->state = state;
			break;

		case DLG_DMQ_RM:
			if (!dlg) {
				LM_DBG("dialog [%u:%u] not found\n", iuid.h_entry, iuid.h_id);
				goto error;
			}
			LM_DBG("Removed dlg [%u:%u] with callid [%.*s] int state [%u]\n", iuid.h_entry, iuid.h_id,
					dlg->callid.len, dlg->callid.s, dlg->state);
			if (dlg->state == DLG_STATE_CONFIRMED) {
				ret = remove_dialog_timer(&dlg->tl);
				if (ret == 0) {
					/* one extra unref due to removal from timer list */
					unref++;
				} else if (ret < 0) {
					LM_CRIT("unable to unlink the timer on dlg %p [%u:%u]\n",
						dlg, dlg->h_entry, dlg->h_id);
				}
			}
			/* prevent DB sync */
			dlg->dflags |= DLG_FLAG_NEW;
			unref++;
			break;

		case DLG_DMQ_SYNC:
			dmq_send_all_dlgs();
			break;

		case DLG_DMQ_NONE:
			break;
	}
	if (dlg && unref)
		dlg_unref(dlg, unref);

	srjson_DestroyDoc(&jdoc);
	resp->reason = dmq_200_rpl;
	resp->resp_code = 200;
	return 0;

invalid:
	srjson_DestroyDoc(&jdoc);
invalid2:
	resp->reason = dmq_400_rpl;
	resp->resp_code = 400;
	return 0;

error:
	srjson_DestroyDoc(&jdoc);
	resp->reason = dmq_500_rpl;
	resp->resp_code = 500;
	return 0;
}


int dlg_dmq_request_sync() {
	srjson_doc_t jdoc;

	LM_DBG("requesting sync from dmq peers\n");

	srjson_InitDoc(&jdoc, NULL);

	jdoc.root = srjson_CreateObject(&jdoc);
	if(jdoc.root==NULL) {
		LM_ERR("cannot create json root\n");
		goto error;
	}

	srjson_AddNumberToObject(&jdoc, jdoc.root, "action", DLG_DMQ_SYNC);
	jdoc.buf.s = srjson_PrintUnformatted(&jdoc, jdoc.root);
	if(jdoc.buf.s==NULL) {
		LM_ERR("unable to serialize data\n");
		goto error;
	}
	jdoc.buf.len = strlen(jdoc.buf.s);
	LM_DBG("sending serialized data %.*s\n", jdoc.buf.len, jdoc.buf.s);
	if (dlg_dmq_broadcast(&jdoc.buf)!=0) {
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


int dlg_dmq_replicate_action(dlg_dmq_action_t action, dlg_cell_t* dlg, int needlock) {

	srjson_doc_t jdoc, prof_jdoc;

	LM_DBG("replicating action [%d] on [%u:%u] to dmq peers\n", action, dlg->h_entry, dlg->h_id);

	if (action == DLG_DMQ_UPDATE) {
		if ((dlg->iflags & DLG_IFLAG_DMQ_SYNC) && ((dlg->dflags & DLG_FLAG_CHANGED_PROF) == 0)) {
			LM_DBG("dlg not changed, no sync\n");
			return 1;
		}
	} else if ((dlg->iflags & DLG_IFLAG_DMQ_SYNC) == 0) {
		LM_DBG("dlg not synced, no sync\n");
		return 1;
	}
	if (action == DLG_DMQ_STATE && (dlg->state != DLG_STATE_CONFIRMED && dlg->state != DLG_STATE_DELETED)){
		LM_DBG("not syncing state %u\n", dlg->state);
		return 1;
	}

	srjson_InitDoc(&jdoc, NULL);

	jdoc.root = srjson_CreateObject(&jdoc);
	if(jdoc.root==NULL) {
		LM_ERR("cannot create json root\n");
		goto error;
	}

	if (needlock)
		dlg_lock(d_table, &(d_table->entries[dlg->h_entry]));

	srjson_AddNumberToObject(&jdoc, jdoc.root, "action", action);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "h_entry", dlg->h_entry);
	srjson_AddNumberToObject(&jdoc, jdoc.root, "h_id", dlg->h_id);

	switch(action) {
		case DLG_DMQ_UPDATE:
			dlg->iflags |= DLG_IFLAG_DMQ_SYNC;
			dlg->dflags &= ~DLG_FLAG_CHANGED_PROF;
			srjson_AddNumberToObject(&jdoc, jdoc.root, "init_ts", dlg->init_ts);
			srjson_AddStrToObject(&jdoc, jdoc.root, "callid", dlg->callid.s, dlg->callid.len);

			if (dlg->profile_links) {
				srjson_InitDoc(&prof_jdoc, NULL);
				dlg_profiles_to_json(dlg, &prof_jdoc);
				if(prof_jdoc.buf.s!=NULL) {
					LM_DBG("adding profiles: [%.*s]\n", prof_jdoc.buf.len, prof_jdoc.buf.s);
					srjson_AddStrToObject(&jdoc, jdoc.root, "profiles",
							prof_jdoc.buf.s, prof_jdoc.buf.len);
					prof_jdoc.free_fn(prof_jdoc.buf.s);
					prof_jdoc.buf.s = NULL;
				}
				srjson_DestroyDoc(&prof_jdoc);
			}
			/* intentional fallthrough */

		case DLG_DMQ_STATE:
			srjson_AddNumberToObject(&jdoc, jdoc.root, "state", dlg->state);
			switch (dlg->state) {
				case DLG_STATE_CONFIRMED:
					srjson_AddNumberToObject(&jdoc, jdoc.root, "start_ts", dlg->start_ts);
					srjson_AddNumberToObject(&jdoc, jdoc.root, "lifetime", dlg->lifetime);
					break;
				case DLG_STATE_DELETED:
					dlg->iflags &= ~DLG_IFLAG_DMQ_SYNC;
					break;
				default:
					LM_DBG("not syncing state %u\n", dlg->state);
			}
			break;

		case DLG_DMQ_RM:
			srjson_AddNumberToObject(&jdoc, jdoc.root, "state", dlg->state);
			dlg->iflags &= ~DLG_IFLAG_DMQ_SYNC;
			break;

		case DLG_DMQ_NONE:
		case DLG_DMQ_SYNC:
			break;
	}
	if (needlock)
		dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));

	jdoc.buf.s = srjson_PrintUnformatted(&jdoc, jdoc.root);
	if(jdoc.buf.s==NULL) {
		LM_ERR("unable to serialize data\n");
		goto error;
	}
	jdoc.buf.len = strlen(jdoc.buf.s);
	LM_DBG("sending serialized data %.*s\n", jdoc.buf.len, jdoc.buf.s);
	if (dlg_dmq_broadcast(&jdoc.buf)!=0) {
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


int dmq_send_all_dlgs() {
	int index;
	dlg_entry_t entry;
	dlg_cell_t *dlg;

	LM_DBG("sending all dialogs \n");

	for(index = 0; index< d_table->size; index++){
		/* lock the whole entry */
		entry = (d_table->entries)[index];
		dlg_lock( d_table, &entry);

		for(dlg = entry.first; dlg != NULL; dlg = dlg->next){
			dlg->iflags &= ~DLG_IFLAG_DMQ_SYNC;
			dlg_dmq_replicate_action(DLG_DMQ_UPDATE, dlg, 0);
		}

		dlg_unlock( d_table, &entry);
	}

	return 0;
}


/**
* @brief dmq response callback
*/
int dlg_dmq_resp_callback_f(struct sip_msg* msg, int code,
                            dmq_node_t* node, void* param)
{
	LM_DBG("dmq response callback triggered [%p %d %p]\n", msg, code, param);
	return 0;
}

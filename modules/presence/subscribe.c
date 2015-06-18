/*
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of Kamailio, a free SIP serves.
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

/*! \file
 * \brief Kamailio presence module :: Support for SUBSCRIBE handling
 * \ingroup presence 
 */


#include "../../ut.h"
#include "../../usr_avp.h"
#include "../../data_lump_rpl.h"
#include "../../parser/parse_expires.h"
#include "../../parser/parse_event.h"
#include "../../parser/contact/parse_contact.h"
#include "../../hashes.h"
#include "presence.h"
#include "subscribe.h"
#include "utils_func.h"
#include "notify.h"
#include "../pua/hash.h"
#include "../../mod_fix.h"

int get_stored_info(struct sip_msg* msg, subs_t* subs, int* error_ret,
		str* reply_str);
int get_database_info(struct sip_msg* msg, subs_t* subs, int* error_ret,
		str* reply_str);
int get_db_subs_auth(subs_t* subs, int* found);
int insert_db_subs_auth(subs_t* subs);

static str su_200_rpl  = str_init("OK");
static str pu_481_rpl  = str_init("Subscription does not exist");
static str pu_400_rpl  = str_init("Bad request");
static str pu_500_rpl  = str_init("Server Internal Error");
static str pu_489_rpl  = str_init("Bad Event");
static str pu_423_rpl  = str_init("Interval Too Brief");

int send_2XX_reply(struct sip_msg * msg, int reply_code, int lexpire,
		str* local_contact)
{
	str hdr_append = {0, 0};
	str tmp;
	char *t = NULL;
	
	tmp.s = int2str((unsigned long)lexpire, &tmp.len);
	hdr_append.len = 9 + tmp.len + CRLF_LEN
		+ 10 + local_contact->len + 16 + CRLF_LEN;
	hdr_append.s = (char *)pkg_malloc(sizeof(char)*(hdr_append.len+1));
	if(hdr_append.s == NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	strncpy(hdr_append.s, "Expires: ", 9);
	strncpy(hdr_append.s+9, tmp.s, tmp.len);
	tmp.s = hdr_append.s+9+tmp.len;
	strncpy(tmp.s, CRLF, CRLF_LEN);
	tmp.s += CRLF_LEN;
	strncpy(tmp.s, "Contact: <", 10);
	tmp.s += 10;
	strncpy(tmp.s, local_contact->s, local_contact->len);
	tmp.s[local_contact->len] = '\0';
	t = strstr(tmp.s, ";transport=");
	tmp.s += local_contact->len;
	if(t==NULL)
	{
		switch (msg->rcv.proto)
		{
			case PROTO_TCP:
				strncpy(tmp.s, ";transport=tcp", 14);
				tmp.s += 14;
				hdr_append.len -= 1;
			break;
			case PROTO_TLS:
				strncpy(tmp.s, ";transport=tls", 14);
				tmp.s += 14;
				hdr_append.len -= 1;
			break;
			case PROTO_SCTP:
				strncpy(tmp.s, ";transport=sctp", 15);
				tmp.s += 15;
			break;
			case PROTO_WS:
			case PROTO_WSS:
				strncpy(tmp.s, ";transport=ws", 13);
				tmp.s += 13;
				hdr_append.len -= 2;
			break;
			default:
				hdr_append.len -= 15;
		}
	} else {
		hdr_append.len -= 15;
	}
	*tmp.s = '>';
	strncpy(tmp.s+1, CRLF, CRLF_LEN);

	hdr_append.s[hdr_append.len]= '\0';
	
	if (add_lump_rpl( msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR)==0 )
	{
		LM_ERR("unable to add lump_rl\n");
		goto error;
	}

	if(slb.freply(msg, reply_code, &su_200_rpl) < 0)
	{
		LM_ERR("sending reply\n");
		goto error;
	}
	
	pkg_free(hdr_append.s);
	return 0;

error:

	if(hdr_append.s!=NULL)
		pkg_free(hdr_append.s);
	return -1;
}


int delete_db_subs(str* to_tag, str* from_tag, str* callid)
{
	db_key_t query_cols[3];
	db_val_t query_vals[3];
	int n_query_cols= 0;

	query_cols[n_query_cols] = &str_callid_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *callid;
	n_query_cols++;

	query_cols[n_query_cols] = &str_to_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *to_tag;
	n_query_cols++;

	query_cols[n_query_cols] = &str_from_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *from_tag;
	n_query_cols++;

	if (pa_dbf.use_table(pa_db, &active_watchers_table) < 0) 
	{
		LM_ERR("in use table sql operation\n");
		return -1;
	}

	if(pa_dbf.delete(pa_db, query_cols, 0, query_vals,
				n_query_cols)< 0 )
	{
		LM_ERR("sql delete failed\n");
		return -1;
	}

	return 0;
}

int insert_subs_db(subs_t* s, int type)
{
	db_key_t query_cols[24];
	db_val_t query_vals[24];
	int n_query_cols = 0;
	int pres_uri_col, to_user_col, to_domain_col, from_user_col, from_domain_col,
		callid_col, totag_col, fromtag_col, event_col,status_col, event_id_col, 
		local_cseq_col, remote_cseq_col, expires_col, record_route_col, 
		contact_col, local_contact_col, version_col,socket_info_col,reason_col,
		watcher_user_col, watcher_domain_col, updated_col, updated_winfo_col;
		
	if(pa_dbf.use_table(pa_db, &active_watchers_table)< 0)
	{
		LM_ERR("sql use table failed\n");
		return -1;
	}
	
	query_cols[pres_uri_col= n_query_cols] =&str_presentity_uri_col;
	query_vals[pres_uri_col].type = DB1_STR;
	query_vals[pres_uri_col].nul = 0;
	n_query_cols++;
	
	query_cols[callid_col= n_query_cols] =&str_callid_col;
	query_vals[callid_col].type = DB1_STR;
	query_vals[callid_col].nul = 0;
	n_query_cols++;

	query_cols[totag_col= n_query_cols] =&str_to_tag_col;
	query_vals[totag_col].type = DB1_STR;
	query_vals[totag_col].nul = 0;
	n_query_cols++;

	query_cols[fromtag_col= n_query_cols] =&str_from_tag_col;
	query_vals[fromtag_col].type = DB1_STR;
	query_vals[fromtag_col].nul = 0;
	n_query_cols++;

	query_cols[to_user_col= n_query_cols] =&str_to_user_col;
	query_vals[to_user_col].type = DB1_STR;
	query_vals[to_user_col].nul = 0;
	n_query_cols++;

	query_cols[to_domain_col= n_query_cols] =&str_to_domain_col;
	query_vals[to_domain_col].type = DB1_STR;
	query_vals[to_domain_col].nul = 0;
	n_query_cols++;
	
	query_cols[from_user_col= n_query_cols] =&str_from_user_col;
	query_vals[from_user_col].type = DB1_STR;
	query_vals[from_user_col].nul = 0;
	n_query_cols++;

	query_cols[from_domain_col= n_query_cols] =&str_from_domain_col;
	query_vals[from_domain_col].type = DB1_STR;
	query_vals[from_domain_col].nul = 0;
	n_query_cols++;

	query_cols[watcher_user_col= n_query_cols] =&str_watcher_username_col;
	query_vals[watcher_user_col].type = DB1_STR;
	query_vals[watcher_user_col].nul = 0;
	n_query_cols++;

	query_cols[watcher_domain_col= n_query_cols] =&str_watcher_domain_col;
	query_vals[watcher_domain_col].type = DB1_STR;
	query_vals[watcher_domain_col].nul = 0;
	n_query_cols++;

	query_cols[event_col= n_query_cols] =&str_event_col;
	query_vals[event_col].type = DB1_STR;
	query_vals[event_col].nul = 0;
	n_query_cols++;	

	query_cols[event_id_col= n_query_cols] =&str_event_id_col;
	query_vals[event_id_col].type = DB1_STR;
	query_vals[event_id_col].nul = 0;
	n_query_cols++;

	query_cols[local_cseq_col= n_query_cols]=&str_local_cseq_col;
	query_vals[local_cseq_col].type = DB1_INT;
	query_vals[local_cseq_col].nul = 0;
	n_query_cols++;

	query_cols[remote_cseq_col= n_query_cols]=&str_remote_cseq_col;
	query_vals[remote_cseq_col].type = DB1_INT;
	query_vals[remote_cseq_col].nul = 0;
	n_query_cols++;

	query_cols[expires_col= n_query_cols] =&str_expires_col;
	query_vals[expires_col].type = DB1_INT;
	query_vals[expires_col].nul = 0;
	n_query_cols++;

	query_cols[status_col= n_query_cols] =&str_status_col;
	query_vals[status_col].type = DB1_INT;
	query_vals[status_col].nul = 0;
	n_query_cols++;

	query_cols[reason_col= n_query_cols] =&str_reason_col;
	query_vals[reason_col].type = DB1_STR;
	query_vals[reason_col].nul = 0;
	n_query_cols++;

	query_cols[record_route_col= n_query_cols] =&str_record_route_col;
	query_vals[record_route_col].type = DB1_STR;
	query_vals[record_route_col].nul = 0;
	n_query_cols++;
	
	query_cols[contact_col= n_query_cols] =&str_contact_col;
	query_vals[contact_col].type = DB1_STR;
	query_vals[contact_col].nul = 0;
	n_query_cols++;

	query_cols[local_contact_col= n_query_cols] =&str_local_contact_col;
	query_vals[local_contact_col].type = DB1_STR;
	query_vals[local_contact_col].nul = 0;
	n_query_cols++;

	query_cols[socket_info_col= n_query_cols] =&str_socket_info_col;
	query_vals[socket_info_col].type = DB1_STR;
	query_vals[socket_info_col].nul = 0;
	n_query_cols++;

	query_cols[version_col= n_query_cols]=&str_version_col;
	query_vals[version_col].type = DB1_INT;
	query_vals[version_col].nul = 0;
	n_query_cols++;

	query_cols[updated_col= n_query_cols]=&str_updated_col;
	query_vals[updated_col].type = DB1_INT;
	query_vals[updated_col].nul = 0;
	n_query_cols++;

	query_cols[updated_winfo_col= n_query_cols]=&str_updated_winfo_col;
	query_vals[updated_winfo_col].type = DB1_INT;
	query_vals[updated_winfo_col].nul = 0;
	n_query_cols++;

	query_vals[pres_uri_col].val.str_val= s->pres_uri;
	query_vals[callid_col].val.str_val= s->callid;
	query_vals[totag_col].val.str_val= s->to_tag;
	query_vals[fromtag_col].val.str_val= s->from_tag;
	query_vals[to_user_col].val.str_val = s->to_user;
	query_vals[to_domain_col].val.str_val = s->to_domain;
	query_vals[from_user_col].val.str_val = s->from_user;
	query_vals[from_domain_col].val.str_val = s->from_domain;
	query_vals[watcher_user_col].val.str_val = s->watcher_user;
	query_vals[watcher_domain_col].val.str_val = s->watcher_domain;
	query_vals[event_col].val.str_val = s->event->name;
	query_vals[event_id_col].val.str_val = s->event_id;
	query_vals[local_cseq_col].val.int_val= s->local_cseq;
	query_vals[remote_cseq_col].val.int_val= s->remote_cseq;
	query_vals[expires_col].val.int_val = s->expires + (int)time(NULL);
	query_vals[record_route_col].val.str_val = s->record_route;
	query_vals[contact_col].val.str_val = s->contact;
	query_vals[local_contact_col].val.str_val = s->local_contact;
	query_vals[version_col].val.int_val= s->version;
	query_vals[status_col].val.int_val= s->status;
	query_vals[reason_col].val.str_val= s->reason;
	query_vals[socket_info_col].val.str_val= s->sockinfo_str;
	query_vals[updated_col].val.int_val = s->updated;
	query_vals[updated_winfo_col].val.int_val = s->updated_winfo;

	if (pa_dbf.use_table(pa_db, &active_watchers_table) < 0)
	{
		LM_ERR("in use table sql operation\n");	
		return -1;
	}

	LM_DBG("inserting subscription in active_watchers table\n");
	if(pa_dbf.insert(pa_db, query_cols, query_vals, n_query_cols) < 0)
	{
		LM_ERR("unsuccessful sql insert\n");
		return -1;
	}
	return 0;
}

int update_subs_db(subs_t* subs, int type)
{
	db_key_t query_cols[3], update_keys[8];
	db_val_t query_vals[3], update_vals[8];
	int n_update_cols= 0;
	int n_query_cols = 0;

	query_cols[n_query_cols] = &str_callid_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->callid;
	n_query_cols++;

	query_cols[n_query_cols] = &str_to_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->to_tag;
	n_query_cols++;

	query_cols[n_query_cols] = &str_from_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->from_tag;
	n_query_cols++;

	if(type & REMOTE_TYPE)
	{
		update_keys[n_update_cols] = &str_expires_col;
		update_vals[n_update_cols].type = DB1_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = subs->expires + (int)time(NULL);
		n_update_cols++;
	
		update_keys[n_update_cols] = &str_remote_cseq_col;
		update_vals[n_update_cols].type = DB1_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = subs->remote_cseq; 
		n_update_cols++;

		update_keys[n_update_cols] = &str_updated_col;
		update_vals[n_update_cols].type = DB1_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = subs->updated;
		n_update_cols++;

		update_keys[n_update_cols] = &str_updated_winfo_col;
		update_vals[n_update_cols].type = DB1_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = subs->updated_winfo;
		n_update_cols++;
	}
	if(type & LOCAL_TYPE)
	{
		update_keys[n_update_cols] = &str_local_cseq_col;
		update_vals[n_update_cols].type = DB1_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = subs->local_cseq;
		n_update_cols++;
	
		update_keys[n_update_cols] = &str_version_col;
		update_vals[n_update_cols].type = DB1_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = subs->version;
		n_update_cols++;
	}

	update_keys[n_update_cols] = &str_status_col;
	update_vals[n_update_cols].type = DB1_INT;
	update_vals[n_update_cols].nul = 0;
	update_vals[n_update_cols].val.int_val = subs->status;
	n_update_cols++;

	update_keys[n_update_cols] = &str_reason_col;
	update_vals[n_update_cols].type = DB1_STR;
	update_vals[n_update_cols].nul = 0;
	update_vals[n_update_cols].val.str_val = subs->reason;
	n_update_cols++;

	if (pa_dbf.use_table(pa_db, &active_watchers_table) < 0)
	{
		LM_ERR("in use table sql operation\n");	
		return -1;
	}
		
	if( pa_dbf.update( pa_db,query_cols, 0, query_vals,
				update_keys, update_vals, n_query_cols,n_update_cols)<0) 
	{
		LM_ERR("updating presence information\n");
		return -1;
	}
	return 0;
}

void delete_subs(str* pres_uri, str* ev_name, str* to_tag,
		str* from_tag, str* callid)
{
	subs_t subs;

	memset(&subs, 0, sizeof(subs_t));
	subs.pres_uri = *pres_uri;
	subs.from_tag = *from_tag;
	subs.to_tag = *to_tag;
	subs.callid = *callid;

	/* delete record from hash table also if not in dbonly mode */
	if(subs_dbmode != DB_ONLY)
	{
		unsigned int hash_code= core_case_hash(pres_uri, ev_name, shtable_size);
		if(delete_shtable(subs_htable, hash_code, &subs) < 0) {
			LM_ERR("Failed to delete subscription from memory"
					" [slot: %u ev: %.*s pu: %.*s ci: %.*s ft: %.*s tt: %.*s]\n",
					hash_code, pres_uri->len, pres_uri->s,
					ev_name->len, ev_name->s,
					callid->len, callid->s, from_tag->len, from_tag->s,
					to_tag->len, to_tag->s);
		}
	}

	if(subs_dbmode != NO_DB && delete_db_subs(to_tag, from_tag, callid)< 0)
		LM_ERR("Failed to delete subscription from database\n");
}

int update_subscription_notifier(struct sip_msg* msg, subs_t* subs,
		int to_tag_gen, int* sent_reply)
{
	int num_peers = 0;

	*sent_reply= 0;

	/* Set the notifier/update fields for the subscription */
	subs->updated = core_case_hash(&subs->callid, &subs->from_tag, 0) %
				(pres_waitn_time * pres_notifier_poll_rate
					* pres_notifier_processes);
	if (subs->event->type & WINFO_TYPE)
		subs->updated_winfo = UPDATED_TYPE;
	else if (subs->event->wipeer)
	{
		if ((num_peers = set_wipeer_subs_updated(&subs->pres_uri,
						subs->event->wipeer,
						subs->expires == 0)) < 0)
		{
			LM_ERR("failed to update database record(s)\n");
			goto error;
		}

		if (num_peers > 0)
			subs->updated_winfo = UPDATED_TYPE;
	}
	if (subs->expires == 0)
	{
		subs->status = TERMINATED_STATUS;
		subs->reason.s = "timeout";
		subs->reason.len = 7;
	}

	printf_subs(subs);

	if (to_tag_gen == 0)
	{
		if (update_subs_db(subs, REMOTE_TYPE) < 0)
		{
			LM_ERR("updating subscription in database table\n");
			goto error;
		}
	}
	else
	{
		subs->version = 1;
		if (insert_subs_db(subs, REMOTE_TYPE) < 0)
		{
			LM_ERR("failed to insert new record in database\n");
			goto error;
		}
	}

	if(send_2XX_reply(msg, subs->event->type & PUBL_TYPE ? 202 : 200,
				subs->expires, &subs->local_contact) < 0)
	{
		LM_ERR("sending %d response\n",
			subs->event->type & PUBL_TYPE ? 202 : 200);
		goto error;
	}
	*sent_reply= 1;

	return 1;

error:
	return -1;
}

int update_subscription(struct sip_msg* msg, subs_t* subs, int to_tag_gen,
		int* sent_reply)
{
	unsigned int hash_code;

	LM_DBG("update subscription\n");
	printf_subs(subs);

	*sent_reply= 0;

	if( to_tag_gen ==0) /*if a SUBSCRIBE within a dialog */
	{
		if(subs->expires == 0)
		{
			LM_DBG("expires =0 -> deleting record\n");

			delete_subs(&subs->pres_uri, &subs->event->name, &subs->to_tag,
					&subs->from_tag, &subs->callid);

			if(subs->event->type & PUBL_TYPE)
			{
				if( send_2XX_reply(msg, 202, subs->expires,
							&subs->local_contact) <0)
				{
					LM_ERR("sending 202 OK\n");
					goto error;
				}
				*sent_reply= 1;
				if(subs->event->wipeer)
				{
					if(query_db_notify(&subs->pres_uri,
								subs->event->wipeer, NULL)< 0)
					{
						LM_ERR("Could not send notify for winfo\n");
						goto error;
					}
				}

			}
			else /* if unsubscribe for winfo */
			{
				if( send_2XX_reply(msg, 200, subs->expires,
							&subs->local_contact) <0)
				{
					LM_ERR("sending 200 OK reply\n");
					goto error;
				}
				*sent_reply= 1;
			}
		
			if(notify(subs, NULL, NULL, 0)< 0)
			{
				LM_ERR("Could not send notify\n");
				goto error;
			}
			return 1;
		}
		/* if subscriptions are stored in memory, update them */
		if(subs_dbmode != DB_ONLY)
		{
			hash_code= core_case_hash(&subs->pres_uri, &subs->event->name, shtable_size);
			if(update_shtable(subs_htable, hash_code, subs, REMOTE_TYPE)< 0)
			{
				LM_ERR("failed to update subscription in memory\n");
				goto error;
			}
		}
		/* for modes that update the subscription synchronously in database, write in db */
		if(subs_dbmode == DB_ONLY ||  subs_dbmode== WRITE_THROUGH)
		{
			/* update in database table */
			if(update_subs_db(subs, REMOTE_TYPE|LOCAL_TYPE)< 0)
			{
				LM_ERR("updating subscription in database table\n");
				goto error;
			}
		}
	}
	else
	{
		LM_DBG("subscription not in dialog\n");
		if(subs->expires!= 0)
		{
			if(subs_dbmode != DB_ONLY)
			{
				LM_DBG("inserting in shtable\n");
				subs->db_flag = (subs_dbmode==WRITE_THROUGH)?WTHROUGHDB_FLAG:INSERTDB_FLAG;
				hash_code= core_case_hash(&subs->pres_uri, &subs->event->name, shtable_size);
				subs->version = 0;
				if(insert_shtable(subs_htable,hash_code,subs)< 0)
				{
					LM_ERR("failed to insert new record in subs htable\n");
					goto error;
				}
			}

			if(subs_dbmode == DB_ONLY || subs_dbmode == WRITE_THROUGH)
			{
				subs->version = 1;
				if(insert_subs_db(subs, REMOTE_TYPE) < 0)
				{
					LM_ERR("failed to insert new record in database\n");
					goto error;
				}
			}
			/* TODO if req_auth, the subscription was in the watcher table first, we must delete it */
		}
		/*otherwise there is a subscription outside a dialog with expires= 0 
		 * no update in database, but should try to send Notify */
		else
		{
			LM_DBG("subscription request with expiry=0 not in dialog\n");
		}
	}

	/* reply_and_notify  */

	if(subs->event->type & PUBL_TYPE)
	{	
		if(send_2XX_reply(msg, 202, subs->expires,
					&subs->local_contact)<0)
		{
			LM_ERR("sending 202 OK reply\n");
			goto error;
		}
		*sent_reply= 1;

		if(subs->expires!= 0 && subs->event->wipeer)
		{
			LM_DBG("send Notify with winfo\n");
			if(query_db_notify(&subs->pres_uri, subs->event->wipeer, subs)< 0)
			{
				LM_ERR("Could not send notify winfo\n");
				goto error;
			}
			if(subs->send_on_cback== 0)
			{
				if(notify(subs, NULL, NULL, 0)< 0)
				{
					LM_ERR("Could not send notify\n");
					goto error;
				}
			}
		}
		else
		{
			if(send_fast_notify && (notify(subs, NULL, NULL, 0)< 0))
			{
				LM_ERR("Could not send notify\n");
				goto error;
			}
		}	
			
	}
	else 
	{
		if( send_2XX_reply(msg, 200, subs->expires,
					&subs->local_contact)<0)
		{
			LM_ERR("sending 200 OK reply\n");
			goto error;
		}		
		*sent_reply= 1;
		
		if(send_fast_notify && (notify(subs, NULL, NULL, 0 )< 0))
		{
			LM_ERR("sending notify request\n");
			goto error;
		}
	}
	return 0;
	
error:

	LM_ERR("occured\n");
	return -1;

}

void msg_watchers_clean(unsigned int ticks,void *param)
{
	db_key_t db_keys[2];
	db_val_t db_vals[2];
	db_op_t  db_ops[2] ;

	LM_DBG("cleaning pending subscriptions\n");

	db_keys[0] = &str_inserted_time_col;
	db_ops[0] = OP_LT;
	db_vals[0].type = DB1_INT;
	db_vals[0].nul = 0;
	db_vals[0].val.int_val = (int)time(NULL)- 24*3600 ;

	db_keys[1] = &str_status_col;
	db_ops [1] = OP_EQ;
	db_vals[1].type = DB1_INT;
	db_vals[1].nul = 0;
	db_vals[1].val.int_val = PENDING_STATUS;

	if (pa_dbf.use_table(pa_db, &watchers_table) < 0) 
	{
		LM_ERR("unsuccessful use table sql operation\n");
		return ;
	}

	if (pa_dbf.delete(pa_db, db_keys, db_ops, db_vals, 2) < 0)
		LM_ERR("cleaning pending subscriptions\n");
}

static char* _pres_subs_last_presentity = NULL;

int pv_parse_subscription_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 3:
			if(strncmp(in->s, "uri", 3)==0) {
				sp->pvp.pvn.u.isname.name.n = 1;
			} else {
				goto error;
			};
			break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV subscription name %.*s\n", in->len, in->s);
	return -1;
}

int pv_get_subscription(struct sip_msg *msg, pv_param_t *param,	pv_value_t *res)
{
	if(param->pvn.u.isname.name.n==1) /* presentity */
		return (_pres_subs_last_presentity == NULL) ? pv_get_null(msg, param, res)
						: pv_get_strzval(msg, param, res, _pres_subs_last_presentity);

	LM_ERR("unknown specifier\n");
	return pv_get_null(msg, param, res);

}

int handle_subscribe0(struct sip_msg* msg)
{
	struct to_body *pfrom;

	if (parse_from_uri(msg) < 0)
	{
		LM_ERR("failed to find From header\n");
		if (slb.freply(msg, 400, &pu_400_rpl) < 0)
		{
			LM_ERR("while sending 400 reply\n");
			return -1;
		}
		return 0;
	}
	pfrom = (struct to_body *) msg->from->parsed;

	return handle_subscribe(msg, pfrom->parsed_uri.user,
				pfrom->parsed_uri.host);
}

int w_handle_subscribe(struct sip_msg* msg, char* watcher_uri)
{
	str wuri;
	struct sip_uri parsed_wuri;

	if (fixup_get_svalue(msg, (gparam_p)watcher_uri, &wuri) != 0)
	{
		LM_ERR("invalid uri parameter\n");
		return -1;
	}

	if (parse_uri(wuri.s, wuri.len, &parsed_wuri) < 0)
	{
		LM_ERR("failed to parse watcher URI\n");
		return -1;
	}

	return handle_subscribe(msg, parsed_wuri.user, parsed_wuri.host);
}

int handle_subscribe(struct sip_msg* msg, str watcher_user, str watcher_domain)
{
	int  to_tag_gen = 0;
	subs_t subs;
	pres_ev_t* event= NULL;
	event_t* parsed_event= NULL;
	param_t* ev_param= NULL;
	int found = 0;
	str reason= {0, 0};
	struct sip_uri uri;
	int reply_code;
	str reply_str;
	int sent_reply= 0;

	if(_pres_subs_last_presentity) {
		pkg_free(_pres_subs_last_presentity);
		_pres_subs_last_presentity = NULL;
	}

	/* ??? rename to avoid collisions with other symbols */
	counter++;

	memset(&subs, 0, sizeof(subs_t));
	
	reply_code= 500;
	reply_str= pu_500_rpl;

	if(parse_headers(msg,HDR_EOH_F, 0) == -1)
	{
		LM_ERR("parsing headers\n");
		reply_code= 400;
		reply_str= pu_400_rpl;
		goto error;
	}
	
	/* inspecting the Event header field */
	if(msg->event && msg->event->body.len > 0)
	{
		if (!msg->event->parsed && (parse_event(msg->event) < 0))
		{
			reply_code= 400;
			reply_str= pu_400_rpl;
			goto error;
		}
	}
	else
		goto bad_event;

	/* search event in the list */
	parsed_event= (event_t*)msg->event->parsed;
	event= search_event(parsed_event);
	if(event== NULL)
	{
		goto bad_event;
	}
	subs.event= event;
	
	/* extract the id if any*/
	ev_param= parsed_event->params.list;
	while(ev_param)
	{
		if(ev_param->name.len== 2 && strncmp(ev_param->name.s, "id", 2)== 0)
		{
			subs.event_id= ev_param->body;
			break;
		}
		ev_param= ev_param->next;
	}
	
	if(extract_sdialog_info_ex(&subs, msg, min_expires, max_expires, &to_tag_gen,
				server_address, watcher_user, watcher_domain, &reply_code, &reply_str)< 0)
	{
            goto error;
	}

	if (pres_notifier_processes > 0 && pa_dbf.start_transaction)
	{
		if (pa_dbf.use_table(pa_db, &active_watchers_table) < 0) 	
		{
			LM_ERR("unsuccessful use_table sql operation\n");
			goto error;
		}
		if (pa_dbf.start_transaction(pa_db, db_table_lock) < 0)
		{
			LM_ERR("in start_transaction\n");
			goto error;
		}
	}

	/* getting presentity uri from Request-URI if initial subscribe - or else from database*/
	if(to_tag_gen)
	{
		if (!EVENT_DIALOG_SLA(parsed_event))
		{
			if( parse_sip_msg_uri(msg)< 0)
			{
				LM_ERR("failed to parse R-URI\n");
				return -1;
			}
			if(uandd_to_uri(msg->parsed_uri.user, msg->parsed_uri.host,
					&subs.pres_uri)< 0)
			{
				LM_ERR("failed to construct uri from user and domain\n");
				goto error;
			}
		}
	}
	else
	{
		if(get_stored_info(msg, &subs, &reply_code, &reply_str )< 0)
		{
			LM_INFO("getting stored info\n");
			goto error;
		}
		reason= subs.reason;
	}

	if(subs.pres_uri.len > 0 && subs.pres_uri.s) {
		_pres_subs_last_presentity =
				(char*)pkg_malloc((subs.pres_uri.len+1) * sizeof(char));
		strncpy(_pres_subs_last_presentity, subs.pres_uri.s, subs.pres_uri.len);
		_pres_subs_last_presentity[subs.pres_uri.len] = '\0';
	}
		
	/* mark that the received event is a SUBSCRIBE message */
	subs.recv_event = PRES_SUBSCRIBE_RECV;

	/* call event specific subscription handling */
	if(event->evs_subs_handl)
	{
		if(event->evs_subs_handl(msg)< 0)
		{
			LM_ERR("in event specific subscription handling\n");
			goto error;
		}
	}


	/* if dialog initiation Subscribe - get subscription state */
	if(to_tag_gen)
	{
		subs.updated = NO_UPDATE_TYPE;
		subs.updated_winfo = NO_UPDATE_TYPE;

		if(!event->req_auth) 
			subs.status = ACTIVE_STATUS;
		else
		{
			/* query in watchers_table */
			if(get_db_subs_auth(&subs, &found)< 0)
			{
				LM_ERR("getting subscription status from watchers table\n");
				goto error;
			}
			if(found== 0)
			{
				/*default 'pending' status */
				subs.status= PENDING_STATUS;
				subs.reason.s= NULL;
				subs.reason.len= 0;
				/* here a query to xcap server must be done -> new process maybe */
			
				if(parse_uri(subs.pres_uri.s, subs.pres_uri.len, &uri)< 0)
				{
					LM_ERR("parsing uri\n");
					goto error;

				}
				if(subs.event->get_rules_doc(&uri.user, &uri.host, &subs.auth_rules_doc)< 0)
				{
					LM_ERR("getting rules doc\n");
					goto error;
				}
				
				if(subs.event->get_auth_status(&subs)< 0)
				{
					LM_ERR("in event specific function is_watcher_allowed\n");
					goto error;
				}
				if(get_status_str(subs.status) == NULL)
				{
					LM_ERR("wrong status= %d\n", subs.status);
					goto error;
				}

				if(insert_db_subs_auth(&subs)< 0)
				{
					LM_ERR("while inserting record in watchers table\n");
					goto error;
				}
			}
			else
			{
				reason= subs.reason;
			}
		}
	}

	/* check if correct status */
	if(get_status_str(subs.status)== NULL)
	{
		LM_ERR("wrong status\n");
		goto error;
	}
	LM_DBG("subscription status= %s - %s\n", get_status_str(subs.status), 
            (found==0)?"inserted":"found in watcher table");
	
	if (pres_notifier_processes > 0)
	{
		if (update_subscription_notifier(msg, &subs, to_tag_gen,
							&sent_reply) < 0)
		{
			LM_ERR("in update_subscription_notifier\n");
			goto error;
		}
	}
	else if (update_subscription(msg, &subs, to_tag_gen, &sent_reply) <0)
	{	
		LM_ERR("in update_subscription\n");
		goto error;
	}
	if (pres_notifier_processes > 0 && pa_dbf.end_transaction)
	{
		if (pa_dbf.end_transaction(pa_db) < 0)
		{
			LM_ERR("in end_transaction\n");
			goto error;
		}
	}

	if(subs.auth_rules_doc)
	{
		pkg_free(subs.auth_rules_doc->s);
		pkg_free(subs.auth_rules_doc);
	}
	if(reason.s)
		pkg_free(reason.s);
	
	if(subs.pres_uri.s)
		pkg_free(subs.pres_uri.s);
	
	if((!server_address.s) || (server_address.len== 0))
	{
		pkg_free(subs.local_contact.s);
	}
	if(subs.record_route.s)
		pkg_free(subs.record_route.s);

	return 1;

bad_event:

	if (parsed_event && parsed_event->name.s)
	    LM_NOTICE("Unsupported presence event %.*s\n",
		      parsed_event->name.len,parsed_event->name.s);
	else
	    LM_ERR("Missing event header field value\n");
	
	reply_code= BAD_EVENT_CODE;
	reply_str= pu_489_rpl;

error:
	
	if(sent_reply== 0)
	{
		if(send_error_reply(msg, reply_code, reply_str)< 0)
		{
			LM_ERR("failed to send reply on error case\n");
		}
	}

	if(subs.pres_uri.s)	
		pkg_free(subs.pres_uri.s);
	
	if(subs.auth_rules_doc)
	{
		if(subs.auth_rules_doc->s)
			pkg_free(subs.auth_rules_doc->s);
		pkg_free(subs.auth_rules_doc);
	}
	if(reason.s)
		pkg_free(reason.s);

	if(((!server_address.s) ||(server_address.len== 0))&& subs.local_contact.s)
	{
		pkg_free(subs.local_contact.s);
	}
	if(subs.record_route.s)
		pkg_free(subs.record_route.s);

	if (pres_notifier_processes > 0 && pa_dbf.abort_transaction)
	{
		if (pa_dbf.abort_transaction(pa_db) < 0)
			LM_ERR("in abort_transaction\n");
	}

	return -1;

}


int extract_sdialog_info_ex(subs_t* subs,struct sip_msg* msg, int miexp,
		int mexp, int* to_tag_gen, str scontact,
		str watcher_user, str watcher_domain,
        int* reply_code, str* reply_str)
{
	str rec_route= {0, 0};
	int rt  = 0;
	contact_body_t *b;
	struct to_body *pto, TO = {0}, *pfrom = NULL;
	int lexpire;
	str rtag_value;
	struct sip_uri uri;

	/* examine the expire header field */
	if(msg->expires && msg->expires->body.len > 0)
	{
		if (!msg->expires->parsed && (parse_expires(msg->expires) < 0))
		{
			LM_ERR("cannot parse Expires header\n");
			goto error;
		}
		lexpire = ((exp_body_t*)msg->expires->parsed)->val;
		LM_DBG("'Expires' header found, value= %d\n", lexpire);

	}
	else 
	{
		LM_DBG("'expires' not found; default=%d\n",subs->event->default_expires);
		lexpire = subs->event->default_expires;
	}
	if(lexpire > mexp)
		lexpire = mexp;

    if (lexpire && miexp && lexpire < miexp) {
        if(min_expires_action == 1) {
            LM_DBG("subscription expiration invalid , requested=%d, minimum=%d, returning error \"423 Interval Too brief\"\n", lexpire, miexp);
            *reply_code = INTERVAL_TOO_BRIEF;
            *reply_str = pu_423_rpl;
            goto error;
        } else {
            LM_DBG("subscription expiration set to minimum (%d) for requested (%d)\n", lexpire, miexp);
            lexpire = miexp;
        }
    }
    
	subs->expires = lexpire;

	if( msg->to==NULL || msg->to->body.s==NULL)
	{
		LM_ERR("cannot parse TO header\n");
		goto error;
	}
	/* examine the to header */
	if(msg->to->parsed != NULL)
	{
		pto = (struct to_body*)msg->to->parsed;
		LM_DBG("'To' header ALREADY PARSED: <%.*s>\n",pto->uri.len,pto->uri.s);
	}
	else
	{
		parse_to(msg->to->body.s,msg->to->body.s + msg->to->body.len + 1, &TO);
		if( TO.uri.len <= 0 )
		{
			LM_DBG("'To' header NOT parsed\n");
			goto error;
		}
		pto = &TO;
	}

	if( pto->parsed_uri.user.s && pto->parsed_uri.host.s &&
		pto->parsed_uri.user.len && pto->parsed_uri.host.len)
	{
		subs->to_user = pto->parsed_uri.user;
		subs->to_domain = pto->parsed_uri.host;
	}
	else
	{
		if(parse_uri(pto->uri.s, pto->uri.len, &uri)< 0)
		{
			LM_ERR("while parsing uri\n");
			goto error;
		}
		subs->to_user = uri.user;
		subs->to_domain = uri.host;
	}

	/* examine the from header */
	if (!msg->from || !msg->from->body.s)
	{
		LM_DBG("cannot find 'from' header!\n");
		goto error;
	}
	if (msg->from->parsed == NULL)
	{
		LM_DBG("'From' header not parsed\n");
		/* parsing from header */
		if ( parse_from_header( msg )<0 ) 
		{
			LM_DBG("cannot parse From header\n");
			goto error;
		}
	}
	pfrom = (struct to_body*)msg->from->parsed;
	
	if( pfrom->parsed_uri.user.s && pfrom->parsed_uri.host.s && 
		pfrom->parsed_uri.user.len && pfrom->parsed_uri.host.len)
	{
		subs->from_user = pfrom->parsed_uri.user;
		subs->from_domain = pfrom->parsed_uri.host;
	}
	else
	{
		if(parse_uri(pfrom->uri.s, pfrom->uri.len, &uri)< 0)
		{
			LM_ERR("while parsing uri\n");
			goto error;
		}
		subs->from_user = uri.user;
		subs->from_domain = uri.host;
	}

	subs->watcher_user = watcher_user;
	subs->watcher_domain = watcher_domain;

	/* get to_tag if the message does not have a to_tag*/
	if (pto->tag_value.s==NULL || pto->tag_value.len==0 )
	{  
		LM_DBG("generating to_tag\n");
		*to_tag_gen = 1;
		rtag_value.len = 0;
		if(slb.get_reply_totag(msg, &rtag_value)<0 || rtag_value.len <= 0)
		{
			LM_ERR("while creating to_tag\n");
			goto error;
		}
	}
	else
	{
		*to_tag_gen = 0;
		rtag_value=pto->tag_value;
	}
	subs->to_tag = rtag_value;

	if( msg->callid==NULL || msg->callid->body.s==NULL)
	{
		LM_ERR("cannot parse callid header\n");
		goto error;
	}
	subs->callid = msg->callid->body;

	if( msg->cseq==NULL || msg->cseq->body.s==NULL)
	{
		LM_ERR("cannot parse cseq header\n");
		goto error;
	}
	if (str2int( &(get_cseq(msg)->number), &subs->remote_cseq)!=0 )
	{
		LM_ERR("cannot parse cseq number\n");
		goto error;
	}
	if( msg->contact==NULL || msg->contact->body.s==NULL)
	{
		LM_ERR("cannot parse contact header\n");
		goto error;
	}
	if( parse_contact(msg->contact) <0 )
	{
		LM_ERR(" cannot parse contact"
				" header\n");
		goto error;
	}
	b= (contact_body_t* )msg->contact->parsed;

	if(b == NULL)
	{
		LM_ERR("cannot parse contact header\n");
		goto error;
	}
	if(b->star || b->contacts==NULL)
	{
		LM_ERR("Wrong contact header\n");
		goto error;
	}

	subs->contact = b->contacts->uri;
	
	LM_DBG("subs->contact= %.*s - len = %d\n",subs->contact.len,
			subs->contact.s, subs->contact.len);	

	if (EVENT_DIALOG_SLA(subs->event->evp))
	{
		/* user_contact@from_domain */
		if(parse_uri(subs->contact.s, subs->contact.len, &uri)< 0)
		{
			LM_ERR("failed to parse contact uri\n");
			goto error;
		}
		if(uandd_to_uri(uri.user, subs->from_domain, &subs->pres_uri)< 0)
		{
			LM_ERR("failed to construct uri\n");
			goto error;
		}
		LM_DBG("&&&&&&&&&&&&&&& dialog pres_uri= %.*s\n",
				subs->pres_uri.len, subs->pres_uri.s);
	}

	/*process record route and add it to a string*/
	if(*to_tag_gen && msg->record_route!=NULL)
	{
		rt = print_rr_body(msg->record_route, &rec_route, 0, 0);
		if(rt != 0)
		{
			LM_ERR("processing the record route [%d]\n", rt);	
			rec_route.s=NULL;
			rec_route.len=0;
		//	goto error;
		}
	}
	subs->record_route = rec_route;
			
	subs->sockinfo_str= msg->rcv.bind_address->sock_str;

	if( pfrom->tag_value.s ==NULL || pfrom->tag_value.len == 0)
	{
		LM_ERR("no from tag value present\n");
		goto error;
	}
	subs->from_tag = pfrom->tag_value;

	subs->version = 1;

	if((!scontact.s) || (scontact.len== 0))
	{
		if(ps_fill_local_contact(msg, &subs->local_contact)<0)
		{
			LM_ERR("cannot get local contact address\n");
			goto error;
		}
	}
	else
		subs->local_contact= scontact;

	free_to_params(&TO);
	return 0;
	
error:
	free_to_params(&TO);
	return -1;
}

int extract_sdialog_info(subs_t* subs,struct sip_msg* msg, int mexp,
                         int* to_tag_gen, str scontact,
                         str watcher_user, str watcher_domain)
{
    int reply_code = 500;
    str reply_str = pu_500_rpl;
    return extract_sdialog_info_ex(subs, msg, min_expires, mexp, to_tag_gen,
        scontact, watcher_user, watcher_domain, &reply_code, &reply_str);
}

int get_stored_info(struct sip_msg* msg, subs_t* subs, int* reply_code,
		str* reply_str)
{
	str pres_uri= {0, 0}, reason={0, 0};
	subs_t* s;
	int i;
	unsigned int hash_code;

	if(subs_dbmode == DB_ONLY)
		return get_database_info(msg, subs, reply_code, reply_str);

	/* first try to_user== pres_user and to_domain== pres_domain */
	if(subs->pres_uri.s == NULL)
	{
		uandd_to_uri(subs->to_user, subs->to_domain, &pres_uri);
		if(pres_uri.s== NULL)
		{
			LM_ERR("creating uri from user and domain\n");
			return -1;
		}
	}
	else
		pres_uri = subs->pres_uri;

	hash_code= core_case_hash(&pres_uri, &subs->event->name, shtable_size);
	lock_get(&subs_htable[hash_code].lock);
	s= search_shtable(subs_htable, subs->callid, subs->to_tag,
		subs->from_tag, hash_code);
	if(s)
		goto found_rec;

	lock_release(&subs_htable[hash_code].lock);

	if(subs->pres_uri.s)
		goto not_found;

	pkg_free(pres_uri.s);
	pres_uri.s= NULL;

	LM_DBG("record not found using R-URI search iteratively\n");
	/* take one row at a time */
	for(i= 0; i< shtable_size; i++)
	{
		lock_get(&subs_htable[i].lock);
		s= search_shtable(subs_htable, subs->callid,subs->to_tag,subs->from_tag, i);
		if (s)
		{
			pres_uri.s= (char*)pkg_malloc(s->pres_uri.len* sizeof(char));
			if(pres_uri.s== NULL)
			{
				lock_release(&subs_htable[i].lock);
				ERR_MEM(PKG_MEM_STR);
			}
			memcpy(pres_uri.s, s->pres_uri.s, s->pres_uri.len);
			pres_uri.len= s->pres_uri.len;

			hash_code = i;
			break;
		}
		lock_release(&subs_htable[i].lock);
	}

	if(!s)
		goto not_found;

found_rec:

	LM_DBG("Record found in hash_table\n");

	if(subs->pres_uri.s == NULL)
		subs->pres_uri= pres_uri;

	subs->version = s->version + 1;
	subs->status= s->status;
	if(s->reason.s && s->reason.len)
	{
		reason.s= (char*)pkg_malloc(s->reason.len* sizeof(char));
		if(reason.s== NULL)
		{
			lock_release(&subs_htable[hash_code].lock);
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(reason.s, s->reason.s, s->reason.len);
		reason.len= s->reason.len;
		subs->reason= reason;
	}
	if(s->record_route.s && s->record_route.len)
	{
		subs->record_route.s= (char*)pkg_malloc
			(s->record_route.len* sizeof(char));
		if(subs->record_route.s== NULL)
		{
			lock_release(&subs_htable[hash_code].lock);
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(subs->record_route.s, s->record_route.s, s->record_route.len);
		subs->record_route.len= s->record_route.len;
	}

	subs->local_cseq= s->local_cseq +1;

	if(subs->remote_cseq<= s->remote_cseq)
	{
		LM_ERR("wrong sequence number;received: %d - stored: %d\n",
				subs->remote_cseq, s->remote_cseq);
		
		*reply_code= 400;
		*reply_str= pu_400_rpl;

		lock_release(&subs_htable[hash_code].lock);
		goto error;
	}
	lock_release(&subs_htable[hash_code].lock);

	return 0;

not_found:

	LM_INFO("record not found in hash_table\n");
	*reply_code= 481;
	*reply_str= pu_481_rpl;

	return -1;

error:
	if(subs->reason.s)
		pkg_free(subs->reason.s);
	subs->reason.s= NULL;
	if(subs->record_route.s)
		pkg_free(subs->record_route.s);
	subs->record_route.s= NULL;
	return -1;
}

int get_database_info(struct sip_msg* msg, subs_t* subs, int* reply_code, str* reply_str)
{
	db_key_t query_cols[3];
	db_val_t query_vals[3];
	db_key_t result_cols[9];
	db1_res_t *result= NULL;
	db_row_t *row ;	
	db_val_t *row_vals ;
	int n_query_cols = 0;
	int n_result_cols = 0;
	int remote_cseq_col= 0, local_cseq_col= 0, status_col, reason_col;
	int record_route_col, version_col, pres_uri_col;
	int updated_col, updated_winfo_col;
	unsigned int remote_cseq;
	str pres_uri, record_route;
	str reason;
	db_query_f query_fn = pa_dbf.query;

	query_cols[n_query_cols] = &str_callid_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->callid;
	n_query_cols++;

	query_cols[n_query_cols] = &str_to_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->to_tag;
	n_query_cols++;

	query_cols[n_query_cols] = &str_from_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->from_tag;
	n_query_cols++;

	result_cols[pres_uri_col=n_result_cols++] = &str_presentity_uri_col;
	result_cols[remote_cseq_col=n_result_cols++] = &str_remote_cseq_col;
	result_cols[local_cseq_col=n_result_cols++] = &str_local_cseq_col;
	result_cols[status_col=n_result_cols++] = &str_status_col;
	result_cols[reason_col=n_result_cols++] = &str_reason_col;
	result_cols[record_route_col=n_result_cols++] = &str_record_route_col;
	result_cols[version_col=n_result_cols++] = &str_version_col;
	result_cols[updated_col=n_result_cols++] = &str_updated_col;
	result_cols[updated_winfo_col=n_result_cols++] = &str_updated_winfo_col;
	
	if (pa_dbf.use_table(pa_db, &active_watchers_table) < 0) 
	{
		LM_ERR("unsuccessful use_table sql operation\n");
		return -1;
	}
	
	if (pres_notifier_processes > 0 && pa_dbf.start_transaction)
		query_fn = pa_dbf.query_lock ? pa_dbf.query_lock : pa_dbf.query;

	if (query_fn (pa_db, query_cols, 0, query_vals,
		 result_cols, n_query_cols, n_result_cols, 0,  &result) < 0) 
	{
		LM_ERR("querying subscription dialog\n");
		if(result)
			pa_dbf.free_result(pa_db, result);
		return -1;
	}
	if(result== NULL)
		return -1;

	if(result && result->n <=0)
	{
		LM_INFO("No matching subscription dialog found in database\n");
		
		pa_dbf.free_result(pa_db, result);
		*reply_code= 481;
		*reply_str= pu_481_rpl;

		return -1;
	}

	row = &result->rows[0];
	row_vals = ROW_VALUES(row);
	remote_cseq= row_vals[remote_cseq_col].val.int_val;
	
	if(subs->remote_cseq<= remote_cseq)
	{
		LM_ERR("wrong sequence number received: %d - stored: %d\n",
				subs->remote_cseq, remote_cseq);
		*reply_code= 400;
		*reply_str= pu_400_rpl;
		pa_dbf.free_result(pa_db, result);
		return -1;
	}
	
	subs->status= row_vals[status_col].val.int_val;
	reason.s= (char*)row_vals[reason_col].val.string_val;
	if(reason.s)
	{
		reason.len= strlen(reason.s);
		subs->reason.s= (char*)pkg_malloc(reason.len* sizeof(char));
		if(subs->reason.s== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(subs->reason.s, reason.s, reason.len);
		subs->reason.len= reason.len;
	}

	subs->local_cseq= row_vals[local_cseq_col].val.int_val + 1;
	subs->version= row_vals[version_col].val.int_val + 1;

	if(!EVENT_DIALOG_SLA(subs->event->evp))
	{
		pres_uri.s= (char*)row_vals[pres_uri_col].val.string_val;
		pres_uri.len= strlen(pres_uri.s);
		subs->pres_uri.s= (char*)pkg_malloc(pres_uri.len* sizeof(char));
		if(subs->pres_uri.s== NULL)
		{	
			if(subs->reason.s)
				pkg_free(subs->reason.s);
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(subs->pres_uri.s, pres_uri.s, pres_uri.len);
		subs->pres_uri.len= pres_uri.len;
	}

	record_route.s= (char*)row_vals[record_route_col].val.string_val;
	if(record_route.s)
	{
		record_route.len= strlen(record_route.s);
		subs->record_route.s= (char*)pkg_malloc(record_route.len*sizeof(char));
		if(subs->record_route.s== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(subs->record_route.s, record_route.s, record_route.len);
		subs->record_route.len= record_route.len;
	}

	subs->updated= row_vals[updated_col].val.int_val;
	subs->updated_winfo= row_vals[updated_winfo_col].val.int_val;

	pa_dbf.free_result(pa_db, result);
	result= NULL;

	return 0;
error:
	if(result)
		pa_dbf.free_result(pa_db, result);

	return -1;

}


int handle_expired_subs(subs_t* s)
{
	/* send Notify with state=terminated;reason=timeout */
	
	s->status= TERMINATED_STATUS;
	s->reason.s= "timeout";
	s->reason.len= 7;
	s->expires= 0;
	s->local_cseq++;

	if(send_notify_request(s, NULL, NULL, 1)< 0)
	{
		LM_ERR("send Notify not successful\n");
		return -1;
	}
	
	return 0;

}

void update_db_subs_timer_notifier(void)
{
	db_key_t query_cols[2], result_cols[3];
	db_val_t query_vals[2], *values;
	db_op_t query_ops[2];
	db_row_t *rows;
	db1_res_t *result = NULL;
	int n_query_cols = 0, n_result_cols = 0;
	int r_callid_col = 0, r_to_tag_col = 0, r_from_tag_col = 0;
	int i, res;
	subs_t subs;

	if(pa_db == NULL)
	{
		LM_ERR("null database connection\n");
		goto error;
	}

	if(pa_dbf.use_table(pa_db, &active_watchers_table)< 0)
	{
		LM_ERR("use table failed\n");
		goto error;
	}

	query_cols[n_query_cols]= &str_expires_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= (int)time(NULL) - expires_offset;
	query_ops[n_query_cols]= OP_LT;
	n_query_cols++;

	query_cols[n_query_cols]= &str_updated_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= NO_UPDATE_TYPE;
	query_ops[n_query_cols]= OP_EQ;
	n_query_cols++;

	result_cols[r_callid_col=n_result_cols++] = &str_callid_col;
	result_cols[r_to_tag_col=n_result_cols++] = &str_to_tag_col;
	result_cols[r_from_tag_col=n_result_cols++] = &str_from_tag_col;

	if (pa_dbf.start_transaction)
	{
		if (pa_dbf.start_transaction(pa_db, db_table_lock) < 0)
		{
			LM_ERR("in start_transaction\n");
			goto error;
		}
	}

	if (pa_dbf.query_lock)
		res = db_fetch_query_lock(&pa_dbf, pres_fetch_rows, pa_db, query_cols,
				  query_ops, query_vals, result_cols,
				  n_query_cols, n_result_cols, 0, &result );
	else
		res = db_fetch_query(&pa_dbf, pres_fetch_rows, pa_db, query_cols,
				  query_ops, query_vals, result_cols,
				  n_query_cols, n_result_cols, 0, &result );
	if (res < 0)
	{
		LM_ERR("Can't query db\n");
		goto error;
	}

	if(result == NULL)
	{
		LM_ERR("bad result\n");
		goto error;
	}

	do {
		rows = RES_ROWS(result);
	
		for (i = 0; i <RES_ROW_N(result); i++)
		{
			values = ROW_VALUES(&rows[i]);
			memset(&subs, 0, sizeof(subs_t));

			subs.callid.s = (char *) VAL_STRING(&values[r_callid_col]);
			subs.callid.len = strlen(subs.callid.s);
			subs.to_tag.s = (char *) VAL_STRING(&values[r_to_tag_col]);
			subs.to_tag.len = strlen(subs.to_tag.s);
			subs.from_tag.s = (char *) VAL_STRING(&values[r_from_tag_col]);
			subs.from_tag.len = strlen(subs.from_tag.s);

			set_updated(&subs);
		}
	} while (db_fetch_next(&pa_dbf, pres_fetch_rows, pa_db, &result) == 1
			&& RES_ROW_N(result) > 0);

	if (pa_dbf.end_transaction)
	{
		if (pa_dbf.end_transaction(pa_db) < 0)
		{
			LM_ERR("in end_transaction\n");
			goto error;
		}
	}

error:
	if (result) pa_dbf.free_result(pa_db, result);

	if (pa_dbf.abort_transaction)
	{
		if (pa_dbf.abort_transaction(pa_db) < 0)
			LM_ERR("in abort_transaction\n");
	}

}

void update_db_subs_timer_dbonly(void)
{
	db_op_t qops[1];
	db_key_t qcols[1];
	db_val_t qvals[1];
	db_key_t result_cols[18];
	int pres_uri_col, to_user_col, to_domain_col, from_user_col, from_domain_col,
		callid_col, totag_col, fromtag_col, event_col, event_id_col,
		local_cseq_col, expires_col, rr_col, sockinfo_col,
		contact_col, lcontact_col, watcher_user_col, watcher_domain_col;
	int n_result_cols = 0;
	db1_res_t *result= NULL;
	db_row_t *row = NULL;
	db_val_t *row_vals= NULL;
	int i;
	subs_t s, *s_new, *s_array = NULL, *s_del;
	str ev_name;
	pres_ev_t* event;

	LM_DBG("update_db_subs_timer_dbonly: start\n");

	qcols[0]= &str_expires_col;
	qvals[0].type = DB1_INT;
	qvals[0].nul = 0;
	qvals[0].val.int_val= (int)time(NULL) - expires_offset;
	qops[0]= OP_LT;

	/* query the expired subscriptions */
	result_cols[pres_uri_col=n_result_cols++]   =&str_presentity_uri_col;
	result_cols[expires_col=n_result_cols++]    =&str_expires_col;
	result_cols[event_col=n_result_cols++]      =&str_event_col;
	result_cols[event_id_col=n_result_cols++]   =&str_event_id_col;
	result_cols[to_user_col=n_result_cols++]    =&str_to_user_col;
	result_cols[to_domain_col=n_result_cols++]  =&str_to_domain_col;
	result_cols[from_user_col=n_result_cols++]  =&str_from_user_col;
	result_cols[from_domain_col=n_result_cols++]=&str_from_domain_col;
	result_cols[watcher_user_col=n_result_cols++]  =&str_watcher_username_col;
	result_cols[watcher_domain_col=n_result_cols++]=&str_watcher_domain_col;
	result_cols[callid_col=n_result_cols++]     =&str_callid_col;
	result_cols[totag_col=n_result_cols++]      =&str_to_tag_col;
	result_cols[fromtag_col=n_result_cols++]    =&str_from_tag_col;
	result_cols[local_cseq_col= n_result_cols++]=&str_local_cseq_col;
	result_cols[rr_col= n_result_cols++]        =&str_record_route_col;
	result_cols[sockinfo_col= n_result_cols++]  =&str_socket_info_col;
	result_cols[contact_col= n_result_cols++]   =&str_contact_col;
	result_cols[lcontact_col= n_result_cols++]  =&str_local_contact_col;

	if(pa_dbf.use_table(pa_db, &active_watchers_table)< 0)
	{
		LM_ERR("sql use table failed\n");
		return;
	}

	if (pa_dbf.query(pa_db, qcols, qops, qvals, result_cols,
				1, n_result_cols, 0, &result) < 0) {
		LM_ERR("failed to query database for expired subscriptions\n");
		if(result)
			pa_dbf.free_result(pa_db, result);
		return;
	}

	if(result== NULL)
		return;

	if(result->n <=0 ) {
		pa_dbf.free_result(pa_db, result);
		return;
	}
	LM_DBG("found %d dialogs\n", result->n);
	
	for(i=0; i<result->n; i++)
	{
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);

		memset(&s, 0, sizeof(subs_t));

		s.pres_uri.s= (char*)row_vals[pres_uri_col].val.string_val;
		s.pres_uri.len = strlen(s.pres_uri.s);

		s.to_user.s= (char*)row_vals[to_user_col].val.string_val;
		s.to_user.len= strlen(s.to_user.s);

		s.to_domain.s= (char*)row_vals[to_domain_col].val.string_val;
		s.to_domain.len= strlen(s.to_domain.s);

		s.from_user.s= (char*)row_vals[from_user_col].val.string_val;
		s.from_user.len= strlen(s.from_user.s);

		s.from_domain.s= (char*)row_vals[from_domain_col].val.string_val;
		s.from_domain.len= strlen(s.from_domain.s);

		s.watcher_user.s= (char*)row_vals[watcher_user_col].val.string_val;
		s.watcher_user.len= strlen(s.watcher_user.s);

		s.watcher_domain.s= (char*)row_vals[watcher_domain_col].val.string_val;
		s.watcher_domain.len= strlen(s.watcher_domain.s);

		s.event_id.s=(char*)row_vals[event_id_col].val.string_val;
		s.event_id.len= (s.event_id.s)?strlen(s.event_id.s):0;

		s.to_tag.s= (char*)row_vals[totag_col].val.string_val;
		s.to_tag.len= strlen(s.to_tag.s);

		s.from_tag.s= (char*)row_vals[fromtag_col].val.string_val; 
		s.from_tag.len= strlen(s.from_tag.s);

		s.callid.s= (char*)row_vals[callid_col].val.string_val;
		s.callid.len= strlen(s.callid.s);

		s.record_route.s=  (char*)row_vals[rr_col].val.string_val;
		s.record_route.len= (s.record_route.s)?strlen(s.record_route.s):0;

		s.contact.s= (char*)row_vals[contact_col].val.string_val;
		s.contact.len= strlen(s.contact.s);

		s.sockinfo_str.s = (char*)row_vals[sockinfo_col].val.string_val;
		s.sockinfo_str.len = s.sockinfo_str.s?strlen(s.sockinfo_str.s):0;

		s.local_contact.s = (char*)row_vals[lcontact_col].val.string_val;
		s.local_contact.len = s.local_contact.s?strlen(s.local_contact.s):0;

		ev_name.s= (char*)row_vals[event_col].val.string_val;
		ev_name.len= strlen(ev_name.s);

		event= contains_event(&ev_name, 0);
		if(event== NULL) {
			LM_ERR("Wrong event in database %.*s\n", ev_name.len, ev_name.s);
			continue;
		}
		s.event= event;

		s.local_cseq = row_vals[local_cseq_col].val.int_val +1;
		s.expires = 0;

		s_new= mem_copy_subs(&s, PKG_MEM_TYPE);
		if(s_new== NULL)
		{
			LM_ERR("while copying subs_t structure\n");
			continue;
		}
		s_new->next= s_array;
		s_array= s_new;
		printf_subs(s_new);
	}
	pa_dbf.free_result(pa_db, result);

	s_new = s_array;
	while(s_new) {
		handle_expired_subs(s_new);
		s_del = s_new;
		s_new = s_new->next;
		pkg_free(s_del);
	}

	/* delete the expired subscriptions */
	if(pa_dbf.delete(pa_db, qcols, qops, qvals, 1) < 0)
	{
		LM_ERR("deleting expired information from database\n");
	}
}

void update_db_subs_timer_dbnone(int no_lock)
{
	int i;
	int now = (int)time(NULL);
	subs_t* s= NULL, *prev_s= NULL, *del_s;

	LM_DBG("update_db_subs_timer_dbnone: start\n");

	for(i=0; i<shtable_size; i++) {
		if(!no_lock)
			lock_get(&subs_htable[i].lock);

		prev_s= subs_htable[i].entries;
		s= prev_s->next;

		while(s) {
			printf_subs(s);
			if(s->expires < now - expires_offset) {
				LM_DBG("Found expired record\n");
				if(!no_lock) {
					if(handle_expired_subs(s)< 0) {
						LM_ERR("in function handle_expired_record\n");
					}
				}
				del_s= s;
				s= s->next;
				prev_s->next= s;

				if (del_s->contact.s)
					shm_free(del_s->contact.s);
				shm_free(del_s);
				continue;
			}
			prev_s= s;
			s= s->next;
		}
		if(!no_lock)
			lock_release(&subs_htable[i].lock);
	}
}



void update_db_subs_timer(db1_con_t *db,db_func_t dbf, shtable_t hash_table,
	int htable_size, int no_lock, handle_expired_func_t handle_expired_func)
{
	db_key_t query_cols[24], update_cols[6];
	db_val_t query_vals[24], update_vals[6];
	db_op_t update_ops[1];
	subs_t* del_s;
	int pres_uri_col, to_user_col, to_domain_col, from_user_col, from_domain_col,
		callid_col, totag_col, fromtag_col, event_col,status_col, event_id_col,
		local_cseq_col, remote_cseq_col, expires_col, record_route_col,
		contact_col, local_contact_col, version_col,socket_info_col,reason_col,
		watcher_user_col, watcher_domain_col, updated_col, updated_winfo_col;
	int u_expires_col, u_local_cseq_col, u_remote_cseq_col, u_version_col,
		u_reason_col, u_status_col;
	int i;
	subs_t* s= NULL, *prev_s= NULL;
	int n_query_cols= 0, n_update_cols= 0;
	int n_query_update;
	int now = (int)time(NULL);

	LM_DBG("update_db_subs_timer: start\n");

	query_cols[pres_uri_col= n_query_cols] =&str_presentity_uri_col;
	query_vals[pres_uri_col].type = DB1_STR;
	query_vals[pres_uri_col].nul = 0;
	n_query_cols++;

	query_cols[callid_col= n_query_cols] =&str_callid_col;
	query_vals[callid_col].type = DB1_STR;
	query_vals[callid_col].nul = 0;
	n_query_cols++;

	query_cols[totag_col= n_query_cols] =&str_to_tag_col;
	query_vals[totag_col].type = DB1_STR;
	query_vals[totag_col].nul = 0;
	n_query_cols++;

	query_cols[fromtag_col= n_query_cols] =&str_from_tag_col;
	query_vals[fromtag_col].type = DB1_STR;
	query_vals[fromtag_col].nul = 0;
	n_query_cols++;

	n_query_update= n_query_cols;

	query_cols[to_user_col= n_query_cols] =&str_to_user_col;
	query_vals[to_user_col].type = DB1_STR;
	query_vals[to_user_col].nul = 0;
	n_query_cols++;

	query_cols[to_domain_col= n_query_cols] =&str_to_domain_col;
	query_vals[to_domain_col].type = DB1_STR;
	query_vals[to_domain_col].nul = 0;
	n_query_cols++;

	query_cols[from_user_col= n_query_cols] =&str_from_user_col;
	query_vals[from_user_col].type = DB1_STR;
	query_vals[from_user_col].nul = 0;
	n_query_cols++;

	query_cols[from_domain_col= n_query_cols] =&str_from_domain_col;
	query_vals[from_domain_col].type = DB1_STR;
	query_vals[from_domain_col].nul = 0;
	n_query_cols++;

	query_cols[watcher_user_col= n_query_cols] =&str_watcher_username_col;
	query_vals[watcher_user_col].type = DB1_STR;
	query_vals[watcher_user_col].nul = 0;
	n_query_cols++;

	query_cols[watcher_domain_col= n_query_cols] =&str_watcher_domain_col;
	query_vals[watcher_domain_col].type = DB1_STR;
	query_vals[watcher_domain_col].nul = 0;
	n_query_cols++;

	query_cols[event_col= n_query_cols] =&str_event_col;
	query_vals[event_col].type = DB1_STR;
	query_vals[event_col].nul = 0;
	n_query_cols++;

	query_cols[event_id_col= n_query_cols] =&str_event_id_col;
	query_vals[event_id_col].type = DB1_STR;
	query_vals[event_id_col].nul = 0;
	n_query_cols++;

	query_cols[local_cseq_col= n_query_cols]=&str_local_cseq_col;
	query_vals[local_cseq_col].type = DB1_INT;
	query_vals[local_cseq_col].nul = 0;
	n_query_cols++;

	query_cols[remote_cseq_col= n_query_cols]=&str_remote_cseq_col;
	query_vals[remote_cseq_col].type = DB1_INT;
	query_vals[remote_cseq_col].nul = 0;
	n_query_cols++;

	query_cols[expires_col= n_query_cols] =&str_expires_col;
	query_vals[expires_col].type = DB1_INT;
	query_vals[expires_col].nul = 0;
	n_query_cols++;

	query_cols[status_col= n_query_cols] =&str_status_col;
	query_vals[status_col].type = DB1_INT;
	query_vals[status_col].nul = 0;
	n_query_cols++;

	query_cols[reason_col= n_query_cols] =&str_reason_col;
	query_vals[reason_col].type = DB1_STR;
	query_vals[reason_col].nul = 0;
	n_query_cols++;

	query_cols[record_route_col= n_query_cols] =&str_record_route_col;
	query_vals[record_route_col].type = DB1_STR;
	query_vals[record_route_col].nul = 0;
	n_query_cols++;
	
	query_cols[contact_col= n_query_cols] =&str_contact_col;
	query_vals[contact_col].type = DB1_STR;
	query_vals[contact_col].nul = 0;
	n_query_cols++;

	query_cols[local_contact_col= n_query_cols] =&str_local_contact_col;
	query_vals[local_contact_col].type = DB1_STR;
	query_vals[local_contact_col].nul = 0;
	n_query_cols++;

	query_cols[socket_info_col= n_query_cols] =&str_socket_info_col;
	query_vals[socket_info_col].type = DB1_STR;
	query_vals[socket_info_col].nul = 0;
	n_query_cols++;

	query_cols[version_col= n_query_cols]=&str_version_col;
	query_vals[version_col].type = DB1_INT;
	query_vals[version_col].nul = 0;
	n_query_cols++;

	query_cols[updated_col= n_query_cols]=&str_updated_col;
	query_vals[updated_col].type = DB1_INT;
	query_vals[updated_col].nul = 0;
	n_query_cols++;

	query_cols[updated_winfo_col= n_query_cols]=&str_updated_winfo_col;
	query_vals[updated_winfo_col].type = DB1_INT;
	query_vals[updated_winfo_col].nul = 0;
	n_query_cols++;

	/* cols and values used for update */
	update_cols[u_expires_col= n_update_cols]= &str_expires_col;
	update_vals[u_expires_col].type = DB1_INT;
	update_vals[u_expires_col].nul = 0;
	n_update_cols++;

	update_cols[u_status_col= n_update_cols]= &str_status_col;
	update_vals[u_status_col].type = DB1_INT;
	update_vals[u_status_col].nul = 0;
	n_update_cols++;

	update_cols[u_reason_col= n_update_cols]= &str_reason_col;
	update_vals[u_reason_col].type = DB1_STR;
	update_vals[u_reason_col].nul = 0;
	n_update_cols++;

	update_cols[u_remote_cseq_col= n_update_cols]= &str_remote_cseq_col;
	update_vals[u_remote_cseq_col].type = DB1_INT;
	update_vals[u_remote_cseq_col].nul = 0;
	n_update_cols++;

	update_cols[u_local_cseq_col= n_update_cols]= &str_local_cseq_col;
	update_vals[u_local_cseq_col].type = DB1_INT;
	update_vals[u_local_cseq_col].nul = 0;
	n_update_cols++;

	update_cols[u_version_col= n_update_cols]= &str_version_col;
	update_vals[u_version_col].type = DB1_INT;
	update_vals[u_version_col].nul = 0;
	n_update_cols++;

	for(i=0; i<htable_size; i++) 
	{
		if(!no_lock)
			lock_get(&hash_table[i].lock);

		prev_s= hash_table[i].entries;
		s= prev_s->next;

		while(s)
		{
			printf_subs(s);
			if(s->expires < now- expires_offset)
			{
				LM_DBG("Found expired record\n");
				if(!no_lock)
				{
					if(handle_expired_func(s)< 0)
						LM_ERR("in function handle_expired_record\n");
				}
				del_s= s;
				s= s->next;
				prev_s->next= s;
				
				/* need for a struct free/destroy? */
				if (del_s->contact.s)
					shm_free(del_s->contact.s);
				shm_free(del_s);
				continue;
			}
			switch(s->db_flag)
			{
				case NO_UPDATEDB_FLAG:
				case WTHROUGHDB_FLAG:
					LM_DBG("%s\n", (s->db_flag==NO_UPDATEDB_FLAG)?
							"NO_UPDATEDB_FLAG":"WTHROUGHDB_FLAG");
					break;

				case UPDATEDB_FLAG:
					LM_DBG("UPDATEDB_FLAG\n");

					query_vals[pres_uri_col].val.str_val= s->pres_uri;
					query_vals[callid_col].val.str_val= s->callid;
					query_vals[totag_col].val.str_val= s->to_tag;
					query_vals[fromtag_col].val.str_val= s->from_tag;
				
					update_vals[u_expires_col].val.int_val= s->expires;
					update_vals[u_local_cseq_col].val.int_val= s->local_cseq;
					update_vals[u_remote_cseq_col].val.int_val= s->remote_cseq;
					update_vals[u_version_col].val.int_val= s->version;
					update_vals[u_status_col].val.int_val= s->status;
					update_vals[u_reason_col].val.str_val= s->reason;

					if(dbf.update(db, query_cols, 0, query_vals, update_cols, 
								update_vals, n_query_update, n_update_cols)< 0)
					{
						LM_ERR("updating in database\n");
					} else {
						s->db_flag= NO_UPDATEDB_FLAG;
					}
					break;

				case  INSERTDB_FLAG:
					LM_DBG("INSERTDB_FLAG\n");

					query_vals[pres_uri_col].val.str_val= s->pres_uri;
					query_vals[callid_col].val.str_val= s->callid;
					query_vals[totag_col].val.str_val= s->to_tag;
					query_vals[fromtag_col].val.str_val= s->from_tag;
					query_vals[to_user_col].val.str_val = s->to_user;
					query_vals[to_domain_col].val.str_val = s->to_domain;
					query_vals[from_user_col].val.str_val = s->from_user;
					query_vals[from_domain_col].val.str_val = s->from_domain;
					query_vals[watcher_user_col].val.str_val = s->watcher_user;
					query_vals[watcher_domain_col].val.str_val = s->watcher_domain;
					query_vals[event_col].val.str_val = s->event->name;
					query_vals[event_id_col].val.str_val = s->event_id;
					query_vals[local_cseq_col].val.int_val= s->local_cseq;
					query_vals[remote_cseq_col].val.int_val= s->remote_cseq;
					query_vals[expires_col].val.int_val = s->expires;
					query_vals[record_route_col].val.str_val = s->record_route;
					query_vals[contact_col].val.str_val = s->contact;
					query_vals[local_contact_col].val.str_val = s->local_contact;
					query_vals[version_col].val.int_val= s->version;
					query_vals[status_col].val.int_val= s->status;
					query_vals[reason_col].val.str_val= s->reason;
					query_vals[socket_info_col].val.str_val= s->sockinfo_str;
					query_vals[updated_col].val.int_val = -1;
					query_vals[updated_winfo_col].val.int_val = -1;

					if(dbf.insert(db,query_cols,query_vals,n_query_cols )<0)
					{
						LM_ERR("unsuccessful sql insert\n");
					} else {
						s->db_flag= NO_UPDATEDB_FLAG;
					}
					break;
			} /* switch */
			prev_s= s;
			s= s->next;
		}
		if(!no_lock)
			lock_release(&hash_table[i].lock);
	}

	update_vals[0].val.int_val= (int)time(NULL) - expires_offset;
	update_ops[0]= OP_LT;
	if(dbf.delete(db, update_cols, update_ops, update_vals, 1) < 0)
	{
		LM_ERR("deleting expired information from database\n");
	}
}

/**
 * timer_db_update function does the following tasks:
 *	1. checks for expires subscriptions and does the corresponding processing
 *		- if db_mode != DB_ONLY : checks by traversing the hash table
 *		- if db_mode == DB_ONLY : checks by querying database
 *	2. if db_mode == WRITE_BACK : updates the subscriptions in database
*/
void timer_db_update(unsigned int ticks,void *param)
{
	int no_lock=0;
	LM_DBG("db_update timer\n");
	if(ticks== 0 && param == NULL)
		no_lock= 1;


	switch (subs_dbmode) {
	case DB_ONLY:
		if (pres_notifier_processes > 0)
			update_db_subs_timer_notifier();
		else
			update_db_subs_timer_dbonly();
	break;
	case NO_DB:
		update_db_subs_timer_dbnone(no_lock);
	break;
	default:
		if(pa_dbf.use_table(pa_db, &active_watchers_table)< 0)
		{
			LM_ERR("sql use table failed\n");
			return;
		}
		update_db_subs_timer(pa_db, pa_dbf, subs_htable, shtable_size,
						no_lock, handle_expired_subs);
	}
}


int restore_db_subs(void)
{
	db_key_t result_cols[22]; 
	db1_res_t *result= NULL;
	db_row_t *rows = NULL;	
	db_val_t *row_vals= NULL;
	int i;
	int n_result_cols= 0;
	int pres_uri_col, expires_col, from_user_col, from_domain_col,to_user_col; 
	int callid_col,totag_col,fromtag_col,to_domain_col,sockinfo_col,reason_col;
	int event_col,contact_col,record_route_col, event_id_col, status_col;
	int remote_cseq_col, local_cseq_col, local_contact_col, version_col;
	int watcher_user_col, watcher_domain_col;
	subs_t s;
	str ev_sname;
	pres_ev_t* event= NULL;
	event_t parsed_event;
	unsigned int expires;
	unsigned int hash_code;
	int nr_rows;

	result_cols[pres_uri_col=n_result_cols++]	=&str_presentity_uri_col;
	result_cols[expires_col=n_result_cols++]	=&str_expires_col;
	result_cols[event_col=n_result_cols++]		=&str_event_col;
	result_cols[event_id_col=n_result_cols++]	=&str_event_id_col;
	result_cols[to_user_col=n_result_cols++]	=&str_to_user_col;
	result_cols[to_domain_col=n_result_cols++]	=&str_to_domain_col;
	result_cols[from_user_col=n_result_cols++]	=&str_from_user_col;
	result_cols[from_domain_col=n_result_cols++]	=&str_from_domain_col;
	result_cols[watcher_user_col=n_result_cols++]	=&str_watcher_username_col;
	result_cols[watcher_domain_col=n_result_cols++]	=&str_watcher_domain_col;
	result_cols[callid_col=n_result_cols++]		=&str_callid_col;
	result_cols[totag_col=n_result_cols++]		=&str_to_tag_col;
	result_cols[fromtag_col=n_result_cols++]	=&str_from_tag_col;
	result_cols[local_cseq_col= n_result_cols++]	=&str_local_cseq_col;
	result_cols[remote_cseq_col= n_result_cols++]	=&str_remote_cseq_col;
	result_cols[record_route_col= n_result_cols++]	=&str_record_route_col;
	result_cols[sockinfo_col= n_result_cols++]	=&str_socket_info_col;
	result_cols[contact_col= n_result_cols++]	=&str_contact_col;
	result_cols[local_contact_col= n_result_cols++]	=&str_local_contact_col;
	result_cols[version_col= n_result_cols++]	=&str_version_col;
	result_cols[status_col= n_result_cols++]	=&str_status_col;
	result_cols[reason_col= n_result_cols++]	=&str_reason_col;
	
	if(!pa_db)
	{
		LM_ERR("null database connection\n");
		return -1;
	}

	if(pa_dbf.use_table(pa_db, &active_watchers_table)< 0)
	{
		LM_ERR("in use table\n");
		return -1;
	}

	/* select the whole table and all the columns */
	if (db_fetch_query(&pa_dbf, pres_fetch_rows, pa_db, 0, 0, 0, result_cols,
				0, n_result_cols, 0, &result) < 0)
	{
		LM_ERR("querying presentity\n");
		goto error;
	}

	if (result == NULL)
	{
		LM_ERR("bad result\n");
		goto error;
	}

	do {
		nr_rows = RES_ROW_N(result);
		LM_DBG("loading information from database %i records\n", nr_rows);

		rows = RES_ROWS(result);

		/* for every row */
		for(i=0; i<nr_rows; i++)
		{

			row_vals = ROW_VALUES(rows +i);
			memset(&s, 0, sizeof(subs_t));

			expires= row_vals[expires_col].val.int_val;
		
			if(expires< (int)time(NULL))
			    continue;
	
			s.pres_uri.s= (char*)row_vals[pres_uri_col].val.string_val;
			s.pres_uri.len= strlen(s.pres_uri.s);
		
			s.to_user.s=(char*)row_vals[to_user_col].val.string_val;
			s.to_user.len= strlen(s.to_user.s);
			
			s.to_domain.s=(char*)row_vals[to_domain_col].val.string_val;
			s.to_domain.len= strlen(s.to_domain.s);
			
			s.from_user.s=(char*)row_vals[from_user_col].val.string_val;
			s.from_user.len= strlen(s.from_user.s);
			
			s.from_domain.s=(char*)row_vals[from_domain_col].val.string_val;
			s.from_domain.len= strlen(s.from_domain.s);

			s.watcher_user.s=(char*)row_vals[watcher_user_col].val.string_val;
			s.watcher_user.len= strlen(s.watcher_user.s);
			
			s.watcher_domain.s=(char*)row_vals[watcher_domain_col].val.string_val;
			s.watcher_domain.len= strlen(s.watcher_domain.s);
			
			s.to_tag.s=(char*)row_vals[totag_col].val.string_val;
			s.to_tag.len= strlen(s.to_tag.s);

			s.from_tag.s=(char*)row_vals[fromtag_col].val.string_val;
			s.from_tag.len= strlen(s.from_tag.s);

			s.callid.s=(char*)row_vals[callid_col].val.string_val;
			s.callid.len= strlen(s.callid.s);

			ev_sname.s= (char*)row_vals[event_col].val.string_val;
			ev_sname.len= strlen(ev_sname.s);
		
			event= contains_event(&ev_sname, &parsed_event);
			if(event== NULL)
			    {
				LM_DBG("insert a new event structure in the list waiting"
				       " to be filled in\n");
				
				/*insert a new event structure in the list waiting to be filled in*/
				event= (pres_ev_t*)shm_malloc(sizeof(pres_ev_t));
				if(event== NULL)
				    {
					free_event_params(parsed_event.params.list, PKG_MEM_TYPE);
					ERR_MEM(SHM_MEM_STR);
				    }
				memset(event, 0, sizeof(pres_ev_t));
				event->name.s= (char*)shm_malloc(ev_sname.len* sizeof(char));
				if(event->name.s== NULL)
				    {
					free_event_params(parsed_event.params.list, PKG_MEM_TYPE);
					ERR_MEM(SHM_MEM_STR);
				    }
				memcpy(event->name.s,ev_sname.s, ev_sname.len);
				event->name.len= ev_sname.len;
				
				event->evp= shm_copy_event(&parsed_event);
				if(event->evp== NULL)
				    {
					LM_ERR("ERROR copying event_t structure\n");
					free_event_params(parsed_event.params.list, PKG_MEM_TYPE);
					goto error;
				    }
				event->next= EvList->events;
				EvList->events= event;
			    }
			
			free_event_params(parsed_event.params.list, PKG_MEM_TYPE);
			
			s.event= event;

			s.event_id.s=(char*)row_vals[event_id_col].val.string_val;
			if(s.event_id.s)
			    s.event_id.len= strlen(s.event_id.s);
			
			s.remote_cseq= row_vals[remote_cseq_col].val.int_val;
			s.local_cseq= row_vals[local_cseq_col].val.int_val;
			s.version= row_vals[version_col].val.int_val;
		
			s.expires= expires- (int)time(NULL);
			s.status= row_vals[status_col].val.int_val;

			s.reason.s= (char*)row_vals[reason_col].val.string_val;
			if(s.reason.s)
			    s.reason.len= strlen(s.reason.s);

			s.contact.s=(char*)row_vals[contact_col].val.string_val;
			s.contact.len= strlen(s.contact.s);
			
			s.local_contact.s=(char*)row_vals[local_contact_col].val.string_val;
			s.local_contact.len= strlen(s.local_contact.s);
			
			s.record_route.s=(char*)row_vals[record_route_col].val.string_val;
			if(s.record_route.s)
			    s.record_route.len= strlen(s.record_route.s);
	
			s.sockinfo_str.s=(char*)row_vals[sockinfo_col].val.string_val;
			s.sockinfo_str.len= strlen(s.sockinfo_str.s);
			s.db_flag = (subs_dbmode==WRITE_THROUGH)?WTHROUGHDB_FLAG:NO_UPDATEDB_FLAG;
			hash_code= core_case_hash(&s.pres_uri, &s.event->name, shtable_size);
			if(insert_shtable(subs_htable, hash_code, &s)< 0)
			{
				LM_ERR("adding new record in hash table\n");
				goto error;
			}
		}

	} while((db_fetch_next(&pa_dbf, pres_fetch_rows, pa_db, &result)==1)
			&& (RES_ROW_N(result)>0));

	pa_dbf.free_result(pa_db, result);

	/* delete all records  only if in memory mode */
	if(subs_dbmode == NO_DB) {
		if(pa_dbf.delete(pa_db, 0,0,0,0)< 0)
		{
			LM_ERR("deleting all records from database table\n");
			return -1;
		}
	}
	return 0;

error:
	if(result)
		pa_dbf.free_result(pa_db, result);
	return -1;

}

int get_db_subs_auth(subs_t* subs, int* found)
{
	db_key_t db_keys[5];
	db_val_t db_vals[5];
	int n_query_cols= 0; 
	db_key_t result_cols[3];
	db1_res_t *result = NULL;
	db_row_t *row ;	
	db_val_t *row_vals ;

	db_keys[n_query_cols] =&str_presentity_uri_col;
	db_vals[n_query_cols].type = DB1_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val= subs->pres_uri;
	n_query_cols++;

	db_keys[n_query_cols] =&str_watcher_username_col;
	db_vals[n_query_cols].type = DB1_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val = subs->watcher_user;
	n_query_cols++;

	db_keys[n_query_cols] =&str_watcher_domain_col;
	db_vals[n_query_cols].type = DB1_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val = subs->watcher_domain;
	n_query_cols++;
	
	db_keys[n_query_cols] =&str_event_col;
	db_vals[n_query_cols].type = DB1_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val = subs->event->name;
	n_query_cols++;

	result_cols[0] = &str_status_col;
	result_cols[1] = &str_reason_col;
	
	if(pa_dbf.use_table(pa_db, &watchers_table)< 0)
	{
		LM_ERR("in use table\n");
		return -1;
	}	

	if(pa_dbf.query(pa_db, db_keys, 0, db_vals, result_cols,
					n_query_cols, 2, 0, &result )< 0)
	{
		LM_ERR("while querying watchers table\n");
		if(result)
			pa_dbf.free_result(pa_db, result);
		return -1;
	}
	if(result== NULL)
		return -1;
	
	if(result->n<= 0)
	{
		*found= 0;
		pa_dbf.free_result(pa_db, result);
		return 0;
	}

	*found= 1;
	row = &result->rows[0];
	row_vals = ROW_VALUES(row);
	subs->status= row_vals[0].val.int_val;

	if(row_vals[1].val.string_val)
	{

		subs->reason.len= strlen(row_vals[1].val.string_val);
		if(subs->reason.len== 0)
			subs->reason.s= NULL;
		else
		{
			subs->reason.s= (char*)pkg_malloc(subs->reason.len*sizeof(char));
			if(subs->reason.s== NULL)
			{
				pa_dbf.free_result(pa_db, result);
				ERR_MEM(PKG_MEM_STR);
			}		
			memcpy(subs->reason.s, row_vals[1].val.string_val, subs->reason.len);
		}
	}
	
	pa_dbf.free_result(pa_db, result);
	return 0;
error:
	return -1;
}	

int insert_db_subs_auth(subs_t* subs)
{
	db_key_t db_keys[10];
	db_val_t db_vals[10];
	int n_query_cols= 0; 

	db_keys[n_query_cols] =&str_presentity_uri_col;
	db_vals[n_query_cols].type = DB1_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val= subs->pres_uri;
	n_query_cols++;

	db_keys[n_query_cols] =&str_watcher_username_col;
	db_vals[n_query_cols].type = DB1_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val = subs->watcher_user;
	n_query_cols++;

	db_keys[n_query_cols] =&str_watcher_domain_col;
	db_vals[n_query_cols].type = DB1_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val = subs->watcher_domain;
	n_query_cols++;
	
	db_keys[n_query_cols] =&str_event_col;
	db_vals[n_query_cols].type = DB1_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val = subs->event->name;
	n_query_cols++;

	db_keys[n_query_cols] =&str_status_col;
	db_vals[n_query_cols].type = DB1_INT;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.int_val = subs->status;
	n_query_cols++;
								
	db_keys[n_query_cols] = &str_inserted_time_col;
	db_vals[n_query_cols].type = DB1_INT;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.int_val= (int)time(NULL);
	n_query_cols++;

	db_keys[n_query_cols] =&str_reason_col;
	db_vals[n_query_cols].type = DB1_STR;
	db_vals[n_query_cols].nul = 0;
	if(subs->reason.s && subs->reason.len)
		db_vals[n_query_cols].val.str_val = subs->reason;
	else
	{
		db_vals[n_query_cols].val.str_val.s = ""; 
		db_vals[n_query_cols].val.str_val.len = 0; 
	}
	n_query_cols++;	
	
	if (pa_dbf.use_table(pa_db, &watchers_table) < 0) 
	{
		LM_ERR("in use_table\n");
		return -1;
	}

	if (pa_dbf.replace != NULL)
	{
		if(pa_dbf.replace(pa_db, db_keys, db_vals, n_query_cols,
					2, 0) < 0)
		{
			LM_ERR("in sql replace\n");
			goto error;
		}
	}
	else
	{
		/* If you use insert() instead of replace() be prepared for some
 		   DB error messages.  There is a lot of time between the 
		   query() that indicated there was no matching entry in the DB
		   and this insert(), so on a multi-user system it is entirely
		   possible (even likely) that a record will be added after the
		   query() but before this insert(). */
		if(pa_dbf.insert(pa_db, db_keys, db_vals, n_query_cols )< 0)
		{	
			LM_ERR("in sql insert\n");
			goto error;
		}
	}

	return 0;

error:
	return -1;
}

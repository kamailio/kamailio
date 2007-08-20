/*
 * $Id$
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP serves.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2006-08-15  initial version (anca)
 */

#include "../../ut.h"
#include "../../usr_avp.h"
#include "../../data_lump_rpl.h"
#include "../../parser/parse_expires.h"
#include "../../parser/parse_event.h"
#include "../../parser/contact/parse_contact.h"
#include "presence.h"
#include "subscribe.h"
#include "utils_func.h"
#include "notify.h"
#include "../pua/hash.h"

int get_stored_info(struct sip_msg* msg, subs_t* subs, int* error_ret);
int get_database_info(struct sip_msg* msg, subs_t* subs, int* error_ret);

static str su_200_rpl  = str_init("OK");
static str pu_481_rpl  = str_init("Subscription does not exist");
static str pu_400_rpl  = str_init("Bad request");

int send_202ok(struct sip_msg * msg, int lexpire, str *rtag, str* local_contact)
{
	static str hdr_append;

	hdr_append.s = (char *)pkg_malloc( sizeof(char)*(local_contact->len+ 128));
	if(hdr_append.s == NULL)
	{
		LOG(L_ERR,"PRESENCE: send_202ok:ERROR no more pkg memory\n");
		return -1;
	}
	hdr_append.len = sprintf(hdr_append.s, "Expires: %d\r\n", lexpire);	
	
	strncpy(hdr_append.s+hdr_append.len ,"Contact: <", 10);
	hdr_append.len += 10;
	strncpy(hdr_append.s+hdr_append.len, local_contact->s, local_contact->len);
	hdr_append.len+= local_contact->len;
	strncpy(hdr_append.s+hdr_append.len, ">", 1);
	hdr_append.len += 1;
	strncpy(hdr_append.s+hdr_append.len, CRLF, CRLF_LEN);
	hdr_append.len += CRLF_LEN;

	hdr_append.s[hdr_append.len]= '\0';
	
	if (add_lump_rpl( msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR)==0 )
	{
		LOG(L_ERR,"ERROR:send_202oky : unable to add lump_rl\n");
		goto error;
	}

	if( slb.reply_dlg( msg, 202, &su_200_rpl, rtag)== -1)
	{
		LOG(L_ERR,"PRESENCE:send_202ok: ERORR while sending reply\n");
		goto error;
	}
	
	pkg_free(hdr_append.s);
	return 0;

error:

	pkg_free(hdr_append.s);
	return -1;
}

int send_200ok(struct sip_msg * msg, int lexpire, str *rtag, str* local_contact)
{
	static str hdr_append;	

	hdr_append.s = (char *)pkg_malloc( sizeof(char)*(local_contact->len+ 128));
	if(hdr_append.s == NULL)
	{
		LOG(L_ERR,"ERROR:send_200ok : unable to add lump_rl\n");
		return -1;
	}
	hdr_append.len = sprintf(hdr_append.s, "Expires: %d\r\n", lexpire);
	strncpy(hdr_append.s+hdr_append.len ,"Contact: <", 10);
	hdr_append.len += 10;
	strncpy(hdr_append.s+hdr_append.len, local_contact->s, local_contact->len);
	hdr_append.len+= local_contact->len;
	strncpy(hdr_append.s+hdr_append.len, ">", 1);
	hdr_append.len += 1;
	strncpy(hdr_append.s+hdr_append.len, CRLF, CRLF_LEN);
	hdr_append.len += CRLF_LEN;

	hdr_append.s[hdr_append.len]= '\0';

	if (add_lump_rpl( msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR)==0 )
	{
		LOG(L_ERR,"ERROR:send_200ok: unable to add lump_rl\n");
		goto error;
	}

	if( slb.reply_dlg( msg, 200, &su_200_rpl, rtag)== -1)
	{
		LOG(L_ERR,"PRESENCE:send_200ok : ERORR while sending reply\n");
		goto error;
	}
	
	pkg_free(hdr_append.s);
	return 0;
error:
	pkg_free(hdr_append.s);
	return -1;

}

int delete_db_subs(str pres_uri, str ev_stored_name, str to_tag)
{
	db_key_t query_cols[5];
	db_val_t query_vals[5];
	int n_query_cols= 0;

	query_cols[n_query_cols] = "pres_uri";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = pres_uri;
	n_query_cols++;

	query_cols[n_query_cols] = "event";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = ev_stored_name;
	n_query_cols++;

	query_cols[n_query_cols] = "to_tag";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = to_tag;
	n_query_cols++;
	
	if (pa_dbf.use_table(pa_db, active_watchers_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE: delete_db_subs: ERROR in use_table\n");
		return -1;
	}

	if(pa_dbf.delete(pa_db, query_cols, 0, query_vals,
				n_query_cols)< 0 )
	{
		LOG(L_ERR,"PRESENCE: delete_db_subs: ERROR cleaning"
				" unsubscribed messages\n");
		return -1;
	}

	return 0;
}

int update_subs_db(subs_t* subs, int type)
{
	db_key_t query_cols[22];
	db_val_t query_vals[22], update_vals[6];
	db_key_t update_keys[5];
	int n_update_cols= 0;
	int n_query_cols = 0;

	query_cols[n_query_cols] = "pres_uri";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->pres_uri;
	n_query_cols++;
	
	query_cols[n_query_cols] = "from_user";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->from_user;
	n_query_cols++;
	
	query_cols[n_query_cols] = "from_domain";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->from_domain;
	n_query_cols++;

	query_cols[n_query_cols] = "event";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->event->name;
	n_query_cols++;

	if(subs->event_id.s)
	{
		query_cols[n_query_cols] = "event_id";
		query_vals[n_query_cols].type = DB_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = subs->event_id;
		n_query_cols++;
	}
	query_cols[n_query_cols] = "callid";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->callid;
	n_query_cols++;

	query_cols[n_query_cols] = "to_tag";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->to_tag;
	n_query_cols++;

	query_cols[n_query_cols] = "from_tag";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->from_tag;
	n_query_cols++;

	if(type & REMOTE_TYPE)
	{
		update_keys[n_update_cols] = "expires";
		update_vals[n_update_cols].type = DB_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = subs->expires + (int)time(NULL);
		n_update_cols++;
	
		update_keys[n_update_cols] = "remote_cseq";
		update_vals[n_update_cols].type = DB_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = subs->remote_cseq; 
		n_update_cols++;
	}
	else
	{
		update_keys[n_update_cols] = "local_cseq";
		update_vals[n_update_cols].type = DB_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = subs->local_cseq;
		n_update_cols++;
	
		update_keys[n_update_cols] = "version";
		update_vals[n_update_cols].type = DB_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = subs->version+ 1; 
		n_update_cols++;

	}
	update_keys[n_update_cols] = "status";
	update_vals[n_update_cols].type = DB_INT;
	update_vals[n_update_cols].nul = 0;
	update_vals[n_update_cols].val.int_val = subs->status;
	n_update_cols++;

	if (pa_dbf.use_table(pa_db, active_watchers_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE:update_subs_db: ERROR in use_table\n");
		return -1;
	}
		
	if( pa_dbf.update( pa_db,query_cols, 0, query_vals,
				update_keys, update_vals, n_query_cols,n_update_cols )<0) 
	{
		LOG( L_ERR , "PRESENCE:update_subs_db:ERROR while updating"
				" presence information\n");
		return -1;
	}
	return 0;
}

int update_subscription(struct sip_msg* msg, subs_t* subs, str *rtag,
		int to_tag_gen)
{	

	DBG("PRESENCE: update_subscription...\n");
	printf_subs(subs);	
	
	if( to_tag_gen ==0) /*if a SUBSCRIBE within a dialog */
	{
		if(subs->expires == 0)
		{
			DBG("PRESENCE:update_subscription: expires =0 ->"
					" deleting record\n");
		
			if( delete_db_subs(subs->pres_uri, 
						subs->event->name, subs->to_tag)< 0)
			{
				LOG(L_ERR, "PRESENCE:update_subscription: ERROR while deleting"
						"subscription record from database\n");
				goto error;
			}
			/* delete record from hash table also */

			delete_shtable(subs->pres_uri,subs->event->name,subs->to_tag);
		
			if(subs->event->type & PUBL_TYPE)
			{	
				if( send_202ok(msg, subs->expires, rtag, &subs->local_contact) <0)
				{
					LOG(L_ERR, "PRESENCE:update_subscription:ERROR while"
						" sending 202 OK\n");
					goto error;
				}
				
				if(subs->event->wipeer)
				{
					if(query_db_notify(&subs->pres_uri,
								subs->event->wipeer, NULL)< 0)
					{
						LOG(L_ERR, "PRESENCE:update_subscription:Could not send"
							" notify for winfo\n");
						goto error;
					}
				}

			}	
			else /* if unsubscribe for winfo */
			{
				if( send_200ok(msg, subs->expires, rtag, &subs->local_contact) <0)
				{
					LOG(L_ERR, "PRESENCE:update_subscription:ERROR while"
						" sending 202 OK\n");
					goto error;
				}
			}
		
			if(notify(subs, NULL, NULL, 0)< 0)
			{
				LOG(L_ERR, "PRESENCE:update_subscription: Could not send"
				" notify \n");
				goto error;
			}

			return 1;
		}

		if(update_shtable(subs, REMOTE_TYPE)< 0)
		{
			if(fallback2db)
			{
				/* update in database table */
				if(update_subs_db(subs, REMOTE_TYPE)< 0)
				{
					LOG(L_ERR, "PRESENCE:update_subscription: ERROR updating"
							"subscription in database table\n");
					goto error;
				}
			}
			else
			{
				LOG(L_ERR, "PRESENCE:update_subscription: ERROR updating"
					"subscription record in hash table\n");
				goto error;
			}
		}
	}
	else
	{
		if(subs->expires!= 0)
		{	
			if(insert_shtable(subs)< 0)
			{
				LOG(L_ERR, "PRESENCE:update_subscription: ERROR when inserting"
						" new record in subs_htable\n");
				goto error;
			}
		}
		/*otherwise there is a subscription outside a dialog with expires= 0 
		 * no update in database, but should try to send Notify */
		 
	}

/* reply_and_notify  */

	if(subs->event->type & PUBL_TYPE)
	{	
		if( send_202ok(msg, subs->expires, rtag, &subs->local_contact) <0)
		{
			LOG(L_ERR, "PRESENCE:update_subscription:ERROR while"
					" sending 202 OK\n");
			goto error;
		}
		
		if(subs->expires!= 0 && subs->event->wipeer)
		{	
			DBG("PRESENCE:update_subscription: send Notify with winfo\n");
			if(query_db_notify(&subs->pres_uri, subs->event->wipeer,subs )< 0)
			{
				LOG(L_ERR, "PRESENCE:update_subscription:Could not send"
					" notify winfo\n");
				goto error;
			}	
			if(subs->send_on_cback== 0)
			{	
				if(notify(subs, NULL, NULL, 0)< 0)
				{
					LOG(L_ERR, "PRESENCE:update_subscription: Could not send"
					" notify \n");
					goto error;
				}
			}
		}
		else
		{
			if(notify(subs, NULL, NULL, 0)< 0)
			{
				LOG(L_ERR, "PRESENCE:update_subscription: Could not send"
				" notify\n");
				goto error;
			}
		}	
			
	}
	else 
	{
		if( send_200ok(msg, subs->expires, rtag, &subs->local_contact) <0)
		{
			LOG(L_ERR, "PRESENCE:update_subscription:ERROR while"
					" sending 202 OK\n");
			goto error;
		}		
		if(notify(subs, NULL, NULL, 0 )< 0)
		{
			LOG(L_ERR, "PRESENCE:update_subscription: ERROR while"
				" sending notify\n");
			goto error;
		}
	}
	return 0;
	
error:

	LOG(L_ERR, "PRESENCE:update_presentity: ERROR occured\n");
	return -1;

}

void msg_watchers_clean(unsigned int ticks,void *param)
{
	db_key_t db_keys[3], result_cols[1];
	db_val_t db_vals[3];
	db_op_t  db_ops[3] ;
	db_res_t *result= NULL;

	DBG("PRESENCE: msg_watchers_clean:cleaning pending subscriptions\n");
	
	db_keys[0] ="inserted_time";
	db_ops[0] = OP_LT;
	db_vals[0].type = DB_INT;
	db_vals[0].nul = 0;
	db_vals[0].val.int_val = (int)time(NULL)- 24*3600 ;

	db_keys[1] = "subs_status";
	db_ops [1] = OP_EQ;
	db_vals[1].type = DB_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val.s = "pending";
	db_vals[1].val.str_val.len = 7;

	result_cols[0]= "id";
	if (pa_dbf.use_table(pa_db, watchers_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE:msg_watchers_clean: ERROR in use_table\n");
		return ;
	}
	
	if(pa_dbf.query(pa_db, db_keys, db_ops, db_vals, result_cols, 2, 1, 0, &result )< 0)
	{
		LOG(L_ERR, "PRESENCE:msg_watchers_clean: ERROR while querying database"
				" for expired messages\n");
		if(result)
			pa_dbf.free_result(pa_db, result);
		return;
	}
	if(result == NULL)
		return;
	if(result->n <= 0)
	{
		pa_dbf.free_result(pa_db, result);
		return;
	}
	pa_dbf.free_result(pa_db, result);

	if (pa_dbf.delete(pa_db, db_keys, db_ops, db_vals, 2) < 0) 
		LOG(L_ERR,"PRESENCE:msg_watchers_clean: ERROR cleaning pending "
				" subscriptions\n");
}

int handle_subscribe(struct sip_msg* msg, char* str1, char* str2)
{
	struct sip_uri to_uri;
	struct sip_uri from_uri;
	struct to_body *pto, *pfrom = NULL, TO;
	int lexpire;
	int  to_tag_gen = 0;
	str rtag_value;
	subs_t subs;
	static char buf[50];
	str rec_route= {0, 0};
	int error_ret = -1;
	int rt  = 0;
	db_key_t db_keys[10];
	db_val_t db_vals[10];
	db_key_t update_keys[5];
	db_val_t update_vals[5];
	int n_query_cols= 0; 
	db_key_t result_cols[2];
	db_res_t *result = NULL;
	db_row_t *row ;	
	db_val_t *row_vals ;
	int status= PENDING_STATUS;
	str reason= {0, 0};
	str* contact= NULL;
	pres_ev_t* event= NULL;
	event_t* parsed_event= NULL;
	param_t* ev_param= NULL;
	int result_code, result_n;

	/* ??? rename to avoid collisions with other symbols */
	counter ++;
	contact_body_t *b;

	memset(&subs, 0, sizeof(subs_t));

	if ( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LOG(L_ERR, "PRESENCE: handle_subscribe:error parsing headers\n");

		if (slb.reply(msg, 400, &pu_400_rpl) == -1)
		{
			LOG(L_ERR, "PRESENCE: handle_subscribe: ERROR while sending"
					" 400 reply\n");
		}
		error_ret = 0;
		goto error;
	}
	
	/* inspecting the Event header field */
	if(msg->event && msg->event->body.len > 0)
	{
		if (!msg->event->parsed && (parse_event(msg->event) < 0))
		{
			LOG(L_ERR,
				"PRESENCE: handle_subscribe: ERROR cannot parse Event header\n");
			goto error;
		}
		if(((event_t*)msg->event->parsed)->parsed & EVENT_OTHER)
		{	
			goto bad_event;
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
	ev_param= parsed_event->params;
	while(ev_param)
	{
		if(ev_param->name.len== 2 && strncmp(ev_param->name.s, "id", 2)== 0)
		{
			subs.event_id= ev_param->body;
			break;
		}
		ev_param= ev_param->next;
	}		
	
	/* examine the expire header field */
	if(msg->expires && msg->expires->body.len > 0)
	{
		if (!msg->expires->parsed && (parse_expires(msg->expires) < 0))
		{
			LOG(L_ERR,
				"PRESENCE: handle_subscribe: ERROR cannot parse Expires header\n");
			goto error;
		}
		DBG("PRESENCE: handle_subscribe: 'expires' found\n");
		lexpire = ((exp_body_t*)msg->expires->parsed)->val;
		DBG("PRESENCE: handle_subscribe: lexpire= %d\n", lexpire);

	}
	else 
	{
		DBG("PRESENCE: handle_subscribe: 'expires' not found; default=%d\n",
				event->default_expires);
		lexpire = event->default_expires;
	}
	if(lexpire > max_expires)
		lexpire = max_expires;

	subs.expires = lexpire;

	if( msg->to==NULL || msg->to->body.s==NULL)
	{
		LOG(L_ERR, "PRESENCE: handle_subscribe: ERROR cannot parse TO"
				" header\n");
		goto error;
	}
	/* examine the to header */
	if(msg->to->parsed != NULL)
	{
		pto = (struct to_body*)msg->to->parsed;
		DBG("PRESENCE: handle_subscribe: 'To' header ALREADY PARSED: <%.*s>\n",
				pto->uri.len, pto->uri.s );	
	}
	else
	{
		memset( &TO , 0, sizeof(TO) );
		if( !parse_to(msg->to->body.s,msg->to->body.s + msg->to->body.len + 1, &TO));
		{
			DBG("PRESENCE: handle_subscribe: 'To' header NOT parsed\n");
			goto error;
		}
		pto = &TO;
	}
	
	if(parse_uri(pto->uri.s, pto->uri.len, &to_uri)!=0)
	{
		LOG(L_ERR, "PRESENCE: handle_subscribe: bad R-URI!\n");
		goto error;
	}

	if(to_uri.user.len<=0 || to_uri.user.s==NULL || to_uri.host.len<=0 ||
			to_uri.host.s==NULL)
	{
		LOG(L_ERR, "PRESENCE: handle_subscribe: bad URI in To header!\n");
		goto error;
	}
	subs.to_user.s = to_uri.user.s;
	subs.to_user.len = to_uri.user.len;

	subs.to_domain.s = to_uri.host.s;
	subs.to_domain.len = to_uri.host.len;
	
	/* examine the from header */
	if (!msg->from || !msg->from->body.s)
	{
		DBG("PRESENCE:handle_subscribe: ERROR cannot find 'from' header!\n");
		goto error;
	}
	if (msg->from->parsed == NULL)
	{
		DBG("PRESENCE:handle_subscribe: 'From' header not parsed\n");
		/* parsing from header */
		if ( parse_from_header( msg )<0 ) 
		{
			DBG("PRESENCE:handle_subscribe: ERROR cannot parse From header\n");
			goto error;
		}
	}
	pfrom = (struct to_body*)msg->from->parsed;

	if(parse_uri(pfrom->uri.s, pfrom->uri.len, &from_uri)!=0)
	{
		LOG(L_ERR, "PRESENCE: handle_subscribe: bad R-URI!\n");
		goto error;
	}

	if(from_uri.user.len<=0 || from_uri.user.s==NULL || from_uri.host.len<=0 ||
			from_uri.host.s==NULL)
	{
		LOG(L_ERR, "PRESENCE: handle_subscribe: bad URI in To header!\n");
		goto error;
	}
	
	subs.from_user.s = from_uri.user.s;
	subs.from_user.len = from_uri.user.len;

	subs.from_domain.s = from_uri.host.s;
	subs.from_domain.len = from_uri.host.len;

	/*generate to_tag if the message does not have a to_tag*/
	if (pto->tag_value.s==NULL || pto->tag_value.len==0 )
	{  
		DBG("PRESENCE:handle_subscribe: generating to_tag\n");
		to_tag_gen = 1;
		/*generate to_tag then insert it in avp*/
		
		rtag_value.s = buf;
		rtag_value.len = sprintf(rtag_value.s,"%s.%d.%d.%d", to_tag_pref,
				pid, (int)time(NULL), counter);
		if(rtag_value.len<= 0)
		{
			LOG(L_ERR, "PRESENCE: handle_subscribe: ERROR while creating"
					" to_tag\n");
			goto error;
		}
	}
	else
	{
		rtag_value=pto->tag_value;
	}
	subs.to_tag = rtag_value;

	if( msg->callid==NULL || msg->callid->body.s==NULL)
	{
		LOG(L_ERR, "PRESENCE: handle_subscribe: ERROR cannot parse callid"
				" header\n");
		goto error;
	}
	subs.callid.s = msg->callid->body.s;
	subs.callid.len = msg->callid->body.len;

	if( msg->cseq==NULL || msg->cseq->body.s==NULL)
	{
		LOG(L_ERR, "PRESENCE: handle_subscribe: ERROR cannot parse cseq"
				" header\n");
		goto error;
	}
	if (str2int( &(get_cseq(msg)->number), &subs.remote_cseq)!=0 )
	{
		LOG(L_ERR, "PRESENCE: handle_subscribe: ERROR cannot parse cseq"
				" number\n");
		goto error;
	}
	if( msg->contact==NULL || msg->contact->body.s==NULL)
	{
		LOG(L_ERR, "PRESENCE: handle_subscribe: ERROR cannot parse contact"
				" header\n");
		goto error;
	}
	if( parse_contact(msg->contact) <0 )
	{
		LOG(L_ERR, "PRESENCE: handle_subscribe: ERROR cannot parse contact"
				" header\n");
		goto error;
	}
	b= (contact_body_t* )msg->contact->parsed;

	if(b == NULL)
	{
		LOG(L_ERR, "PRESENCE: handle_subscribe: ERROR cannot parse contact"
				" header\n");
		goto error;
	}
	subs.contact.s = b->contacts->uri.s;
	subs.contact.len = b->contacts->uri.len;

	DBG("PRESENCE: handle_subscribe: subs.contact= %.*s - len = %d\n",
			subs.contact.len, subs.contact.s, subs.contact.len);	

	/*process record route and add it to a string*/
	if (msg->record_route!=NULL)
	{
		rt = print_rr_body(msg->record_route, &rec_route, 0, 0);
		if(rt != 0)
		{
			LOG(L_ERR,"PRESENCE:handle_subscribe:error processing the record"
					" route [%d]\n", rt);	
			rec_route.s=NULL;
			rec_route.len=0;
		//	goto error;
		}
	}
	subs.record_route.s = rec_route.s;
	subs.record_route.len = rec_route.len;
			
	subs.sockinfo_str= msg->rcv.bind_address->sock_str;

	if( pfrom->tag_value.s ==NULL || pfrom->tag_value.len == 0)
	{
		LOG(L_ERR, "PRESENCE: handle_subscribe: ERROR no from tag value"
				" present\n");
		goto error;
	}
	subs.from_tag.s = pfrom->tag_value.s;
	subs.from_tag.len = pfrom->tag_value.len;

	subs.version = 0;
	
	if((!server_address.s) || (server_address.len== 0))
	{
		contact= get_local_contact(msg);
		if(contact== NULL)
		{
			LOG(L_ERR, "PRESENCE: handle_subscribe: ERROR in function"
					" get_local_contact\n");
			goto error;
		}
		subs.local_contact= *contact;
	}
	else
		subs.local_contact= server_address;

	/* getting presentity uri from Request-URI if initial subscribe - or else from database*/
	if(to_tag_gen)
	{
		subs.pres_uri= msg->first_line.u.request.uri;
	}
	else
	{
		if(get_stored_info(msg, &subs, &error_ret )< 0)
		{
			LOG(L_ERR, "PRESENCE: handle_subscribe:error while getting"
					" stored info\n");
			goto error;
		}
	}	

	/* call event specific subscription handling */
	if(event->evs_subs_handl)
	{
		if(event->evs_subs_handl(msg)< 0)
		{
			LOG(L_ERR, "PRESENCE: handle_subscribe: ERROR in event specific"
					" subscription handling\n");
			goto error;
		}
	}	

	/* particular case for expires= 0*/	
	if( subs.expires== 0 )
	{
		subs.status= TERMINATED_STATUS;
		subs.reason.s= "timeout";
		subs.reason.len= 7;
		goto after_status;	
	}	

	/* subscription status handling */
	db_keys[n_query_cols] ="p_uri";
	db_vals[n_query_cols].type = DB_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val= subs.pres_uri;
	n_query_cols++;

	db_keys[n_query_cols] ="w_user";
	db_vals[n_query_cols].type = DB_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val = subs.from_user;
	n_query_cols++;

	db_keys[n_query_cols] ="w_domain";
	db_vals[n_query_cols].type = DB_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val = subs.from_domain;
	n_query_cols++;
	
	db_keys[n_query_cols] ="event";
	db_vals[n_query_cols].type = DB_STR;
	db_vals[n_query_cols].nul = 0;
	db_vals[n_query_cols].val.str_val = subs.event->name;
	n_query_cols++;

	result_cols[0] = "subs_status";
	result_cols[1] = "reason";

	if(pa_dbf.use_table(pa_db, watchers_table)< 0)
	{
		LOG(L_ERR,"PRESENCE:handle_subscribe: ERROR in use table\n");
		goto error;
	}	

	if(pa_dbf.query(pa_db, db_keys, 0, db_vals, result_cols,
					n_query_cols, 2, 0, &result )< 0)
	{
		LOG(L_ERR, "PRESENCE:handle_subscribe: ERROR while querying"
				" watchers table\n");
		if(result)
			pa_dbf.free_result(pa_db, result);
		goto error;
	}
	if(result== NULL)
		goto error;
	
	result_n= result->n;
	if(result_n> 0)
	{	
		row = &result->rows[0];
		row_vals = ROW_VALUES(row);
		status= row_vals[0].val.int_val;

		if(row_vals[1].val.string_val)
		{
			reason.len= strlen(row_vals[1].val.string_val);
			if(reason.len== 0)
				reason.s= NULL;
			else
			{
				reason.s= (char*)pkg_malloc(reason.len*sizeof(char));
				if(reason.s== NULL)
				{
					LOG(L_ERR, "PRESENCE:handle_subscribe: ERORR No more memory"
							" when allocating reason\n");
					pa_dbf.free_result(pa_db, result);
					goto error;		
				}		
				memcpy(reason.s, row_vals[1].val.string_val, reason.len);
			}
		}
	}	
	pa_dbf.free_result(pa_db, result);
	subs.status= status;
	subs.reason= reason;
	
	/* get subs.status */

	if(!event->req_auth)
	{
		subs.status = ACTIVE_STATUS;
		/* if record noes not exist in watchers_table insert */
		if(result_n<= 0)
		{
			db_keys[n_query_cols] ="subs_status";
			db_vals[n_query_cols].type = DB_INT;
			db_vals[n_query_cols].nul = 0;
			db_vals[n_query_cols].val.int_val = ACTIVE_STATUS;
			n_query_cols++;
								
			db_keys[n_query_cols] = "inserted_time";
			db_vals[n_query_cols].type = DB_INT;
			db_vals[n_query_cols].nul = 0;
			db_vals[n_query_cols].val.int_val= (int)time(NULL);
			n_query_cols++;
			
			if(pa_dbf.insert(pa_db, db_keys, db_vals, n_query_cols )< 0)
			{	
				LOG(L_ERR, "PRESENCE: subscribe:ERROR while updating watchers table\n");
				goto error;
			}
		}	
	}
	else    /* check authorization rules or take status from 'watchers' table */
	{
		result_code= subs.event->is_watcher_allowed(&subs);
		if(result_code< 0)
		{
			LOG(L_ERR, "PRESENCE: subscribe: ERROR in event specific function"
					" is_watcher_allowed\n");
			goto error;
		}	
		if(result_code!=0)	/* a change has ocured */
		{
			if(result_n <=0)
			{
				db_keys[n_query_cols] ="subs_status";
				db_vals[n_query_cols].type = DB_INT;
				db_vals[n_query_cols].nul = 0;
				db_vals[n_query_cols].val.int_val = subs.status;
				n_query_cols++;
					
				if(subs.reason.s && subs.reason.len)
				{
					db_keys[n_query_cols] ="reason";
					db_vals[n_query_cols].type = DB_STR;
					db_vals[n_query_cols].nul = 0;
					db_vals[n_query_cols].val.str_val = subs.reason;
					n_query_cols++;	
				}	
				
				db_keys[n_query_cols] = "inserted_time";
				db_vals[n_query_cols].type = DB_INT;
				db_vals[n_query_cols].nul = 0;
				db_vals[n_query_cols].val.int_val= (int)time(NULL);
				n_query_cols++;

				if(pa_dbf.insert(pa_db, db_keys, db_vals, n_query_cols )< 0)
				{	
					LOG(L_ERR, "PRESENCE: handle_subscribe:ERROR while updating watchers table\n");
					goto error;
				}
			}
			else
			{	/* update if different */
				if((status!= subs.status)||	(reason.s && subs.reason.s &&
						strncmp(reason.s, subs.reason.s, reason.len)))
				{										
					int n_update_cols= 0;

					update_keys[0]="subs_status";
					update_vals[0].type = DB_INT;
					update_vals[0].nul = 0;
					update_vals[0].val.int_val= subs.status;
					n_update_cols++;

					if(subs.reason.s && subs.reason.len)
					{
						update_keys[1]="reason";
						update_vals[1].type = DB_STR;
						update_vals[1].nul = 0;
						update_vals[1].val.str_val= subs.reason;
						n_update_cols++;
					}	
					
					if(pa_dbf.update(pa_db, db_keys, 0, db_vals, 
								update_keys, update_vals, n_query_cols, n_update_cols)< 0)
					{
						LOG(L_ERR, "PRESENCE:handle_subscribe: ERORR while"
								" updating database table\n");
						goto error;
					}
				}
			}
		}
		else	/* if nothing is known put status "pending" */
		{	
			if(result_n<= 0)
			{
				subs.status= PENDING_STATUS;
				
				db_keys[n_query_cols] ="subs_status";
				db_vals[n_query_cols].type = DB_INT;
				db_vals[n_query_cols].nul = 0;
				db_vals[n_query_cols].val.int_val = subs.status;
				n_query_cols++;
					
				db_keys[n_query_cols] = "inserted_time";
				db_vals[n_query_cols].type = DB_INT;
				db_vals[n_query_cols].nul = 0;
				db_vals[n_query_cols].val.int_val= (int)time(NULL);
				n_query_cols++;

				if(pa_dbf.insert(pa_db, db_keys, db_vals, n_query_cols )< 0)
				{	
					LOG(L_ERR, "PRESENCE: handle_subscribe:ERROR while updating watchers table\n");
					goto error;
				}

			}
			
		}
	}

after_status:

	if( update_subscription(msg, &subs, &rtag_value, to_tag_gen) <0 )
	{	
		LOG(L_ERR,"PRESENCE:handle_subscribe: ERROR while updating database\n");
		goto error;
	}


	if(reason.s )
			pkg_free(reason.s);
	
	if(subs.record_route.s)
		pkg_free(subs.record_route.s);
	
	if(to_tag_gen== 0)
	{
		if(subs.pres_uri.s)
			pkg_free(subs.pres_uri.s);
	}
	if(contact)
	{	
		if(contact->s)
			pkg_free(contact->s);
		pkg_free(contact);
	}
	return 1;

bad_event:

	LOG(L_ERR, "PRESENCE: handle_subscribe:Missing or unsupported event"
		" header field value");
		
	if(parsed_event)
		LOG(L_ERR," event= %.*s\n",parsed_event->text.len,parsed_event->text.s);
	else
		LOG(L_ERR,"\n");
	
	if(reply_bad_event(msg)< 0)
		return -1;

	error_ret = 0;

error:

	if(subs.record_route.s)
		pkg_free(subs.record_route.s);

	if(to_tag_gen== 0)
	{
		if(subs.pres_uri.s)
			pkg_free(subs.pres_uri.s);
		}
	if(contact)
	{	
		if(contact->s)
			pkg_free(contact->s);
		pkg_free(contact);
	}

	return error_ret;

}

int get_stored_info(struct sip_msg* msg, subs_t* subs, int* error_ret)
{	
	str pres_uri= {0, 0};
	subs_t* s;
	int i;
	unsigned int hash_code;

	*error_ret= -1;

	DBG("PRESENCE:get_stored_info ...\n");
	/* first try to_user== pres_user and to_domain== pres_domain */

	uandd_to_uri(subs->to_user, subs->to_domain, &pres_uri);
	if(pres_uri.s== NULL)
	{
		LOG(L_ERR, "PRESENCE: get_stored_info: ERROR when creating uri"
				" from user and domain\n");
		return -1;
	}
	hash_code= core_hash(&pres_uri, &subs->event->name, shtable_size);
	lock_get(&subs_htable[hash_code].lock);
	i= hash_code;
	s= search_shtable(subs->callid, subs->to_tag, subs->from_tag, hash_code);
	if(s)
	{
		goto found_rec;
	}
	lock_release(&subs_htable[hash_code].lock);

	pkg_free(pres_uri.s);
	pres_uri.s= NULL;
	DBG("PRESENCE: get_stored_info: record not found using R-URI"
			"-> search iteratively\n");
	/* take one row at a time */
	for(i= 0; i< shtable_size; i++)
	{
		lock_get(&subs_htable[i].lock);
		s= search_shtable(subs->callid,subs->to_tag,subs->from_tag, i);
		if(s)
		{
			pres_uri.s= (char*)pkg_malloc(s->pres_uri.len* sizeof(char));
			memcpy(pres_uri.s, s->pres_uri.s, s->pres_uri.len);
			pres_uri.len= s->pres_uri.len;
			goto found_rec;
		}
		lock_release(&subs_htable[i].lock);
	}

	if(fallback2db)
	{
		return get_database_info(msg, subs, error_ret);	
	}

	LOG(L_ERR, "PRESENCE: get_stored_info: ERROR Record not found"
			" in hash_table\n");
	if (slb.reply(msg, 481, &pu_481_rpl) == -1)
	{
		LOG(L_ERR, "PRESENCE: get_stored_info: ERROR while"
				" sending reply\n");
		return -1;
	}
	*error_ret= 0;
	return -1;

found_rec:
	DBG("PRESENCE: get_stored_info: Record found in hash_table\n");
	subs->pres_uri= pres_uri;
	subs->local_cseq= s->local_cseq;
	if(subs->remote_cseq<= s->remote_cseq)
	{
		LOG(L_ERR, "PRESENCE: get_stored_info: ERROR wrong sequence number"
			" received: %d -  stored: %d\n",subs->remote_cseq, s->remote_cseq);
		if (slb.reply(msg, 400, &pu_400_rpl) == -1)
		{
			LOG(L_ERR, "PRESENCE: get_stored_info: ERROR while"
					" sending reply\n");
		}
		*error_ret= 0;
		lock_release(&subs_htable[i].lock);
		if(pres_uri.s)
			pkg_free(pres_uri.s);
		return -1;
	}	
	lock_release(&subs_htable[i].lock);

	return 0;
}

int get_database_info(struct sip_msg* msg, subs_t* subs, int* error_ret)
{	
	db_key_t query_cols[10];
	db_val_t query_vals[10];
	db_key_t result_cols[5];
	db_res_t *result= NULL;
	db_row_t *row ;	
	db_val_t *row_vals ;
	int n_query_cols = 0;
	int n_result_cols = 0;
	int remote_cseq_col= 0, local_cseq_col= 0;
	int pres_uri_col;
	unsigned int remote_cseq;
	str pres_uri;	

	*error_ret= -1;

	query_cols[n_query_cols] = "to_user";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->to_user;
	n_query_cols++;
	
	query_cols[n_query_cols] = "to_domain";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->to_domain;
	n_query_cols++;

	query_cols[n_query_cols] = "from_user";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->from_user;
	n_query_cols++;
	
	query_cols[n_query_cols] = "from_domain";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->from_domain;
	n_query_cols++;

	query_cols[n_query_cols] = "event";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->event->name;
	n_query_cols++;

	query_cols[n_query_cols] = "event_id";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	if( subs->event_id.s != NULL)
	{
		query_vals[n_query_cols].val.str_val.s = subs->event_id.s;
		query_vals[n_query_cols].val.str_val.len = subs->event_id.len;
	} else {
		query_vals[n_query_cols].val.str_val.s = "";
		query_vals[n_query_cols].val.str_val.len = 0;
	}
	n_query_cols++;
	
	query_cols[n_query_cols] = "callid";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->callid;
	n_query_cols++;

	query_cols[n_query_cols] = "to_tag";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->to_tag;
	n_query_cols++;

	query_cols[n_query_cols] = "from_tag";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->from_tag;
	n_query_cols++;

	result_cols[pres_uri_col=n_result_cols++] = "pres_uri";
	result_cols[remote_cseq_col=n_result_cols++] = "remote_cseq";
	result_cols[local_cseq_col=n_result_cols++] = "local_cseq";

	if (pa_dbf.use_table(pa_db, active_watchers_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE: get_database_info: ERROR in use_table\n");
		return -1;
	}
	
	DBG("PRESENCE: get_database_info: querying database  \n");
	if (pa_dbf.query (pa_db, query_cols, 0, query_vals,
		 result_cols, n_query_cols, n_result_cols, 0,  &result) < 0) 
	{
		LOG(L_ERR, "PRESENCE: get_database_info: ERROR while querying"
				" presentity\n");
		if(result)
			pa_dbf.free_result(pa_db, result);
		return -1;
	}
	if(result== NULL)
		return -1;

	if(result && result->n <=0)
	{
		LOG(L_ERR, "PRESENCE: get_database_info: The query returned"
				" no result\n");
		
		if (slb.reply(msg, 481, &pu_481_rpl) == -1)
		{
			LOG(L_ERR, "PRESENCE: get_database_info: ERROR while"
					" sending reply\n");
			pa_dbf.free_result(pa_db, result);
			return -1;
		}
		pa_dbf.free_result(pa_db, result);
		*error_ret= 0;
		return -1;
	}

	row = &result->rows[0];
	row_vals = ROW_VALUES(row);
	subs->local_cseq= row_vals[local_cseq_col].val.int_val;
	remote_cseq= row_vals[remote_cseq_col].val.int_val;
	
	if(subs->remote_cseq<= remote_cseq)
	{
		LOG(L_ERR, "PRESENCE: get_database_info: ERROR wrong sequence number"
				" received: %d -  stored: %d\n",subs->remote_cseq, remote_cseq);
		if (slb.reply(msg, 400, &pu_400_rpl) == -1)
		{
			LOG(L_ERR, "PRESENCE: get_database_info: ERROR while"
					" sending reply\n");
		}
		pa_dbf.free_result(pa_db, result);
		*error_ret= 0;
		return -1;
	}
	
	pres_uri.s= (char*)row_vals[pres_uri_col].val.string_val;
	pres_uri.len= strlen(pres_uri.s);
	subs->pres_uri.s= (char*)pkg_malloc(pres_uri.len);
	memcpy(subs->pres_uri.s, pres_uri.s, pres_uri.len);
	subs->pres_uri.len= pres_uri.len;

	pa_dbf.free_result(pa_db, result);
	result= NULL;

	return 0;

}


int handle_expired_subs(subs_t* s, subs_t* prev_s)
{
	/* send Notify with state=terminated;reason=timeout */
	DBG("PRESENCE:handle_expired_subs ...\n");
	
	s->status= TERMINATED_STATUS;
	s->reason.s= "timeout";
	s->reason.len= 7;
	s->expires= 0;

	if(send_notify_request(s, NULL, NULL, 0)< 0)
	{
		LOG(L_ERR, "PRESENCE:handle_expired_subs:"
				"ERROR send Notify not successful\n");
		return -1;
	}
	
	return 0;

}

void timer_db_update(unsigned int ticks,void *param)
{
	db_key_t query_cols[21], update_cols[6], result_cols[6];
	db_val_t query_vals[21], update_vals[6];
	db_op_t update_ops[1];
	subs_t* del_s;

	int pres_uri_col, to_user_col, to_domain_col, from_user_col, from_domain_col,
		callid_col, totag_col, fromtag_col, event_col,status_col, event_id_col, 
		local_cseq_col, remote_cseq_col, expires_col, record_route_col, 
		contact_col, local_contact_col, version_col, socket_info_col;
	int u_expires_col, u_local_cseq_col, u_remote_cseq_col, u_version_col,
		u_status_col; 
	int no_lock= 0, i;
	subs_t* s= NULL, *prev_s= NULL;
	int n_query_cols= 0, n_update_cols= 0;
	int n_query_update;

	if(ticks== 0 && param == NULL)
		no_lock= 1;

	DBG("PRESENCE: timer_db_update ...\n");
	query_cols[pres_uri_col= n_query_cols] ="pres_uri";
	query_vals[pres_uri_col].type = DB_STR;
	query_vals[pres_uri_col].nul = 0;
	n_query_cols++;
	
	query_cols[callid_col= n_query_cols] ="callid";
	query_vals[callid_col].type = DB_STR;
	query_vals[callid_col].nul = 0;
	n_query_cols++;

	query_cols[totag_col= n_query_cols] ="to_tag";
	query_vals[totag_col].type = DB_STR;
	query_vals[totag_col].nul = 0;
	n_query_cols++;

	query_cols[fromtag_col= n_query_cols] ="from_tag";
	query_vals[fromtag_col].type = DB_STR;
	query_vals[fromtag_col].nul = 0;
	n_query_cols++;

	n_query_update= n_query_cols;

	query_cols[to_user_col= n_query_cols] ="to_user";
	query_vals[to_user_col].type = DB_STR;
	query_vals[to_user_col].nul = 0;
	n_query_cols++;

	query_cols[to_domain_col= n_query_cols] ="to_domain";
	query_vals[to_domain_col].type = DB_STR;
	query_vals[to_domain_col].nul = 0;
	n_query_cols++;
	
	query_cols[from_user_col= n_query_cols] ="from_user";
	query_vals[from_user_col].type = DB_STR;
	query_vals[from_user_col].nul = 0;
	n_query_cols++;

	query_cols[from_domain_col= n_query_cols] ="from_domain";
	query_vals[from_domain_col].type = DB_STR;
	query_vals[from_domain_col].nul = 0;
	n_query_cols++;

	query_cols[event_col= n_query_cols] ="event";
	query_vals[event_col].type = DB_STR;
	query_vals[event_col].nul = 0;
	n_query_cols++;	

	query_cols[event_id_col= n_query_cols] ="event_id";
	query_vals[event_id_col].type = DB_STR;
	query_vals[event_id_col].nul = 0;
	n_query_cols++;

	query_cols[local_cseq_col= n_query_cols]="local_cseq";
	query_vals[local_cseq_col].type = DB_INT;
	query_vals[local_cseq_col].nul = 0;
	n_query_cols++;

	query_cols[remote_cseq_col= n_query_cols]="remote_cseq";
	query_vals[remote_cseq_col].type = DB_INT;
	query_vals[remote_cseq_col].nul = 0;
	n_query_cols++;

	query_cols[expires_col= n_query_cols] ="expires";
	query_vals[expires_col].type = DB_INT;
	query_vals[expires_col].nul = 0;
	n_query_cols++;

	query_cols[status_col= n_query_cols] ="status";
	query_vals[status_col].type = DB_INT;
	query_vals[status_col].nul = 0;
	n_query_cols++;

	query_cols[record_route_col= n_query_cols] ="record_route";
	query_vals[record_route_col].type = DB_STR;
	query_vals[record_route_col].nul = 0;
	n_query_cols++;
	
	query_cols[contact_col= n_query_cols] ="contact";
	query_vals[contact_col].type = DB_STR;
	query_vals[contact_col].nul = 0;
	n_query_cols++;

	query_cols[local_contact_col= n_query_cols] ="local_contact";
	query_vals[local_contact_col].type = DB_STR;
	query_vals[local_contact_col].nul = 0;
	n_query_cols++;

	query_cols[socket_info_col= n_query_cols] ="socket_info";
	query_vals[socket_info_col].type = DB_STR;
	query_vals[socket_info_col].nul = 0;
	n_query_cols++;

	query_cols[version_col= n_query_cols]="version";
	query_vals[version_col].type = DB_INT;
	query_vals[version_col].nul = 0;
	n_query_cols++;

	/* cols and values used for update */
	update_cols[u_expires_col= n_update_cols]= "expires";
	update_vals[u_expires_col].type = DB_INT;
	update_vals[u_expires_col].nul = 0;
	n_update_cols++;

	update_cols[u_status_col= n_update_cols]= "status";
	update_vals[u_status_col].type = DB_INT;
	update_vals[u_status_col].nul = 0;
	n_update_cols++;

	update_cols[u_remote_cseq_col= n_update_cols]= "remote_cseq";
	update_vals[u_remote_cseq_col].type = DB_INT;
	update_vals[u_remote_cseq_col].nul = 0;
	n_update_cols++;

	update_cols[u_local_cseq_col= n_update_cols]= "local_cseq";
	update_vals[u_local_cseq_col].type = DB_INT;
	update_vals[u_local_cseq_col].nul = 0;
	n_update_cols++;
	
	update_cols[u_version_col= n_update_cols]= "version";
	update_vals[u_version_col].type = DB_INT;
	update_vals[u_version_col].nul = 0;
	n_update_cols++;

	result_cols[0]= "expires";

	if(pa_db== NULL)
	{
		LOG(L_ERR,"RLS: active_watchers_table_update:ERROR null database connection\n");
		return;
	}
	if(pa_dbf.use_table(pa_db, active_watchers_table)< 0)
	{
		LOG(L_ERR, "RLS: active_watchers_table_update:ERROR in use table\n");
		return ;
	}

	for(i=0; i<shtable_size; i++) 
	{
		if(!no_lock)
			lock_get(&subs_htable[i].lock);	

		prev_s= subs_htable[i].entries;
		s= prev_s->next;
	
		while(s)
		{
	//		printf_subs(s);
			if(s->expires <= (int)time(NULL))	
			{
				DBG("PRESENCE:timer_db_update: Found expired record\n");
				if(!no_lock)
				{
					if(handle_expired_subs(s, prev_s)< 0)
					{
						LOG(L_ERR, "PRESENCE:timer_db_update: ERROR in function"
								" handle_expired_record\n");
						if(!no_lock)
							lock_release(&subs_htable[i].lock);	
						return ;
					}
				}
				del_s= s;	
				s= s->next;
				prev_s->next= s;
				
				shm_free(del_s);
				continue;
			}
			switch(s->db_flag)
			{
				case NO_UPDATEDB_FLAG:
				{
					DBG("PRESENCE:timer_db_update: NO_UPDATEDB_FLAG\n");
					break;			  
				}
				case UPDATEDB_FLAG:
				{
					DBG("PRESENCE:timer_db_update: UPDATEDB_FLAG\n ");

					query_vals[pres_uri_col].val.str_val= s->pres_uri;
					query_vals[callid_col].val.str_val= s->callid;
					query_vals[totag_col].val.str_val= s->to_tag;
					query_vals[fromtag_col].val.str_val= s->from_tag;
				
					update_vals[u_expires_col].val.int_val= s->expires;
					update_vals[u_local_cseq_col].val.int_val= s->local_cseq;
					update_vals[u_remote_cseq_col].val.int_val= s->remote_cseq;
					update_vals[u_version_col].val.int_val= s->version;
					update_vals[u_status_col].val.int_val= s->status;

					if(pa_dbf.update(pa_db, query_cols, 0, query_vals, update_cols, 
								update_vals, n_query_update, n_update_cols)< 0)
					{
						LOG(L_ERR, "PRESENCE:timer_db_update: ERROR while updating"
								" in database\n");
						if(!no_lock)
							lock_release(&subs_htable[i].lock);	
						return ;
					}
					break;
				}
				case  INSERTDB_FLAG:
				{
					DBG("PRESENCE:timer_db_update: INSERTDB_FLAG\n ");

					query_vals[pres_uri_col].val.str_val= s->pres_uri;
					query_vals[callid_col].val.str_val= s->callid;
					query_vals[totag_col].val.str_val= s->to_tag;
					query_vals[fromtag_col].val.str_val= s->from_tag;
					query_vals[to_user_col].val.str_val = s->to_user;
					query_vals[to_domain_col].val.str_val = s->to_domain;
					query_vals[from_user_col].val.str_val = s->from_user;
					query_vals[from_domain_col].val.str_val = s->from_domain;
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
					query_vals[socket_info_col].val.str_val= s->sockinfo_str;
				
					if(pa_dbf.insert(pa_db,query_cols,query_vals,n_query_cols )<0)
					{
						LOG(L_ERR, "PRESENCE:timer_db_update: ERROR in sql insert\n");
						if(!no_lock)
							lock_release(&subs_htable[i].lock);
						return ;
					}
					break;										
				}

			}
			s->db_flag= NO_UPDATEDB_FLAG;	
			prev_s= s;
			s= s->next;
		}
		if(!no_lock)
			lock_release(&subs_htable[i].lock);	
	}

	update_vals[0].val.int_val= (int)time(NULL)- 10;
	update_ops[0]= OP_LT;
	if(pa_dbf.delete(pa_db, update_cols, update_ops, update_vals, 1) < 0)
	{
		LOG(L_ERR,"PRESENCE:timer_db_update: ERROR cleaning expired"
				" information\n");
	}

}

int restore_db_subs()
{
	db_key_t result_cols[21]; 
	db_res_t *res= NULL;
	db_row_t *row = NULL;	
	db_val_t *row_vals= NULL;
	int i;
	int n_result_cols= 0;
	int pres_uri_col, expires_col, from_user_col, from_domain_col,to_user_col; 
	int callid_col,totag_col,fromtag_col,to_domain_col, sockinfo_col;
	int event_col,contact_col,record_route_col, event_id_col, status_col;
	int remote_cseq_col, local_cseq_col, local_contact_col, version_col;
	subs_t s;
	str ev_sname;
	pres_ev_t* event= NULL;
	event_t parsed_event;
	unsigned int expires;

	result_cols[pres_uri_col=n_result_cols++]	="pres_uri";		
	result_cols[expires_col=n_result_cols++]="expires";
	result_cols[event_col=n_result_cols++]	="event";
	result_cols[event_id_col=n_result_cols++]="event_id";
	result_cols[to_user_col=n_result_cols++]	="to_user";
	result_cols[to_domain_col=n_result_cols++]	="to_domain";
	result_cols[from_user_col=n_result_cols++]	="from_user";
	result_cols[from_domain_col=n_result_cols++]="from_domain";
	result_cols[callid_col=n_result_cols++] ="callid";
	result_cols[totag_col=n_result_cols++]	="to_tag";
	result_cols[fromtag_col=n_result_cols++]="from_tag";
	result_cols[local_cseq_col= n_result_cols++]	="local_cseq";
	result_cols[remote_cseq_col= n_result_cols++]	="remote_cseq";
	result_cols[record_route_col= n_result_cols++]	="record_route";
	result_cols[sockinfo_col= n_result_cols++]	="socket_info";
	result_cols[contact_col= n_result_cols++]	="contact";
	result_cols[local_contact_col= n_result_cols++]	="local_contact";
	result_cols[version_col= n_result_cols++]	="version";
	result_cols[status_col= n_result_cols++]	="status";
	
	if(!pa_db)
	{
		LOG(L_ERR,"PRESENCE:restore_db_subs: ERROR null database connection\n");
		return -1;
	}
	if(pa_dbf.use_table(pa_db, active_watchers_table)< 0)
	{
		LOG(L_ERR, "PRESENCE:restore_db_subs:ERROR in use table\n");
		return -1;
	}

	if(pa_dbf.query(pa_db,0, 0, 0, result_cols,0, n_result_cols, 0,&res)< 0)
	{
		LOG(L_ERR, "PRESENCE:restore_db_subs:ERROR while querrying table\n");
		if(res)
		{
			pa_dbf.free_result(pa_db, res);
			res = NULL;
		}
		return -1;
	}
	if(res== NULL)
		return -1;

	if(res->n<=0)
	{
		LOG(L_INFO, "PRESENCE:restore_db_subs:The query returned no result\n");
		pa_dbf.free_result(pa_db, res);
		res = NULL;
		return 0;
	}

	DBG("PRESENCE:restore_db_subs: found %d db entries\n", res->n);

	for(i =0 ; i< res->n ; i++)
	{
		row = &res->rows[i];
		row_vals = ROW_VALUES(row);
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
			DBG("PRESENCE:restore_db_subs:insert a new event structure in the "
					"list waiting to be filled in\n");
			/*insert a new event structure in the list waiting to be filled in*/
			event= (pres_ev_t*)shm_malloc(sizeof(pres_ev_t));
			if(event== NULL)
			{
				ERR_MEM("restore_db_subs");
			}
			memset(event, 0, sizeof(pres_ev_t));
			event->name.s= (char*)shm_malloc(ev_sname.len* sizeof(char));
			if(event->name.s== NULL)
			{
				ERR_MEM("restore_db_subs");
			}
			memcpy(event->name.s,ev_sname.s, ev_sname.len);
			event->name.len= ev_sname.len;
			
			event->evp= shm_copy_event(&parsed_event);
			if(event->evp== NULL)
			{
				LOG(L_ERR, "PRESENCE:restore_db_subs: ERROR copying event_t"
						" structure\n");
				goto error;
			}

			event->next= EvList->events;
			EvList->events= event;
		}
		s.event= event;

		s.event_id.s=(char*)row_vals[event_id_col].val.string_val;
		if(s.event_id.s)
			s.event_id.len= strlen(s.event_id.s);

		s.remote_cseq= row_vals[remote_cseq_col].val.int_val;
		s.local_cseq= row_vals[local_cseq_col].val.int_val;
		s.version= row_vals[version_col].val.int_val;
		
		s.expires= expires- (int)time(NULL);
		s.status= row_vals[status_col].val.int_val;

		s.contact.s=(char*)row_vals[contact_col].val.string_val;
		s.contact.len= strlen(s.contact.s);

		s.local_contact.s=(char*)row_vals[local_contact_col].val.string_val;
		s.local_contact.len= strlen(s.local_contact.s);
	
		s.record_route.s=(char*)row_vals[record_route_col].val.string_val;
		if(s.record_route.s)
			s.record_route.len= strlen(s.record_route.s);
	
		s.sockinfo_str.s=(char*)row_vals[sockinfo_col].val.string_val;
		s.sockinfo_str.len= strlen(s.sockinfo_str.s);

		if(insert_shtable(&s)< 0)
		{
			LOG(L_ERR, "PRESENCE:restore_db_subs: ERROR while" 
					"adding new record in hash table\n");
			goto error;
		}
	}

	pa_dbf.free_result(pa_db, res);

	/* delete all records */
	if(pa_dbf.delete(pa_db, 0,0,0,0)< 0)
	{
		LOG(L_ERR, "PRESENCE:restore_db_subs:ERROR when deleting all records"
				" from rl_presentity table\n");
		return -1;
	}

	return 0;

error:
	if(res)
		pa_dbf.free_result(pa_db, res);
	return -1;

}

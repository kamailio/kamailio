/*
 * $Id$
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
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
#include "../../parser/contact/parse_contact.h"
#include "presence.h"
#include "subscribe.h"
#include "utils_func.h"
#include "notify.h"


static str su_200_rpl  = str_init("OK");
static str pu_481_rpl  = str_init("Subscription does not exist");
static str pu_400_rpl  = str_init("Bad request");
static str pu_489_rpl  = str_init("Bad Event");

int process_rr(	struct hdr_field *i_route, str *o_route)
{
	rr_t *p;
	int n = 0;
	int i = 0;
	int route_len;
	str route[64];
	char *cp;

	if(i_route==NULL)
		return 0;
	
	route_len=0;
	while (i_route!=NULL) 
	{
		
		if (parse_rr(i_route) < 0) 
		{
			LOG(L_ERR,"PRESENCE:process_rr: ERROR while parsing RR\n");
			goto error;
		}

		p =(rr_t*)i_route->parsed;
		while (p)
		{
			route[n].s = p->nameaddr.name.s;
			route[n].len = p->len;
			route_len+=p->len;
			n++;
			p = p->next;
		}
		i_route = i_route->sibling;
	}

	route_len += --n;
	o_route->s=(char*)pkg_malloc(route_len);
	if(o_route->s==0)
	{
		LOG(L_ERR, "PRESENCE:process_rr: ERROR no more pkg mem\n");
		goto error;
	}
	cp = o_route->s;
	i = 0;
	while (i<=n)
	{
		memcpy( cp, route[i].s, route[i].len );
		cp += route[i].len;
		if (++i<=n)
			*(cp++) = ',';
	}
	o_route->len=route_len;

	DBG("PRESENCE :proces_rr: out rr [%.*s]\n",
			o_route->len, o_route->s);

	return 0;

error:
	return -1;
} 

int send_202ok(struct sip_msg * msg, int lexpire, str *rtag)
{
	static str hdr_append;

	hdr_append.s = (char *)pkg_malloc( sizeof(char)*50);
	if(hdr_append.s == NULL)
	{
		LOG(L_ERR,"ERROR:send_202ok : no more pkg memory\n");
		return -1;
	}
	hdr_append.len = sprintf(hdr_append.s, "Expires: %d\r\n", lexpire);
	
	strncpy(hdr_append.s+hdr_append.len ,"Contact: <", 10);
	hdr_append.len += 10;
	strncpy(hdr_append.s+hdr_append.len, server_address.s, server_address.len);
	hdr_append.len += server_address.len;
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

int send_200ok(struct sip_msg * msg, int lexpire, str *rtag)
{
	static str hdr_append;

	hdr_append.s = (char *)pkg_malloc( sizeof(char)*255);
	if(hdr_append.s == NULL)
	{
		LOG(L_ERR,"ERROR:send_200ok : unable to add lump_rl\n");
		return -1;
	}
	hdr_append.len = sprintf(hdr_append.s, "Expires: %d\r\n", lexpire);
	
	strncpy(hdr_append.s+hdr_append.len ,"Contact: <", 10);
	hdr_append.len += 10;
	strncpy(hdr_append.s+hdr_append.len, server_address.s, server_address.len);
	hdr_append.len += server_address.len;
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

int update_subscribtion(struct sip_msg* msg, subs_t* subs, str *rtag,
		int to_tag_gen)
{	
	db_key_t query_cols[15];
	db_op_t  query_ops[15];
	db_val_t query_vals[15], update_vals[5];
	db_key_t result_cols[4], update_keys[5];
	db_res_t *result;
	
	int n_query_cols = 0;
	int n_result_cols = 0;
	int version_col= 0, i ;

	DBG("PRESENCE: update_subscribtion ...\n");
	printf_subs(subs);	
	
	query_cols[n_query_cols] = "to_user";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = subs->to_user.s;
	query_vals[n_query_cols].val.str_val.len = subs->to_user.len;
	n_query_cols++;
	
	query_cols[n_query_cols] = "to_domain";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = subs->to_domain.s;
	query_vals[n_query_cols].val.str_val.len = subs->to_domain.len;
	n_query_cols++;

	query_cols[n_query_cols] = "from_user";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = subs->from_user.s;
	query_vals[n_query_cols].val.str_val.len = subs->from_user.len;
	n_query_cols++;
	
	query_cols[n_query_cols] = "from_domain";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = subs->from_domain.s;
	query_vals[n_query_cols].val.str_val.len = subs->from_domain.len;
	n_query_cols++;

	query_cols[n_query_cols] = "event";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = subs->event.s;
	query_vals[n_query_cols].val.str_val.len = subs->event.len;
	n_query_cols++;

	query_cols[n_query_cols] = "event_id";
	query_ops[n_query_cols] = OP_EQ;
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
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = subs->callid.s;
	query_vals[n_query_cols].val.str_val.len = subs->callid.len;
	n_query_cols++;

	query_cols[n_query_cols] = "to_tag";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = subs->to_tag.s;
	query_vals[n_query_cols].val.str_val.len = subs->to_tag.len;
	n_query_cols++;

	query_cols[n_query_cols] = "from_tag";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = subs->from_tag.s;
	query_vals[n_query_cols].val.str_val.len = subs->from_tag.len;
	n_query_cols++;
	//	result_cols[status_col=n_result_cols++] = "status" ;
	result_cols[version_col=n_result_cols++] = "version";
	
	if (pa_dbf.use_table(pa_db, active_watchers_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE:update_subscribtion: ERROR in use_table\n");
		goto error;
	}
	
/* update the informations in database*/

	if( to_tag_gen ==0) /*if a SUBSCRIBE within a dialog */
	{
		LOG(L_INFO,"PRESENCE:update_subscribtion: querying database  \n");
		if (pa_dbf.query (pa_db, query_cols, query_ops, query_vals,
			 result_cols, n_query_cols, n_result_cols, 0,  &result) < 0) 
		{
			LOG(L_ERR, "PRESENCE:update_subscribtion: ERROR while querying"
					" presentity\n");
			if(result)
				pa_dbf.free_result(pa_db, result);
			return -1;
		}
		if(result== NULL)
			return -1;

		if(result && result->n <=0)
		{
			LOG(L_ERR, "PRESENCE:update_subscribtion: The query returned"
					" no result\n");
			
			if (slb.reply(msg, 481, &pu_481_rpl) == -1)
			{
				LOG(L_ERR, "PRESENCE: update_subscribtion: ERROR while"
						" sending reply\n");
				pa_dbf.free_result(pa_db, result);
				return -1;
			}
			pa_dbf.free_result(pa_db, result);
			return 0;
		}
		pa_dbf.free_result(pa_db, result);
		
		/* delete previous stored subscribtion if the expires value is 0*/
		if(subs->expires == 0)
		{
			LOG(L_INFO,"PRESENCE:update_subscribtion: expires =0 ->"
					" deleting from database\n");
			if(pa_dbf.delete(pa_db, query_cols, query_ops, query_vals,
						n_query_cols)< 0 )
			{
				LOG(L_ERR,"PRESENCE:update_subscribtion: ERROR cleaning"
						" unsubscribed messages\n");	
			}
			if(subs->event.len == strlen("presence"))
			{	
				if( send_202ok(msg, subs->expires, rtag) <0)
				{
					LOG(L_ERR, "PRESENCE:update_subscribtion:ERROR while"
						" sending 202 OK\n");
					goto error;
				}
		
				if(query_db_notify(&subs->to_user,&subs->to_domain,"presence.winfo",
							NULL, NULL)< 0)
				{
					LOG(L_ERR, "PRESENCE:update_subscribtion:Could not send"
							" notify for presence.winfo\n");
				}
			}	
			else /* if unsubscribe for winfo */
			{
				if( send_200ok(msg, subs->expires, rtag) <0)
				{
					LOG(L_ERR, "PRESENCE:update_subscribtion:ERROR while"
						" sending 202 OK\n");
					goto error;
				}
			}
			return 1;
		}

		/* otherwise update in database and send Subscribe on refresh */

		update_keys[0] = "expires";
		update_vals[0].type = DB_INT;
		update_vals[0].nul = 0;
		update_vals[0].val.int_val = subs->expires + (int)time(NULL);
		
		if( pa_dbf.update( pa_db,query_cols, query_ops, query_vals,
					update_keys, update_vals, n_query_cols,1 )<0) 
		{
			LOG( L_ERR , "PRESENCE:update_subscribtion:ERROR while updating"
					" presence information\n");
			goto error;
		}
	}
	else
	{
		if(subs->expires!= 0)
		{		
			query_cols[n_query_cols] = "contact";
			query_vals[n_query_cols].type = DB_STR;
			query_vals[n_query_cols].nul = 0;
			query_vals[n_query_cols].val.str_val.s = subs->contact.s;
			query_vals[n_query_cols].val.str_val.len = subs->contact.len;
			n_query_cols++;
	
			query_cols[n_query_cols] = "status";
			query_vals[n_query_cols].type = DB_STR;
			query_vals[n_query_cols].nul = 0;
			query_vals[n_query_cols].val.str_val.s = subs->status.s;
			query_vals[n_query_cols].val.str_val.len = subs->status.len;
			n_query_cols++;

			query_cols[n_query_cols] = "cseq";
			query_vals[n_query_cols].type = DB_INT;
			query_vals[n_query_cols].nul = 0;
			query_vals[n_query_cols].val.int_val = subs->cseq;
			n_query_cols++;

			DBG("expires: %d\n", subs->expires);
			query_cols[n_query_cols] = "expires";
			query_vals[n_query_cols].type = DB_INT;
			query_vals[n_query_cols].nul = 0;
			query_vals[n_query_cols].val.int_val = subs->expires +
				(int)time(NULL);
			n_query_cols++;

			if(subs->record_route.s!=NULL && subs->record_route.len!=0)
			{
				query_cols[n_query_cols] = "record_route";
				query_vals[n_query_cols].type = DB_STR;
				query_vals[n_query_cols].nul = 0;
				query_vals[n_query_cols].val.str_val.s = subs->record_route.s;
				query_vals[n_query_cols].val.str_val.len = 
					subs->record_route.len;
				n_query_cols++;
			}


			DBG("PRESENCE:update_subscribtion:Inserting into database:"		
				"\nn_query_cols:%d\n",n_query_cols);
			for(i = 0;i< n_query_cols-2; i++)
			{
				if(query_vals[i].type==DB_STR)
				DBG("[%d] = %s %.*s\n",i, query_cols[i], 
					query_vals[i].val.str_val.len,query_vals[i].val.str_val.s );
				if(query_vals[i].type==DB_INT)
					DBG("[%d] = %s %d\n",i, query_cols[i], 
						query_vals[i].val.int_val);
			}
	
			if (pa_dbf.insert(pa_db, query_cols, query_vals, n_query_cols) < 0) 
			{
				LOG(L_ERR, "PRESENCE:update_subscribtion: ERROR while storing"
						" new subscribtion\n");
				goto error;
			}
		
		}
		/*otherwise there is a subscription outside a dialog with expires= 0 
		 * no update in database, but
		 * should try to send Notify */
		 
	}

/* reply_and_notify  */

	if(subs->event.len == strlen("presence"))
	{	
		if( send_202ok(msg, subs->expires, rtag) <0)
		{
			LOG(L_ERR, "PRESENCE:update_subscribtion:ERROR while"
					" sending 202 OK\n");
			goto error;
		}
		
		if(query_db_notify(&subs->to_user,&subs->to_domain,"presence.winfo",
					subs, NULL)< 0)
		{
			LOG(L_ERR, "PRESENCE:update_subscribtion:Could not send"
					" notify for presence.winfo\n");
		}
		if(subs->send_on_cback== 0)
		{	
			if(notify(subs, NULL, NULL, 0)< 0)
			{
				LOG(L_ERR, "PRESENCE:update_subscribtion: Could not send"
					" notify for presence\n");
			}
		}
		
			
	}
	else /* if a new subscribe for winfo */
	{
		if( send_200ok(msg, subs->expires, rtag) <0)
		{
			LOG(L_ERR, "PRESENCE:update_subscribtion:ERROR while"
					" sending 202 OK\n");
			goto error;
		}		
		if(notify(subs, NULL, NULL, 0 )< 0)
		{
			LOG(L_ERR, "PRESENCE:update_subscribtion: ERROR while"
				" sending notify\n");
		}
	}
	
	return 0;
	
error:

	LOG(L_ERR, "PRESENCE:update_presentity: ERROR occured\n");
	return -1;

}

void msg_watchers_clean(unsigned int ticks,void *param)
{
	db_key_t db_keys[3];
	db_val_t db_vals[3];
	db_op_t  db_ops[3] ;

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

	if (pa_dbf.use_table(pa_db, watchers_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE:msg_watchers_clean: ERROR in use_table\n");
		return ;
	}

	if (pa_dbf.delete(pa_db, db_keys, db_ops, db_vals, 2) < 0) 
		LOG(L_ERR,"PRESENCE:msg_watchers_clean: ERROR cleaning pending "
				" subscriptions\n");
}

void msg_active_watchers_clean(unsigned int ticks,void *param)
{
	db_key_t db_keys[1];
	db_val_t db_vals[1];
	db_op_t  db_ops[1] ;
	db_key_t result_cols[20];
	db_res_t *result;
	db_row_t *row ;	
	db_val_t *row_vals ;
	subs_t subs;

	int n_result_cols = 0;
	int from_user_col, from_domain_col, to_tag_col, from_tag_col;
	int to_user_col, to_domain_col, event_col;
	int callid_col, cseq_col, i, event_id_col = 0;
	int record_route_col = 0, contact_col;
	
	DBG("PRESENCE: msg_active_watchers_clean:cleaning expired watcher information\n");
	
	db_keys[0] ="expires";
	db_ops[0] = OP_LT;
	db_vals[0].type = DB_INT;
	db_vals[0].nul = 0;
	db_vals[0].val.int_val = (int)time(NULL);
	

	result_cols[event_col=n_result_cols++] = "event";
	result_cols[from_user_col=n_result_cols++] = "from_user" ;
	result_cols[from_domain_col=n_result_cols++] = "from_domain" ;
	result_cols[to_user_col=n_result_cols++] = "to_user" ;
	result_cols[to_domain_col=n_result_cols++] = "to_domain" ;
	result_cols[event_id_col=n_result_cols++] = "event_id";
	result_cols[from_tag_col=n_result_cols++] = "from_tag";
	result_cols[to_tag_col=n_result_cols++] = "to_tag";	
	result_cols[callid_col=n_result_cols++] = "callid";
	result_cols[cseq_col=n_result_cols++] = "cseq";
	result_cols[record_route_col=n_result_cols++] = "record_route";
	result_cols[contact_col=n_result_cols++] = "contact";

	if (pa_dbf.use_table(pa_db, active_watchers_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE:msg_active_watchers_clean: ERROR in use_table\n");
		return ;
	}

	if(pa_dbf.query(pa_db, db_keys, db_ops, db_vals, result_cols,
						1, n_result_cols, 0, &result )< 0)
	{
		LOG(L_ERR, "PRESENCE:msg_active_watchers_clean: ERROR while querying database"
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

	for(i=0 ; i< result->n; i++)
	{
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);		
		
		memset(&subs, 0, sizeof(subs_t));

		subs.to_user.s = row_vals[to_user_col].val.str_val.s;
		subs.to_user.len =strlen(row_vals[to_user_col].val.str_val.s);
 
		subs.to_domain.s = row_vals[to_domain_col].val.str_val.s;
		subs.to_domain.len =strlen(row_vals[to_domain_col].val.str_val.s);

		subs.event.s =row_vals[event_col].val.str_val.s;
		subs.event.len = strlen(row_vals[event_col].val.str_val.s);

		subs.from_user.s = row_vals[from_user_col].val.str_val.s;
		subs.from_user.len = strlen(row_vals[from_user_col].val.str_val.s);
		
		subs.from_domain.s = row_vals[from_domain_col].val.str_val.s;
		subs.from_domain.len = strlen(row_vals[from_domain_col].val.str_val.s);
		
		subs.event_id.s = row_vals[event_id_col].val.str_val.s;
		if(subs.event_id.s)
			subs.event_id.len = strlen(subs.event_id.s);
			
		subs.to_tag.s = row_vals[to_tag_col].val.str_val.s;
		subs.to_tag.len = strlen(row_vals[to_tag_col].val.str_val.s);
		
		subs.from_tag.s = row_vals[from_tag_col].val.str_val.s;
		subs.from_tag.len = strlen(row_vals[from_tag_col].val.str_val.s);

		subs.callid.s = row_vals[callid_col].val.str_val.s;
		subs.callid.len = strlen(row_vals[callid_col].val.str_val.s);
	
		subs.contact.s = row_vals[contact_col].val.str_val.s;
		if(subs.contact.s)
			subs.contact.len = strlen(row_vals[contact_col].val.str_val.s);

		subs.cseq = row_vals[cseq_col].val.int_val;

		subs.record_route.s = row_vals[record_route_col].val.str_val.s;
		if(subs.record_route.s )
			subs.record_route.len = strlen(subs.record_route.s);
	
		subs.expires = 0;
	
		subs.status.s = "terminated";
		subs.status.len = 10;

		subs.reason.s = "timeout";
		subs.reason.len = 7;
	
		notify(&subs,  NULL, NULL, 0);
	}
	
	if (pa_dbf.use_table(pa_db, active_watchers_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE:msg_active_watchers_clean: ERROR in use_table\n");
		return ;
	}

	if (pa_dbf.delete(pa_db, db_keys, db_ops, db_vals, 1) < 0) 
		LOG(L_ERR,"PRESENCE:msg_active_watchers_clean: ERROR cleaning expired"
				" messages\n");

	pa_dbf.free_result(pa_db, result);
	return;
}


int handle_subscribe(struct sip_msg* msg, char* str1, char* str2)
{
	struct sip_uri to_uri;
	struct sip_uri from_uri;
	struct to_body *pto, *pfrom = NULL, TO;
	int lexpire, i;
	int  to_tag_gen = 0;
	str rtag_value;
	subs_t subs;
	char buf[50];
	str rec_route;
	int error_ret = -1;
	int rt  = 0;
	db_key_t db_keys[7];
	db_val_t db_vals[7];
	db_key_t result_cols[2];
	db_res_t *result = NULL;
	db_row_t *row ;	
	db_val_t *row_vals ;
	int len= 0;
	int old_contact= 0;

	/* ??? rename to avoid collisions with other symbols */
	counter ++;
	contact_body_t *b;
	rec_route.s = NULL;
	rec_route.len = 0;

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
	if( (!msg->event ) ||(msg->event->body.len<=0) ||
		((strncmp(msg->event->body.s, "presence",8 )!=0)&&
		(strncmp(msg->event->body.s, "presence.winfo",14 )!=0)) )
	{
		LOG(L_ERR, "PRESENCE: handle_subscribe:Missing or unsupported event"
				" header field value\n");

		if (slb.reply(msg, 489, &pu_489_rpl) == -1)
		{
			LOG(L_ERR, "PRESENCE: handle_subscribe: ERROR while sending"
					" reply\n");
		}
		error_ret = 0;
		goto error;
	}
	
	/* ??? implement event header parser */
	for(i= 0; i< msg->event->body.len; i++ )
	{
		if (msg->event->body.s[i] == ';')
			break;

	}
	subs.event.s = msg->event->body.s;
	subs.event.len = i;

	if(i == msg->event->body.len)
	{
		subs.event_id.s = NULL;
		subs.event_id.len= 0;
	}
	else
	{
		/* ??? check parameter name to be 'id' and if other parameters */
		for(i= subs.event.len; i< msg->event->body.len; i++ )
			if ( msg->event->body.s[i] == '=')
				break;
		i++;
		subs.event_id.s = msg->event->body.s+ i;
		subs.event_id.len = msg->event->body.len- i;
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
				default_expires);
		lexpire = default_expires;
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
		parse_to(msg->to->body.s,msg->to->body.s + msg->to->body.len + 1, &TO);
		if(TO.uri.len <= 0) 
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
		LOG(L_INFO,"PRESENCE:handle_subscribe: generating to_tag\n");
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
	if (str2int( &(get_cseq(msg)->number), &subs.cseq)!=0 )
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
	
/*process record route and add it to a string*/
	if (msg->record_route!=NULL)
	{
		rt = process_rr(msg->record_route, &rec_route);
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

	if( pfrom->tag_value.s ==NULL || pfrom->tag_value.len == 0)
	{
		LOG(L_ERR, "PRESENCE: handle_subscribe: ERROR no from tag value"
				" present\n");
		goto error;
	}

	subs.from_tag.s = pfrom->tag_value.s;
	subs.from_tag.len = pfrom->tag_value.len;


	subs.version = 0;

	if( PWINFO_LEN==subs.event.len)
	{
		subs.status.s = "active";
		subs.status.len = 6 ;
	}
	else    /* take status from 'watchers' table */
	{
		db_keys[0] ="p_user";
		db_vals[0].type = DB_STR;
		db_vals[0].nul = 0;
		db_vals[0].val.str_val.s= subs.to_user.s;
		db_vals[0].val.str_val.len= subs.to_user.len;


		db_keys[1] ="p_domain";
		db_vals[1].type = DB_STR;
		db_vals[1].nul = 0;
		db_vals[1].val.str_val.s = subs.to_domain.s;
		db_vals[1].val.str_val.len = subs.to_domain.len;

		db_keys[2] ="w_user";
		db_vals[2].type = DB_STR;
		db_vals[2].nul = 0;
		db_vals[2].val.str_val.s = subs.from_user.s;
		db_vals[2].val.str_val.len = subs.from_user.len;

		db_keys[3] ="w_domain";
		db_vals[3].type = DB_STR;
		db_vals[3].nul = 0;
		db_vals[3].val.str_val.s = subs.from_domain.s;
		db_vals[3].val.str_val.len = subs.from_domain.len;

		result_cols[0] = "subs_status";
		result_cols[1] = "reason";

		if(pa_dbf.use_table(pa_db, watchers_table)< 0)
		{
			LOG(L_ERR,"PRESENCE:handle_subscribe: ERROR in use table\n");
			goto error;
		}	

		if(pa_dbf.query(pa_db, db_keys, 0, db_vals, result_cols,
						4, 2, 0, &result )< 0)
		{
			LOG(L_ERR, "PRESENCE:handle_subscribe: ERROR while querying"
					" watchers table\n");
			goto error;
		}
		if(result== NULL)
			goto error;
		
		if(result->n <=0)
		{
			LOG(L_INFO, "PRESENCE:handle_subscribe:The query in table watches "
				"returned no result\n");
	
			subs.status.s = "pending";
			subs.status.len = 7;
			subs.reason.s = NULL;

			db_keys[4] ="subs_status";
			db_vals[4].type = DB_STR;
			db_vals[4].nul = 0;
			if(force_active==0 )
			{
				db_vals[4].val.str_val.s = "pending";
				db_vals[4].val.str_val.len = 7;
			}
			else
			{
				db_vals[4].val.str_val.s = "active";
				db_vals[4].val.str_val.len = 6;
			}
		
			db_keys[5] = "inserted_time";
			db_vals[5].type = DB_INT;
			db_vals[5].nul = 0;
			db_vals[5].val.int_val= (int)time(NULL);


			if(pa_dbf.insert(pa_db, db_keys, db_vals, 6)< 0)
			{
				LOG(L_ERR, "PRESENCE:handle_subscribe: ERROR while inserting into"
						" watchers table\n");
				goto error;
			}
		}
		else
		{
			old_contact= 1;
			row = &result->rows[0];
			row_vals = ROW_VALUES(row);		
			
			len= strlen(row_vals[0].val.str_val.s);
			subs.status.s= (char*)pkg_malloc(len* sizeof(char));
			if(subs.status.s== NULL)
			{
				LOG(L_ERR, "PRESENCE:handle_subscribe: ERORR No more memory\n");
				goto error;
			}	
			memcpy(subs.status.s, row_vals[0].val.str_val.s, len);
			subs.status.len = len;

			if(row_vals[1].val.str_val.s)
			{
				len= strlen(row_vals[1].val.str_val.s);
				if(len== 0)
					subs.reason.s= NULL;
				else
				{
					subs.reason.s= (char*)pkg_malloc(len*sizeof(char));
					if(subs.reason.s)
					{
						LOG(L_ERR, "PRESENCE:handle_subscribe: ERORR No more memory\n");
						goto error;		
					}		
					old_contact= 2;
					memcpy(subs.reason.s, row_vals[1].val.str_val.s, len);
				}
				subs.reason.len = len;
			}
		}
		if(result)
			pa_dbf.free_result(pa_db, result);
		result= NULL;
	
	}

	printf_subs(&subs);	
	if( update_subscribtion(msg, &subs, &rtag_value, to_tag_gen) <0 )
	{	
		LOG(L_ERR,"PRESENCE:handle_subscribe: ERROR while updating database\n");
		goto error;
	}

	if(old_contact && subs.status.s)
	{
		pkg_free(subs.status.s);
		if(old_contact== 2&& subs.reason.s )
			pkg_free(subs.reason.s);
	}
	return 1;

error:
	LOG(L_ERR, "PRESENCE:handle_subscribe: ERROR occured\n");
	if(result)
			pa_dbf.free_result(pa_db, result);
	if(old_contact)
	{
		pkg_free(subs.status.s);
		if(old_contact== 2)
			pkg_free(subs.reason.s);
	}

	return error_ret;

}


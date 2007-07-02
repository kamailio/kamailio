/*
 * $Id$
 *
 * presence module- presence server implementation
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>

#include "../../trim.h"
#include "../../ut.h"
#include "../../globals.h"
#include "../../parser/contact/parse_contact.h"
#include "../../str.h"
#include "../../db/db.h"
#include "../../db/db_val.h"
#include "../tm/tm_load.h"
#include "../../socket_info.h"
#include "presentity.h"
#include "presence.h"
#include "notify.h"
#include "utils_func.h"

#define ALLOC_SIZE 3000
#define MAX_FORWARD 70

extern struct tm_binds tmb;
c_back_param* shm_dup_subs(subs_t* subs, str to_tag);

void p_tm_callback( struct cell *t, int type, struct tmcb_params *ps);

void printf_subs(subs_t* subs)
{	
	DBG("\n\tpres_user= %.*s - len: %d\tpres_domain= %.*s - len: %d", 
			subs->pres_user.len,  subs->pres_user.s, subs->pres_user.len,
			subs->pres_domain.len,  subs->pres_domain.s, subs->pres_domain.len);
	DBG("\n\t[p_user]= %.*s  [p_domain]= %.*s\n\t[w_user]= %.*s "  
			"[w_domain]= %.*s\n",
			subs->to_user.len, subs->to_user.s, subs->to_domain.len,
			subs->to_domain.s,
			subs->from_user.len, subs->from_user.s, subs->from_domain.len,
			subs->from_domain.s);
	DBG("[event]= %.*s\n\t[status]= %.*s\n\t[expires]= %d\n",
			subs->event->stored_name.len, subs->event->stored_name.s,	subs->status.len, subs->status.s,
			subs->expires );
	DBG("[to_tag]= %.*s\n\t[from_tag]= %.*s\n",
			subs->to_tag.len, subs->to_tag.s,	subs->from_tag.len, subs->from_tag.s);
	DBG("[contact]= %.*s\n",
			subs->contact.len, subs->contact.s);
}
str* create_winfo_xml(watcher_t* watchers,int n, char* version,char* resource, int STATE_FLAG );

int build_str_hdr(subs_t* subs, int is_body, str** hdr)
{
	str* str_hdr = NULL;	
	char* subs_expires = NULL;
	int len = 0;
	ev_t* event= subs->event;
	int expires_t;

	str_hdr =(str*)pkg_malloc(sizeof(str));
	if(str_hdr== NULL)
	{
		LOG(L_ERR, "PRESENCE: build_str_hdr:ERROR while allocating memory\n");
		return -1;
	}
	memset(str_hdr, 0, sizeof(str));

	str_hdr->s = (char*)pkg_malloc(ALLOC_SIZE* sizeof(char));
	if(str_hdr->s== NULL)
	{
		LOG(L_ERR, "PRESENCE: build_str_hdr:ERROR while allocating memory\n");
		pkg_free(str_hdr);
		return -1;
	}	

	strncpy(str_hdr->s ,"Max-Forwards: ", 14);
	str_hdr->len = 14;
	len= sprintf(str_hdr->s+str_hdr->len, "%d", MAX_FORWARD);
	if(len<= 0)
	{
		LOG(L_ERR, "PRESENCE: build_str_hdr:ERROR while printing in string\n");
		pkg_free(str_hdr->s);
		pkg_free(str_hdr);
		return -1;
	}	
	str_hdr->len+= len; 
	strncpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;

	strncpy(str_hdr->s+str_hdr->len  ,"Event: ", 7);
	str_hdr->len+= 7;
	strncpy(str_hdr->s+str_hdr->len, event->stored_name.s, event->stored_name.len);
	str_hdr->len+= event->stored_name.len;
	if (subs->event_id.len) 
	{
 		strncpy(str_hdr->s+str_hdr->len, ";id=", 4);
 		str_hdr->len += 4;
 		strncpy(str_hdr->s+str_hdr->len, subs->event_id.s, subs->event_id.len);
 		str_hdr->len += subs->event_id.len;
 	}
	strncpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;


	strncpy(str_hdr->s+str_hdr->len ,"Contact: <", 10);
	str_hdr->len += 10;
	strncpy(str_hdr->s+str_hdr->len, subs->local_contact.s, subs->local_contact.len);
	str_hdr->len +=  subs->local_contact.len;
	strncpy(str_hdr->s+str_hdr->len, ">", 1);
	str_hdr->len += 1;
	strncpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;

	if(strncmp(subs->status.s, "terminated",10) == 0)
	{
		DBG( "PRESENCE: build_str_hdr: state = terminated \n");

		strncpy(str_hdr->s+str_hdr->len,"Subscription-State: ", 20);
		str_hdr->len+= 20;
		strncpy(str_hdr->s+str_hdr->len, subs->status.s ,subs->status.len );
		str_hdr->len+= subs->status.len;
		
		strncpy(str_hdr->s+str_hdr->len,";reason=", 8);
		str_hdr->len+= 8;
		strncpy(str_hdr->s+str_hdr->len, subs->reason.s ,subs->reason.len );
		str_hdr->len+= subs->reason.len;
		strncpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
		str_hdr->len+= CRLF_LEN;

	}
	else
	{	
		strncpy(str_hdr->s+str_hdr->len,"Subscription-State: ", 20);
		str_hdr->len += 20;
		strncpy(str_hdr->s+str_hdr->len, subs->status.s ,subs->status.len );
		str_hdr->len += subs->status.len;
		strncpy(str_hdr->s+str_hdr->len,";expires=", 9);
		str_hdr->len+= 9;
	
		if(subs->expires < 0)
			expires_t = 0;
		else
			expires_t= subs->expires;

		subs_expires= int2str(expires_t, &len); 

		if(subs_expires == NULL || len == 0)
		{
			LOG(L_ERR, "PRESENCE:built_str_hdr: ERROR while converting int "
					" to str\n");
			pkg_free(str_hdr->s);
			pkg_free(str_hdr);
			return -1;
		}

		DBG("PRESENCE:build_str_hdr: expires = %d\n", expires_t);
		DBG("PRESENCE:build_str_hdr: subs_expires : %.*s\n", len , subs_expires);

		strncpy(str_hdr->s+str_hdr->len,subs_expires ,len );
		str_hdr->len += len;
		strncpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
		str_hdr->len += CRLF_LEN;

		if(is_body)
		{	
			strncpy(str_hdr->s+str_hdr->len,"Content-Type: ", 14);
			str_hdr->len += 14;
			strncpy(str_hdr->s+str_hdr->len, event->content_type.s , event->content_type.len);
			str_hdr->len += event->content_type.len;
			strncpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
			str_hdr->len += CRLF_LEN;
		}
	}

	str_hdr->s[str_hdr->len] = '\0';
	*hdr= str_hdr;

	return 0;

}

str* get_wi_notify_body(subs_t* subs, subs_t* watcher_subs)
{
	db_key_t query_cols[6];
	db_op_t  query_ops[6];
	db_val_t query_vals[6];
	db_key_t result_cols[6];
	db_res_t *result = NULL;
	db_row_t *row = NULL ;	
	db_val_t *row_vals = NULL;
	str* notify_body = NULL;
	str p_uri;
	char* version_str;
	watcher_t *watchers = NULL;
	watcher_t swatchers;
	int n_result_cols = 0;
	int n_query_cols = 0;
	int i , len = 0;
	int status_col, expires_col, from_user_col, from_domain_col;
	
	uandd_to_uri(subs->to_user, subs->to_domain, &p_uri);
	if(p_uri.s == NULL)
	{
		LOG(L_ERR,"PRESENCE:get_wi_notify_body: ERROR while creating uri\n");
		return NULL;
	}

	memset(&swatchers, 0, sizeof(watcher_t));
	version_str = int2str(subs->version, &len);

	if(version_str ==NULL)
	{
		LOG(L_ERR,"PRESENCE:get_wi_notify_body: ERROR while converting int"
				" to str\n ");
		goto error;
	}

	if(watcher_subs != NULL) /*no need to query data base */
	{
		
		swatchers.status= watcher_subs->status;
		uandd_to_uri( watcher_subs->from_user,watcher_subs->from_domain,
						&swatchers.uri);
		if(swatchers.uri.s== NULL)
			goto error;

		swatchers.id.s = (char *)pkg_malloc(swatchers.uri.len *2 +1);
		if(swatchers.id.s==0)
		{
			LOG(L_ERR,"PRESENCE:get_wi_notify_body: ERROR no more pkg mem\n");
			pkg_free(swatchers.uri.s);
			goto error;
		}
		to64frombits((unsigned char *)swatchers.id.s,
				(const unsigned char*) swatchers.uri.s, swatchers.uri.len );
			
		swatchers.id.len = strlen(swatchers.id.s);
		
		swatchers.event= watcher_subs->event->stored_name;
		
		notify_body = create_winfo_xml(&swatchers, 1, version_str,p_uri.s,
				PARTIAL_STATE_FLAG );

		if(swatchers.uri.s !=NULL)
			pkg_free(swatchers.uri.s );
		if(swatchers.id.s !=NULL)
			pkg_free(swatchers.id.s );

		pkg_free(p_uri.s);			
		return notify_body;
	}

	query_cols[n_query_cols] = "pres_user";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= subs->pres_user;
	n_query_cols++;

	query_cols[n_query_cols] = "pres_domain";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->pres_domain;
	n_query_cols++;

	
	query_cols[n_query_cols] = "event";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = subs->event->wipeer->stored_name;
	n_query_cols++;

	result_cols[status_col=n_result_cols++] = "status" ;
	result_cols[expires_col=n_result_cols++] = "expires";
	result_cols[from_user_col=n_result_cols++] = "from_user";
	result_cols[from_domain_col=n_result_cols++] = "from_domain";

	
	if (pa_dbf.use_table(pa_db, active_watchers_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE:get_wi_notify_body: Error in use_table\n");
		goto error;
	}

	DBG("PRESENCE:get_wi_notify_body: querying database  \n");
	if (pa_dbf.query (pa_db, query_cols, query_ops, query_vals,
		 result_cols, n_query_cols, n_result_cols, 0,  &result) < 0) 
	{
		LOG(L_ERR, "PRESENCE:get_wi_notify_body: Error while querying"
				" presentity\n");
		goto error;
	}

	if (result== NULL )
	{
		LOG(L_ERR, "PRESENCE: get_wi_notify_body:The query returned no"
				" result\n");
		goto error;
	}
	else
	if(result->n >0)			
	{
		str from_user;
		str from_domain;

		watchers =(watcher_t*)pkg_malloc( (result->n+1)*sizeof(watcher_t));
		if(watchers == NULL)
		{
			LOG(L_ERR, "PRESENCE:get_wi_notify_body:ERROR while allocating"
					" memory\n");
			goto error;
		}
		memset(watchers, 0, (result->n+1)*sizeof(watcher_t));

		for(i=0; i<result->n; i++)
		{
			row = &result->rows[i];
			row_vals = ROW_VALUES(row);
			watchers[i].status.s = (char*)row_vals[status_col].val.string_val;
			watchers[i].status.len= strlen(watchers[i].status.s);

			from_user.s= (char*)row_vals[from_user_col].val.string_val;
			from_user.len= strlen(from_user.s);

			from_domain.s= (char*)row_vals[from_domain_col].val.string_val;
			from_domain.len= strlen(from_domain.s);

			if(uandd_to_uri(from_user, from_domain, &watchers[i].uri)<0)
			{
				LOG(L_ERR, "PRESENCE:get_wi_notify_body:ERROR while creating"
					" uri\n");
				goto error;
			}	
			watchers[i].id.s = (char*)pkg_malloc(watchers[i].uri.len*2 +1);
			to64frombits((unsigned char *)watchers[i].id.s,
					(const unsigned char*) watchers[i].uri.s,watchers[i].uri.len );
			
			watchers[i].id.len = strlen(watchers[i].id.s);
			watchers[i].event= subs->event->wipeer->stored_name;
		}
	}

	DBG( "PRESENCE:get_wi_notify_body: the query returned no result\n");
	notify_body = create_winfo_xml(watchers, result->n, version_str, p_uri.s,
			FULL_STATE_FLAG );

	if(watchers!=NULL) 
	{
		for(i = 0; i<result->n; i++)
		{
			if(watchers[i].uri.s !=NULL)
				pkg_free(watchers[i].uri.s );
			if(watchers[i].id.s !=NULL)
				pkg_free(watchers[i].id.s );
		}
		pkg_free(watchers);
	}
	pa_dbf.free_result(pa_db, result);
	if(p_uri.s)
		pkg_free(p_uri.s);

	return notify_body;

error:
	if(result!=NULL)
		pa_dbf.free_result(pa_db, result);

	if(notify_body)
	{
		if(notify_body->s)
			xmlFree(notify_body->s);
		pkg_free(notify_body);
	}
	if(watchers!=NULL) 
	{
		for(i = 0; i<result->n; i++)
		{
			if(watchers[i].uri.s !=NULL)
				pkg_free(watchers[i].uri.s );
			if(watchers[i].id.s !=NULL)
				pkg_free(watchers[i].id.s );
		}
		pkg_free(watchers);
	}
	if(p_uri.s)
		pkg_free(p_uri.s);

	return NULL;

}
str* get_p_notify_body(str user, str host, ev_t* event, str* etag,
		subs_t* subs)
{
	db_key_t query_cols[6];
	db_val_t query_vals[6];
	db_key_t result_cols[6];
	db_res_t *result = NULL;
	int body_col, expires_col, etag_col= 0;
	str** body_array= NULL;
	str* notify_body= NULL;	
	db_row_t *row= NULL ;	
	db_val_t *row_vals;
	int n_result_cols = 0;
	int n_query_cols = 0;
	int i, n, len;
	int build_off_n= -1; 
	str etags;
	str* body;
	int size= 0;

	query_cols[n_query_cols] = "domain";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = host.s;
	query_vals[n_query_cols].val.str_val.len = host.len;
	n_query_cols++;

	query_cols[n_query_cols] = "username";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = user.s;
	query_vals[n_query_cols].val.str_val.len = user.len;
	n_query_cols++;

	query_cols[n_query_cols] = "event";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= event->stored_name;
	n_query_cols++;

	result_cols[body_col=n_result_cols++] = "body" ;
	result_cols[expires_col=n_result_cols++] = "expires";
	result_cols[etag_col=n_result_cols++] = "etag";

	if (pa_dbf.use_table(pa_db, presentity_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE:get_p_notify_body: Error in use_table\n");
		return NULL;
	}

	DBG("PRESENCE:get_p_notify_body: querying presentity\n");
	if (pa_dbf.query (pa_db, query_cols, 0, query_vals,
		 result_cols, n_query_cols, n_result_cols, "received_time",  &result) < 0) 
	{
		LOG(L_ERR, "PRESENCE:get_p_notify_body: Error while querying"
				" presentity\n");
		if(result)
			pa_dbf.free_result(pa_db, result);
		return NULL;
	}
	
	if(result== NULL)
		return NULL;

	if (result && result->n<=0 )
	{
		DBG("PRESENCE: get_p_notify_body: The query returned no"
				" result\n[username]= %.*s\t[domain]= %.*s\t[event]= %.*s\n",
				user.len, user.s, host.len, host.s, event->stored_name.len, event->stored_name.s);
		
		pa_dbf.free_result(pa_db, result);
		result= NULL;

		if(event->agg_nbody)
		{
			notify_body = event->agg_nbody(&user, &host, NULL, 0, -1);
			if(notify_body)
				goto done;
		}	
			
		return NULL;
	}
	else
	{
		n= result->n;
		if(event->agg_nbody== NULL )
		{
			DBG("PRESENCE:get_p_notify_body: Event does not require aggregation\n");
			row = &result->rows[n-1];
			row_vals = ROW_VALUES(row);
			if(row_vals[body_col].val.string_val== NULL)
			{
				LOG(L_ERR, "PRESENCE:get_p_notify_body:ERROR NULL notify body record\n");
				goto error;
			}
			len= strlen(row_vals[body_col].val.string_val);
			if(len== 0)
			{
				LOG(L_ERR, "PRESENCE:get_p_notify_body:ERROR Empty notify body record\n");
				goto error;
			}
			notify_body= (str*)pkg_malloc(sizeof(str));
			if(notify_body== NULL)
			{
				LOG(L_ERR, "PRESENCE:get_p_notify_body: ERROR while allocating memory\n");
				goto error;
			}
			memset(notify_body, 0, sizeof(str));
			notify_body->s= (char*)pkg_malloc( len* sizeof(char));
			if(notify_body->s== NULL)
			{
				LOG(L_ERR, "PRESENCE:get_p_notify_body: ERROR while allocating memory\n");
				pkg_free(notify_body);
				goto error;
			}
			memcpy(notify_body->s, row_vals[body_col].val.string_val, len);
			notify_body->len= len;
			pa_dbf.free_result(pa_db, result);
			
			return notify_body;
		}
		
		DBG("PRESENCE:get_p_notify_body: Event requires aggregation\n");
		
		body_array =(str**)pkg_malloc( (n+1) *sizeof(str*));
		if(body_array == NULL)
		{
			LOG(L_ERR, "PRESENCE:get_p_notify_body:ERROR while allocating"
					" memory\n");
			goto error;
		}

		if(etag!= NULL)
		{
			DBG("PRESENCE:get_p_notify_body:searched etag = %.*s len= %d\n", 
					etag->len, etag->s, etag->len);
			DBG( "PRESENCE:get_p_notify_body: etag not NULL\n");
			for(i= 0; i< n; i++)
			{
				row = &result->rows[i];
				row_vals = ROW_VALUES(row);
				etags.s = (char*)row_vals[etag_col].val.string_val;
				etags.len = strlen(etags.s);

				DBG("PRESENCE:get_p_notify_body:etag = %.*s len= %d\n", 
						etags.len, etags.s, etags.len);
				if( (etags.len == etag->len) && (strncmp(etags.s, etag->s,
								etags.len)==0 ) )
				{
					DBG("PRESENCE:get_p_notify_body found etag  \n");
					build_off_n= i;
				}
				len= strlen(row_vals[body_col].val.string_val);
				if(len== 0)
				{
					LOG(L_ERR, "PRESENCE:get_p_notify_body:ERROR Empty notify body record\n");
					goto error;
				}
			
				size= sizeof(str)+ len* sizeof(char);
				body= (str*)pkg_malloc(size);
				if(body== NULL)
				{
					LOG(L_ERR, "PRESENCE:get_p_notify_body: ERROR while allocating memory\n");
					goto error;
				}
				memset(body, 0, size);
				size= sizeof(str);
				body->s= (char*)body+ size;
				memcpy(body->s, row_vals[body_col].val.string_val, len);
				body->len= len;

				body_array[i]= body;
			}
		}	
		else
		{	
			for(i=0; i< n; i++)
			{
				row = &result->rows[i];
				row_vals = ROW_VALUES(row);
				
				len= strlen(row_vals[body_col].val.string_val);
				if(len== 0)
				{
					LOG(L_ERR, "PRESENCE:get_p_notify_body:ERROR Empty notify body record\n");
					goto error;
				}
				
				size= sizeof(str)+ len* sizeof(char);
				body= (str*)pkg_malloc(size);
				if(body== NULL)
				{
					LOG(L_ERR, "PRESENCE:get_p_notify_body: ERROR while allocating memory\n");
					goto error;
				}
				memset(body, 0, size);
				size= sizeof(str);
				body->s= (char*)body+ size;
				memcpy(body->s, row_vals[body_col].val.string_val, len);
				body->len= len;

				body_array[i]= body;
			}			
		}
		pa_dbf.free_result(pa_db, result);
		result= NULL;

		notify_body = event->agg_nbody(&user, &host, body_array, n, build_off_n);
	}

done:	
	if(body_array!=NULL)
	{
		for(i= 0; i< n; i++)
		{
			if(body_array[i])
				pkg_free(body_array[i]);
		}
		pkg_free(body_array);
	}
	return notify_body;

error:
	if(result!=NULL)
		pa_dbf.free_result(pa_db, result);

	if(body_array!=NULL)
	{
		for(i= 0; i< n; i++)
		{
			if(body_array[i])
				pkg_free(body_array[i]);
			else
				break;

		}
	
		pkg_free(body_array);
	}

	return NULL;
}

int free_tm_dlg(dlg_t *td)
{
	if(td)
	{
		if(td->route_set)
			free_rr(&td->route_set);
		pkg_free(td);
	}
	return 0;
}

dlg_t* build_dlg_t (str p_uri, subs_t* subs)
{
	dlg_t* td =NULL;
	str w_uri;
	int found_contact = 1;

	td = (dlg_t*)pkg_malloc(sizeof(dlg_t));
	if(td == NULL)
	{
		LOG(L_ERR, "PRESENCE:build_dlg_t: No memory left\n");
		return NULL;
	}
	memset(td, 0, sizeof(dlg_t));

	td->loc_seq.value = subs->cseq++;
	td->loc_seq.is_set = 1;

	td->id.call_id = subs->callid;
	td->id.rem_tag = subs->from_tag;
	td->id.loc_tag =subs->to_tag;
	td->loc_uri = p_uri;

	if(subs->contact.len ==0 || subs->contact.s == NULL )
	{
		found_contact = 0;
	}
	else
	{
		DBG("CONTACT = %.*s\n", subs->contact.len , subs->contact.s);

		td->rem_target = subs->contact;
	}

	uandd_to_uri(subs->from_user, subs->from_domain, &w_uri);
	if(w_uri.s ==NULL)
	{
		LOG(L_ERR, "PRESENCE:build_dlg_t :ERROR while creating uri\n");
		goto error;
	}
	
	td->rem_uri = w_uri;
	if(found_contact == 0)
	{
		td->rem_target = w_uri;
	}
	if(subs->record_route.s && subs->record_route.len)
	{
		if(parse_rr_body(subs->record_route.s, subs->record_route.len,
			&td->route_set)< 0)
		{
			LOG(L_ERR, "PRESENCE:build_dlg_t :ERROR in function parse_rr_body\n");
			goto error;
		}
	}	
	td->state= DLG_CONFIRMED ;

	if (subs->sockinfo_str.len) {
		int port, proto;
        str host;
		if (parse_phostport (
				subs->sockinfo_str.s,subs->sockinfo_str.len,&host.s,
				&host.len,&port, &proto )) {
			LOG (L_ERR,"PRESENCE:build_dlg_t:bad sockinfo string\n");
			goto error;
		}
		td->send_sock = grep_sock_info (
			&host, (unsigned short) port, (unsigned short) proto);
	}
	
	return td;

error:
	if(w_uri.s ==NULL)
	{
		pkg_free(w_uri.s);
		w_uri.s= NULL;
	}
	if(td!=NULL)
		free_tm_dlg(td);

	return NULL;
}


int get_subs_dialog(str* p_user, str* p_domain, ev_t* event,str* sender,
		subs_t*** array, int *n)
{

	subs_t** subs_array= NULL;
	subs_t* subs= NULL;
	db_key_t query_cols[7];
	db_op_t  query_ops[7];
	db_val_t query_vals[7];
	db_key_t result_cols[18];
	int n_result_cols = 0, n_query_cols = 0;
	db_row_t *row ;	
	db_val_t *row_vals ;
	db_res_t *result = NULL;
	int size= 0;
	str from_user, from_domain, from_tag;
	str to_user, to_domain, to_tag;
	str event_id, callid, record_route, contact, status;
	str sockinfo_str, local_contact;
	int from_user_col, from_domain_col, from_tag_col;
	int to_user_col, to_domain_col, to_tag_col;
	int expires_col= 0,callid_col, cseq_col, i, status_col =0, event_id_col = 0;
	int version_col= 0, record_route_col = 0, contact_col = 0;
	int sockinfo_col= 0, local_contact_col= 0;


	if (pa_dbf.use_table(pa_db, active_watchers_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE:get_subs_dialog: Error in use_table\n");
		return -1;
	}

	DBG("PRESENCE:get_subs_dialog:querying database table = active_watchers\n");
	query_cols[n_query_cols] = "pres_domain";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = p_domain->s;
	query_vals[n_query_cols].val.str_val.len = p_domain->len;
	n_query_cols++;

	query_cols[n_query_cols] = "pres_user";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = p_user->s;
	query_vals[n_query_cols].val.str_val.len = p_user->len;
	n_query_cols++;
	
	query_cols[n_query_cols] = "event";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = event->stored_name;
	n_query_cols++;

	if(sender)
	{	
		DBG("PRESENCE:get_subs_dialog: Should not send Notify to: [uri]= %.*s\n",
				 sender->len, sender->s);

		query_cols[n_query_cols] = "contact";
		query_ops[n_query_cols] = OP_NEQ;
		query_vals[n_query_cols].type = DB_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val.s = sender->s;
		query_vals[n_query_cols].val.str_val.len = sender->len;
		n_query_cols++;
	}
	result_cols[to_user_col=n_result_cols++] = "to_user" ;
	result_cols[to_domain_col=n_result_cols++] = "to_domain" ;
	result_cols[from_user_col=n_result_cols++] = "from_user" ;
	result_cols[from_domain_col=n_result_cols++] = "from_domain" ;
	result_cols[event_id_col=n_result_cols++] = "event_id";
	result_cols[from_tag_col=n_result_cols++] = "from_tag";
	result_cols[to_tag_col=n_result_cols++] = "to_tag";	
	result_cols[callid_col=n_result_cols++] = "callid";
	result_cols[cseq_col=n_result_cols++] = "local_cseq";
	result_cols[record_route_col=n_result_cols++] = "record_route";
	result_cols[contact_col=n_result_cols++] = "contact";
	result_cols[expires_col=n_result_cols++] = "expires";
	result_cols[status_col=n_result_cols++] = "status"; 
	result_cols[sockinfo_col=n_result_cols++] = "socket_info"; 
	result_cols[local_contact_col=n_result_cols++] = "local_contact"; 
	result_cols[version_col=n_result_cols++] = "version";

	if (pa_dbf.query(pa_db, query_cols, query_ops, query_vals,result_cols,
				n_query_cols, n_result_cols, 0, &result) < 0) 
	{
		LOG(L_ERR, "PRESENCE:get_subs_dialog:Error while querying database\n");
		if(result)
		{
			pa_dbf.free_result(pa_db, result);
		}
		return -1;
	}

	if(result== NULL)
		return -1;

	if (result->n <=0 )
	{
		DBG("PRESENCE: get_subs_dialog:The query for subscribtion for"
				" [user]= %.*s,[domain]= %.*s for [event]= %.*s returned no"
				" result\n",p_user->len, p_user->s, p_domain->len, 
				p_domain->s,event->stored_name.len, event->stored_name.s);
		pa_dbf.free_result(pa_db, result);
		result = NULL;
		array= NULL;
		return 0;

	}
	DBG("PRESENCE: get_subs_dialog:n= %d\n", result->n);
	
	subs_array = (subs_t**)pkg_malloc(result->n*sizeof(subs_t*));
	if(subs_array == NULL)
	{
		LOG(L_ERR,"PRESENCE: get_subs_dialog: ERROR while allocating memory\n");
		goto error;
	}
	memset(subs_array, 0, result->n*sizeof(subs_t*));
	
	for(i=0; i<result->n; i++)
	{
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);		
	
		memset(&status, 0, sizeof(str));
		to_user.s= (char*)row_vals[to_user_col].val.string_val;
		to_user.len= 	strlen(to_user.s);
		to_domain.s= (char*)row_vals[to_domain_col].val.string_val;
		to_domain.len= strlen(to_domain.s);

		from_user.s= (char*)row_vals[from_user_col].val.string_val;
		from_user.len= 	strlen(from_user.s);
		from_domain.s= (char*)row_vals[from_domain_col].val.string_val;
		from_domain.len= strlen(from_domain.s);
		event_id.s=(char*)row_vals[event_id_col].val.string_val;
		if(event_id.s== NULL)
			event_id.len = 0;
		else
			event_id.len= strlen(event_id.s);
		
		to_tag.s= (char*)row_vals[to_tag_col].val.string_val;
		to_tag.len= strlen(to_tag.s);
		from_tag.s= (char*)row_vals[from_tag_col].val.string_val; 
		from_tag.len= strlen(from_tag.s);
		callid.s= (char*)row_vals[callid_col].val.string_val;
		callid.len= strlen(callid.s);
		
		record_route.s=  (char*)row_vals[record_route_col].val.string_val;
		if(record_route.s== NULL )
			record_route.len= 0;
		else
			record_route.len= strlen(record_route.s);

		contact.s= (char*)row_vals[contact_col].val.string_val;
		contact.len= strlen(contact.s);
		
		status.s=  (char*)row_vals[status_col].val.string_val;
		status.len= strlen(status.s);
			
		sockinfo_str.s = (char*)row_vals[sockinfo_col].val.string_val;
		sockinfo_str.len = sockinfo_str.s?strlen (sockinfo_str.s):0;

		local_contact.s = (char*)row_vals[local_contact_col].val.string_val;
		local_contact.len = local_contact.s?strlen (local_contact.s):0;
		
		size= sizeof(subs_t)+ (p_user->len+ p_domain->len+ to_user.len+
				to_domain.len+ from_user.len+ from_domain.len+ event_id.len+
				to_tag.len+ status.len+ from_tag.len+ callid.len+ 
				record_route.len+ contact.len+ sockinfo_str.len+ 
				local_contact.len)* sizeof(char);

		subs= (subs_t*)pkg_malloc(size);
		if(subs ==NULL)
		{
			LOG(L_ERR,"PRESENCE: get_subs_dialog: ERROR while allocating"
					" memory\n");
			goto error;
		}	
		memset(subs, 0, size);
		size= sizeof(subs_t);

		subs->pres_user.s= (char*)subs+ size;
		memcpy(subs->pres_user.s, p_user->s, p_user->len);
		subs->pres_user.len = p_user->len;
		size+= p_user->len;

		subs->pres_domain.s= (char*)subs+ size;
		memcpy(subs->pres_domain.s, p_domain->s, p_domain->len);
		subs->pres_domain.len = p_domain->len;
		size+= p_domain->len;
	
		subs->to_user.s= (char*)subs+ size;
		memcpy(subs->to_user.s, to_user.s, to_user.len);
		subs->to_user.len = to_user.len;
		size+= to_user.len;

		subs->to_domain.s= (char*)subs+ size;
		memcpy(subs->to_domain.s, to_domain.s, to_domain.len);
		subs->to_domain.len = to_domain.len;
		size+= to_domain.len;

		subs->event= event;

		subs->from_user.s= (char*)subs+ size;
		memcpy(subs->from_user.s, from_user.s, from_user.len);
		subs->from_user.len = from_user.len;
		size+= from_user.len;

		subs->from_domain.s= (char*)subs+ size;
		memcpy(subs->from_domain.s, from_domain.s, from_domain.len);
		subs->from_domain.len = from_domain.len;
		size+= from_domain.len;
		
		subs->to_tag.s= (char*)subs+ size;
		memcpy(subs->to_tag.s, to_tag.s, to_tag.len);
		subs->to_tag.len = to_tag.len;
		size+= to_tag.len;

		subs->from_tag.s= (char*)subs+ size;
		memcpy(subs->from_tag.s, from_tag.s, from_tag.len);
		subs->from_tag.len = from_tag.len;
		size+= from_tag.len;

		subs->callid.s= (char*)subs+ size;
		memcpy(subs->callid.s, callid.s, callid.len);
		subs->callid.len = callid.len;
		size+= callid.len;
		
		if(event_id.s && event_id.len)
		{
			subs->event_id.s= (char*)subs+ size;
			memcpy(subs->event_id.s, event_id.s, event_id.len);
			subs->event_id.len = event_id.len;
			size+= event_id.len;
		}

		if(record_route.s && record_route.len)
		{	
			subs->record_route.s =(char*)subs+ size;
			memcpy(subs->record_route.s, record_route.s, record_route.len);
			subs->record_route.len = record_route.len;
			size+= record_route.len;
		}
		
		subs->contact.s =(char*)subs+ size;
		memcpy(subs->contact.s, contact.s, contact.len);
		subs->contact.len = contact.len;
		size+= contact.len;
	
		subs->status.s= (char*)subs+ size;
		memcpy(subs->status.s, status.s, status.len);
		subs->status.len = status.len;
		size+= status.len;
		
		subs->sockinfo_str.s =(char*)subs+ size;
		memcpy(subs->sockinfo_str.s, sockinfo_str.s, sockinfo_str.len);
		subs->sockinfo_str.len = sockinfo_str.len;
		size+= sockinfo_str.len;
		
		subs->local_contact.s =(char*)subs+ size;
		memcpy(subs->local_contact.s, local_contact.s, local_contact.len);
		subs->local_contact.len = local_contact.len;
		size+= local_contact.len;

		subs->cseq = row_vals[cseq_col].val.int_val;
		subs->expires = row_vals[expires_col].val.int_val - 
			(int)time(NULL);
		subs->version = row_vals[version_col].val.int_val;
		
		subs_array[i]= subs;
	}

	*n = result->n;
	pa_dbf.free_result(pa_db, result);

	*array= subs_array;
	return 0;

error:
	if(subs_array)
	{
		for(i=0; i<result->n; i++)
			if(subs_array[i])
				pkg_free(subs_array[i]);
		pkg_free(subs_array);
	}

	if(result)
		pa_dbf.free_result(pa_db, result);
	
	return -1;
	
}
int publ_notify(presentity_t* p, str* body, str* offline_etag)
{
	int n=0, i=0;
	str* notify_body = NULL;
	subs_t** subs_array = NULL;

	if(get_subs_dialog(&p->user, &p->domain, p->event , p->sender,
				&subs_array, &n)< 0)
	{
		LOG(L_ERR, "PRESENCE:publ_notify: ERROR while getting subs_dialog"
				" from database\n");
		goto error;
	}
	if(subs_array == NULL)
	{
		DBG("PRESENCE: publ_notify: Could not get subs_dialog from"
			" database\n");
		goto done;
	}

	/* if the event does not require aggregation - we have the final body */
	if(p->event->agg_nbody)
	{	
		notify_body = get_p_notify_body(p->user, p->domain,
				p->event , offline_etag, NULL);
		if(notify_body == NULL)
		{
			DBG( "PRESENCE: publ_notify: Could not get the"
					" notify_body\n");
			/* goto error; */
		}
	}

	for(i =0; i<n; i++)
	{
		if(notify(subs_array[i], NULL, notify_body?notify_body:body, 0)< 0 )
		{
			LOG(L_ERR, "PRESENCE: publ_notify: Could not send notify for"
					"%.*s\n", p->event->stored_name.len, 
					p->event->stored_name.s);
			goto error;
		}
	}

done:

	if(subs_array!=NULL)
	{	
		for(i =0; i<n; i++)
		{
			if(subs_array[i]!=NULL)
				pkg_free(subs_array[i]);
		}
		pkg_free(subs_array);
	}
	if(notify_body!=NULL)
	{
		if(notify_body->s)
		{
			if(	p->event->agg_nbody== NULL && 	p->event->apply_auth_nbody== NULL)
				pkg_free(notify_body->s);
			else
				p->event->free_body(notify_body->s);
		}
		pkg_free(notify_body);
	}

	return 1;

error:
	if(subs_array!=NULL)
	{
		for(i =0; i<n; i++)
		{
			if(subs_array[i]!=NULL)
				pkg_free(subs_array[i]);
		}
		pkg_free(subs_array);
	}
	if(notify_body!=NULL)
	{
		if(notify_body->s)
		{
			if(	p->event->agg_nbody== NULL && 	p->event->apply_auth_nbody== NULL)
				pkg_free(notify_body->s);
			else
				p->event->free_body(notify_body->s);
		}
		pkg_free(notify_body);
	}
	return -1;

}	


int query_db_notify(str* p_user, str* p_domain, ev_t* event, 
		subs_t* watcher_subs )
{
	subs_t** subs_array = NULL;
	int n=0, i=0;
	str* notify_body = NULL;

	if(get_subs_dialog(p_user, p_domain, event , NULL, &subs_array, &n)< 0)
	{
		LOG(L_ERR, "PRESENCE:query_db_notify: ERROR while getting subs_dialog"
				" from database\n");
		goto error;
	}
	if(subs_array == NULL)
	{
		DBG("PRESENCE:query_db_notify: Could not get subs_dialog from"
			" database\n");
		goto done;
	}
	
	if(event->type & PUBL_TYPE)
	{
		notify_body = get_p_notify_body(*p_user, *p_domain, event,
				NULL, NULL);
		if(notify_body == NULL)
		{
			DBG( "PRESENCE:query_db_notify: Could not get the"
					" notify_body\n");
			/* goto error; */
		}
	}	

	for(i =0; i<n; i++)
	{
		if(notify(subs_array[i], watcher_subs, notify_body, 0)< 0 )
		{
			LOG(L_ERR, "PRESENCE:query_db_notify: Could not send notify for"
					"%.*s\n", event->stored_name.len, event->stored_name.s);
			goto error;
		}
	}

done:

	if(subs_array!=NULL)
	{	
		for(i =0; i<n; i++)
		{
			if(subs_array[i]!=NULL)
				pkg_free(subs_array[i]);
		}
		pkg_free(subs_array);
	}
	if(notify_body!=NULL)
	{
		if(notify_body->s)
		{
			if(event->agg_nbody== NULL && event->apply_auth_nbody== NULL)
				pkg_free(notify_body->s);
			else
				event->free_body(notify_body->s);
		}
		pkg_free(notify_body);
	}

	return 1;

error:
	if(subs_array!=NULL)
	{
		for(i =0; i<n; i++)
		{
			if(subs_array[i]!=NULL)
				pkg_free(subs_array[i]);
		}
		pkg_free(subs_array);
	}
	if(notify_body!=NULL)
	{
		if(notify_body->s)
		{
			if(event->agg_nbody== NULL && event->apply_auth_nbody== NULL)
				pkg_free(notify_body->s);
			else
				event->free_body(notify_body->s);
		}
		pkg_free(notify_body);
	}
	return -1;
}

int notify(subs_t* subs, subs_t * watcher_subs,str* n_body,int force_null_body)
{

	str p_uri= {NULL, 0};
	dlg_t* td = NULL;
	str met = {"NOTIFY", 6};
	str* str_hdr = NULL;
	str* notify_body = NULL;
	int result= 0;
	int n_update_keys = 0;
	db_key_t db_keys[1], update_keys[4];
	db_val_t db_vals[1], update_vals[4];
    c_back_param *cb_param= NULL;
	str* final_body= NULL;
	
	DBG("PRESENCE:notify:dialog informations:\n");
	printf_subs(subs);

    /* getting the status of the subscription */

	if(force_null_body)
	{	
		goto jump_over_body;
	}

	if(n_body!= NULL && strncmp( subs->status.s, "active", 6) == 0 )
	{
		if( subs->event->req_auth)
		{	
			if( subs->event->apply_auth_nbody(n_body, subs, &notify_body)< 0)
			{
				LOG(L_ERR, "PRESENCE:notify: ERROR in function hget_nbody\n");
				goto error;
			}
			if(notify_body== NULL)
				notify_body= n_body;
		}
		else
			notify_body= n_body;
	}	
	else
	{	
		if(strncmp( subs->status.s, "terminated", 10) == 0 ||
			strncmp( subs->status.s, "pending", 7) == 0) 
		{
			DBG("PRESENCE:notify: state terminated or pending-"
					" notify body NULL\n");
			notify_body = NULL;
		}
		else  
		{		
			if(subs->event->type & WINFO_TYPE)	
			{	
				notify_body = get_wi_notify_body(subs, watcher_subs );
				if(notify_body == NULL)
				{
					DBG("PRESENCE:notify: Could not get the notify_body\n");
					goto error;
				}
			}
			else
			{
				notify_body = get_p_notify_body(subs->pres_user,
						subs->pres_domain, subs->event, NULL, subs);
				if(notify_body == NULL || notify_body->s== NULL)
				{
					DBG("PRESENCE:notify: Could not get the notify_body\n");
				}
				else		/* apply authorization rules if exists */
				if(subs->event->req_auth)
				{
					 
					if(subs->event->apply_auth_nbody(notify_body, subs, &final_body)< 0)
					{
						LOG(L_ERR, "PRESENCE:notify: ERROR in function apply_auth\n");
						goto error;
					}
					if(final_body)
					{
						xmlFree(notify_body->s);
						pkg_free(notify_body);
						notify_body= final_body;
					}	
				}	
			}		
		}
	}
	
jump_over_body:

	/* built extra headers */	
	uandd_to_uri(subs->to_user, subs->to_domain, &p_uri);
	
	if(p_uri.s ==NULL)
	{
		LOG(L_ERR, "PRESENCE:notify :ERROR while creating uri\n");
		goto error;
	}
	DBG("PRESENCE: notify: build notify to user= %.*s domain= %.*s"
			" for event= %.*s\n", subs->from_user.len, subs->from_user.s,
			subs->from_domain.len, subs->from_domain.s,subs->event->stored_name.len,
			subs->event->stored_name.s);

	printf_subs(subs);

	/* build extra headers */
	if( build_str_hdr( subs, notify_body?1:0, &str_hdr)< 0 )
	{
		LOG(L_ERR, "PRESENCE:notify:ERROR while building headers \n");
		goto error;
	}	
	DBG("PRESENCE:notify: headers:\n%.*s\n", str_hdr->len, str_hdr->s);

	/* construct the dlg_t structure */
	td = build_dlg_t(p_uri, subs);
	if(td ==NULL)
	{
		LOG(L_ERR, "PRESENCE:notify:ERROR while building dlg_t structure \n");
		goto error;	
	}

	if(subs->event->type == WINFO_TYPE && watcher_subs )
	{
		DBG("PRESENCE: notify:Send notify for presence on callback\n");
		watcher_subs->send_on_cback = 1;			
	}
	cb_param = shm_dup_subs(watcher_subs, subs->to_tag);
	if(cb_param == NULL)
	{
		LOG(L_ERR, "PRESENCE:notify:ERROR while duplicating cb_param in"
			" share memory\n");
		goto error;	
	}	

	result = tmb.t_request_within
		(&met,						             
		str_hdr,                               
		notify_body,                           
		td,					                  
		p_tm_callback,				        
		(void*)cb_param);				

	if(result < 0)
	{
		LOG(L_ERR, "PRESENCE:notify: ERROR in function tmb.t_request_within\n");
		shm_free(cb_param);
		goto error;	
	}
	
	if (pa_dbf.use_table(pa_db, active_watchers_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE:notify: Error in use_table\n");
		goto error;
	}
		
	db_keys[0] ="to_tag";
	db_vals[0].type = DB_STR;
	db_vals[0].nul = 0;
	db_vals[0].val.str_val = subs->to_tag ;
	
	update_keys[n_update_keys] = "local_cseq";
	update_vals[n_update_keys].type = DB_INT;
	update_vals[n_update_keys].nul = 0;
	update_vals[n_update_keys].val.int_val = subs->cseq;
	n_update_keys++;

	update_keys[n_update_keys] = "status";
	update_vals[n_update_keys].type = DB_STR;
	update_vals[n_update_keys].nul = 0;
	update_vals[n_update_keys].val.str_val = subs->status;
	n_update_keys++;

	if(subs->event->type & WINFO_TYPE)
	{	
		update_keys[n_update_keys] = "version";
		update_vals[n_update_keys].type = DB_INT;
		update_vals[n_update_keys].nul = 0;
		update_vals[n_update_keys].val.int_val = subs->version +1;
		n_update_keys++;

	}
	if(pa_dbf.update(pa_db,db_keys, 0, db_vals, update_keys, update_vals, 1,
			n_update_keys )<0 )
	{
		LOG(L_ERR, "PRESENCE:notify: Error while updating cseq value\n");
		goto error;
	}
	
	if(p_uri.s!=NULL)
		pkg_free(p_uri.s);
	if(td!=NULL)
	{
		if(td->rem_uri.s)
			pkg_free(td->rem_uri.s);
		free_tm_dlg(td);
	}
	if(str_hdr!=NULL)
	{
		if(str_hdr->s)
			pkg_free(str_hdr->s);
		pkg_free(str_hdr);
	}
	if((int)n_body!= (int)notify_body)
	{
		if(notify_body!=NULL)
		{
			if(notify_body->s!=NULL)
			{
				subs->event->free_body(notify_body->s);
			}
			pkg_free(notify_body);
		}
	}	

	return 0;

error:
	if(p_uri.s!=NULL)
		pkg_free(p_uri.s);
	if(td!=NULL)
	{
		if(td->rem_uri.s)
			pkg_free(td->rem_uri.s);
		free_tm_dlg(td);
	}
	if(str_hdr!=NULL)
	{
		if(str_hdr->s)
			pkg_free(str_hdr->s);
		pkg_free(str_hdr);
	}

	if((int)n_body!= (int)notify_body)
	{
		if(notify_body!=NULL)
		{
			if(notify_body->s!=NULL)
				subs->event->free_body(notify_body->s);
			pkg_free(notify_body);
		}
	}	

	return -1;

}

void p_tm_callback( struct cell *t, int type, struct tmcb_params *ps)
{
	if(ps->param==NULL || *ps->param==NULL || 
			((c_back_param*)(*ps->param))->w_id == NULL)
	{
		DBG("PRESENCE p_tm_callback: message id not received\n");
		if(*ps->param !=NULL  )
			shm_free(*ps->param);

		return;
	}
	
	DBG( "PRESENCE:p_tm_callback: completed with status %d [watcher_id:"
			"%p/%s]\n",ps->code, ps->param, ((c_back_param*)(*ps->param))->w_id);

	if(ps->code >= 300)
	{
		db_key_t db_keys[1];
		db_val_t db_vals[1];
		db_op_t  db_ops[1] ;
	
		if (pa_dbf.use_table(pa_db, active_watchers_table) < 0) 
		{
			LOG(L_ERR, "PRESENCE:p_tm_callback: Error in use_table\n");
			goto done;
		}
		
		db_keys[0] ="to_tag";
		db_ops[0] = OP_EQ;
		db_vals[0].type = DB_STRING;
		db_vals[0].nul = 0;
		db_vals[0].val.string_val = ((c_back_param*)(*ps->param))->w_id ;

		if (pa_dbf.delete(pa_db, db_keys, db_ops, db_vals, 1) < 0) 
			LOG(L_ERR,"PRESENCE: p_tm_callback: ERROR cleaning expired"
					" messages\n");
	
	}	
	/* send a more accurate Notify for presence depending on the reply for winfo*/
	if(((c_back_param*)(*ps->param))->wi_subs!= NULL)
	{
		/* if an error message is received as a reply for the winfo Notify 
	  * send a Notify for presence with no body (the stored presence information is 
	  * not valid ) */

		if(ps->code >= 300)
		{
			if(notify( ((c_back_param*)(*ps->param))->wi_subs, NULL, NULL, 1)< 0)
			{
				LOG(L_ERR, "PRESENCE:update_subscribtion: Could not send"
					" notify for presence\n");
			}
		}
		else
		{
			if(notify( ((c_back_param*)(*ps->param))->wi_subs, NULL, NULL, 0)< 0)
			{
				LOG(L_ERR, "PRESENCE:update_subscribtion: Could not send"
					" notify for presence\n");
			}
		}	
	}

done:
	if(*ps->param !=NULL  )
		shm_free(*ps->param);
	return ;

}

c_back_param* shm_dup_subs(subs_t* subs, str to_tag)
{
	int size;
	c_back_param* cb_param = NULL;

	size = sizeof(c_back_param) + to_tag.len +10;

	if(subs && subs->send_on_cback)
	{
		size+= sizeof(subs_t) + (subs->to_user.len+ 
			subs->to_domain.len+ subs->from_user.len+ subs->from_domain.len+
			subs->pres_user.len+ subs->pres_domain.len +
			subs->event_id.len + subs->to_tag.len +
			subs->from_tag.len + subs->callid.len +subs->contact.len +
			subs->record_route.len +subs->status.len + subs->reason.len+
			subs->local_contact.len+ subs->sockinfo_str.len)* sizeof(char);
		
		DBG("PRESENCE: notify: \tlocal_contact.len= %d\n\tsockinfo_str.len= %d\n",
				subs->local_contact.len, subs->sockinfo_str.len);
	}	
	cb_param = (c_back_param*)shm_malloc(size);
	

	if(cb_param == NULL)
	{
		LOG(L_ERR, "PRESENCE: notify:Error no more share memory\n ");
		goto error;	
	}
	memset(cb_param, 0, size);

	size =  sizeof(c_back_param);
	cb_param->w_id = (char*)cb_param + size;
	strncpy(cb_param->w_id, to_tag.s ,to_tag.len ) ;
		cb_param->w_id[to_tag.len] = '\0';	
	
	if(!(subs&& subs->send_on_cback))
	{
		cb_param->wi_subs = NULL;
		return cb_param;
	}	

	size+= subs->to_tag.len + 1;

	cb_param->wi_subs = (subs_t*)((char*)cb_param + size);
	size+= sizeof(subs_t);

	cb_param->wi_subs->pres_user.s = (char*)cb_param + size;
	strncpy(cb_param->wi_subs->pres_user.s, subs->pres_user.s, subs->pres_user.len);
	cb_param->wi_subs->pres_user.len = subs->pres_user.len;
	size+= subs->pres_user.len;

	cb_param->wi_subs->pres_domain.s = (char*)cb_param + size;
	strncpy(cb_param->wi_subs->pres_domain.s, subs->pres_domain.s, subs->pres_domain.len);
	cb_param->wi_subs->pres_domain.len = subs->pres_domain.len;
	size+= subs->pres_domain.len;

	cb_param->wi_subs->to_user.s = (char*)cb_param + size;
	strncpy(cb_param->wi_subs->to_user.s, subs->to_user.s, subs->to_user.len);
	cb_param->wi_subs->to_user.len = subs->to_user.len;
	size+= subs->to_user.len;

	cb_param->wi_subs->to_domain.s = (char*)cb_param + size;
	strncpy(cb_param->wi_subs->to_domain.s, subs->to_domain.s, subs->to_domain.len);
	cb_param->wi_subs->to_domain.len = subs->to_domain.len;
	size+= subs->to_domain.len;

	cb_param->wi_subs->from_user.s = (char*)cb_param + size;
	strncpy(cb_param->wi_subs->from_user.s, subs->from_user.s, subs->from_user.len);
	cb_param->wi_subs->from_user.len = subs->from_user.len;
	size+= subs->from_user.len;

	cb_param->wi_subs->from_domain.s = (char*)cb_param + size;
	strncpy(cb_param->wi_subs->from_domain.s, subs->from_domain.s, subs->from_domain.len);
	cb_param->wi_subs->from_domain.len = subs->from_domain.len;
	size+= subs->from_domain.len;

	cb_param->wi_subs->event= subs->event; 

	cb_param->wi_subs->event_id.s = (char*)cb_param + size;
	strncpy(cb_param->wi_subs->event_id.s, subs->event_id.s, subs->event_id.len);
	cb_param->wi_subs->event_id.len = subs->event_id.len;
	size+= subs->event_id.len;

	cb_param->wi_subs->to_tag.s = (char*)cb_param + size;
	strncpy(cb_param->wi_subs->to_tag.s, subs->to_tag.s, subs->to_tag.len);
	cb_param->wi_subs->to_tag.len = subs->to_tag.len;
	size+= subs->to_tag.len;

	cb_param->wi_subs->from_tag.s = (char*)cb_param + size;
	strncpy(cb_param->wi_subs->from_tag.s, subs->from_tag.s, subs->from_tag.len);
	cb_param->wi_subs->from_tag.len = subs->from_tag.len;
	size+= subs->from_tag.len;

	cb_param->wi_subs->callid.s = (char*)cb_param + size;
	strncpy(cb_param->wi_subs->callid.s, subs->callid.s, subs->callid.len);
	cb_param->wi_subs->callid.len = subs->callid.len;
	size+= subs->callid.len;

	cb_param->wi_subs->cseq = subs->cseq;
	
	cb_param->wi_subs->contact.s = (char*)cb_param + size;
	strncpy(cb_param->wi_subs->contact.s, subs->contact.s, subs->contact.len);
	cb_param->wi_subs->contact.len= subs->contact.len;
	size+= subs->contact.len;

	if(subs->record_route.s)
	{
		cb_param->wi_subs->record_route.s = (char*)cb_param + size;
		strncpy(cb_param->wi_subs->record_route.s, subs->record_route.s, subs->record_route.len);
		cb_param->wi_subs->record_route.len = subs->record_route.len;
		size+= subs->record_route.len;
	}

	cb_param->wi_subs->expires = subs->expires;

	cb_param->wi_subs->status.s = (char*)cb_param + size;
	strncpy(cb_param->wi_subs->status.s, subs->status.s, subs->status.len);
	cb_param->wi_subs->status.len = subs->status.len;
	size+= subs->status.len;

	if(subs->reason.s)
	{	
		cb_param->wi_subs->reason.s = (char*)cb_param + size;
		strncpy(cb_param->wi_subs->reason.s, subs->reason.s, subs->reason.len);
		cb_param->wi_subs->reason.len = subs->reason.len;
		size+= subs->reason.len;
	}
	cb_param->wi_subs->local_contact.s = (char*)cb_param + size;
	strncpy(cb_param->wi_subs->local_contact.s, subs->local_contact.s, subs->local_contact.len);
	cb_param->wi_subs->local_contact.len= subs->local_contact.len;
	size+= subs->local_contact.len;

	cb_param->wi_subs->sockinfo_str.s = (char*)cb_param + size;
	strncpy(cb_param->wi_subs->sockinfo_str.s, subs->sockinfo_str.s, subs->sockinfo_str.len);
	cb_param->wi_subs->sockinfo_str.len= subs->sockinfo_str.len;
	size+= subs->sockinfo_str.len;

	cb_param->wi_subs->version = subs->version;

	return cb_param;

error:
	if(cb_param!= NULL)
		shm_free(cb_param);
	return NULL;
}


str* create_winfo_xml(watcher_t* watchers,int n, char* version,char* resource, int STATE_FLAG )
{
	xmlDocPtr doc = NULL;       
    xmlNodePtr root_node = NULL, node = NULL;
	xmlNodePtr w_list_node = NULL;	
    int i;
	char content[200];
	str *body;

	body = (str*)pkg_malloc(sizeof(str));
	if(body == NULL)
	{
		LOG(L_ERR,"PRESENCE:create_winfo_xmls:Error while allocating memory\n");
		return NULL;
	}
	memset(body, 0, sizeof(str));

    LIBXML_TEST_VERSION;

    /* 
     * Creates a new document, a node and set it as a root node
     */
    doc = xmlNewDoc(BAD_CAST "1.0");
    root_node = xmlNewNode(NULL, BAD_CAST "watcherinfo");
    xmlDocSetRootElement(doc, root_node);

    xmlNewProp(root_node, BAD_CAST "xmlns",
			BAD_CAST "urn:ietf:params:xml:ns:watcherinfo");
    xmlNewProp(root_node, BAD_CAST "version", BAD_CAST version );
   
	if(STATE_FLAG & FULL_STATE_FLAG)
	{
		if( xmlNewProp(root_node, BAD_CAST "state", BAD_CAST "full") == NULL)
		{
			LOG(L_ERR, "PRESENCE: create_winfo_xml: ERROR while adding new"
					"attribute\n");
			goto error;
		}
	}
	else	
	{	
		if( xmlNewProp(root_node, BAD_CAST "state", 
					BAD_CAST "partial")== NULL) /* chage this */
		{
			LOG(L_ERR, "PRESENCE: create_winfo_xml: ERROR while adding new"
					"attribute\n");
			goto error;
		}
	}
	/* 
     * xmlNewChild() creates a new node, which is "attached" as child node
     * of root_node node. 
     */

     w_list_node =xmlNewChild(root_node, NULL, BAD_CAST "watcher-list",NULL);

	if( w_list_node == NULL)
	{
		LOG(L_ERR, "PRESENCE: create_winfo_xml: ERROR while adding child\n");
		goto error;
	}
	xmlNewProp(w_list_node, BAD_CAST "resource", BAD_CAST resource);
	xmlNewProp(w_list_node, BAD_CAST "package", BAD_CAST "presence");

    for( i =0; i<n; i++)
	{
		strncpy( content,watchers[i].uri.s, watchers[i].uri.len);
		content[ watchers[i].uri.len ]='\0';
		node = xmlNewChild(w_list_node, NULL, BAD_CAST "watcher",
				BAD_CAST content) ;
		if( node ==NULL)
		{
			LOG(L_ERR, "PRESENCE: create_winfo_xml: ERROR while adding"
					" child\n");
			goto error;
		}
		if(xmlNewProp(node, BAD_CAST "id", BAD_CAST watchers[i].id.s)== NULL)
		{
			LOG(L_ERR, "PRESENCE: create_winfo_xml: ERROR while adding"
					" new attribute\n");
			goto error;
		}	
		
		if(xmlNewProp(node, BAD_CAST "event", BAD_CAST "subscribe")== NULL)
		{
			LOG(L_ERR, "PRESENCE: create_winfo_xml: ERROR while adding new"
					" attribute\n");
			goto error;
		}	
		
		if(xmlNewProp(node, BAD_CAST "status", 
					BAD_CAST watchers[i].status.s )== NULL)
		{
			LOG(L_ERR, "PRESENCE: create_winfo_xml: ERROR while adding"
					" new attribute\n");
			goto error;
		}	
	}
    
    /* 
     * Dumping document to stdio or file
     */
    //xmlSaveFormatFileEnc("stdout", doc, "UTF-8", 1);
	xmlDocDumpFormatMemory(doc,(xmlChar**)(void*)&body->s, &body->len, 1);

    /*free the document */
    if(doc)
		xmlFreeDoc(doc);

    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();

    /*
     * this is to debug memory for regression tests
     */
    xmlMemoryDump();

    return body;

error:
	if(body)
	{
		if(body->s)
			xmlFree(body->s);
		pkg_free(body);
	}
    if(doc)
		xmlFreeDoc(doc);
	return NULL;
}




	




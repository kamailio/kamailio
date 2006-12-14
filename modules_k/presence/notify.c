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

#include "../../trim.h"
#include "../../ut.h"
#include "../../globals.h"
#include "../../parser/contact/parse_contact.h"
#include "../../str.h"
#include "../../db/db.h"
#include "../../db/db_val.h"
#include "../tm/tm_load.h"

#include "presence.h"
#include "notify.h"
#include "pidf.h"
#include "utils_func.h"

extern struct tm_binds tmb;
c_back_param* shm_dup_subs(subs_t* subs, str to_tag, int is_sec);

void p_tm_callback( struct cell *t, int type, struct tmcb_params *ps);

void printf_subs(subs_t* subs)
{
	LOG(L_INFO,"\n\t[p_user]= %.*s  [p_domain]= %.*s\n\t[w_user]= %.*s "  
			"[w_domain]= %.*s\n",
			subs->to_user.len, subs->to_user.s, subs->to_domain.len,
			subs->to_domain.s,
			subs->from_user.len, subs->from_user.s, subs->from_domain.len,
			subs->from_domain.s);
	LOG(L_INFO,"[event]= %.*s\n\t[staus]= %.*s\n\t[expires]= %d\n",
			subs->event.len, subs->event.s,	subs->status.len, subs->status.s,
			subs->expires );
	LOG(L_INFO,"[to_tag]= %.*s\n\t[from_tag]= %.*s\n",
			subs->to_tag.len, subs->to_tag.s,	subs->from_tag.len, subs->from_tag.s);

}

str* build_str_hdr(str p_uri, str event, str status, int expires_t, str reason)
{

	static 	char buf[3000];
	str* str_hdr = NULL;	
	char* subs_expires = NULL;
	int len = 0;

	str_hdr =(str*) pkg_malloc(sizeof(str));
	if(!str_hdr)
	{
		LOG(L_ERR, "PRESENCE: build_str_hdr:ERROR while allocating memory\n");
		return NULL;
	}

	str_hdr->s = buf;

	LOG(L_INFO, "PRESENCE: build str_hdr:\n\tp_uri = %.*s\n\tevent="
			"%.*s\n\tstatus= %.*s\n\texpires = %d\n",
			p_uri.len, p_uri.s, event.len, event.s, status.len, status.s,
			expires_t);

	strncpy(str_hdr->s ,"Event: ", 7);
	str_hdr->len = 7;
	strncpy(str_hdr->s+str_hdr->len, event.s, event.len);
	str_hdr->len += event.len;
	strncpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;
	
	strncpy(str_hdr->s+str_hdr->len ,"Contact: <", 10);
	str_hdr->len += 10;
	strncpy(str_hdr->s+str_hdr->len, p_uri.s, p_uri.len);
	str_hdr->len += p_uri.len;
	strncpy(str_hdr->s+str_hdr->len, ">", 1);
	str_hdr->len += 1;
	strncpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;

	if(strncmp(status.s, "terminated",10) == 0)
	{
		LOG(L_INFO, "PRESENCE: build_str_hdr: state = terminated writing state"
				" and reason\n");

		strncpy(str_hdr->s+str_hdr->len,"Subscription-State: ", 20);
		str_hdr->len += 20;
		strncpy(str_hdr->s+str_hdr->len, status.s ,status.len );
		str_hdr->len += status.len;
		
		strncpy(str_hdr->s+str_hdr->len,";reason=", 8);
		str_hdr->len += 8;
		strncpy(str_hdr->s+str_hdr->len, reason.s ,reason.len );
		str_hdr->len += reason.len;
		strncpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
		str_hdr->len += CRLF_LEN;

	}
	else
	{	
	
		strncpy(str_hdr->s+str_hdr->len,"Subscription-State: ", 20);
		str_hdr->len += 20;
		strncpy(str_hdr->s+str_hdr->len, status.s ,status.len );
		str_hdr->len += status.len;
		strncpy(str_hdr->s+str_hdr->len,";expires=", 9);
		str_hdr->len+= 9;
	
		if(expires_t < 0)
			expires_t = 0;

		subs_expires= int2str(expires_t, &len); 

		if(subs_expires == NULL || len == 0)
		{
			LOG(L_ERR, "PRESENCE:built_str_hdr: ERROR while converting int "
					" to str\n");
			pkg_free(str_hdr);
			return NULL;
		}

		DBG("PRESENCE:build_str_hdr: expires = %d\n", expires_t);
		DBG("PRESENCE:build_str_hdr: subs_expires : %.*s\n", len , subs_expires);

		strncpy(str_hdr->s+str_hdr->len,subs_expires ,len );
		str_hdr->len += len;
		strncpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
		str_hdr->len += CRLF_LEN;

		strncpy(str_hdr->s+str_hdr->len,"Content-Type: ", 14);
		str_hdr->len += 14;
		if (event.len ==PRES_LEN)
		{
			strncpy(str_hdr->s+str_hdr->len,"application/pidf+xml" , 20);
			str_hdr->len += 20;
		}
		else
		{
			strncpy(str_hdr->s+str_hdr->len,"application/watcherinfo+xml" , 27);
			str_hdr->len += 27;
		}	
		strncpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
		str_hdr->len += CRLF_LEN;
	}

	str_hdr->s[str_hdr->len] = '\0';
	DBG("PRESENCE: build_str_hdr: headers:\n%.*s\n", str_hdr->len, str_hdr->s);
		
	return str_hdr;

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
		swatchers.event.s = "presence";
		swatchers.event.len = 8;
		
		notify_body = create_winfo_xml(&swatchers, 1, version_str,p_uri.s,
				PARTIAL_STATE_FLAG );

		if(swatchers.uri.s !=NULL)
			pkg_free(swatchers.uri.s );
		if(swatchers.id.s !=NULL)
			pkg_free(swatchers.id.s );

		pkg_free(p_uri.s);			
		return notify_body;
	}

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

	query_cols[n_query_cols] = "event";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = "presence";
	query_vals[n_query_cols].val.str_val.len = strlen("presence");
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

	LOG(L_INFO,"PRESENCE:get_wi_notify_body: querying database  \n");
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
			watchers =(watcher_t*)pkg_malloc( (result->n+1)*sizeof(watcher_t));
			if(watchers == NULL)
			{
				LOG(L_ERR, "PRESENCE:get_wi_notify_body:ERROR while allocating"
						" memory\n");
				goto error;
			}
			for(i=0;i<result->n;i++)
			{
				row = &result->rows[i];
				row_vals = ROW_VALUES(row);
				watchers[i].status.s = row_vals[status_col].val.str_val.s;
				watchers[i].status.len=
					strlen(row_vals[status_col].val.str_val.s);

				row_vals[from_user_col].val.str_val.len =
					strlen(row_vals[from_user_col].val.str_val.s);
				row_vals[from_domain_col].val.str_val.len =
					strlen(row_vals[from_domain_col].val.str_val.s);

				if(uandd_to_uri(row_vals[from_user_col].val.str_val,
					row_vals[from_domain_col].val.str_val, &watchers[i].uri)<0)
				{
					LOG(L_ERR, "PRESENCE:get_wi_notify_body:ERROR while creating"
						" memory\n");
					goto error;

				}	
			
				watchers[i].id.s = 
					(char *)pkg_malloc(watchers[i].uri.len *2 +1);

				to64frombits((unsigned char *)watchers[i].id.s,
						(const unsigned char*) watchers[i].uri.s,
						watchers[i].uri.len );
			
				watchers[i].id.len = strlen(watchers[i].id.s);
				watchers[i].event.s = "presence";
				watchers[i].event.len = strlen("presence");	
			}
		}

	LOG(L_INFO, "PRESENCE:get_wi_notify_body: the query returned no result\n");
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

	if(p_uri.s)
		pkg_free(p_uri.s);

	if(result!=NULL)
		pa_dbf.free_result(pa_db, result);
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


str* get_p_notify_body(str user, str host, str* etag)
{
	db_key_t query_cols[4];
	db_op_t  query_ops[4];
	db_val_t query_vals[4];
	db_key_t result_cols[4];
	db_res_t *result = NULL;
	int body_col, expires_col, etag_col;
	str** body_array;
	str* notify_body;	
	db_row_t *row ;	
	db_val_t *row_vals;
	int n_result_cols = 0;
	int n_query_cols = 0;
	int i, n;
	int build_off_n= -1; 
	str etags;

	query_cols[n_query_cols] = "domain";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = host.s;
	query_vals[n_query_cols].val.str_val.len = host.len;
	n_query_cols++;

	query_cols[n_query_cols] = "username";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = user.s;
	query_vals[n_query_cols].val.str_val.len = user.len;
	n_query_cols++;

	result_cols[body_col=n_result_cols++] = "body" ;
	result_cols[expires_col=n_result_cols++] = "expires";
	result_cols[etag_col=n_result_cols++] = "etag";

	if (pa_dbf.use_table(pa_db, presentity_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE:get_p_notify_body: Error in use_table\n");
		return NULL;
	}

	LOG(L_INFO,"PRESENCE:get_p_notify_body: querying presentity\n");
	if (pa_dbf.query (pa_db, query_cols, query_ops, query_vals,
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

	if (result && result->n <=0 )
	{
		DBG("PRESENCE: get_p_notify_body: The query returned no"
				" result\nThere is no presence information recorded for"
				"the user\n");
		pa_dbf.free_result(pa_db, result);
		return NULL;
	}
	else
	{
		n= result->n;
		body_array =(str**)pkg_malloc( (result->n)*sizeof(str*));
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
			LOG(L_INFO, "PRESENCE:get_p_notify_body: etag not NULL\n");
			for(i= 0; i<result->n; i++)
			{
				row = &result->rows[i];
				row_vals = ROW_VALUES(row);
				etags.s = row_vals[etag_col].val.str_val.s;
				etags.len = strlen(etags.s);

				DBG("PRESENCE:get_p_notify_body:etag = %.*s len= %d\n", 
						etags.len, etags.s, etags.len);
				if( (etags.len == etag->len) && (strncmp(etags.s, etag->s,
								etags.len)==0 ) )
				{
					DBG("PRESENCE:get_p_notify_body found etag  \n");
					build_off_n= i;
					
					body_array[build_off_n]= offline_nbody(&row_vals[body_col].val.str_val);
					if(body_array[i] == NULL)
					{
						LOG(L_INFO, "PRESENCE: get_p_notify_body:The users's"
								"status was already offline\n");
						goto error;
					}
				}
				else
					body_array[i] =&row_vals[body_col].val.str_val;
			}
		}	
		else	
			for(i=0;i<result->n;i++)
			{
				row = &result->rows[i];
				row_vals = ROW_VALUES(row);
				body_array[i] =&row_vals[body_col].val.str_val;
			}			
		notify_body = agregate_xmls(body_array, result->n);
	}

	if(result!=NULL)
		pa_dbf.free_result(pa_db, result);
	if(build_off_n >= 0)
	{
		if(body_array[build_off_n])
		{
			if(body_array[build_off_n]->s)
				xmlFree(body_array[build_off_n]->s);
			pkg_free(body_array[build_off_n]);
		}
	}	
	if(body_array!=NULL)
		pkg_free(body_array);
	return notify_body;

error:
	if(result!=NULL)
		pa_dbf.free_result(pa_db, result);
	if(build_off_n > 0)
	{
		if(body_array[build_off_n])
		{
			if(body_array[build_off_n]->s)
				xmlFree(body_array[build_off_n]->s);
			pkg_free(body_array[build_off_n]);
		}
	}	

	if(body_array!=NULL)
		pkg_free(body_array);
	return NULL;
}

static inline int shm_strdup(str* dst, str* src)
{
	dst->s = shm_malloc(src->len);
	if (dst->s==NULL)
	{
		LOG(L_ERR, "PRESENCE:shm_strdup: No memory left\n");
		return -1;
	}
	
	memcpy(dst->s, src->s, src->len);
	dst->len = src->len;
	return 0;
}

static inline int pkg_strdup(str* dst, str* src)
{
	dst->s = pkg_malloc(src->len);
	if (dst->s==NULL)
	{
		LOG(L_ERR, "PRESENCE:pkg_strdup: No memory left\n");
		return -1;
	}
	
	memcpy(dst->s, src->s, src->len);
	dst->len = src->len;
	return 0;
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

	td->loc_seq.value = subs->cseq ;
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

	if(subs->event.len == PRES_LEN)
	{
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
		// pkg_free(w_uri.s);		
		
	}
	else
	{
		td->rem_uri = p_uri;
		if(found_contact == 0)
		{
			td->rem_target = p_uri;
		}

	}
	
	parse_rr_body(subs->record_route.s, subs->record_route.len,
			&td->route_set);
		
	td->state= DLG_CONFIRMED ;
	
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


subs_t** get_subs_dialog(str* p_user, str* p_domain, char* event, int *n)
{

	subs_t** subs_array= NULL;
	subs_t* subs= NULL;
	db_key_t query_cols[6];
	db_op_t  query_ops[6];
	db_val_t query_vals[6];
	db_key_t result_cols[15];
	int n_result_cols = 0, n_query_cols = 0;
	db_row_t *row ;	
	db_val_t *row_vals ;
	db_res_t *result = NULL;
	int size= 0;
	str from_user, from_domain, to_tag, from_tag;
	str event_id, callid, record_route, contact, status;
	int from_user_col, from_domain_col, to_tag_col, from_tag_col;
	int expires_col= 0,callid_col, cseq_col, i, status_col =0, event_id_col = 0;
	int version_col = 0, record_route_col = 0, contact_col = 0;

	if (pa_dbf.use_table(pa_db, active_watchers_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE:get_subs_dialog: Error in use_table\n");
		return NULL;
	}

	LOG(L_INFO,"PRESENCE:get_subs_dialog:querying database table = active_watchers\n");
	query_cols[n_query_cols] = "to_domain";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = p_domain->s;
	query_vals[n_query_cols].val.str_val.len = p_domain->len;
	n_query_cols++;

	query_cols[n_query_cols] = "to_user";
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
	query_vals[n_query_cols].val.str_val.s = event;
	query_vals[n_query_cols].val.str_val.len = strlen(event);
	n_query_cols++;

	result_cols[from_user_col=n_result_cols++] = "from_user" ;
	result_cols[from_domain_col=n_result_cols++] = "from_domain" ;
	result_cols[event_id_col=n_result_cols++] = "event_id";
	result_cols[from_tag_col=n_result_cols++] = "from_tag";
	result_cols[to_tag_col=n_result_cols++] = "to_tag";	
	result_cols[callid_col=n_result_cols++] = "callid";
	result_cols[cseq_col=n_result_cols++] = "cseq";
	result_cols[record_route_col=n_result_cols++] = "record_route";
	result_cols[contact_col=n_result_cols++] = "contact";
	result_cols[expires_col=n_result_cols++] = "expires";
	result_cols[status_col=n_result_cols++] = "status"; 
	
	if(strlen(event)== strlen( "presence.winfo"))
	{
		result_cols[version_col=n_result_cols++] = "version";
	}

	if (pa_dbf.query(pa_db, query_cols, 0, query_vals,result_cols,
				n_query_cols, n_result_cols, 0, &result) < 0) 
	{
		LOG(L_ERR, "PRESENCE:get_subs_dialog:Error while querying database\n");
		if(result)
		{
			pa_dbf.free_result(pa_db, result);
		}
		return NULL;
	}

	if(result== NULL)
		return NULL;

	if (result->n <=0 )
	{
		LOG(L_ERR, "PRESENCE: get_subs_dialog:The query for subscribtion for"
				" [user]= %.*s,[domain]= %.*s for [event]= %s returned no"
				" result\n",p_user->len, p_user->s, p_domain->len, 
				p_domain->s,event);
		pa_dbf.free_result(pa_db, result);
		result = NULL;
		return NULL;

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
		
		from_user.s= row_vals[from_user_col].val.str_val.s;
		from_user.len= 	strlen(from_user.s);
		from_domain.s= row_vals[from_domain_col].val.str_val.s;
		from_domain.len= strlen(from_domain.s);
		event_id.s=row_vals[event_id_col].val.str_val.s;
		event_id.len= strlen(event_id.s);
		if(event_id.s== NULL)
			event_id.len = 0;
		to_tag.s= row_vals[to_tag_col].val.str_val.s;
		to_tag.len= strlen(to_tag.s);
		from_tag.s= row_vals[from_tag_col].val.str_val.s; 
		from_tag.len= strlen(from_tag.s);
		callid.s= row_vals[callid_col].val.str_val.s;
		callid.len= strlen(callid.s);
		
		record_route.s=  row_vals[record_route_col].val.str_val.s;
		record_route.len= strlen(record_route.s);
		if(record_route.s== NULL )
			record_route.len= 0;
		
		contact.s= row_vals[contact_col].val.str_val.s;
		contact.len= strlen(contact.s);
		
		if(!force_active && row_vals[status_col].val.str_val.s)
		{
			status.s=  row_vals[status_col].val.str_val.s;
			status.len= strlen(status.s);
		}
		else
		{
			status.s= NULL;
			status.len= 0;
		}
		
		size= sizeof(subs_t)+ (p_user->len+ p_domain->len+ from_user.len+ 
				from_domain.len+ event_id.len+ + strlen(event)+ to_tag.len+ 
				from_tag.len+ callid.len+ record_route.len+ contact.len)* sizeof(char);

		DBG("PRESENCE: get_subs_dialog: size = %d\n\n", size);

		if(force_active== 0)
			size+= status.len* sizeof(char);
		else
			size+= 6;

		subs= (subs_t*)pkg_malloc(size);
		if(subs ==NULL)
		{
			LOG(L_ERR,"PRESENCE: get_subs_dialog: ERROR while allocating"
					" memory\n");
			goto error;
		}	
		memset(subs, 0, size);
		size= sizeof(subs_t);

		subs->to_user.s= (char*)subs+ size;
		memcpy(subs->to_user.s, p_user->s, p_user->len);
		subs->to_user.len = p_user->len;
		size+= p_user->len;

		subs->to_domain.s= (char*)subs+ size;
		memcpy(subs->to_domain.s, p_domain->s, p_domain->len);
		subs->to_domain.len = p_domain->len;
		size+= p_domain->len;
		
		subs->event.s= (char*)subs+ size;
		memcpy(subs->event.s, event, strlen(event));
		subs->event.len= strlen(event);
		size+= subs->event.len;

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
		
		if(force_active!=0)
		{
			subs->status.s=(char*)subs+ size;
			memcpy(subs->status.s, "active", 6);
			subs->status.len = 6;
			size+= 6;
		}
		else
		{
			if(status.s && status.len)
			{
				subs->status.s= (char*)subs+ size;
				memcpy(subs->status.s, status.s, status.len);
				subs->status.len = status.len;
				size+= status.len;
			}
		}

		subs->cseq = row_vals[cseq_col].val.int_val;
		subs->expires = row_vals[expires_col].val.int_val - 
			(int)time(NULL);

		if(strlen(event) == PWINFO_LEN)
			subs->version = row_vals[version_col].val.int_val;
		
		subs_array[i]= subs;
	}

	*n = result->n;
	pa_dbf.free_result(pa_db, result);

	return subs_array;

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
	
	return NULL;
	
}



int query_db_notify(str* p_user, str* p_domain, char* event, 
		subs_t* watcher_subs, str* etag )
{
	subs_t** subs_array = NULL;
	int n=0, i=0;
	str * notify_body = NULL;

	subs_array= get_subs_dialog(p_user, p_domain, event, &n);
	if(subs_array == NULL)
	{
		LOG(L_ERR, "PRESENCE:query_db_notify: Could not get subs_dialog from"
				" database\n");
		goto error;
	}
	
	if(PRES_LEN == strlen(event) )
	{
		notify_body = get_p_notify_body(*p_user, *p_domain, etag);
		if(notify_body == NULL)
		{
			LOG(L_DBG, "PRESENCE:query_db_notify: Could not get the"
					" notify_body\n");
			/* goto error; */
		}
	}	

	for(i =0; i<n; i++)
	{
		if(notify(subs_array[i], watcher_subs, notify_body)< 0 )
		{
			LOG(L_DBG, "PRESENCE:query_db_notify: Could not send notify for"
					"%s\n", event);
		}
	}

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
			free(notify_body->s);
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
			free(notify_body->s);
		pkg_free(notify_body);
	}
	return -1;
}

xmlNodePtr is_watcher_allowed( subs_t* subs, xmlDocPtr xcap_tree )
{
	xmlNodePtr ruleset_node = NULL, node1= NULL, node2= NULL;
	xmlNodePtr cond_node = NULL, except_node = NULL, actions_node = NULL;
	xmlNodePtr identity_node = NULL, validity_node =NULL, sphere_node = NULL;
	xmlNodePtr sub_handling_node = NULL;
	int apply_rule = -1;
	char* id = NULL, *domain = NULL;
	str w_uri;
	char* sub_handling = NULL;

	if(xcap_tree == NULL)
	{
		LOG(L_ERR, "PRESENCE: is_watcher_allowed: The authorization document"
				" is NULL\n");
		return NULL;
	}
	
	uandd_to_uri(subs->from_user, subs->from_domain, &w_uri);
	if(w_uri.s == NULL)
	{
		LOG(L_ERR, "PRESENCE: is_watcher_allowed:Error while creating uri\n");
		return NULL;
	}
	ruleset_node = xmlDocGetNodeByName(xcap_tree, "ruleset", NULL);
	if(ruleset_node == NULL)
	{
		LOG(L_INFO, "PRESENCE:is_watcher_allowed: ruleset_node NULL\n");
		goto error;

	}	
	for(node1 = ruleset_node->children ; node1; node1 = node1->next)
	{
		if(xmlStrcasecmp(node1->name, (unsigned char*)"text")==0 )
				continue;

		/* process conditions */
		DBG("PRESENCE:is_watcher_allowed:node1->name= %s\n",node1->name);

		cond_node = xmlNodeGetChildByName(node1, "conditions");
		if(cond_node == NULL)
		{	
			LOG(L_INFO, "PRESENCE:is_watcher_allowed:cond node NULL\n");
			goto error;
		}
		DBG("PRESENCE:is_watcher_allowed:cond_node->name= %s\n",
				cond_node->name);

		validity_node = xmlNodeGetChildByName(cond_node, "validity");
		if(validity_node !=NULL)
		{
			LOG(L_INFO, "PRESENCE:is_watcher_allowed:found validity tag\n");

		}	
		sphere_node = xmlNodeGetChildByName(cond_node, "sphere");

		identity_node = xmlNodeGetChildByName(cond_node, "identity");
		if(identity_node == NULL)
		{
			LOG(L_ERR, "PRESENCE:is_watcher_allowed:ERROR didn't found"
					" identity tag\n");
			goto error;
		}	
		id = NULL;
		
		if(strcmp ((const char*)identity_node->children->name, "one") == 0)	
			for(node2 = identity_node->children; node2; node2 = node2->next)
			{
				if(xmlStrcasecmp(node2->name, (unsigned char*)"text")== 0)
					continue;

				id = xmlNodeGetAttrContentByName(node2, "id");	
				if((strlen(id)== w_uri.len && 
							(strncmp(id, w_uri.s, w_uri.len)==0)))	
				{
					apply_rule = 1;
					break;
				}
			}	
		else
		{	
			domain = NULL;
			for(node2 = identity_node->children; node2; node2 = node2->next)
			{
				if(xmlStrcasecmp(node2->name, (unsigned char*)"text")== 0)
					continue;
	
				domain = xmlNodeGetAttrContentByName(node2, "domain");
			
				if(domain == NULL)
				{	
					apply_rule = 1;
					break;
				}
				else	
					if((strlen(domain)!= subs->from_domain.len && 
								strncmp(domain, subs->from_domain.s,
									subs->from_domain.len) ))
						continue;

				apply_rule = 1;
				if(node2->children == NULL)       /* there is no exception */
					break;

				for(except_node = node2->children; except_node;
						except_node= except_node->next)
				{
					if(xmlStrcasecmp(except_node->name, 
								(unsigned char*)"text")== 0)
						continue;

					id = xmlNodeGetAttrContentByName(except_node, "id");	
					if(id!=NULL)
					{
						if((strlen(id)== w_uri.len && (strncmp(id, w_uri.s,
											w_uri.len)==0)))	
						{
							apply_rule = 0;
							break;
						}
					}	
					else
					{
						domain = NULL;
						domain = xmlNodeGetAttrContentByName(except_node,
								"domain");
						if((domain!=NULL && strlen(domain)== 
									subs->from_domain.len &&
						(strncmp(domain,subs->from_domain.s , 
								 subs->from_domain.len)==0)))	
						{
							apply_rule = 0;
							break;
						}
					}	
					if (apply_rule == 0)
						break;
				}
				if(apply_rule ==1 || apply_rule==0)
					break;

			}		
		}
		if(apply_rule ==1 || apply_rule==0)
					break;
	}


	if(w_uri.s!=NULL)
		pkg_free(w_uri.s);

	if( !apply_rule || !node1)
		return NULL;
	else
	/* process actions */	
	{	actions_node = xmlNodeGetChildByName(node1, "actions");
		if(actions_node == NULL)
		{	
			LOG(L_INFO, "PRESENCE:is_watcher_allowed: actions_node NULL\n");
			return NULL;
		}
		DBG("PRESENCE:is_watcher_allowed:actions_node->name= %s\n",
				actions_node->name);
	
		
		sub_handling_node = xmlNodeGetChildByName(actions_node, "sub-handling");
		if(sub_handling_node== NULL)
		{	
			LOG(L_INFO, "PRESENCE:is_watcher_allowed:sub_handling_node NULL\n");
			return NULL;
		}
		sub_handling = (char*)xmlNodeGetContent(sub_handling_node);

		DBG("PRESENCE:is_watcher_allowed:sub_handling_node->name= %s\n",
				sub_handling_node->name);
		DBG("PRESENCE:is_watcher_allowed:sub_handling_node->content= %s\n",
				sub_handling);

//		sub_handling = (char *)sub_handling_node->content;
		
		if(sub_handling== NULL)
		{
			LOG(L_ERR, "PRESENCE:is_watcher_allowed:ERROR Couldn't get"
					" sub-handling content\n");
			return NULL;
		}
		if( strncmp((char*)sub_handling, "block",5 )==0)
		{	
			subs->status.s = "terminated";
			subs->status.len = 10;
			subs->reason.s= "rejected";
			subs->reason.len = 8;
		}
		
		if( strncmp((char*)sub_handling, "confirm",7 )==0)
		{	
			subs->status.s = "pending";
			subs->status.len = 7;
		}
		
		if( strncmp((char*)sub_handling , "polite-block",12 )==0)
		{	
			subs->status.s = "active";
			subs->status.len = 6;
			subs->reason.s= "polite-block";
			subs->reason.len = 12;
		
		}
		
		if( strncmp((char*)sub_handling , "allow",5 )==0)
		{	
			subs->status.s = "active";
			subs->status.len = 6;
			subs->reason.s = NULL;
		}

		return node1;
	}	
error:
	pkg_free(w_uri.s);
	return NULL;
}

xmlDocPtr get_xcap_tree(str user, str domain)
{
	db_key_t query_cols[4];
	db_val_t query_vals[4];
	db_key_t result_cols[3];
	int n_query_cols = 0;
	db_res_t *result = 0;
	db_row_t *row ;	
	db_val_t *row_vals ;
	str body ;
	xmlDocPtr xcap_tree =NULL;

	query_cols[n_query_cols] = "user";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = user.s;
	query_vals[n_query_cols].val.str_val.len = user.len;
	n_query_cols++;
	
	query_cols[n_query_cols] = "domain";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = domain.s;
	query_vals[n_query_cols].val.str_val.len = domain.len;
	n_query_cols++;
	
	result_cols[0] = "xcap";

	if (pa_dbf.use_table(pa_db, "xcap_xml") < 0) 
	{
		LOG(L_ERR, "PRESENCE:get_xcap_tree: Error in use_table\n");
		return NULL;
	}

	if( pa_dbf.query(pa_db, query_cols, 0 , query_vals, result_cols, 
				n_query_cols, 1, 0, &result)<0)
	{
		LOG(L_ERR, "PRESENCE:get_xcap_tree:Error while querying table xcap for"
		" [user]=%.*s , domain=%.*s\n",user.len, user.s, domain.len, domain.s);
		goto error;
	}
	if(result== NULL)
		return NULL;

	if(result && result->n<=0)
	{
		LOG(L_ERR, "PRESENCE:get_xcap_tree:The query in table xcap for"
				" [user]=%.*s , domain=%.*s returned no result\n",
				user.len, user.s, domain.len, domain.s);
		goto error;
	}
	LOG(L_ERR, "PRESENCE:get_xcap_tree:The query in table xcap for"
			" [user]=%.*s , domain=%.*s returned result",	user.len, user.s, domain.len, domain.s );

	row = &result->rows[0];
	row_vals = ROW_VALUES(row);

	body.s = row_vals[0].val.str_val.s;
	body.len = strlen(body.s);
	
	DBG("PRESENCE:get_xcap_tree: xcap body:\n%.*s", body.len,body.s);
	
	xcap_tree = xmlParseMemory(body.s, body.len);
	if(xcap_tree == NULL)
	{
		LOG(L_ERR,"PRESENCE:get_xcap_tree: ERROR while parsing memory\n");
		goto error;
	}

	if(result!=NULL)
		pa_dbf.free_result(pa_db, result);

	return xcap_tree;

error:
	if(result!=NULL)
		pa_dbf.free_result(pa_db, result);
	return NULL;
}


int notify(subs_t* subs, subs_t * watcher_subs, str* n_body )
{

	str p_uri= {NULL, 0};
	dlg_t* td = NULL;
	str met = {"NOTIFY", 6};
	str* str_hdr = NULL;
	str* notify_body = NULL, *final_body = NULL;
	int result= 0, send_on_cback = 0;
	int n_update_keys = 0, wn_update_keys = 0;
	db_key_t db_keys[1], update_keys[3];
	db_val_t db_vals[1], update_vals[3];
	db_key_t w_keys[5], w_up_keys[2];
	db_val_t  w_vals[5], w_up_vals[2];
	xmlNodePtr rule_node = NULL;
	xmlDocPtr xcap_tree = NULL;
    c_back_param *cb_param;


	LOG(L_INFO, "PRESENCE:notify:dialog informations:\n");
	printf_subs(subs);

	if ( subs->event.len == PRES_LEN)
	{	
		xcap_tree = get_xcap_tree(subs->to_user, subs->to_domain);
		if(xcap_tree == NULL)
		{	
			LOG(L_INFO, "PRESENCE:notify: Couldn't get xcap_tree\n");
			if(force_active && subs->status.len ==7)
			{
				subs->status.s = "active";
				subs->status.len = 6;
			}	
		}	
		else
		{
			rule_node = is_watcher_allowed(subs, xcap_tree);
			if(rule_node ==NULL)
			{
				LOG(L_INFO, "PRESENCE:notify: The subscriber didn't match"
					" the conditions\n");
			}	
			else
			{	
				DBG("PRESENCE:notify: [status]=%s\n",subs->status.s);
				if(subs->reason.s!= NULL)
					DBG(" [reason]= %s\n", subs->reason.s);
			
				w_keys[0] = "p_user";
				w_vals[0].type = DB_STR; 
				w_vals[0].nul = 0;
				w_vals[0].val.str_val = subs->to_user;
				
				w_keys[1] = "p_domain";
				w_vals[1].type = DB_STR; 
				w_vals[1].nul = 0;
				w_vals[1].val.str_val = subs->to_domain;

				w_keys[2] = "w_user";
				w_vals[2].type = DB_STR; 
				w_vals[2].nul = 0;
				w_vals[2].val.str_val = subs->from_user;
				
				w_keys[3] = "w_domain";
				w_vals[3].type = DB_STR; 
				w_vals[3].nul = 0;
				w_vals[3].val.str_val = subs->from_domain;
				
				w_up_keys[wn_update_keys]= "subs_status";
				w_up_vals[wn_update_keys].type = DB_STR; 
				w_up_vals[wn_update_keys].nul = 0;
				w_up_vals[wn_update_keys].val.str_val = subs->status;
				wn_update_keys++;
				
				if(subs->reason.s)
				{	
					w_up_keys[wn_update_keys]= "reason";
					w_up_vals[wn_update_keys].type = DB_STR; 
					w_up_vals[wn_update_keys].nul = 0;
					w_up_vals[wn_update_keys].val.str_val = subs->reason;
					wn_update_keys++;
				}
				if(pa_dbf.use_table(pa_db, watchers_table)< 0)
				{	
					LOG(L_ERR, "PRESENCE: notify:ERROR in use watchers table\n");
					goto error;
				}
				if(pa_dbf.update(pa_db, w_keys, 0, w_vals, w_up_keys,w_up_vals,
							4,wn_update_keys++)< 0)
				{
					LOG(L_ERR, "PRESENCE: notify:ERROR while updating watchers table\n");
					goto error;
				}
			}	
	
		}	
	
	}
	
	if(n_body!= NULL && strncmp( subs->status.s, "active", 6) == 0 )
		notify_body = n_body;
	else
	{	
		if(strncmp( subs->status.s, "terminated", 10) == 0 ||
			strncmp( subs->status.s, "pending", 7) == 0) 
		{
			LOG(L_INFO, "PRESENCE:notify: state terminated or pending-"
					" notify body NULL");
			notify_body = NULL;
		}
		else  
		{		
			if(PWINFO_LEN == subs->event.len)	
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
				if(strncmp(subs->status.s, "active", 6)==0 && subs->reason.s
					&& strncmp(subs->reason.s, "polite-block", 12)==0)
				{
					notify_body = build_off_nbody(subs->to_user,subs->to_domain,
							NULL);
					if(notify_body == NULL)
					{
						LOG(L_ERR, "PRESENCE:notify: ERROR while building"
							" polite-block body\n");
						goto error;
					}	
				}
				else
				{	
					notify_body = get_p_notify_body(subs->to_user,
							subs->to_domain, NULL);
					if(notify_body == NULL)
					{
						LOG(L_DBG, "PRESENCE:notify: Could not get the"
								" notify_body\n");
						/* goto error; */
					}
				}	
			}		
			
		}
	}
	
	if( (subs->event.len == PRES_LEN) && notify_body&& rule_node)
	{
		DBG("PRESENCE:notify: get final body according to transformations\n");

		final_body = get_final_notify_body(subs,notify_body, rule_node);
		if(final_body == NULL)
		{
			LOG(L_ERR, "PRESENCE:notify: ERROR occured while transforming"
					" body\n");
			goto error;
		}

		if(n_body == NULL)
		{	
			if(notify_body!=NULL)
			{
				if(notify_body->s!=NULL)
					xmlFree(notify_body->s);
				pkg_free(notify_body);
			}
			notify_body = NULL;
		}
	}
	else
		final_body= notify_body;

	/* built extra headers */	
	uandd_to_uri(subs->to_user, subs->to_domain, &p_uri);
	
	if(p_uri.s ==NULL)
	{
		LOG(L_ERR, "PRESENCE:notify :ERROR while creating uri\n");
		goto error;
	}
	LOG(L_INFO, "PRESENCE: notify: build notify to user= %.*s domain= %.*s"
			" for event= %.*s\n", subs->from_user.len, subs->from_user.s,
			subs->from_domain.len, subs->from_domain.s,subs->event.len,
			subs->event.s);

	printf_subs(subs);
	str_hdr = build_str_hdr(p_uri, subs->event,subs->status, subs->expires,
			subs->reason );
	if(str_hdr == NULL|| str_hdr->s== NULL|| str_hdr->len==0)
	{
		LOG(L_ERR, "PRESENCE:notify:ERROR while building headers \n");
		goto error;
	}	
	LOG(L_INFO, "PRESENCE:notify: headers:%.*s\n ", str_hdr->len, str_hdr->s);

	/* construct the dlg_t structure */
	td = build_dlg_t(p_uri, subs);
	if(td ==NULL)
	{
		LOG(L_ERR, "PRESENCE:notify:ERROR while building dlg_t structure \n");
		goto error;	
	}

	if(subs->event.len == PWINFO_LEN && watcher_subs && force_active &&
			watcher_subs->status.len == 7)
	{
		DBG("PRESENCE: notify:Send notify for presence on callback");
		send_on_cback = 1;
	}
	printf_subs(subs);	

	cb_param = shm_dup_subs(watcher_subs, subs->to_tag, send_on_cback);
	if(cb_param == NULL)
	{
		LOG(L_ERR, "PRESENCE:notify:ERROR while duplicating cb_param in"
				" share memory\n");
		goto error;	
	}	
	
	printf_subs(subs);	
	if(send_on_cback)
	{
		printf_subs(cb_param->wi_subs);
	}
	
	if(final_body != NULL)
		DBG("body :\n:%.*s\n", final_body->len, final_body->s);	
			
	result = tmb.t_request_within
		(&met,						             
		str_hdr,                               
		final_body,                           
		td,					                  
		p_tm_callback,				        
		(void*)cb_param);				

	if(result < 0)
	{
		LOG(L_ERR, "PRESENCE:notify: ERROR in function tmb.t_request_within\n");
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
	
	update_keys[n_update_keys] = "cseq";
	update_vals[n_update_keys].type = DB_INT;
	update_vals[n_update_keys].nul = 0;
	update_vals[n_update_keys].val.int_val = subs->cseq +1;
	n_update_keys++;

	update_keys[n_update_keys] = "status";
	update_vals[n_update_keys].type = DB_STR;
	update_vals[n_update_keys].nul = 0;
	update_vals[n_update_keys].val.str_val = subs->status;
	n_update_keys++;


	if(PWINFO_LEN == subs->event.len)
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
	if(send_on_cback)
	{	
		subs->reason.len = 100;
	}
	if(p_uri.s!=NULL)
		pkg_free(p_uri.s);
	if(td!=NULL)
	{
		if(subs->event.len == PRES_LEN && td->rem_uri.s)
			pkg_free(td->rem_uri.s);
		free_tm_dlg(td);
	}
	if(str_hdr!=NULL)
		pkg_free(str_hdr);

	if(n_body == NULL)
	{
		if(final_body!=NULL)
		{
			if(final_body->s!=NULL)
				free(final_body->s);
			pkg_free(final_body);
		}
	}	
	else
		if(subs->event.len == PRES_LEN && rule_node)
		{
			if(final_body!=NULL)
			{
				if(final_body->s!=NULL)
					xmlFree(final_body->s);
				pkg_free(final_body);
			}
		}
	if(xcap_tree!=NULL)
		xmlFreeDoc(xcap_tree);

	return 0;

error:
	if(p_uri.s!=NULL)
		pkg_free(p_uri.s);
	if(td!=NULL)
	{
		if(subs->event.len == PRES_LEN && td->rem_uri.s)
			pkg_free(td->rem_uri.s);
		free_tm_dlg(td);
	}
	if(str_hdr!=NULL)
		pkg_free(str_hdr);

	if(n_body == NULL)
	{
		if(final_body!=NULL)
		{
			if(final_body->s!=NULL)
				xmlFree(final_body->s);
			pkg_free(final_body);
		}
	}	
	else
		if(subs->event.len == PRES_LEN && rule_node)
		{
			if(final_body!=NULL)
			{
				if(final_body->s!=NULL)
					free(final_body->s);
				pkg_free(final_body);
			}
		}
	if(xcap_tree!=NULL)
		xmlFreeDoc(xcap_tree);

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
	
	LOG(L_DBG, "PRESENCE:p_tm_callback: completed with status %d [watcher_id:"
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
	else
		if(((c_back_param*)(*ps->param))->wi_subs!= NULL)
		{
			if(notify( ((c_back_param*)(*ps->param))->wi_subs, NULL, NULL)< 0)
			{
				LOG(L_ERR, "PRESENCE:update_subscribtion: Could not send"
					" notify for presence\n");
			}
		}

done:
	if(*ps->param !=NULL  )
		shm_free(*ps->param);
	return ;

}

		
	
c_back_param* shm_dup_subs(subs_t* subs, str to_tag, int is_sec)
{
	int size;
	c_back_param* cb_param = NULL;

	size = sizeof(c_back_param) + to_tag.len +1;

	if(is_sec)
	{
		size+= sizeof(subs_t) + (subs->to_user.len+ 
			subs->to_domain.len+ subs->from_user.len+ subs->from_domain.len+
			subs->event.len +subs->event_id.len + subs->to_tag.len +
			subs->from_tag.len + subs->callid.len +subs->contact.len +
			subs->record_route.len +subs->status.len + subs->reason.len)
			*sizeof(char);
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
	
	if(!is_sec)
	{
		cb_param->wi_subs = NULL;
		return cb_param;
	}	

	size+= subs->to_tag.len + 1;

	cb_param->wi_subs = (subs_t*)((char*)cb_param + size);
	size+= sizeof(subs_t);

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

	cb_param->wi_subs->event.s = (char*)cb_param + size;
	strncpy(cb_param->wi_subs->event.s, subs->event.s, subs->event.len);
	cb_param->wi_subs->event.len = subs->event.len;
	size+= subs->event.len;

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
	size+= subs->contact.len;

	if(cb_param->wi_subs->record_route.s)
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
	cb_param->wi_subs->version = subs->version;

	return cb_param;

error:
	if(cb_param!= NULL)
		shm_free(cb_param);
	return NULL;
}



	




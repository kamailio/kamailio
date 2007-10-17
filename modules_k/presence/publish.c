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

#include <time.h>

#include "../../ut.h"
#include "../../str.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h" 
#include "../../parser/parse_expires.h" 
#include "../../parser/parse_event.h" 
#include "../../parser/parse_content.h" 
#include "../../lock_ops.h"
#include "../../hash_func.h"
#include "../../db/db.h"
#include "presence.h"
#include "notify.h"
#include "utils_func.h"
#include "publish.h"
#include "presentity.h"

extern db_con_t* pa_db;
extern db_func_t pa_dbf;
extern gen_lock_set_t* set;
extern int counter ;
extern int pid;
extern int startup_time;

static str pu_400a_rpl = str_init("Bad request");
static str pu_400b_rpl = str_init("Invalid request");

void msg_presentity_clean(unsigned int ticks,void *param)
{
	db_key_t db_keys[2];
	db_val_t db_vals[2];
	db_op_t  db_ops[2] ;
	db_key_t result_cols[6];
	db_res_t *result = NULL;
	db_row_t *row ;	
	db_val_t *row_vals ;
	int i =0, size= 0;
	presentity_t** p= NULL;
	presentity_t* pres= NULL;
	int n= 0;
	int event_col, etag_col, user_col, domain_col;
	event_t e;
	str user, domain, etag, event;
	int n_result_cols= 0;
	str pres_uri;
	str* rules_doc= NULL;

	if (pa_dbf.use_table(pa_db, presentity_table) < 0) 
	{
		LM_ERR("in use_table\n");
		return ;
	}
	
	LM_DBG("cleaning expired presentity information\n");

	db_keys[0] ="expires";
	db_ops[0] = OP_LT;
	db_vals[0].type = DB_INT;
	db_vals[0].nul = 0;
	db_vals[0].val.int_val = (int)time(NULL);
		
	result_cols[user_col= n_result_cols++] = "username";
	result_cols[domain_col=n_result_cols++] = "domain";
	result_cols[etag_col=n_result_cols++] = "etag";
	result_cols[event_col=n_result_cols++] = "event";

	if(pa_dbf.query(pa_db, db_keys, db_ops, db_vals, result_cols,
						1, n_result_cols, "username", &result )< 0)
	{
		LM_ERR("querying database for expired messages\n");
		if(result)
			pa_dbf.free_result(pa_db, result);
		return;
	}
	if(result== NULL)
		return;

	if(result && result->n<= 0)
	{
		pa_dbf.free_result(pa_db, result);	
		return;
	}
	LM_DBG("found n= %d expires messages\n ",result->n);

	n= result->n;
	
	p= (presentity_t**)pkg_malloc(n* sizeof(presentity_t*));
	if(p== NULL)
	{
		ERR_MEM(PKG_MEM_STR);	
	}
	memset(p, 0, n* sizeof(presentity_t*));

	for(i = 0; i< n; i++)
	{	
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);	
	
		user.s= (char*)row_vals[user_col].val.string_val;
		user.len= strlen(user.s);
		
		domain.s= (char*)row_vals[domain_col].val.string_val;
		domain.len= strlen(domain.s);

		etag.s= (char*)row_vals[etag_col].val.string_val;
		etag.len= strlen(etag.s);

		event.s= (char*)row_vals[event_col].val.string_val;
		event.len= strlen(event.s);
		
		size= sizeof(presentity_t)+ user.len+ domain.len+ etag.len; 
		pres= (presentity_t*)pkg_malloc(size);
		if(pres== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		memset(pres, 0, size);
		size= sizeof(presentity_t);
		
		pres->user.s= (char*)pres+ size;	
		memcpy(pres->user.s, user.s, user.len);
		pres->user.len= user.len;
		size+= user.len;

		pres->domain.s= (char*)pres+ size;
		memcpy(pres->domain.s, domain.s, domain.len);
		pres->domain.len= domain.len;
		size+= domain.len;

		pres->etag.s= (char*)pres+ size;
		memcpy(pres->etag.s, etag.s, etag.len);
		pres->etag.len= etag.len;
		size+= etag.len;
			
		pres->event= contains_event(&event, &e);
		if(pres->event== NULL)
		{
			LM_ERR("event not found\n");
			goto error;
		}	
		p[i]= pres;

		/* delete from hash table */
		if(uandd_to_uri(user, domain, &pres_uri)< 0)
		{
			LM_ERR("constructing uri\n");
			goto error;
		}

		if(delete_phtable(&pres_uri, e.parsed)< 0)
		{
			LM_ERR("deleting from pres hash table\n");
			pkg_free(pres_uri.s);
			goto error;
		}
		pkg_free(pres_uri.s);

	}
	pa_dbf.free_result(pa_db, result);
	result= NULL;
	
	for(i= 0; i<n ; i++)
	{
		LM_DBG("found expired publish for [user]=%.*s  [domanin]=%.*s\n",
			p[i]->user.len,p[i]->user.s, p[i]->domain.len, p[i]->domain.s);
		
		rules_doc= NULL;
		
		if(p[i]->event->get_rules_doc && 
		p[i]->event->get_rules_doc(&p[i]->user, &p[i]->domain, &rules_doc)< 0)
		{
			LM_ERR("getting rules doc\n");
			goto error;
		}
		if(publ_notify( p[i], NULL, &p[i]->etag, rules_doc)< 0)
		{
			LM_ERR("sending Notify request\n");
			goto error;
		}
		if(rules_doc)
		{
			if(rules_doc->s)
				pkg_free(rules_doc->s);
			pkg_free(rules_doc);
		}
		rules_doc= NULL;
	}

	if (pa_dbf.use_table(pa_db, presentity_table) < 0) 
	{
		LM_ERR("in use_table\n");
		goto error;
	}
	
	if (pa_dbf.delete(pa_db, db_keys, db_ops, db_vals, 1) < 0) 
		LM_ERR("cleaning expired messages\n");
	
	for(i= 0; i< n; i++)
	{
		if(p[i])
			pkg_free(p[i]);
	}
	pkg_free(p);

	return;

error:
	if(result)
		pa_dbf.free_result(pa_db, result);
	if(p)
	{
		for(i= 0; i< n; i++)
		{
			if(p[i])
				pkg_free(p[i]);
			else
				break;
		}
		pkg_free(p);
	}
	if(rules_doc)
	{
		if(rules_doc->s)
			pkg_free(rules_doc->s);
		pkg_free(rules_doc);
	}

	return;	
}

/**
 * PUBLISH request handling
 *
 */
int handle_publish(struct sip_msg* msg, char* sender_uri, char* str2)
{
	struct sip_uri puri;
	str body;
	int lexpire;
	presentity_t* presentity = 0;
	struct hdr_field* hdr;
	int found= 0, etag_gen = 0;
	str etag={0, 0};
	int error_ret = -1; 
	str* sender= NULL;
	static char buf[256];
	int buf_len= 255;
	pres_ev_t* event= NULL;
	str pres_user;
	str pres_domain;
	struct sip_uri pres_uri;

	counter++;
	if ( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LM_ERR("parsing headers\n");
		if (slb.reply(msg, 400, &pu_400a_rpl) == -1)
 			LM_ERR("Error while sending 400 reply\n");
		else
			error_ret = 0;
		return error_ret;
	}
	memset(&body, 0, sizeof(str));
	
	/* inspecting the Event header field */
	
	if(msg->event && msg->event->body.len > 0)
	{
		if (!msg->event->parsed && (parse_event(msg->event) < 0))
		{
			LM_ERR("cannot parse Event header\n");
			goto error;
		}
		if(((event_t*)msg->event->parsed)->parsed & EVENT_OTHER)
		{	
			goto unsupported_event;
		}
	}
	else
		goto unsupported_event;

	/* search event in the list */
	event= search_event((event_t*)msg->event->parsed);
	if(event== NULL)
	{
		goto unsupported_event;
	}
	
	/* examine the SIP-If-Match header field */
	hdr = msg->headers;
	while (hdr!= NULL)
	{
		if(strncmp(hdr->name.s, "SIP-If-Match",12)==0|| 
				strncmp(hdr->name.s,"Sip-If-Match",12)==0 )
		{
			found = 1;
			break;
		}
		hdr = hdr->next;
	}
	if(found==0 )
	{
		LM_DBG("SIP-If-Match header not found\n");
		etag.s = generate_ETag(0);
		if(etag.s == NULL)
		{
			LM_ERR("when generating etag\n");
			return -1;
		}
		etag.len=(strlen(etag.s));
		etag_gen=1;
		LM_DBG("new etag  = %.*s \n", etag.len,	etag.s);
	}
	else
	{
		LM_DBG("SIP-If-Match header found\n");
		etag.s = (char*)pkg_malloc((hdr->body.len+ 1)* sizeof(char));
		if(etag.s== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(etag.s, hdr->body.s, hdr->body.len );
		etag.len = hdr->body.len; 	 
		etag.s[ etag.len] = '\0';
		LM_DBG("existing etag  = %.*s \n", etag.len, etag.s);
	}

	/* examine the expire header field */
	if(msg->expires && msg->expires->body.len > 0)
	{
		if (!msg->expires->parsed && (parse_expires(msg->expires) < 0))
		{
			LM_ERR("cannot parse Expires header\n");
			goto error;
		}
		lexpire = ((exp_body_t*)msg->expires->parsed)->val;
		LM_DBG("Expires header found, value= %d\n", lexpire);

	}
	else 
	{
		LM_DBG("'expires' not found; default=%d\n",	event->default_expires);
		lexpire = event->default_expires;
	}
	if(lexpire > max_expires)
		lexpire = max_expires;

	/* get pres_uri from Request-URI*/
	if( parse_uri(msg->first_line.u.request.uri.s, 
				msg->first_line.u.request.uri.len, &pres_uri)< 0)
	{
		LM_ERR("parsing Request URI\n");
		goto error;
	}
	pres_user= pres_uri.user;
	pres_domain= pres_uri.host;

	if (!msg->content_length) 
	{
		LM_ERR("no Content-Length header found!\n");
		goto error;
	}	

	/* process the body */
	if ( get_content_length(msg) == 0 )
	{
		body.s = NULL;
		if (etag_gen)
		{
			LM_ERR("No E-Tag and no body found\n");
			if (slb.reply(msg, 400, &pu_400b_rpl) == -1)
				LM_ERR("sending 400 Invalid request reply\n");
			else
				error_ret = 0;
			goto error;
		}
	}
	else
	{
		body.s=get_body(msg);
		if (body.s== NULL) 
		{
			LM_ERR("cannot extract body\n");
			goto error;
		}
		body.len= get_content_length( msg );
	}	
	memset(&puri, 0, sizeof(struct sip_uri));
	if(sender_uri)
	{
		sender=(str*)pkg_malloc(sizeof(str));
		if(sender== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}	
		if(pv_printf(msg, (pv_elem_t*)sender_uri, buf, &buf_len)<0)
		{
			LM_ERR("cannot print the format\n");
			goto error;
		}
		if(parse_uri(buf, buf_len, &puri)!=0)
		{
			LM_ERR("bad owner SIP address!\n");
			goto error;
		} else 
		{
			LM_DBG("using user id [%.*s]\n",buf_len,buf);
		}
		sender->s= buf;
		sender->len= buf_len;
	}
	/* call event specific handling function*/
	if(event->evs_publ_handl)
	{
		if(event->evs_publ_handl(msg)< 0)
		{
			LM_ERR("in event specific publish handling\n");
			goto error;
		}
	}

	/* now we have all the necessary values */
	/* fill in the filds of the structure */

	presentity= new_presentity(&pres_domain, &pres_user, lexpire, event,
			&etag, sender);
	if(presentity== NULL)
	{
		LM_ERR("creating presentity structure\n");
		goto error;
	}

	/* querry the database and update or insert */
	if(update_presentity(msg, presentity, &body, etag_gen) <0)
	{
		LM_ERR("when updating presentity\n");
		goto error;
	}

	if(presentity)
		pkg_free(presentity);
	if(etag.s)
		pkg_free(etag.s);
	if(sender)
		pkg_free(sender);

	return 1;

error:
	
	if(presentity)
		pkg_free(presentity);
	if(etag.s)
		pkg_free(etag.s);
	if(sender)
		pkg_free(sender);
	
	return error_ret;

unsupported_event:
	
	LM_ERR("Missing or unsupported event header field value\n");
		
	if(msg->event && msg->event->body.s && msg->event->body.len>0)
		LM_ERR("\tevent=[%.*s]\n", msg->event->body.len, msg->event->body.s);
	
	if(reply_bad_event(msg)< 0)
		return -1;

	return 0;

}



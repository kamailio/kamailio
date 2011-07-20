/*
 * $Id$
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2006-08-15  initial version (anca)
 */

/*! \file
 * \brief Kamailio presence module :: Support for PUBLISH handling
 * \ingroup presence 
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
#include "../../lib/kcore/hash_func.h"
#include "../../lib/kcore/cmpapi.h"
#include "../../lib/srdb1/db.h"
#include "presence.h"
#include "notify.h"
#include "utils_func.h"
#include "publish.h"
#include "presentity.h"

extern gen_lock_set_t* set;

static str pu_400a_rpl = str_init("Bad request");
static str pu_400b_rpl = str_init("Invalid request");
static str pu_500_rpl  = str_init("Server Internal Error");
static str pu_489_rpl  = str_init("Bad Event");

struct p_modif
{
	presentity_t* p;
	str uri;
};

void msg_presentity_clean(unsigned int ticks,void *param)
{
	db_key_t db_keys[2];
	db_val_t db_vals[2];
	db_op_t  db_ops[2] ;
	db_key_t result_cols[6];
	db1_res_t *result = NULL;
	db_row_t *row ;	
	db_val_t *row_vals ;
	int i =0, size= 0;
	struct p_modif* p= NULL;
	presentity_t* pres= NULL;
	int n= 0;
	int event_col, etag_col, user_col, domain_col;
	event_t ev;
	str user, domain, etag, event;
	int n_result_cols= 0;
	str* rules_doc= NULL;


	if (pa_dbf.use_table(pa_db, &presentity_table) < 0) 
	{
		LM_ERR("in use_table\n");
		return ;
	}
	
	LM_DBG("cleaning expired presentity information\n");

	db_keys[0] = &str_expires_col;
	db_ops[0] = OP_LT;
	db_vals[0].type = DB1_INT;
	db_vals[0].nul = 0;
	db_vals[0].val.int_val = (int)time(NULL);
		
	result_cols[user_col= n_result_cols++] = &str_username_col;
	result_cols[domain_col=n_result_cols++] = &str_domain_col;
	result_cols[etag_col=n_result_cols++] = &str_etag_col;
	result_cols[event_col=n_result_cols++] = &str_event_col;

	static str query_str = str_init("username");
	if(pa_dbf.query(pa_db, db_keys, db_ops, db_vals, result_cols,
						1, n_result_cols, &query_str, &result )< 0)
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
	
	p= (struct p_modif*)pkg_malloc(n* sizeof(struct p_modif));
	if(p== NULL)
	{
		ERR_MEM(PKG_MEM_STR);	
	}
	memset(p, 0, n* sizeof(struct p_modif));

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
		
		size= sizeof(presentity_t)+ (user.len+ domain.len+ etag.len)*
			sizeof(char); 
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
			
		pres->event= contains_event(&event, &ev);
		if(pres->event== NULL)
		{
			LM_ERR("event not found\n");
			free_event_params(ev.params.list, PKG_MEM_TYPE);
			goto error;
		}	
	
		p[i].p= pres;
		if(uandd_to_uri(user, domain, &p[i].uri)< 0)
		{
			LM_ERR("constructing uri\n");
			free_event_params(ev.params.list, PKG_MEM_TYPE);
			goto error;
		}
		
		/* delete from hash table */
		if(delete_phtable(&p[i].uri, ev.type)< 0)
		{
			LM_ERR("deleting from pres hash table\n");
			free_event_params(ev.params.list, PKG_MEM_TYPE);
			goto error;
		}
		free_event_params(ev.params.list, PKG_MEM_TYPE);

	}
	pa_dbf.free_result(pa_db, result);
	result= NULL;
	
	for(i= 0; i<n ; i++)
	{
		LM_DBG("found expired publish for [user]=%.*s  [domanin]=%.*s\n",
			p[i].p->user.len,p[i].p->user.s, p[i].p->domain.len, p[i].p->domain.s);
		
		rules_doc= NULL;
		
		if(p[i].p->event->get_rules_doc && 
		p[i].p->event->get_rules_doc(&p[i].p->user, &p[i].p->domain, &rules_doc)< 0)
		{
			LM_ERR("getting rules doc\n");
			goto error;
		}
		if(publ_notify( p[i].p, p[i].uri, NULL, &p[i].p->etag, rules_doc)< 0)
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

	if (pa_dbf.use_table(pa_db, &presentity_table) < 0) 
	{
		LM_ERR("in use_table\n");
		goto error;
	}
	
	if (pa_dbf.delete(pa_db, db_keys, db_ops, db_vals, 1) < 0) 
		LM_ERR("cleaning expired messages\n");
	
	for(i= 0; i< n; i++)
	{
		if(p[i].p)
			pkg_free(p[i].p);
		if(p[i].uri.s)
			pkg_free(p[i].uri.s);

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
			
			if(p[i].p)
				pkg_free(p[i].p);
			if(p[i].uri.s)
				pkg_free(p[i].uri.s);
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
	str* sender= NULL;
	static char buf[256];
	int buf_len= 255;
	pres_ev_t* event= NULL;
	str pres_user;
	str pres_domain;
	int reply_code;
	str reply_str;
	int sent_reply= 0;
	char* sphere= NULL;

	reply_code= 500;
	reply_str= pu_500_rpl;

	counter++;
	if ( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LM_ERR("parsing headers\n");
		reply_code= 400;
		reply_str= pu_400a_rpl;
		goto error;
	}
	memset(&body, 0, sizeof(str));
	
	/* inspecting the Event header field */
	
	if(msg->event && msg->event->body.len > 0)
	{
		if (!msg->event->parsed && (parse_event(msg->event) < 0))
		{
			LM_ERR("cannot parse Event header\n");
			reply_code= 400;
			reply_str= pu_400a_rpl;
			goto error;
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
		if(cmp_hdrname_strzn(&hdr->name, "SIP-If-Match", 12)==0)
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
			goto error;
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
	if(parse_sip_msg_uri(msg)< 0)
	{
		LM_ERR("parsing Request URI\n");
		reply_code= 400; 
		reply_str= pu_400a_rpl;
		goto error;
	}
	pres_user= msg->parsed_uri.user;
	pres_domain= msg->parsed_uri.host;

	if (!msg->content_length) 
	{
		LM_ERR("no Content-Length header found!\n");
		reply_code= 400; 
		reply_str= pu_400a_rpl;
		goto error;
	}	

	/* process the body */
	if ( get_content_length(msg) == 0 )
	{
		body.s = NULL;
		if (etag_gen)
		{
			LM_ERR("No E-Tag and no body found\n");
			reply_code= 400;
			reply_str= pu_400b_rpl;
			goto error;
		}
	}
	else
	{
		body.s=get_body(msg);
		if (body.s== NULL) 
		{
			LM_ERR("cannot extract body\n");
			reply_code= 400; 
			reply_str= pu_400a_rpl;
			goto error;
		}
		body.len= get_content_length( msg );

		if(sphere_enable && event->evp->type == EVENT_PRESENCE &&
				get_content_type(msg)== SUBTYPE_PIDFXML)
		{
			sphere= extract_sphere(body);			
		}

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
			LM_ERR("bad sender SIP address!\n");
			reply_code= 400; 
			reply_str= pu_400a_rpl;
			goto error;
		} 
		else 
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
	if(update_presentity(msg, presentity, &body, etag_gen, &sent_reply, sphere) <0)
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
	if(sphere)
		pkg_free(sphere);

	return 1;

unsupported_event:
	
	LM_WARN("Missing or unsupported event header field value\n");
		
	if(msg->event && msg->event->body.s && msg->event->body.len>0)
		LM_ERR("    event=[%.*s]\n", msg->event->body.len, msg->event->body.s);

	reply_code= BAD_EVENT_CODE;
	reply_str=	pu_489_rpl; 

error:
	if(sent_reply== 0)
	{
		if(send_error_reply(msg, reply_code, reply_str)< 0)
		{
			LM_ERR("failed to send error reply\n");
		}
	}
	
	if(presentity)
		pkg_free(presentity);
	if(etag.s)
		pkg_free(etag.s);
	if(sender)
		pkg_free(sender);
	if(sphere)
		pkg_free(sphere);

	return -1;

}



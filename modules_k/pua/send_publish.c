/*
 * $Id$
 *
 * pua module - presence user agent module
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
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <time.h>

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../parser/parse_expires.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../tm/tm_load.h"
#include "pua.h"
#include "hash.h"
#include "send_publish.h"
#include "pua_callback.h"

extern struct tm_binds tmb;
int process_body(publ_info_t* publ, str** fin_body, int ver, str* tuple);
int pres_process_body(publ_info_t* publ, str** fin_body, int ver, str* tuple);
int bla_process_body (str* init_body, str** fin_body, int ver);
int mwi_process_body (str* init_body, str** fin_body, int ver);

xmlNodePtr xmlNodeGetNodeByName(xmlNodePtr node, const char *name,
															const char *ns)
{
	xmlNodePtr cur = node;
	while (cur) {
		xmlNodePtr match = NULL;
		if (xmlStrcasecmp(cur->name, (unsigned char*)name) == 0) {
			if (!ns || (cur->ns && xmlStrcasecmp(cur->ns->prefix,
							(unsigned char*)ns) == 0))
				return cur;
		}
		match = xmlNodeGetNodeByName(cur->children, name, ns);
		if (match)
			return match;
		cur = cur->next;
	}
	return NULL;
}
xmlAttrPtr xmlNodeGetAttrByName(xmlNodePtr node, const char *name)
{
	xmlAttrPtr attr = node->properties;
	while (attr) {
		if (xmlStrcasecmp(attr->name, (unsigned char*)name) == 0)
			return attr;
		attr = attr->next;
	}
	return NULL;
}

char *xmlNodeGetAttrContentByName(xmlNodePtr node, const char *name)
{
	xmlAttrPtr attr = xmlNodeGetAttrByName(node, name);
	if (attr)
		return (char*)xmlNodeGetContent(attr->children);
	else
		return NULL;
}

void print_hentity(ua_pres_t* h)
{
	DBG("\tpresentity:\n");
	DBG("\turi= %.*s\n", h->pres_uri->len, h->pres_uri->s);
	
	if(h->id.s)
		DBG("\tid= %.*s\n", h->id.len, h->id.s);

	if(h->tuple_id.s)
		DBG("\ttuple_id: %.*s\n", h->tuple_id.len, h->tuple_id.s);
}	

str* publ_build_hdr(int expires, int ev, str* content_type, str* etag,
		str* extra_headers, int is_body)
{
	static char buf[3000];
	str* str_hdr = NULL;	
	char* expires_s = NULL;
	int len = 0;
	int t= 0;
	str event= {0, 0};

	DBG("PUA: publ_build_hdr ...\n");
	str_hdr =(str*) pkg_malloc(sizeof(str));
	if(!str_hdr)
	{
		LOG(L_ERR, "PUA: publ_build_hdr:ERROR while allocating memory\n");
		return NULL;
	}
	memset(str_hdr, 0 , sizeof(str));
	memset(buf, 0, 2999);
	str_hdr->s = buf;
	str_hdr->len= 0;

	get_event_name(ev, &event);	
	if(event.s== NULL)
	{
		LOG(L_ERR, "PUA: publ_build_hdr:ERROR NULL event parameter\n");
		goto error;
	}

	memcpy(str_hdr->s ,"Event: ", 7);
	str_hdr->len = 7;
	memcpy(str_hdr->s+ str_hdr->len, event.s, event.len);
	str_hdr->len+= event.len;
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;
	

	memcpy(str_hdr->s+str_hdr->len ,"Expires: ", 9);
	str_hdr->len += 9;

	t= expires; 

	if( t<=0 )
	{
		t= min_expires;
	}
	else
	{
		t++;
	}
	expires_s = int2str(t, &len);

	memcpy(str_hdr->s+str_hdr->len, expires_s, len);
	str_hdr->len+= len;
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;
	
	if(etag)
	{
		DBG("PUA:publ_build_hdr: UPDATE_TYPE\n");
		memcpy(str_hdr->s+str_hdr->len,"SIP-If-Match: ", 14);
		str_hdr->len += 14;
		memcpy(str_hdr->s+str_hdr->len, etag->s, etag->len);
		str_hdr->len += etag->len;
		memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
		str_hdr->len += CRLF_LEN;
	}
	if(is_body)
	{	
		if(content_type== NULL || content_type->s== NULL || content_type->len== 0)
		{
			LOG(L_ERR, "PUA: publ_build_hdr:ERROR NULL content_type parameter\n");
			goto error;
		}
		memcpy(str_hdr->s+str_hdr->len,"Content-Type: ", 14);
		str_hdr->len += 14;
		memcpy(str_hdr->s+str_hdr->len, content_type->s, content_type->len);
		str_hdr->len += content_type->len;
		memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
		str_hdr->len += CRLF_LEN;
	}

	if(extra_headers && extra_headers->s && extra_headers->len)
	{
		memcpy(str_hdr->s+str_hdr->len,extra_headers->s , extra_headers->len);
		str_hdr->len += extra_headers->len;
	}	
	str_hdr->s[str_hdr->len] = '\0';
	
	return str_hdr;

error:
	pkg_free(str_hdr);
	return NULL;
}

void publ_cback_func(struct cell *t, int type, struct tmcb_params *ps)
{
	struct hdr_field* hdr= NULL;
	struct sip_msg* msg= NULL;
	ua_pres_t* presentity= NULL;
	treq_cbparam_t* hentity= NULL;
	int found = 0;
	int size= 0;
	int lexpire= 0;
	str etag;
	int desired_expires;
	unsigned int hash_code;

	if(ps->param== NULL)
	{
		LOG(L_ERR, "PUA:publ_cback_func: Error NULL callback parameter\n");
		goto error;
	}
	
	if( ps->code>= 300 )
	{
		if( *ps->param== NULL ) 
		{
			DBG("PUA publ_cback_func: NULL callback parameter\n");
			return;
		}
		hentity= (treq_cbparam_t*)(*ps->param);

		hash_code= core_hash(hentity->publ.pres_uri, NULL,
					HASH_SIZE);
		lock_get(&HashT->p_records[hash_code].lock);
		presentity= search_htable( hentity->publ.pres_uri, NULL, hentity->publ.source_flag,
				hentity->publ.id, hentity->publ.etag, hash_code);
		if(presentity)
		{
			if(ps->code== 412 && hentity->publ.body)
			{
				/* sent a PUBLISH within a dialog that no longer exists
				 * send again an intial PUBLISH */
				DBG("PUA:publ_cback_func: received s 412 reply- send an"
						" INSERT_TYPE publish request\n");
				delete_htable(presentity, hash_code);
				lock_release(&HashT->p_records[hash_code].lock);
				publ_info_t publ;
				memset(&publ, 0, sizeof(publ_info_t));
				publ.pres_uri= hentity->publ.pres_uri; 
				publ.body= hentity->publ.body;
				publ.expires= hentity->publ.expires;
				publ.flag|= INSERT_TYPE;
				publ.source_flag|= hentity->publ.source_flag;
				publ.event|= hentity->publ.event;
				publ.etag= hentity->publ.etag;
				publ.content_type= hentity->publ.content_type;	
				publ.id= hentity->publ.id;
				publ.extra_headers= hentity->publ.extra_headers;
				if(send_publish(&publ)< 0)
				{
					LOG(L_ERR, "PUA:publ_cback_func: ERROR when trying to send PUBLISH\n");
					goto error;
				}
			}
			else
			{	
				delete_htable(presentity, hash_code);
				DBG("PUA:publ_cback_func: ***Delete from table\n");
				lock_release(&HashT->p_records[hash_code].lock);
			}
		}
		else
			lock_release(&HashT->p_records[hash_code].lock);
		goto error;
	}

	msg= ps->rpl;
	if(msg == NULL)
	{
		LOG(L_ERR, "PUA:publ_cback_func: ERROR no reply message found\n ");
		goto error;
	}
	if(msg== FAKED_REPLY)
	{
		DBG("PUA:publ_cback_func: FAKED_REPLY\n");
		goto error;
	}

	if( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LOG(L_ERR, "PUA:publ_cback_func: error parsing headers\n");
		goto error;
	}	
	if(msg->expires== NULL || msg->expires->body.len<= 0)
	{
			LOG(L_ERR, "PUA:publ_cback_func: ERROR No Expires header found\n");
			goto error;
	}	
	
	if (!msg->expires->parsed && (parse_expires(msg->expires) < 0))
	{
		LOG(L_ERR, "PUA:publ_cback_func: ERROR cannot parse Expires"
					" header\n");
		goto error;
	}
	lexpire = ((exp_body_t*)msg->expires->parsed)->val;
	DBG("PUA:publ_cback_func: lexpire= %d\n", lexpire);
		
	hdr = msg->headers;
	while (hdr!= NULL)
	{
		if(strncmp(hdr->name.s, "SIP-ETag",8)==0 )
		{
			found = 1;
			break;
		}
		hdr = hdr->next;
	}
	if(found== 0) /* must find SIP-Etag header field in 200 OK msg*/
	{	
		LOG(L_ERR, "PUA:publ_cback_func:no SIP-ETag header field found\n");
		goto error;
	}
	etag= hdr->body;

	/* if publish with 0 the callback parameter is NULL*/
	if(lexpire== 0 && *ps->param== NULL )	
	{
		DBG("PUA: publ_cback_func: reply with expires= 0 and entity already"
				" deleted\n");
		return;
	}

	if( *ps->param== NULL ) 
	{
		DBG("PUA publ_cback_func: Error NULL callback parameter\n");
		return;
	}
		
	hentity= (treq_cbparam_t*)(*ps->param);

	LOG(L_DBG, "PUA:publ_cback_func: completed with status %d [contact:"
			"%.*s]\n",ps->code, hentity->publ.pres_uri->len, hentity->publ.pres_uri->s);

	desired_expires= hentity->publ.expires<0?0: hentity->publ.expires+ (int)time(NULL);

	hash_code= core_hash(hentity->publ.pres_uri, NULL, HASH_SIZE);
	lock_get(&HashT->p_records[hash_code].lock);
	
	presentity= search_htable(hentity->publ.pres_uri, NULL,hentity->publ.source_flag,
			hentity->publ.id, hentity->publ.etag,  hash_code);
	if(presentity)
	{
			DBG("PUA:publ_cback_func: update record\n");
			if(lexpire == 0)
			{
				DBG("PUA:publ_cback_func: expires= 0- delete from htable\n"); 
				delete_htable(presentity, hash_code);
				lock_release(&HashT->p_records[hash_code].lock);
				goto done;
			}
			
			update_htable(presentity, desired_expires,
					lexpire, &etag, hash_code);
			lock_release(&HashT->p_records[hash_code].lock);
			goto done;
	}
	lock_release(&HashT->p_records[hash_code].lock);

	if(lexpire== 0)
	{
		LOG(L_ERR, "PUA:publ_cback_func:expires= 0: no not insert\n");
		goto error;
	}
	size= sizeof(ua_pres_t)+ sizeof(str)+ 
		(hentity->publ.pres_uri->len+ hentity->tuple_id.len + 
		 hentity->publ.id.len)* sizeof(char);
	presentity= (ua_pres_t*)shm_malloc(size);
	if(presentity== NULL)
	{
		LOG(L_ERR,"PUA:publ_cback_func: ERROR no more share memory\n");
		goto error;
	}	
	memset(presentity, 0, size);
	size= sizeof(ua_pres_t);
	presentity->pres_uri= (str*)((char*)presentity+ size);
	size+= sizeof(str);

	presentity->pres_uri->s= (char*)presentity+ size;
	memcpy(presentity->pres_uri->s, hentity->publ.pres_uri->s, 
			hentity->publ.pres_uri->len);
	presentity->pres_uri->len= hentity->publ.pres_uri->len;
	size+= hentity->publ.pres_uri->len;
	
	presentity->tuple_id.s= (char*)presentity+ size;
	memcpy(presentity->tuple_id.s, hentity->tuple_id.s,
			hentity->tuple_id.len);
	presentity->tuple_id.len= hentity->tuple_id.len;
	size+= presentity->tuple_id.len;

	presentity->id.s=(char*)presentity+ size;
	memcpy(presentity->id.s, hentity->publ.id.s, 
			hentity->publ.id.len);
	presentity->id.len= hentity->publ.id.len; 
	size+= presentity->id.len;
		
	presentity->expires= lexpire +(int)time(NULL);
	presentity->desired_expires= desired_expires;
	presentity->flag|= hentity->publ.source_flag;
	presentity->event|= hentity->publ.event;
	presentity->db_flag|= INSERTDB_FLAG;

	presentity->etag.s= (char*)shm_malloc(etag.len* sizeof(char));
	if(presentity->etag.s== NULL)
	{
		LOG(L_ERR, "PUA: publ_cback_func: ERROR No more share memory\n");
		goto error;
	}
	memcpy(presentity->etag.s, etag.s, etag.len);
	presentity->etag.len= etag.len;

	insert_htable( presentity);
	DBG("PUA: publ_cback_func: ***Inserted in hash table\n");		

done:

	if(hentity->publ.cbrpl )
	{
		if( hentity->publ.cbrpl(msg,  hentity->publ.cbparam)< 0)
		{
			LOG(L_ERR,"PUA:publ_cback_func: in callback function ERROR ");
			goto error;
		}	
			
	}	
	if(*ps->param)
	{
		shm_free(*ps->param);
		*ps->param= NULL;
	}


	return;

error:
	if(*ps->param)
	{
		shm_free(*ps->param);
		*ps->param= NULL;
	}

	return;

}	

int send_publish( publ_info_t* publ )
{
	str met = {"PUBLISH", 7};
	str* str_hdr = NULL;
	ua_pres_t* presentity= NULL;
	unsigned int size= 0;
	str* body= NULL;
	str* tuple_id= NULL;
	treq_cbparam_t* cb_param= NULL;
	unsigned int hash_code;
	str etag= {0, 0};
	int ver= 0;
	int result;

	DBG("PUA: send_publish for: uri=%.*s\n", publ->pres_uri->len,
			publ->pres_uri->s );
	

	hash_code= core_hash(publ->pres_uri, NULL, HASH_SIZE);

	lock_get(&HashT->p_records[hash_code].lock);
	
	presentity= search_htable(publ->pres_uri, NULL,
			     publ->source_flag, publ->id, publ->etag, hash_code);

	if(publ->flag & INSERT_TYPE)
		goto insert;
	
	if(publ->etag && presentity== NULL)
		return 418;

	if(presentity== NULL)
	{
insert:	
		lock_release(&HashT->p_records[hash_code].lock);
		DBG("PUA: send_publish: insert type\n"); 
		
		if(publ->flag & UPDATE_TYPE )
		{
			DBG("PUA: send_publish: UPDATE_TYPE and no record found \n");
			publ->flag= INSERT_TYPE;
		}
		if(publ->expires== 0)
		{
			DBG("PUA: send_publish: request for a publish with expires 0 and"
					" no record found\n");
			return 0;
		}
		if(publ->body== NULL)
		{
			LOG(L_INFO, "PUA: send_publish: New PUBLISH and no body found\n");
			return -1;
		}
	}
	else
	{
		publ->flag= UPDATE_TYPE;
		etag.s= (char*)pkg_malloc(presentity->etag.len* sizeof(char));
		if(etag.s== NULL)
		{
			LOG(L_ERR, "PUA:send_publish: ERROR while allocating memory\n");
			lock_release(&HashT->p_records[hash_code].lock);
			return -1;
		}
		memcpy(etag.s, presentity->etag.s, presentity->etag.len);
		etag.len= presentity->etag.len;

		if(publ->expires== 0)
		{
			DBG("PUA:send_publish: expires= 0- delete from hash table\n");
			delete_htable(presentity, hash_code);
			presentity= NULL;
			lock_release(&HashT->p_records[hash_code].lock);
			goto send_publish;
		}
		presentity->version++;
		ver= presentity->version;
		lock_release(&HashT->p_records[hash_code].lock);
	}

    /* handle body */

	if(publ->body && publ->body->s)
	{
		if( (process_body(publ, &body, ver, tuple_id)< 0) || body== NULL)
		{
			LOG(L_ERR, "PUA:send_publish: ERROR while processing body\n");
			if(body== NULL)
				LOG(L_ERR, "PUA:send_publish: ERROR NULL body\n");
			goto error;
		}
	}
	/* construct the callback parameter */

	size= sizeof(treq_cbparam_t)+ sizeof(str)+ (publ->pres_uri->len+ 
		+ publ->content_type.len+ publ->id.len+ 1)*sizeof(char);
	if(body && body->s && body->len)
		size+= sizeof(str)+ body->len* sizeof(char);
	if(publ->etag)
		size+= sizeof(str)+ publ->etag->len* sizeof(char);
	if(publ->extra_headers)
		size+= sizeof(str)+ publ->extra_headers->len* sizeof(char);
	if(tuple_id )
		size+= tuple_id->len* sizeof(char);

	DBG("PUA: send_publish: before allocating size= %d\n", size);
	cb_param= (treq_cbparam_t*)shm_malloc(size);
	if(cb_param== NULL)
	{
		LOG(L_ERR, "PUA: send_publish: ERROR no more share memory while"
				" allocating cb_param - size= %d\n", size);
		goto error;
	}
	memset(cb_param, 0, size);
	size =  sizeof(treq_cbparam_t);
	DBG("PUA: send_publish: size= %d\n", size);
	
	cb_param->publ.pres_uri = (str*)((char*)cb_param + size);
	size+= sizeof(str);

	cb_param->publ.pres_uri->s = (char*)cb_param + size;
	memcpy(cb_param->publ.pres_uri->s, publ->pres_uri->s ,
			publ->pres_uri->len ) ;
	cb_param->publ.pres_uri->len= publ->pres_uri->len;
	size+= publ->pres_uri->len;

	if(publ->id.s && publ->id.len)
	{	
		cb_param->publ.id.s = ((char*)cb_param+ size);
		memcpy(cb_param->publ.id.s, publ->id.s, publ->id.len);
		cb_param->publ.id.len= publ->id.len;
		size+= publ->id.len;
	}

	if(body && body->s && body->len)
	{
		cb_param->publ.body = (str*)((char*)cb_param  + size);
		size+= sizeof(str);
		
		cb_param->publ.body->s = (char*)cb_param + size;
		memcpy(cb_param->publ.body->s, body->s ,
			body->len ) ;
		cb_param->publ.body->len= body->len;
		size+= body->len;
	}
	if(publ->etag)
	{
		cb_param->publ.etag = (str*)((char*)cb_param  + size);
		size+= sizeof(str);
		cb_param->publ.etag->s = (char*)cb_param + size;
		memcpy(cb_param->publ.etag->s, publ->etag->s ,
			publ->etag->len ) ;
		cb_param->publ.etag->len= publ->etag->len;
		size+= publ->etag->len;
	}
	if(publ->extra_headers)
	{
		cb_param->publ.extra_headers = (str*)((char*)cb_param  + size);
		size+= sizeof(str);
		cb_param->publ.extra_headers->s = (char*)cb_param + size;
		memcpy(cb_param->publ.extra_headers->s, publ->extra_headers->s ,
			publ->extra_headers->len ) ;
		cb_param->publ.extra_headers->len= publ->extra_headers->len;
		size+= publ->extra_headers->len;
	}	

	if(publ->content_type.s && publ->content_type.len)
	{
		cb_param->publ.content_type.s= (char*)cb_param + size;
		memcpy(cb_param->publ.content_type.s, publ->content_type.s, publ->content_type.len);
		cb_param->publ.content_type.len= publ->content_type.len;
		size+= cb_param->publ.content_type.len;

	}	
	if(tuple_id)
	{	
		cb_param->tuple_id.s = (char*)cb_param+ size;
		memcpy(cb_param->tuple_id.s, tuple_id->s ,tuple_id->len);
		cb_param->tuple_id.len= tuple_id->len;
		size+= tuple_id->len;
	}
	cb_param->publ.cbparam= publ->cbparam;
	cb_param->publ.cbrpl= publ->cbrpl;
	cb_param->publ.event= publ->event;
	cb_param->publ.source_flag|= publ->source_flag;
	cb_param->publ.expires= publ->expires;
	
send_publish:
	
	if(publ->flag & UPDATE_TYPE)
		DBG("PUA:send_publish: etag:%.*s\n", etag.len, etag.s);
	str_hdr = publ_build_hdr((publ->expires< 0)?3600:publ->expires, publ->event, &publ->content_type, 
				(publ->flag & UPDATE_TYPE)?&etag:NULL, publ->extra_headers, (body)?1:0);

	if(str_hdr == NULL)
	{
		LOG(L_ERR, "PUA:send_publish: ERROR while building extra_headers\n");
		goto error;
	}

	DBG("PUA: send_publish: publ->pres_uri:\n%.*s\n ", publ->pres_uri->len, publ->pres_uri->s);
	DBG("PUA: send_publish: str_hdr:\n%.*s %d\n ", str_hdr->len, str_hdr->s, str_hdr->len);
	if(body && body->len && body->s )
		DBG("PUA: send_publish: body:\n%.*s\n ", body->len, body->s);

	result= tmb.t_request(&met,				/* Type of the message */
			publ->pres_uri,					/* Request-URI */
			publ->pres_uri,					/* To */
			publ->pres_uri,					/* From */
			str_hdr,						/* Optional headers */
			body,							/* Message body */
			0,								/* outbound proxy*/
			publ_cback_func,				/* Callback function */
			(void*)cb_param					/* Callback parameter */
			);

	if(result< 0)
	{
		LOG(L_ERR, "PUA: send_publish: ERROR while sending request\n");
		goto error;
	}

	pkg_free(str_hdr);

	if( body)
	{
		if(body->s)
			free(body->s);
		pkg_free(body);
	}	
	if(etag.s)
		pkg_free(etag.s);
	if(tuple_id)
	{
		if(tuple_id->s)
			pkg_free(tuple_id->s);
		pkg_free(tuple_id);
	}

	return 0;

error:
	if(etag.s)
		pkg_free(etag.s);

	if(cb_param)
	{
		shm_free(cb_param);
	}	
	if(body)
	{
		if(body->s)
			free(body->s);
		pkg_free(body);
	}	
	if(str_hdr)
		pkg_free(str_hdr);
	if(tuple_id)
	{
		if(tuple_id->s)
			pkg_free(tuple_id->s);
		pkg_free(tuple_id);
	}
	return -1;
}

int process_body(publ_info_t* publ, str** fin_body, int ver, str* tuple)
{
	tuple= NULL;
	switch(publ->event)
	{
		case(PRESENCE_EVENT): {
			return pres_process_body(publ, fin_body, ver, tuple);
		}  
		case(BLA_EVENT): {
			return bla_process_body(publ->body, fin_body, ver);
		}  
		case(MSGSUM_EVENT): {
			return mwi_process_body(publ->body, fin_body, ver);
		}  
	}
	return -1;
}

int pres_process_body(publ_info_t* publ, str** fin_body, int ver, str* tuple)
{

	xmlDocPtr doc= NULL;
	xmlNodePtr node= NULL;
	char* tuple_id= NULL, *person_id= NULL;
	int tuple_id_len= 0;
	char buf[50];
	str* body= NULL;

	tuple= NULL;
	doc= xmlParseMemory(publ->body->s, publ->body->len );
	if(doc== NULL)
	{
		LOG(L_ERR, "PUA: pres_process_body: ERROR while parsing xml memory\n");
		goto error;
	}

	node= xmlNodeGetNodeByName(doc->children, "tuple", NULL);
	if(node == NULL)
	{
		LOG(L_ERR, "PUA: pres_process_body:ERROR while extracting tuple node\n");
		goto error;
	}
	tuple_id= xmlNodeGetAttrContentByName(node, "id");
	if(tuple_id== NULL)
	{	
		tuple_id= buf;
		tuple_id_len= sprintf(tuple_id, "%p", publ);
		tuple_id[tuple_id_len]= '\0'; 
		
		if(!xmlNewProp(node, BAD_CAST "id", BAD_CAST tuple_id))
		{
			LOG(L_ERR, "PUA: pres_process_body:ERROR while extracting xml"
						" node\n");
			goto error;
		}
	}
	else
	{
		strcpy(buf, tuple_id);
		xmlFree(tuple_id);
		tuple_id= buf;
		tuple_id_len= strlen(tuple_id);
	}	
	node= xmlNodeGetNodeByName(doc->children, "person", NULL);
	if(node)
	{
		DBG("PUA: pres_process_body:found person node\n");
		person_id= xmlNodeGetAttrContentByName(node, "id");
		if(person_id== NULL)
		{	
			if(!xmlNewProp(node, BAD_CAST "id", BAD_CAST tuple_id))
			{
				LOG(L_ERR, "PUA:pres_process_body:ERROR while extracting xml"
						" node\n");
				goto error;
			}
		}
		else
		{
			xmlFree(person_id);
		}
	}	
	body= (str*)pkg_malloc(sizeof(str));
	if(body== NULL)
	{
		LOG(L_ERR, "PUA:pres_process_body ERROR NO more memory left\n");
		goto error;
	}
	memset(body, 0, sizeof(str));
	xmlDocDumpFormatMemory(doc,(xmlChar**)(void*)&body->s, &body->len, 1);	
	if(body->s== NULL || body->len== 0)
	{
		LOG(L_ERR, "PUA: pres_process_body: ERROR while dumping xml format\n");
		goto error;
	}	
	xmlFreeDoc(doc);
	doc= NULL;
	
	tuple= (str*)pkg_malloc(sizeof(str));
	if(tuple== NULL)
	{
		LOG(L_ERR, "PUA: pres_process_body: ERROR No more memory\n");
		goto error;
	}
	tuple->s= tuple_id;
	tuple->len= tuple_id_len;
	
	*(fin_body)= body;
	xmlMemoryDump();
	xmlCleanupParser();
	return 0;

error:
	if(doc)
		xmlFreeDoc(doc);
	if(body)
		pkg_free(body);
	return -1;

}	

int bla_process_body(str* init_body, str** fin_body, int ver)
{
	xmlNodePtr node= NULL;
	xmlDocPtr doc= NULL;
	char* version;
	str* body= NULL;
	int len;

	DBG("PUA: bla_process_body: start\n");
	doc= xmlParseMemory(init_body->s, init_body->len );
	if(doc== NULL)
	{
		LOG(L_ERR, "PUA: bla_process_body: ERROR while parsing xml memory\n");
		goto error;
	}
	/* change version and state*/
	node= xmlNodeGetNodeByName(doc->children, "dialog-info", NULL);
	if(node == NULL)
	{
		LOG(L_ERR, "PUA: bla_process_body: ERROR while extracting dialog-info node\n");
		goto error;
	}
	version= int2str(ver,&len);
	version[len]= '\0';

	if( xmlSetProp(node, (const xmlChar *)"version",(const xmlChar*)version)== NULL)
	{
		LOG(L_ERR, "PUA: bla_process_body: ERROR while setting version attribute\n");
		goto error;	
	}
	body= (str*)pkg_malloc(sizeof(str));
	if(body== NULL)
	{
		LOG(L_ERR, "PUA: bla_process_body: ERROR NO more memory left\n");
		goto error;
	}
	memset(body, 0, sizeof(str));
	xmlDocDumpFormatMemory(doc, (xmlChar**)(void*)&body->s, &body->len, 1);	

	xmlFreeDoc(doc);
	doc= NULL;
	*(fin_body)= body;	
	if(*fin_body== NULL)
		DBG("PUA: bla_process_body: NULL fin_body\n");

	xmlMemoryDump();
	xmlCleanupParser();
	DBG("PUA: bla_process_body: successful\n");
	return 0;

error:
	if(doc)
		xmlFreeDoc(doc);
	if(body)
		pkg_free(body);
	
	xmlMemoryDump();
	xmlCleanupParser();
	return -1;
}

int mwi_process_body(str* init_body, str** fin_body,  int ver)
{
	return 0;
}


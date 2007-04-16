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

extern struct tm_binds tmb;

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

str* publ_build_hdr(int expires, int event, str* etag, str* extra_headers, int is_body)
{
	static char buf[3000];
	str* str_hdr = NULL;	
	char* expires_s = NULL;
	int len = 0;
	int t= 0;

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

	if(event & PRESENCE_EVENT)
	{
		memcpy(str_hdr->s ,"Event: presence", 15);
		str_hdr->len = 15;
	}
	else
	if(event & BLA_EVENT)	
	{
		memcpy(str_hdr->s ,"Event: dialog;sla", 17);
		str_hdr->len = 17;	
	}
	else
	{
		LOG(L_ERR, "PUA: publ_build_hdr:ERROR BAD event value\n");
		pkg_free(str_hdr);
		return NULL;
	}	
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
		if(event & PRESENCE_EVENT)
		{
			memcpy(str_hdr->s+str_hdr->len,"Content-Type: application/pidf+xml" , 34);
			str_hdr->len += 34;
			memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
			str_hdr->len += CRLF_LEN;
		}
		else
		{
			memcpy(str_hdr->s+str_hdr->len,"Content-Type: application/dialog-info+xml" , 41);
			str_hdr->len += 41;
			memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
			str_hdr->len += CRLF_LEN;		
		}	
	}

	if(extra_headers && extra_headers->s && extra_headers->len)
	{
		memcpy(str_hdr->s+str_hdr->len,extra_headers->s , extra_headers->len);
		str_hdr->len += extra_headers->len;
	}	
	str_hdr->s[str_hdr->len] = '\0';
	
	return str_hdr;
}

void publ_cback_func(struct cell *t, int type, struct tmcb_params *ps)
{
	struct hdr_field* hdr= NULL;
	struct sip_msg* msg= NULL;
	ua_pres_t* presentity= NULL;
	ua_pres_t* hentity= NULL;
	int found = 0;
	int size= 0;
	int lexpire= 0;
	str etag;
	unsigned int hash_code;

	if(ps->param== NULL)
	{
		LOG(L_ERR, "PUA:publ_cback_func: Error NULL callback parameter\n");
		goto done;
	}
	if( ps->code>= 300 )
	{
		if( *ps->param== NULL ) 
		{
			DBG("PUA publ_cback_func: NULL callback parameter\n");
			return;
		}
		hentity= (ua_pres_t*)(*ps->param);
		hash_code= core_hash(hentity->pres_uri, NULL,
					HASH_SIZE);
		lock_get(&HashT->p_records[hash_code].lock);
		presentity= search_htable( hentity->pres_uri, NULL, hentity->flag,
								   hentity->id, hash_code);
		if(presentity)
		{
			if(ps->code== 412 && hentity->body)
			{
				/* sent a PUBLISH within a dialog that no longer exists
				 * send again an intial PUBLISH */
				DBG("PUA:publ_cback_func: received s 412 reply- send an"
						" INSERT_TYPE publish request\n");
				delete_htable(presentity, hash_code);
				lock_release(&HashT->p_records[hash_code].lock);
				publ_info_t publ;
				memset(&publ, 0, sizeof(publ_info_t));
				publ.pres_uri= hentity->pres_uri; 
				publ.body= hentity->body;
				publ.expires= (hentity->desired_expires>0)?
					hentity->desired_expires- (int)time(NULL)+ 10:-1;
				publ.flag|= INSERT_TYPE;
				publ.source_flag|= hentity->flag;
				publ.event|= hentity->event;
				publ.id= hentity->id;
				if(send_publish(&publ)< 0)
				{
					LOG(L_ERR, "PUA:publ_cback_func: ERROR when trying to send PUBLISH\n");
					goto done;
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
		goto done;
	}

	msg= ps->rpl;
	if(msg == NULL || msg== FAKED_REPLY)
	{
		LOG(L_ERR, "PUA:publ_cback_func: ERROR no reply message found\n ");
		goto done;
	}
	if( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LOG(L_ERR, "PUA:publ_cback_func: error parsing headers\n");
		goto done;
	}	
	if(ps->rpl->expires && msg->expires->body.len > 0)
	{
		if (!msg->expires->parsed && (parse_expires(msg->expires) < 0))
		{
			LOG(L_ERR, "PUA:publ_cback_func: ERROR cannot parse Expires"
					" header\n");
			goto done;
		}
		lexpire = ((exp_body_t*)msg->expires->parsed)->val;
		DBG("PUA:publ_cback_func: lexpire= %d\n", lexpire);
	}

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
	hentity= (ua_pres_t*)(*ps->param);

	LOG(L_DBG, "PUA:publ_cback_func: completed with status %d [contact:"
			"%.*s]\n",ps->code, hentity->pres_uri->len, hentity->pres_uri->s);

	DBG("PUA: publ_cback_func: searched presentity:\n");
	print_hentity( hentity);

	hash_code= core_hash(hentity->pres_uri, NULL, HASH_SIZE);
	lock_get(&HashT->p_records[hash_code].lock);
	
	presentity= search_htable(hentity->pres_uri, NULL,hentity->flag,
			hentity->id, hash_code);
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
			update_htable(presentity, hentity->desired_expires,
					lexpire, hash_code);
			lock_release(&HashT->p_records[hash_code].lock);
			goto done;
	}
	lock_release(&HashT->p_records[hash_code].lock);

	if(lexpire== 0)
	{
		LOG(L_ERR, "PUA:publ_cback_func:expires= 0: no not insert\n");
		goto done;
	}
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
		goto done;
	}
	etag= hdr->body;
	size= sizeof(ua_pres_t)+ sizeof(str)+ 
		(((ua_pres_t*)(*ps->param))->pres_uri->len+etag.len+ 
		 ((ua_pres_t*)(*ps->param))->tuple_id.len + 
		 ((ua_pres_t*)(*ps->param))->id.len+ 1)* sizeof(char);
	presentity= (ua_pres_t*)shm_malloc(size);
	if(presentity== NULL)
	{
		LOG(L_ERR,"PUA:publ_cback_func: ERROR no more share memory\n");
		goto done;
	}	
	memset(presentity, 0, size);
	size= sizeof(ua_pres_t);
	presentity->pres_uri= (str*)((char*)presentity+ size);
	size+= sizeof(str);

	presentity->pres_uri->s= (char*)presentity+ size;
	memcpy(presentity->pres_uri->s,
			((ua_pres_t*)(*ps->param))->pres_uri->s,
			((ua_pres_t*)(*ps->param))->pres_uri->len);
	presentity->pres_uri->len= 
		((ua_pres_t*)(*ps->param))->pres_uri->len;
	size+= ((ua_pres_t*)(*ps->param))->pres_uri->len;
	presentity->etag.s= (char*)presentity+ size;
	memcpy(presentity->etag.s, etag.s, etag.len);
	presentity->etag.len= etag.len;
	size+= etag.len;

	presentity->tuple_id.s= (char*)presentity+ size;
	memcpy(presentity->tuple_id.s,
			((ua_pres_t*)(*ps->param))->tuple_id.s,
			((ua_pres_t*)(*ps->param))->tuple_id.len);
	presentity->tuple_id.len= ((ua_pres_t*)(*ps->param))->tuple_id.len;
	size+= presentity->tuple_id.len;

	presentity->id.s=(char*)presentity+ size;
	memcpy(presentity->id.s, ((ua_pres_t*)(*ps->param))->id.s, 
			((ua_pres_t*)(*ps->param))->id.len);
	presentity->id.len= ((ua_pres_t*)(*ps->param))->id.len; 
	size+= presentity->id.len;
		
	presentity->expires= lexpire +(int)time(NULL);
	presentity->desired_expires= ((ua_pres_t*)(*ps->param))->desired_expires;
	presentity->flag|= ((ua_pres_t*)(*ps->param))->flag;
	presentity->event|= ((ua_pres_t*)(*ps->param))->event;
	presentity->db_flag|= INSERTDB_FLAG;

	insert_htable( presentity);
	DBG("PUA: publ_cback_func: ***Inserted in hash table\n");		
		

done:
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
	char buf[50];
	unsigned int size= 0;
	str* body= NULL;
	xmlDocPtr doc= NULL;
	xmlNodePtr node= NULL;
	char* tuple_id= NULL, *person_id= NULL;
	int tuple_id_len= 0;
	ua_pres_t* hentity= NULL;
	unsigned int hash_code;
	str etag= {0, 0};
	int ver= 0;

	DBG("PUA: send_publish for: uri=%.*s\n", publ->pres_uri->len,
			publ->pres_uri->s );

	hash_code= core_hash(publ->pres_uri, NULL, HASH_SIZE);

	lock_get(&HashT->p_records[hash_code].lock);
	
	presentity= search_htable(publ->pres_uri, NULL,
			     publ->source_flag, publ->id,hash_code);

	if(publ->flag & INSERT_TYPE)
		goto insert;
	
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

	if(publ->body && publ->body->s)
	{
		doc= xmlParseMemory(publ->body->s, publ->body->len );
		if(doc== NULL)
		{
			LOG(L_ERR, "PUA: send_publish: ERROR while parsing xml memory\n");
			goto error;
		}
		if(publ->event& BLA_EVENT)
		{
			char* version;
			int len;
			/* change version and state*/
			node= xmlNodeGetNodeByName(doc->children, "dialog-info", NULL);
			if(node == NULL)
			{
				LOG(L_ERR, "PUA: send_publish: ERROR while extracting dialog-info node\n");
				goto error;
			}
			version= int2str(ver,&len);
			version[len]= '\0';

			if( xmlSetProp(node, (const xmlChar *)"version",(const xmlChar*)version)== NULL)
			{
				LOG(L_ERR, "PUA: send_publish: ERROR while setting version attribute\n");
				goto error;	
			}
			body= (str*)pkg_malloc(sizeof(str));
			if(body== NULL)
			{
				LOG(L_ERR, "PUA: send_publish: ERROR NO more memory left\n");
				goto error;
			}
			memset(body, 0, sizeof(str));
			xmlDocDumpFormatMemory(doc, (xmlChar**)(void*)&body->s, &body->len, 1);	

			xmlFreeDoc(doc);
			doc= NULL;

			goto after_handle_body;
		}

		DBG("PUA: send_publish: completing the body with tuple id if needed\n");
		node= xmlNodeGetNodeByName(doc->children, "tuple", NULL);
		if(node == NULL)
		{
			LOG(L_ERR, "PUA: send_publish: ERROR while extracting tuple node\n");
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
				LOG(L_ERR, "PUA: send_publish: ERROR while extracting xml"
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
			DBG("PUA: send_publish: found person node\n");
			person_id= xmlNodeGetAttrContentByName(node, "id");
			if(person_id== NULL)
			{	
				if(!xmlNewProp(node, BAD_CAST "id", BAD_CAST tuple_id))
				{
					LOG(L_ERR, "PUA: send_publish: ERROR while extracting xml"
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
			LOG(L_ERR, "PUA: send_publish: ERROR NO more memory left\n");
			goto error;
		}
		memset(body, 0, sizeof(str));
		xmlDocDumpFormatMemory(doc,(xmlChar**)(void*)&body->s, &body->len, 1);	
		if(body->s== NULL || body->len== 0)
		{
			LOG(L_ERR, "PUA: send_publish: ERROR while dumping xml format\n");
			goto error;
		}	
		xmlFreeDoc(doc);
		doc= NULL;
	}

after_handle_body:

	/* construct the callback parameter */

	size= sizeof(ua_pres_t)+ sizeof(str)+ (publ->pres_uri->len+ 
		tuple_id_len + 1)*sizeof(char);
	if(body && body->s && body->len)
		size+= sizeof(str)+ body->len* sizeof(char);
	DBG("PUA: send_publish: size= %d\n", size);
	if(publ->id.s && publ->id.len)
		size+= publ->id.len* sizeof(char);

	DBG("PUA: send_publish: before allocating size= %d\n", size);
	hentity= (ua_pres_t*)shm_malloc(size);
	if(hentity== NULL)
	{
		LOG(L_ERR, "PUA: send_publish: ERROR no more share memory while"
				" allocating hentity - size= %d\n", size);
		goto error;
	}
	memset(hentity, 0, size);

	size =  sizeof(ua_pres_t);
	
	hentity->pres_uri = (str*)((char*)hentity + size);
	size+= sizeof(str);

	hentity->pres_uri->s = (char*)hentity+ size;
	memcpy(hentity->pres_uri->s, publ->pres_uri->s ,
			publ->pres_uri->len ) ;
	hentity->pres_uri->len= publ->pres_uri->len;
	size+= publ->pres_uri->len;

	hentity->tuple_id.s = (char*)hentity+ size;
	memcpy(hentity->tuple_id.s, tuple_id ,tuple_id_len);
	hentity->tuple_id.len= tuple_id_len;
	size+= tuple_id_len;

	if(publ->id.s && publ->id.len)
	{	
		hentity->id.s = ((char*)hentity+ size);
		memcpy(hentity->id.s, publ->id.s, publ->id.len);
		hentity->id.len= publ->id.len;
		size+= publ->id.len;
	}

	if(body && body->s && body->len)
	{
		hentity->body = (str*)((char*)hentity + size);
		size+= sizeof(str);
		
		hentity->body->s = (char*)hentity+ size;
		memcpy(hentity->body->s, body->s ,
			body->len ) ;
		hentity->body->len= body->len;
		size+= body->len;
	}

	hentity->event|= publ->event;
	hentity->flag|= publ->source_flag;
	if(publ->expires> 0)
		hentity->desired_expires= publ->expires + (int )time(NULL);
	else
		hentity->desired_expires= 0;

send_publish:
	
	if(publ->flag & UPDATE_TYPE)
		DBG("PUA:send_publish: etag:%.*s\n", etag.len, etag.s);
	str_hdr = publ_build_hdr((publ->expires< 0)?3600:publ->expires, publ->event, 
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

	tmb.t_request(&met,						/* Type of the message */
			publ->pres_uri,					/* Request-URI */
			publ->pres_uri,					/* To */
			publ->pres_uri,					/* From */
			str_hdr,						/* Optional headers */
			body,							/* Message body */
			0,								/* outbound proxy*/
			publ_cback_func,				/* Callback function */
			(void*)hentity					/* Callback parameter */
			);
	
	pkg_free(str_hdr);

	if( body)
	{
		if(body->s)
			xmlFree(body->s);
		pkg_free(body);
	}	
	if(etag.s)
		pkg_free(etag.s);
	xmlCleanupParser();
	xmlMemoryDump();

	return 0;

error:
	if(etag.s)
		pkg_free(etag.s);

	if(hentity)
		shm_free(hentity);

	if(body)
	{
		if(body->s)
			xmlFree(body->s);
		pkg_free(body);
	}	
	
	if(str_hdr)
		pkg_free(str_hdr);
	
	if(doc)
		xmlFreeDoc(doc);
	xmlCleanupParser();
	xmlMemoryDump();

	return -1;
}

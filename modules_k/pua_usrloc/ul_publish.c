/*
 * $Id$
 *
 * pua_usrloc module - usrloc pua module
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

#include "../../parser/parse_expires.h"
#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../usrloc/usrloc.h"
#include "../usrloc/ul_callback.h"
#include "../tm/tm_load.h"
#include "../pua/pua.h"
#include "pua_usrloc.h"

int pua_set_publish(struct sip_msg* msg , char* s1, char* s2)
{
	DBG("pua_usrloc:pua_set_publish: set send publish\n");
	pua_ul_publish= 1;
	return 1;
}	
/* for debug purpose only */
void print_publ(publ_info_t* p)
{
	DBG("publ:\n");
	DBG("uri= %.*s\n", p->pres_uri->len, p->pres_uri->s);
	DBG("id= %.*s\n", p->id.len, p->id.s);
	DBG("expires= %d\n", p->expires);
}	

str* build_pidf(ucontact_t* c)
{
	xmlDocPtr  doc = NULL; 
	xmlNodePtr root_node = NULL;
	xmlNodePtr tuple_node = NULL;
	xmlNodePtr status_node = NULL;
	xmlNodePtr basic_node = NULL;
	str *body= NULL;
	str pres_uri= {NULL, 0};
	char buf[265];
	char* at= NULL;

	DBG("pua_usrloc:build_pidf... \n");

	if(c->expires< (int)time(NULL))
	{
		DBG("pua_usrloc:build_pidf: found expired \n\n");
		return NULL;
	}

	pres_uri.s = buf;
	if(pres_prefix.s)
	{
		memcpy(pres_uri.s, pres_prefix.s, pres_prefix.len);
		pres_uri.len+= pres_prefix.len;
		memcpy(pres_uri.s+ pres_uri.len, ":", 1);
		pres_uri.len+= 1;
	}	
	memcpy(pres_uri.s+ pres_uri.len, c->aor->s, c->aor->len);
	pres_uri.len+= c->aor->len;

	at = memchr(c->aor->s, '@', c->aor->len);
	if(!at)
	{
		pres_uri.s[pres_uri.len++]= '@';
		memcpy(pres_uri.s+ pres_uri.len, default_domain.s, default_domain.len);
		pres_uri.len+= default_domain.len;		
	}
	pres_uri.s[pres_uri.len]= '\0';

	if(pres_uri.len> 256)
		return NULL;
	/* create the Publish body  */
	doc = xmlNewDoc(BAD_CAST "1.0");
	if(doc==0)
		return NULL;

    root_node = xmlNewNode(NULL, BAD_CAST "presence");
	if(root_node==0)
		goto error;
    
	xmlDocSetRootElement(doc, root_node);

    xmlNewProp(root_node, BAD_CAST "xmlns",
			BAD_CAST "urn:ietf:params:xml:ns:pidf");
	xmlNewProp(root_node, BAD_CAST "xmlns:dm",
			BAD_CAST "urn:ietf:params:xml:ns:pidf:data-model");
	xmlNewProp(root_node, BAD_CAST  "xmlns:rpid",
			BAD_CAST "urn:ietf:params:xml:ns:pidf:rpid" );
	xmlNewProp(root_node, BAD_CAST "xmlns:c",
			BAD_CAST "urn:ietf:params:xml:ns:pidf:cipid");
	xmlNewProp(root_node, BAD_CAST "entity", BAD_CAST pres_uri.s);

	tuple_node =xmlNewChild(root_node, NULL, BAD_CAST "tuple", NULL) ;
	if( tuple_node ==NULL)
	{
		LOG(L_ERR, "pua_usrloc:build_pidf: ERROR while adding child\n");
		goto error;
	}
	
	status_node = xmlNewChild(tuple_node, NULL, BAD_CAST "status", NULL) ;
	if( status_node ==NULL)
	{
		LOG(L_ERR, "pua_usrloc:build_pidf: ERROR while adding child\n");
		goto error;
	}
	
	basic_node = xmlNewChild(status_node, NULL, BAD_CAST "basic",
		BAD_CAST "open") ;
	
	if( basic_node ==NULL)
	{
		LOG(L_ERR, "pua_usrloc:build_pidf: ERROR while adding child\n");
		goto error;
	}
	
	body = (str*)pkg_malloc(sizeof(str));
	if(body == NULL)
	{
		LOG(L_ERR,"pua_usrloc:build_pidf: Error while allocating memory\n");
		return NULL;
	}
	memset(body, 0, sizeof(str));

	xmlDocDumpFormatMemory(doc,(unsigned char**)(void*)&body->s,&body->len,1);

	DBG("pua_usrloc:build_pidf: new_body:\n%.*s\n",body->len, body->s);

    /*free the document */
	xmlFreeDoc(doc);
    xmlCleanupParser();

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

void ul_publish(ucontact_t* c, int type, void* param)
{
	str* body= NULL;
	str uri= {NULL, 0};
	char* at= NULL;
	publ_info_t* publ= NULL;
	int size= 0;
	str content_type;

	content_type.s= "application/pidf+xml";
	content_type.len= 20;

	if(pua_ul_publish== 0)
	{
		LOG(L_INFO, "pua_usrloc:ul_publish: should not send ul publish\n");
		return;
	}	

	if(type & UL_CONTACT_DELETE)
		DBG("\nul_publish: DELETE type\n");
	else
		if(type & UL_CONTACT_INSERT)
			DBG("\nul_publish: INSERT type\n");
		else
			if(type & UL_CONTACT_UPDATE)
				DBG("\nul_publish: UPDATE type\n");
			else
				if(type & UL_CONTACT_EXPIRE)
					DBG("\nul_publish: EXPIRE type\n");

	if((type & UL_CONTACT_INSERT) || (type& UL_CONTACT_UPDATE))
	{
		body= build_pidf(c);
		if(body == NULL || body->s == NULL)
			goto error;
	}
	else
		body = NULL;
	
	uri.s = (char*)pkg_malloc(sizeof(char)*(c->aor->len+default_domain.len+6));
	if(uri.s == NULL)
		goto error;

	memcpy(uri.s, "sip:", 4);
	uri.len = 4;
	memcpy(uri.s+ uri.len, c->aor->s, c->aor->len);
	uri.len+= c->aor->len;
	at = memchr(c->aor->s, '@', c->aor->len);
	if(!at)
	{
		uri.s[uri.len++]= '@';
		memcpy(uri.s+ uri.len, default_domain.s, default_domain.len);
		uri.len+= default_domain.len;		
	}
	DBG("ul_publish: uri= %.*s\n", uri.len, uri.s);
	
	size= sizeof(publ_info_t)+ sizeof(str)+( uri.len 
			+c->callid.len+ 12 + content_type.len)*sizeof(char); 
	
	if(body)
		size+= sizeof(str)+ body->len* sizeof(char);

	publ= (publ_info_t*)pkg_malloc(size);
	if(publ== NULL)
	{
		LOG(L_ERR, "pua_usrloc: ul_publish: Error no more share memory\n");
		goto error;
	}
	memset(publ, 0, size);
	size= sizeof(publ_info_t);

	publ->pres_uri= (str*)((char*)publ + size);
	size+= sizeof(str);
	publ->pres_uri->s= (char*)publ+ size;
	memcpy(publ->pres_uri->s, uri.s, uri.len);
	publ->pres_uri->len= uri.len;
	size+= uri.len;

	if(body)
	{
		publ->body= (str*)( (char*)publ + size);
		size+= sizeof(str);

		publ->body->s= (char*)publ + size;
		memcpy(publ->body->s, body->s, body->len);
		publ->body->len= body->len;
		size+= body->len;
	}
	publ->id.s= (char*)publ+ size;
	memcpy(publ->id.s, "UL_PUBLISH.", 11);
	memcpy(publ->id.s+11, c->callid.s, c->callid.len);
	publ->id.len= 11+ c->callid.len;
	size+= publ->id.len;

	publ->content_type.s= (char*)publ+ size;
	memcpy(publ->content_type.s, content_type.s, content_type.len);
	publ->content_type.len= content_type.len;
	size+= content_type.len;

	if(type & UL_CONTACT_EXPIRE || type & UL_CONTACT_DELETE)
		publ->expires= 0;
	else
		publ->expires= c->expires - (int)time(NULL);
	
	if(type & UL_CONTACT_INSERT)
		publ->flag|= INSERT_TYPE;
	else
		publ->flag|= UPDATE_TYPE;

	publ->source_flag|= UL_PUBLISH;
	publ->event|= PRESENCE_EVENT;
	publ->extra_headers= NULL;
	print_publ(publ);
	if(pua_send_publish(publ)< 0)
	{
		LOG(L_ERR, "pua_usrloc:ul_publish: ERROR while sending publish\n");
	}	

	pua_ul_publish= 0;

error:

	if(publ)
		pkg_free(publ);

	if(body)
	{
		if(body->s)
			xmlFree(body->s);
		pkg_free(body);
	}
	
	if(uri.s)
		pkg_free(uri.s);
	pua_ul_publish= 0;

	return;

}



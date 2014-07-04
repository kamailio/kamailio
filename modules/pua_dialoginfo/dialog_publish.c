/*
 * $Id: ul_publish.c 4518 2008-07-28 15:39:28Z henningw $
 *
 * pua_usrloc module - usrloc pua module
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <time.h>

#include "../../parser/parse_expires.h"
#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../str_list.h"
#include "../../name_alias.h"
#include "../../socket_info.h"
#include "../usrloc/usrloc.h"
#include "../usrloc/ul_callback.h"
#include "../../modules/tm/tm_load.h"
#include "../pua/pua.h"
#include "pua_dialoginfo.h"

/* global modul parameters */
extern int include_callid;
extern int include_localremote;
extern int include_tags;


/* for debug purpose only */
void print_publ(publ_info_t* p)
{
	LM_DBG("publ:\n");
	LM_DBG("uri= %.*s\n", p->pres_uri->len, p->pres_uri->s);
	LM_DBG("id= %.*s\n", p->id.len, p->id.s);
	LM_DBG("expires= %d\n", p->expires);
}	

str* build_dialoginfo(char *state, str *entity, str *peer, str *callid, 
	unsigned int initiator, str *localtag, str *remotetag,
	str *localtarget, str *remotetarget)
{
	xmlDocPtr  doc = NULL; 
	xmlNodePtr root_node = NULL;
	xmlNodePtr dialog_node = NULL;
	xmlNodePtr state_node = NULL;
	xmlNodePtr remote_node = NULL;
	xmlNodePtr local_node = NULL;
	xmlNodePtr tag_node = NULL;
	str *body= NULL;
	char buf[MAX_URI_SIZE+1];

	if (entity->len > MAX_URI_SIZE) {
		LM_ERR("entity URI '%.*s' too long, maximum=%d\n",entity->len, entity->s, MAX_URI_SIZE);
		return NULL;
	}
	memcpy(buf, entity->s, entity->len);
	buf[entity->len]= '\0';

	/* create the Publish body */
	doc = xmlNewDoc(BAD_CAST "1.0");
	if(doc==0)
		return NULL;

	root_node = xmlNewNode(NULL, BAD_CAST "dialog-info");
	if(root_node==0)
		goto error;

	xmlDocSetRootElement(doc, root_node);

	xmlNewProp(root_node, BAD_CAST "xmlns",
			BAD_CAST "urn:ietf:params:xml:ns:dialog-info");
	/* we set the version to 0 but it should be set to the correct value
	in the pua module */
	xmlNewProp(root_node, BAD_CAST "version",
			BAD_CAST "0");
	xmlNewProp(root_node, BAD_CAST  "state",
			BAD_CAST "full" );
	xmlNewProp(root_node, BAD_CAST "entity", 
			BAD_CAST buf);

	/* RFC 3245 differs between id and call-id. For example if a call
	   is forked and 2 early dialogs are established, we should send 2
	   PUBLISH requests, both have the same call-id but different id.
	   Thus, id could be for example derived from the totag.

	   Currently the dialog module does not support multiple dialogs.
	   Thus, it does no make sense to differ here between multiple dialog.
	   Thus, id and call-id will be populated identically */

	/* dialog tag */
	dialog_node =xmlNewChild(root_node, NULL, BAD_CAST "dialog", NULL) ;
	if( dialog_node ==NULL)
	{
		LM_ERR("while adding child\n");
		goto error;
	}

	if (callid->len > MAX_URI_SIZE) {
		LM_ERR("call-id '%.*s' too long, maximum=%d\n", callid->len, callid->s, MAX_URI_SIZE);
		return NULL;
	}
	memcpy(buf, callid->s, callid->len);
	buf[callid->len]= '\0';

	xmlNewProp(dialog_node, BAD_CAST "id", BAD_CAST buf);
	if (include_callid) {
		xmlNewProp(dialog_node, BAD_CAST "call-id", BAD_CAST buf);
	}
	if (include_tags) {
		if (localtag && localtag->s) {
			if (localtag->len > MAX_URI_SIZE) {
				LM_ERR("localtag '%.*s' too long, maximum=%d\n", localtag->len, localtag->s, MAX_URI_SIZE);
				return NULL;
			}
			memcpy(buf, localtag->s, localtag->len);
			buf[localtag->len]= '\0';
			xmlNewProp(dialog_node, BAD_CAST "local-tag", BAD_CAST buf);
		}
		if (remotetag && remotetag->s) {
			if (remotetag->len > MAX_URI_SIZE) {
				LM_ERR("remotetag '%.*s' too long, maximum=%d\n", remotetag->len, remotetag->s, MAX_URI_SIZE);
				return NULL;
			}
			memcpy(buf, remotetag->s, remotetag->len);
			buf[remotetag->len]= '\0';
			xmlNewProp(dialog_node, BAD_CAST "remote-tag", BAD_CAST buf);
		}
	}

	if (initiator) {
		xmlNewProp(dialog_node, BAD_CAST "direction", BAD_CAST "initiator");
	}else {
		xmlNewProp(dialog_node, BAD_CAST "direction", BAD_CAST "recipient");
	}

	/* state tag */
	state_node = xmlNewChild(dialog_node, NULL, BAD_CAST "state", BAD_CAST state) ;
	if( state_node ==NULL)
	{
		LM_ERR("while adding child\n");
		goto error;
	}

	if (include_localremote) {
		/* remote tag*/	
		remote_node = xmlNewChild(dialog_node, NULL, BAD_CAST "remote", NULL) ;
		if( remote_node ==NULL)
		{
			LM_ERR("while adding child\n");
			goto error;
		}

		if (peer->len > MAX_URI_SIZE) {
			LM_ERR("peer '%.*s' too long, maximum=%d\n", peer->len, peer->s, MAX_URI_SIZE);
			return NULL;
		}
		memcpy(buf, peer->s, peer->len);
		buf[peer->len]= '\0';

		tag_node = xmlNewChild(remote_node, NULL, BAD_CAST "identity", BAD_CAST buf) ;
		if( tag_node ==NULL)
		{
			LM_ERR("while adding child\n");
			goto error;
		}
		tag_node = xmlNewChild(remote_node, NULL, BAD_CAST "target", NULL) ;
		if( tag_node ==NULL)
		{
			LM_ERR("while adding child\n");
			goto error;
		}
		if (remotetarget && remotetarget->s) {
			memcpy(buf, remotetarget->s, remotetarget->len);
			buf[remotetarget->len]= '\0';
		}
		xmlNewProp(tag_node, BAD_CAST "uri", BAD_CAST buf);

		/* local tag*/	
		local_node = xmlNewChild(dialog_node, NULL, BAD_CAST "local", NULL) ;
		if( local_node ==NULL)
		{
			LM_ERR("while adding child\n");
			goto error;
		}

		if (entity->len > MAX_URI_SIZE) {
			LM_ERR("entity '%.*s' too long, maximum=%d\n", entity->len, entity->s, MAX_URI_SIZE);
			return NULL;
		}
		memcpy(buf, entity->s, entity->len);
		buf[entity->len]= '\0';

		tag_node = xmlNewChild(local_node, NULL, BAD_CAST "identity", BAD_CAST buf) ;
		if( tag_node ==NULL)
		{
			LM_ERR("while adding child\n");
			goto error;
		}
		tag_node = xmlNewChild(local_node, NULL, BAD_CAST "target", NULL) ;
		if( tag_node ==NULL)
		{
			LM_ERR("while adding child\n");
			goto error;
		}
		if (localtarget && localtarget->s) {
			memcpy(buf, localtarget->s, localtarget->len);
			buf[localtarget->len]= '\0';
		}
		xmlNewProp(tag_node, BAD_CAST "uri", BAD_CAST buf);
	}

	/* create the body */
	body = (str*)pkg_malloc(sizeof(str));
	if(body == NULL)
	{
		LM_ERR("while allocating memory\n");
		return NULL;
	}
	memset(body, 0, sizeof(str));

	xmlDocDumpFormatMemory(doc,(unsigned char**)(void*)&body->s,&body->len,1);

	LM_DBG("new_body:\n%.*s\n",body->len, body->s);

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

void dialog_publish(char *state, str* ruri, str *entity, str *peer, str *callid,
	unsigned int initiator, unsigned int lifetime, str *localtag, str *remotetag,
	str *localtarget, str *remotetarget, unsigned short do_pubruri_localcheck)
{
	str* body= NULL;
	str uri= {NULL, 0};
	publ_info_t* publ= NULL;
	int size= 0;
	str content_type;
	struct sip_uri ruri_uri;


	if (parse_uri(ruri->s, ruri->len, &ruri_uri) < 0) {
		LM_ERR("failed to parse the PUBLISH R-URI\n");
		return;
	}

	if(do_pubruri_localcheck) {

		/* send PUBLISH only if the receiver PUBLISH R-URI is local*/
		if (!check_self(&(ruri_uri.host), 0, 0)) {
			LM_DBG("do not send PUBLISH to external URI %.*s\n",ruri->len, ruri->s);
			return;
		}

	}

	content_type.s= "application/dialog-info+xml";
	content_type.len= 27;

	body= build_dialoginfo(state, entity, peer, callid, initiator, localtag, remotetag, localtarget, remotetarget);
	if(body == NULL || body->s == NULL)
		goto error;
	
	LM_DBG("publish uri= %.*s\n", ruri->len, ruri->s);
	
	size= sizeof(publ_info_t) 
			+ sizeof(str) 			/* *pres_uri */
			+ ( ruri->len 		/* pres_uri->s */
			+ callid->len + 16	/* id.s */
			+ content_type.len	/* content_type.s */
			)*sizeof(char); 
	
	if(body)
		size+= sizeof(str)+ body->len* sizeof(char);

	publ= (publ_info_t*)pkg_malloc(size);
	if(publ== NULL)
	{
		LM_ERR("no more share memory\n");
		goto error;
	}
	memset(publ, 0, size);
	size= sizeof(publ_info_t);

	publ->pres_uri= (str*)((char*)publ + size);
	size+= sizeof(str);
	publ->pres_uri->s= (char*)publ+ size;
	memcpy(publ->pres_uri->s, ruri->s, ruri->len);
	publ->pres_uri->len= ruri->len;
	size+= ruri->len;

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
	memcpy(publ->id.s, "DIALOG_PUBLISH.", 15);
	memcpy(publ->id.s+15, callid->s, callid->len);
	publ->id.len= 15+ callid->len;
	size+= publ->id.len;

	publ->content_type.s= (char*)publ+ size;
	memcpy(publ->content_type.s, content_type.s, content_type.len);
	publ->content_type.len= content_type.len;
	size+= content_type.len;

	publ->expires= lifetime;
	
	/* make UPDATE_TYPE, as if this "publish dialog" is not found 
	   by pua it will fallback to INSERT_TYPE anyway */
	publ->flag|= UPDATE_TYPE;

	publ->source_flag|= DIALOG_PUBLISH;
	publ->event|= DIALOG_EVENT;
	publ->extra_headers= NULL;
	print_publ(publ);
	if(pua_send_publish(publ)< 0)
	{
		LM_ERR("while sending publish\n");
	}	

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

	return;
}



void dialog_publish_multi(char *state, struct str_list* ruris, str *entity, str *peer, str *callid,
	unsigned int initiator, unsigned int lifetime, str *localtag, str *remotetag,
	str *localtarget, str *remotetarget, unsigned short do_pubruri_localcheck) {

	while(ruris) {
		LM_INFO("CALLING dialog_publish for URI %.*s\n",ruris->s.len, ruris->s.s);
		dialog_publish(state,&(ruris->s),entity,peer,callid,initiator,lifetime,localtag,remotetag,localtarget,remotetarget,do_pubruri_localcheck);
		ruris=ruris->next;
	}

}

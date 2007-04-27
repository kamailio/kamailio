/*
 * $Id: notify.c 1666 2007-03-02 13:40:09Z anca_vamanu $
 *
 * pua_bla module - pua Bridged Line Appearance
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
 *  2007-03-30  initial version (anca)
 */
#include<stdio.h>
#include<stdlib.h>
#include<libxml/parser.h>

#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../pua/hash.h"
#include"pua_bla.h"


xmlNodePtr xmlDocGetNodeByName(xmlDocPtr doc, const char *name, const char *ns);
xmlNodePtr xmlNodeGetChildByName(xmlNodePtr node, const char *name);

int bla_handle_notify(struct sip_msg* msg, char* s1, char* s2)
{

	publ_info_t publ;
	struct to_body *pto= NULL, TO, *pfrom = NULL;
	str body;
	ua_pres_t dialog;
	unsigned int expires= 0;
	struct hdr_field* hdr;
	str subs_state;
	int found= 0;
	str extra_headers= {0, 0};
	static char buf[255];
	xmlDoc* doc= NULL;
	xmlNode* node= NULL, *dialog_node= NULL;
	char* stare= NULL;

	memset(&publ, 0, sizeof(publ_info_t));
	memset(&dialog, 0, sizeof(ua_pres_t));
	
	DBG( "PUA_BLA: handle_notify...\n");
	
	if ( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LOG(L_ERR, "PUA_BLA: handle_notify:ERROR parsing headers\n");
		return -1;
	}

	if( msg->to==NULL || msg->to->body.s==NULL)
	{
		LOG(L_ERR, "PUA_BLA: handle_notify: ERROR cannot parse TO"
				" header\n");
		goto error;
	}
	/* examine the to header */
	if(msg->to->parsed != NULL)
	{
		pto = (struct to_body*)msg->to->parsed;
		DBG("PUA_BLA: handle_notify: 'To' header ALREADY PARSED: <%.*s>\n",
				pto->uri.len, pto->uri.s );	
	}
	else
	{
		memset( &TO , 0, sizeof(TO) );
		parse_to(msg->to->body.s,msg->to->body.s + msg->to->body.len + 1, &TO);
		if(TO.uri.len <= 0) 
		{
			DBG("PUA_BLA: handle_notify: 'To' header NOT parsed\n");
			goto error;
		}
		pto = &TO; 
	}
	publ.pres_uri= &pto->uri;
	dialog.watcher_uri= publ.pres_uri;

	if (pto->tag_value.s==NULL || pto->tag_value.len==0 )
	{  
		LOG(L_ERR ,"PUA_BLA: handle_notify: ERROR: NULL to_tag value\n");
	}
	dialog.from_tag= pto->tag_value;

	if( msg->callid==NULL || msg->callid->body.s==NULL)
	{
		LOG(L_ERR, "PUA_BLA: handle_notify:ERROR cannot parse callid"
				" header\n");
		goto error;
	}
	dialog.call_id = msg->callid->body;
	
	if (!msg->from || !msg->from->body.s)
	{
		LOG(L_ERR, "PUA_BLA: handle_notify:  ERROR cannot find 'from'"
				" header!\n");
		goto error;
	}
	if (msg->from->parsed == NULL)
	{
		DBG("PUA_BLA: handle_notify:  'From' header not parsed\n");
		/* parsing from header */
		if ( parse_from_header( msg )<0 ) 
		{
			DBG("PUA_BLA: handle_notify:  ERROR cannot parse From header\n");
			goto error;
		}
	}
	pfrom = (struct to_body*)msg->from->parsed;
	dialog.pres_uri= &pfrom->uri;
	
	if( pfrom->tag_value.s ==NULL || pfrom->tag_value.len == 0)
	{
		LOG(L_ERR, "PPUA_BLA: handle_notify:ERROR no from tag value"
				" present\n");
		goto error;
	}

	dialog.to_tag= pfrom->tag_value;
	dialog.event= BLA_EVENT;
	dialog.flag= BLA_SUBSCRIBE;
	if(pua_is_dialog(&dialog)< 0)
	{
		LOG(L_ERR, "PUA_BLA: handle_notify: ERROR Notify in a non existing"
				" dialog\n");
		goto error;
	}
	DBG("PUA_BLA: handle_notify: found a matching dialog\n");

	/* parse Subscription-State and extract expires if existing */
	hdr = msg->headers;
	while (hdr!= NULL)
	{
		if(strncmp(hdr->name.s, "Subscription-State",18)==0 )
		{
			found = 1;
			break;
		}
		hdr = hdr->next;
	}
	if(found==0 )
	{
		LOG(L_ERR, "PUA_BLA: handle_notify: No Subscription-State header"
				" found\n");
		goto error;
	}
	subs_state= hdr->body;
	if(strncmp(subs_state.s, "terminated", 10)== 0)
		expires= 0;
	else
		if(strncmp(subs_state.s, "active", 6)== 0 || 
				strncmp(subs_state.s, "pending", 7)==0 )
		{	
			char* sep= NULL;
			str exp= {0, 0};
			sep= strchr(subs_state.s, ';');
			if(sep== NULL)
			{
				LOG(L_ERR, "PUA_BLA: handle_notify: No expires found in"
						" Notify\n");
				goto error;	
			}	
			if(strncmp(sep+1, "expires=", 8)!= 0)
			{
				LOG(L_ERR, "PUA_BLA: handle_notify: No expires found in"
						" Notify\n");
				goto error;
			}
			exp.s= sep+ 9;
			sep= exp.s;
			while((*sep)>='0' && (*sep)<='9')
			{
				sep++;
				exp.len++;
			}
			if( str2int(&exp, &expires)< 0)
			{
				LOG(L_ERR, "PUA_BLA: handle_notify: ERROR while parsing int\n");
				goto error;
			}
		}
	
	if ( get_content_length(msg) == 0 )
	{
		LOG(L_ERR, "PUA_BLA: handle_notify: ERROR content length= 0\n");
		goto error;
	}
	else
	{
		body.s=get_body(msg);
		if (body.s== NULL) 
		{
			LOG(L_ERR,"PUA_BLA: handle_notify: ERROR cannot extract body"
					" from msg\n");
			goto error;
		}
		body.len = get_content_length( msg );
	}

/* build extra_headers with Sender*/
	extra_headers.s= buf;
	memcpy(extra_headers.s, header_name.s, header_name.len);
	extra_headers.len= header_name.len;
	memcpy(extra_headers.s+extra_headers.len,": ",2);
	extra_headers.len+= 2;
	memcpy(extra_headers.s+ extra_headers.len, dialog.pres_uri->s,
			dialog.pres_uri->len);
	extra_headers.len+= dialog.pres_uri->len;
	memcpy(extra_headers.s+ extra_headers.len, CRLF, CRLF_LEN);
	extra_headers.len+= CRLF_LEN;
	
	publ.body= &body;
	publ.expires= expires;
	publ.event= BLA_EVENT;
	publ.extra_headers= &extra_headers;

	/* if state= terminated construct a Publish message with no dialog-info elements*/
	if(body.s && body.len)
	{	
		doc= xmlParseMemory(body.s, body.len );
		if(doc== NULL)
		{
			LOG(L_ERR, "PUA_BLA: handle_notify: ERROR while parsing xml memory\n");
			goto error;
		}
		dialog_node= xmlDocGetNodeByName(doc, "dialog", NULL);
		if(dialog_node )
		{
		    
			node= xmlNodeGetChildByName(dialog_node, "state");
			if(node == NULL)
			{
				LOG(L_ERR, "PUA_BLA: handle_notify: No dialog node found\n");
					goto error;
			}
			stare= (char*)xmlNodeGetContent(node->children);
		
			if( strncmp(stare, "terminated", 10) == 0)
			{
				DBG("PUA_BLA: handle_notify: Found state terminated - send another Publish\n");
				publ.source_flag= BLA_TERM_PUBLISH;	
			}	
			else
				publ.source_flag= BLA_PUBLISH;	
			xmlFree(stare);
		}
		else
			publ.source_flag= BLA_PUBLISH;	
	}
	else
		publ.source_flag= BLA_PUBLISH;

	if(pua_send_publish(&publ)< 0)
	{
		LOG(L_ERR, "PUA_BLA: handle_notify: ERROR while sending Publish\n");
		goto error;
	}

	xmlCleanupParser();
	xmlMemoryDump();

	return 1;

error:
	if(doc)
		xmlFreeDoc(doc);

	return 0;
}	

void term_publ_callback(ua_pres_t* hentity, struct msg_start * fl)
{
	publ_info_t publ;
	str extra_headers= {0, 0};
	static char buf[255];
	xmlDocPtr doc= NULL;
	xmlNodePtr dialog_node= NULL;
	str body;

	memset(&publ, 0, sizeof(publ_info_t));

	/* build extra_headers with Sender*/
	extra_headers.s= buf;
	memcpy(extra_headers.s, header_name.s, header_name.len);
	extra_headers.len= header_name.len;
	memcpy(extra_headers.s+extra_headers.len,": ",2);
	extra_headers.len+= 2;
	memcpy(extra_headers.s+ extra_headers.len, hentity->pres_uri->s,
			hentity->pres_uri->len);
	extra_headers.len+= hentity->pres_uri->len;
	memcpy(extra_headers.s+ extra_headers.len, CRLF, CRLF_LEN);
	extra_headers.len+= CRLF_LEN;

	publ.pres_uri= hentity->pres_uri;
	publ.expires= hentity->expires- (int)time(NULL);
	publ.event= BLA_EVENT;
	publ.extra_headers= &extra_headers;
	publ.source_flag= BLA_PUBLISH;
	
	memset(&body, 0, sizeof(str));
	
	if(hentity->body->s== NULL || hentity->body->len== 0)
	{
		LOG(L_ERR, "PUA_BLA: handle_notify: ERROR NULL body\n");
   		goto error;
	}
	doc= xmlParseMemory(hentity->body->s, hentity->body->len );
	if(doc== NULL)
  	{
  		LOG(L_ERR, "PUA_BLA: handle_notify: ERROR while parsing xml memory\n");
   		goto error;
   	}
   	dialog_node= xmlDocGetNodeByName(doc, "dialog", NULL);
   	if(dialog_node == NULL)
   	{
   		DBG("PUA_BLA: handle_notify: No dialog node found\n");
   		goto error;
   	}
   
   	xmlUnlinkNode(dialog_node);
   	xmlFreeNode(dialog_node);
   
  	memset(&body, 0, sizeof(str));
   	xmlDocDumpFormatMemory(doc, (xmlChar**)(void*)&body.s, &body.len, 1);
   	if(body.s== NULL || body.len== 0)
   	{
   		LOG(L_ERR, "PUA_BLA: handle_notify: ERROR while dumping xml memory\n");
   		goto error;
   	}
   	xmlFreeDoc(doc);
   	doc= NULL;
   
   	publ.body= &body;
   	if(pua_send_publish(&publ)< 0)
   	{
   		LOG(L_ERR, "PUA_BLA: handle_notify: ERROR while sending Publish\n");
   		xmlFree(body.s);
   		goto error;
   	}
   	xmlFree(body.s);
   	
	return;	

error:
	if(doc)
		xmlFreeDoc(doc);
	return;

}	


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

xmlNodePtr xmlDocGetNodeByName(xmlDocPtr doc, const char *name, const char *ns)
{
	xmlNodePtr cur = doc->children;
	return xmlNodeGetNodeByName(cur, name, ns);
}

xmlNodePtr xmlNodeGetChildByName(xmlNodePtr node, const char *name)
{
	xmlNodePtr cur = node->children;
	while (cur) {
		if (xmlStrcasecmp(cur->name, (unsigned char*)name) == 0)
			return cur;
		cur = cur->next;
	}
	return NULL;
}

#if 0
	entity= (char*)pkg_malloc((hentity->pres_uri->len+ 1)* sizeof(char));
	if(entity== NULL)
	{
		LOG(L_ERR, "PUA_BLA: term_publ_callback: NO more memory\n");
		return;
	}
	memcpy(entity, hentity->pres_uri->s, hentity->pres_uri->len);
	entity[hentity->pres_uri->len]= '\0';

	/* construct body*/
	doc = xmlNewDoc(BAD_CAST "1.0");
    if(doc== NULL)
	{
		LOG(L_ERR, "PUA_BLA: term_publ_callback: ERROR when creating new xml doc\n");
		goto error;
	}
	dialog_node = xmlNewNode(NULL, BAD_CAST "dialog-info");
    if(dialog_node== NULL)
	{
		LOG(L_ERR, "PUA_BLA: term_publ_callback: ERROR when creating new node\n");
		goto error;
	}	
	xmlDocSetRootElement(doc, dialog_node);

	xmlNewProp(dialog_node, BAD_CAST "xmlns",
			BAD_CAST "urn:ietf:params:xml:ns:dialog-info");

	xmlNewProp(dialog_node, BAD_CAST "version", BAD_CAST "1");
	xmlNewProp(dialog_node, BAD_CAST "state", BAD_CAST "partial");
	xmlNewProp(dialog_node, BAD_CAST "entity", BAD_CAST entity);
	pkg_free(entity);
	entity= NULL;

	xmlDocDumpFormatMemory(doc, (xmlChar**)(void*)&body.s, &body.len, 1);	
	if(body.s== NULL || body.len== 0)
	{
		LOG(L_ERR, "PUA_BLA: handle_notify: ERROR while dumping xml memory\n");
		xmlFreeDoc(doc);
		goto error;
	}	
	xmlFreeDoc(doc);
	doc= NULL;

#endif

/*
 * $Id: simple2xmpp.c 1666 2007-03-02 13:40:09Z anca_vamanu $
 *
 * pua_xmpp module - presence SIP - XMPP Gateway
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
 *
 * History:
 * --------
 *  2007-03-29  initial version (anca)
 */

/*! \file
 * \brief Kamailio presence gateway: SIP/SIMPLE -- XMPP (pua_xmpp)
 */

#include <stdio.h>
#include <stdlib.h>
#include <libxml/parser.h>

#include "../../ut.h"
#include "../../lib/kcore/cmpapi.h"
#include "../../dprint.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_expires.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_fline.h"
#include "../../mem/mem.h"
#include "pua_xmpp.h"
#include "simple2xmpp.h"

int winfo2xmpp(str* to_uri, str* body, str* id);
int build_xmpp_content(str* to_uri, str* from_uri, str* body, str* id, int is_terminated);

int Notify2Xmpp(struct sip_msg* msg, char* s1, char* s2)
{
	struct to_body *pto, TO = {0}, *pfrom = NULL;
	str to_uri;
	char* uri= NULL;
	str from_uri;
	struct hdr_field* hdr= NULL;
	str body;
	int is_terminated= 0;
	str id;
	ua_pres_t dialog;
	int event_flag= 0;

	memset(&dialog, 0, sizeof(ua_pres_t));
	
	LM_DBG("start...\n\n");

	if( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LM_ERR("parsing headers\n");
		return -1;
	}
	if((!msg->event ) ||(msg->event->body.len<=0))
	{
		LM_ERR("Missing event header field value\n");
		return -1;
	}

	if( msg->to==NULL || msg->to->body.s==NULL)
	{
		LM_ERR("cannot parse TO header\n");
		return -1;
	}

	if(msg->to->parsed != NULL)
	{
		pto = (struct to_body*)msg->to->parsed;
		LM_DBG("'To' header ALREADY PARSED:<%.*s>\n",pto->uri.len,pto->uri.s);
	}
	else
	{
		parse_to(msg->to->body.s,msg->to->body.s + msg->to->body.len + 1, &TO);
		if(TO.uri.len <= 0) 
		{
			LM_ERR("'To' header NOT parsed\n");
			goto error;
		}
		pto = &TO;
	}

	dialog.watcher_uri= &pto->uri;
	
	uri=(char*)pkg_malloc(sizeof(char)*( pto->uri.len+1));
	if(uri== NULL)
	{
		LM_ERR("no more memory\n");
		goto error;
	}
	memcpy(uri, pto->uri.s, pto->uri.len);
	uri[pto->uri.len]= '\0';
	to_uri.s= duri_sip_xmpp(uri);
	if(to_uri.s== NULL)
	{
		LM_ERR("while decoding sip uri in xmpp\n");
		pkg_free(uri);
		goto error;
	}	
	to_uri.len= strlen(to_uri.s);
	pkg_free(uri);

	if (pto->tag_value.s==NULL || pto->tag_value.len==0 )
	{  
		LM_ERR("to tag value not parsed\n");
		goto error;
	}
	id=  pto->tag_value;
	dialog.from_tag= id;

	if( msg->callid==NULL || msg->callid->body.s==NULL)
	{
		LM_ERR("cannot parse callid header\n");
		goto error;
	}
	dialog.call_id = msg->callid->body;

	if (!msg->from || !msg->from->body.s)
	{
		LM_ERR("ERROR cannot find 'from' header!\n");
		goto error;
	}
	if (msg->from->parsed == NULL)
	{
		LM_ERR("'From' header not parsed\n");
		/* parsing from header */
		if ( parse_from_header( msg )<0 ) 
		{
			LM_ERR("ERROR cannot parse From header\n");
			goto error;
		}
	}
	pfrom = (struct to_body*)msg->from->parsed;
	dialog.pres_uri= &pfrom->uri;
	
	uri=(char*)pkg_malloc(sizeof(char)*( pfrom->uri.len+1));
	if(uri== NULL)
	{
		LM_ERR("no more memory\n");
		goto error;
	}
	memcpy(uri, pfrom->uri.s, pfrom->uri.len);
	uri[pfrom->uri.len]= '\0';
	
	from_uri.s= euri_sip_xmpp(uri);
	if(from_uri.s== NULL)
	{
		LM_ERR("while encoding sip uri in xmpp\n");
		pkg_free(uri);
		goto error;
	}	
	from_uri.len= strlen(from_uri.s);
	pkg_free(uri);
	
	if( pfrom->tag_value.s ==NULL || pfrom->tag_value.len == 0)
	{
		LM_ERR("no from tag value present\n");
		goto error;
	}

	dialog.to_tag= pfrom->tag_value;
	dialog.flag|= XMPP_SUBSCRIBE;
	if(msg->event->body.len== 8 && 
			(strncmp(msg->event->body.s,"presence",8 )==0))
		event_flag|= PRESENCE_EVENT;
	else
	if(msg->event->body.len== 14 && 
			(strncmp(msg->event->body.s,"presence.winfo",14 )==0))
		event_flag|= PWINFO_EVENT;
	else
	{
		LM_ERR("wrong event\n");
		goto error;
	}	
	dialog.event= event_flag;
	
	if(pua_is_dialog(&dialog)< 0) // verify if within a stored dialog
	{
		LM_ERR("Notify in a non existing dialog\n");
		goto error;
	}
	/*constructing the xml body*/
	if(get_content_length(msg) == 0 )
	{
		body.s= NULL;
		body.len= 0;
	}	
	else
	{
		body.s=get_body(msg);
		if (body.s== NULL) 
		{
			LM_ERR("cannot extract body from msg\n");
			goto error;
		}
		body.len = get_content_length( msg );
	}

	/* treat the two cases: event= presence & event=presence.winfo */	
	if(event_flag & PRESENCE_EVENT)
	{	
		LM_DBG("PRESENCE\n");
		hdr = msg->headers;
		while (hdr!= NULL)
		{
			if(cmp_hdrname_strzn(&hdr->name, "Subscription-State", 18)==0)
				break;
			hdr = hdr->next;
		}
		if(hdr && strncmp(hdr->body.s,"terminated", 10)== 0)
		{
			/* chack if reason timeout => don't send notification */
			if(strncmp(hdr->body.s+11,"reason=timeout", 14)== 0)
			{
				LM_DBG("Received Notification with state"
					"terminated; reason= timeout=> don't send notification\n");
				return 1;
			}	
			is_terminated= 1;

		}
	
		if(build_xmpp_content(&to_uri, &from_uri, &body, &id, is_terminated)< 0)
		{
			LM_ERR("in function build_xmpp_content\n");	
			goto error;
		}
	}	
	else
	{	
		if(event_flag & PWINFO_EVENT)
		{
			LM_DBG("PRESENCE.WINFO\n");
			hdr = msg->headers;
			while (hdr!= NULL)
			{
				if(cmp_hdrname_strzn(&hdr->name, "Subscription-State", 18)==0)
					break;
				hdr = hdr->next;
			}
			if(hdr && strncmp(hdr->body.s,"terminated", 10)== 0)
			{
				LM_DBG("Notify for presence.winfo with" 
					" Subscription-State terminated- should not translate\n");
				goto error;
			}
			if(winfo2xmpp(&to_uri, &body, &id)< 0)
			{
				LM_ERR("while sending subscription\n");
				goto error;
			}

		}
		else
		{
			LM_ERR("Missing or unsupported event header field value\n");
			goto error;
		}

	}
	free_to_params(&TO);
	return 1;

error:
	free_to_params(&TO);
	return 0;
}

int build_xmpp_content(str* to_uri, str* from_uri, str* body, str* id,
		int is_terminated)
{
	xmlDocPtr sip_doc= NULL;
	xmlDocPtr doc= NULL;
	xmlNodePtr xmpp_root= NULL;
	xmlNodePtr sip_root= NULL;
	xmlNodePtr new_node= NULL;
	xmlNodePtr node = NULL;
	xmlBufferPtr buffer= NULL;
	xmlAttrPtr attr= NULL;
	char* basic= NULL, *priority= NULL, *note= NULL;
	str xmpp_msg;

	LM_DBG("start...\n");

	/* creating the xml doc for the xmpp message*/
	doc= xmlNewDoc(0);
	if(doc== NULL)
	{
		LM_ERR("when creating new xml doc\n");
		goto error;
	}
	xmpp_root = xmlNewNode(NULL, BAD_CAST "presence");
	if(xmpp_root==0)
	{
		LM_ERR("when adding new node- presence\n");
		goto error;
	}
	xmlDocSetRootElement(doc, xmpp_root);

	attr= xmlNewProp(xmpp_root, BAD_CAST "to", BAD_CAST to_uri->s);
	if(attr== NULL)
	{
		LM_ERR("while adding new attribute\n");
		goto error;
	}
	attr= xmlNewProp(xmpp_root, BAD_CAST "from", BAD_CAST from_uri->s);
	if(attr== NULL)
	{
		LM_ERR("while adding new attribute\n");
		goto error;
	}
	if(is_terminated)
	{	
		attr=  xmlNewProp(xmpp_root, BAD_CAST "type", BAD_CAST "unsubscribed");
		if(attr== NULL)
		{
			LM_ERR("while adding new attribute\n");
			goto error;
		}
		goto done;
	}
	if(body->s== NULL)
	{
		attr=  xmlNewProp(xmpp_root, BAD_CAST "type", BAD_CAST "unavailable");
		if(attr== NULL)
		{
			LM_ERR("while adding new attribute\n");
			goto error;
		}
		goto done;
	}

	/*extractiong the information from the sip message body*/
	sip_doc= xmlParseMemory(body->s, body->len);
	if(sip_doc== NULL)
	{
		LM_ERR("while parsing xml memory\n");
		return -1;
	}
	sip_root= XMLDocGetNodeByName(sip_doc, "presence", NULL);
	if(sip_root== NULL)
	{
		LM_ERR("while extracting 'presence' node\n");
		goto error;
	}
	
	node = XMLNodeGetNodeByName(sip_root, "basic", NULL);
	if(node== NULL)
	{
		LM_ERR("while extracting status basic node\n");
		goto error;
	}
	basic= (char*)xmlNodeGetContent(node);
	if(basic== NULL)
	{
		LM_ERR("while extracting status basic node content\n");
		goto error;
	}
	if(xmlStrcasecmp( (unsigned char*)basic,(unsigned char*) "closed")==0 )
	{
		attr= xmlNewProp(xmpp_root, BAD_CAST "type", BAD_CAST "unavailable");
		if(attr== NULL)
		{
			LM_ERR("while adding node attr\n");
			xmlFree(basic);
			goto error;
		}
		xmlFree(basic);
		goto done;
	}/* else the status is open so no type attr should be added */

	xmlFree(basic);
	/* addind show node */
	node= XMLNodeGetNodeByName(sip_root, "note", NULL);
	if(node== NULL)
	{
		LM_DBG("No note node found\n");
		node= XMLNodeGetNodeByName(sip_root, "person", NULL);
		if(node== NULL)
		{
			LM_DBG("No person node found\n");
			goto done;
		}
		node= XMLNodeGetNodeByName(node, "note", NULL);
		if(node== NULL)
		{	
			LM_DBG("Person node has no note node\n");
			goto done;
		}	
	}	
	note= (char*)xmlNodeGetContent(node);
	if(note== NULL)
	{
		LM_ERR("while extracting note node content\n");
		goto error;
	}

	if((xmlStrcasecmp((unsigned char*)note, (unsigned char*)"away")== 0)||
			(xmlStrcasecmp((unsigned char*)note, (unsigned char*)"On the phone")== 0))
	{
		new_node = xmlNewChild(xmpp_root, NULL, BAD_CAST "show",
				BAD_CAST "away");
		if(new_node== NULL)
		{
			LM_ERR("while adding node show: away\n");
			goto error;
		}	
	}
	else
		if(xmlStrcasecmp((unsigned char*)note, (unsigned char*)"busy")== 0)
		{
			new_node = xmlNewChild(xmpp_root, NULL, BAD_CAST "show"
					, BAD_CAST "xa");
			if(new_node== NULL)
			{
				LM_ERR("while adding node show: away\n");
				goto error;
			}	
		}

		/*
		if(xmlStrcasecmp((unsigned char*)note, (unsigned char*)"on the phone")== 0)
		{
			new_node = xmlNewChild(xmpp_root, NULL, BAD_CAST "show", BAD_CAST "chat");
			if(new_node== NULL)
			{
				LM_ERR("while adding node show: chat\n");
				goto error;
			}	
		}
		else 
			if(xmlStrcasecmp((unsigned char*)note, (unsigned char*)"idle")== 0)
			{
				new_node = xmlNewChild(xmpp_root, NULL, BAD_CAST "show", BAD_CAST "idle");
				if(new_node== NULL)
				{
					LM_ERR("while adding node: idle\n");
					goto error;
				}	
			}*/
			else
				if((xmlStrcasecmp((unsigned char*)note,
					(unsigned char*)"dnd")== 0)||
					(xmlStrcasecmp((unsigned char*)note,
						(unsigned char*)"do not disturb")== 0)||
					(xmlStrcasecmp((unsigned char*)note,
					(unsigned char*)"Busy (DND)")== 0))
				{
					new_node = xmlNewChild(xmpp_root, NULL, BAD_CAST "show",
							BAD_CAST "dnd");
					if(new_node== NULL)
					{
						LM_ERR("while adding node show: dnd\n");
						goto error;
					}		
				}
				else
					LM_DBG("Not Found Status\n");

	
	/* adding status node */
	new_node = xmlNewChild(xmpp_root, NULL, BAD_CAST "status", BAD_CAST note);
	if(new_node== NULL)
	{
		LM_ERR("while adding node status\n");
		goto error;
	}	
	
	xmlFree(note);
	note= NULL;

	/* adding priotity node*/
	node= XMLNodeGetNodeByName(sip_root, "contact", NULL);
	if(node== NULL)
	{
		LM_DBG("No contact node found\n");
	}
	else
	{
		priority= XMLNodeGetAttrContentByName(node, "priority");
		if(priority== NULL)
			LM_DBG("No priority attribute found\n");
		else
		{
			new_node= xmlNewChild(xmpp_root, NULL, BAD_CAST "priority",
					BAD_CAST priority);
			if(sip_root== NULL)
			{
				LM_ERR("while adding node\n");
				xmlFree(priority);
				goto error;
			}
			xmlFree(priority);
		}
	}

done:
	buffer= xmlBufferCreate();
	if(buffer== NULL)
	{
		LM_ERR("while adding creating new buffer\n");
		goto error;
	}
	
	xmpp_msg.len= xmlNodeDump(buffer, doc, xmpp_root, 1,1);
	if(xmpp_msg.len== -1)
	{
		LM_ERR("while dumping node\n");
		goto error;
	}
	xmpp_msg.s= (char*)xmlBufferContent( buffer);
	if(xmpp_msg.s==  NULL)
	{
		LM_ERR("while extracting buffer content\n");
		goto error;
	}
	
	LM_DBG("xmpp_msg: %.*s\n",xmpp_msg.len, xmpp_msg.s);
	if( xmpp_notify(from_uri, to_uri, &xmpp_msg, id)< 0)
	{
		LM_ERR("while sending xmpp_notify\n");
		goto error;
	}

	xmlBufferFree(buffer);
	xmlCleanupParser();
	xmlMemoryDump();

	if(sip_doc)
		xmlFreeDoc(sip_doc);
	if(doc)
		xmlFreeDoc(doc);
	return 0;

error:
	if(sip_doc)
		xmlFreeDoc(sip_doc);
	if(note)
		xmlFree(note);
	if(buffer)
		xmlBufferFree(buffer);
	xmlCleanupParser();
	xmlMemoryDump();

	return -1;

}


int winfo2xmpp(str* to_uri, str* body, str* id)
{
	xmlAttrPtr attr= NULL;
	str xmpp_msg;
	char* watcher= NULL ;
	str from_uri;
	xmlDocPtr notify_doc= NULL;
	xmlDocPtr doc= NULL;
	xmlNodePtr pidf_root= NULL;
	xmlNodePtr root_node= NULL;
	xmlNodePtr node= NULL;
	xmlBufferPtr buffer= NULL;

	LM_DBG("start...\n");
	notify_doc= xmlParseMemory(body->s, body->len);
	if(notify_doc== NULL)
	{
		LM_ERR("while parsing xml memory\n");
		return -1;
	}
	pidf_root= XMLDocGetNodeByName(notify_doc, "watcherinfo", NULL);
	if(pidf_root== NULL)
	{
		LM_ERR("while extracting 'presence' node\n");
		goto error;
	}
	
	node = XMLNodeGetNodeByName(pidf_root, "watcher", NULL);

	for (; node!=NULL; node = node->next)
	{		
		if( xmlStrcasecmp(node->name,(unsigned char*)"watcher"))
			continue;

		watcher= (char*)xmlNodeGetContent(node->children);	
		if(watcher== NULL)
		{
			LM_ERR("while extracting watcher node content\n");
			goto error;
		}
		from_uri.s= euri_sip_xmpp(watcher);
		if(from_uri.s== NULL)
		{
			LM_ERR("while encoding sip uri in xmpp\n");
			goto error;
		}	
		from_uri.len= strlen(from_uri.s);
		xmlFree(watcher);
		watcher= NULL;

		doc= xmlNewDoc( 0 );
		if(doc== NULL)
		{
			LM_ERR("when creating new xml doc\n");
			goto error;
		}
		root_node = xmlNewNode(NULL, BAD_CAST "presence");
		if(root_node== NULL)
		{
			LM_ERR("when adding new node\n");
			goto error;
		}
		xmlDocSetRootElement(doc, root_node);

		attr= xmlNewProp(root_node, BAD_CAST "to", BAD_CAST to_uri->s);
		if(attr== NULL)
		{
			LM_ERR("while adding attribute to_uri\n");
			goto error;
		}
		attr= xmlNewProp(root_node, BAD_CAST "from", BAD_CAST from_uri.s);
		if(attr== NULL)
		{
			LM_ERR("while adding attribute from_uri\n");
			goto error;
		}
		attr= xmlNewProp(root_node, BAD_CAST "type", BAD_CAST "subscribe");
		if(attr== NULL)
		{
			LM_ERR("while adding attribute type\n");
			goto error;
		}
		buffer= xmlBufferCreate();
		if(buffer== NULL)
		{
			LM_ERR("while adding creating new buffer\n");
			goto error;
		}
		
		xmpp_msg.len= xmlNodeDump(buffer, doc, root_node, 1,1);
		if(xmpp_msg.len== -1)
		{
			LM_ERR("while dumping node\n");
			goto error;
		}
		xmpp_msg.s= (char*)xmlBufferContent( buffer);
		if(xmpp_msg.s==  NULL)
		{
			LM_ERR("while extracting buffer content\n");
			goto error;
		}
	
		LM_DBG("xmpp_msg: %.*s\n",xmpp_msg.len, xmpp_msg.s);
		
		if( xmpp_subscribe(&from_uri, to_uri, &xmpp_msg, id)< 0)
		{
			LM_ERR("while sending xmpp_subscribe\n");
			goto error;
		}
		xmlBufferFree(buffer);
		buffer= NULL;
		xmlFreeDoc(doc);
		doc= NULL;
	}

	xmlFreeDoc(notify_doc);
	xmlCleanupParser();
	xmlMemoryDump();
	return 0;

error:

	if(doc)
		xmlFreeDoc(doc);
	if(notify_doc)
		xmlFreeDoc(notify_doc);
	if(watcher)
		xmlFree(watcher);
	if(buffer)
		xmlBufferFree(buffer);
	xmlCleanupParser();
	xmlMemoryDump();

	return -1;

}

char* get_error_reason(int code, str* reason)
{
	char* err_cond= NULL;

	err_cond= (char*)pkg_malloc(50* sizeof(char));
	if(err_cond== NULL)
	{
		LM_ERR("no more memory\n");
		return NULL;
	}
	
	switch( code )
	{
		case 300:	{ strcpy(err_cond, "redirect");				break;}
		case 301:	{ strcpy(err_cond, "gone");				break;}
		case 302:	{ strcpy(err_cond, "redirect");				break;}
		case 305:	{ strcpy(err_cond, "redirect");				break;}
		case 380:	{ strcpy(err_cond, "not-acceptable");			break;}
		case 400:	{ strcpy(err_cond, "bad-request");			break;}
		case 401:	{ strcpy(err_cond, "not-authorized");			break;}
		case 402:	{ strcpy(err_cond, "payment-required");			break;}
		case 403:	{ strcpy(err_cond, "forbidden");			break;}
		case 404:	{ strcpy(err_cond, "item-not-found");			break;}
		case 405:	{ strcpy(err_cond, "not-allowed");			break;}
		case 406:	{ strcpy(err_cond, "not-acceptable");			break;}
		case 407:	{ strcpy(err_cond, "registration-required");		break;}
		case 408:	{ strcpy(err_cond, "service-unavailable");		break;}
		case 410:	{ strcpy(err_cond, "gone");				break;}
		case 413: 	{ strcpy(err_cond, "bad-request");			break;}
		case 414:	{ strcpy(err_cond, "bad-request");			break;}
		case 415:	{ strcpy(err_cond, "bad-request");			break;}
		case 416:	{ strcpy(err_cond, "bad-request");			break;}
		case 420:	{ strcpy(err_cond, "bad-request");			break;}
		case 421:	{ strcpy(err_cond, "bad-request");			break;}
		case 423:	{ strcpy(err_cond, "bad-request");			break;}
		case 480:	{ strcpy(err_cond, "recipient-unavailable");		break;}
		case 481: 	{ strcpy(err_cond, "item-not-found");			break;}
		case 482:	{ strcpy(err_cond, "not-acceptable");			break;}
		case 483:	{ strcpy(err_cond, "not-acceptable");			break;}
		case 484:	{ strcpy(err_cond, "jid-malformed");			break;}
		case 485: 	{ strcpy(err_cond, "item-not-found");			break;}
		case 488:	{ strcpy(err_cond, "not-acceptable");			break;}
		case 491:	{ strcpy(err_cond, "unexpected-request");		break;}
		case 500:	{ strcpy(err_cond, "internal-server-error");		break;}
		case 501:	{ strcpy(err_cond, "feature-not-implemented");		break;}
		case 502:	{ strcpy(err_cond, "remote-server-not-found");		break;}
		case 503:	{ strcpy(err_cond, "service-unavailable");		break;}
		case 504:	{ strcpy(err_cond, "remote-server-timeout");		break;}
		case 505:	{ strcpy(err_cond, "not-acceptable");			break;}
		case 513:	{ strcpy(err_cond, "bad-request");			break;}
		case 600:	{ strcpy(err_cond, "service-unavailable");		break;}
		case 603:	{ strcpy(err_cond, "service-unavailable");		break;}
		case 604:	{ strcpy(err_cond, "item-not-found");			break;}
		case 606:	{ strcpy(err_cond, "not-acceptable");			break;}
		default:	{ strcpy(err_cond, "not-acceptable");			break;}
	}

	return err_cond;
}	


int Sipreply2Xmpp(ua_pres_t* hentity, struct sip_msg * msg) 
{
	char* uri;
	/* named according to the direction of the message in xmpp*/
	str from_uri;
	str to_uri;
	xmlDocPtr doc= NULL;
	xmlNodePtr root_node= NULL, node = NULL;
	xmlAttrPtr attr= NULL;
	str xmpp_msg;
	int code;
	str reason;
	char* err_reason= NULL;
	xmlBufferPtr buffer= NULL;
	
	LM_DBG("start..\n");
	uri=(char*)pkg_malloc(sizeof(char)*( hentity->watcher_uri->len+1));
	if(uri== NULL)
	{
		LM_ERR("no more memory\n");
		goto error;
	}
	memcpy(uri, hentity->watcher_uri->s, hentity->watcher_uri->len);
	uri[hentity->watcher_uri->len]= '\0';
	to_uri.s= duri_sip_xmpp(uri);
	if(to_uri.s== NULL)
	{
		LM_ERR("whil decoding sip uri in xmpp\n");
		pkg_free(uri);
		goto error;	
	}	

	to_uri.len= strlen(to_uri.s);
	pkg_free(uri);
	
	uri=(char*)pkg_malloc(sizeof(char)*( hentity->pres_uri->len+1));
	if(uri== NULL)
	{
		LM_ERR("no more memory\n");
		goto error;
	}
	memcpy(uri, hentity->pres_uri->s, hentity->pres_uri->len);
	uri[hentity->pres_uri->len]= '\0';
	from_uri.s= euri_sip_xmpp(uri);
	if(from_uri.s== NULL)
	{
		LM_ERR("while encoding sip uri in xmpp\n");
		pkg_free(uri);
		goto error;
	}

	from_uri.len= strlen(from_uri.s);
	pkg_free(uri);

	doc= xmlNewDoc(BAD_CAST "1.0");
	if(doc==0)
		goto error;
	root_node = xmlNewNode(NULL, BAD_CAST "presence");
	
	if(root_node==0)
		goto error;
	xmlDocSetRootElement(doc, root_node);

	attr= xmlNewProp(root_node, BAD_CAST "to", BAD_CAST to_uri.s);
	if(attr== NULL)
	{
		LM_ERR("while adding attribute to\n");
		goto error;
	}
	attr= xmlNewProp(root_node, BAD_CAST "from", BAD_CAST from_uri.s);
	if(attr== NULL)
	{
		LM_ERR("while adding attribute from\n");
		goto error;
	}

	if(msg== FAKED_REPLY)
	{
		code = 408;
		reason.s= "Request Timeout";
		reason.len= strlen(reason.s)- 1;
	}
	else
	{
		code= msg->first_line.u.reply.statuscode;
		reason= msg->first_line.u.reply.reason;
	}

	LM_DBG(" to_uri= %s\n\t from_uri= %s\n",
			to_uri.s, from_uri.s);

	if(code>=300)
	{
		LM_DBG(" error code(>= 300)\n");
		err_reason= get_error_reason(code, &reason);
		if(err_reason== NULL)
		{
			LM_ERR("couldn't get response phrase\n");
			goto error;
		}
	
		attr= xmlNewProp(root_node, BAD_CAST "type", BAD_CAST "error");
		if(attr== NULL)
		{
			LM_ERR("while adding new attribute\n");
			goto error;
		}
		node= xmlNewChild(root_node, 0, BAD_CAST  "error", 0 );
		if(node== NULL)
		{
			LM_ERR("while adding new node\n");
			goto error;
		}	
		node= xmlNewChild(node, 0,  BAD_CAST err_reason, 0 );
		if(node== NULL)
		{
			LM_ERR("while adding new node\n");
			goto error;
		}	

		attr= xmlNewProp(node, BAD_CAST "xmlns", 
				BAD_CAST "urn:ietf:params:xml:ns:xmpp-stanzas");
		if(attr== NULL)
		{
			LM_ERR("while adding new attribute\n");
			goto error;
		}

	}
	else
		if(code>=200 )
		{
			LM_DBG(" 2xx code\n");
			attr= xmlNewProp(root_node, BAD_CAST "type", BAD_CAST "subscribed");
			if(attr== NULL)
			{
				LM_ERR("while adding new attribute\n");
				goto error;
			}
		}

		buffer= xmlBufferCreate();
		if(buffer== NULL)
		{
			LM_ERR("while adding creating new buffer\n");
			goto error;
		}
		
		xmpp_msg.len= xmlNodeDump(buffer, doc, root_node, 1,1);
		if(xmpp_msg.len== -1)
		{
			LM_ERR("while dumping node\n");
			goto error;
		}
		xmpp_msg.s= (char*)xmlBufferContent( buffer);
		if(xmpp_msg.s==  NULL)
		{
			LM_ERR("while extracting buffer content\n");
			goto error;
		}
	

	LM_DBG("xmpp_msg: %.*s\n",xmpp_msg.len, xmpp_msg.s);
	
	if(xmpp_packet(&from_uri, &to_uri, &xmpp_msg, &hentity->to_tag)< 0)
	{
		LM_ERR("while sending xmpp_reply_to_subscribe\n");
		goto error;
	}
	if(err_reason)
		pkg_free(err_reason);
	xmlFreeDoc(doc);
	
	return 0;

error:

	if(doc)
		xmlFreeDoc(doc);
	if(err_reason)
		pkg_free(err_reason);
	return -1;

}


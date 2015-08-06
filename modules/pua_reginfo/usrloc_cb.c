/*
 * pua_reginfo module - Presence-User-Agent Handling of reg events
 *
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
 * http://www.ng-voice.com
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

#include "usrloc_cb.h"
#include "pua_reginfo.h"
#include <libxml/parser.h>
#include "../pua/pua.h"
#include "../pua/send_publish.h"

/*
Contact: <sip:carsten@10.157.87.36:44733;transport=udp>;expires=600000;+g.oma.sip-im;language="en,fr";+g.3gpp.smsip;+g.oma.sip-im.large-message;audio;+g.3gpp.icsi-ref="urn%3Aurn-7%3A3gpp-application.ims.iari.gsma-vs";+g.3gpp.cs-voice.
Call-ID: 9ad9f89f-164d-bb86-1072-52e7e9eb5025.
*/

/*<?xml version="1.0"?>
<reginfo xmlns="urn:ietf:params:xml:ns:reginfo" version="0" state="full">
.<registration aor="sip:carsten@ng-voice.com" id="0xb33fa860" state="active">
..<contact id="0xb33fa994" state="active" event="registered" expires="3600">
...<uri>sip:carsten@10.157.87.36:43582;transport=udp</uri>
...<unknown-param name="+g.3gpp.cs-voice"></unknown-param>
...<unknown-param name="+g.3gpp.icsi-ref">urn0X0.0041FB74E7B54P-1022urn-70X0P+03gpp-application.ims.iari.gsma-vs</unknown-param>
...<unknown-param name="audio"></unknown-param>
...<unknown-param name="+g.oma.sip-im.large-message"></unknown-param>
...<unknown-param name="+g.3gpp.smsip"></unknown-param>
...<unknown-param name="language">en,fr</unknown-param>
...<unknown-param name="+g.oma.sip-im"></unknown-param>
...<unknown-param name="expires">600000</unknown-param>
..</contact>
.</registration>
</reginfo> */

static int _pua_reginfo_self_op = 0;

void pua_reginfo_update_self_op(int v)
{
	_pua_reginfo_self_op = v;
}

str* build_reginfo_full(urecord_t * record, str uri, ucontact_t* c, int type) {
	xmlDocPtr  doc = NULL; 
	xmlNodePtr root_node = NULL;
	xmlNodePtr registration_node = NULL;
	xmlNodePtr contact_node = NULL;
	xmlNodePtr uri_node = NULL;
	str * body= NULL;
	ucontact_t * ptr;
	char buf[512];
	int reg_active = 0;
	time_t cur_time = time(0);

	/* create the XML-Body */
	doc = xmlNewDoc(BAD_CAST "1.0");
	if(doc==0) {
		LM_ERR("Unable to create XML-Doc\n");
		return NULL;
	}

	root_node = xmlNewNode(NULL, BAD_CAST "reginfo");
	if(root_node==0) {
		LM_ERR("Unable to create reginfo-XML-Element\n");
		return NULL;
	}
	/* This is our Root-Element: */
    	xmlDocSetRootElement(doc, root_node);
	
	xmlNewProp(root_node, BAD_CAST "xmlns",	BAD_CAST "urn:ietf:params:xml:ns:reginfo");

	/* we set the version to 0 but it should be set to the correct value in the pua module */
	xmlNewProp(root_node, BAD_CAST "version", BAD_CAST "0");
	xmlNewProp(root_node, BAD_CAST "state", BAD_CAST "full" );

	/* Registration Node */
	registration_node =xmlNewChild(root_node, NULL, BAD_CAST "registration", NULL) ;
	if( registration_node ==NULL) {
		LM_ERR("while adding child\n");
		goto error;
	}

	/* Add the properties to this Node for AOR and ID: */
	xmlNewProp(registration_node, BAD_CAST "aor", BAD_CAST uri.s);
	snprintf(buf, sizeof(buf), "%p", record);
	xmlNewProp(registration_node, BAD_CAST "id", BAD_CAST buf);

	LM_DBG("Updated Contact %.*s[%.*s]\n", c->c.len, c->c.s,
		c->ruid.len, c->ruid.s);

	ptr = record->contacts;
	while (ptr) {
		if (VALID_CONTACT(ptr, cur_time)) {
			LM_DBG("Contact %.*s[%.*s]\n", ptr->c.len, ptr->c.s,
				ptr->ruid.len, ptr->ruid.s);
			/* Contact-Node */
			contact_node =xmlNewChild(registration_node, NULL, BAD_CAST "contact", NULL) ;
			if( contact_node ==NULL) {
				LM_ERR("while adding child\n");
				goto error;
			}
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "%p", ptr);
			xmlNewProp(contact_node, BAD_CAST "id", BAD_CAST buf);
			/* Check, if this is the modified contact: */
			if ((c->ruid.len == ptr->ruid.len) &&
				!memcmp(c->ruid.s, ptr->ruid.s, c->ruid.len))
			{
				if ((type & UL_CONTACT_INSERT) || (type & UL_CONTACT_UPDATE)) {
					reg_active = 1;
					xmlNewProp(contact_node, BAD_CAST "state", BAD_CAST "active");
				} else
					xmlNewProp(contact_node, BAD_CAST "state", BAD_CAST "terminated");
				if (type & UL_CONTACT_INSERT) xmlNewProp(contact_node, BAD_CAST "event", BAD_CAST "created");
				else if (type & UL_CONTACT_UPDATE) xmlNewProp(contact_node, BAD_CAST "event", BAD_CAST "refreshed");
				else if (type & UL_CONTACT_EXPIRE) xmlNewProp(contact_node, BAD_CAST "event", BAD_CAST "expired");
				else if (type & UL_CONTACT_DELETE) xmlNewProp(contact_node, BAD_CAST "event", BAD_CAST "unregistered");
				else xmlNewProp(contact_node, BAD_CAST "event", BAD_CAST "unknown");
				memset(buf, 0, sizeof(buf));
				snprintf(buf, sizeof(buf), "%i", (int)(ptr->expires-cur_time));
				xmlNewProp(contact_node, BAD_CAST "expires", BAD_CAST buf);
			} else {
				reg_active = 1;
				xmlNewProp(contact_node, BAD_CAST "state", BAD_CAST "active");
				xmlNewProp(contact_node, BAD_CAST "event", BAD_CAST "registered");
				memset(buf, 0, sizeof(buf));
				snprintf(buf, sizeof(buf), "%i", (int)(ptr->expires-cur_time));
				xmlNewProp(contact_node, BAD_CAST "expires", BAD_CAST buf);
			}
			if (ptr->q != Q_UNSPECIFIED) {
				float q = (float)ptr->q/1000;
				memset(buf, 0, sizeof(buf));
				snprintf(buf, sizeof(buf), "%.3f", q);
				xmlNewProp(contact_node, BAD_CAST "q", BAD_CAST buf);
			}
			/* CallID Attribute */
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "%.*s", ptr->callid.len, ptr->callid.s);
			xmlNewProp(contact_node, BAD_CAST "callid", BAD_CAST buf);

			/* CSeq Attribute */
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "%d", ptr->cseq);
			xmlNewProp(contact_node, BAD_CAST "cseq", BAD_CAST buf);

			/* received Attribute */
			memset(buf, 0, sizeof(buf));
	                snprintf(buf, sizeof(buf), "%.*s", ptr->received.len, ptr->received.s);
         	       	xmlNewProp(contact_node, BAD_CAST "received", BAD_CAST buf);
			
			/* path Attribute */
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "%.*s", ptr->path.len, ptr->path.s);
			xmlNewProp(contact_node, BAD_CAST "path", BAD_CAST buf);

			/* user_agent Attribute */
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "%.*s", ptr->user_agent.len, ptr->user_agent.s);
			xmlNewProp(contact_node, BAD_CAST "user_agent", BAD_CAST buf);

			/* URI-Node */
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "%.*s", ptr->c.len, ptr->c.s);
			uri_node = xmlNewChild(contact_node, NULL, BAD_CAST "uri", BAD_CAST buf) ;
			if(uri_node == NULL) {
				LM_ERR("while adding child\n");
				goto error;
			}
		}
		ptr = ptr->next;
	}

	/* add registration state (at least one active contact): */
	if (reg_active==0)
		xmlNewProp(registration_node, BAD_CAST "state", BAD_CAST "terminated");
	else
		xmlNewProp(registration_node, BAD_CAST "state", BAD_CAST "active");


	/* create the body */
	body = (str*)pkg_malloc(sizeof(str));
	if(body == NULL) {
		LM_ERR("while allocating memory\n");
		return NULL;
	}
	memset(body, 0, sizeof(str));

	/* Write the XML into the body */
	xmlDocDumpFormatMemory(doc,(unsigned char**)(void*)&body->s,&body->len,1);

	/*free the document */
	xmlFreeDoc(doc);
	xmlCleanupParser();

	return body;
error:
	if(body) {
		if(body->s) xmlFree(body->s);
		pkg_free(body);
	}
	if(doc) xmlFreeDoc(doc);
	return NULL;
}	

void reginfo_usrloc_cb(ucontact_t* c, int type, void* param) {
	str* body= NULL;
	publ_info_t publ;
	str content_type;
	udomain_t * domain;
	urecord_t * record;
	int res;
	str uri = {NULL, 0};
	str user = {NULL, 0};

	char* at = NULL;
	char id_buf[512];
	int id_buf_len;

	if(_pua_reginfo_self_op == 1) {
		LM_DBG("operation triggered by own action for aor: %.*s (%d)\n",
				c->aor->len, c->aor->s, type);
		return;
	}

	content_type.s = "application/reginfo+xml";
	content_type.len = 23;
	
	/* Debug Output: */
	LM_DBG("AOR: %.*s (%.*s)\n", c->aor->len, c->aor->s, c->domain->len, c->domain->s);
	if(type & UL_CONTACT_INSERT) LM_DBG("type= UL_CONTACT_INSERT\n");
	else if(type & UL_CONTACT_UPDATE) LM_DBG("type= UL_CONTACT_UPDATE\n");
	else if(type & UL_CONTACT_EXPIRE) LM_DBG("type= UL_CONTACT_EXPIRE\n");
	else if(type & UL_CONTACT_DELETE) LM_DBG("type= UL_CONTACT_DELETE\n");
	else {
		LM_ERR("Unknown Type %i\n", type);
		return;
	}
	/* make a local copy of the AOR */
	user.len = c->aor->len;
	user.s = c->aor->s;

	/* Get the UDomain for this account */
	res = ul.get_udomain(c->domain->s, &domain);
	if(res < 0) {
		LM_ERR("no domain found\n");
		return;
	}

	/* Get the URecord for this AOR */
	res = ul.get_urecord(domain, &user, &record);
	if (res > 0) {
		LM_ERR("' %.*s (%.*s)' Not found in usrloc\n", c->aor->len, c->aor->s, c->domain->len, c->domain->s);
		return;
	}

	/* Create AOR to be published */
	/* Search for @ in the AOR. In case no domain was provided, we will add the "default domain" */
	at = memchr(record->aor.s, '@', record->aor.len);
	if (!at) {
		uri.len = record->aor.len + default_domain.len + 6;
		uri.s = (char*)pkg_malloc(sizeof(char) * uri.len);
		if(uri.s == NULL) {
			LM_ERR("Error allocating memory for URI!\n");
			goto error;
		}
		if (record->aor.len > 0)
			uri.len = snprintf(uri.s, uri.len, "sip:%.*s@%.*s", record->aor.len, record->aor.s, default_domain.len, default_domain.s);
		else
			uri.len = snprintf(uri.s, uri.len, "sip:%.*s", default_domain.len, default_domain.s);
	} else {
		uri.len = record->aor.len + 6;
		uri.s = (char*)pkg_malloc(sizeof(char) * uri.len);
		if(uri.s == NULL) {
			LM_ERR("Error allocating memory for URI!\n");
			goto error;
		}
		uri.len = snprintf(uri.s, uri.len, "sip:%.*s", record->aor.len, record->aor.s);
	}
	
	/* Build the XML-Body: */
	body = build_reginfo_full(record, uri, c, type);

	if(body == NULL || body->s == NULL) {
		LM_ERR("Error on creating XML-Body for publish\n");
		goto error;
	}
	LM_DBG("XML-Body:\n%.*s\n", body->len, body->s);

	LM_DBG("Contact %.*s, %p\n", c->c.len, c->c.s, c);

	memset(&publ, 0, sizeof(publ_info_t));

	publ.pres_uri = &uri;
	publ.body = body;
	id_buf_len = snprintf(id_buf, sizeof(id_buf), "REGINFO_PUBLISH.%.*s@%.*s",
		c->aor->len, c->aor->s,
		c->domain->len, c->domain->s);
	publ.id.s = id_buf;
	publ.id.len = id_buf_len;
	publ.content_type = content_type;
	publ.expires = 3600;
	
	/* make UPDATE_TYPE, as if this "publish dialog" is not found 
	   by pua it will fallback to INSERT_TYPE anyway */
	publ.flag|= UPDATE_TYPE;
	publ.source_flag |= REGINFO_PUBLISH;
	publ.event |= REGINFO_EVENT;
	publ.extra_headers= NULL;

	if(pua.send_publish(&publ) < 0) {
		LM_ERR("Error while sending publish\n");
	}	
error:
	if (uri.s) pkg_free(uri.s);
	if(body) {
		if(body->s) xmlFree(body->s);
		pkg_free(body);
	}

	return;
}	

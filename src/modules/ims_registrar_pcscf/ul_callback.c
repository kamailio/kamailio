/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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
 */

#include "ul_callback.h"
#include "../pua/pua.h"
#include "../pua/send_publish.h"

#include "../pua/pua_bind.h"
#include "async_reginfo.h"

#include <libxml/parser.h>

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

extern pua_api_t pua;
extern str pcscf_uri;
extern int publish_reginfo;

/* methods for building reg publish */
str* build_reginfo_partial(ppublic_t *impu, struct pcontact* c, int type) {
	xmlDocPtr doc = NULL;
	xmlNodePtr root_node = NULL;
	xmlNodePtr registration_node = NULL;
	xmlNodePtr contact_node = NULL;
	xmlNodePtr uri_node = NULL;
	str * body = NULL;
	char buf[512];

	/* create the XML-Body */
	doc = xmlNewDoc(BAD_CAST "1.0");
	if (doc == 0) {
		LM_ERR("Unable to create XML-Doc\n");
		return NULL;
	}

	root_node = xmlNewNode(NULL, BAD_CAST "reginfo");
	if (root_node == 0) {
		LM_ERR("Unable to create reginfo-XML-Element\n");
		return NULL;
	}
	/* This is our Root-Element: */
	xmlDocSetRootElement(doc, root_node);

	xmlNewProp(root_node, BAD_CAST "xmlns", BAD_CAST "urn:ietf:params:xml:ns:reginfo");

	/* we set the version to 0 but it should be set to the correct value in the pua module */
	xmlNewProp(root_node, BAD_CAST "version", BAD_CAST "0");
	xmlNewProp(root_node, BAD_CAST "state", BAD_CAST "partial" );

	/* Registration Node */
	registration_node = xmlNewChild(root_node, NULL, BAD_CAST "registration", NULL);
	if (registration_node == NULL) {
		LM_ERR("while adding child\n");
		goto error;
	}

	/* Add the properties to this Node for AOR and ID: */
	//registration aor nodes
	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "%.*s", impu->public_identity.len, impu->public_identity.s);
	xmlNewProp(registration_node, BAD_CAST "aor", BAD_CAST buf);
	
	//registration id
	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "%p", impu);
	xmlNewProp(registration_node, BAD_CAST "id", BAD_CAST buf);

	//now the updated contact
	contact_node =xmlNewChild(registration_node, NULL, BAD_CAST "contact", NULL);
	if (contact_node == NULL) {
		LM_ERR("while adding child\n");
		goto error;
	}
	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "%p", c);
	xmlNewProp(contact_node, BAD_CAST "id", BAD_CAST buf);

	//TODO: currently we only support publish of termination for event unregistered and expires 0
	xmlNewProp(contact_node, BAD_CAST "state", BAD_CAST "terminated");
	xmlNewProp(contact_node, BAD_CAST "event", BAD_CAST "unregistered");
	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "%i", 0);
	xmlNewProp(contact_node, BAD_CAST "expires", BAD_CAST buf);

	/* URI-Node */
	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "%.*s", c->aor.len, c->aor.s);
	uri_node = xmlNewChild(contact_node, NULL, BAD_CAST "uri", BAD_CAST buf);
	if (uri_node == NULL) {
		LM_ERR("while adding child\n");
		goto error;
	}

	/* create the body */
	body = (str*) pkg_malloc(sizeof(str));
	if (body == NULL) {
		LM_ERR("while allocating memory\n");
		return NULL;
	}
	memset(body, 0, sizeof(str));

	/* Write the XML into the body */
	xmlDocDumpFormatMemory(doc, (unsigned char**) (void*) &body->s, &body->len,
			1);

	/*free the document */
	xmlFreeDoc(doc);
	xmlCleanupParser();

	return body;

error:
	if (body) {
		if (body->s)
			xmlFree(body->s);
		pkg_free(body);
	}
	if (doc)
		xmlFreeDoc(doc);
	return NULL;

}

#define P_ASSERTED_IDENTITY_HDR_PREFIX	"P-Asserted-Identity: <"
int send_partial_publish(ppublic_t *impu, struct pcontact *c, int type)
{
	//publ_info_t publ;
	str content_type;
	int id_buf_len;
	char id_buf[512];
	str p_asserted_identity_header;
	str publ_id;
	reginfo_event_t *new_event;
	
	content_type.s = "application/reginfo+xml";
	content_type.len = 23;
	
	int len = strlen(P_ASSERTED_IDENTITY_HDR_PREFIX) + pcscf_uri.len + 1 + CRLF_LEN;
	p_asserted_identity_header.s = (char *)pkg_malloc( len );
	if ( p_asserted_identity_header.s == NULL ) {
	    LM_ERR( "insert_asserted_identity: pkg_malloc %d bytes failed", len );
	    goto error;
	}
	
	memcpy(p_asserted_identity_header.s, P_ASSERTED_IDENTITY_HDR_PREFIX, strlen(P_ASSERTED_IDENTITY_HDR_PREFIX));
	p_asserted_identity_header.len = strlen(P_ASSERTED_IDENTITY_HDR_PREFIX);
	memcpy(p_asserted_identity_header.s + p_asserted_identity_header.len, pcscf_uri.s, pcscf_uri.len);
	p_asserted_identity_header.len += pcscf_uri.len;
	*(p_asserted_identity_header.s + p_asserted_identity_header.len) = '>';
	p_asserted_identity_header.len ++;
	memcpy( p_asserted_identity_header.s + p_asserted_identity_header.len, CRLF, CRLF_LEN );
	p_asserted_identity_header.len += CRLF_LEN;
	
	LM_DBG("p_asserted_identity_header: [%.*s]", p_asserted_identity_header.len, p_asserted_identity_header.s);
	
	LM_DBG("Sending publish\n");
	str *body = build_reginfo_partial(impu, c, type);

	if (body == NULL || body->s == NULL) {
		LM_ERR("Error on creating XML-Body for publish\n");
		goto error;
	}
	LM_DBG("XML-Body:\n%.*s\n", body->len, body->s);
	
	id_buf_len = snprintf(id_buf, sizeof(id_buf), "IMSPCSCF_PUBLISH.%.*s", c->aor.len, c->aor.s);
	publ_id.s = id_buf;
	publ_id.len = id_buf_len;

	new_event = new_reginfo_event(REG_EVENT_PUBLISH, body, &publ_id, &content_type, 0, 0,
	0, 0, 3600, UPDATE_TYPE, REGINFO_PUBLISH, REGINFO_EVENT, &p_asserted_identity_header, &impu->public_identity);
	
	if (!new_event) {
            LM_ERR("Unable to create event for cdp callback\n");
            goto error;
        }
        //push the new event onto the stack (FIFO)
        push_reginfo_event(new_event);
	
	if (p_asserted_identity_header.s) {
		pkg_free(p_asserted_identity_header.s);
	}
	
	if (body) {
		if (body->s)
			xmlFree(body->s);
		pkg_free(body);
	}
	
	return 1;

error:
	
	if (p_asserted_identity_header.s) {
		pkg_free(p_asserted_identity_header.s);
	}
	if (body) {
		if (body->s)
			xmlFree(body->s);
		pkg_free(body);
	}
	return -1;
}

void callback_pcscf_contact_cb(struct pcontact *c, int type, void *param) {
	ppublic_t *ptr;

	LM_DBG("----------------------!\n");
	LM_DBG("PCSCF Contact Callback in regsitrar!\n");
	LM_DBG("Contact AOR: [%.*s]\n", c->aor.len, c->aor.s);
	LM_DBG("Callback type [%d]\n", type);
	LM_DBG("Reg state [%d]\n", c->reg_state);

	if ((type&PCSCF_CONTACT_UPDATE)) {
		//send publish for each associated IMPU
		ptr = c->head;
			while (ptr) {
				if (c->reg_state == PCONTACT_DEREG_PENDING_PUBLISH && publish_reginfo) {
					LM_DBG("delete/update on contact <%.*s> associated with IMPU <%.*s> (sending publish)\n",
											c->aor.len, c->aor.s,
											ptr->public_identity.len, ptr->public_identity.s);
					if (ptr->public_identity.len > 4 && strncasecmp(ptr->public_identity.s,"tel:",4)==0) {
					    LM_DBG("This is a tel URI - it is not routable so we don't publish for it");
					}else{
					    send_partial_publish(ptr, c, type);
					}
				}

				ptr = ptr->next;
			}

	}
}


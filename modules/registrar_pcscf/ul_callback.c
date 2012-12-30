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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */

#include "ul_callback.h"
//#include "../../modules_k/pua/pua.h"
//#include "../../modules_k/pua/send_publish.h"
#include <libxml/parser.h>

/* methods for building reg publish */
//str* build_reginfo_partial(ppublic_t *impu, struct pcontact* c, int type) {
//	xmlDocPtr doc = NULL;
//	xmlNodePtr root_node = NULL;
//	xmlNodePtr registration_node = NULL;
//	xmlNodePtr contact_node = NULL;
//	xmlNodePtr uri_node = NULL;
//	str * body = NULL;
//	struct pcontact * ptr;
//	char buf[512];
//	int buf_len;
//	int reg_active = 0;
//	time_t cur_time = time(0);
//
//	/* create the XML-Body */
//	doc = xmlNewDoc(BAD_CAST "1.0");
//	if (doc == 0) {
//		LM_ERR("Unable to create XML-Doc\n");
//		return NULL;
//	}
//
//	root_node = xmlNewNode(NULL, BAD_CAST "reginfo");
//	if (root_node == 0) {
//		LM_ERR("Unable to create reginfo-XML-Element\n");
//		return NULL;
//	}
//	/* This is our Root-Element: */
//	xmlDocSetRootElement(doc, root_node);
//
//	xmlNewProp(root_node, BAD_CAST "xmlns", BAD_CAST "urn:ietf:params:xml:ns:reginfo");
//
//	/* we set the version to 0 but it should be set to the correct value in the pua module */
//	xmlNewProp(root_node, BAD_CAST "version", BAD_CAST "0");
//	xmlNewProp(root_node, BAD_CAST "state", BAD_CAST "partial" );
//
//	/* Registration Node */
//	registration_node = xmlNewChild(root_node, NULL, BAD_CAST "registration", NULL);
//	if (registration_node == NULL) {
//		LM_ERR("while adding child\n");
//		goto error;
//	}
//
//	/* Add the properties to this Node for AOR and ID: */
//	xmlNewProp(registration_node, BAD_CAST "aor", BAD_CAST impu->public_identity.s);
//	buf_len = snprintf(buf, sizeof(buf), "%p", impu);
//	xmlNewProp(registration_node, BAD_CAST "id", BAD_CAST buf);
//
//
//	//now the updated contact
//	contact_node =xmlNewChild(registration_node, NULL, BAD_CAST "contact", NULL);
//	if (contact_node == NULL) {
//		LM_ERR("while adding child\n");
//		goto error;
//	}
//	memset(buf, 0, sizeof(buf));
//	buf_len = snprintf(buf, sizeof(buf), "%p", c);
//	xmlNewProp(contact_node, BAD_CAST "id", BAD_CAST buf);
//
//	//TODO: this needs to be dependent on state
//	xmlNewProp(contact_node, BAD_CAST "state", BAD_CAST "terminated");
//	xmlNewProp(contact_node, BAD_CAST "event", BAD_CAST "unregistered");
//	memset(buf, 0, sizeof(buf));
//    //buf_len = snprintf(buf, sizeof(buf), "%i", (int)(ptr->expires-cur_time));
//    //xmlNewProp(contact_node, BAD_CAST "expires", BAD_CAST buf);
//
//	/* URI-Node */
//	memset(buf, 0, sizeof(buf));
//	buf_len = snprintf(buf, sizeof(buf), "%.*s", c->aor.len, c->aor.s);
//	uri_node = xmlNewChild(contact_node, NULL, BAD_CAST "uri", BAD_CAST buf);
//	if (uri_node == NULL) {
//		LM_ERR("while adding child\n");
//		goto error;
//	}
//
//	/* create the body */
//	body = (str*) pkg_malloc(sizeof(str));
//	if (body == NULL) {
//		LM_ERR("while allocating memory\n");
//		return NULL;
//	}
//	memset(body, 0, sizeof(str));
//
//	/* Write the XML into the body */
//	xmlDocDumpFormatMemory(doc, (unsigned char**) (void*) &body->s, &body->len,
//			1);
//
//	/*free the document */
//	xmlFreeDoc(doc);
//	xmlCleanupParser();
//
//	return body;
//
//error:
//	if (body) {
//		if (body->s)
//			xmlFree(body->s);
//		pkg_free(body);
//	}
//	if (doc)
//		xmlFreeDoc(doc);
//	return NULL;
//
//}
//
//int send_partial_publish(ppublic_t *impu, struct pcontact *c, int type)
//{
//	publ_info_t publ;
//    str content_type;
//    int id_buf_len;
//    char id_buf[512];
//
//    content_type.s = "application/reginfo+xml";
//    content_type.len = 23;
//
//	LM_DBG("Sending publish\n");
//	str *body = build_reginfo_partial(impu, c, type);
//
//	if (body == NULL || body->s == NULL) {
//		LM_ERR("Error on creating XML-Body for publish\n");
//		goto error;
//	}
//	LM_DBG("XML-Body:\n%.*s\n", body->len, body->s);
//
//	memset(&publ, 0, sizeof(publ_info_t));
//	publ.pres_uri = &impu->public_identity;
//	publ.body = body;
//	id_buf_len = snprintf(id_buf, sizeof(id_buf), "IMSPCSCF_PUBLISH.%.*s", c->aor.len, c->aor.s);
//	publ.id.s = id_buf;
//	publ.id.len = id_buf_len;
//	publ.content_type = content_type;
//	publ.expires = 3600;
//
//	/* make UPDATE_TYPE, as if this "publish dialog" is not found
//	 by pua it will fallback to INSERT_TYPE anyway */
//	publ.flag |= UPDATE_TYPE;
//	publ.source_flag |= REGINFO_PUBLISH;
//	publ.event |= REGINFO_EVENT;
//	publ.extra_headers = NULL;
//
//	if (pua.send_publish(&publ) < 0) {
//		LM_ERR("Error while sending publish\n");
//	}
//
//	return 1;
//
//error:
//	if (body) {
//		if (body->s)
//			xmlFree(body->s);
//		pkg_free(body);
//	}
//	return -1;
//}

void callback_pcscf_contact_cb(struct pcontact *c, int type, void *param) {
	ppublic_t *ptr;

	LM_DBG("----------------------!\n");
	LM_DBG("PCSCF Contact Callback in regsitrar!\n");
	LM_DBG("Contact AOR: [%.*s]\n", c->aor.len, c->aor.s);
	LM_DBG("Callback type [%d]\n", type);

	if (type == (PCSCF_CONTACT_DELETE | PCSCF_CONTACT_UPDATE)) {
		//send publish for each associated IMPU
		ptr = c->head;
			while (ptr) {
				if (c->reg_state == PCONTACT_DEREG_PENDING_PUBLISH) {
					LM_DBG("delete/update on contact <%.*s> associated with IMPU <%.*s> (sending publish)\n",
											c->aor.len, c->aor.s,
											ptr->public_identity.len, ptr->public_identity.s);
					//send_partial_publish(ptr, c, type);
				}

				ptr = ptr->next;
			}

	}
}


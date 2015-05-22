/* Kamailio PURPLE MODULE
 * 
 * Copyright (C) 2008 Atos Worldline
 * Contact: Eric PTAK <eric.ptak@atosorigin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>

#include "../../dprint.h"
#include "../../str.h"
#include "../../cfg/cfg_struct.h"
#include "../../modules/tm/tm_load.h"
#include "../pua/pua_bind.h"
#include "../pua/pidf.h"

#include "purple.h"
#include "purple_sip.h"

extern struct tm_binds tmb;
extern send_publish_t pua_send_publish;

/* Relay a MESSAGE to a SIP client */
int purple_send_sip_msg(char *to, char *from, char *msg) {
	LM_DBG("sending message from %s to %s\n", from, to);
	str msg_type = { "MESSAGE", 7 };
	str ruri, hdr, fromstr, tostr, msgstr;
	char hdr_buf[512], ruri_buf[512];
	uac_req_t uac_r;
	
	/* update the local config framework structures */
	cfg_update();

	ruri.s = ruri_buf;
	ruri.len = snprintf(ruri_buf, sizeof(ruri_buf), "%s;proto=purple", to);
	
	hdr.s = hdr_buf;
	hdr.len = snprintf(hdr_buf, sizeof(hdr_buf), "Content-type: text/plain" CRLF "Contact: %s" CRLF, from);

	fromstr.s = from;
	fromstr.len = strlen(from);
	tostr.s = to;
	tostr.len = strlen(to);
	msgstr.s = msg;
	msgstr.len = strlen(msg);

	set_uac_req(&uac_r, &msg_type, &hdr, &msgstr, 0, 0, 0, 0);
	if (tmb.t_request(&uac_r, &ruri, &tostr, &fromstr, 0) < 0) {
		LM_ERR("error sending request\n");
		return -1;
	}
	LM_DBG("message sent successfully\n");
	return 0;
}

static str* build_pidf(char *uri, char *id, enum purple_publish_basic basic, enum purple_publish_activity activity, const char *note) {
	LM_DBG("build pidf : %s, %d, %d, %s\n", uri, basic, activity, note);
	str* body = NULL;
	xmlDocPtr doc = NULL;
	xmlNodePtr root_node = NULL, status_node = NULL;
	xmlNodePtr tuple_node = NULL, basic_node = NULL;
	xmlNodePtr person_node = NULL, activities_node = NULL;
	xmlNsPtr pidf_ns, dm_ns, rpid_ns;
	char* entity = NULL;

	entity = (char*)pkg_malloc(7+ strlen(uri)*sizeof(char));
	if(entity == NULL) {	
		LM_ERR("no more memory\n");
		goto error;
	}
	strcpy(entity, "pres:");
	memcpy(entity+5, uri+4, strlen(uri)-4);
	entity[1+ strlen(uri)] = '\0';
	LM_DBG("building pidf for entity: %s\n", entity);

	doc = xmlNewDoc(BAD_CAST "1.0");
	if(doc == NULL) {
		LM_ERR("allocating new xml doc\n");
		goto error;
	}

	root_node = xmlNewNode(NULL, BAD_CAST "presence");
	if(root_node == 0) {
		LM_ERR("extracting presence node\n");
		goto error;
	}
	xmlDocSetRootElement(doc, root_node);
	xmlNewProp(root_node, BAD_CAST "entity", BAD_CAST entity);

	pidf_ns = xmlNewNs(root_node, BAD_CAST "urn:ietf:params:xml:ns:pidf", NULL);
	dm_ns = xmlNewNs(root_node, BAD_CAST "urn:ietf:params:xml:ns:pidf:data-model", BAD_CAST "dm");
	rpid_ns = xmlNewNs(root_node, BAD_CAST "urn:ietf:params:xml:ns:pidf:rpid", BAD_CAST "rpid");
	xmlNewNs(root_node, BAD_CAST "urn:ietf:params:xml:ns:pidf:cipid", BAD_CAST "c");
	
	tuple_node = xmlNewChild(root_node, NULL, BAD_CAST "tuple", NULL);
	if( tuple_node == NULL) {
		LM_ERR("while adding child\n");
		goto error;
	}
	xmlNewProp(tuple_node, BAD_CAST "id", BAD_CAST id);
	
	status_node = xmlNewChild(tuple_node, pidf_ns, BAD_CAST "status", NULL);
	if( status_node ==NULL) {
		LM_ERR("while adding child\n");
		goto error;
	}

	switch (basic) {
		case PURPLE_BASIC_OPEN:
			basic_node = xmlNewChild(status_node, pidf_ns, BAD_CAST "basic", BAD_CAST "open");
			if(basic_node == NULL) {
				LM_ERR("while adding child\n");
				goto error;
			}
			break;

		case PURPLE_BASIC_CLOSED:	
			basic_node = xmlNewChild(status_node, pidf_ns, BAD_CAST "basic", BAD_CAST "closed") ;
			if(basic_node == NULL) {
				LM_ERR("while adding child\n");
				goto error;
			}
			break;
		default:
			break;
	}
		
	person_node = xmlNewChild(root_node, dm_ns, BAD_CAST "person", NULL);
	if (person_node == NULL) {
		LM_ERR("while adding child\n");
		goto error;
	}
	xmlNewProp(person_node, BAD_CAST "id", BAD_CAST id);

	activities_node = xmlNewChild(person_node, rpid_ns, BAD_CAST "activities", NULL);
	if (activities_node == NULL) {
		LM_ERR("while adding child\n");
		goto error;
	}
	
	switch (activity) {
		case PURPLE_ACTIVITY_AVAILABLE:
			xmlNewChild(activities_node, rpid_ns, BAD_CAST "unknown", NULL);
			xmlNewChild(person_node, dm_ns, BAD_CAST "note", (note != NULL) ? BAD_CAST note : NULL);
			break;
		case PURPLE_ACTIVITY_BUSY:
			xmlNewChild(activities_node, rpid_ns, BAD_CAST "busy", NULL);
			xmlNewChild(activities_node, rpid_ns, BAD_CAST "unknown", NULL);
			if (note == NULL)
				xmlNewChild(person_node, dm_ns, BAD_CAST "note", BAD_CAST "Busy");
			break;
		case PURPLE_ACTIVITY_AWAY:
			xmlNewChild(activities_node, rpid_ns, BAD_CAST "away", NULL);
			xmlNewChild(activities_node, rpid_ns, BAD_CAST "unknown", NULL);
			if (note == NULL)
				xmlNewChild(person_node, dm_ns, BAD_CAST "note", BAD_CAST "Away");
			break;
		default:
			break;
	}	

	if (note != NULL)
		xmlNewChild(person_node, dm_ns, BAD_CAST "note", BAD_CAST note);
		
		
	body = (str*)pkg_malloc(sizeof(str));
	if(body == NULL) {
		LM_ERR("no more memory\n");
		goto error;
	}
	xmlDocDumpFormatMemory(doc, (xmlChar**)(void*)&body->s, &body->len, 1);
		
	if(entity)
		pkg_free(entity);
	xmlFreeDoc(doc);
	
	return body;

error:
	if(entity)
		pkg_free(entity);
	if(body) {
		if(body->s) {
			xmlFree(body->s);
		}
		pkg_free(body);
	}
	if(doc) {
		xmlFreeDoc(doc);
	}

	return NULL;
}	


/* Relay a PUBLISH */
int purple_send_sip_publish(char *from, char *tupleid, enum purple_publish_basic basic, enum purple_publish_activity primitive, const char *note) {

	LM_DBG("publishing presence for <%s> with tuple [%s]\n", from, tupleid);
	
	char pres_buff[512];
	publ_info_t publ;
	str pres_uri;

	/* update the local config framework structures */
	cfg_update();

	memset(&publ, 0, sizeof(publ_info_t));
	
	pres_uri.s = pres_buff;
	pres_uri.len = sprintf(pres_buff, "%s;proto=purple", from);

	publ.pres_uri = &pres_uri;
	publ.source_flag = PURPLE_PUBLISH;
	publ.event = PRESENCE_EVENT;

	str *body = NULL;
	if (basic == PURPLE_BASIC_OPEN) {
		body = build_pidf(from, tupleid, basic, primitive, note);
		publ.expires = 3600; 
	}
	else {
		publ.body = NULL;
		publ.expires = 0;
	}

	publ.body = body;	

	if(pua_send_publish(&publ) < 0) {
		LM_ERR("error while sending publish\n");
		return -1;
	}
	LM_DBG("publish sent successfully for <%s>\n", from);
	return 0;
}



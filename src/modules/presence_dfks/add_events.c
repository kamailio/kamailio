/*
 * add "as-feature" event to presence module - mariusbucur
 *
 * Copyright (C) 2014 Maja Stanislawska <maja.stanislawska@yahoo.com>
 * Copyright (C) 2024 Victor Seva <vseva@sipwise.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include "../../core/parser/parse_content.h"
#include "../../core/parser/parse_expires.h"
#include "../../core/parser/parse_event.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/parser/parse_to.h"
#include "../presence/event_list.h"
#include "../pua/pua.h"
#include "presence_dfks.h"
#include "add_events.h"

static str pu_415_rpl = str_init("Unsupported media type");
static str unk_dev = str_init("<notKnown/>");
static str content_type = str_init("application/x-as-feature-event+xml");
// -4
static str dnd_xml = str_init(
		"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n<DoNotDisturbEvent "
		"xmlns=\"http://www.ecma-international.org/standards/ecma-323/csta/"
		"ed3\">\n<device>%s</device>\n<doNotDisturbOn>%s</doNotDisturbOn>\n</"
		"DoNotDisturbEvent>\n\r\n");
// -8
static str fwd_xml = str_init(
		"<?xml version='1.0' encoding='ISO-8859-1'?><ForwardingEvent "
		"xmlns=\"http://www.ecma-international.org/standards/ecma-323/csta/"
		"ed3\">\n<device>%s</device>\n<forwardingType>%s</"
		"forwardingType>\n<forwardStatus>%s</forwardStatus>\n<forwardTo>%s</"
		"forwardTo>\n</ForwardingEvent>\n\r\n");

int dfks_add_events(void)
{
	pres_ev_t event;

	/* constructing message-summary event */
	memset(&event, 0, sizeof(pres_ev_t));
	event.name.s = "as-feature-event";
	event.name.len = 16;

	event.content_type.s = "application/x-as-feature-event+xml";
	event.content_type.len = 34;
	event.default_expires = 3600;
	event.type = PUBL_TYPE;
	event.req_auth = 0;
	event.evs_publ_handl = dfks_publ_handler;
	event.evs_subs_handl = dfks_subs_handler;

	if(pres_add_event(&event) < 0) {
		LM_ERR("failed to add event \"as-feature-event\"\n");
		return -1;
	}
	return 0;
}

int dfks_publ_handler(struct sip_msg *msg)
{
	str body = {0, 0};
	xmlDocPtr doc = NULL;

	LM_DBG("dfks_publ_handl start\n");
	if(get_content_length(msg) == 0)
		return 1;

	body.s = get_body(msg);
	if(body.s == NULL) {
		LM_ERR("cannot extract body from msg\n");
		goto error;
	}

	/* content-length (if present) must be already parsed */
	body.len = get_content_length(msg);
	doc = xmlParseMemory(body.s, body.len);
	if(doc == NULL) {
		LM_ERR("bad body format\n");
		if(slb.freply(msg, 415, &pu_415_rpl) < 0) {
			LM_ERR("while sending '415 Unsupported media type' reply\n");
		}
		goto error;
	}
	xmlFreeDoc(doc);
	xmlCleanupParser();
	return 1;

error:
	xmlFreeDoc(doc);
	xmlCleanupParser();
	return -1;
}

int dfks_subs_handler(struct sip_msg *msg)
{
	str body = {0, 0};
	xmlDocPtr doc = NULL;
	xmlNodePtr top_elem = NULL;
	xmlNodePtr param = NULL;
	char *dndact = NULL, *fwdact = NULL, *fwdtype = NULL, *fwdDN = NULL,
		 *device = NULL;
	publ_info_t publ;
	str pres_uri;
	char id_buf[512];
	int id_buf_len;
	struct to_body *pto = NULL, TO = {0};


	LM_DBG("dfks_subs_handl start\n");
	if(msg->to == NULL || msg->to->body.s == NULL) {
		LM_ERR("cannot parse TO header\n");
		goto error;
	}
	/* examine the to header */
	if(msg->to->parsed != NULL) {
		pto = (struct to_body *)msg->to->parsed;
		LM_DBG("'To' header ALREADY PARSED: <%.*s>\n", pto->uri.len,
				pto->uri.s);
	} else {
		parse_to(msg->to->body.s, msg->to->body.s + msg->to->body.len + 1, &TO);
		if(TO.uri.len <= 0) {
			LM_ERR("'To' header NOT parsed\n");
			goto error;
		}
		pto = &TO;
	}
	if(pto->uri.s && pto->uri.len) {
		pres_uri.s = pto->uri.s;
		pres_uri.len = pto->uri.len;
	} else {
		pres_uri.s = msg->first_line.u.request.uri.s;
		pres_uri.len = msg->first_line.u.request.uri.len;
	}
	/* content-length (if present) must be already parsed */
	body.len = get_content_length(msg);
	if(body.len == 0) {
		LM_DBG("no body. (ok for initial)\n");
		return 1;
	}
	body.s = get_body(msg);
	if(body.s == NULL) {
		LM_ERR("cannot extract body from msg\n");
		goto error;
	}

	doc = xmlParseMemory(body.s, body.len);
	if(doc == NULL) {
		LM_ERR("bad body format\n");
		if(slb.freply(msg, 415, &pu_415_rpl) < 0) {
			LM_ERR("while sending '415 Unsupported media type' reply\n");
		}
		goto error;
	}

	top_elem = libxml_api.xmlDocGetNodeByName(doc, "SetDoNotDisturb", NULL);
	if(top_elem != NULL) {
		LM_DBG(" got SetDoNotDisturb\n");
		param = libxml_api.xmlNodeGetNodeByName(
				top_elem, "doNotDisturbOn", NULL);
		if(param != NULL) {
			dndact = (char *)xmlNodeGetContent(param);
		}
		if(dndact == NULL) {
			LM_ERR("while extracting value from 'doNotDisturbOn' in "
				   "'SetDoNotDisturb'\n");
			goto error;
		}
		LM_DBG("got 'doNotDisturbOn'=%s in 'SetDoNotDisturb'\n", dndact);

		param = libxml_api.xmlNodeGetNodeByName(top_elem, "device", NULL);
		if(param != NULL) {
			device = (char *)xmlNodeGetContent(param);
			if(device == NULL) {
				LM_ERR("while extracting value from 'device' in "
					   "'SetDoNotDisturb'\n");
				goto error;
			}
			if(strlen(device) == 0)
				device = unk_dev.s;
			LM_DBG("got 'device'=%s in 'SetDoNotDisturb'\n", device);
		} else {
			device = unk_dev.s;
		}
		body.len = dnd_xml.len - 4 + strlen(dndact) + strlen(device);
		body.s = pkg_malloc(body.len + 1);
		if(body.s == NULL) {
			LM_ERR("while extracting allocating body for publish in "
				   "'SetDoNotDisturb'\n");
			goto error;
		}
		sprintf(body.s, dnd_xml.s, device, dndact);
		LM_DBG("body for dnd publish is %d, %s\n", body.len, body.s);

		memset(&publ, 0, sizeof(publ_info_t));
		publ.pres_uri = &pres_uri;
		publ.body = &body;
		id_buf_len = snprintf(id_buf, sizeof(id_buf), "dfks_PUBLISH.%.*s",
				pres_uri.len, pres_uri.s);
		LM_DBG("ID = %.*s\n", id_buf_len, id_buf);
		publ.id.s = id_buf;
		publ.id.len = id_buf_len;
		publ.content_type = content_type;
		publ.expires = 3600;

		/* make UPDATE_TYPE, as if this "publish dialog" is not found.
		   by pua it will fallback to INSERT_TYPE anyway */
		publ.flag |= INSERT_TYPE;
		publ.source_flag |= DFKS_PUBLISH;
		publ.event |= DFKS_EVENT;
		publ.extra_headers = NULL;

		if(pua.send_publish(&publ) < 0) {
			LM_ERR("error while sending publish\n");
			pkg_free(body.s);
			goto error;
		}
		pkg_free(body.s);
	}

	top_elem = libxml_api.xmlDocGetNodeByName(doc, "SetForwarding", NULL);
	if(top_elem != NULL) {
		LM_DBG(" got SetForwarding\n");
		param = libxml_api.xmlNodeGetNodeByName(top_elem, "forwardDN", NULL);
		if(param != NULL) {
			fwdDN = (char *)xmlNodeGetContent(param);
		}
		if(fwdDN == NULL) {
			LM_ERR("while extracting value from 'forwardDN' in "
				   "'SetForwarding'\n");
			goto error;
		}
		LM_DBG("got 'forwardDN'=%s in 'SetForwarding'\n", fwdDN);

		param = libxml_api.xmlNodeGetNodeByName(
				top_elem, "forwardingType", NULL);
		if(param != NULL) {
			fwdtype = (char *)xmlNodeGetContent(param);
		}
		if(fwdtype == NULL) {
			LM_ERR("while extracting value from 'forwardingType' in "
				   "'SetForwarding'\n");
			goto error;
		}
		LM_DBG("got 'forwardingType'=%s in 'SetForwarding'\n", fwdtype);

		param = libxml_api.xmlNodeGetNodeByName(
				top_elem, "activateForward", NULL);
		if(param != NULL) {
			fwdact = (char *)xmlNodeGetContent(param);
		}
		if(fwdact == NULL) {
			LM_ERR("while extracting value from 'activateForward' in "
				   "'SetForwarding'\n");
			goto error;
		}
		LM_DBG("got 'activateForward'=%s in 'SetForwarding'\n", fwdact);

		param = libxml_api.xmlNodeGetNodeByName(top_elem, "device", NULL);
		if(param != NULL) {
			device = (char *)xmlNodeGetContent(param);
			if(device == NULL) {
				LM_ERR("while extracting value from 'device' in "
					   "'SetForwarding'\n");
				goto error;
			}
			LM_DBG("got 'device'=%s in 'SetDoNotDisturb'\n", device);
		} else {
			device = unk_dev.s;
		}
		body.len = fwd_xml.len - 8 + strlen(device) + strlen(fwdtype)
				   + strlen(fwdact) + strlen(fwdDN);
		body.s = pkg_malloc(body.len + 1);
		if(body.s == NULL) {
			LM_ERR("while extracting allocating body for publish in "
				   "'SetForwarding'\n");
			goto error;
		}
		sprintf(body.s, fwd_xml.s, device, fwdtype, fwdact, fwdDN);
		LM_DBG("body for dnd publish is %d %s\n", body.len, body.s);
		memset(&publ, 0, sizeof(publ_info_t));
		publ.pres_uri = &pres_uri;
		publ.body = &body;
		id_buf_len = snprintf(id_buf, sizeof(id_buf), "DFKS_PUBLISH.%.*s",
				pres_uri.len, pres_uri.s);
		LM_DBG("ID = %.*s\n", id_buf_len, id_buf);
		publ.id.s = id_buf;
		publ.id.len = id_buf_len;
		publ.content_type = content_type;
		publ.expires = 3600;

		/* make UPDATE_TYPE, as if this "publish dialog" is not found.
		   by pua it will fallback to INSERT_TYPE anyway */
		publ.flag |= INSERT_TYPE;
		publ.source_flag |= DFKS_PUBLISH;
		publ.event |= DFKS_EVENT;
		publ.extra_headers = NULL;

		if(pua.send_publish(&publ) < 0) {
			LM_ERR("error while sending publish\n");
			pkg_free(body.s);
			goto error;
		}
		pkg_free(body.s);
	}

	xmlFreeDoc(doc);
	xmlCleanupParser();
	return 1;

error:
	xmlFreeDoc(doc);
	xmlCleanupParser();
	return -1;
}

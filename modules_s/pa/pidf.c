/*
 * Presence Agent, PIDF document support
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "../../dprint.h"
#include "paerrno.h"
#include "common.h"
#include "pidf.h"

#define CRLF "\r\n"
#define CRLF_L (sizeof(CRLF) - 1)

#define PUBLIC_ID "//IETF//DTD RFCxxxx PIDF 1.0//EN"
#define PUBLIC_ID_L (sizeof(PUBLIC_ID) - 1)

#define MIME_TYPE "application/cpim-pidf+xml"
#define MIME_TYPE_L (sizeof(MIME_TYPE) - 1)

#define XML_VERSION "<?xml version=\"1.0\"?>"
#define XML_VERSION_L (sizeof(XML_VERSION) - 1)

#define ADDRESS_ETAG "</contact>"
#define ADDRESS_ETAG_L (sizeof(ADDRESS_ETAG) - 1)

#define TUPLE_ETAG "</tuple>"
#define TUPLE_ETAG_L (sizeof(TUPLE_ETAG) - 1)

#define PIDF_DTD "pidf.dtd"
#define PIDF_DTD_L (sizeof(XPDIF_DTD) - 1)

#define DOCTYPE "<!DOCTYPE presence PUBLIC \"" PUBLIC_ID "\" \"" PIDF_DTD "\">"
#define DOCTYPE_L (sizeof(DOCTYPE) - 1)

#define PRESENCE_START "<presence entity=\"sip:"
#define PRESENCE_START_L (sizeof(PRESENCE_START) - 1)

#define PRESENCE_END "\">"
#define PRESENCE_END_L (sizeof(PRESENCE_END) - 1)

#define PRESENCE_ETAG "</presence>"
#define PRESENCE_ETAG_L (sizeof(PRESENCE_ETAG) - 1)

#define TUPLE_STAG "<tuple id=\"9r28r49\">"
#define TUPLE_STAG_L (sizeof(TUPLE_STAG) - 1)

#define ADDRESS_STAG "  <contact>"
#define ADDRESS_STAG_L (sizeof(ADDRESS_STAG) - 1)

#define STATUS_STAG "  <status>"
#define STATUS_STAG_L (sizeof(STATUS_STAG) - 1)

#define STATUS_ETAG "  </status>"
#define STATUS_ETAG_L (sizeof(STATUS_ETAG) - 1)

#define BASIC_OPEN "    <basic>open</basic>\r\n"
#define BASIC_OPEN_L (sizeof(BASIC_OPEN) - 1)

#define BASIC_CLOSED "    <basic>closed</basic>\r\n"
#define BASIC_CLOSED_L (sizeof(BASIC_CLOSED) - 1)

#define BASIC_INUSE "    <basic>inuse</basic>\r\n"
#define BASIC_INUSE_L (sizeof(BASIC_INUSE) - 1)

#define LOCATION_STAG "    <geopriv><location-info><civilAddress>"
#define LOCATION_STAG_L (sizeof(LOCATION_STAG) - 1)

#define LOCATION_ETAG "    </civilAddress></location-info></geopriv>"
#define LOCATION_ETAG_L (sizeof(LOCATION_ETAG) - 1)

#define LOC_STAG "      <loc>"
#define LOC_STAG_L (sizeof(LOC_STAG) - 1)

#define LOC_ETAG "</loc>"
#define LOC_ETAG_L (sizeof(LOC_ETAG) - 1)

#define SITE_STAG "      <site>"
#define SITE_STAG_L (sizeof(SITE_STAG) - 1)

#define SITE_ETAG "</site>"
#define SITE_ETAG_L (sizeof(SITE_ETAG) - 1)

#define FLOOR_STAG "      <floor>"
#define FLOOR_STAG_L (sizeof(FLOOR_STAG) - 1)

#define FLOOR_ETAG "</floor>"
#define FLOOR_ETAG_L (sizeof(FLOOR_ETAG) - 1)

#define ROOM_STAG "      <room>"
#define ROOM_STAG_L (sizeof(ROOM_STAG) - 1)

#define ROOM_ETAG "</room>"
#define ROOM_ETAG_L (sizeof(ROOM_ETAG) - 1)

#define X_STAG "      <x>"
#define X_STAG_L (sizeof(X_STAG) - 1)

#define X_ETAG "</x>"
#define X_ETAG_L (sizeof(X_ETAG) - 1)

#define Y_STAG "      <y>"
#define Y_STAG_L (sizeof(Y_STAG) - 1)

#define Y_ETAG "</y>"
#define Y_ETAG_L (sizeof(Y_ETAG) - 1)

#define RADIUS_STAG "      <radius>"
#define RADIUS_STAG_L (sizeof(RADIUS_STAG) - 1)

#define RADIUS_ETAG "</radius>"
#define RADIUS_ETAG_L (sizeof(RADIUS_ETAG) - 1)



/*
 * Create start of pidf document
 */
int start_pidf_doc(str* _b, int _l)
{
	if ((XML_VERSION_L + 
	     CRLF_L +
	     DOCTYPE_L + 
	     CRLF_L
	    ) > _l) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "start_pidf_doc(): Buffer too small\n");
		return -1;
	}

	str_append(_b, XML_VERSION CRLF DOCTYPE CRLF,
		   XML_VERSION_L + CRLF_L + DOCTYPE_L + CRLF_L);
	return 0;
}


/*
 * Add a presentity information
 */
int pidf_add_presentity(str* _b, int _l, str* _uri)
{
	if (_l < PRESENCE_START_L + _uri->len + PRESENCE_END_L + CRLF_L) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "pidf_add_presentity(): Buffer too small\n");
		return -1;
	}

	str_append(_b, PRESENCE_START, PRESENCE_START_L);
	str_append(_b, _uri->s, _uri->len);
	str_append(_b, PRESENCE_END CRLF, 
		   PRESENCE_END_L + CRLF_L);
	return 0;
}


/*
 * Create start of pidf tuple
 */
int start_pidf_tuple(str* _b, int _l)
{
	if ((TUPLE_STAG_L + 
	     CRLF_L
	    ) > _l) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "start_pidf_tuple(): Buffer too small\n");
		return -1;
	}

	str_append(_b, TUPLE_STAG CRLF,
		   TUPLE_STAG_L + CRLF_L);
	return 0;
}

/*
 * Add a contact address with given status
 */
int pidf_add_address(str* _b, int _l, str* _addr, pidf_status_t _st)
{
	int len = 0;
	char* basic;

	switch(_st) {
	case PIDF_ST_OPEN:   basic = BASIC_OPEN;   len = BASIC_OPEN_L;   break;
	case PIDF_ST_CLOSED: basic = BASIC_CLOSED; len = BASIC_CLOSED_L; break;
	case PIDF_ST_INUSE:  basic = BASIC_INUSE;  len = BASIC_INUSE_L;  break;
	default:              basic = BASIC_CLOSED; len = BASIC_CLOSED_L; break; /* Makes gcc happy */
	}

	str_append(_b, ADDRESS_STAG, ADDRESS_STAG_L);
	str_append(_b, _addr->s, _addr->len);
	str_append(_b, ADDRESS_ETAG CRLF , 
		   ADDRESS_ETAG_L + CRLF_L);
	str_append(_b, STATUS_STAG CRLF, STATUS_STAG_L + CRLF_L);
	str_append(_b, basic, len);
	return 0;
}

/*
 * Add location information
 */
int pidf_add_location(str* _b, int _l, str *_loc, str *_site, str *_floor, str *_room, double _x, double _y, double _radius)
{
	str_append(_b, LOCATION_STAG, LOCATION_STAG_L);

	if (_loc->len) {
		str_append(_b, LOC_STAG, LOC_STAG_L);
		str_append(_b, _loc->s, _loc->len);
		str_append(_b, LOC_ETAG CRLF, LOC_ETAG_L + CRLF_L);
	}
	if (_site->len) {
		str_append(_b, SITE_STAG, SITE_STAG_L);
		str_append(_b, _site->s, _site->len);
		str_append(_b, SITE_ETAG CRLF, SITE_ETAG_L + CRLF_L);
	}
	if (_floor->len) {
		str_append(_b, FLOOR_STAG, FLOOR_STAG_L);
		str_append(_b, _floor->s, _floor->len);
		str_append(_b, FLOOR_ETAG CRLF, FLOOR_ETAG_L + CRLF_L);
	}
	if (_room->len) {
		str_append(_b, ROOM_STAG, ROOM_STAG_L);
		str_append(_b, _room->s, _room->len);
		str_append(_b, ROOM_ETAG CRLF, ROOM_ETAG_L + CRLF_L);
	}

	if (_x) {
		char buf[128];
		int len = sprintf(buf, "%g", _x);
		str_append(_b, X_STAG, X_STAG_L);
		str_append(_b, buf, len);
		str_append(_b, X_ETAG CRLF, X_ETAG_L + CRLF_L);
	}
	if (_y) {
		char buf[128];
		int len = sprintf(buf, "%g", _y);
		str_append(_b, Y_STAG, Y_STAG_L);
		str_append(_b, buf, len);
		str_append(_b, Y_ETAG CRLF, Y_ETAG_L + CRLF_L);
	}
	if (_radius) {
		char buf[128];
		int len = sprintf(buf, "%g", _radius);
		str_append(_b, RADIUS_STAG, RADIUS_STAG_L);
		str_append(_b, buf, len);
		str_append(_b, RADIUS_ETAG CRLF, RADIUS_ETAG_L + CRLF_L);
	}

	str_append(_b, LOCATION_ETAG CRLF, CRLF_L + LOCATION_ETAG_L);
	str_append(_b, STATUS_ETAG CRLF, STATUS_ETAG_L + CRLF_L);
	return 0;
}

/*
 * Create start of pidf tuple
 */
int end_pidf_tuple(str* _b, int _l)
{
	if ((TUPLE_ETAG_L + 
	     CRLF_L
	    ) > _l) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "end_pidf_tuple(): Buffer too small\n");
		return -1;
	}

	str_append(_b, TUPLE_ETAG CRLF,
		   TUPLE_ETAG_L + CRLF_L);
	return 0;
}

/*
 * End the document
 */
int end_pidf_doc(str* _b, int _l)
{
	if (_l < (PRESENCE_ETAG_L + CRLF_L)) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "end_pidf_doc(): Buffer too small\n");
		return -1;
	}

	str_append(_b, PRESENCE_ETAG CRLF, PRESENCE_ETAG_L + CRLF_L);
	return 0;
}



xmlDocPtr event_body_parse(char *event_body)
{
	return xmlParseMemory(event_body, strlen(event_body));
}

/*
 * apply procedure f to each xmlNodePtr in doc matched by xpath
 */
void xpath_map(xmlDocPtr doc, char *xpath, void (*f)(xmlNodePtr, void *), void *data)
{
	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;
	xmlNodeSetPtr nodeset;
	int i;

	context = xmlXPathNewContext(doc);
	result = xmlXPathEvalExpression(xpath, context);
	if(!result || xmlXPathNodeSetIsEmpty(result->nodesetval)){
		fprintf(stderr, "xpath_map: no result for xpath=%s\n", xpath);
		return;
	}
	nodeset = result->nodesetval;
	for (i=0; i < nodeset->nodeNr; i++) {
		xmlNodePtr node = nodeset->nodeTab[i];
		printf("name[%d]: %s\n", i, node->name);
		f(node, data);
	}
	xmlXPathFreeContext(context);
}

xmlNodePtr xpath_get_node(xmlDocPtr doc, char *xpath)
{
	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;
	xmlNodeSetPtr nodeset;
	xmlNodePtr node;

	context = xmlXPathNewContext(doc);
	result = xmlXPathEvalExpression(xpath, context);
	if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
		fprintf(stderr, "xpath_get_node: no result for xpath=%s\n", xpath);
		return NULL;
	}
	nodeset = result->nodesetval;
	node = nodeset->nodeTab[0];
	xmlXPathFreeContext(context);
	return node;
}

xmlAttrPtr xmlNodeGetAttrByName(xmlNodePtr node, const char *name)
{
	xmlAttrPtr attr = node->properties;
	while (attr) {
		if (xmlStrcasecmp(attr->name, name) == 0)
			return attr;
		attr = attr->next;
	}
	return NULL;
}

char *xmlNodeGetAttrContentByName(xmlNodePtr node, const char *name)
{
     xmlAttrPtr attr = xmlNodeGetAttrByName(node, name);
     if (attr)
	  return xmlNodeGetContent(attr->children);
     else
	  return NULL;
}

xmlNodePtr xmlNodeGetChildByName(xmlNodePtr node, const char *name)
{
	xmlNodePtr cur = node->children;
	while (cur) {
		if (xmlStrcasecmp(cur->name, name) == 0)
			return cur;
		cur = cur->next;
	}
	return NULL;
}

xmlNodePtr xmlNodeGetNodeByName(xmlNodePtr node, const char *name, const char *ns)
{
	xmlNodePtr cur = node;
	while (cur) {
		xmlNodePtr match = NULL;
		if (xmlStrcasecmp(cur->name, name) == 0) {
			if (!ns || (cur->ns && xmlStrcasecmp(cur->ns->prefix, ns) == 0))
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

char *xmlDocGetNodeContentByName(xmlDocPtr doc, const char *name, const char *ns)
{
	xmlNodePtr node = xmlDocGetNodeByName(doc, name, ns);
	if (node)
		return xmlNodeGetContent(node->children);
	else
		return NULL;
}


void xmlNodeMapByName(xmlNodePtr node, const char *name, const char *ns, 
		      void (f)(xmlNodePtr, void*), void *data)
{
	xmlNodePtr cur = node;
	if (!f)
		return;
	while (cur) {
		if (xmlStrcasecmp(cur->name, name) == 0) {
			if (!ns || (cur->ns && xmlStrcasecmp(cur->ns->prefix, ns) == 0))
				f(cur, data);
		}
		/* visit children */
		xmlNodeMapByName(cur->children, name, ns, f, data);

		cur = cur->next;
	}
}

void xmlDocMapByName(xmlDocPtr doc, const char *name, const char *ns,
			   void (f)(xmlNodePtr, void*), void *data )
{
	xmlNodePtr cur = doc->children;
	xmlNodeMapByName(cur, name, ns, f, data);
}

int parse_pidf(char *pidf_body, str *contact_str, str *basic_str, str *status_str, 
	       str *location_str, str *site_str, str *floor_str, str *room_str,
	       double *xp, double *yp, double *radiusp,
	       str *packet_loss_str)
{
     int flags = 0;
     xmlDocPtr doc = NULL;
     xmlNodePtr presenceNode = NULL;
     char *presence = NULL;
     char *sipuri = NULL;
     char *contact = NULL;
     char *basic = NULL;
     char *status = NULL;
     char *location = NULL;
     char *site = NULL;
     char *floor = NULL;
     char *room = NULL;
     char *x = NULL;
     char *y = NULL;
     char *radius = NULL;
     char *packet_loss = NULL;

     doc = event_body_parse(pidf_body);

     presenceNode = xmlDocGetNodeByName(doc, "presence", NULL);
     presence = xmlDocGetNodeContentByName(doc, "presence", NULL);
     contact = xmlDocGetNodeContentByName(doc, "contact", NULL);
     basic = xmlDocGetNodeContentByName(doc, "basic", NULL);
     status = xmlDocGetNodeContentByName(doc, "status", NULL);
     location = xmlDocGetNodeContentByName(doc, "loc", NULL);
     site = xmlDocGetNodeContentByName(doc, "site", NULL);
     floor = xmlDocGetNodeContentByName(doc, "floor", NULL);
     room = xmlDocGetNodeContentByName(doc, "room", NULL);
     x = xmlDocGetNodeContentByName(doc, "x", NULL);
     y = xmlDocGetNodeContentByName(doc, "y", NULL);
     radius = xmlDocGetNodeContentByName(doc, "radius", NULL);
     packet_loss = xmlDocGetNodeContentByName(doc, "packet-loss", NULL);
		
     if (presenceNode)
	  sipuri = xmlNodeGetAttrContentByName(presenceNode, "entity");

     LOG(L_INFO, "parse_pidf: sipuri=%p:%s contact=%p:%s basic=%p:%s location=%p:%s\n",
	 sipuri, sipuri, contact, contact, basic, basic, location, location);
     LOG(L_INFO, "parse_pidf: site=%p:%s floor=%p:%s room=%p:%s\n",
	 site, site, floor, floor, room, room);
     LOG(L_INFO, "parse_pidf: x=%p:%s y=%p:%s radius=%p:%s\n",
	 x, x, y, y, radius, radius);
     if (packet_loss)
	  LOG(L_INFO, "packet_loss=%p:%s\n", packet_loss, packet_loss);

     if (contact_str && contact) {
	  contact_str->len = strlen(contact);
	  contact_str->s = strdup(contact);
	  flags |= PARSE_PIDF_CONTACT;
     }
     if (basic_str && basic) {
	  basic_str->len = strlen(basic);
	  basic_str->s = strdup(basic);
	  flags |= PARSE_PIDF_BASIC;
     }
     if (status_str && status) {
	  status_str->len = strlen(status);
	  status_str->s = strdup(status);
	  flags |= PARSE_PIDF_STATUS;
     }
     if (location_str && location) {
	  location_str->len = strlen(location);
	  location_str->s = strdup(location);
	  flags |= PARSE_PIDF_LOC;
     }
     if (site_str && site) {
	  site_str->len = strlen(site);
	  site_str->s = strdup(site);
	  flags |= PARSE_PIDF_SITE;
     }
     if (floor_str && floor) {
	  floor_str->len = strlen(floor);
	  floor_str->s = strdup(floor);
	  flags |= PARSE_PIDF_FLOOR;
     }
     if (room_str && room) {
	  room_str->len = strlen(room);
	  room_str->s = strdup(room);
	  flags |= PARSE_PIDF_ROOM;
     }
     if (xp && x) {
	  *xp = strtod(x, NULL);
	  flags |= PARSE_PIDF_X;
     }
     if (yp && y) {
	  *yp = strtod(y, NULL);
	  flags |= PARSE_PIDF_Y;
     }
     if (radiusp && radius) {
	  *radiusp = strtod(radius, NULL);
	  flags |= PARSE_PIDF_RADIUS;
     }
     if (packet_loss_str && packet_loss) {
	  packet_loss_str->len = strlen(packet_loss);
	  packet_loss_str->s = strdup(packet_loss);
	  flags |= PARSE_PIDF_PACKET_LOSS;
     }
     return flags;
}

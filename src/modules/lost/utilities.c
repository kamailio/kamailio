/*
 * lost module utility functions
 *
 * Copyright (C) 2019 Wolfgang Kampichler
 * DEC112, FREQUENTIS AG
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

/*!
 * \file
 * \brief Kamailio lost :: utilities
 * \ingroup lost
 * Module: \ref lost
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_content.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_ppi_pai.h"
#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/rand/kam_rand.h"

#include "pidf.h"
#include "utilities.h"

/*
 * lost_trim_content(dest, lgth)
 * removes whitespace that my occur in a content of an xml element
 */
char *lost_trim_content(char *str, int *lgth)
{
	char *end;

	while(isspace(*str))
		str++;

	if(*str == 0)
		return NULL;

	end = str + strlen(str) - 1;

	while(end > str && isspace(*end))
		end--;

	*(end + 1) = '\0';

	*lgth = (end + 1) - str;

	return str;
}

/*
 * lost_rand_str(dest, length)
 * creates a random string used as temporary id in a findService request
 */
void lost_rand_str(char *dest, size_t lgth)
{
	size_t index;
	char charset[] = "0123456789"
					 "abcdefghijklmnopqrstuvwxyz"
					 "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	while(lgth-- > 0) {
		index = (double)kam_rand() / RAND_MAX * (sizeof charset - 1);
		*dest++ = charset[index];
	}
	*dest = '\0';
}

/*
 * lost_free_loc(ptr)
 * frees a location object
 */
void lost_free_loc(p_loc_t ptr)
{
	pkg_free(ptr->identity);
	pkg_free(ptr->urn);
	pkg_free(ptr->longitude);
	pkg_free(ptr->latitude);
	pkg_free(ptr);
}

/*
 * lost_free_string(ptr)
 * frees and resets a string
 */
void lost_free_string(str *string)
{
	str ptr = *string;

	if(ptr.s) {
		pkg_free(ptr.s);
		ptr.s = NULL;
		ptr.len = 0;
	}
}

/*
 * lost_new_loc(urn)
 * creates a new location object in private memory and returns a pointer
 */
p_loc_t lost_new_loc(str rurn)
{
	s_loc_t *ptr = NULL;;
	char *id = NULL;
	char *urn = NULL;

	ptr = (s_loc_t *)pkg_malloc(sizeof(s_loc_t));
	if(ptr == NULL) {
		goto err;
	}

	id = (char *)pkg_malloc(RANDSTRSIZE * sizeof(char) + 1);
	if(id == NULL) {
		pkg_free(ptr);
		goto err;
	}

	urn = (char *)pkg_malloc(rurn.len + 1);
	if(urn == NULL) {
		pkg_free(ptr);
		pkg_free(id);
		goto err;
	}

	memset(urn, 0, rurn.len + 1);
	memcpy(urn, rurn.s, rurn.len);
	urn[rurn.len] = '\0';

	lost_rand_str(id, RANDSTRSIZE);

	ptr->identity = id;
	ptr->urn = urn;
	ptr->longitude = NULL;
	ptr->latitude = NULL;
	ptr->radius = 0;
	ptr->recursive = 0;

	return ptr;

err:	
	LM_ERR("no more private memory\n");
	return NULL;
}

/*
 * lost_get_content(node, name, lgth)
 * gets a nodes "name" content and returns string allocated in private memory
 */
char *lost_get_content(xmlNodePtr node, const char *name, int *lgth)
{
	xmlNodePtr cur = node;
	char *content;
	char *cnt = NULL;
	int len;

	*lgth = 0;
	content = xmlNodeGetNodeContentByName(cur, name, NULL);
	if (content == NULL) {
		LM_ERR("could not get XML node content\n");
		return cnt;
	} else {
		len = strlen(content);
		cnt = (char *)pkg_malloc((len + 1) * sizeof(char));
		if(cnt == NULL) {
			LM_ERR("no more private memory\n");
			xmlFree(content);
			return cnt;
		}
		memset(cnt, 0, len + 1);
		memcpy(cnt, content, len);
		cnt[len] = '\0';
	}

	xmlFree(content);
	*lgth = strlen(cnt);

	return cnt;
}

/*
 * lost_get_property(node, name, lgth)
 * gets a nodes property "name" and returns string allocated in private memory
 */
char *lost_get_property(xmlNodePtr node, const char *name, int *lgth)
{
	xmlNodePtr cur = node;
	char *content;
	char *cnt = NULL;
	int len;

	*lgth = 0;
	content = xmlNodeGetAttrContentByName(cur, name);
	if (content == NULL) {
		LM_ERR("could not get XML node content\n");
		return cnt;
	} else {
		len = strlen(content);
		cnt = (char *)pkg_malloc((len + 1) * sizeof(char));
		if(cnt == NULL) {
			LM_ERR("no more private memory\n");
			xmlFree(content);
			return cnt;
		}
		memset(cnt, 0, len + 1);
		memcpy(cnt, content, len);
		cnt[len] = '\0';
	}

	xmlFree(content);
	*lgth = strlen(cnt);

	return cnt;
}

/*
 * lost_get_childname(name, lgth)
 * gets a nodes child name and returns string allocated in private memory
 */
char *lost_get_childname(xmlNodePtr node, const char *name, int *lgth)
{
	xmlNodePtr cur = node;
	xmlNodePtr parent = NULL;
	xmlNodePtr child = NULL;
	char *cnt = NULL;
	int len;

	*lgth = 0;
	parent = xmlNodeGetNodeByName(cur, name, NULL);
	child = parent->children;

	if(child) {
		len = strlen((char *)child->name);
		cnt = (char *)pkg_malloc((len + 1) * sizeof(char));
		if(cnt == NULL) {
			LM_ERR("no more private memory\n");
			return cnt;
		}

		memset(cnt, 0, len + 1);
		memcpy(cnt, child->name, len);
		cnt[len] = '\0';

		*lgth = strlen(cnt);
	}
	return cnt;
}

/*
 * lost_get_geolocation_header(msg, lgth)
 * gets the Geolocation header value and returns string allocated in
 * private memory
 */
char *lost_get_geolocation_header(struct sip_msg *msg, int *lgth)
{
	struct hdr_field *hf;
	char *res = NULL;

	*lgth = 0;

	if(parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse geolocation header\n");
		return res;
	}

	for(hf = msg->headers; hf; hf = hf->next) {
		if((hf->type == HDR_OTHER_T)
				&& (hf->name.len == LOST_GEOLOC_HEADER_SIZE - 2)) {
			/* possible hit */
			if(strncasecmp(
					   hf->name.s, LOST_GEOLOC_HEADER, LOST_GEOLOC_HEADER_SIZE)
					== 0) {

				res = (char *)pkg_malloc((hf->body.len + 1) * sizeof(char));
				if(res == NULL) {
					LM_ERR("no more private memory\n");
					return res;
				} else {
					memset(res, 0, hf->body.len + 1);
					memcpy(res, hf->body.s, hf->body.len + 1);
					res[hf->body.len] = '\0';

					*lgth = strlen(res);
				}
			} else {
				LM_ERR("header '%.*s' length %d\n", hf->body.len, hf->body.s,
						hf->body.len);
			}
			break;
		}
	}
	return res;
}

/*
 * lost_get_pai_header(msg, lgth)
 * gets the P-A-I header value and returns string allocated in
 * private memory
 */
char *lost_get_pai_header(struct sip_msg *msg, int *lgth)
{
	struct hdr_field *hf;
	to_body_t *pai_body;
	char *res = NULL;

	*lgth = 0;

	if (parse_headers(msg, HDR_PAI_F, 0) == -1) {
		LM_ERR("could not parse P-A-I header\n");
		return res;
	}

	for(hf = msg->headers; hf; hf = hf->next) {
		if((hf->type == HDR_PAI_T)
				&& (hf->name.len == LOST_PAI_HEADER_SIZE - 2)) {
			/* possible hit */
			if(strncasecmp(hf->name.s, LOST_PAI_HEADER, LOST_PAI_HEADER_SIZE)
					== 0) {

				LM_DBG("P-A-I body:  [%.*s]\n", hf->body.len, hf->body.s);

				/* first, get some memory */
				pai_body = pkg_malloc(sizeof(to_body_t));
				if (pai_body == NULL) {
					LM_ERR("no more private memory\n");
					return res;
				}
				/* parse P-A-I body */
				memset(pai_body, 0, sizeof(to_body_t));
				parse_to(hf->body.s, hf->body.s + hf->body.len + 1, pai_body);
				if (pai_body->error == PARSE_ERROR) {
					LM_ERR("bad P-A-I header\n");
					pkg_free(pai_body);
					return res;
				}
				if (pai_body->error == PARSE_OK) {
					res = (char *)pkg_malloc((pai_body->uri.len + 1) * sizeof(char));
					if(res == NULL) {
						LM_ERR("no more private memory\n");
						pkg_free(pai_body);
						return res;
					} else {
						memset(res, 0, pai_body->uri.len + 1);
						memcpy(res, pai_body->uri.s, pai_body->uri.len + 1);
						res[pai_body->uri.len] = '\0';
						pkg_free(pai_body);

						*lgth = strlen(res);
					}
				}
			} else {
				LM_ERR("header '%.*s' length %d\n", hf->body.len, hf->body.s,
						hf->body.len);
			}
			break;
		}
	}
	return res;
}

/*
 * lost_get_from_header(msg, lgth)
 * gets the From header value and returns string allocated in
 * private memory
 */
char *lost_get_from_header(struct sip_msg *msg, int *lgth)
{
	to_body_t *f_body;
	char *res = NULL;

	*lgth = 0;

	if(parse_headers(msg, HDR_FROM_F, 0) == -1) {
		LM_ERR("failed to parse From header\n");
		return res;
	}

	if(msg->from == NULL || get_from(msg) == NULL) {
		LM_ERR("From header not found\n");
		return res;
	}
	f_body = get_from(msg);

	LM_DBG("From body:  [%.*s]\n", f_body->body.len, f_body->body.s);

	res = (char *)pkg_malloc((f_body->uri.len + 1) * sizeof(char));
	if(res == NULL) {
		LM_ERR("no more private memory\n");
		return res;
	} else {
		memset(res, 0, f_body->uri.len + 1);
		memcpy(res, f_body->uri.s, f_body->uri.len + 1);
		res[f_body->uri.len] = '\0';

		*lgth = strlen(res);
	}
	return res;
}

/*
 * lost_parse_location_info(node, loc)
 * parses locationResponse and writes results to location object
 */
int lost_parse_location_info(xmlNodePtr node, p_loc_t loc)
{
	char bufLat[BUFSIZE];
	char bufLon[BUFSIZE];
	int iRadius;
	char *content = NULL;
	int ret = -1;

	xmlNodePtr cur = node;

	content = xmlNodeGetNodeContentByName(cur, "pos", NULL);
	if(content) {
		sscanf(content, "%s %s", bufLat, bufLon);

		loc->latitude = (char *)pkg_malloc(strlen((char *)bufLat) + 1);
		snprintf(loc->latitude, strlen((char *)bufLat) + 1, "%s",
				(char *)bufLat);

		loc->longitude = (char *)pkg_malloc(strlen((char *)bufLon) + 1);
		snprintf(loc->longitude, strlen((char *)bufLon) + 1, "%s",
				(char *)bufLon);

		loc->radius = 0;
		ret = 0;
	}

	content = xmlNodeGetNodeContentByName(cur, "radius", NULL);
	if(content) {
		iRadius = 0;

		sscanf(content, "%d", &iRadius);
		loc->radius = iRadius;
		ret = 0;
	}

	if(ret < 0) {
		LM_ERR("could not parse location information\n");
	}
	return ret;
}

/*
 * lost_held_location_request(id, lgth)
 * assembles and returns locationRequest string (allocated in private memory)
 */
char *lost_held_location_request(char *id, int *lgth)
{
	int buffersize = 0;

	char buf[BUFSIZE];
	char *doc = NULL;

	xmlChar *xmlbuff = NULL;
	xmlDocPtr request = NULL;

	xmlNodePtr ptrLocationRequest = NULL;
	xmlNodePtr ptrLocationType = NULL;
	xmlNodePtr ptrDevice = NULL;

	xmlKeepBlanksDefault(1);
	*lgth = 0;

	/*
https://tools.ietf.org/html/rfc6155

<?xml version="1.0" encoding="UTF-8"?>
<locationRequest xmlns="urn:ietf:params:xml:ns:geopriv:held" responseTime="8">
    <locationType exact="true">geodetic locationURI</locationType>
    <device xmlns="urn:ietf:params:xml:ns:geopriv:held:id">
        <uri>sip:user@example.net</uri>
    </device>
</locationRequest>
*/

	/* create request */
	request = xmlNewDoc(BAD_CAST "1.0");
	if(!request) {
		LM_ERR("locationRequest xmlNewDoc() failed\n");
		return doc;
	}
	/* locationRequest - element */
	ptrLocationRequest = xmlNewNode(NULL, BAD_CAST "locationRequest");
	if(!ptrLocationRequest) {
		LM_ERR("locationRequest xmlNewNode() failed\n");
		xmlFreeDoc(request);
		return doc;
	}
	xmlDocSetRootElement(request, ptrLocationRequest);
	/* properties */
	xmlNewProp(ptrLocationRequest, BAD_CAST "xmlns",
			BAD_CAST "urn:ietf:params:xml:ns:geopriv:held");
	xmlNewProp(ptrLocationRequest, BAD_CAST "responseTime", BAD_CAST "8");
	/* locationType - element */
	ptrLocationType = xmlNewChild(ptrLocationRequest, NULL,
			BAD_CAST "locationType", BAD_CAST "geodetic locationURI");
	/* properties */
	xmlNewProp(ptrLocationType, BAD_CAST "exact", BAD_CAST "false");
	/* device - element */
	ptrDevice = xmlNewChild(ptrLocationRequest, NULL, BAD_CAST "device", NULL);
	if(!ptrDevice) {
		LM_ERR("locationRequest xmlNewChild() failed\n");
		xmlFreeDoc(request);
		return doc;
	}
	/* properties */
	xmlNewProp(ptrDevice, BAD_CAST "xmlns",
			BAD_CAST "urn:ietf:params:xml:ns:geopriv:held:id");
	/* uri - element */
	snprintf(buf, BUFSIZE, "%s", id);
	xmlNewChild(ptrDevice, NULL, BAD_CAST "uri", BAD_CAST buf);

	xmlDocDumpFormatMemory(request, &xmlbuff, &buffersize, 0);
	if(!xmlbuff) {
		LM_ERR("locationRequest xmlDocDumpFormatMemory() failed\n");
		xmlFreeDoc(request);
		return doc;
	}

	doc = (char *)pkg_malloc((buffersize + 1) * sizeof(char));
	if(doc == NULL) {
		LM_ERR("no more private memory\n");
		xmlFree(xmlbuff);
		xmlFreeDoc(request);
		return doc;
	}

	memset(doc, 0, buffersize + 1);
	memcpy(doc, (char *)xmlbuff, buffersize);
	doc[buffersize] = '\0';

	*lgth = strlen(doc);

	xmlFree(xmlbuff);
	xmlFreeDoc(request);

	return doc;
}

/*
 * lost_find_service_request(loc, lgth)
 * assembles and returns findService request string (allocated in private memory)
 */
char *lost_find_service_request(p_loc_t loc, int *lgth)
{
	int buffersize = 0;

	char buf[BUFSIZE];
	char *doc = NULL;

	xmlChar *xmlbuff = NULL;
	xmlDocPtr request = NULL;

	xmlNodePtr ptrFindService = NULL;
	xmlNodePtr ptrLocation = NULL;
	xmlNodePtr ptrPoint = NULL;
	xmlNodePtr ptrCircle = NULL;
	xmlNodePtr ptrRadius = NULL;

	xmlKeepBlanksDefault(1);
	*lgth = 0;

	/*
https://tools.ietf.org/html/rfc5222

<?xml version="1.0" encoding="UTF-8"?>
<findService
 xmlns="urn:ietf:params:xml:ns:lost1"
 xmlns:p2="http://www.opengis.net/gml"
 serviceBoundary="value"
 recursive="true">
    <location id="6020688f1ce1896d" profile="geodetic-2d">
        <p2:Point id="point1" srsName="urn:ogc:def:crs:EPSG::4326">
            <p2:pos>37.775 -122.422</p2:pos>
        </p2:Point>
    </location>
    <service>urn:service:sos.police</service>
</findService>
 */
	/* create request */
	request = xmlNewDoc(BAD_CAST "1.0");
	if(!request) {
		LM_ERR("findService request xmlNewDoc() failed\n");
		return doc;
	}
	/* findService - element */
	ptrFindService = xmlNewNode(NULL, BAD_CAST "findService");
	if(!ptrFindService) {
		LM_ERR("findService xmlNewNode() failed\n");
		xmlFreeDoc(request);
		return doc;
	}
	xmlDocSetRootElement(request, ptrFindService);
	/* set properties */
	xmlNewProp(ptrFindService, BAD_CAST "xmlns",
			BAD_CAST "urn:ietf:params:xml:ns:lost1");
	xmlNewProp(ptrFindService, BAD_CAST "xmlns:p2",
			BAD_CAST "http://www.opengis.net/gml");
	xmlNewProp(
			ptrFindService, BAD_CAST "serviceBoundary", BAD_CAST "reference");
	xmlNewProp(ptrFindService, BAD_CAST "recursive", BAD_CAST "true");
	/* location - element */
	ptrLocation = xmlNewChild(ptrFindService, NULL, BAD_CAST "location", NULL);
	xmlNewProp(ptrLocation, BAD_CAST "id", BAD_CAST loc->identity);
	xmlNewProp(ptrLocation, BAD_CAST "profile", BAD_CAST "geodetic-2d");
	/* set pos */
	snprintf(buf, BUFSIZE, "%s %s", loc->latitude, loc->longitude);
	/* Point */
	if(loc->radius == 0) {
		ptrPoint = xmlNewChild(ptrLocation, NULL, BAD_CAST "Point", NULL);
		if(!ptrPoint) {
			LM_ERR("locationRequest xmlNewChild() failed\n");
			xmlFreeDoc(request);
			return doc;
		}
		xmlNewProp(ptrPoint, BAD_CAST "xmlns",
				BAD_CAST "http://www.opengis.net/gml");
		xmlNewProp(ptrPoint, BAD_CAST "srsName",
				BAD_CAST "urn:ogc:def:crs:EPSG::4326");
		/* pos */
		xmlNewChild(ptrPoint, NULL, BAD_CAST "pos", BAD_CAST buf);
	} else {
		/* circle - Point */
		ptrCircle = xmlNewChild(ptrLocation, NULL, BAD_CAST "gs:Circle", NULL);
		if(!ptrCircle) {
			LM_ERR("locationRequest xmlNewChild() failed\n");
			xmlFreeDoc(request);
			return doc;
		}
		xmlNewProp(ptrCircle, BAD_CAST "xmlns:gml",
				BAD_CAST "http://www.opengis.net/gml");
		xmlNewProp(ptrCircle, BAD_CAST "xmlns:gs",
				BAD_CAST "http://www.opengis.net/pidflo/1.0");
		xmlNewProp(ptrCircle, BAD_CAST "srsName",
				BAD_CAST "urn:ogc:def:crs:EPSG::4326");
		/* pos */
		xmlNewChild(ptrCircle, NULL, BAD_CAST "gml:pos", BAD_CAST buf);
		/* circle - radius */
		snprintf(buf, BUFSIZE, "%d", loc->radius);
		ptrRadius = xmlNewChild(
				ptrCircle, NULL, BAD_CAST "gs:radius", BAD_CAST buf);
		if(!ptrRadius) {
			LM_ERR("locationRequest xmlNewChild() failed\n");
			xmlFreeDoc(request);
			return doc;
		}
		xmlNewProp(ptrRadius, BAD_CAST "uom",
				BAD_CAST "urn:ogc:def:uom:EPSG::9001");
	}
	/* service - element */
	snprintf(buf, BUFSIZE, "%s", loc->urn);
	xmlNewChild(ptrFindService, NULL, BAD_CAST "service", BAD_CAST buf);

	xmlDocDumpFormatMemory(request, &xmlbuff, &buffersize, 0);
	if(!xmlbuff) {
		LM_ERR("findService request xmlDocDumpFormatMemory() failed\n");
		xmlFreeDoc(request);
		return doc;
	}

	doc = (char *)pkg_malloc((buffersize + 1) * sizeof(char));
	if(doc == NULL) {
		LM_ERR("no more private memory\n");
		xmlFree(xmlbuff);
		xmlFreeDoc(request);
		return doc;
	}

	memset(doc, 0, buffersize + 1);
	memcpy(doc, (char *)xmlbuff, buffersize);
	doc[buffersize] = '\0';

	*lgth = strlen(doc);

	xmlFree(xmlbuff);
	xmlFreeDoc(request);

	return doc;
}

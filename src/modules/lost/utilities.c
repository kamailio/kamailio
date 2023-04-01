/*
 * lost module utility functions
 *
 * Copyright (C) 2021 Wolfgang Kampichler
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
#include <arpa/inet.h>

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
#include "response.h"

extern int lost_recursion;
extern int lost_profile;

/*
 * lost_trim_content(dest, lgth)
 * removes whitespace that my occur in a content of an xml element
 */
char *lost_trim_content(char *str, int *lgth)
{
	char *end;

	*lgth = 0;

	if(str == NULL)
		return NULL;

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
 * lost_new_loc(urn)
 * creates a new location object in private memory and returns a pointer
 */
p_lost_loc_t lost_new_loc(str rurn)
{
	s_lost_loc_t *ptr = NULL;

	char *id = NULL;
	char *urn = NULL;

	ptr = (s_lost_loc_t *)pkg_malloc(sizeof(s_lost_loc_t));
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
		pkg_free(id);
		pkg_free(ptr);
		goto err;
	}

	memset(urn, 0, rurn.len);
	memcpy(urn, rurn.s, rurn.len);
	urn[rurn.len] = '\0';

	lost_rand_str(id, RANDSTRSIZE);

	ptr->identity = id;
	ptr->urn = urn;
	ptr->longitude = NULL;
	ptr->latitude = NULL;
	ptr->geodetic = NULL;
	ptr->xpath = NULL;
	ptr->profile = NULL;
	ptr->radius = 0;
	ptr->recursive = lost_recursion;	 /* set recursion to param */
	ptr->boundary = LOST_BOUNDARY_FALSE; /* set boundary to reference */

	return ptr;

err:
	PKG_MEM_ERROR;
	return NULL;
}

/*
 * lost_new_held(uri, type, time, exact)
 * creates a new held object in private memory and returns a pointer
 */
p_lost_held_t lost_new_held(str s_uri, str s_type, int time, int exact)
{
	s_lost_held_t *ptr = NULL;

	char *uri = NULL;
	char *type = NULL;

	ptr = (s_lost_held_t *)pkg_malloc(sizeof(s_lost_held_t));
	if(ptr == NULL) {
		goto err;
	}

	uri = (char *)pkg_malloc(s_uri.len + 1);
	if(uri == NULL) {
		pkg_free(ptr);
		goto err;
	}

	type = (char *)pkg_malloc(s_type.len + 1);
	if(type == NULL) {
		pkg_free(uri);
		pkg_free(ptr);
		goto err;
	}

	memset(uri, 0, s_uri.len);
	memcpy(uri, s_uri.s, s_uri.len);
	uri[s_uri.len] = '\0';

	memset(type, 0, s_type.len);
	memcpy(type, s_type.s, s_type.len);
	type[s_type.len] = '\0';

	ptr->identity = uri;
	ptr->type = type;
	ptr->time = time;
	ptr->exact = exact;

	return ptr;

err:
	PKG_MEM_ERROR;
	return NULL;
}

/*
 * lost_free_loc(ptr)
 * frees a location object
 */
void lost_free_loc(p_lost_loc_t *loc)
{
	p_lost_loc_t ptr;

	if(*loc == NULL)
		return;

	ptr = *loc;

	if(ptr->identity)
		pkg_free(ptr->identity);
	if(ptr->urn)
		pkg_free(ptr->urn);
	if(ptr->xpath)
		pkg_free(ptr->xpath);
	if(ptr->geodetic)
		pkg_free(ptr->geodetic);
	if(ptr->longitude)
		pkg_free(ptr->longitude);
	if(ptr->latitude)
		pkg_free(ptr->latitude);
	if(ptr->profile)
		pkg_free(ptr->profile);

	pkg_free(ptr);
	*loc = NULL;

	LM_DBG("### location object removed\n");

	return;
}

/*
 * lost_free_loc(ptr)
 * frees a held location request object
 */
void lost_free_held(p_lost_held_t *held)
{
	p_lost_held_t ptr;

	if(*held == NULL)
		return;

	ptr = *held;

	if(ptr->identity)
		pkg_free(ptr->identity);
	if(ptr->type)
		pkg_free(ptr->type);

	pkg_free(ptr);
	*held = NULL;

	LM_DBG("### location-request object removed\n");

	return;
}

/*
 * lost_copy_string(str, int*) {
 * copies a string and returns a zero terminated string allocated
 * in private memory
 */
char *lost_copy_string(str src, int *lgth)
{
	char *res = NULL;
	*lgth = 0;

	/* only copy a valid string */
	if(src.s != NULL && src.len > 0) {
		res = (char *)pkg_malloc((src.len + 1) * sizeof(char));
		if(res == NULL) {
			PKG_MEM_ERROR;
		} else {
			memset(res, 0, src.len);
			memcpy(res, src.s, src.len);
			res[src.len] = '\0';
			*lgth = (int)strlen(res);
		}
	}

	return res;
}

/*
 * lost_free_string(ptr)
 * frees and resets a string
 */
void lost_free_string(str *string)
{
	str ptr = STR_NULL;

	if(string->s == NULL)
		return;

	ptr = *string;

	if(ptr.s != NULL && ptr.len > 0) {
		pkg_free(ptr.s);

		LM_DBG("### string object removed\n");
	
	}

	string->s = NULL;
	string->len = 0;

	return;
}

/*
 * lost_get_content(node, name, lgth)
 * gets a nodes "name" content, removes whitespace and returns string
 * allocated in private memory
 */
char *lost_get_content(xmlNodePtr node, const char *name, int *lgth)
{
	xmlNodePtr cur = node;
	char *content = NULL;
	char *trimmed = NULL;
	char *cnt = NULL;
	int len;

	*lgth = 0;

	content = xmlNodeGetNodeContentByName(cur, name, NULL);
	if(content == NULL) {
		LM_ERR("could not get XML node content\n");
		return cnt;
	} else {
		trimmed = lost_trim_content(content, &len);
		cnt = (char *)pkg_malloc((len + 1) * sizeof(char));
		if(cnt == NULL) {
			PKG_MEM_ERROR;
			xmlFree(content);
			return cnt;
		}
		memset(cnt, 0, len);
		memcpy(cnt, trimmed, len);
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
	if(content == NULL) {
		LM_ERR("could not get XML node content\n");
		return cnt;
	} else {
		len = strlen(content);
		cnt = (char *)pkg_malloc((len + 1) * sizeof(char));
		if(cnt == NULL) {
			PKG_MEM_ERROR;
			xmlFree(content);
			return cnt;
		}
		memset(cnt, 0, len);
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
	char *trimmed = NULL;

	int len;

	*lgth = 0;

	parent = xmlNodeGetNodeByName(cur, name, NULL);
	if(parent == NULL) {
		LM_ERR("xmlNodeGetNodeByName() failed\n");
		return cnt;
	}
	child = parent->children;
	if(child == NULL) {
		LM_ERR("%s has no children '%s'\n", parent->name, name);
		return cnt;
	}
	trimmed = lost_trim_content((char *)child->name, &len);
	cnt = (char *)pkg_malloc((len + 1) * sizeof(char));
	if(cnt == NULL) {
		PKG_MEM_ERROR;
		return cnt;
	}
	memset(cnt, 0, len);
	memcpy(cnt, trimmed, len);
	cnt[len] = '\0';

	*lgth = strlen(cnt);

	return cnt;
}

/*
 * lost_get_geolocation_header(msg, hdr)
 * gets the Geolocation header value and returns 1 on success
 */
p_lost_geolist_t lost_get_geolocation_header(struct sip_msg *msg, int *items)
{
	struct hdr_field *hf;
	str hdr = STR_NULL;
	p_lost_geolist_t list = NULL;

	*items = 0;

	if(parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse SIP headers\n");
		return list;
	}

	for(hf = msg->headers; hf; hf = hf->next) {
		if((hf->type == HDR_OTHER_T)
				&& (hf->name.len == LOST_GEOLOC_HEADER_SIZE - 2)) {
			/* possible hit */
			if(strncasecmp(hf->name.s, LOST_GEOLOC_HEADER,
					LOST_GEOLOC_HEADER_SIZE) == 0) {
                
				hdr.s = hf->body.s;
                hdr.len = hf->body.len;

				LM_DBG("found geolocation header [%.*s]\n", hdr.len, hdr.s);

				*items += lost_new_geoheader_list(&list, hdr);
			}
		}
	}

	return list;
}

/*
 * lost_get_pai_header(msg, lgth)
 * gets the P-A-I header value and returns string allocated in
 * private memory
 */
char *lost_get_pai_header(struct sip_msg *msg, int *lgth)
{
	p_id_body_t *p_body;

	char *tmp = NULL;
	char *res = NULL;
	int len = 0;

	*lgth = 0;

	if(parse_pai_header(msg) == -1) {
		
		LM_DBG("failed to parse P-A-I header\n");
		
		return res;
	}

	if(msg->pai == NULL || get_pai(msg) == NULL) {
		LM_ERR("P-A-I header not found\n");
		return res;
	}
	p_body = get_pai(msg);

	/* warning in case multiple P-A-I headers were found and use first */
	if(p_body->num_ids > 1) {
		LM_WARN("multiple P-A-I headers found, selecting first!\n");
	}

	LM_DBG("P-A-I body: [%.*s]\n", p_body->id->body.len, p_body->id->body.s);

	/* accept any identity string (body), but remove <..> if present */ 
	tmp = p_body->id->body.s;
	len = p_body->id->body.len;
	if(tmp[0] == '<' && tmp[len - 1] == '>') {
		tmp++;
		len -= 2;
	}
	/* allocate memory, copy and \0 terminate string */
	res = (char *)pkg_malloc((len + 1) * sizeof(char));
	if(res == NULL) {
		PKG_MEM_ERROR;
		return res;
	} else {
		memset(res, 0, len);
		memcpy(res, tmp, len);
		res[len] = '\0';
		*lgth = strlen(res);
	}

	return res;
}

/*
 * lost_parse_host(uri, string)
 * parses host (ipv4, ipv6 or name) from uri and returns string
 */
int lost_parse_host(const char *uri, str *host, int *flag)
{
	char *search = (char *)uri;
	char *end;

	int len = 0;
	int ip6 = 0;

	while((len < strlen(uri)) && (*search++ != '@')) {
		len++;
	}

	if(len == strlen(uri)) {
		return 0;
	}

	if(*(search) == 0)
		return 0;

	if(*(search) == '[') {
		ip6 = 1;
	}

	end = search;

	if(ip6) {
		while((len < strlen(uri)) && (*end++ != ']')) {
			len++;
		}
		if(len == strlen(uri)) {
			return 0;
		}
	} else {
		while(len < strlen(uri)) {
			if((*end == ':') || (*end == '>')) {
				break;
			}
			end++;
			len++;
		}
	}

	if(*(search) == 0)
		return 0;

	host->s = search;
	host->len = end - search;

	if(ip6) {
		*flag = AF_INET6;
	} else {
		*flag = AF_INET;
	}

	return 1;
}

/*
 * lost_get_nameinfo(ip, name, flag)
 * translates socket address to service name (flag: IPv4 or IPv6)
 */
int lost_get_nameinfo(char *ip, str *name, int flag)
{
	struct sockaddr_in sa4;
	struct sockaddr_in6 sa6;

	if(flag == AF_INET) {
		bzero(&sa4, sizeof(sa4));
		sa4.sin_family = flag;
		if(inet_pton(flag, ip, &sa4.sin_addr) <= 0)
			return 0;
		if(getnameinfo((struct sockaddr *)&sa4, sizeof(sa4), name->s, name->len,
				   NULL, 0, NI_NAMEREQD))
			return 0;

		return 1;
	}

	if(flag == AF_INET6) {
		bzero(&sa6, sizeof(sa6));
		sa6.sin6_family = flag;
		if(inet_pton(flag, ip, &sa6.sin6_addr) <= 0)
			return 0;
		if(getnameinfo((struct sockaddr *)&sa6, sizeof(sa6), name->s, name->len,
				   NULL, 0, NI_NAMEREQD))
			return 0;

		return 1;
	}

	return 0;
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

	if(parse_from_header(msg) == -1) {
		LM_ERR("failed to parse From header\n");
		return res;
	}

	if(msg->from == NULL || get_from(msg) == NULL) {
		LM_ERR("From header not found\n");
		return res;
	}
	f_body = get_from(msg);

	LM_DBG("From body: [%.*s]\n", f_body->body.len, f_body->body.s);

	/* allocate memory, copy and \0 terminate string */
	res = (char *)pkg_malloc((f_body->uri.len + 1) * sizeof(char));
	if(res == NULL) {
		PKG_MEM_ERROR;
		return res;
	} else {
		memset(res, 0, f_body->uri.len);
		memcpy(res, f_body->uri.s, f_body->uri.len);
		res[f_body->uri.len] = '\0';

		*lgth = strlen(res);
	}

	return res;
}

/*
 * lost_free_geoheader_list(list)
 * removes geoheader list from private memory
 */
void lost_free_geoheader_list(p_lost_geolist_t *list)
{
	p_lost_geolist_t curr;

	while((curr = *list) != NULL) {
		*list = curr->next;
		if(curr->value != NULL) {
			pkg_free(curr->value);
		}
		if(curr->param != NULL) {
			pkg_free(curr->param);
		}
		pkg_free(curr);
	}

	*list = NULL;

	LM_DBG("### geoheader list removed\n");

	return;
}

/*
 * lost_get_geoheader_value(list, type, rtype)
 * returns geoheader value and type (rtype) of given type
 */
char *lost_get_geoheader_value(p_lost_geolist_t list, lost_geotype_t type, int *rtype)
{
	p_lost_geolist_t head = list;
	char *value = NULL;

	if(head == NULL) {
		return value;
	}

	/* type is not important, take first element value and type */
	if((type == ANY) || (type == UNKNOWN)) {
		*rtype = head->type;
		return head->value;
	}

	/* take first element value and type of given type */
	while(head) {
		if(type == head->type) {
			value = head->value;
			*rtype = head->type;
			break;
		}
		head = head->next;
	}

	return value;
}

/*
 * lost_reverse_geoheader_list(list)
 * reverses list order
 */
void lost_reverse_geoheader_list(p_lost_geolist_t *head)
{
	p_lost_geolist_t prev = NULL;
	p_lost_geolist_t next = NULL;
	p_lost_geolist_t current = *head;

	while(current != NULL) {
		next = current->next;
		current->next = prev;
		prev = current;
		current = next;
	}

	*head = prev;
}

/*
 * lost_copy_geoheader_value(src, len)
 * returns a header vaule string (src to src + len) allocated in private memory
 */
char *lost_copy_geoheader_value(char *src, int len)
{
	char *res = NULL;

	res = (char *)pkg_malloc((len + 1) * sizeof(char));
	if(res == NULL) {
		PKG_MEM_ERROR;
		return res;
	} else {
		memset(res, 0, len);
		memcpy(res, src, len);
		res[len] = '\0';
	}

	return res;
}

/*
 * lost_new_geoheader_list(hdr, items)
 * searches and parses Geolocation header and returns a list
 * allocated in private memory and an item count
 */
int lost_new_geoheader_list(p_lost_geolist_t *list, str hdr)
{
	char *search = NULL;
	char *cidptr = NULL;
	char *urlptr = NULL;
	char *ptr = NULL;

	int count = 0;
	int len = 0;
	int i = 0;

	p_lost_geolist_t new = NULL;

	LM_DBG("parsing geolocation header value ...\n");

	/* search the complete header field */
	search = hdr.s;
	for(i = 0; i < hdr.len; i++) {
		/* check for cid content */
		/* <cid:x> might be the shortest */
		if(strlen(search) > 6) {
			if((*(search + 0) == '<')
					&& ((*(search + 1) == 'c') || (*(search + 1) == 'C'))
					&& ((*(search + 2) == 'i') || (*(search + 2) == 'I'))
					&& ((*(search + 3) == 'd') || (*(search + 3) == 'D'))
					&& (*(search + 4) == ':')) {
				cidptr = search + 4;
				*cidptr = LAQUOT;
				ptr = cidptr;
				len = 1;
				while(*(ptr + len) != '>') {
					if((len == strlen(ptr)) || (*(ptr + len) == '<')) {
						LM_WARN("invalid cid: [%.*s]\n", hdr.len, hdr.s);
						break;
					}
					len++;
				}
				if((*(ptr + len) == '>') && (len > 6)) {
					new = (p_lost_geolist_t)pkg_malloc(sizeof(s_lost_geolist_t));
					if(new == NULL) {
						PKG_MEM_ERROR;
					} else {

						LM_DBG("\t[%.*s]\n", len + 1, cidptr);

						new->value = lost_copy_geoheader_value(cidptr, len + 1);
						new->param = NULL;
						new->type = CID;
						new->next = *list;
						*list = new;
						count++;

						LM_DBG("adding cid [%s]\n", new->value);
					}
				} else {
					LM_WARN("invalid value: [%.*s]\n", hdr.len, hdr.s);
				}
				*cidptr = COLON;
			}
		}

		/* check for http(s) content */
		/* <http://goo.gl/> might be the shortest */
		if(strlen(search) > 10) {
			if((*(search + 0) == '<')
					&& ((*(search + 1) == 'h') || (*(search + 1) == 'H'))
					&& ((*(search + 2) == 't') || (*(search + 2) == 'T'))
					&& ((*(search + 3) == 't') || (*(search + 3) == 'T'))
					&& ((*(search + 4) == 'p') || (*(search + 4) == 'P'))) {
				urlptr = search + 1;
				ptr = urlptr;
				len = 0;
				while(*(ptr + len) != '>') {
					if((len == strlen(ptr)) || (*(ptr + len) == '<')) {
						LM_WARN("invalid url: [%.*s]\n", hdr.len, hdr.s);
						break;
					}
					len++;
				}
				if((*(ptr + len) == '>') && (len > 10)) {
					new = (p_lost_geolist_t)pkg_malloc(sizeof(s_lost_geolist_t));
					if(new == NULL) {
						PKG_MEM_ERROR;
					} else {

						LM_DBG("\t[%.*s]\n", len, urlptr);

						new->value = lost_copy_geoheader_value(urlptr, len);
						new->param = NULL;
						if(*(search + 5) == ':') {

							LM_DBG("adding http url [%s]\n", new->value);

							new->type = HTTP;
						} else if(((*(search + 5) == 's')
								|| (*(search + 5) == 'S'))
								&& (*(search + 6) == ':')) {

							LM_DBG("adding https url [%s]\n", new->value);

							new->type = HTTPS;
						}
						new->next = *list;
						*list = new;
						count++;
					}
				} else {
					LM_WARN("invalid value: [%.*s]\n", hdr.len, hdr.s);
				}
			}
		}
		search++;
	}

	return count;
}

/*
 * lost_parse_pidf(pidf, urn)
 * parses pidf and returns a new location object
 */
p_lost_loc_t lost_parse_pidf(str pidf, str urn)
{

	p_lost_loc_t loc = NULL;

	xmlDocPtr doc = NULL;
	xmlNodePtr root = NULL;

	/* read and parse pidf-lo */
	doc = xmlReadMemory(pidf.s, pidf.len, 0, NULL,
			XML_PARSE_NOBLANKS | XML_PARSE_NONET | XML_PARSE_NOCDATA);

	if(doc == NULL) {
		LM_WARN("invalid xml (pidf-lo): [%.*s]\n", pidf.len, pidf.s);
		doc = xmlRecoverMemory(pidf.s, pidf.len);
		if(doc == NULL) {
			LM_ERR("xml (pidf-lo) recovery failed on: [%.*s]\n", pidf.len,
					pidf.s);
			goto err;
		}

		LM_DBG("xml (pidf-lo) recovered\n");
	}

	root = xmlDocGetRootElement(doc);
	if(root == NULL) {
		LM_ERR("empty pidf-lo document\n");
		goto err;
	}
	if((!xmlStrcmp(root->name, (const xmlChar *)"presence"))
			|| (!xmlStrcmp(root->name, (const xmlChar *)"locationResponse"))) {
		/* get the geolocation: point or circle, urn, ... */
		loc = lost_new_loc(urn);
		if(loc == NULL) {
			LM_ERR("location object allocation failed\n");
			goto err;
		}
		if(lost_parse_location_info(root, loc) < 0) {
			LM_ERR("location element not found\n");
			goto err;
		}
	} else {
		LM_ERR("findServiceResponse or presence element not found in "
			   "[%.*s]\n",
				pidf.len, pidf.s);
		goto err;
	}
	/* clean up */
	xmlFreeDoc(doc);
	doc = NULL;

	return loc;

err:
	LM_ERR("pidflo parsing error\n");
	/* clean up */
	lost_free_loc(&loc);
	if(doc != NULL) {
		xmlFreeDoc(doc);
		doc = NULL;
	}

	return NULL;
}

/*
 * lost_parse_geo(node, loc)
 * parses locationResponse (pos|circle) and writes 
 * results to location object
 */
int lost_parse_geo(xmlNodePtr node, p_lost_loc_t loc)
{
	xmlNodePtr cur = NULL;

	char bufLat[BUFSIZE];
	char bufLon[BUFSIZE];
	char *content = NULL;

	char s_profile[] = LOST_PRO_GEO2D;

	int iRadius = 0;
	int len = 0;

	cur = node;
	/* find <pos> element */
	content = xmlNodeGetNodeContentByName(cur, "pos", NULL);

	if(content == NULL) {
		LM_WARN("could not find pos element\n");
		return -1;
	}

	sscanf(content, "%s %s", bufLat, bufLon);
	xmlFree(content);

	len = strlen((char *)bufLat);
	loc->latitude = (char *)pkg_malloc(len + 1);
	if(loc->latitude == NULL)
		goto err;

	snprintf(loc->latitude, len, "%s", (char *)bufLat);

	len = strlen((char *)bufLon);
	loc->longitude = (char *)pkg_malloc(len + 1);
	if(loc->longitude == NULL) {
		pkg_free(loc->latitude);
		goto err;
	}

	snprintf(loc->longitude, len, "%s", (char *)bufLon);

	len = strlen((char *)bufLat) + strlen((char *)bufLon) + 1;
	loc->geodetic = (char *)pkg_malloc(len + 1);
	if(loc->longitude == NULL) {
		pkg_free(loc->latitude);
		pkg_free(loc->longitude);
		goto err;
	}

	snprintf(loc->geodetic, len, "%s %s", (char *)bufLat, (char *)bufLon);

	/* find <radius> element */
	content = xmlNodeGetNodeContentByName(cur, "radius", NULL);
	if(content != NULL) {
		sscanf(content, "%d", &iRadius);
		xmlFree(content);
	}

	/* write results */
	loc->radius = iRadius;
	loc->profile = (char *)pkg_malloc(strlen(s_profile) + 1);
	strcpy(loc->profile, s_profile);

	return 0;

err:
	PKG_MEM_ERROR;
	return -1;
}

/*
 * lost_xpath_location(doc, path, loc)
 * performs xpath expression on locationResponse and writes 
 * results (location-info child element) to location object
 */
int lost_xpath_location(xmlDocPtr doc, char *path, p_lost_loc_t loc)
{
	xmlXPathObjectPtr result = NULL;
	xmlNodeSetPtr nodes = NULL;
	xmlNodePtr root = NULL;
	xmlNodePtr cur = NULL;
	xmlDocPtr new = NULL;
	xmlChar *xpath = NULL;
	xmlChar *xmlbuff = NULL;
	xmlChar *cname = NULL;

	const unsigned char s_point[] = LOST_PNT;
	const unsigned char s_circle[] = LOST_CIR;
	const unsigned char s_civic[] = LOST_CIV;

	char *ptr = NULL;
	char *tmp = NULL;
	char *s_profile = NULL;

	int buffersize = 0;
	int remove = 0;
	int size = 0;
	int len = 0;
	int i = 0;
	int nok = -1;
	int select = -1;
	int selgeo = -1;
	int selciv = -1;

	xpath = (xmlChar *)path;
	/* get location via xpath expression */
	result = xmlGetNodeSet(doc, xpath, BAD_CAST XPATH_NS);

	if(result == NULL) {
		LM_DBG("xmlGetNodeSet() returned no result\n");
		return -1;
	}

	nodes = result->nodesetval;
	if(nodes != NULL) {
		size = nodes->nodeNr;
		for(i = 0; i < size; ++i) {
			if(nodes->nodeTab[i] == NULL) {
				LM_WARN("xpath '%s' failed\n", xpath);
				xmlXPathFreeObject(result);
				return -1;
			}
			if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE) {
				cur = nodes->nodeTab[i];
				/* check if child element is point, circle or civic */
				while(nok < LOST_XPATH_DPTH) {
					if(cur->children == NULL) {
						/* no additional DOM level */
						break;
					} else {
						/* check current DOM level */
						nok++;
						cname = BAD_CAST cur->name;
						if(xmlStrcasecmp(cname, s_point) == 0) {
							s_profile = LOST_PRO_GEO2D;
							selgeo = i;
							break;
						}
						if(xmlStrcasecmp(cname, s_circle) == 0) {
							s_profile = LOST_PRO_GEO2D;
							selgeo = i;
							break;
						}
						if(xmlStrcasecmp(cname, s_civic) == 0) {
							s_profile = LOST_PRO_CIVIC;
							selciv = i;
							break;
						}
						/* nothing found ... try next DOM level */
						cur = cur->children;
					}
				}

				if(nok == 0) {
					LM_DBG("xpath '%s' returned valid element (level %d/%d)\n",
							xpath, nok, LOST_XPATH_DPTH);
				} else if(nok < LOST_XPATH_DPTH) {
					/* malformed pidf-lo but still ok */
					LM_WARN("xpath '%s' returned malformed pidf-lo (level "
							"%d/%d)\n",
							xpath, nok, LOST_XPATH_DPTH);
				} else {
					/* really bad pidf-lo */
					LM_WARN("xpath '%s' failed (level %d/%d)\n", xpath, nok,
							LOST_XPATH_DPTH);
					xmlXPathFreeObject(result);
					return -1;
				}

				if(cur == NULL) {
					LM_ERR("xpath xmlCopyNode() failed\n");
					xmlXPathFreeObject(result);
					return -1;
				}

				root = xmlCopyNode(cur, 1);
				if(root == NULL) {
					LM_ERR("xpath xmlCopyNode() failed\n");
					xmlXPathFreeObject(result);
					return -1;
				}

				new = xmlNewDoc(BAD_CAST "1.0");
				if(new == NULL) {
					LM_ERR("xpath xmlNewDoc() failed\n");
					xmlXPathFreeObject(result);
					return -1;
				}
				xmlDocSetRootElement(new, root);
				xmlDocDumpFormatMemory(new, &xmlbuff, &buffersize, 0);
				if(xmlbuff == NULL) {
					LM_ERR("xpath xmlDocDumpFormatMemory() failed\n");
					xmlFreeDoc(new);
					xmlXPathFreeObject(result);
					return -1;
				}
				/* take the first location-info element */
				if(lost_profile == 0) {

					LM_DBG("first location-info element [%d]\n", i);

					select = 0;
					/* take the last location-info element */
				} else if(lost_profile == 1) {

					LM_DBG("last location-info element [%d]\n", i);

					select = size - 1;
					/* take the first geodetic location-info element */
				} else if(lost_profile == 2) {

					LM_DBG("first geodetic location-info element [%d]\n", i);

					select = selgeo;
					/* take the first civic location-info element */
				} else if(lost_profile == 3) {

					LM_DBG("first civic location-info element [%d]\n", i);

					select = selciv;
				}

				/* nothing found */
				if((select < 0) && (i == size - 1)) {
					LM_ERR("could not find proper location-info element\n");
					xmlFree(xmlbuff); /* clean up */
					xmlFreeDoc(new);
					xmlXPathFreeObject(result);
					return -1;
				}

				if(i == select) {
					/* return the current profile */
					if(s_profile != NULL) {
						loc->profile = (char *)pkg_malloc(strlen(s_profile) + 1);
						if(loc->profile == NULL) {
							xmlFree(xmlbuff); /* clean up */
							xmlFreeDoc(new);
							xmlXPathFreeObject(result);
							goto err;
						}
						memset(loc->profile, 0, strlen(s_profile) + 1);
						memcpy(loc->profile, s_profile, strlen(s_profile));
					} else {
						xmlFree(xmlbuff); /* clean up */
						xmlFreeDoc(new);
						xmlXPathFreeObject(result);
						goto err;
					}
					/* remove xml header from location element */
					remove = strlen("<?xml version='1.0'?>\n");
					buffersize = buffersize - remove;
					ptr = (char *)pkg_malloc((buffersize + 1) * sizeof(char));
					if(ptr == NULL) {
						xmlFree(xmlbuff); /* clean up */
						xmlFreeDoc(new);
						xmlXPathFreeObject(result);
						goto err;
					}

					memset(ptr, 0, buffersize);
					memcpy(ptr, (char *)(xmlbuff + remove), buffersize);
					ptr[buffersize] = '\0';

					/* trim the result */
					tmp = lost_trim_content(ptr, &len);

					/* return the location DOM */
					loc->xpath = (char *)pkg_malloc(len + 1);
					if(loc->xpath == NULL) {
						pkg_free(ptr); /* clean up */
						ptr = NULL;
						xmlFree(xmlbuff);
						xmlFreeDoc(new);
						xmlXPathFreeObject(result);
						goto err;
					}
					memset(loc->xpath, 0, len + 1);
					memcpy(loc->xpath, tmp, len);
					pkg_free(ptr); /* clean up */
					ptr = NULL;
				} else {
					LM_WARN("xpath location-info element(%d) ignored\n", i + 1);
				}
				/* clean up */
				xmlFree(xmlbuff);
				xmlFreeDoc(new);
				/* reset level count */
				nok = -1;
			}
		}
	} else {
		LM_WARN("xpath '%s' failed\n", xpath);
		xmlXPathFreeObject(result);
		return -1;
	}
	xmlXPathFreeObject(result);

	return 0;

err:
	PKG_MEM_ERROR;
	return -1;
}

/*
 * lost_parse_location_info(node, loc)
 * wrapper to call xpath or simple pos|circle parser (last resort)
 */
int lost_parse_location_info(xmlNodePtr root, p_lost_loc_t loc)
{
	if(lost_xpath_location(root->doc, LOST_XPATH_GP, loc) == 0) {
		return 0;
	}

	LM_WARN("xpath expression failed ... trying pos|circle\n");

	if(lost_parse_geo(root, loc) == 0) {
		return 0;
	}

	return -1;
}

/*
 * lost_held_post_request(lgth)
 * assembles and returns locationRequest string (allocated in private memory)
 */
char *lost_held_post_request(int *lgth, long rtime, char *type)
{
	int buffersize = 0;

	char buf[BUFSIZE];
	char *doc = NULL;

	xmlChar *xmlbuff = NULL;
	xmlDocPtr request = NULL;

	xmlNodePtr ptrLocationRequest = NULL;
	xmlNodePtr ptrLocationType = NULL;

	xmlKeepBlanksDefault(1);
	*lgth = 0;

	/*
https://tools.ietf.org/html/rfc6753

<?xml version="1.0" encoding="UTF-8"?>
<locationRequest
	responseTime="emergencyRouting"
	xmlns="urn:ietf:params:xml:ns:geopriv:held">
	<locationType exact="false">
		geodetic civic
	</locationType>
</locationRequest>
*/

	/* create request */
	request = xmlNewDoc(BAD_CAST "1.0");
	if(request == NULL) {
		LM_ERR("locationRequest xmlNewDoc() failed\n");
		return doc;
	}
	/* locationRequest - element */
	ptrLocationRequest = xmlNewNode(NULL, BAD_CAST "locationRequest");
	if(ptrLocationRequest == NULL) {
		LM_ERR("locationRequest xmlNewNode() failed\n");
		xmlFreeDoc(request);
		return doc;
	}

	xmlDocSetRootElement(request, ptrLocationRequest);
	/* properties */
	xmlNewProp(ptrLocationRequest, BAD_CAST "xmlns",
			BAD_CAST "urn:ietf:params:xml:ns:geopriv:held");
	/* responseTime - element (default: emergencyRouting) */
	if(rtime > 0) {
		/* set responseTime "<ms>") */
		snprintf(buf, BUFSIZE, "%ld", rtime);
		xmlNewProp(ptrLocationRequest, BAD_CAST HELD_RT, BAD_CAST buf);
	} else if(rtime == -1) {
		/* set responseTime "emergencyDispatch" */
		xmlNewProp(ptrLocationRequest, BAD_CAST HELD_RT, BAD_CAST HELD_ED);
	} else {
		/* set responseTime "emergencyRouting" */
		xmlNewProp(ptrLocationRequest, BAD_CAST HELD_RT, BAD_CAST HELD_ER);
	}
	/* locationType - element (optional) */
	ptrLocationType =
			xmlNewChild(ptrLocationRequest, NULL, BAD_CAST "locationType",
					type == NULL ? BAD_CAST "geodetic civic" : BAD_CAST type);
	/* properties */
	xmlNewProp(ptrLocationType, BAD_CAST "exact", BAD_CAST "false");

	xmlDocDumpFormatMemory(request, &xmlbuff, &buffersize, 0);
	if(xmlbuff == NULL) {
		LM_ERR("locationRequest xmlDocDumpFormatMemory() failed\n");
		xmlFreeDoc(request);
		return doc;
	}

	doc = (char *)pkg_malloc((buffersize + 1) * sizeof(char));
	if(doc == NULL) {
		PKG_MEM_ERROR;
		xmlFree(xmlbuff);
		xmlFreeDoc(request);
		return doc;
	}

	memset(doc, 0, buffersize);
	memcpy(doc, (char *)xmlbuff, buffersize);
	doc[buffersize] = '\0';

	*lgth = strlen(doc);

	xmlFree(xmlbuff);
	xmlFreeDoc(request);

	return doc;
}

/*
 * lost_held_location_request(held, lgth)
 * assembles and returns locationRequest string (allocated in private memory)
 */
char *lost_held_location_request(p_lost_held_t held, int *lgth)
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
https://tools.ietf.org/html/rfc5985

<?xml version="1.0" encoding="UTF-8"?>
<locationRequest xmlns="urn:ietf:params:xml:ns:geopriv:held"
	responseTime="emergencyRouting">
    <locationType exact="true">geodetic locationURI</locationType>
    <device xmlns="urn:ietf:params:xml:ns:geopriv:held:id">
        <uri>sip:user@example.net</uri>
    </device>
</locationRequest>
*/

	/* create request */
	request = xmlNewDoc(BAD_CAST "1.0");
	if(request == NULL) {
		LM_ERR("locationRequest xmlNewDoc() failed\n");
		return doc;
	}
	/* locationRequest - element */
	ptrLocationRequest = xmlNewNode(NULL, BAD_CAST HELD_LR);
	if(ptrLocationRequest == NULL) {
		LM_ERR("locationRequest xmlNewNode() failed\n");
		xmlFreeDoc(request);
		return doc;
	}
	xmlDocSetRootElement(request, ptrLocationRequest);
	/* properties */
	xmlNewProp(ptrLocationRequest, BAD_CAST "xmlns",
			BAD_CAST "urn:ietf:params:xml:ns:geopriv:held");
	/* responseTime - element (default: emergencyRouting) */
	if(held->time > 0) {
		/* set responseTime "<ms>") */
		snprintf(buf, BUFSIZE, "%d", held->time);
		xmlNewProp(ptrLocationRequest, BAD_CAST HELD_RT, BAD_CAST buf);
	} else if(held->time == -1) {
		/* set responseTime "emergencyDispatch" */
		xmlNewProp(ptrLocationRequest, BAD_CAST HELD_RT, BAD_CAST HELD_ED);
	} else {
		/* set responseTime "emergencyRouting" */
		xmlNewProp(ptrLocationRequest, BAD_CAST HELD_RT, BAD_CAST HELD_ER);
	}
	/* locationType - element (optional) */
	ptrLocationType = xmlNewChild(
			ptrLocationRequest, NULL, BAD_CAST HELD_LT, BAD_CAST held->type);
	/* properties */
	xmlNewProp(ptrLocationType, BAD_CAST "exact",
			(held->exact == HELD_EXACT_TRUE) ? BAD_CAST "true"
											 : BAD_CAST "false");
	/* device - element */
	ptrDevice = xmlNewChild(ptrLocationRequest, NULL, BAD_CAST "device", NULL);
	if(ptrDevice == NULL) {
		LM_ERR("locationRequest xmlNewChild() failed\n");
		xmlFreeDoc(request);
		return doc;
	}
	/* properties */
	xmlNewProp(ptrDevice, BAD_CAST "xmlns",
			BAD_CAST "urn:ietf:params:xml:ns:geopriv:held:id");
	/* uri - element */
	xmlNewChild(ptrDevice, NULL, BAD_CAST "uri", BAD_CAST held->identity);

	xmlDocDumpFormatMemory(request, &xmlbuff, &buffersize, 0);
	if(xmlbuff == NULL) {
		LM_ERR("locationRequest xmlDocDumpFormatMemory() failed\n");
		xmlFreeDoc(request);
		return doc;
	}

	doc = (char *)pkg_malloc((buffersize + 1) * sizeof(char));
	if(doc == NULL) {
		PKG_MEM_ERROR;
		xmlFree(xmlbuff);
		xmlFreeDoc(request);
		return doc;
	}

	memset(doc, 0, buffersize);
	memcpy(doc, (char *)xmlbuff, buffersize);
	doc[buffersize] = '\0';

	*lgth = strlen(doc);

	xmlFree(xmlbuff);
	xmlFreeDoc(request);

	return doc;
}

/*
 * lost_append_via_element(head, parent)
 * appends via elements and returns the number of via elements added
 */
int lost_append_via_element(p_lost_list_t *head, xmlNodePtr *parent)
{
	int cnt = 0;
	int i;
	p_lost_list_t current = NULL;

	if (head == NULL) {
		return 0;
	}

	/* at least one <via> element to add */
	current = *head;
	cnt++;

	/* check for more <via> elements to add */
	while (current->next != NULL) {
		cnt++;
		current = current->next;
	}

	current = *head;
	xmlNodePtr ptrVia[cnt];

	/* ad <via> elements to <path> element */
	for (i = 0; i < cnt; i++) {
		ptrVia[i] = xmlNewChild(*parent, NULL, BAD_CAST "via", NULL);
		xmlNewProp(ptrVia[i], BAD_CAST "source", BAD_CAST current->value);
		current = current->next;
	}

	return cnt;
}


/*
 * lost_find_service_request(loc, path, lgth)
 * assembles and returns findService request string (allocated in private memory)
 */
char *lost_find_service_request(p_lost_loc_t loc, p_lost_list_t path, int *lgth)
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
	xmlNodePtr ptrNode = NULL;
	xmlNodePtr ptrPath = NULL;

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
		<path>
			<via source="resolver.example"/>
			<via source="authoritative.example"/>
		</path>
</findService>
 */
	/* create request */
	request = xmlNewDoc(BAD_CAST "1.0");
	if(request == NULL) {
		LM_ERR("findService request xmlNewDoc() failed\n");
		return doc;
	}
	/* findService - element */
	ptrFindService = xmlNewNode(NULL, BAD_CAST "findService");
	if(ptrFindService == NULL) {
		LM_ERR("findService xmlNewNode() failed\n");
		xmlFreeDoc(request);
		return doc;
	}
	xmlDocSetRootElement(request, ptrFindService);
	/* set properties */
	xmlNewProp(ptrFindService, BAD_CAST "xmlns",
			BAD_CAST "urn:ietf:params:xml:ns:lost1");
	xmlNewProp(ptrFindService, BAD_CAST "serviceBoundary",
			(loc->boundary == 1) ? BAD_CAST "value" : BAD_CAST "reference");
	xmlNewProp(ptrFindService, BAD_CAST "recursive",
			(loc->recursive == 1) ? BAD_CAST "true" : BAD_CAST "false");
	if(loc->xpath == NULL) {
		xmlNewProp(ptrFindService, BAD_CAST "xmlns:p2",
				BAD_CAST "http://www.opengis.net/gml");
	}

	/* location - element */
	ptrLocation = xmlNewChild(ptrFindService, NULL, BAD_CAST "location", NULL);
	xmlNewProp(ptrLocation, BAD_CAST "id", BAD_CAST loc->identity);
	xmlNewProp(ptrLocation, BAD_CAST "profile", BAD_CAST loc->profile);
	/* set pos */
	snprintf(buf, BUFSIZE, "%s %s", loc->latitude, loc->longitude);
	/* xpath result */
	if(loc->xpath != NULL) {
		xmlParseInNodeContext(
				ptrLocation, loc->xpath, strlen(loc->xpath), 0, &ptrNode);
		if(ptrNode == NULL) {
			LM_ERR("locationRequest xmlParseInNodeContext() failed\n");
			xmlFreeDoc(request);
			return doc;
		} else {
			xmlAddChild(ptrLocation, ptrNode);
		}
	}
	/* Point */
	else if(loc->radius == 0) {
		ptrPoint = xmlNewChild(ptrLocation, NULL, BAD_CAST "Point", NULL);
		if(ptrPoint == NULL) {
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
	}
	/* circle - Point */
	else {
		ptrCircle = xmlNewChild(ptrLocation, NULL, BAD_CAST "gs:Circle", NULL);
		if(ptrCircle == NULL) {
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
		if(ptrRadius == NULL) {
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
	/* service - element */
	if (path != NULL) {
		ptrPath = xmlNewChild(ptrFindService, NULL, BAD_CAST "path", NULL);
		if(ptrPath == NULL) {
			LM_ERR("locationRequest xmlNewChild() failed\n");
			xmlFreeDoc(request);
			return doc;
		}
		if (lost_append_via_element(&path, &ptrPath) == 0) {
			LM_ERR("appending <via> elements to <path> failed\n");
			xmlFreeDoc(request);
			return doc;
		}
	}

	xmlDocDumpFormatMemory(request, &xmlbuff, &buffersize, 0);
	if(xmlbuff == NULL) {
		LM_ERR("findService request xmlDocDumpFormatMemory() failed\n");
		xmlFreeDoc(request);
		return doc;
	}

	doc = (char *)pkg_malloc((buffersize + 1) * sizeof(char));
	if(doc == NULL) {
		PKG_MEM_ERROR;
		xmlFree(xmlbuff);
		xmlFreeDoc(request);
		return doc;
	}

	memset(doc, 0, buffersize);
	memcpy(doc, (char *)xmlbuff, buffersize);
	doc[buffersize] = '\0';

	*lgth = strlen(doc);

	xmlFree(xmlbuff);
	xmlFreeDoc(request);

	return doc;
}

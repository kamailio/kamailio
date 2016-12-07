/*
 * $Id$
 *
 * LDAP Database Driver for SER
 *
 * Copyright (C) 2008 iptelorg GmbH
 *
 * This file is part of SER, a free SIP server.
 *
 * SER is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/** \addtogroup ldap
 * @{
 */

/** \file
 * The implementation of parser parsing ldap:.. URIs.
 */

#include "ld_uri.h"
#include "ld_cfg.h"

#include "../../mem/mem.h"
#include "../../ut.h"

#include <string.h>

/** compare s1 & s2  with a function f (which should return 0 if ==);
 * s1 & s2 can be null
 * return 0 if match, 1 if not
 */
#define cmpstr(s1, s2, f) \
	((s1)!=(s2)) && ((s1)==0 || (s2)==0 || (f)((s1), (s2))!=0)


/** Compares two LDAP connection URIs.
 * This function is called whenever the database abstraction layer in
 * SER needs to compare to URIs with the ldap scheme. The function
 * compares hosts and port numbers of both URIs (host part comparison
 * is case insensitive). The URI comparison is mainly used to
 * by the connection pool to determine if a connection to a given
 * server already exists.
 **/
static unsigned char ld_uri_cmp(db_uri_t* uri1, db_uri_t* uri2)
{
	struct ld_uri* luri1, *luri2;

	if (!uri1 || !uri2) return 0;

	luri1 = DB_GET_PAYLOAD(uri1);
	luri2 = DB_GET_PAYLOAD(uri2);

	if (luri1->ldap_url->lud_port != luri2->ldap_url->lud_port) return 0;
	if (cmpstr(luri1->ldap_url->lud_host,
			   luri2->ldap_url->lud_host, strcasecmp))
		return 0;
	return 1;
}

/** Duplicate a string
 */
static int dupl_string(char** dst, const char* begin, const char* end)
{
	if (*dst) pkg_free(*dst);

	*dst = pkg_malloc(end - begin + 1);
	if ((*dst) == NULL) {
		return -1;
	}

	memcpy(*dst, begin, end - begin);
	(*dst)[end - begin] = '\0';
	return 0;
}

/** Duplicate a string
 */
static char* pkgstrdup(str* s)
{
	char* dst;

	if (!s)
		return NULL;

	dst = pkg_malloc(s->len + 1);
	if (dst == NULL)
		return NULL;

	memcpy(dst, s->s, s->len);
	dst[s->len] = '\0';

	return dst;
}


/*
 * Parse ldap URI of form
 * //[username[:password]@]hostname[:port]
 *
 * Returns 0 if parsing was successful and -1 otherwise
 */
int parse_ldap_uri(struct ld_uri* res, str* scheme, str* uri)
{
#define SHORTEST_DB_URL "a"
#define SHORTEST_DB_URL_LEN (sizeof(SHORTEST_DB_URL) - 1)

	enum state {
		ST_BEGIN,      /* First state */
		ST_SECTION_ID,  /* Config section id */
		ST_SLASH2,     /* Second slash */
		ST_USER_HOST,  /* Username or hostname */
		ST_PASS_PORT,  /* Password or port part */
		ST_HOST_PORT   /* Hostname and port part */
	};
	enum state st;
	int  i, ldapurllen;
	const char* begin;
	const char* ldapbegin;
	char* prev_token;
	struct ld_con_info* cfg_conn_info;
	char* sport, *puri;
	int portlen = 0;

	prev_token = 0;

	if (!res || !scheme || !uri) {
		goto err;
	}

	if (uri->len < SHORTEST_DB_URL_LEN) {
		goto err;
	}

	st = ST_BEGIN;
	ldapbegin = begin = uri->s;

	for(i = 0; i < uri->len && st != ST_SECTION_ID; i++) {
		switch(st) {
		case ST_BEGIN:
			switch(uri->s[i]) {
			case '/':
				st = ST_SLASH2;
				break;

			default:
				st = ST_SECTION_ID;
			}
			break;

		case ST_SECTION_ID:
			break;

		case ST_SLASH2:
			switch(uri->s[i]) {
			case '/':
				st = ST_USER_HOST;
				ldapbegin = begin = uri->s + i + 1;
				break;

			default:
				goto err;
			}
			break;

		case ST_USER_HOST:
			switch(uri->s[i]) {
			case '@':
				st = ST_HOST_PORT;
				if (dupl_string(&res->username, begin, uri->s + i) < 0) goto err;
				ldapbegin = begin = uri->s + i + 1;
				break;

			case ':':
				st = ST_PASS_PORT;
				if (dupl_string(&prev_token, begin, uri->s + i) < 0) goto err;
				begin = uri->s + i + 1;
				break;
			}
			break;

		case ST_PASS_PORT:
			switch(uri->s[i]) {
			case '@':
				st = ST_HOST_PORT;
				res->username = prev_token;
				if (dupl_string(&res->password, begin, uri->s + i) < 0) goto err;
				ldapbegin = begin = uri->s + i + 1;
				break;
			}
			break;

		case ST_HOST_PORT:
			break;
		}
	}

	switch(st) {
	case ST_PASS_PORT:
	case ST_USER_HOST:
	case ST_HOST_PORT:
		ldapurllen = uri->len - (int)(ldapbegin - uri->s);
		// +3 for the '://' ldap url snippet
		res->uri = pkg_malloc(scheme->len + 3 + ldapurllen + 1);
		if (res->uri== NULL) {
			ERR("ldap: No memory left\n");
			goto err;
		}
		memcpy(res->uri, scheme->s, scheme->len);
		res->uri[scheme->len] = ':';
		res->uri[scheme->len + 1] = '/';
		res->uri[scheme->len + 2] = '/';
		memcpy(res->uri + scheme->len + 3, ldapbegin, ldapurllen);
		res->uri[scheme->len + 3 + ldapurllen] = '\0';

		if (ldap_url_parse(res->uri, &res->ldap_url) != 0) {
			ERR("ldap: Error while parsing URL '%s'\n", res->uri);
			goto err;
		}
		break;
	case ST_SECTION_ID:
		/* the value of uri is the id of the config
		   connection section in this case */
		cfg_conn_info = ld_find_conn_info(uri);
		if (!cfg_conn_info) {
			ERR("ldap: connection id '%.*s' not found in ldap config\n", uri->len, uri->s);
			goto err;
		}

		ldapurllen = cfg_conn_info->host.len;
		sport = NULL;
		if (cfg_conn_info->port) {
			sport = int2str(cfg_conn_info->port, &portlen);
			// +1: we need space for ':' host and port delimiter
			ldapurllen += portlen + 1;
		}

		// +3 for the '://' ldap url snippet
		puri = res->uri = pkg_malloc(scheme->len + 3 + ldapurllen + 1);
		if (res->uri== NULL) {
			ERR("ldap: No memory left\n");
			goto err;
		}
		memcpy(puri, scheme->s, scheme->len);
		puri += scheme->len;
		memcpy(puri, "://", strlen("://"));
		puri+= strlen("://");
		memcpy(puri, cfg_conn_info->host.s, cfg_conn_info->host.len);
		puri+=cfg_conn_info->host.len;
		if (sport) {
			*puri++ = ':';
			memcpy(puri, sport, portlen);
		}
		res->uri[scheme->len + 3 + ldapurllen] = '\0';

		if (ldap_url_parse(res->uri, &res->ldap_url) != 0) {
			ERR("ldap: Error while parsing URL '%s'\n", res->uri);
			goto err;
		}

		if (cfg_conn_info->username.s) {
			if (!(res->username = pkgstrdup(&cfg_conn_info->username))) {
				ERR("ldap: No memory left\n");
				goto err;
			}
		}

		if (cfg_conn_info->password.s) {
			if (!(res->password = pkgstrdup(&cfg_conn_info->password))) {
				ERR("ldap: No memory left\n");
				goto err;
			}
		}

		res->authmech = cfg_conn_info->authmech;
		res->tls = cfg_conn_info->tls;
		if (cfg_conn_info->ca_list.s) {
			if (!(res->ca_list = pkgstrdup(&cfg_conn_info->ca_list))) {
					ERR("ldap: No memory left\n");
					goto err;
			}
		}
		if (cfg_conn_info->req_cert.s) {
			if (!(res->req_cert = pkgstrdup(&cfg_conn_info->req_cert))) {
					ERR("ldap: No memory left\n");
					goto err;
			}
		}

		break;
	default:
		goto err;
	}

	return 0;

 err:
	if (prev_token) pkg_free(prev_token);
	if (res == NULL) return -1;
	if (res->username) {
		pkg_free(res->username);
		res->username = NULL;
	}
	if (res->password) {
		pkg_free(res->password);
		res->password = NULL;
	}
	if (res->ca_list) {
		pkg_free(res->ca_list);
		res->ca_list = NULL;
	}
	if (res->req_cert) {
		pkg_free(res->req_cert);
		res->req_cert = NULL;
	}
	return -1;
}

static void ld_uri_free(db_uri_t* uri, struct ld_uri* payload)
{
	if (payload == NULL) return;
	if (payload->ldap_url) ldap_free_urldesc(payload->ldap_url);
	if (payload->uri) pkg_free(payload->uri);
    if (payload->username) pkg_free(payload->username);
    if (payload->password) pkg_free(payload->password);
    if (payload->ca_list) pkg_free(payload->ca_list);
    if (payload->req_cert) pkg_free(payload->req_cert);
	db_drv_free(&payload->drv);
	pkg_free(payload);
}


int ld_uri(db_uri_t* uri)
{
	struct ld_uri* luri;

	luri = (struct ld_uri*)pkg_malloc(sizeof(struct ld_uri));
	if (luri == NULL) {
		ERR("ldap: No memory left\n");
		goto error;
	}
	memset(luri, '\0', sizeof(struct ld_uri));
	if (db_drv_init(&luri->drv, ld_uri_free) < 0) goto error;
    if (parse_ldap_uri(luri,  &uri->scheme, &uri->body) < 0) goto error;

	DB_SET_PAYLOAD(uri, luri);
	uri->cmp = ld_uri_cmp;
	return 0;

 error:
	if (luri) {
		if (luri->uri) pkg_free(luri->uri);
		if (luri->ldap_url) ldap_free_urldesc(luri->ldap_url);
		db_drv_free(&luri->drv);
		pkg_free(luri);
	}
	return -1;
}


/** @} */

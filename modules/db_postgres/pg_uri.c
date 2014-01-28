/* 
 * $Id$ 
 *
 * PostgreSQL Database Driver for SER
 *
 * Portions Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005-2008 iptelorg GmbH
 *
 * This file is part of SER, a free SIP server.
 *
 * SER is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version
 *
 * For a license to use the ser software under conditions other than those
 * described here, or to purchase support for this software, please contact
 * iptel.org by e-mail at the following addresses: info@iptel.org
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/** \addtogroup postgres
 * @{ 
 */

/** \file 
 * The implementation of parser parsing postgres://.. URIs.
 */

#include "pg_uri.h"

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../ut.h"
#include "../../lib/srdb2/db_gen.h"

#include <stdlib.h>
#include <string.h>


/** compare s1 & s2  with a function f (which should return 0 if ==);
 * s1 & s2 can be null
 * return 0 if match, 1 if not 
 */
#define cmpstr(s1, s2, f) \
	((s1)!=(s2)) && ((s1)==0 || (s2)==0 || (f)((s1), (s2))!=0)


/** Compare two connection URIs */
static unsigned char pg_uri_cmp(db_uri_t* uri1, db_uri_t* uri2)
{
	struct pg_uri* puri1, *puri2;

	if (!uri1 || !uri2) return 0;

	puri1 = DB_GET_PAYLOAD(uri1);
	puri2 = DB_GET_PAYLOAD(uri2);
	if (puri1->port != puri2->port) return 0;

	if (cmpstr(puri1->username, puri2->username, strcmp)) return 0;
	if (cmpstr(puri1->password, puri2->password, strcmp)) return 0;
	if (cmpstr(puri1->host, puri2->host, strcasecmp)) return 0;
	if (cmpstr(puri1->database, puri2->database, strcmp)) return 0;
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


/** Parses postgres URI of form 
 * //[username[:password]@]hostname[:port]/database
 *
 * Returns 0 if parsing was successful and -1 otherwise
 */
static int parse_postgres_uri(struct pg_uri* res, str* uri)
{
#define SHORTEST_DB_URL "//a/b"
#define SHORTEST_DB_URL_LEN (sizeof(SHORTEST_DB_URL) - 1)

	enum state {
		ST_SLASH1,     /* First slash */
		ST_SLASH2,     /* Second slash */
		ST_USER_HOST,  /* Username or hostname */
		ST_PASS_PORT,  /* Password or port part */
		ST_HOST,       /* Hostname part */
		ST_PORT,       /* Port part */
		ST_DB          /* Database part */
	};

	enum state st;
	int  i;
	const char* begin;
	char* prev_token;

	prev_token = 0;

	if (!res || !uri) {
		goto err;
	}
	
	if (uri->len < SHORTEST_DB_URL_LEN) {
		goto err;
	}
	
	st = ST_SLASH1;
	begin = uri->s;

	for(i = 0; i < uri->len; i++) {
		switch(st) {
		case ST_SLASH1:
			switch(uri->s[i]) {
			case '/':
				st = ST_SLASH2;
				break;

			default:
				goto err;
			}
			break;

		case ST_SLASH2:
			switch(uri->s[i]) {
			case '/':
				st = ST_USER_HOST;
				begin = uri->s + i + 1;
				break;
				
			default:
				goto err;
			}
			break;

		case ST_USER_HOST:
			switch(uri->s[i]) {
			case '@':
				st = ST_HOST;
				if (dupl_string(&res->username, begin, uri->s + i) < 0) goto err;
				begin = uri->s + i + 1;
				break;

			case ':':
				st = ST_PASS_PORT;
				if (dupl_string(&prev_token, begin, uri->s + i) < 0) goto err;
				begin = uri->s + i + 1;
				break;

			case '/':
				if (memchr(uri->s + i + 1, '/', uri->len - i - 1) != NULL)
					break;
				if (dupl_string(&res->host, begin, uri->s + i) < 0) goto err;
				if (dupl_string(&res->database, uri->s + i + 1, uri->s + uri->len) < 0) 
					goto err;
				return 0;
			}
			break;

		case ST_PASS_PORT:
			switch(uri->s[i]) {
			case '@':
				st = ST_HOST;
				res->username = prev_token;
				if (dupl_string(&res->password, begin, uri->s + i) < 0) goto err;
				begin = uri->s + i + 1;
				break;

			case '/':
				if (memchr(uri->s + i + 1, '/', uri->len - i - 1) != NULL)
					break;
				res->host = prev_token;
				res->port = str2s(begin, uri->s + i - begin, 0);
				if (dupl_string(&res->database, uri->s + i + 1, uri->s + uri->len) < 0) 
					goto err;
				return 0;
			}
			break;

		case ST_HOST:
			switch(uri->s[i]) {
			case ':':
				st = ST_PORT;
				if (dupl_string(&res->host, begin, uri->s + i) < 0) goto err;
				begin = uri->s + i + 1;
				break;

			case '/':
				if (memchr(uri->s + i + 1, '/', uri->len - i - 1) != NULL)
					break;
				if (dupl_string(&res->host, begin, uri->s + i) < 0) goto err;
				if (dupl_string(&res->database, uri->s + i + 1, uri->s + uri->len) < 0) 
					goto err;
				return 0;
			}
			break;

		case ST_PORT:
			switch(uri->s[i]) {
			case '/':
				res->port = str2s(begin, uri->s + i - begin, 0);
				if (dupl_string(&res->database, uri->s + i + 1, uri->s + uri->len) < 0) 
					goto err;
				return 0;
			}
			break;
			
		case ST_DB:
			break;
		}
	}

	if (st != ST_DB) goto err;
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
	if (res->host) {
		pkg_free(res->host);
		res->host = NULL;
	}
	if (res->database) {
		pkg_free(res->database);
		res->database = NULL;
	}
	return -1;
}



static void pg_uri_free(db_uri_t* uri, struct pg_uri* payload)
{
	if (payload == NULL) return;
	db_drv_free(&payload->drv);
	if (payload->username) pkg_free(payload->username);
	if (payload->password) pkg_free(payload->password);
	if (payload->host) pkg_free(payload->host);
	if (payload->database) pkg_free(payload->database);
	pkg_free(payload);
}


int pg_uri(db_uri_t* uri)
{
	struct pg_uri* puri;

	puri = (struct pg_uri*)pkg_malloc(sizeof(struct pg_uri));
	if (puri == NULL) {
		ERR("postgres: No memory left\n");
		goto error;
	}
	memset(puri, '\0', sizeof(struct pg_uri));
	if (db_drv_init(&puri->drv, pg_uri_free) < 0) goto error;
	if (parse_postgres_uri(puri, &uri->body) < 0) goto error;

	DB_SET_PAYLOAD(uri, puri);
	uri->cmp = pg_uri_cmp;
	return 0;

 error:
	if (puri) {
		db_drv_free(&puri->drv);
		if (puri) pkg_free(puri);
	}
	return -1;
}

/** @} */

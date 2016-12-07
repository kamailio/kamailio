/* 
 * MySQL module interface
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2006-2007 iptelorg GmbH
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

#include "my_uri.h"

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../ut.h"
#include "../../lib/srdb2/db_gen.h"

#include <stdlib.h>
#include <string.h>


/* compare s1 & s2  with a function f (which should return 0 if ==);
 * s1 & s2 can be null
 * return 0 if match, 1 if not */
#define cmpstr(s1, s2, f) \
	((s1)!=(s2)) && ((s1)==0 || (s2)==0 || (f)((s1), (s2))!=0)

/*
 * Compare two connection identifiers
 */
static unsigned char my_uri_cmp(db_uri_t* uri1, db_uri_t* uri2)
{
	struct my_uri* muri1, *muri2;

	if (!uri1 || !uri2) return 0;

	muri1 = DB_GET_PAYLOAD(uri1);
	muri2 = DB_GET_PAYLOAD(uri2);
	if (muri1->port != muri2->port) return 0;

	if (cmpstr(muri1->username, muri2->username, strcmp)) return 0;
	if (cmpstr(muri1->password, muri2->password, strcmp)) return 0;
	if (cmpstr(muri1->host, muri2->host, strcasecmp)) return 0;
	if (cmpstr(muri1->database, muri2->database, strcmp)) return 0;
	return 1;
}



/*
 * Duplicate a string
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


/*
 * Parse mysql URI of form 
 * //[username[:password]@]hostname[:port]/database
 *
 * Returns 0 if parsing was successful and -1 otherwise
 */
static int parse_mysql_uri(struct my_uri* res, str* uri)
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
				if (dupl_string(&res->host, begin, uri->s + i) < 0) goto err;
				if (dupl_string(&res->database, uri->s + i + 1, uri->s + uri->len) < 0) goto err;
				return 0;
			}
			break;

		case ST_PASS_PORT:
			switch(uri->s[i]) {
			case '@':
				st = ST_HOST;
				res->username = prev_token;
				prev_token = 0;
				if (dupl_string(&res->password, begin, uri->s + i) < 0) goto err;
				begin = uri->s + i + 1;
				break;

			case '/':
				res->host = prev_token;
				prev_token = 0;
				res->port = str2s(begin, uri->s + i - begin, 0);
				if (dupl_string(&res->database, uri->s + i + 1, uri->s + uri->len) < 0) goto err;
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
				if (dupl_string(&res->host, begin, uri->s + i) < 0) goto err;
				if (dupl_string(&res->database, uri->s + i + 1, uri->s + uri->len) < 0) goto err;
				return 0;
			}
			break;

		case ST_PORT:
			switch(uri->s[i]) {
			case '/':
				res->port = str2s(begin, uri->s + i - begin, 0);
				if (dupl_string(&res->database, uri->s + i + 1, uri->s + uri->len) < 0) goto err;
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



static void my_uri_free(db_uri_t* uri, struct my_uri* payload)
{
	if (payload == NULL) return;
	db_drv_free(&payload->drv);
	if (payload->username) pkg_free(payload->username);
	if (payload->password) pkg_free(payload->password);
	if (payload->host) pkg_free(payload->host);
	if (payload->database) pkg_free(payload->database);
	pkg_free(payload);
}


int my_uri(db_uri_t* uri)
{
	struct my_uri* res;

	res = (struct my_uri*)pkg_malloc(sizeof(struct my_uri));
	if (res == NULL) {
		ERR("mysql: No memory left\n");
		goto error;
	}
	memset(res, '\0', sizeof(struct my_uri));
	if (db_drv_init(&res->drv, my_uri_free) < 0) goto error;
	if (parse_mysql_uri(res, &uri->body) < 0) goto error;

	DB_SET_PAYLOAD(uri, res);
	uri->cmp = my_uri_cmp;
	return 0;

 error:
	if (res) {
		db_drv_free(&res->drv);
		if (res) pkg_free(res);
	}
	return -1;
}


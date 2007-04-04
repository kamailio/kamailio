/* 
 * $Id$
 *
 * Copyright (C) 2001-2005 FhG FOKUS
 * Copyright (C) 2006-2007 iptelorg GmbH
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

/** \ingroup DB_API @{ */

#include <string.h>
#include "../dprint.h"
#include "../mem/mem.h"
#include "../ut.h"
#include "db_uri.h"


/* compare s1 & s2  with a function f (which should return 0 if ==);
 * s1 & s2 can be null
 * return 0 if match, 1 if not */
#define CMP_STR(s1, s2, f) \
	((s1) != (s2)) && ((s1) == 0 || (s2) == 0 || (f)((s1), (s2)) != 0)


unsigned char db_uri_cmp(db_uri_t* uri1, db_uri_t* uri2)
{
	if (!uri1 || !uri2) return 0;

	if (CMP_STR(uri1->scheme.s, uri2->scheme.s, strcmp)) return 0;

	if (uri1->cmp) {
		return uri1->cmp(uri1, uri2);
	} else {
		/* No driver specific comparison function, compare bodies
		 * byte-wise
		 */
		if (CMP_STR(uri1->body.s, uri2->body.s, strcmp)) return 0;
	}
	return 1;
}



/*
 * Create a new database URI
 */
db_uri_t* db_uri(const char* uri)
{
    char* colon;
    int len;
    db_uri_t* r;
    
    r = (db_uri_t*)pkg_malloc(sizeof(db_uri_t));
    if (r == NULL) goto error;
    memset(r, '\0', sizeof(db_uri_t));
	if (db_gen_init(&r->gen) < 0) goto error;	

    len = strlen(uri);
    colon = q_memchr((char*)uri, ':', len);
    if (colon == NULL) {
		r->scheme.s = pkg_malloc(len + 1);
		if (r->scheme.s == NULL) goto error;
		memcpy(r->scheme.s, uri, len);
		r->scheme.len = len;
    } else {
		r->scheme.len = colon - uri;
		r->scheme.s = pkg_malloc(r->scheme.len + 1);
		if (r->scheme.s == NULL) goto error;
		memcpy(r->scheme.s, uri, colon - uri);
		
		r->body.len = len - r->scheme.len - 1;
		r->body.s = pkg_malloc(r->body.len + 1);
		if (r->body.s == NULL) goto error;
		memcpy(r->body.s, colon + 1, r->body.len);
		r->body.s[r->body.len] = '\0';
    }
    r->scheme.s[r->scheme.len] = '\0';

	/* Call db_uri function if the driver has it */
	if (db_drv_call(&r->scheme, "db_uri", r, 0) < 0) goto error;
    return r;
    
 error:
    ERR("db_uri: Error while creating db_uri structure\n");
	if (r) {
		db_gen_free(&r->gen);
		if (r->body.s) pkg_free(r->body.s);
		if (r->scheme.s) pkg_free(r->scheme.s);
		pkg_free(r);
	}
    return 0;
}


/*
 * Free a connection identifier
 */
void db_uri_free(db_uri_t* uri)
{
    if (uri == NULL) return;
	db_gen_free(&uri->gen);
    if (uri->body.s) pkg_free(uri->body.s);
    if (uri->scheme.s) pkg_free(uri->scheme.s);
    pkg_free(uri);
}

/** @} */

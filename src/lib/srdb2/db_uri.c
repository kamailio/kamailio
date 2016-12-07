/* 
 * Copyright (C) 2001-2005 FhG FOKUS
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

/** \ingroup DB_API 
 * @{ 
 */

#include "db_uri.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../ut.h"
#include <string.h>


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
	db_uri_t* newp;
	char *turi;
    
	newp = (db_uri_t*)pkg_malloc(sizeof(db_uri_t));
	if (newp == NULL) goto error;
	memset(newp, '\0', sizeof(db_uri_t));
	if (db_gen_init(&newp->gen) < 0) goto error;	

	len = strlen(uri);
	turi = (char*)uri;
	colon = q_memchr(turi, ':', len);
	if (colon == NULL) {
		newp->scheme.s = pkg_malloc(len + 1);
		if (newp->scheme.s == NULL) goto error;
		memcpy(newp->scheme.s, uri, len);
		newp->scheme.len = len;
	} else {
		newp->scheme.len = colon - uri;
		newp->scheme.s = pkg_malloc(newp->scheme.len + 1);
		if (newp->scheme.s == NULL) goto error;
		memcpy(newp->scheme.s, uri, colon - uri);
		
		newp->body.len = len - newp->scheme.len - 1;
		newp->body.s = pkg_malloc(newp->body.len + 1);
		if (newp->body.s == NULL) goto error;
		memcpy(newp->body.s, colon + 1, newp->body.len);
		newp->body.s[newp->body.len] = '\0';
	}
	newp->scheme.s[newp->scheme.len] = '\0';

	/* Call db_uri function if the driver has it */
	if (db_drv_call(&newp->scheme, "db_uri", newp, 0) < 0) goto error;
	return newp;
    
 error:
	ERR("db_uri: Error while creating db_uri structure\n");
	if (newp) {
		db_gen_free(&newp->gen);
		if (newp->body.s) pkg_free(newp->body.s);
		if (newp->scheme.s) pkg_free(newp->scheme.s);
		pkg_free(newp);
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

/*
 * BDB Database Driver for Kamailio
 *
 * Copyright (C) 2008 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Kamailio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/** \addtogroup bdb
 * @{
 */

/*! \file
 * Berkeley DB : The implementation of parser parsing bdb:.. URIs.
 *
 * \ingroup database
 */


#include <string.h>

#include "../../mem/mem.h"
#include "../../ut.h"

#include "bdb_uri.h"

#ifndef CFG_DIR
#define CFG_DIR "/tmp"
#endif

#define BDB_ID		"bdb://"
#define BDB_ID_LEN	(sizeof(BDB_ID)-1)

/** compare s1 & s2  with a function f (which should return 0 if ==);
 * s1 & s2 can be null
 * return 0 if match, 1 if not
 */
#define cmpstr(s1, s2, f) \
	((s1)!=(s2)) && ((s1)==0 || (s2)==0 || (f)((s1), (s2))!=0)


/** Compares two BDB connection URIs.
 * This function is called whenever the database abstraction layer in
 * SER needs to compare to URIs with the bdb scheme. The function
 * compares hosts and port numbers of both URIs (host part comparison
 * is case insensitive). The URI comparison is mainly used to
 * by the connection pool to determine if a connection to a given
 * server already exists.
 **/
static unsigned char bdb_uri_cmp(db_uri_t* uri1, db_uri_t* uri2)
{
	bdb_uri_t * buri1, *buri2;

	if (!uri1 || !uri2) return 0;

	buri1 = DB_GET_PAYLOAD(uri1);
	buri2 = DB_GET_PAYLOAD(uri2);

	if (cmpstr(buri1->uri, buri2->uri, strcmp))
		return 0;
	return 1;
}

/*
 * Parse BDB URI of form
 * //path/to/dir
 *
 * Returns 0 if parsing was successful and -1 otherwise
 */
int parse_bdb_uri(bdb_uri_t* res, str* uri)
{
	str s;

	if(uri==NULL || uri->s==NULL)
		return -1;

	s = *uri;

	res->uri = (char*)pkg_malloc((s.len+1)*sizeof(char));

	if(res->uri == NULL)
	{
		ERR("bdb: no more pkg\n");
		return -1;
	}

	memcpy(res->uri, s.s, s.len);
	res->uri[s.len] = '\0';

	if(s.s[0]!='/')
	{
		res->path.s = (char*)pkg_malloc((sizeof(CFG_DIR)+s.len+2)*sizeof(char));
		memset(res->path.s, 0, (sizeof(CFG_DIR)+s.len+2)*sizeof(char));
		if(res->path.s==NULL)
		{
			ERR("bdb: no more pkg.\n");
			pkg_free(res->uri);
			res->uri = NULL;
			return -1;
		}
		strcpy(res->path.s, CFG_DIR);
		res->path.s[sizeof(CFG_DIR)] = '/';
		strncpy(&res->path.s[sizeof(CFG_DIR)+1], s.s, s.len);
		res->path.len = sizeof(CFG_DIR)+s.len;
	} else {
		res->path.s = res->uri;
		res->path.len = strlen(res->path.s);
	}
	
	return 0;
}

static void bdb_uri_free(db_uri_t* uri, bdb_uri_t* payload)
{
	if (payload == NULL) return;
	if(payload->path.s && payload->path.s!=payload->uri)
		pkg_free(payload->path.s);
	if (payload->uri) pkg_free(payload->uri);
	db_drv_free(&payload->drv);
	pkg_free(payload);
}


int bdb_uri(db_uri_t* uri)
{
	bdb_uri_t *buri;

	buri = (bdb_uri_t*)pkg_malloc(sizeof(bdb_uri_t));
	if (buri == NULL) {
		ERR("bdb: No memory left\n");
		goto error;
	}
	memset(buri, '\0', sizeof(bdb_uri_t));
	if (db_drv_init(&buri->drv, bdb_uri_free) < 0) goto error;
    if (parse_bdb_uri(buri,  &uri->body) < 0) goto error;

	DB_SET_PAYLOAD(uri, buri);
	uri->cmp = bdb_uri_cmp;
	return 0;

 error:
	if (buri) {
		if (buri->uri) pkg_free(buri->uri);
		db_drv_free(&buri->drv);
		pkg_free(buri);
	}
	return -1;
}


/** @} */

/*
 * $Id$
 *
 * Copyright (C) 2008 iptelorg GmbH
 * Written by Jan Janak <jan@iptel.org>
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/** \addtogroup flatstore
 * @{ 
 */

/** \file 
 * The implementation of parser parsing flatstore:.. URIs.
 */

#include "flat_uri.h"

#include "../../mem/mem.h"
#include "../../ut.h"

#include <stdlib.h>
#include <string.h>


static void flat_uri_free(db_uri_t* uri, struct flat_uri* payload)
{
	if (payload == NULL) return;
	if (payload->path.s) free(payload->path.s);
	db_drv_free(&payload->drv);
	pkg_free(payload);
}


int flat_uri(db_uri_t* uri)
{
	struct flat_uri* furi;

	if ((furi = (struct flat_uri*)pkg_malloc(sizeof(*furi))) == NULL) {
		ERR("flatstore: No memory left\n");
		return -1;
	}
	memset(furi, '\0', sizeof(*furi));
	if (db_drv_init(&furi->drv, flat_uri_free) < 0) goto error;

	if ((furi->path.s = get_abs_pathname(NULL, &uri->body)) == NULL) {
		ERR("flatstore: Error while obtaining absolute pathname for '%.*s'\n",
			STR_FMT(&uri->body));
		goto error;
	}
	furi->path.len = strlen(furi->path.s);

	DB_SET_PAYLOAD(uri, furi);
	return 0;

 error:
	if (furi) {
		if (furi->path.s) pkg_free(furi->path.s);
		db_drv_free(&furi->drv);
		pkg_free(furi);
	}
	return -1;	
}

/** @} */

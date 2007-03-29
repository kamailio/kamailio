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

#include <string.h>
#include "../mem/mem.h"
#include "../dprint.h"
#include "db_fld.h"


int db_fld_init(db_fld_t* fld, size_t n)
{
	int i;

	memset(fld, '\0', sizeof(db_fld_t) * n);
	for(i = 0; i < n; i++) {
		if (db_gen_init(&fld[i].gen) < 0) return -1;
	}
	return 0;
}


db_fld_t* db_fld(size_t n)
{
	db_fld_t* r;

	r = (db_fld_t*)pkg_malloc(sizeof(db_fld_t) * n);
	if (r == NULL) {
		ERR("db_fld: No memory left\n");
		return NULL;
	}
	if (db_fld_init(r, n) < 0) goto error;
	return r;

 error:
	if (r) {
		db_gen_free(&r->gen);
		pkg_free(r);
	}
	return NULL;
}


void db_fld_free(db_fld_t* fld, size_t n)
{
    int i;
    if (!fld || !n) return;

	for(i = 0; i < n; i++) {
		db_gen_free(&fld[i].gen);
		if (fld[i].name.s) pkg_free(fld[i].name.s);
	}
	pkg_free(fld);
}


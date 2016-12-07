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

#include "db_fld.h"

#include "../../mem/mem.h"
#include "../../dprint.h"

#include <string.h>


char* db_fld_str[] = {
	"DB_NONE",
	"DB_INT",
	"DB_FLOAT",
	"DB_DOUBLE",
	"DB_CSTR",
	"DB_STR",
	"DB_DATETIME",
	"DB_BLOB",
	"DB_BITMAP"
};



int db_fld_init(db_fld_t* fld)
{
	int i;

	for(i = 0; !DB_FLD_LAST(fld[i]); i++) {
		if (db_gen_init(&fld[i].gen) < 0) return -1;
	}
	return 0;
}


void db_fld_close(db_fld_t* fld)
{
	int i;

	for(i = 0; !DB_FLD_LAST(fld[i]); i++) {
		db_gen_free(&fld[i].gen);
	}
}


db_fld_t* db_fld(size_t n)
{
	int i;
	db_fld_t* newp;

	newp = (db_fld_t*)pkg_malloc(sizeof(db_fld_t) * n);
	if (newp == NULL) {
		ERR("db_fld: No memory left\n");
		return NULL;
	}
	memset(newp, '\0', sizeof(db_fld_t) * n);

	for(i = 0; i < n; i++) {
		if (db_gen_init(&newp[i].gen) < 0) goto error;
	}
	return newp;

 error:
	if (newp) {
		while(i >= 0) {
			db_gen_free(&newp[i].gen);
			i--;
		}
		pkg_free(newp);
	}
	return NULL;
}


db_fld_t* db_fld_copy(db_fld_t* fld)
{
	int i, n;
	db_fld_t* newp;

	for(n = 0; fld[n].name; n++);
	n++; /* We need to copy the terminating element too */

	newp = (db_fld_t*)pkg_malloc(sizeof(db_fld_t) * n);
	if (newp == NULL) {
		ERR("db_fld: No memory left\n");
		return NULL;
	}
	memcpy(newp, fld, sizeof(db_fld_t) * n);
	for(i = 0; i < n; i++) {
		if (db_gen_init(&newp[i].gen) < 0) goto error;
	}
	
	return newp;

 error:
 	ERR("db_fld_copy() failed\n");
	if (newp) {
		/* Free everything allocated in this function so far */
		while(i >= 0) {
			db_gen_free(&newp[i].gen);
			i--;
		}
		pkg_free(newp);
	}
	return NULL;
}


void db_fld_free(db_fld_t* fld)
{
	int i;
	
	if (DB_FLD_EMPTY(fld)) return;
	for(i = 0; !DB_FLD_LAST(fld[i]); i++) {
		db_gen_free(&fld[i].gen);
	}
	pkg_free(fld);
}

/** @} */

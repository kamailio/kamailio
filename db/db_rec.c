/* 
 * $Id$ 
 *
 * Copyright (C) 2001-2003 FhG FOKUS
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

#include <stdlib.h>
#include <string.h>
#include "../dprint.h"
#include "../mem/mem.h"
#include "db_rec.h"


db_rec_t* db_rec(db_res_t* res, db_fld_t* fld)
{
    db_rec_t* r;

    r = (db_rec_t*)pkg_malloc(sizeof(db_rec_t));
    if (r == NULL) goto err;
    memset(r, '\0', sizeof(db_rec_t));
	if (db_gen_init(&r->gen) < 0) goto err;
	r->res = res;
	r->fld = fld;
    return r;

 err:
    ERR("Cannot create db_rec structure\n");
	if (r) {
		db_gen_free(&r->gen);
		pkg_free(r);
	}
    return NULL;
}


void db_rec_free(db_rec_t* r)
{
    if (r == NULL) return;
	/* Do not release fld here, it points to an array in db_cmd */
	db_gen_free(&r->gen);
    pkg_free(r);
}

/** @} */

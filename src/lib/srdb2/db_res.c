/* 
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

/** \ingroup DB_API 
 * @{ 
 */

#include "db_res.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include <string.h>


db_res_t* db_res(db_cmd_t* cmd)
{
    db_res_t* newp;
	int ret;

    newp = (db_res_t*)pkg_malloc(sizeof(db_res_t));
    if (newp == NULL) goto err;
	memset(newp, '\0', sizeof(db_res_t));
	if (db_gen_init(&newp->gen) < 0) goto err;
    newp->cmd = cmd;
	newp->field_count = cmd->result_count;

	ret = db_drv_call(&cmd->ctx->con[db_payload_idx]->uri->scheme, 
					  "db_res", newp, db_payload_idx);
	if (ret < 0) goto err;

	newp->cur_rec = db_rec(newp, cmd->result);
	if (newp->cur_rec == NULL) goto err;
    return newp;

 err:
    ERR("db_res: Cannot create db_res structure\n");
	if (newp) {
		if (newp->cur_rec) db_rec_free(newp->cur_rec);
		db_gen_free(&newp->gen);
		pkg_free(newp);
	}
    return NULL;
}


void db_res_free(db_res_t* r)
{
    if (r == NULL) return;
	db_gen_free(&r->gen);
	if (r->cur_rec) db_rec_free(r->cur_rec);
    pkg_free(r);
}


db_rec_t* db_first(db_res_t* res)
{
	if (res->cmd->first[0](res) != 0) {
		return NULL;
	}
	return res->cur_rec;
}


db_rec_t* db_next(db_res_t* res)
{
	if (res->cmd->next[0](res) != 0) {
		return NULL;
	}
	return res->cur_rec;
}

/** @} */

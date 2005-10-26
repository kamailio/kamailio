/* 
 * Copyright (C) 2005 iptelorg GmbH
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

#ifndef __DLG_MOD_H
#define __DLG_MOD_H

#include "../tm/dlg.h"
#include "../../db/db.h"
#include <cds/serialize.h>

/* Prototype of function for storing dialog into database.
 * This function computes ID of newly added row and returns
 * it in dst_id (if set). Function returns 0 if OK, nonzero
 * on error.
 */
/*typedef int (*db_store_dlg_f)(db_con_t* conn, dlg_t *dlg, str *dst_id);
typedef int (*db_load_dlg_f)(db_con_t* conn, str *id, dlg_t **dst_dlg);*/
typedef int (*serialize_dlg_f)(sstream_t *ss, dlg_t *dlg);
typedef int (*dlg2str_f)(dlg_t *dlg, str *dst_str);
typedef int (*str2dlg_f)(const str *s, dlg_t *dst_dlg);

typedef struct {
	/* db_store_dlg_f db_store;
	db_load_dlg_f db_load;*/
	serialize_dlg_f serialize;
	dlg2str_f dlg2str;
	str2dlg_f str2dlg;
} dlg_func_t;

typedef int (*bind_dlg_mod_f)(dlg_func_t *dst);

#endif

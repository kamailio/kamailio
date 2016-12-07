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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __DLG_MOD_H
#define __DLG_MOD_H

#include "../../modules/tm/dlg.h"
#include "../../lib/srdb2/db.h"
#include "../../modules/tm/t_hooks.h"
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
typedef int (*preset_dialog_route_f)(dlg_t* dialog, str *route);
typedef int (*request_outside_f)(str* method, str* headers, str* body, dlg_t* dialog, transaction_cb cb, void* cbp);
typedef int (*request_inside_f)(str* method, str* headers, str* body, dlg_t* dialog, transaction_cb cb, void* cbp);
typedef int (*cmp_dlg_ids_f)(dlg_id_t *a, dlg_id_t *b);
typedef unsigned int (*hash_dlg_id_f)(dlg_id_t *id);

typedef struct {
	/* dialog creation/destruction functions */
	
	
	/* db_store_dlg_f db_store;
	db_load_dlg_f db_load;*/
	
	/* utility functions */
	serialize_dlg_f serialize;
	dlg2str_f dlg2str;
	str2dlg_f str2dlg;
	hash_dlg_id_f hash_dlg_id;
	cmp_dlg_ids_f cmp_dlg_ids;

	/* dialog functions */
	preset_dialog_route_f preset_dialog_route;
	request_outside_f request_outside;
	request_inside_f request_inside;
	
} dlg_func_t;

typedef int (*bind_dlg_mod_f)(dlg_func_t *dst);

#endif

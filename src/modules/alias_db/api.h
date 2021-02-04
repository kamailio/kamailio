/*
 * ALIAS_DB Module
 *
 * Copyright (C) 2011 Crocodile RCS
 *
 * This file is part of a module for Kamailio, a free SIP server.
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
 *
 */

#ifndef ALIASDB_API_H
#define ALIASDB_API_H

#include "../../core/str.h"

typedef int (*alias_db_lookup_t)(sip_msg_t *, str table);
typedef int (*alias_db_lookup_ex_t)(
		sip_msg_t *, str table, unsigned long flags);
typedef int (*alias_db_find_t)(
		sip_msg_t *, str table, char *_in, char *_out, char *flags);

/* clang-format off */
typedef struct alias_db_binds {
	alias_db_lookup_t alias_db_lookup;
	alias_db_lookup_ex_t alias_db_lookup_ex;
	alias_db_find_t alias_db_find;
} alias_db_api_t;
/* clang-format on */

typedef int (*bind_alias_db_f)(alias_db_api_t *);

int bind_alias_db(struct alias_db_binds *);

inline static int alias_db_load_api(alias_db_api_t *pxb)
{
	bind_alias_db_f bind_alias_db_exports;
	if(!(bind_alias_db_exports =
					   (bind_alias_db_f)find_export("bind_alias_db", 1, 0))) {
		LM_ERR("Failed to import bind_alias_db\n");
		return -1;
	}
	return bind_alias_db_exports(pxb);
}

#endif /*ALIASDB_API_H*/

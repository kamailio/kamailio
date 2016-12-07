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

#ifndef _DB_CON_H
#define _DB_CON_H  1

/** \ingroup DB_API 
 * @{ 
 */

#include "db_gen.h"
#include "db_ctx.h"
#include "db_uri.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct db_con;
struct db_ctx;

typedef int (db_con_connect_t)(struct db_con* con);
typedef void (db_con_disconnect_t)(struct db_con* con);


typedef struct db_con {
	db_gen_t gen;            /* Generic part of the structure */
	db_con_connect_t* connect;
	db_con_disconnect_t* disconnect;

	struct db_ctx* ctx;
	db_uri_t* uri;
} db_con_t;

struct db_con* db_con(struct db_ctx* ctx, db_uri_t* uri);
void db_con_free(struct db_con* con);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* _DB_CON_H */



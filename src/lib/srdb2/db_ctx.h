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

#ifndef _DB_CTX_H
#define _DB_CTX_H  1

/** \ingroup DB_API 
 * @{ 
 */

#include "db_drv.h"
#include "db_gen.h"
#include "db_con.h"

#include "../../str.h"
#include "../../list.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct db_ctx;


/* This structure is stored in a linked list inside db_ctx
 * and is used to lookup driver-specific data based on module
 * name. A driver can have multiple connections in a context but
 * it should have only one structure attached to db_ctx structure
 * (which will be shared by all the connections of that driver in
 * db_ctx. 
 */
struct db_ctx_data {
	str module;
	db_drv_t* data;
	SLIST_ENTRY(db_ctx_data) next;
};


typedef struct db_ctx {
	db_gen_t gen;    /* Generic data common for all DB API structures */
	str id;          /* Text id of the context */
	int con_n;       /* Number of connections in the context */
	SLIST_HEAD(, db_ctx_data) data;
	struct db_con* con[DB_PAYLOAD_MAX];
} db_ctx_t;

	
/*
 * Create a new database context
 */
struct db_ctx* db_ctx(const char* id);


/* Remove the database context structure
 * from the linked list and free all memory
 * used by the structure
 */
void db_ctx_free(struct db_ctx* ctx);


/*
 * Add a new database to database context
 */
int db_add_db(struct db_ctx* ctx, const char* uri);


/*
 * Attempt to connect all connections in the
 * context
 */
int db_connect(struct db_ctx* ctx);


/*
 * Disconnect all database connections in the
 * context
 */
void db_disconnect(struct db_ctx* ctx);


#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* _DB_CTX_H */

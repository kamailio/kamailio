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

#include <string.h>
#include "../dprint.h"
#include "../mem/mem.h"
#include "db.h"
#include "db_ctx.h"


static struct db_ctx_data* db_ctx_data(str* module, db_drv_t* data)
{
	struct db_ctx_data* res;

	res = (struct db_ctx_data*)pkg_malloc(sizeof(struct db_ctx_data));
	if (res == NULL) goto error;
	memset(res, '\0', sizeof(struct db_ctx_data));

	res->module.s = pkg_malloc(module->len);
	if (res->module.s == NULL) goto error;

	memcpy(res->module.s, module->s, module->len);
	res->module.len = module->len;
	res->data = data;

	return res;

 error:
	ERR("db_ctx_data: No memory left\n");
	if (res) pkg_free(res);
	return NULL;
}


static void db_ctx_data_free(struct db_ctx_data* ptr)
{
	if (ptr->module.s) pkg_free(ptr->module.s);
	pkg_free(ptr);
}



/*
 * Create a new database context
 */
db_ctx_t* db_ctx(const char* id)
{
    db_ctx_t* r;

    r = (db_ctx_t*)pkg_malloc(sizeof(db_ctx_t));
    if (r == NULL) goto error;
    memset(r, '\0', sizeof(db_ctx_t));
	if (db_gen_init(&r->gen) < 0) goto error;

	DBLIST_INIT(&r->con);

	r->id.len = strlen(id);
	r->id.s = pkg_malloc(r->id.len + 1);
	if (r->id.s == NULL) goto error;
	memcpy(r->id.s, id, r->id.len + 1);

	/* Insert the newly created context into the linked list
	 * of all existing contexts
	 */
	DBLIST_INSERT_HEAD(&db, r);
    return r;

 error:
	if (r) {
		db_gen_free(&r->gen);
		if (r->id.s) pkg_free(r->id.s);
		pkg_free(r);
	}
	return NULL;
}


/* Remove the database context structure
 * from the linked list and free all memory
 * used by the structure
 */
void db_ctx_free(db_ctx_t* ctx)
{
	db_con_t* con, *tmp_con;
	struct db_ctx_data* ptr, *ptr2;

    if (ctx == NULL) return;

	/* Remove the context from the linked list of
	 * all contexts
	 */
	DBLIST_REMOVE(&db, ctx);

	/* Disconnect all connections */
	db_disconnect(ctx);

	/* Destroy the list of all connections */
	DBLIST_FOREACH_SAFE(con, &ctx->con, tmp_con) {
 		DBLIST_REMOVE_HEAD(&ctx->con);
		db_con_free(con);
	}

	/* Dispose all driver specific data structures as well as
	 * the data structures in db_ctx_data linked list
	 */
	SLIST_FOREACH_SAFE(ptr, &ctx->data, next, ptr2) {
		if (ptr->data) ptr->data->free(ptr->data);
		db_ctx_data_free(ptr);
	}
	/* Clear all pointers to attached data structures because we have
	 * freed the attached data structures above already. This is to
	 * ensure that the call to db_gen_free below will not try to free
	 * them one more time
	 */
	memset(((db_gen_t*)ctx)->data, '\0', sizeof(((db_gen_t*)ctx)->data));
	db_gen_free(&ctx->gen);
	
	/* Destroy the structure */
	if (ctx->id.s) pkg_free(ctx->id.s);
    pkg_free(ctx);
}


/*
 * This function traverses the linked list of all db_ctx_data
 * structures stored in a database context to find out whether
 * db_ctx function of the driver whose name is in "module"
 * parameter has already been called.
 */
static struct db_ctx_data* lookup_ctx_data(db_ctx_t* ctx, str* module)
{
	struct db_ctx_data* ptr;

	SLIST_FOREACH(ptr, &ctx->data, next) {
		if (ptr->module.len == module->len &&
			!memcmp(ptr->module.s, module->s, module->len)) {
			return ptr;
		}
	}
	return NULL;
}


/*
 * Add a new database to database context
 */
int db_add_db(db_ctx_t* ctx, const char* uri)
{
	db_con_t* con;
	db_uri_t* parsed_uri = NULL;
	struct db_ctx_data* d;

	/* Make sure we do not attempt to open more than DB_MAX_CON
	 * database connections within one context
	 */
	if (ctx->con_n == DB_PAYLOAD_MAX) {
		ERR("db_add_db: Too many database connections in db context\n");
		return -1;
	}

	/* Get the name of desired database driver */
	parsed_uri = db_uri(uri);
	if (parsed_uri == NULL) goto error;

	d = lookup_ctx_data(ctx, &parsed_uri->scheme);
	if (d) {
		/* We have already called db_ctx function of this DB driver
		 * so reuse the result from the previous run and that's it
		 */
		db_payload_idx = ctx->con_n;
		DB_SET_PAYLOAD(ctx, d->data);
	} else {
		/* We haven't run db_ctx of the DB driver yet, so call the
		 * function, let it create the data if needed and then remember
		 * the result in a newly created db_ctx_data structure
		 */

		/* Call db_ctx function if the driver has it */
		if (db_drv_call(&parsed_uri->scheme, "db_ctx", ctx, ctx->con_n) < 0) {
			goto error;
		}

		d = db_ctx_data(&parsed_uri->scheme, DB_GET_PAYLOAD(ctx));
		if (d == NULL) {
			/* We failed to create db_ctx_data for this payload so we have
			 * to dispose it manually here before bailing out.
			 */
			((struct db_drv*)DB_GET_PAYLOAD(ctx))->free(DB_GET_PAYLOAD(ctx));
			goto error;
		}

		/* Add the newly created db_ctx_data structure to the linked list so
		 * that next time the result stored in the structure will be reused
		 * if another database connection from the same driver is added to
		 * the context
		 */
		SLIST_INSERT_HEAD(&ctx->data, d, next);
	}

	/* We must create the db_con structure after lookup_ctx_data and associated
	 * code above, this is to ensure that db_con in the DB driver gets called 
	 * after db_ctx in the same driver. db_con function might rely on the
	 * previously created context structures
	 */
	con = db_con(ctx, parsed_uri);
	if (con == NULL) goto error;

	DBLIST_INSERT_TAIL(&ctx->con, con);
	ctx->con_n++;

	return 0;

 error:
	if (parsed_uri) db_uri_free(parsed_uri);
	return -1;
}


/*
 * Attempt to connect all connections in the
 * context
 */
int db_connect(db_ctx_t* ctx)
{
	db_con_t* con;

	DBLIST_FOREACH(con, &ctx->con) {
		if (con->connect && con->connect(con) < 0) return -1;
	}
	return 0;
}


/*
 * Disconnect all database connections in the
 * context
 */
void db_disconnect(db_ctx_t* ctx)
{
	db_con_t* con;

	DBLIST_FOREACH(con, &ctx->con) {
		if (con->disconnect) con->disconnect(con);
	}
}

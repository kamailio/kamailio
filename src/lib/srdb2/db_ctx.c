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
 * For a license to use the Kamailio software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
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

#include "db_ctx.h"
#include "db.h"

#include "../../dprint.h"
#include "../../mem/mem.h"

#include <string.h>

static struct db_ctx_data* db_ctx_data(str* module, db_drv_t* data)
{
	struct db_ctx_data* newp;

	newp = (struct db_ctx_data*)pkg_malloc(sizeof(struct db_ctx_data));
	if (newp == NULL) goto error;
	memset(newp, '\0', sizeof(struct db_ctx_data));

	newp->module.s = pkg_malloc(module->len);
	if (newp->module.s == NULL) goto error;

	memcpy(newp->module.s, module->s, module->len);
	newp->module.len = module->len;
	newp->data = data;
	return newp;

 error:
	ERR("No memory left\n");
	if (newp) pkg_free(newp);
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
    db_ctx_t* newp;

    newp = (db_ctx_t*)pkg_malloc(sizeof(db_ctx_t));
    if (newp == NULL) goto error;
    memset(newp, '\0', sizeof(db_ctx_t));
	if (db_gen_init(&newp->gen) < 0) goto error;

	newp->id.len = strlen(id);
	newp->id.s = pkg_malloc(newp->id.len + 1);
	if (newp->id.s == NULL) goto error;
	memcpy(newp->id.s, id, newp->id.len + 1);

	/* Insert the newly created context into the linked list
	 * of all existing contexts
	 */
	DBLIST_INSERT_HEAD(&db_root, newp);
    return newp;

 error:
	if (newp) {
		db_gen_free(&newp->gen);
		if (newp->id.s) pkg_free(newp->id.s);
		pkg_free(newp);
	}
	return NULL;
}


/* Remove the database context structure
 * from the linked list and free all memory
 * used by the structure
 */
void db_ctx_free(db_ctx_t* ctx)
{
	int i;
	struct db_ctx_data* ptr, *ptr2;

    if (ctx == NULL) return;

	/* Remove the context from the linked list of
	 * all contexts
	 */
	DBLIST_REMOVE(&db_root, ctx);

	/* Disconnect all connections */
	db_disconnect(ctx);

	for(i = 0; i < ctx->con_n; i++) {
		db_con_free(ctx->con[i]);
	}

	/* Dispose all driver specific data structures as well as
	 * the data structures in db_ctx_data linked list
	 */
	SLIST_FOREACH_SAFE(ptr, &ctx->data, next, ptr2) {
		if (ptr->data) ptr->data->free((void*)ptr, ptr->data);
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
			((struct db_drv*)DB_GET_PAYLOAD(ctx))->free((void*)ctx, DB_GET_PAYLOAD(ctx));
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

	ctx->con[ctx->con_n++] = con;
	return 0;

 error:
	if (parsed_uri) db_uri_free(parsed_uri);
	ERR("db: db_add_db failed\n");
	return -1;
}


/*
 * Attempt to connect all connections in the
 * context
 */
int db_connect(db_ctx_t* ctx)
{
	int i;

	for(i = 0; i < ctx->con_n; i++) {
		if (ctx->con[i]->connect && ctx->con[i]->connect(ctx->con[i]) < 0) return -1;
	}
	return 0;
}


/*
 * Disconnect all database connections in the
 * context
 */
void db_disconnect(db_ctx_t* ctx)
{
	int i;

	if (ctx != NULL) {
		for(i = 0; i < ctx->con_n; i++) {
			if (ctx->con[i]->disconnect) ctx->con[i]->disconnect(ctx->con[i]);
		}
	}
}

/** @} */

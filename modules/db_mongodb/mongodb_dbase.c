/*
 * Copyright (C) 2014 Daniel-Constantin Mierla (asipto.com)
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
 *
 */


#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../lib/srdb1/db_ut.h"
#include "../../lib/srdb1/db_query.h"
#include "mongodb_connection.h"
#include "mongodb_dbase.h"

typedef struct db_mongodb_result {
	mongoc_collection_t *collection;   /*!< Collection link */
	mongoc_cursor_t *cursor;           /*!< Cursor link */
	bson_t *rdoc;
	int idx;
	bson_t *colsdoc;
	int nrcols;
} db_mongodb_result_t;

/*
 * Initialize database module
 * No function should be called before this
 */
db1_con_t* db_mongodb_init(const str* _url)
{
	return db_do_init(_url, (void *)db_mongodb_new_connection);
}

/*
 * Shut down database module
 * No function should be called after this
 */
void db_mongodb_close(db1_con_t* _h)
{
	db_do_close(_h, db_mongodb_free_connection);
}

int db_mongodb_bson_add(bson_t *doc, int vtype, const db_key_t _k, const db_val_t *_v, int idx)
{
	switch(vtype) {
		case DB1_INT:
			if(!bson_append_int32(doc, _k->s, _k->len,
						VAL_INT(_v)))
			{
				LM_ERR("failed to append int to bson doc %.*s = %d [%d]\n",
						_k->len, _k->s, VAL_INT(_v), idx);
				goto error;
			}
			break;

		case DB1_BIGINT:
			if(!bson_append_int64(doc, _k->s, _k->len,
						VAL_BIGINT(_v )))
			{
				LM_ERR("failed to append bigint to bson doc %.*s = %lld [%d]\n",
						_k->len, _k->s, VAL_BIGINT(_v), idx);
				goto error;
			}
			return -1;

		case DB1_DOUBLE:
			if(!bson_append_double(doc, _k->s, _k->len,
						VAL_DOUBLE(_v)))
			{
				LM_ERR("failed to append double to bson doc %.*s = %f [%d]\n",
						_k->len, _k->s, VAL_DOUBLE(_v), idx);
				goto error;
			}
			break;

		case DB1_STRING:
			if(!bson_append_utf8(doc, _k->s, _k->len,
						VAL_STRING(_v), strlen(VAL_STRING(_v))) )
			{
				LM_ERR("failed to append string to bson doc %.*s = %s [%d]\n",
						_k->len, _k->s, VAL_STRING(_v), idx);
				goto error;
			}
			break;

		case DB1_STR:
			if(!bson_append_utf8(doc, _k->s, _k->len,
						VAL_STR(_v).s, VAL_STR(_v).len) )
			{
				LM_ERR("failed to append str to bson doc %.*s = %.*s [%d]\n",
						_k->len, _k->s, VAL_STR(_v).len, VAL_STR(_v).s, idx);
				goto error;
			}
			break;

		case DB1_DATETIME:
			if(!bson_append_time_t(doc, _k->s, _k->len,
						VAL_TIME(_v)))
			{
				LM_ERR("failed to append time to bson doc %.*s = %ld [%d]\n",
						_k->len, _k->s, VAL_TIME(_v), idx);
				goto error;
			}
			break;

		case DB1_BLOB:
			if(!bson_append_binary(doc, _k->s, _k->len,
						BSON_SUBTYPE_BINARY,
						(const uint8_t *)VAL_BLOB(_v).s, VAL_BLOB(_v).len) )
			{
				LM_ERR("failed to append blob to bson doc %.*s = [bin] [%d]\n",
						_k->len, _k->s, idx);
				goto error;
			}
			break;

		case DB1_BITMAP:
			if(!bson_append_int32(doc, _k->s, _k->len,
						VAL_INT(_v)))
			{
				LM_ERR("failed to append bitmap to bson doc %.*s = %d [%d]\n",
						_k->len, _k->s, VAL_INT(_v), idx);
				goto error;
			}
			break;

		default:
			LM_ERR("val type [%d] not supported\n", vtype);
			return -1;
	}

	return 0;
error:
	return -1;
}

db1_res_t* db_mongodb_new_result(void)
{
	db1_res_t* obj;

	obj = db_new_result();
	if (!obj)
		return NULL;
	RES_PTR(obj) = pkg_malloc(sizeof(db_mongodb_result_t));
	if (!RES_PTR(obj)) {
		db_free_result(obj);
		return NULL;
	}
	memset(RES_PTR(obj), 0, sizeof(db_mongodb_result_t));
	return obj;
}
/*
 * Retrieve result set
 */
static int db_mongodb_store_result(const db1_con_t* _h, db1_res_t** _r)
{
	km_mongodb_con_t *mgcon;
	db_mongodb_result_t *mgres;
	const bson_t *itdoc;
	char *jstr;

	mgcon = MONGODB_CON(_h);
	if(!_r) {
		LM_ERR("invalid result parameter\n");
		return -1;
	}

	*_r = db_mongodb_new_result();
	if (!*_r)  {
		LM_ERR("no memory left for result \n");
		goto error;
	}
	mgres = (db_mongodb_result_t*)RES_PTR(*_r);
	mgres->collection = mgcon->collection;
	mgcon->collection = NULL;
	mgres->cursor = mgcon->cursor;
	mgcon->cursor = NULL;
	mgres->colsdoc = mgcon->colsdoc;
	mgcon->colsdoc = NULL;
	mgres->nrcols = mgcon->nrcols;
	mgcon->nrcols = 0;
	while (mongoc_cursor_more (mgres->cursor)
			&& mongoc_cursor_next (mgres->cursor, &itdoc)) {
		if(is_printable(L_DBG)) {
			jstr = bson_as_json (itdoc, NULL);
			LM_DBG("selected document: %s\n", jstr);
			bson_free (jstr);
		}
	}
	return 0;

error:
	if(mgcon->colsdoc) {
		bson_destroy (mgcon->colsdoc);
		mgcon->colsdoc = NULL;
	}
	mgcon->nrcols = 0;
	if(mgcon->cursor) {
		mongoc_cursor_destroy (mgcon->cursor);
		mgcon->cursor = NULL;
	}
	if(mgcon->collection) {
		mongoc_collection_destroy (mgcon->collection);
		mgcon->collection = NULL;
	}
	return -1;
}

/*
 * Release a result set from memory
 */
int db_mongodb_free_result(db1_con_t* _h, db1_res_t* _r)
{
	if(!_r)
		return -1;
	if(RES_PTR(_r)) {
		if(((db_mongodb_result_t*)RES_PTR(_r))->rdoc) {
			bson_destroy(((db_mongodb_result_t*)RES_PTR(_r))->rdoc);
			((db_mongodb_result_t*)RES_PTR(_r))->rdoc = NULL;
		}
		if(((db_mongodb_result_t*)RES_PTR(_r))->colsdoc) {
			bson_destroy (((db_mongodb_result_t*)RES_PTR(_r))->colsdoc);
			((db_mongodb_result_t*)RES_PTR(_r))->colsdoc = NULL;
		}
		((db_mongodb_result_t*)RES_PTR(_r))->nrcols = 0;
		if(((db_mongodb_result_t*)RES_PTR(_r))->cursor) {
			mongoc_cursor_destroy (((db_mongodb_result_t*)RES_PTR(_r))->cursor);
			((db_mongodb_result_t*)RES_PTR(_r))->cursor = NULL;
		}
		if(((db_mongodb_result_t*)RES_PTR(_r))->collection) {
			mongoc_collection_destroy (((db_mongodb_result_t*)RES_PTR(_r))->collection);
			((db_mongodb_result_t*)RES_PTR(_r))->collection = NULL;
		}
		pkg_free(RES_PTR(_r));
	}
		db_free_result(_r);
	return 0;
}

/*
 * Query table for specified rows
 * _h: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _c: column names to return
 * _n: number of key=values pairs to compare
 * _nc: number of columns to return
 * _o: order by the specified column
 */
int db_mongodb_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
		const db_key_t _o, db1_res_t** _r)
{
	int i;
	km_mongodb_con_t *mgcon;
	mongoc_client_t *client;
	bson_t *seldoc = NULL;
	char *cname;
	char b1;
	char *jstr;

	mgcon = MONGODB_CON(_h);
	if(mgcon==NULL || mgcon->id== NULL || mgcon->con==NULL) {
		LM_ERR("connection to server is null\n");
		return -1;
	}
	if(mgcon->collection) {
		mongoc_collection_destroy (mgcon->collection);
		mgcon->collection = NULL;
	}
	if(mgcon->cursor) {
		mongoc_cursor_destroy (mgcon->cursor);
		mgcon->cursor = NULL;
	}
	if(mgcon->colsdoc) {
		bson_destroy (mgcon->colsdoc);
		mgcon->colsdoc = NULL;
	}
	mgcon->nrcols = 0;
	client = mgcon->con;
	if(CON_TABLE(_h)->s==NULL) {
		LM_ERR("collection (table) name not set\n");
		return -1;
	}

	if(_r) *_r = NULL;

	b1 = '\0';
	if(CON_TABLE(_h)->s[CON_TABLE(_h)->len]!='\0') {
		b1 = CON_TABLE(_h)->s[CON_TABLE(_h)->len];
		CON_TABLE(_h)->s[CON_TABLE(_h)->len] = '\0';
	}
	cname = CON_TABLE(_h)->s;
	LM_DBG("query to collection [%s]\n", cname);
	mgcon->collection = mongoc_client_get_collection(client, mgcon->id->database, cname);
	if(mgcon->collection==NULL) {
		LM_ERR("cannot get collection (table): %s\n", cname);
		if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;
		return -1;
	}
	if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;

	seldoc = bson_new();
	if(seldoc==NULL) {
		LM_ERR("cannot initialize query bson document\n");
		goto error;
	}

	for(i = 0; i < _n; i++) {
		if(db_mongodb_bson_add(seldoc, VAL_TYPE(_v + i), _k[i], _v+i, i)<0)
			goto error;
	}
	if(is_printable(L_DBG)) {
		jstr = bson_as_json (seldoc, NULL);
		LM_DBG("query filter: %s\n", jstr);
		bson_free (jstr);
	}
	if(_nc > 0) {
		mgcon->colsdoc = bson_new();
		if(mgcon->colsdoc==NULL) {
			LM_ERR("cannot initialize columns bson document\n");
			goto error;
		}
		for(i = 0; i < _n; i++) {
			if(!bson_append_int32(mgcon->colsdoc, _c[i]->s, _c[i]->len, 1))
			{
				LM_ERR("failed to append int to columns bson %.*s = %d [%d]\n",
						_c[i]->len, _c[i]->s, 1, i);
				goto error;
			}
		}
		if(is_printable(L_DBG)) {
			jstr = bson_as_json (mgcon->colsdoc, NULL);
			LM_DBG("query filter: %s\n", jstr);
			bson_free (jstr);
		}
		mgcon->nrcols = _nc;
	}
	mgcon->cursor = mongoc_collection_find (mgcon->collection,
						MONGOC_QUERY_NONE, 0, 0, 0,
						seldoc, mgcon->colsdoc, NULL);

	if(!_r) {
		goto done;
	}

	if(db_mongodb_store_result(_h, _r)<0) {
		LM_ERR("failed to store result\n");
		goto error;
	}

done:
	bson_destroy (seldoc);
	return 0;

error:
	LM_ERR("failed to do the query\n");
	if(seldoc) bson_destroy (seldoc);
	if(mgcon->colsdoc) {
		bson_destroy (mgcon->colsdoc);
		mgcon->colsdoc = NULL;
	}
	mgcon->nrcols = 0;
	if(mgcon->collection) {
		mongoc_collection_destroy (mgcon->collection);
		mgcon->collection = NULL;
	}
	if(mgcon->cursor) {
		mongoc_cursor_destroy (mgcon->cursor);
		mgcon->cursor = NULL;
	}
	if(_r && *_r) { db_mongodb_free_result((db1_con_t*)_h, *_r); *_r = NULL; }
	return -1;
}

/*!
 * \brief Gets a partial result set, fetch rows from a result
 *
 * Gets a partial result set, fetch a number of rows from a databae result.
 * This function initialize the given result structure on the first run, and
 * fetches the nrows number of rows. On subsequenting runs, it uses the
 * existing result and fetches more rows, until it reaches the end of the
 * result set. Because of this the result needs to be null in the first
 * invocation of the function. If the number of wanted rows is zero, the
 * function returns anything with a result of zero.
 * \param _h structure representing the database connection
 * \param _r pointer to a structure representing the result
 * \param nrows number of fetched rows
 * \return return zero on success, negative value on failure
 */
int db_mongodb_fetch_result(const db1_con_t* _h, db1_res_t** _r, const int nrows)
{
	return -1;
}


/*
 * Execute a raw SQL query
 */
int db_mongodb_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r)
{
	return -1;
}

/*
 * Insert a row into specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys
 * _n: number of key=value pairs
 */
int db_mongodb_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v, const int _n)
{
	int i;
	km_mongodb_con_t *mgcon;
	mongoc_client_t *client;
	mongoc_collection_t *collection = NULL;
	bson_error_t error;
	bson_t *doc = NULL;
	char *cname;
	char b1;

	mgcon = MONGODB_CON(_h);
	if(mgcon==NULL || mgcon->id== NULL || mgcon->con==NULL) {
		LM_ERR("connection to server is null\n");
		return -1;
	}
	client = mgcon->con;
	if(CON_TABLE(_h)->s==NULL) {
		LM_ERR("collection (table) name not set\n");
		return -1;
	}
	b1 = '\0';
	if(CON_TABLE(_h)->s[CON_TABLE(_h)->len]!='\0') {
		b1 = CON_TABLE(_h)->s[CON_TABLE(_h)->len];
		CON_TABLE(_h)->s[CON_TABLE(_h)->len] = '\0';
	}
	cname = CON_TABLE(_h)->s;
	collection = mongoc_client_get_collection(client, mgcon->id->database, cname);
	if(collection==NULL) {
		LM_ERR("cannot get collection (table): %s\n", cname);
		if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;
		return -1;
	}
	if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;

	doc = bson_new();
	if(doc==NULL) {
		LM_ERR("cannot initialize bson document\n");
		goto error;
	}

	for(i = 0; i < _n; i++) {
		if(db_mongodb_bson_add(doc, VAL_TYPE(_v + i), _k[i], _v+i, i)<0)
			goto error;
	}

	if (!mongoc_collection_insert (collection, MONGOC_INSERT_NONE, doc, NULL, &error)) {
		LM_ERR("failed to insert in collection: %s\n", error.message);
		goto error;
	}
	bson_destroy (doc);
	mongoc_collection_destroy (collection);
	
	return 0;
error:
	if(doc) bson_destroy (doc);
	if(collection) mongoc_collection_destroy (collection);
	return -1;
}

/*
 * Delete a row from the specified table
 * _h: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _n: number of key=value pairs
 */
int db_mongodb_delete(const db1_con_t* _h, const db_key_t* _k,
		const db_op_t* _o, const db_val_t* _v, const int _n)
{
	int i;
	km_mongodb_con_t *mgcon;
	mongoc_client_t *client;
	mongoc_collection_t *collection = NULL;
	bson_error_t error;
	bson_t *doc = NULL;
	char *cname;
	char b1;

	mgcon = MONGODB_CON(_h);
	if(mgcon==NULL || mgcon->id== NULL || mgcon->con==NULL) {
		LM_ERR("connection to server is null\n");
		return -1;
	}
	client = mgcon->con;
	if(CON_TABLE(_h)->s==NULL) {
		LM_ERR("collection (table) name not set\n");
		return -1;
	}
	b1 = '\0';
	if(CON_TABLE(_h)->s[CON_TABLE(_h)->len]!='\0') {
		b1 = CON_TABLE(_h)->s[CON_TABLE(_h)->len];
		CON_TABLE(_h)->s[CON_TABLE(_h)->len] = '\0';
	}
	cname = CON_TABLE(_h)->s;
	collection = mongoc_client_get_collection(client, mgcon->id->database,
			cname);
	if(collection==NULL) {
		LM_ERR("cannot get collection (table): %s\n", cname);
		if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;
		return -1;
	}
	if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;

	doc = bson_new();
	if(doc==NULL) {
		LM_ERR("cannot initialize bson document\n");
		goto error;
	}

	for(i = 0; i < _n; i++) {
		if(db_mongodb_bson_add(doc, VAL_TYPE(_v + i), _k[i], _v+i, i)<0)
			goto error;
	}

	if (!mongoc_collection_remove (collection, MONGOC_REMOVE_NONE,
				doc, NULL, &error)) {
		LM_ERR("failed to delete in collection: %s\n", error.message);
		goto error;
	}
	bson_destroy (doc);
	mongoc_collection_destroy (collection);
	
	return 0;
error:
	if(doc) bson_destroy (doc);
	if(collection) mongoc_collection_destroy (collection);
	return -1;
}

/*
 * Update some rows in the specified table
 * _h: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _uk: updated columns
 * _uv: updated values of the columns
 * _n: number of key=value pairs
 * _un: number of columns to update
 */
int db_mongodb_update(const db1_con_t* _h, const db_key_t* _k,
		const db_op_t* _o, const db_val_t* _v, const db_key_t* _uk,
		const db_val_t* _uv, const int _n, const int _un)
{
	int i;
	km_mongodb_con_t *mgcon;
	mongoc_client_t *client;
	mongoc_collection_t *collection = NULL;
	bson_error_t error;
	bson_t *mdoc = NULL;
	bson_t *udoc = NULL;
	char *cname;
	char b1;

	mgcon = MONGODB_CON(_h);
	if(mgcon==NULL || mgcon->id== NULL || mgcon->con==NULL) {
		LM_ERR("connection to server is null\n");
		return -1;
	}
	client = mgcon->con;
	if(CON_TABLE(_h)->s==NULL) {
		LM_ERR("collection (table) name not set\n");
		return -1;
	}
	b1 = '\0';
	if(CON_TABLE(_h)->s[CON_TABLE(_h)->len]!='\0') {
		b1 = CON_TABLE(_h)->s[CON_TABLE(_h)->len];
		CON_TABLE(_h)->s[CON_TABLE(_h)->len] = '\0';
	}
	cname = CON_TABLE(_h)->s;
	collection = mongoc_client_get_collection(client, mgcon->id->database,
			cname);
	if(collection==NULL) {
		LM_ERR("cannot get collection (table): %s\n", cname);
		if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;
		return -1;
	}
	if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;

	udoc = bson_new();
	if(udoc==NULL) {
		LM_ERR("cannot initialize update bson document\n");
		goto error;
	}
	mdoc = bson_new();
	if(mdoc==NULL) {
		LM_ERR("cannot initialize match bson document\n");
		goto error;
	}

	for(i = 0; i < _un; i++) {
		if(db_mongodb_bson_add(udoc, VAL_TYPE(_uv + i), _uk[i], _uv+i, i)<0)
			goto error;
	}

	for(i = 0; i < _n; i++) {
		if(db_mongodb_bson_add(mdoc, VAL_TYPE(_v + i), _k[i], _v+i, i)<0)
			goto error;
	}

	if (!mongoc_collection_find_and_modify (collection, mdoc, NULL, udoc, NULL,
				false, false, false, NULL, &error)) {
		LM_ERR("failed to update in collection: %s\n", error.message);
		goto error;
	}
	bson_destroy (mdoc);
	bson_destroy (udoc);
	mongoc_collection_destroy (collection);
	
	return 0;
error:
	if(mdoc) bson_destroy (mdoc);
	if(udoc) bson_destroy (udoc);
	if(collection) mongoc_collection_destroy (collection);
	return -1;
}

/*
 * Just like insert, but replace the row if it exists
 */
int db_mongodb_replace(const db1_con_t* _h, const db_key_t* _k,
		const db_val_t* _v, const int _n, const int _un, const int _m)
{
	return -1;
}

/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_mongodb_use_table(db1_con_t* _h, const str* _t)
{
	return db_use_table(_h, _t);
}

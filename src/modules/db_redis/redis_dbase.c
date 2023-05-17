/*
 * Copyright (C) 2018 Andreas Granig (sipwise.com)
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

#include <stdlib.h>
#include <time.h>

#include "db_redis_mod.h"
#include "redis_connection.h"
#include "redis_dbase.h"
#include "redis_table.h"

#define TIMESTAMP_STR_LENGTH 19

static void db_redis_dump_reply(redisReply *reply)
{
	int i;
	if(reply->type == REDIS_REPLY_STRING) {
		LM_DBG("%s\n", reply->str);
	} else if(reply->type == REDIS_REPLY_INTEGER) {
		LM_DBG("%lld\n", reply->integer);
	} else if(reply->type == REDIS_REPLY_NIL) {
		LM_DBG("<null>\n");
	} else if(reply->type == REDIS_REPLY_ARRAY) {
		LM_DBG("printing %lu elements in array reply\n",
				(unsigned long)reply->elements);
		for(i = 0; i < reply->elements; ++i) {
			db_redis_dump_reply(reply->element[i]);
		}
	} else {
		LM_DBG("not printing invalid reply type\n");
	}
}

// TODO: utilize auto-expiry? on insert/update, also update expire value
// of mappings

/*
 * Initialize database module
 * No function should be called before this
 */
db1_con_t *db_redis_init(const str *_url)
{
	return db_do_init(_url, (void *)db_redis_new_connection);
}

/*
 * Shut down database module
 * No function should be called after this
 */
void db_redis_close(db1_con_t *_h)
{
	LM_DBG("closing redis db connection\n");
	db_do_close(_h, db_redis_free_connection);
}


static db1_res_t *db_redis_new_result(void)
{
	db1_res_t *obj;

	obj = db_new_result();
	if(!obj)
		return NULL;
	return obj;
}

static int db_redis_val2str(const db_val_t *v, str *_str)
{
	const char *s;
	const str *tmpstr;
	int vtype = VAL_TYPE(v);
	struct tm _time;
	_str->s = NULL;
	_str->len = 32; // default for numbers

	if(VAL_NULL(v)) {
		LM_DBG("converting <null> value to str\n");
		_str->len = 0;
		return 0;
	}

	switch(vtype) {
		case DB1_INT:
			LM_DBG("converting int value %d to str\n", VAL_INT(v));
			_str->s = (char *)pkg_malloc(_str->len);
			if(!_str->s)
				goto memerr;
			snprintf(_str->s, _str->len, "%d", VAL_INT(v));
			_str->len = strlen(_str->s);
			break;
		case DB1_UINT:
			LM_DBG("converting uint value %u to str\n", VAL_UINT(v));
			_str->s = (char *)pkg_malloc(_str->len);
			if(!_str->s)
				goto memerr;
			snprintf(_str->s, _str->len, "%u", VAL_UINT(v));
			_str->len = strlen(_str->s);
			break;
		case DB1_BIGINT:
			LM_DBG("converting bigint value %lld to str\n", VAL_BIGINT(v));
			_str->s = (char *)pkg_malloc(_str->len);
			if(!_str->s)
				goto memerr;
			snprintf(_str->s, _str->len, "%010lld", VAL_BIGINT(v));
			_str->len = strlen(_str->s);
			break;
		case DB1_UBIGINT:
			LM_DBG("converting ubigint value %llu to str\n", VAL_UBIGINT(v));
			_str->s = (char *)pkg_malloc(_str->len);
			if(!_str->s)
				goto memerr;
			snprintf(_str->s, _str->len, "%010llu", VAL_UBIGINT(v));
			_str->len = strlen(_str->s);
			break;
		case DB1_STRING:
			s = VAL_STRING(v);
			_str->len = strlen(s);
			LM_DBG("converting string value '%s' with len %d to str\n", s,
					_str->len);
			_str->s = (char *)pkg_malloc(_str->len + 1);
			if(!_str->s)
				goto memerr;
			//memcpy(_str->s, s, _str->len);
			//_str->s[_str->len] = '\0';
			memset(_str->s, 0, _str->len + 1);
			strncpy(_str->s, s, _str->len);
			break;
		case DB1_STR:
			tmpstr = &(VAL_STR(v));
			LM_DBG("converting str value '%.*s' with len %d to str\n",
					tmpstr->len, tmpstr->s, tmpstr->len);
			// copy manually to add 0 termination
			_str->s = (char *)pkg_malloc(tmpstr->len + 1);
			if(!_str->s)
				goto memerr;
			_str->len = tmpstr->len;
			memcpy(_str->s, tmpstr->s, _str->len);
			_str->s[_str->len] = '\0';
			break;
		case DB1_DATETIME:
			LM_DBG("converting datetime value %" TIME_T_FMT " to str\n",
					TIME_T_CAST(VAL_TIME(v)));
			_str->s = (char *)pkg_malloc(_str->len);
			if(!_str->s)
				goto memerr;
			localtime_r(&(VAL_TIME(v)), &_time);
			strftime(_str->s, _str->len, "%Y-%m-%d %H:%M:%S", &_time);
			_str->len = strlen(_str->s);
			break;
		case DB1_DOUBLE:
			LM_DBG("converting double value %f to str\n", VAL_DOUBLE(v));
			_str->s = (char *)pkg_malloc(_str->len);
			if(!_str->s)
				goto memerr;
			snprintf(_str->s, _str->len, "%.6f", VAL_DOUBLE(v));
			_str->len = strlen(_str->s);
			break;
		case DB1_BITMAP:
			LM_DBG("converting bitmap value %u to str\n", VAL_BITMAP(v));
			_str->s = (char *)pkg_malloc(_str->len);
			if(!_str->s)
				goto memerr;
			snprintf(_str->s, _str->len, "%u", VAL_BITMAP(v));
			_str->len = strlen(_str->s);
			break;
		case DB1_BLOB:
		default:
			LM_ERR("Unsupported val type %d\n", vtype);
			goto err;
	}

	return 0;

memerr:
	LM_ERR("Failed to allocate memory to convert value to string\n");
err:
	return -1;
}

static int db_redis_return_version(const db1_con_t *_h, km_redis_con_t *con,
		const str *table_name, db1_res_t **_r)
{

	struct str_hash_entry *table_e;
	redis_table_t *table;
	db_val_t *dval;
	db_row_t *drow;

	LM_DBG("get table version\n");

	table_e = str_hash_get(&con->tables, table_name->s, table_name->len);
	if(!table_e) {
		LM_ERR("query to undefined table '%.*s', define it in schema file!\n",
				table_name->len, table_name->s);
		return -1;
	}
	table = (redis_table_t *)table_e->u.p;

	*_r = db_redis_new_result();
	if(!*_r) {
		LM_ERR("Failed to allocate memory for result");
		return -1;
	}
	RES_NUM_ROWS(*_r) = 1;
	RES_COL_N(*_r) = 1;
	RES_ROW_N(*_r) = 1;
	if(db_allocate_rows(*_r) != 0) {
		LM_ERR("Failed to allocate memory for rows\n");
		goto err;
	}
	if(db_allocate_columns(*_r, 1) != 0) {
		LM_ERR("Failed to allocate memory for result columns");
		goto err;
	}

	drow = &(RES_ROWS(*_r)[0]);

	if(db_allocate_row(*_r, drow) != 0) {
		LM_ERR("Failed to allocate row %d\n", RES_NUM_ROWS(*_r));
		goto err;
	}

	dval = &(ROW_VALUES(drow)[0]);

	VAL_TYPE(dval) = DB1_INT;
	VAL_NULL(dval) = 0;
	VAL_INT(dval) = table->version;

	LM_DBG("returning short-cut table version %d for table '%.*s'",
			table->version, table_name->len, table_name->s);

	return 0;

err:
	if(*_r)
		db_redis_free_result((db1_con_t *)_h, *_r);
	return -1;
}

static int db_redis_build_entry_manual_keys(redis_table_t *table,
		const db_key_t *_k, const db_val_t *_v, const int _n, int **manual_keys,
		int *manual_key_count)
{

	// TODO: we also put keys here which are already part of type mapping!
	// there must be removed for performance reasons

	redis_key_t *key = NULL;

	*manual_keys = (int *)pkg_malloc(_n * sizeof(int));
	if(!*manual_keys) {
		LM_ERR("Failed to allocate memory for manual key indices\n");
		goto err;
	}
	memset(*manual_keys, 0, _n * sizeof(int));
	*manual_key_count = 0;

	for(key = table->entry_keys; key; key = key->next) {
		int subkey_found = 0;
		int i;
		*manual_key_count = 0;
		LM_DBG("checking for existence of entry key '%.*s' in query to get "
			   "manual key\n",
				key->key.len, key->key.s);
		for(i = 0; i < _n; ++i) {
			const db_key_t k = _k[i];
			if(!str_strcmp(&key->key, (str *)k)) {
				LM_DBG("found key in entry key\n");
				subkey_found = 1;
				break;
			} else {
				(*manual_keys)[*manual_key_count] = i;
				(*manual_key_count)++;
			}
		}
		if(!subkey_found) {
			break;
		}
	}
	return 0;

err:
	if(*manual_keys) {
		pkg_free(*manual_keys);
		*manual_keys = NULL;
	}
	return -1;
}

static int db_redis_find_query_key(redis_key_t *key, const str *table_name,
		redis_table_t *table, str *type_name, const db_key_t *_k,
		const db_val_t *_v, const db_op_t *_op, const int _n, str *key_name,
		int *key_found, uint64_t *ts_scan_start)
{

	unsigned int len;
	str val = {NULL, 0};

	*key_found = 1;
	key_name->len = 0;
	key_name->s = NULL;

	for(; key; key = key->next) {
		int subkey_found = 0;
		int i;
		LM_DBG("checking for existence of entry key '%.*s' in query\n",
				key->key.len, key->key.s);
		for(i = 0; i < _n; ++i) {
			const db_key_t k = _k[i];
			const db_val_t v = _v[i];
			const db_op_t op = _op ? _op[i] : NULL;

			if(VAL_NULL(&v)) {
				LM_DBG("Skipping null value for given key '%.*s'\n", k->len,
						k->s);
				break;
			} else if(op && strcmp(op, OP_EQ)
					  && !((VAL_TYPE(&v) == DB1_DATETIME
								   || VAL_TYPE(&v) == DB1_BIGINT
								   || VAL_TYPE(&v) == DB1_UBIGINT)
							  && (!strcmp(op, OP_LT) || !strcmp(op, OP_GT)))) {
				LM_DBG("Skipping non-EQ op (%s) for given key '%.*s'\n", op,
						k->len, k->s);
				break;
			} else if(!str_strcmp(&key->key, (str *)k)) {
				LM_DBG("found key in entry key\n");
				if(db_redis_val2str(&v, &val) != 0)
					goto err;
				if(val.s == NULL) {
					LM_DBG("key value in entry key is null, skip key\n");
					subkey_found = 0;
					break;
				}
				if(!key_name->len) {
					// <version>:<table_name>:<type>::<val>
					len = table->version_code.len + table_name->len + 1
						  + type_name->len + 2 + val.len
						  + 1; //snprintf writes term 0 char
					key_name->s = (char *)pkg_malloc(len);
					if(!key_name->s) {
						LM_ERR("Failed to allocate key memory\n");
						goto err;
					}
					snprintf(key_name->s, len, "%.*s%.*s:%.*s::%.*s",
							table->version_code.len, table->version_code.s,
							table_name->len, table_name->s, type_name->len,
							type_name->s, val.len, val.s);
					key_name->len = len - 1; // subtract the term 0 char

				} else {
					// :<val>
					key_name->s = (char *)pkg_realloc(
							key_name->s, key_name->len + val.len + 2);
					if(!key_name->s) {
						LM_ERR("Failed to allocate key memory\n");
						goto err;
					}
					snprintf(key_name->s + key_name->len, 1 + val.len + 1,
							":%.*s", val.len, val.s);
					key_name->len += (1 + val.len);
				}
				if(op
						&& (VAL_TYPE(&v) == DB1_DATETIME
								|| VAL_TYPE(&v) == DB1_BIGINT
								|| VAL_TYPE(&v) == DB1_UBIGINT)
						&& (!strcmp(op, OP_LT) || !strcmp(op, OP_GT))) {
					// Special case: we support matching < or > against timestamps and ints using a special
					// key scanning method. We do this only for a single timestamp/int occurance, and we
					// still do a table scan, just not a full table scan.
					if(!ts_scan_start) {
						LM_DBG("key '%.*s' for type '%.*s' found as timestamp "
							   "or int, but table scans "
							   "not supported, unable to use this type\n",
								key->key.len, key->key.s, type_name->len,
								type_name->s);
						break;
					}
					// ts_scan_start is: 31 bits of current full key length, 31 bits of this value length,
					// one bit of directionality, one bit of length variable indicator
					if(VAL_TYPE(&v) == DB1_DATETIME && *ts_scan_start == 0
							&& val.len == TIMESTAMP_STR_LENGTH) {
						*ts_scan_start =
								key_name->len
								| ((uint64_t)TIMESTAMP_STR_LENGTH << 31);
						if(!strcmp(op, OP_LT))
							*ts_scan_start |= 0x8000000000000000ULL;
						LM_DBG("preparing for timestamp range scan at key "
							   "offset %llx\n",
								(unsigned long long)*ts_scan_start);
						*key_found =
								0; // this forces a table scan using the new match key
					} else if((VAL_TYPE(&v) == DB1_BIGINT
									  || VAL_TYPE(&v) == DB1_UBIGINT)
							  && *ts_scan_start == 0) {
						*ts_scan_start =
								key_name->len | ((uint64_t)val.len << 31);
						*ts_scan_start |=
								0x4000000000000000ULL; // length is variable
						if(!strcmp(op, OP_LT))
							*ts_scan_start |= 0x8000000000000000ULL;
						LM_DBG("preparing for int range scan at key offset "
							   "%llx\n",
								(unsigned long long)*ts_scan_start);
						*key_found =
								0; // this forces a table scan using the new match key
					}
				}
				LM_DBG("entry key so far is '%.*s'\n", key_name->len,
						key_name->s);
				subkey_found = 1;
				pkg_free(val.s);
				val.s = NULL;
				break;
			}
		}
		if(!subkey_found) {
			LM_DBG("key '%.*s' for type '%.*s' not found, unable to use this "
				   "type\n",
					key->key.len, key->key.s, type_name->len, type_name->s);
			if(key_name->s) {
				pkg_free(key_name->s);
				key_name->s = NULL;
				key_name->len = 0;
			}
			*key_found = 0;
			break;
		}
	}

	// for value-less master keys
	if(!key_name->len) {
		// <version>:<table_name>:<type>
		len = table->version_code.len + table_name->len + 1 + type_name->len
			  + 1;
		key_name->s = (char *)pkg_malloc(len);
		if(!key_name->s) {
			LM_ERR("Failed to allocate key memory\n");
			goto err;
		}
		snprintf(key_name->s, len, "%.*s%.*s:%.*s", table->version_code.len,
				table->version_code.s, table_name->len, table_name->s,
				type_name->len, type_name->s);
		key_name->len = len - 1;
	}

	return 0;

err:
	if(val.s)
		pkg_free(val.s);
	if(key_name->s) {
		pkg_free(key_name->s);
		key_name->s = NULL;
		key_name->len = 0;
	}
	return -1;
}

static int db_redis_build_entry_keys(km_redis_con_t *con, const str *table_name,
		const db_key_t *_k, const db_val_t *_v, const int _n,
		redis_key_t **keys, int *keys_count)
{

	struct str_hash_entry *table_e;
	redis_table_t *table;
	redis_key_t *key;
	int key_found;
	str type_name = str_init("entry");
	str keyname = {NULL, 0};

	LM_DBG("build entry keys\n");

	table_e = str_hash_get(&con->tables, table_name->s, table_name->len);
	if(!table_e) {
		LM_ERR("query to undefined table '%.*s', define in db_redis keys "
			   "parameter!",
				table_name->len, table_name->s);
		return -1;
	}
	table = (redis_table_t *)table_e->u.p;
	key = table->entry_keys;
	if(db_redis_find_query_key(key, table_name, table, &type_name, _k, _v, NULL,
			   _n, &keyname, &key_found, NULL)
			!= 0) {
		goto err;
	}
	if(key_found) {
		if(db_redis_key_add_str(keys, &keyname) != 0) {
			LM_ERR("Failed to add key string\n");
			goto err;
		}
		LM_DBG("found suitable entry key '%.*s' for query\n", (*keys)->key.len,
				(*keys)->key.s);
		*keys_count = 1;
	} else {
		LM_ERR("Failed to create direct entry key, no matching key "
			   "definition\n");
		goto err;
	}
	if(keyname.s)
		pkg_free(keyname.s);

	return 0;

err:
	db_redis_key_free(keys);
	if(keyname.s)
		pkg_free(keyname.s);
	return -1;
}

static int db_redis_get_keys_for_all_types(km_redis_con_t *con,
		const str *table_name, redis_key_t **keys, int *keys_count)
{

	struct str_hash_entry *table_e;
	redis_table_t *table;
	redis_type_t *type;
	redis_key_t *key;

	*keys = NULL;
	*keys_count = 0;

	table_e = str_hash_get(&con->tables, table_name->s, table_name->len);
	if(!table_e) {
		LM_ERR("query to undefined table '%.*s', define in db_redis keys "
			   "parameter!",
				table_name->len, table_name->s);
		return -1;
	}
	table = (redis_table_t *)table_e->u.p;

	for(type = table->types; type; type = type->next) {
		for(key = type->keys; key; key = key->next) {
			if(db_redis_key_add_str(keys, &key->key) != 0) {
				LM_ERR("Failed to add key string\n");
				goto err;
			}
			(*keys_count)++;
		}
	}

	return 0;

err:
	db_redis_key_free(keys);
	return -1;
}

static int db_redis_build_type_keys(km_redis_con_t *con, const str *table_name,
		const db_key_t *_k, const db_val_t *_v, const int _n,
		redis_key_t **keys, redis_key_t **set_keys, int *keys_count)
{

	struct str_hash_entry *table_e;
	redis_table_t *table;
	redis_type_t *type;
	redis_key_t *key;

	*keys = NULL;
	*keys_count = 0;

	LM_DBG("build type keys\n");

	table_e = str_hash_get(&con->tables, table_name->s, table_name->len);
	if(!table_e) {
		LM_ERR("query to undefined table '%.*s', define in db_redis keys "
			   "parameter!",
				table_name->len, table_name->s);
		return -1;
	}
	table = (redis_table_t *)table_e->u.p;

	for(type = table->types; type; type = type->next) {
		str *type_name = &(type->type);
		int key_found = 0;
		str keyname = {NULL, 0};
		key = type->keys;

		if(db_redis_find_query_key(key, table_name, table, &type->type, _k, _v,
				   NULL, _n, &keyname, &key_found, NULL)
				!= 0) {
			goto err;
		}
		if(key_found) {
			if(db_redis_key_add_str(keys, &keyname) != 0) {
				LM_ERR("Failed to add query key to key list\n");
				goto err;
			}
			(*keys_count)++;
			LM_DBG("found key '%.*s' for type '%.*s'\n", keyname.len, keyname.s,
					type_name->len, type_name->s);

			if(set_keys) {
				// add key for parent set
				// <version>:<table>::index::<type>
				pkg_free(keyname.s);
				keyname.len = table->version_code.len + table_name->len + 9
							  + type->type.len;
				keyname.s = pkg_malloc(keyname.len + 1);
				if(!keyname.s) {
					LM_ERR("Failed to allocate memory for parent set key\n");
					goto err;
				}
				sprintf(keyname.s, "%.*s%.*s::index::%.*s",
						table->version_code.len, table->version_code.s,
						table_name->len, table_name->s, type->type.len,
						type->type.s);
				if(db_redis_key_add_str(set_keys, &keyname) != 0) {
					LM_ERR("Failed to add query key to set key list\n");
					goto err;
				}
			}
		}
		if(keyname.s)
			pkg_free(keyname.s);
	}

	return 0;

err:
	LM_ERR("Failed to get type key\n");
	db_redis_key_free(keys);
	return -1;
}

static int db_redis_build_query_keys(km_redis_con_t *con, const str *table_name,
		const db_key_t *_k, const db_val_t *_v, const db_op_t *_op,
		const int _n, redis_key_t **query_keys, int *query_keys_count,
		int **manual_keys, int *manual_keys_count, int *do_table_scan,
		uint64_t *ts_scan_start, str *ts_scan_key)
{

	struct str_hash_entry *table_e;
	redis_table_t *table;
	redis_type_t *type;
	redis_key_t *key;
	str keyname;
	int key_found;
	redisReply *reply = NULL;
	str typename = str_init(REDIS_DIRECT_PREFIX);

	*query_keys = NULL;
	*query_keys_count = 0;
	*do_table_scan = 1;

	LM_DBG("build query keys\n");

	table_e = str_hash_get(&con->tables, table_name->s, table_name->len);
	if(!table_e) {
		LM_ERR("query to undefined table '%.*s', define in db_redis keys "
			   "parameter!",
				table_name->len, table_name->s);
		return -1;
	}
	table = (redis_table_t *)table_e->u.p;

	// check if given keys directly match entry key
	keyname.s = NULL;
	keyname.len = 0;
	key = table->entry_keys;

	if(db_redis_find_query_key(key, table_name, table, &typename, _k, _v, _op,
			   _n, &keyname, &key_found, NULL)
			!= 0) {
		goto err;
	}
	if(key_found) {
		LM_DBG("found suitable entry key '%.*s' for query\n", keyname.len,
				keyname.s);
		if(db_redis_key_add_str(query_keys, &keyname) != 0) {
			LM_ERR("Failed to add key name to query keys\n");
			goto err;
		}
		*query_keys_count = 1;
		pkg_free(keyname.s);
		keyname.s = NULL;
	} else {
		if(keyname.s)
			pkg_free(keyname.s);
		keyname.s = NULL;
		LM_DBG("no direct entry key found, checking type keys\n");
		for(type = table->types; type; type = type->next) {
			key = type->keys;
			LM_DBG("checking type '%.*s'\n", type->type.len, type->type.s);
			if(db_redis_find_query_key(key, table_name, table, &type->type, _k,
					   _v, _op, _n, &keyname, &key_found, ts_scan_start)
					!= 0) {
				goto err;
			}
			if(key_found) {
				redis_key_t *query_v = NULL;
				char *prefix = "SMEMBERS";

				if(db_redis_key_add_string(&query_v, prefix, strlen(prefix))
						!= 0) {
					LM_ERR("Failed to add smembers command to query\n");
					db_redis_key_free(&query_v);
					goto err;
				}
				if(db_redis_key_add_str(&query_v, &keyname) != 0) {
					LM_ERR("Failed to add key name to smembers query\n");
					db_redis_key_free(&query_v);
					goto err;
				}

				reply = db_redis_command_argv(con, query_v);
				pkg_free(keyname.s);
				keyname.s = NULL;
				db_redis_key_free(&query_v);
				db_redis_check_reply(con, reply, err);
				if(reply->type == REDIS_REPLY_ARRAY) {
					if(reply->elements == 0) {
						LM_DBG("type query returned empty list\n");
						*query_keys_count = 0;
						*do_table_scan = 0;
						db_redis_free_reply(&reply);
						break;
					} else {
						int i;
						LM_DBG("populating query keys list with result of type "
							   "query\n");
						*query_keys_count = reply->elements;
						for(i = 0; i < reply->elements; ++i) {
							redisReply *subreply = reply->element[i];
							if(subreply->type == REDIS_REPLY_STRING) {
								LM_DBG("adding resulting entry key '%s' from "
									   "type query\n",
										subreply->str);
								if(db_redis_key_prepend_string(query_keys,
										   subreply->str, strlen(subreply->str))
										!= 0) {
									LM_ERR("Failed to add query key\n");
									goto err;
								}
							} else {
								LM_ERR("Unexpected entry key type in type "
									   "query, expecting a string\n");
								goto err;
							}
						}
					}
				} else {
					LM_ERR("Unexpected reply for type query, expecting an "
						   "array\n");
					goto err;
				}

				db_redis_free_reply(&reply);
				break;
			} else if(keyname.s && *ts_scan_start) {
				LM_DBG("will use key '%.*s' at offset %llx for timestamp/int "
					   "range scan\n",
						keyname.len, keyname.s,
						(unsigned long long)*ts_scan_start);
				*ts_scan_key = keyname;
				keyname.s = NULL;
			} else if(keyname.s) {
				pkg_free(keyname.s);
				keyname.s = NULL;
			}
		}
	}

	if(*query_keys_count > 0) {
		LM_DBG("building manual keys\n");
		if(db_redis_build_entry_manual_keys(
				   table, _k, _v, _n, manual_keys, manual_keys_count)
				!= 0) {
			LM_ERR("Failed to build manual entry key list\n");
			goto err;
		}
	}

	return 0;
err:
	if(keyname.s) {
		pkg_free(keyname.s);
		keyname.s = NULL;
	}
	if(reply) {
		db_redis_free_reply(&reply);
	}
	db_redis_key_free(query_keys);
	if(*manual_keys) {
		pkg_free(*manual_keys);
		*manual_keys = NULL;
	}
	return -1;
}

static int db_redis_scan_query_keys_pattern(km_redis_con_t *con,
		const str *match_pattern, const int _n, redis_key_t **query_keys,
		int *query_keys_count, int **manual_keys, int *manual_keys_count,
		unsigned int match_count_start_val)
{

	size_t i = 0;
	redis_key_t *query_v = NULL;
	redisReply *reply = NULL;
	redisReply *keys_list = NULL;
	size_t j;
	int l;


#undef USE_SCAN

#ifdef USE_SCAN

	char cursor_str[32] = "";
	unsigned long cursor = 0;
	unsigned int match_count = match_count_start_val;
	char match_count_str[16];

	do {
		snprintf(cursor_str, sizeof(cursor_str), "%lu", cursor);

		if(db_redis_key_add_string(&query_v, "SCAN", 4) != 0) {
			LM_ERR("Failed to add scan command to scan query\n");
			goto err;
		}
		if(db_redis_key_add_string(&query_v, cursor_str, strlen(cursor_str))
				!= 0) {
			LM_ERR("Failed to add cursor to scan query\n");
			goto err;
		}
		if(db_redis_key_add_string(&query_v, "MATCH", 5) != 0) {
			LM_ERR("Failed to add match command to scan query\n");
			goto err;
		}
		if(db_redis_key_add_string(
				   &query_v, match_pattern->s, match_pattern->len)
				!= 0) {
			LM_ERR("Failed to add match pattern to scan query\n");
			goto err;
		}
		if(db_redis_key_add_string(&query_v, "COUNT", 5) != 0) {
			LM_ERR("Failed to add count command to scan query\n");
			goto err;
		}
		l = snprintf(
				match_count_str, sizeof(match_count_str), "%u", match_count);
		if(l <= 0) {
			LM_ERR("Failed to print integer for scan query\n");
			goto err;
		}
		if(db_redis_key_add_string(&query_v, match_count_str, l) != 0) {
			LM_ERR("Failed to add count value to scan query\n");
			goto err;
		}

		reply = db_redis_command_argv(con, query_v);
		db_redis_key_free(&query_v);
		db_redis_check_reply(con, reply, err);
		if(reply->type != REDIS_REPLY_ARRAY) {
			LM_ERR("Invalid reply type for scan on table '%.*s', expected "
				   "array\n",
					match_pattern->len, match_pattern->s);
			goto err;
		}
		if(reply->elements != 2) {
			LM_ERR("Invalid number of reply elements for scan on table '%.*s', "
				   "expected 2, got %lu\n",
					match_pattern->len, match_pattern->s, reply->elements);
			goto err;
		}

		if(reply->element[0]->type == REDIS_REPLY_STRING) {
			cursor = atol(reply->element[0]->str);
		} else if(reply->element[0]->type == REDIS_REPLY_INTEGER) {
			// should not happen, but play it safe
			cursor = reply->element[0]->integer;
		} else {
			LM_ERR("Invalid cursor type for scan on table '%.*s', expected "
				   "string or integer\n",
					match_pattern->len, match_pattern->s);
			goto err;
		}
		LM_DBG("cursor is %lu\n", cursor);

		keys_list = reply->element[1];

#else // use KEYS

	if(db_redis_key_add_string(&query_v, "KEYS", 4) != 0) {
		LM_ERR("Failed to add scan command to scan query\n");
		goto err;
	}
	if(db_redis_key_add_string(&query_v, match_pattern->s, match_pattern->len)
			!= 0) {
		LM_ERR("Failed to add match pattern to scan query\n");
		goto err;
	}

#ifdef WITH_HIREDIS_CLUSTER
	nodeIterator niter;
	cluster_node *node;
	initNodeIterator(&niter, con->con);
	while((node = nodeNext(&niter)) != NULL) {
		if(node->role != REDIS_ROLE_MASTER)
			continue;
		reply = db_redis_command_argv_to_node(con, query_v, node);
		if(!reply) {
			LM_ERR("Invalid null reply from node %s\n", node->addr);
			goto err;
		}

#else
	reply = db_redis_command_argv(con, query_v);
#endif
		db_redis_check_reply(con, reply, err);
		keys_list = reply;

#endif

		if(keys_list->type != REDIS_REPLY_ARRAY) {
			LM_ERR("Invalid content type for scan on table '%.*s', expected "
				   "array\n",
					match_pattern->len, match_pattern->s);
			goto err;
		}

		*query_keys_count += keys_list->elements;

		for(j = 0; j < keys_list->elements; ++i, ++j) {
			redisReply *key = keys_list->element[j];
			if(!key) {
				LM_ERR("Invalid null key at cursor result index %lu while "
					   "scanning table '%.*s'\n",
						(unsigned long)j, match_pattern->len, match_pattern->s);
				goto err;
			}
			if(key->type != REDIS_REPLY_STRING) {
				LM_ERR("Invalid key type at cursor result index %lu while "
					   "scanning table '%.*s', expected string\n",
						(unsigned long)j, match_pattern->len, match_pattern->s);
				goto err;
			}
			if(db_redis_key_prepend_string(
					   query_keys, key->str, strlen(key->str))
					!= 0) {
				LM_ERR("Failed to prepend redis key\n");
				goto err;
			}
		}

#ifdef USE_SCAN
		// exponential increase and falloff, hovering around 1000 results
		if(keys_list->elements > 1300 && match_count > 500)
			match_count /= 2;
		else if(keys_list->elements < 700 && match_count < 500000)
			match_count *= 2;
#endif

		db_redis_free_reply(&reply);

#ifdef USE_SCAN
	} while(cursor > 0);
#endif

#ifdef WITH_HIREDIS_CLUSTER
}
#endif

// for full table scans, we have to manually match all given keys
// but only do this once for repeated invocations
if(!*manual_keys) {
	*manual_keys_count = _n;
	*manual_keys = (int *)pkg_malloc(*manual_keys_count * sizeof(int));
	if(!*manual_keys) {
		LM_ERR("Failed to allocate memory for manual keys\n");
		goto err;
	}
	memset(*manual_keys, 0, *manual_keys_count * sizeof(int));
	for(l = 0; l < _n; ++l) {
		(*manual_keys)[l] = l;
	}
}

if(reply) {
	db_redis_free_reply(&reply);
}

db_redis_key_free(&query_v);

LM_DBG("got %lu entries by scan\n", (unsigned long)i);
return 0;

err : if(reply) db_redis_free_reply(&reply);
db_redis_key_free(&query_v);
db_redis_key_free(query_keys);
*query_keys_count = 0;
if(*manual_keys) {
	pkg_free(*manual_keys);
	*manual_keys = NULL;
}
return -1;
}

static int db_redis_scan_query_keys(km_redis_con_t *con, const str *table_name,
		const int _n, redis_key_t **query_keys, int *query_keys_count,
		int **manual_keys, int *manual_keys_count, uint64_t ts_scan_start,
		const str *ts_scan_key)
{

	struct str_hash_entry *table_e;
	redis_table_t *table;
	char *match = NULL;
	int ret;
	redisReply *reply = NULL;
	redis_key_t *set_key = NULL;
	int i, j;

	*query_keys = NULL;
	*query_keys_count = 0;
	*manual_keys = NULL;
	*manual_keys_count = 0;
	redis_key_t *set_keys = NULL;
	int set_keys_count = 0;

	table_e = str_hash_get(&con->tables, table_name->s, table_name->len);
	if(!table_e) {
		LM_ERR("query to undefined table '%.*s', define it in schema file!\n",
				table_name->len, table_name->s);
		return -1;
	}
	table = (redis_table_t *)table_e->u.p;

	if(!ts_scan_start) {
		// full table scan
		match = (char *)pkg_malloc(table->version_code.len + table_name->len
								   + 10); // length of ':entry::*' plus \0
		if(!match) {
			LM_ERR("Failed to allocate memory for match pattern\n");
			return -1;
		}
		int len = sprintf(match, "%.*s%.*s:entry::*", table->version_code.len,
				table->version_code.s, table_name->len, table_name->s);
		str match_pattern = {match, len};
		ret = db_redis_scan_query_keys_pattern(con, &match_pattern, _n,
				query_keys, query_keys_count, manual_keys, manual_keys_count,
				1000);
		pkg_free(match);
		return ret;
	}

	// timestamp range scan
	// ex: 2019-07-17 17:33:16
	// if >, we match: [3-9]???-??-?? ??:??:??, 2[1-9]??-??-?? ??:??:??, 20[2-9]?-??-?? ??:??:??, etc
	// if <, we match: [0-1]???-??-?? ??:??:??, 200..., 201[0-8]..., etc
	// the maximum match string length is ts_scan_key->len with one character replaced by 5 ('[a-b]')
	//
	// int range scan
	// ex: 12345
	// if >, we match: 2????, 1[3-9]???, ..., plus ?????*
	// if <. we match: ?, ??, ???, ????, 1[0-1]???, 12[0-2]??, etc
	//    ... however we expect a minimum length of 10 digits as per BIGINT printf format

	match = pkg_malloc(ts_scan_key->len + 6);
	if(!match) {
		LM_ERR("Failed to allocate memory for match pattern\n");
		return -1;
	}

	int scan_lt = (ts_scan_start & 0x8000000000000000ULL) ? 1 : 0;
	int scan_len_variable = (ts_scan_start & 0x4000000000000000ULL) ? 1 : 0;
	unsigned int scan_offset = ts_scan_start & 0x7fffffffULL;
	unsigned int scan_length = (ts_scan_start >> 31) & 0x7fffffffULL;
	scan_offset -= scan_length;
	const char *suffix = ts_scan_key->s + scan_offset + scan_length;

	LM_DBG("running timestamp/int range matching: lt %i, lv %i, off %u, len "
		   "%u\n",
			scan_lt, scan_len_variable, scan_offset, scan_length);

	if(scan_lt && scan_len_variable) {
		// match shorter strings

		// copy unchanged prefix
		memcpy(match, ts_scan_key->s, scan_offset);

		// append a number of ?. minimum string length is 10 digits
		for(i = 0; i < scan_length - 1; i++) {
			int len = scan_offset + i;
			char match_char = ts_scan_key->s[len];
			// skip non-numbers
			if(match_char < '0' || match_char > '9') {
				match[len] = match_char;
				continue;
			}
			// append a single ?
			match[len] = '?';
			// append unchanged suffix
			strcpy(match + len + 1, suffix);
			len = strlen(match);

			// minimum bigint printf string length
			if(i < 10)
				continue;

			str match_pattern = {match, len};
			LM_DBG("running timestamp/int range matching using pattern "
				   "'%.*s'\n",
					len, match);

			ret = db_redis_scan_query_keys_pattern(con, &match_pattern, _n,
					&set_keys, &set_keys_count, manual_keys, manual_keys_count,
					5000);
			if(ret)
				goto out;
		}
	}

	for(i = 0; i < scan_length; i++) {
		int len = scan_offset + i;
		char match_char = ts_scan_key->s[len];
		// skip non-numbers
		if(match_char < '0' || match_char > '9')
			continue;
		// skip numbers that are at the edge of their match range
		if(match_char == '0' && scan_lt)
			continue;
		if(match_char == '1' && scan_lt && i == 0) // no leading 0
			continue;
		if(match_char == '9' && !scan_lt)
			continue;

		// copy unchanged prefix
		memcpy(match, ts_scan_key->s, len);
		// append range matcher
		if(scan_lt)
			len += sprintf(match + len, "[0-%c]", match_char - 1);
		else
			len += sprintf(match + len, "[%c-9]", match_char + 1);
		// finish with trailing ?s
		for(j = i + 1; j < scan_length; j++) {
			match_char = ts_scan_key->s[scan_offset + j];
			// skip non-numbers
			if(match_char < '0' || match_char > '9') {
				match[len++] = match_char;
				continue;
			}
			match[len++] = '?';
		}
		// append unchanged suffix
		strcpy(match + len, suffix);
		len = strlen(match);

		str match_pattern = {match, len};
		LM_DBG("running timestamp/int range matching using pattern '%.*s'\n",
				len, match);

		ret = db_redis_scan_query_keys_pattern(con, &match_pattern, _n,
				&set_keys, &set_keys_count, manual_keys, manual_keys_count,
				5000);
		if(ret)
			goto out;
	}

	if(!scan_lt && scan_len_variable) {
		// match longer strings
		int len = sprintf(match, "%.*s*%s", scan_offset + scan_length,
				ts_scan_key->s, suffix);

		str match_pattern = {match, len};
		LM_DBG("running timestamp/int range matching using pattern '%.*s'\n",
				len, match);

		ret = db_redis_scan_query_keys_pattern(con, &match_pattern, _n,
				&set_keys, &set_keys_count, manual_keys, manual_keys_count,
				5000);
		if(ret)
			goto out;
	}

	// we not have a list of matching type keys in set_keys. now we have to iterate through them
	// and retrieve the set members, and finally build our actual key list

	ret = -1;

	for(set_key = set_keys; set_key; set_key = set_key->next) {
		LM_DBG("pulling set members from key '%.*s'\n", set_key->key.len,
				set_key->key.s);

		redis_key_t *query_v = NULL;
		if(db_redis_key_add_string(&query_v, "SMEMBERS", 8) != 0) {
			LM_ERR("Failed to add smembers command to query\n");
			db_redis_key_free(&query_v);
			goto out;
		}
		if(db_redis_key_add_str(&query_v, &set_key->key) != 0) {
			LM_ERR("Failed to add key name to smembers query\n");
			db_redis_key_free(&query_v);
			goto out;
		}

		reply = db_redis_command_argv(con, query_v);
		db_redis_key_free(&query_v);
		db_redis_check_reply(con, reply, out);

		if(reply->type != REDIS_REPLY_ARRAY) {
			LM_ERR("Unexpected reply for type query, expecting an array\n");
			goto out;
		}

		LM_DBG("adding %i keys returned from set", (int)reply->elements);

		for(i = 0; i < reply->elements; i++) {
			if(reply->element[i]->type != REDIS_REPLY_STRING) {
				LM_ERR("Unexpected entry key type in type query, expecting a "
					   "string\n");
				goto out;
			}
			if(db_redis_key_prepend_string(query_keys, reply->element[i]->str,
					   strlen(reply->element[i]->str))
					!= 0) {
				LM_ERR("Failed to prepend redis key\n");
				goto out;
			}
			LM_DBG("adding key '%s'\n", reply->element[i]->str);
		}
		*query_keys_count += reply->elements;

		db_redis_free_reply(&reply);
	}

	ret = 0;

out:
	pkg_free(match);
	db_redis_key_free(&set_keys);
	db_redis_free_reply(&reply);
	if(ret) {
		db_redis_key_free(query_keys);
		*query_keys_count = 0;
		if(*manual_keys) {
			pkg_free(*manual_keys);
			*manual_keys = NULL;
		}
	}
	return ret;
}

static int db_redis_compare_column(
		db_key_t k, db_val_t *v, db_op_t op, redisReply *reply)
{
	int i_value;
	long long ll_value;
	double d_value;
	str *tmpstr;
	char tmp[32] = "";
	struct tm _time;

	int vtype = VAL_TYPE(v);

	if(VAL_NULL(v) && reply->type == REDIS_REPLY_NIL) {
		LM_DBG("comparing matching NULL values\n");
		return 0;
	} else if(VAL_NULL(v) || reply->type == REDIS_REPLY_NIL) {
		LM_DBG("comparing non-matching NULL values\n");
		return -1;
	}

	switch(vtype) {
		case DB1_INT:
			i_value = atoi(reply->str);
			LM_DBG("comparing INT %d %s %d\n", i_value, op, VAL_INT(v));
			if(!strcmp(op, OP_EQ)) {
				if(i_value == VAL_INT(v))
					return 0;
			} else if(!strcmp(op, OP_LT)) {
				if(i_value < VAL_INT(v))
					return 0;
			} else if(!strcmp(op, OP_GT)) {
				if(i_value > VAL_INT(v))
					return 0;
			} else if(!strcmp(op, OP_LEQ)) {
				if(i_value <= VAL_INT(v))
					return 0;
			} else if(!strcmp(op, OP_GEQ)) {
				if(i_value >= VAL_INT(v))
					return 0;
			} else if(!strcmp(op, OP_NEQ)) {
				if(i_value != VAL_INT(v))
					return 0;
			} else if(!strcmp(op, OP_BITWISE_AND)) {
				if(i_value & VAL_INT(v))
					return 0;
			} else {
				LM_ERR("Unsupported op type '%s'\n", op);
				return -1;
			}
			return -1;
		case DB1_BIGINT:
			ll_value = atoll(reply->str);
			LM_DBG("comparing BIGINT %lld %s %lld\n", ll_value, op,
					VAL_BIGINT(v));
			if(!strcmp(op, OP_EQ)) {
				if(ll_value == VAL_BIGINT(v))
					return 0;
			} else if(!strcmp(op, OP_LT)) {
				if(ll_value < VAL_BIGINT(v))
					return 0;
			} else if(!strcmp(op, OP_GT)) {
				if(ll_value > VAL_BIGINT(v))
					return 0;
			} else if(!strcmp(op, OP_LEQ)) {
				if(ll_value <= VAL_BIGINT(v))
					return 0;
			} else if(!strcmp(op, OP_GEQ)) {
				if(ll_value >= VAL_BIGINT(v))
					return 0;
			} else if(!strcmp(op, OP_NEQ)) {
				if(ll_value != VAL_BIGINT(v))
					return 0;
			} else if(!strcmp(op, OP_BITWISE_AND)) {
				if(ll_value & VAL_BIGINT(v))
					return 0;
			} else {
				LM_ERR("Unsupported op type '%s'\n", op);
				return -1;
			}
			return -1;
		case DB1_STRING:
			LM_DBG("comparing STRING %s %s %s\n", reply->str, op,
					VAL_STRING(v));
			if(!strcmp(op, OP_EQ)) {
				return (strcmp(reply->str, VAL_STRING(v)) == 0) ? 0 : -1;
			} else if(!strcmp(op, OP_LT)) {
				return (strcmp(reply->str, VAL_STRING(v)) < 0) ? 0 : -1;
			} else if(!strcmp(op, OP_GT)) {
				return (strcmp(reply->str, VAL_STRING(v)) > 0) ? 0 : -1;
			} else if(!strcmp(op, OP_LEQ)) {
				return (strcmp(reply->str, VAL_STRING(v)) <= 0) ? 0 : -1;
			} else if(!strcmp(op, OP_GEQ)) {
				return (strcmp(reply->str, VAL_STRING(v)) >= 0) ? 0 : -1;
			} else if(!strcmp(op, OP_NEQ)) {
				return (strcmp(reply->str, VAL_STRING(v)) != 0) ? 0 : -1;
			} else {
				LM_ERR("Unsupported op type '%s'\n", op);
				return -1;
			}
			return -1;
		case DB1_STR:
			tmpstr = (struct _str *)&(VAL_STR(v));
			LM_DBG("comparing STR %s %s %.*s\n", reply->str, op, tmpstr->len,
					tmpstr->s);
			if(!strcmp(op, OP_EQ)) {
				return (strncmp(reply->str, tmpstr->s, tmpstr->len) == 0) ? 0
																		  : -1;
			} else if(!strcmp(op, OP_LT)) {
				return (strncmp(reply->str, tmpstr->s, tmpstr->len) < 0) ? 0
																		 : -1;
			} else if(!strcmp(op, OP_GT)) {
				return (strncmp(reply->str, tmpstr->s, tmpstr->len) > 0) ? 0
																		 : -1;
			} else if(!strcmp(op, OP_LEQ)) {
				return (strncmp(reply->str, tmpstr->s, tmpstr->len) <= 0) ? 0
																		  : -1;
			} else if(!strcmp(op, OP_GEQ)) {
				return (strncmp(reply->str, tmpstr->s, tmpstr->len) >= 0) ? 0
																		  : -1;
			} else if(!strcmp(op, OP_NEQ)) {
				return (strncmp(reply->str, tmpstr->s, tmpstr->len) != 0) ? 0
																		  : -1;
			} else {
				LM_ERR("Unsupported op type '%s'\n", op);
				return -1;
			}
			return -1;
		case DB1_DOUBLE:
			d_value = atof(reply->str);
			LM_DBG("comparing DOUBLE %f %s %f\n", d_value, op, VAL_DOUBLE(v));
			if(!strcmp(op, OP_EQ)) {
				return (d_value == VAL_DOUBLE(v)) ? 0 : -1;
			} else if(!strcmp(op, OP_LT)) {
				return (d_value < VAL_DOUBLE(v)) ? 0 : -1;
			} else if(!strcmp(op, OP_GT)) {
				return (d_value > VAL_DOUBLE(v)) ? 0 : -1;
			} else if(!strcmp(op, OP_LEQ)) {
				return (d_value <= VAL_DOUBLE(v)) ? 0 : -1;
			} else if(!strcmp(op, OP_GEQ)) {
				return (d_value >= VAL_DOUBLE(v)) ? 0 : -1;
			} else if(!strcmp(op, OP_NEQ)) {
				return (d_value != VAL_DOUBLE(v)) ? 0 : -1;
			} else {
				LM_ERR("Unsupported op type '%s'\n", op);
				return -1;
			}
			return -1;
		case DB1_DATETIME:
			// TODO: insert int value to db for faster comparison!
			localtime_r(&(VAL_TIME(v)), &_time);
			strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", &_time);
			LM_DBG("comparing DATETIME %s %s %s\n", reply->str, op, tmp);
			if(!strcmp(op, OP_EQ)) {
				return (strcmp(reply->str, tmp) == 0) ? 0 : -1;
			} else if(!strcmp(op, OP_LT)) {
				return (strcmp(reply->str, tmp) < 0) ? 0 : -1;
			} else if(!strcmp(op, OP_GT)) {
				return (strcmp(reply->str, tmp) > 0) ? 0 : -1;
			} else if(!strcmp(op, OP_LEQ)) {
				return (strcmp(reply->str, tmp) <= 0) ? 0 : -1;
			} else if(!strcmp(op, OP_GEQ)) {
				return (strcmp(reply->str, tmp) >= 0) ? 0 : -1;
			} else if(!strcmp(op, OP_NEQ)) {
				return (strcmp(reply->str, tmp) != 0) ? 0 : -1;
			} else {
				LM_ERR("Unsupported op type '%s'\n", op);
				return -1;
			}
			return -1;
		case DB1_BITMAP:
			i_value = atoi(reply->str);
			LM_DBG("comparing BITMAP %d %s %d\n", i_value, op, VAL_BITMAP(v));
			if(!strcmp(op, OP_EQ)) {
				if(i_value == VAL_BITMAP(v))
					return 0;
			} else if(!strcmp(op, OP_LT)) {
				if(i_value < VAL_BITMAP(v))
					return 0;
			} else if(!strcmp(op, OP_GT)) {
				if(i_value > VAL_BITMAP(v))
					return 0;
			} else if(!strcmp(op, OP_LEQ)) {
				if(i_value <= VAL_BITMAP(v))
					return 0;
			} else if(!strcmp(op, OP_GEQ)) {
				if(i_value >= VAL_BITMAP(v))
					return 0;
			} else if(!strcmp(op, OP_NEQ)) {
				if(i_value != VAL_BITMAP(v))
					return 0;
			} else if(!strcmp(op, OP_BITWISE_AND)) {
				if(i_value & VAL_BITMAP(v))
					return 0;
			} else {
				LM_ERR("Unsupported op type '%s'\n", op);
				return -1;
			}
			return -1;
		case DB1_BLOB:
		default:
			LM_ERR("Unsupported val type %d\n", vtype);
			return -1;
	}
}

static int db_redis_convert_row(km_redis_con_t *con, db1_res_t *_r,
		const db_key_t *_k, const db_val_t *_v, const db_op_t *_op,
		redisReply *reply, const str *table_name, const db_key_t *_c, int _nc,
		int *manual_keys, int manual_keys_count)
{
	db_val_t *dval;
	db_row_t *drow;
	size_t col;

	if(reply->type != REDIS_REPLY_ARRAY) {
		LM_ERR("Unexpected redis reply type, expecting array\n");
		return -1;
	}

	if(!reply->elements) {
		LM_DBG("skip empty row");
		return 0;
	}

	// manually filter non-matching replies
	for(col = 0; col < reply->elements; ++col) {
		if(col < manual_keys_count) {
			int idx = manual_keys[col];
			db_key_t k = _k[idx];
			db_val_t v = _v[idx];
			db_op_t o = _op[idx];
			LM_DBG("manually filtering key '%.*s'\n", k->len, k->s);
			if(db_redis_compare_column(k, &v, o, reply->element[col]) != 0) {
				LM_DBG("column %lu does not match, ignore row\n",
						(unsigned long)col);
				return 0;
			}
		}
	}

	RES_NUM_ROWS(_r) = RES_ROW_N(_r) = RES_NUM_ROWS(_r) + 1;
	drow = &(RES_ROWS(_r)[RES_NUM_ROWS(_r) - 1]);

	if(db_allocate_row(_r, drow) != 0) {
		LM_ERR("Failed to allocate row %d\n", RES_NUM_ROWS(_r));
		return -1;
	}

	if(reply->elements - manual_keys_count > RES_COL_N(_r)) {
		LM_ERR("Invalid number of columns at row %d/%d, expecting %d, got "
			   "%lu\n",
				RES_NUM_ROWS(_r), RES_ROW_N(_r), RES_COL_N(_r),
				(unsigned long)reply->elements - manual_keys_count);
		return -1;
	}
	for(col = manual_keys_count; col < reply->elements; ++col) {
		size_t colidx = col - manual_keys_count;
		size_t redisidx = col;
		int coltype;
		redisReply *col_val = reply->element[redisidx];
		str *col_name = _c[colidx];

		LM_DBG("converting column #%lu of row #%d", (unsigned long)colidx,
				RES_ROW_N(_r));

		if(col_val->type != REDIS_REPLY_STRING
				&& col_val->type != REDIS_REPLY_NIL) {

			LM_ERR("Invalid column value type in column '%.*s' of row %d, "
				   "expecting string or null\n",
					col_name->len, col_name->s, RES_NUM_ROWS(_r));
			return -1;
		}

		if(RES_NUM_ROWS(_r) == 1) {
			coltype =
					db_redis_schema_get_column_type(con, table_name, col_name);
			RES_TYPES(_r)[colidx] = coltype;
		} else {
			coltype = RES_TYPES(_r)[colidx];
		}

		dval = &(ROW_VALUES(drow)[colidx]);
		VAL_TYPE(dval) = coltype;

		if(col_val->type == REDIS_REPLY_NIL) {
			VAL_NULL(dval) = 1;
		} else {
			if(db_str2val(coltype, dval, col_val->str, strlen(col_val->str), 1)
					!= 0) {
				LM_ERR("Failed to convert redis column '%.*s' to db value\n",
						col_name->len, col_name->s);
				return -1;
			}
		}
	}

	return 0;
}

static int db_redis_perform_query(const db1_con_t *_h, km_redis_con_t *con,
		const db_key_t *_k, const db_val_t *_v, const db_op_t *_op,
		const db_key_t *_c, const int _n, const int _nc, db1_res_t **_r,
		redis_key_t **keys, int *keys_count, int **manual_keys,
		int *manual_keys_count, int do_table_scan, uint64_t ts_scan_start,
		const str *ts_scan_key)
{

	redisReply *reply = NULL;
	redis_key_t *query_v = NULL;
	int num_rows = 0;
	redis_key_t *key;
	int i, j, max;

	*_r = db_redis_new_result();
	if(!*_r) {
		LM_ERR("Failed to allocate memory for result");
		goto error;
	}

	if(db_allocate_columns(*_r, _nc) != 0) {
		LM_ERR("Failed to allocate memory for result columns");
		goto error;
	}
	RES_NUM_ROWS(*_r) = RES_ROW_N(*_r) = 0;
	RES_COL_N(*_r) = _nc;

	if(!(*keys_count) && do_table_scan) {
		if(_n > 0) {
			LM_WARN("performing full table scan on table '%.*s' while doing "
					"the query\n",
					CON_TABLE(_h)->len, CON_TABLE(_h)->s);
			for(i = 0; i < _n; ++i) {
				LM_WARN("  scan key %d is '%.*s'\n", i, _k[i]->len, _k[i]->s);
			}
		} else {
			LM_DBG("loading full table: '%.*s\n", CON_TABLE(_h)->len,
					CON_TABLE(_h)->s);
		}
		if(db_redis_scan_query_keys(con, CON_TABLE(_h), _n, keys, keys_count,
				   manual_keys, manual_keys_count, ts_scan_start, ts_scan_key)
				!= 0) {
			LM_ERR("failed to scan query keys\n");
			goto error;
		}
	}

	// we allocate best case scenario (all rows match)
	RES_NUM_ROWS(*_r) = RES_ROW_N(*_r) = *keys_count;
	if(db_allocate_rows(*_r) != 0) {
		LM_ERR("Failed to allocate memory for rows\n");
		return -1;
	}
	RES_COL_N(*_r) = _nc;
	// reset and increment in convert_row
	RES_NUM_ROWS(*_r) = RES_ROW_N(*_r) = 0;

	for(key = *keys; key; key = key->next) {
		redis_key_t *tmp = NULL;
		str *keyname = &(key->key);

		num_rows++;

		LM_DBG("checking key '%s' in redis\n", keyname->s);

		if(db_redis_key_add_string(&query_v, "EXISTS", 6) != 0) {
			LM_ERR("Failed to add exists query to list\n");
			goto error;
		}
		if(db_redis_key_add_str(&query_v, keyname) != 0) {
			LM_ERR("Failed to add key name to list\n");
			goto error;
		}
		if(db_redis_append_command_argv(con, query_v, 1) != REDIS_OK) {
			LM_ERR("Failed to append redis command\n");
			goto error;
		}
		tmp = db_redis_key_shift(&query_v);
		if(tmp)
			db_redis_key_free(&tmp);

		// construct HMGET query
		if(_nc + (*manual_keys_count) == 0) {
			if(db_redis_key_prepend_string(&query_v, "HGETALL", 7) != 0) {
				LM_ERR("Failed to add hgetall query to list\n");
				goto error;
			}
		} else {
			if(db_redis_key_prepend_string(&query_v, "HMGET", 5) != 0) {
				LM_ERR("Failed to add hmget query to list\n");
				goto error;
			}
		}

		// we put the manual comparison columns first, so we can skip them
		// easily in result, for the cost of potential duplicate column returns
		for(j = 0; j < *manual_keys_count; ++j) {
			int idx = (*manual_keys)[j];
			str *k_name = _k[idx];
			if(db_redis_key_add_str(&query_v, k_name) != 0) {
				LM_ERR("Failed to add manual key to query list\n");
				goto error;
			}
		}
		for(j = 0; j < _nc; ++j) {
			str *k_name = _c[j];
			if(db_redis_key_add_str(&query_v, k_name) != 0) {
				LM_ERR("Failed to add manual key to query list\n");
				goto error;
			}
		}

		if(db_redis_append_command_argv(con, query_v, 1) != REDIS_OK) {
			LM_ERR("Failed to append redis command\n");
			goto error;
		}

		db_redis_key_free(&query_v);
		query_v = NULL;

		max = 0;
		if(*keys_count == num_rows)
			max = (*keys_count) % 1000;
		else if(num_rows % 1000 == 0)
			max = 1000;

		if(max) {
			LM_DBG("fetching next %d results\n", max);
			for(i = 0; i < max; ++i) {
				// get reply for EXISTS query
				if(db_redis_get_reply(con, (void **)&reply) != REDIS_OK) {
					LM_ERR("Failed to get reply for query: %s\n",
							db_redis_get_error(con));
					goto error;
				}
				db_redis_check_reply(con, reply, error);
				if(reply->integer == 0) {
					LM_DBG("key does not exist, returning no row for query\n");
					db_redis_free_reply(&reply);
					// also free next reply, as this is a null row for the HMGET
					if(db_redis_get_reply(con, (void **)&reply) != REDIS_OK) {
						LM_ERR("Failed to get reply for query: %s\n",
								db_redis_get_error(con));
						goto error;
					}
					db_redis_check_reply(con, reply, error);
					db_redis_free_reply(&reply);
					continue;
				}
				db_redis_free_reply(&reply);

				// get reply for actual HMGET query
				if(db_redis_get_reply(con, (void **)&reply) != REDIS_OK) {
					LM_ERR("Failed to get reply for query: %s\n",
							db_redis_get_error(con));
					goto error;
				}
				db_redis_check_reply(con, reply, error);
				if(reply->type != REDIS_REPLY_ARRAY) {
					LM_ERR("Unexpected reply, expected array\n");
					goto error;
				}
				LM_DBG("dumping full query reply for row\n");
				db_redis_dump_reply(reply);

				if(db_redis_convert_row(con, *_r, _k, _v, _op, reply,
						   CON_TABLE(_h), _c, _nc, *manual_keys,
						   *manual_keys_count)) {
					LM_ERR("Failed to convert redis reply for row\n");
					goto error;
				}
				db_redis_free_reply(&reply);
			}
		}
	}

	LM_DBG("done performing query\n");
	return 0;

error:
	LM_ERR("failed to perform the query\n");
	db_redis_key_free(&query_v);
	if(reply)
		db_redis_free_reply(&reply);
	if(*_r) {
		db_redis_free_result((db1_con_t *)_h, *_r);
		*_r = NULL;
	}
	return -1;
}

static int db_redis_perform_delete(const db1_con_t *_h, km_redis_con_t *con,
		const db_key_t *_k, const db_val_t *_v, const db_op_t *_op,
		const int _n, redis_key_t **keys, int *keys_count, int **manual_keys,
		int *manual_keys_count, int do_table_scan, uint64_t ts_scan_start,
		const str *ts_scan_key)
{

	int i = 0, j = 0;
	redis_key_t *k = NULL;
	int type_keys_count = 0;
	int all_type_keys_count = 0;
	size_t col;

	redisReply *reply = NULL;
	redis_key_t *query_v = NULL;
	redis_key_t *type_keys = NULL;
	redis_key_t *set_keys = NULL;
	redis_key_t *all_type_keys = NULL;
	db_val_t *db_vals = NULL;
	db_key_t *db_keys = NULL;
	redis_key_t *type_key;
	redis_key_t *set_key;

#ifdef WITH_HIREDIS_CLUSTER
	long long scard;
#endif

	if(!*keys_count && do_table_scan) {
		if(!ts_scan_start)
			LM_WARN("performing full table scan on table '%.*s' while "
					"performing delete\n",
					CON_TABLE(_h)->len, CON_TABLE(_h)->s);
		else
			LM_WARN("performing table scan on table '%.*s' while performing "
					"delete using match key "
					"'%.*s' at offset %llx\n",
					CON_TABLE(_h)->len, CON_TABLE(_h)->s, ts_scan_key->len,
					ts_scan_key->s, (unsigned long long)ts_scan_start);
		for(i = 0; i < _n; ++i) {
			LM_WARN("  scan key %d is '%.*s'\n", i, _k[i]->len, _k[i]->s);
		}
		if(db_redis_scan_query_keys(con, CON_TABLE(_h), _n, keys, keys_count,
				   manual_keys, manual_keys_count, ts_scan_start, ts_scan_key)
				!= 0) {
			LM_ERR("failed to scan query keys\n");
			goto error;
		}
	}

	// TODO: this should be moved to redis_connection structure
	// and be parsed at startup:
	//
	// fetch list of keys in all types
	if(db_redis_get_keys_for_all_types(
			   con, CON_TABLE(_h), &all_type_keys, &all_type_keys_count)
			!= 0) {
		LM_ERR("failed to get full list of type keys\n");
		goto error;
	}

	LM_DBG("delete all keys\n");
	for(k = *keys; k; k = k->next) {
		redis_key_t *all_type_key;
		str *key = &k->key;
		redis_key_t *tmp = NULL;
		int row_match;
		LM_DBG("delete key '%.*s'\n", key->len, key->s);

		if(db_redis_key_add_string(&query_v, "EXISTS", 6) != 0) {
			LM_ERR("Failed to add exists command to pre-delete query\n");
			goto error;
		}
		if(db_redis_key_add_str(&query_v, key) != 0) {
			LM_ERR("Failed to add key name to pre-delete query\n");
			goto error;
		}

		// TODO: pipeline commands!
		reply = db_redis_command_argv(con, query_v);
		db_redis_check_reply(con, reply, error);
		if(reply->integer == 0) {
			LM_DBG("key does not exist in redis, skip deleting\n");
			db_redis_free_reply(&reply);
			db_redis_key_free(&query_v);
			continue;
		}
		db_redis_free_reply(&reply);
		tmp = db_redis_key_shift(&query_v);
		if(tmp)
			db_redis_key_free(&tmp);

		if(db_redis_key_prepend_string(&query_v, "HMGET", 5) != 0) {
			LM_ERR("Failed to set hmget command to pre-delete query\n");
			goto error;
		}

		// add all manual keys to query
		for(j = 0; j < *manual_keys_count; ++j) {
			int idx = (*manual_keys)[j];
			str *col = _k[idx];

			if(db_redis_key_add_str(&query_v, col) != 0) {
				LM_ERR("Failed to add manual key to pre-delete query\n");
				goto error;
			}
		}
		// add all type keys to query
		for(all_type_key = all_type_keys; all_type_key;
				all_type_key = all_type_key->next) {
			if(db_redis_key_add_str(&query_v, &all_type_key->key) != 0) {
				LM_ERR("Failed to add type key to pre-delete query\n");
				goto error;
			}
		}
		reply = db_redis_command_argv(con, query_v);
		db_redis_key_free(&query_v);
		db_redis_check_reply(con, reply, error);

		LM_DBG("dumping full query reply\n");
		db_redis_dump_reply(reply);

		// manually filter non-matching replies
		row_match = 1;
		for(col = 0; col < reply->elements; ++col) {
			if(col < *manual_keys_count) {
				int idx = (*manual_keys)[col];
				db_key_t k = _k[idx];
				db_val_t v = _v[idx];
				db_op_t o = _op[idx];
				LM_DBG("manually filtering key '%.*s'\n", k->len, k->s);
				if(db_redis_compare_column(k, &v, o, reply->element[col])
						!= 0) {
					LM_DBG("column %lu does not match, ignore row\n",
							(unsigned long)col);
					row_match = 0;
					break;
				}
			}
		}
		if(!row_match) {
			db_redis_free_reply(&reply);
			continue;
		} else {
			LM_DBG("row matches manual filtering, proceed with deletion\n");
		}

		db_keys =
				(db_key_t *)pkg_malloc(all_type_keys_count * sizeof(db_key_t));
		if(!db_keys) {
			LM_ERR("Failed to allocate memory for db type keys\n");
			goto error;
		}
		for(j = 0, tmp = all_type_keys; tmp; ++j, tmp = tmp->next) {
			db_keys[j] = &tmp->key;
		}

		db_vals = (db_val_t *)pkg_mallocxz(
				all_type_keys_count * sizeof(db_val_t));
		if(!db_vals) {
			LM_ERR("Failed to allocate memory for manual db vals\n");
			goto error;
		}

		for(j = 0, all_type_key = all_type_keys; all_type_key;
				++j, all_type_key = all_type_key->next) {
			db_val_t *v = &(db_vals[j]);
			str *key = &all_type_key->key;
			char *value = reply->element[*manual_keys_count + j]->str;
			int coltype =
					db_redis_schema_get_column_type(con, CON_TABLE(_h), key);
			if(value == NULL) {
				VAL_NULL(v) = 1;
			} else if(db_str2val(coltype, v, value, strlen(value), 0) != 0) {
				LM_ERR("Failed to convert redis reply column to db value\n");
				goto error;
			}
		}
		if(db_redis_build_type_keys(con, CON_TABLE(_h), db_keys, db_vals,
				   all_type_keys_count, &type_keys, &set_keys, &type_keys_count)
				!= 0) {
			LM_ERR("failed to build type keys\n");
			goto error;
		}
		pkg_free(db_keys);
		db_keys = NULL;
		pkg_free(db_vals);
		db_vals = NULL;
		db_redis_free_reply(&reply);

		if(db_redis_key_add_string(&query_v, "DEL", 3) != 0) {
			LM_ERR("Failed to add del command to delete query\n");
			goto error;
		}
		if(db_redis_key_add_str(&query_v, key) != 0) {
			LM_ERR("Failed to add key to delete query\n");
			goto error;
		}

		reply = db_redis_command_argv(con, query_v);
		db_redis_key_free(&query_v);
		db_redis_check_reply(con, reply, error);
		db_redis_free_reply(&reply);

		for(type_key = type_keys, set_key = set_keys; type_key;
				type_key = type_key->next, set_key = set_key->next) {

#ifdef WITH_HIREDIS_CLUSTER
			if(db_redis_key_add_string(&query_v, "SREM", 4) != 0) {
				LM_ERR("Failed to add srem command to post-delete query\n");
				goto error;
			}
			if(db_redis_key_add_str(&query_v, &type_key->key) != 0) {
				LM_ERR("Failed to add key to delete query\n");
				goto error;
			}
			if(db_redis_key_add_str(&query_v, key) != 0) {
				LM_ERR("Failed to add key to delete query\n");
				goto error;
			}
			reply = db_redis_command_argv(con, query_v);
			db_redis_key_free(&query_v);
			db_redis_check_reply(con, reply, error);
			db_redis_free_reply(&reply);

			if(db_redis_key_add_string(&query_v, "SCARD", 5) != 0) {
				LM_ERR("Failed to add scard command to post-delete query\n");
				goto error;
			}
			if(db_redis_key_add_str(&query_v, &type_key->key) != 0) {
				LM_ERR("Failed to add key to delete query\n");
				goto error;
			}
			reply = db_redis_command_argv(con, query_v);
			db_redis_key_free(&query_v);
			db_redis_check_reply(con, reply, error);
			scard = reply->integer;
			db_redis_free_reply(&reply);

			if(scard != 0)
				continue;

			if(db_redis_key_add_string(&query_v, "SREM", 4) != 0) {
				LM_ERR("Failed to add srem command to post-delete query\n");
				goto error;
			}
			if(db_redis_key_add_str(&query_v, &set_key->key) != 0) {
				LM_ERR("Failed to add key to delete query\n");
				goto error;
			}
			if(db_redis_key_add_str(&query_v, &type_key->key) != 0) {
				LM_ERR("Failed to add key to delete query\n");
				goto error;
			}
			reply = db_redis_command_argv(con, query_v);
			db_redis_key_free(&query_v);
			db_redis_check_reply(con, reply, error);
#else
				if(db_redis_key_add_string(&query_v, "EVALSHA", 7) != 0) {
					LM_ERR("Failed to add srem command to post-delete query\n");
					goto error;
				}
				if(db_redis_key_add_string(&query_v, con->srem_key_lua,
						   strlen(con->srem_key_lua))
						!= 0) {
					LM_ERR("Failed to add srem command to post-delete query\n");
					goto error;
				}
				if(db_redis_key_add_string(&query_v, "3", 1) != 0) {
					LM_ERR("Failed to add srem command to post-delete query\n");
					goto error;
				}
				if(db_redis_key_add_str(&query_v, &type_key->key) != 0) {
					LM_ERR("Failed to add key to delete query\n");
					goto error;
				}
				if(db_redis_key_add_str(&query_v, &set_key->key) != 0) {
					LM_ERR("Failed to add key to delete query\n");
					goto error;
				}
				if(db_redis_key_add_str(&query_v, key) != 0) {
					LM_ERR("Failed to add key to delete query\n");
					goto error;
				}
				reply = db_redis_command_argv(con, query_v);
				db_redis_key_free(&query_v);
				db_redis_check_reply(con, reply, error);
				db_redis_free_reply(&reply);
#endif
		}
		LM_DBG("done with loop '%.*s'\n", k->key.len, k->key.s);
		db_redis_key_free(&type_keys);
		db_redis_key_free(&set_keys);
	}
	db_redis_key_free(&all_type_keys);
	db_redis_key_free(&query_v);

	return 0;

error:
	LM_ERR("failed to perform the delete\n");
	if(reply)
		db_redis_free_reply(&reply);
	if(db_keys)
		pkg_free(db_keys);
	if(db_vals)
		pkg_free(db_vals);
	db_redis_key_free(&query_v);
	db_redis_key_free(&type_keys);
	db_redis_key_free(&set_keys);
	db_redis_key_free(&all_type_keys);
	return -1;
}

static int db_redis_perform_update(const db1_con_t *_h, km_redis_con_t *con,
		const db_key_t *_k, const db_val_t *_v, const db_op_t *_op,
		const db_key_t *_uk, const db_val_t *_uv, const int _n, const int _nu,
		redis_key_t **keys, int *keys_count, int **manual_keys,
		int *manual_keys_count, int do_table_scan, uint64_t ts_scan_start,
		const str *ts_scan_key)
{

	redisReply *reply = NULL;
	redis_key_t *query_v = NULL;
	int update_queries = 0;
	redis_key_t *key;
	int i;
	int j;
	size_t col;
	redis_key_t *all_type_keys = NULL;
	int all_type_keys_count = 0;
	db_val_t *db_vals = NULL;
	db_key_t *db_keys = NULL;
	redis_key_t *type_keys = NULL;
	redis_key_t *set_keys = NULL;
	int type_keys_count = 0;
	redis_key_t *new_type_keys = NULL;
	int new_type_keys_count = 0;
	redis_key_t *all_type_key;
#ifdef WITH_HIREDIS_CLUSTER
	long long scard;
#endif

	if(!(*keys_count) && do_table_scan) {
		LM_WARN("performing full table scan on table '%.*s' while performing "
				"update\n",
				CON_TABLE(_h)->len, CON_TABLE(_h)->s);
		for(i = 0; i < _n; ++i) {
			LM_WARN("  scan key %d is '%.*s'\n", i, _k[i]->len, _k[i]->s);
		}
		if(db_redis_scan_query_keys(con, CON_TABLE(_h), _n, keys, keys_count,
				   manual_keys, manual_keys_count, ts_scan_start, ts_scan_key)
				!= 0) {
			LM_ERR("failed to scan query keys\n");
			goto error;
		}
	}

	// TODO: this should be moved to redis_connection structure
	// and be parsed at startup:
	//
	// fetch list of keys in all types
	if(db_redis_get_keys_for_all_types(
			   con, CON_TABLE(_h), &all_type_keys, &all_type_keys_count)
			!= 0) {
		LM_ERR("failed to get full list of type keys\n");
		goto error;
	}

	if(db_redis_build_type_keys(con, CON_TABLE(_h), _uk, _uv, _nu,
			   &new_type_keys, NULL, &new_type_keys_count)
			!= 0) {
		LM_ERR("failed to build type keys\n");
		goto error;
	}
	LM_DBG("%i new type keys\n", new_type_keys_count);

	for(key = *keys; key; key = key->next) {
		str *keyname = &key->key;

		LM_DBG("fetching row for '%.*s' from redis for update\n", keyname->len,
				keyname->s);


		if(db_redis_key_add_string(&query_v, "EXISTS", 6) != 0) {
			LM_ERR("Failed to set exists command to pre-update exists query\n");
			goto error;
		}
		if(db_redis_key_add_str(&query_v, keyname) != 0) {
			LM_ERR("Failed to add key name to pre-update exists query\n");
			goto error;
		}
		if(db_redis_append_command_argv(con, query_v, 1) != REDIS_OK) {
			LM_ERR("Failed to append redis command\n");
			goto error;
		}
		db_redis_key_free(&query_v);

		// construct HMGET query
		if(db_redis_key_add_string(&query_v, "HMGET", 5) != 0) {
			LM_ERR("Failed to set hgetall command to pre-update hget query\n");
			goto error;
		}
		if(db_redis_key_add_str(&query_v, keyname) != 0) {
			LM_ERR("Failed to add key name to pre-update exists query\n");
			goto error;
		}

		for(j = 0; j < *manual_keys_count; ++j) {
			int idx = (*manual_keys)[j];
			str *k_name = _k[idx];
			if(db_redis_key_add_str(&query_v, k_name) != 0) {
				LM_ERR("Failed to add manual key name to hget query\n");
				goto error;
			}
		}
		// add all type keys to query
		for(all_type_key = all_type_keys; all_type_key;
				all_type_key = all_type_key->next) {
			if(db_redis_key_add_str(&query_v, &all_type_key->key) != 0) {
				LM_ERR("Failed to add type key to pre-update query\n");
				goto error;
			}
		}

		if(db_redis_append_command_argv(con, query_v, 1) != REDIS_OK) {
			LM_ERR("Failed to append redis command\n");
			goto error;
		}
		db_redis_key_free(&query_v);
	}

	/*
    key_value = (str*)pkg_malloc(_nu * sizeof(str));
    if (!key_value) {
        LM_ERR("Failed to allocate memory for key buffer\n");
        goto error;
    }
    memset(key_value, 0, _nu * sizeof(str));

    col_value = (str*)pkg_malloc(_nu * sizeof(str));
    if (!col_value) {
        LM_ERR("Failed to allocate memory for column buffer\n");
        goto error;
    }
    memset(col_value, 0, _nu * sizeof(str));
    */


	for(key = *keys; key; key = key->next) {
		redis_key_t *tmp = NULL;
		redis_key_t *type_key;
		redis_key_t *set_key;
		redis_key_t *new_type_key;
		int row_match;

		LM_DBG("fetching replies for '%.*s' from redis for update\n",
				key->key.len, key->key.s);

		// get reply for EXISTS query
		if(db_redis_get_reply(con, (void **)&reply) != REDIS_OK) {
			LM_ERR("Failed to get reply for query: %s\n",
					db_redis_get_error(con));
			goto error;
		}
		db_redis_check_reply(con, reply, error);
		if(reply->integer == 0) {
			LM_DBG("key does not exist, returning no row for query\n");
			db_redis_free_reply(&reply);
			// also free next reply, as this is a null row for the HMGET
			LM_DBG("also fetch hmget reply after non-existent key\n");
			if(db_redis_get_reply(con, (void **)&reply) != REDIS_OK) {
				LM_ERR("Failed to get reply for query: %s\n",
						db_redis_get_error(con));
				goto error;
			}
			db_redis_check_reply(con, reply, error);
			db_redis_free_reply(&reply);
			LM_DBG("continue fetch reply loop\n");
			continue;
		}
		db_redis_free_reply(&reply);

		// get reply for actual HMGET query
		if(db_redis_get_reply(con, (void **)&reply) != REDIS_OK) {
			LM_ERR("Failed to get reply for query: %s\n",
					db_redis_get_error(con));
			goto error;
		}
		db_redis_check_reply(con, reply, error);
		if(reply->type != REDIS_REPLY_ARRAY) {
			LM_ERR("Unexpected reply, expected array\n");
			goto error;
		}
		LM_DBG("dumping full query reply for row\n");
		db_redis_dump_reply(reply);

		// manually filter non-matching replies
		row_match = 1;
		for(col = 0; col < reply->elements; ++col) {
			if(col < *manual_keys_count) {
				int idx = (*manual_keys)[col];
				db_key_t k = _k[idx];
				db_val_t v = _v[idx];
				db_op_t o = _op[idx];
				LM_DBG("manually filtering key '%.*s'\n", k->len, k->s);
				if(db_redis_compare_column(k, &v, o, reply->element[col])
						!= 0) {
					LM_DBG("column %lu does not match, ignore row\n",
							(unsigned long)col);
					row_match = 0;
					break;
				}
			}
		}
		if(!row_match) {
			continue;
		} else {
			LM_DBG("row matches manual filtering, proceed with update\n");
		}

		db_keys =
				(db_key_t *)pkg_malloc(all_type_keys_count * sizeof(db_key_t));
		if(!db_keys) {
			LM_ERR("Failed to allocate memory for db type keys\n");
			goto error;
		}
		for(j = 0, tmp = all_type_keys; tmp; ++j, tmp = tmp->next) {
			db_keys[j] = &tmp->key;
		}

		db_vals =
				(db_val_t *)pkg_malloc(all_type_keys_count * sizeof(db_val_t));
		if(!db_vals) {
			LM_ERR("Failed to allocate memory for manual db vals\n");
			goto error;
		}

		for(j = 0, all_type_key = all_type_keys; all_type_key;
				++j, all_type_key = all_type_key->next) {
			db_val_t *v = &(db_vals[j]);
			str *key = &all_type_key->key;
			char *value = reply->element[*manual_keys_count + j]->str;
			int coltype =
					db_redis_schema_get_column_type(con, CON_TABLE(_h), key);
			if(value == NULL) {
				VAL_NULL(v) = 1;
			} else if(db_str2val(coltype, v, value, strlen(value), 0) != 0) {
				LM_ERR("Failed to convert redis reply column to db value\n");
				goto error;
			}
		}
		if(db_redis_build_type_keys(con, CON_TABLE(_h), db_keys, db_vals,
				   all_type_keys_count, &type_keys, &set_keys, &type_keys_count)
				!= 0) {
			LM_ERR("failed to build type keys\n");
			pkg_free(db_vals);
			db_vals = NULL;
			goto error;
		}
		pkg_free(db_keys);
		db_keys = NULL;
		pkg_free(db_vals);
		db_vals = NULL;
		db_redis_free_reply(&reply);

		if(db_redis_key_add_string(&query_v, "HMSET", 5) != 0) {
			LM_ERR("Failed to add hmset command to update query\n");
			goto error;
		}
		if(db_redis_key_add_str(&query_v, &key->key) != 0) {
			LM_ERR("Failed to add key to update query\n");
			goto error;
		}

		for(i = 0; i < _nu; ++i) {
			str *k = _uk[i];
			str v = {NULL, 0};

			if(db_redis_val2str(&(_uv[i]), &v) != 0) {
				LM_ERR("Failed to convert update value to string\n");
				goto error;
			}
			if(db_redis_key_add_str(&query_v, k) != 0) {
				LM_ERR("Failed to add key to update query\n");
				goto error;
			}
			if(db_redis_key_add_str(&query_v, &v) != 0) {
				LM_ERR("Failed to add key to update query\n");
				goto error;
			}
			if(v.s)
				pkg_free(v.s);
		}
		update_queries++;
		if(db_redis_append_command_argv(con, query_v, 1) != REDIS_OK) {
			LM_ERR("Failed to append redis command\n");
			goto error;
		}

		db_redis_key_free(&query_v);

		for(type_key = type_keys, set_key = set_keys; type_key;
				type_key = type_key->next, set_key = set_key->next) {

			LM_DBG("checking for update of type key '%.*s'\n",
					type_key->key.len, type_key->key.s);
			char *prefix =
					ser_memmem(type_key->key.s, "::", type_key->key.len, 2);
			if(!prefix || prefix == type_key->key.s) {
				LM_DBG("Invalid key without :: '%.*s'\n", type_key->key.len,
						type_key->key.s);
				continue;
			}
			for(new_type_key = new_type_keys; new_type_key;
					new_type_key = new_type_key->next) {
				// compare prefix to see if this is the same key
				if(memcmp(new_type_key->key.s, type_key->key.s,
						   prefix - type_key->key.s))
					continue;
				LM_DBG("checking for update of type key against '%.*s'\n",
						new_type_key->key.len, new_type_key->key.s);
				if(!str_strcmp(&new_type_key->key, &type_key->key))
					continue;

				// add to new set key and delete from old

				if(db_redis_key_add_string(&query_v, "SADD", 4) != 0) {
					LM_ERR("Failed to set sadd command to post-update query\n");
					goto error;
				}
				if(db_redis_key_add_str(&query_v, &new_type_key->key) != 0) {
					LM_ERR("Failed to add map key to post-update query\n");
					goto error;
				}
				if(db_redis_key_add_str(&query_v, &key->key) != 0) {
					LM_ERR("Failed to set entry key to post-update query\n");
					goto error;
				}

				update_queries++;
				if(db_redis_append_command_argv(con, query_v, 1) != REDIS_OK) {
					LM_ERR("Failed to append redis command\n");
					goto error;
				}

				db_redis_key_free(&query_v);

				if(db_redis_key_add_string(&query_v, "SADD", 4) != 0) {
					LM_ERR("Failed to set sadd command to post-update query\n");
					goto error;
				}
				if(db_redis_key_add_str(&query_v, &set_key->key) != 0) {
					LM_ERR("Failed to add map key to post-update query\n");
					goto error;
				}
				if(db_redis_key_add_str(&query_v, &new_type_key->key) != 0) {
					LM_ERR("Failed to set entry key to post-update query\n");
					goto error;
				}

				update_queries++;
				if(db_redis_append_command_argv(con, query_v, 1) != REDIS_OK) {
					LM_ERR("Failed to append redis command\n");
					goto error;
				}

				db_redis_key_free(&query_v);

#ifdef WITH_HIREDIS_CLUSTER
				if(db_redis_key_add_string(&query_v, "SREM", 4) != 0) {
					LM_ERR("Failed to add srem command to post-delete query\n");
					goto error;
				}
				if(db_redis_key_add_str(&query_v, &type_key->key) != 0) {
					LM_ERR("Failed to add key to delete query\n");
					goto error;
				}
				if(db_redis_key_add_str(&query_v, key) != 0) {
					LM_ERR("Failed to add key to delete query\n");
					goto error;
				}
				reply = db_redis_command_argv(con, query_v);
				db_redis_key_free(&query_v);
				db_redis_check_reply(con, reply, error);
				db_redis_free_reply(&reply);

				if(db_redis_key_add_string(&query_v, "SCARD", 5) != 0) {
					LM_ERR("Failed to add scard command to post-delete "
						   "query\n");
					goto error;
				}
				if(db_redis_key_add_str(&query_v, &type_key->key) != 0) {
					LM_ERR("Failed to add key to delete query\n");
					goto error;
				}
				reply = db_redis_command_argv(con, query_v);
				db_redis_key_free(&query_v);
				db_redis_check_reply(con, reply, error);
				scard = reply->integer;
				db_redis_free_reply(&reply);

				if(scard != 0)
					continue;

				if(db_redis_key_add_string(&query_v, "SREM", 4) != 0) {
					LM_ERR("Failed to add srem command to post-delete query\n");
					goto error;
				}
				if(db_redis_key_add_str(&query_v, &set_key->key) != 0) {
					LM_ERR("Failed to add key to delete query\n");
					goto error;
				}
				if(db_redis_key_add_str(&query_v, &type_key->key) != 0) {
					LM_ERR("Failed to add key to delete query\n");
					goto error;
				}
				reply = db_redis_command_argv(con, query_v);
				db_redis_key_free(&query_v);
				db_redis_check_reply(con, reply, error);
				update_queries++;
#else
					if(db_redis_key_add_string(&query_v, "EVAL", 4) != 0) {
						LM_ERR("Failed to add srem command to post-delete "
							   "query\n");
						goto error;
					}
					if(db_redis_key_add_string(
							   &query_v, SREM_KEY_LUA, strlen(SREM_KEY_LUA))
							!= 0) {
						LM_ERR("Failed to add srem command to post-delete "
							   "query\n");
						goto error;
					}
					if(db_redis_key_add_string(&query_v, "3", 1) != 0) {
						LM_ERR("Failed to add srem command to post-delete "
							   "query\n");
						goto error;
					}
					if(db_redis_key_add_str(&query_v, &type_key->key) != 0) {
						LM_ERR("Failed to add key to delete query\n");
						goto error;
					}
					if(db_redis_key_add_str(&query_v, &set_key->key) != 0) {
						LM_ERR("Failed to add key to delete query\n");
						goto error;
					}
					if(db_redis_key_add_str(&query_v, &key->key) != 0) {
						LM_ERR("Failed to add key to delete query\n");
						goto error;
					}

					update_queries++;
					if(db_redis_append_command_argv(con, query_v, 1)
							!= REDIS_OK) {
						LM_ERR("Failed to append redis command\n");
						goto error;
					}

					db_redis_key_free(&query_v);
#endif
			}
		}

		db_redis_key_free(&type_keys);
		db_redis_key_free(&set_keys);
	}

	LM_DBG("getting replies for %d queries\n", update_queries);

	for(i = 0; i < update_queries; ++i) {
		if(db_redis_get_reply(con, (void **)&reply) != REDIS_OK) {
			LM_ERR("Failed to get reply for query: %s\n",
					db_redis_get_error(con));
			goto error;
		}
		db_redis_check_reply(con, reply, error);
		db_redis_free_reply(&reply);
	}

	LM_DBG("done performing update\n");

	db_redis_key_free(&all_type_keys);
	db_redis_key_free(&new_type_keys);
	db_redis_consume_replies(con);
	return 0;

error:
	LM_ERR("failed to perform the update\n");
	if(reply)
		db_redis_free_reply(&reply);
	db_redis_key_free(&query_v);
	db_redis_key_free(&all_type_keys);
	db_redis_key_free(&type_keys);
	db_redis_key_free(&set_keys);
	db_redis_key_free(&new_type_keys);
	db_redis_consume_replies(con);
	return -1;
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
int db_redis_query(const db1_con_t *_h, const db_key_t *_k, const db_op_t *_op,
		const db_val_t *_v, const db_key_t *_c, const int _n, const int _nc,
		const db_key_t _o, db1_res_t **_r)
{
	km_redis_con_t *con = NULL;
	int free_op = 0;
	int do_table_scan = 0;
	uint64_t ts_scan_start = 0;
	str ts_scan_key = {
			0,
	};

	redis_key_t *keys = NULL;
	int keys_count = 0;
	int *manual_keys = NULL;
	int manual_keys_count = 0;
	db_op_t *query_ops = NULL;
	int i;

	// TODO: implement order-by
	// TODO: optimize mapping-based manual post-check (remove check for keys already
	// in type query key)

	if(!_r) {
		LM_ERR("db result is null\n");
		return -1;
	}

	con = REDIS_CON(_h);
	if(con && con->con == NULL) {
		if(db_redis_connect(con) != 0) {
			LM_ERR("Failed to reconnect to server\n");
			return -1;
		}
	}
	if(con == NULL || con->id == NULL || con->con == NULL) {
		LM_ERR("connection to server is null\n");
		return -1;
	}
	if(CON_TABLE(_h)->s == NULL) {
		LM_ERR("prefix (table) name not set\n");
		return -1;
	} else {
		LM_DBG("querying prefix (table) '%.*s'\n", CON_TABLE(_h)->len,
				CON_TABLE(_h)->s);
	}

	*_r = NULL;

	// check if we have a version query, and return version directly from
	// schema instead of loading it from redis
	if(_nc == 1 && _n >= 1 && VAL_TYPE(&_v[0]) == DB1_STR
			&& CON_TABLE(_h)->len == strlen("version")
			&& strncmp(CON_TABLE(_h)->s, "version", CON_TABLE(_h)->len) == 0
			&& _k[0]->len == strlen("table_name")
			&& strncmp(_k[0]->s, "table_name", _k[0]->len) == 0
			&& _c[0]->len == strlen("table_version")
			&& strncmp(_c[0]->s, "table_version", _c[0]->len) == 0) {

		if(db_redis_return_version(_h, con, &VAL_STR(&_v[0]), _r) == 0) {
			return 0;
		}
		// if we fail to return a version from the schema, go query the table, just in case
	}

	free_op = 0;

	if(_op == NULL) {
		char *op = "=";
		free_op = 1;
		query_ops = (db_op_t *)pkg_malloc(_n * sizeof(db_op_t));
		if(!query_ops) {
			LM_ERR("Failed to allocate memory for query op list\n");
			goto error;
		}
		for(i = 0; i < _n; ++i) {
			query_ops[i] = op;
		}
	} else {
		query_ops = (db_op_t *)_op;
	}

	if(_n > 0) {
		if(db_redis_build_query_keys(con, CON_TABLE(_h), _k, _v, query_ops, _n,
				   &keys, &keys_count, &manual_keys, &manual_keys_count,
				   &do_table_scan, &ts_scan_start, &ts_scan_key)
				!= 0) {
			LM_ERR("failed to build query keys\n");
			goto error;
		}
		if(!keys_count) {
			if(do_table_scan) {
				LM_DBG("unable to build query keys, falling back to full table "
					   "scan\n");
			} else {
				LM_DBG("query keys not member of suitable mapping, skip full "
					   "table scan\n");
			}
		}
	} else {
		LM_DBG("no columns given to build query keys, falling back to full "
			   "table scan\n");
		keys_count = 0;
		do_table_scan = 1;
	}

	if(db_redis_perform_query(_h, con, _k, _v, query_ops, _c, _n, _nc, _r,
			   &keys, &keys_count, &manual_keys, &manual_keys_count,
			   do_table_scan, ts_scan_start, &ts_scan_key)
			!= 0) {
		goto error;
	}

	LM_DBG("done performing query\n");

	if(free_op && query_ops) {
		pkg_free(query_ops);
	}
	db_redis_key_free(&keys);

	if(manual_keys) {
		pkg_free(manual_keys);
	}
	if(ts_scan_key.s)
		pkg_free(ts_scan_key.s);

	db_redis_consume_replies(con);
	return 0;

error:
	LM_ERR("failed to do the query\n");
	if(free_op && query_ops) {
		pkg_free(query_ops);
	}
	db_redis_key_free(&keys);
	if(manual_keys) {
		pkg_free(manual_keys);
	}
	if(ts_scan_key.s)
		pkg_free(ts_scan_key.s);
	db_redis_consume_replies(con);


	return -1;
}

/*
 * Execute a raw SQL query
 */
int db_redis_raw_query(const db1_con_t *_h, const str *_s, db1_res_t **_r)
{
	LM_DBG("perform redis raw query\n");
	return -1;
}

/*
 * Insert a row into specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys
 * _n: number of key=value pairs
 */
int db_redis_insert(const db1_con_t *_h, const db_key_t *_k, const db_val_t *_v,
		const int _n)
{
	km_redis_con_t *con = NULL;
	redis_key_t *key = NULL;
	int keys_count = 0;
	redis_key_t *type_keys = NULL;
	redis_key_t *set_keys = NULL;
	int type_keys_count = 0;
	redis_key_t *query_v = NULL;
	redisReply *reply = NULL;
	int i;
	redis_key_t *k;
	redis_key_t *set_key;

	con = REDIS_CON(_h);
	if(con && con->con == NULL) {
		if(db_redis_connect(con) != 0) {
			LM_ERR("Failed to reconnect to server\n");
			return -1;
		}
	}
	if(con == NULL || con->id == NULL || con->con == NULL) {
		LM_ERR("connection to server is null\n");
		return -1;
	}
	if(CON_TABLE(_h)->s == NULL) {
		LM_ERR("prefix (table) name not set\n");
		return -1;
	} else {
		LM_DBG("inserting to prefix (table) '%.*s'\n", CON_TABLE(_h)->len,
				CON_TABLE(_h)->s);
	}

	if(db_redis_build_entry_keys(
			   con, CON_TABLE(_h), _k, _v, _n, &key, &keys_count)
			!= 0) {
		LM_ERR("failed to build entry keys\n");
		goto error;
	}
	if(db_redis_build_type_keys(con, CON_TABLE(_h), _k, _v, _n, &type_keys,
			   &set_keys, &type_keys_count)
			!= 0) {
		LM_ERR("failed to build type keys\n");
		goto error;
	}

	if(db_redis_key_add_string(&query_v, "HMSET", 5) != 0) {
		LM_ERR("Failed to add hmset command to insert query\n");
		goto error;
	}
	if(db_redis_key_add_str(&query_v, &key->key) != 0) {
		LM_ERR("Failed to add key to insert query\n");
		goto error;
	}

	for(i = 0; i < _n; ++i) {
		str *k = _k[i];
		str v;

		if(db_redis_key_add_str(&query_v, k) != 0) {
			LM_ERR("Failed to add column name to insert query\n");
			goto error;
		}
		if(db_redis_val2str(&(_v[i]), &v) != 0) {
			LM_ERR("Failed to allocate memory for query value\n");
			goto error;
		}
		if(db_redis_key_add_str(&query_v, &v) != 0) {
			LM_ERR("Failed to add column value to insert query\n");
			goto error;
		}
		if(v.s)
			pkg_free(v.s);
	}

	reply = db_redis_command_argv(con, query_v);
	db_redis_key_free(&query_v);
	db_redis_check_reply(con, reply, error);
	db_redis_free_reply(&reply);

	for(k = type_keys, set_key = set_keys; k;
			k = k->next, set_key = set_key->next) {
		str *type_key = &k->key;

		LM_DBG("inserting entry key '%.*s' to type map '%.*s'\n", key->key.len,
				key->key.s, type_key->len, type_key->s);

		if(db_redis_key_add_string(&query_v, "SADD", 4) != 0) {
			LM_ERR("Failed to set sadd command to post-insert query\n");
			goto error;
		}
		if(db_redis_key_add_str(&query_v, type_key) != 0) {
			LM_ERR("Failed to add map key to post-insert query\n");
			goto error;
		}
		if(db_redis_key_add_str(&query_v, &key->key) != 0) {
			LM_ERR("Failed to set entry key to post-insert query\n");
			goto error;
		}

		reply = db_redis_command_argv(con, query_v);
		db_redis_key_free(&query_v);
		db_redis_check_reply(con, reply, error);
		db_redis_free_reply(&reply);

		if(db_redis_key_add_string(&query_v, "SADD", 4) != 0) {
			LM_ERR("Failed to set sadd command to post-insert query\n");
			goto error;
		}
		if(db_redis_key_add_str(&query_v, &set_key->key) != 0) {
			LM_ERR("Failed to add map key to post-insert query\n");
			goto error;
		}
		if(db_redis_key_add_str(&query_v, type_key) != 0) {
			LM_ERR("Failed to set entry key to post-insert query\n");
			goto error;
		}

		reply = db_redis_command_argv(con, query_v);
		db_redis_key_free(&query_v);
		db_redis_check_reply(con, reply, error);
		db_redis_free_reply(&reply);
	}

	db_redis_key_free(&key);
	db_redis_key_free(&type_keys);
	db_redis_key_free(&set_keys);
	db_redis_consume_replies(con);

	return 0;

error:
	db_redis_key_free(&key);
	db_redis_key_free(&type_keys);
	db_redis_key_free(&set_keys);
	db_redis_key_free(&query_v);

	if(reply)
		db_redis_free_reply(&reply);

	db_redis_consume_replies(con);

	LM_ERR("failed to do the insert\n");
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
int db_redis_delete(const db1_con_t *_h, const db_key_t *_k, const db_op_t *_op,
		const db_val_t *_v, const int _n)
{
	km_redis_con_t *con = NULL;
	redis_key_t *keys = NULL;
	int keys_count = 0;
	int *manual_keys = NULL;
	int manual_keys_count = 0;
	int free_op = 0;
	int do_table_scan = 0;
	uint64_t ts_scan_start = 0;
	str ts_scan_key = {
			0,
	};
	db_op_t *query_ops = NULL;
	int i;

	// TODO: optimize mapping-based manual post-check (remove check for keys already
	// in type query key)

	con = REDIS_CON(_h);
	if(con && con->con == NULL) {
		if(db_redis_connect(con) != 0) {
			LM_ERR("Failed to reconnect to server\n");
			return -1;
		}
	}
	if(con == NULL || con->id == NULL || con->con == NULL) {
		LM_ERR("connection to server is null\n");
		return -1;
	}
	if(CON_TABLE(_h)->s == NULL) {
		LM_ERR("prefix (table) name not set\n");
		return -1;
	} else {
		LM_DBG("deleting from prefix (table) '%.*s'\n", CON_TABLE(_h)->len,
				CON_TABLE(_h)->s);
	}

	free_op = 0;

	if(_op == NULL) {
		char *op = "=";
		free_op = 1;
		query_ops = (db_op_t *)pkg_malloc(_n * sizeof(db_op_t));
		if(!query_ops) {
			LM_ERR("Failed to allocate memory for query op list\n");
			goto error;
		}
		for(i = 0; i < _n; ++i) {
			query_ops[i] = op;
		}
	} else {
		query_ops = (db_op_t *)_op;
	}

	if(_n > 0) {
		if(db_redis_build_query_keys(con, CON_TABLE(_h), _k, _v, query_ops, _n,
				   &keys, &keys_count, &manual_keys, &manual_keys_count,
				   &do_table_scan, &ts_scan_start, &ts_scan_key)
				!= 0) {
			LM_ERR("failed to build query keys\n");
			goto error;
		}
		if(!keys_count) {
			if(do_table_scan) {
				LM_DBG("unable to build query keys, falling back to full table "
					   "scan\n");
			} else {
				LM_DBG("query keys not member of suitable mapping, skip full "
					   "table scan\n");
			}
		}
	} else {
		LM_DBG("no columns given to build query keys, falling back to full "
			   "table scan\n");
		keys_count = 0;
		do_table_scan = 1;
	}

	if(db_redis_perform_delete(_h, con, _k, _v, query_ops, _n, &keys,
			   &keys_count, &manual_keys, &manual_keys_count, do_table_scan,
			   ts_scan_start, &ts_scan_key)
			!= 0) {
		goto error;
	}

	LM_DBG("done performing delete\n");

	if(free_op && query_ops) {
		pkg_free(query_ops);
	}
	db_redis_key_free(&keys);
	if(manual_keys)
		pkg_free(manual_keys);
	if(ts_scan_key.s)
		pkg_free(ts_scan_key.s);
	db_redis_consume_replies(con);

	return 0;

error:
	LM_ERR("failed to do the query\n");
	if(free_op && query_ops) {
		pkg_free(query_ops);
	}
	db_redis_key_free(&keys);
	if(manual_keys)
		pkg_free(manual_keys);
	if(ts_scan_key.s)
		pkg_free(ts_scan_key.s);
	db_redis_consume_replies(con);
	return -1;
}

/*
 * Update some rows in the specified table
 * _h: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _uk: updated columns
 * _uv: updated values of the columns
 * _n: number of key=value pairs
 * _un: number of columns to update
 */
int db_redis_update(const db1_con_t *_h, const db_key_t *_k, const db_op_t *_op,
		const db_val_t *_v, const db_key_t *_uk, const db_val_t *_uv,
		const int _n, const int _nu)
{
	km_redis_con_t *con = NULL;
	int free_op = 0;
	int do_table_scan = 0;
	uint64_t ts_scan_start = 0;
	str ts_scan_key = {
			0,
	};

	redis_key_t *keys = NULL;
	int keys_count = 0;
	int *manual_keys = NULL;
	int manual_keys_count = 0;
	db_op_t *query_ops = NULL;
	int i;

	// TODO: optimize mapping-based manual post-check (remove check for keys already
	// in type query key)

	con = REDIS_CON(_h);
	if(con && con->con == NULL) {
		if(db_redis_connect(con) != 0) {
			LM_ERR("Failed to reconnect to server\n");
			return -1;
		}
	}
	if(con == NULL || con->id == NULL || con->con == NULL) {
		LM_ERR("connection to server is null\n");
		return -1;
	}
	if(CON_TABLE(_h)->s == NULL) {
		LM_ERR("prefix (table) name not set\n");
		return -1;
	} else {
		LM_DBG("updating prefix (table) '%.*s'\n", CON_TABLE(_h)->len,
				CON_TABLE(_h)->s);
	}

	free_op = 0;

	if(_op == NULL) {
		char *op = "=";
		free_op = 1;
		query_ops = (db_op_t *)pkg_malloc(_n * sizeof(db_op_t));
		if(!query_ops) {
			LM_ERR("Failed to allocate memory for query op list\n");
			goto error;
		}
		for(i = 0; i < _n; ++i) {
			query_ops[i] = op;
		}
	} else {
		query_ops = (db_op_t *)_op;
	}

	if(_n > 0) {
		if(db_redis_build_query_keys(con, CON_TABLE(_h), _k, _v, query_ops, _n,
				   &keys, &keys_count, &manual_keys, &manual_keys_count,
				   &do_table_scan, &ts_scan_start, &ts_scan_key)
				!= 0) {
			LM_ERR("failed to build query keys\n");
			goto error;
		}
		if(!keys_count) {
			if(do_table_scan) {
				LM_DBG("unable to build query keys, falling back to full table "
					   "scan\n");
			} else {
				LM_DBG("query keys not member of suitable mapping, skip full "
					   "table scan\n");
			}
		}
	} else {
		LM_DBG("no columns given to build query keys, falling back to full "
			   "table scan\n");
		keys_count = 0;
	}

	if(db_redis_perform_update(_h, con, _k, _v, query_ops, _uk, _uv, _n, _nu,
			   &keys, &keys_count, &manual_keys, &manual_keys_count,
			   do_table_scan, ts_scan_start, &ts_scan_key)
			!= 0) {
		goto error;
	}

	LM_DBG("done performing update\n");

	if(free_op && query_ops) {
		pkg_free(query_ops);
	}
	db_redis_key_free(&keys);

	if(manual_keys) {
		pkg_free(manual_keys);
	}
	if(ts_scan_key.s)
		pkg_free(ts_scan_key.s);
	db_redis_consume_replies(con);
	return 0;

error:
	LM_ERR("failed to do the query\n");
	if(free_op && query_ops) {
		pkg_free(query_ops);
	}
	db_redis_key_free(&keys);
	if(manual_keys) {
		pkg_free(manual_keys);
	}
	if(ts_scan_key.s)
		pkg_free(ts_scan_key.s);
	db_redis_consume_replies(con);
	return -1;
}

/*
 * Just like insert, but replace the row if it exists
 */
int db_redis_replace(const db1_con_t *_h, const db_key_t *_k,
		const db_val_t *_v, const int _n, const int _un, const int _m)
{
	LM_DBG("perform redis replace\n");
	return -1;
}

/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_redis_use_table(db1_con_t *_h, const str *_t)
{
	return db_use_table(_h, _t);
}

int db_redis_free_result(db1_con_t *_h, db1_res_t *_r)
{
	LM_DBG("perform redis free result\n");

	if(!_r)
		return -1;
	db_free_result(_r);
	return 0;
}

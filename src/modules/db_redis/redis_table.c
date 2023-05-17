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
 */

#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>

#include "db_redis_mod.h"
#include "redis_connection.h"
#include "redis_table.h"

int db_redis_key_add_string(redis_key_t **list, const char *entry, int len)
{
	redis_key_t *k;


	k = (redis_key_t *)pkg_malloc(sizeof(redis_key_t));
	if(!k) {
		LM_ERR("Failed to allocate memory for key list entry\n");
		goto err;
	}
	k->next = NULL;

	k->key.s = (char *)pkg_malloc((len + 1) * sizeof(char));
	if(!k->key.s) {
		LM_ERR("Failed to allocate memory for key list entry\n");
		goto err;
	}

	memcpy(k->key.s, entry, len);
	k->key.s[len] = '\0';
	k->key.len = len;

	if(!*list) {
		*list = k;
	} else {
		redis_key_t *l = *list;
		while(l->next)
			l = l->next;
		l->next = k;
	}

	return 0;

err:
	if(k)
		pkg_free(k);
	return -1;
}

int db_redis_key_add_str(redis_key_t **list, const str *entry)
{
	return db_redis_key_add_string(list, entry->s, entry->len);
}

int db_redis_key_prepend_string(redis_key_t **list, const char *entry, int len)
{
	redis_key_t *k;

	k = (redis_key_t *)pkg_malloc(sizeof(redis_key_t));
	if(!k) {
		LM_ERR("Failed to allocate memory for key list entry\n");
		goto err;
	}
	k->next = NULL;

	k->key.s = (char *)pkg_malloc((len + 1) * sizeof(char));
	if(!k->key.s) {
		LM_ERR("Failed to allocate memory for key list entry\n");
		goto err;
	}

	memset(k->key.s, 0, len + 1);
	strncpy(k->key.s, entry, len);
	k->key.len = len;

	if(!*list) {
		*list = k;
	} else {
		k->next = *list;
		*list = k;
	}

	return 0;

err:
	if(k)
		pkg_free(k);
	return -1;
}

redis_key_t *db_redis_key_shift(redis_key_t **list)
{
	redis_key_t *k;

	k = *list;
	*list = (*list)->next;
	k->next = NULL;
	return k;
}

void db_redis_key_free(redis_key_t **list)
{
	redis_key_t *l;
	redis_key_t **head;

	if(!list || !*list) {
		return;
	}
	head = list;
	do {
		l = (*list)->next;
		if((*list)->key.s) {
			pkg_free((*list)->key.s);
			(*list)->key.s = NULL;
			(*list)->key.len = 0;
		}
		pkg_free(*list);
		*list = l;
	} while(l);
	*head = NULL;
}

int db_redis_key_list2arr(redis_key_t *list, char ***arr)
{
	int len = 0, i = 0;
	redis_key_t *tmp = NULL;

	*arr = NULL;
	for(tmp = list, len = 0; tmp; tmp = tmp->next, len++)
		;
	if(len < 1) {
		return 0;
	}

	*arr = (char **)pkg_malloc(len * sizeof(char *));
	if(!*arr) {
		LM_ERR("Failed to allocate memory for array\n");
		return -1;
	}
	for(tmp = list, i = 0; tmp; tmp = tmp->next, i++) {
		(*arr)[i] = tmp->key.s;
	}
	LM_DBG("returning %d entries\n", len);

	return len;
}


int db_redis_schema_get_column_type(
		km_redis_con_t *con, const str *table_name, const str *col_name)
{
	struct str_hash_entry *table_e;
	struct str_hash_entry *col_e;
	redis_table_t *table;

	table_e = str_hash_get(&con->tables, table_name->s, table_name->len);
	if(!table_e) {
		LM_ERR("Failed to find table '%.*s' in table hash\n", table_name->len,
				table_name->s);
		return -1;
	}
	table = (redis_table_t *)table_e->u.p;
	col_e = str_hash_get(&table->columns, col_name->s, col_name->len);
	if(!col_e) {
		LM_ERR("Failed to find column '%.*s' in schema for table '%.*s'\n",
				col_name->len, col_name->s, table_name->len, table_name->s);
		return -1;
	}
	return col_e->u.n;
}

void db_redis_print_all_tables(km_redis_con_t *con)
{
	struct str_hash_table *ht;
	struct str_hash_table *col_ht;
	struct str_hash_entry *he;
	struct str_hash_entry *col_he;
	struct str_hash_entry *last;
	struct str_hash_entry *col_last;
	redis_table_t *table;
	redis_key_t *key;
	redis_type_t *type;
	int i, j;

	LM_DBG("dumping all redis tables:\n");
	ht = &con->tables;

	for(i = 0; i < ht->size; ++i) {
		last = (&ht->table[i])->prev;
		clist_foreach(&ht->table[i], he, next)
		{
			LM_DBG("  table %.*s\n", he->key.len, he->key.s);
			table = (redis_table_t *)he->u.p;

			LM_DBG("    schema:\n");
			col_ht = &table->columns;
			for(j = 0; j < col_ht->size; ++j) {
				col_last = (&col_ht->table[j])->prev;
				clist_foreach(&col_ht->table[j], col_he, next)
				{
					LM_DBG("      %.*s: %d\n", col_he->key.len, col_he->key.s,
							col_he->u.n);
					if(col_he == col_last)
						break;
				}
			}

			LM_DBG("    entry keys:\n");
			key = table->entry_keys;
			while(key) {
				LM_DBG("      %.*s\n", key->key.len, key->key.s);
				key = key->next;
			}

			type = table->types;
			while(type) {
				LM_DBG("    %.*s keys:\n", type->type.len, type->type.s);
				key = type->keys;
				while(key) {
					LM_DBG("      %.*s\n", key->key.len, key->key.s);
					key = key->next;
				}
				type = type->next;
			}

			if(he == last)
				break;
		}
	}
}

void db_redis_print_table(km_redis_con_t *con, char *name)
{
	str table_name;
	struct str_hash_entry *table_entry;
	redis_table_t *table;
	redis_key_t *key;
	redis_type_t *type;
	int j;

	struct str_hash_table *col_ht;
	struct str_hash_entry *col_he;
	struct str_hash_entry *col_last;

	table_name.s = name;
	table_name.len = strlen(name);

	table_entry = str_hash_get(&con->tables, table_name.s, table_name.len);
	if(!table_entry) {
		LM_ERR("Failed to print table '%.*s', no such table entry found\n",
				table_name.len, table_name.s);
		return;
	}

	table = (redis_table_t *)table_entry->u.p;
	LM_DBG("table %.*s:\n", table_name.len, table_name.s);

	LM_DBG("  schema:\n");
	col_ht = &table->columns;
	for(j = 0; j < col_ht->size; ++j) {
		col_last = (&col_ht->table[j])->prev;
		clist_foreach(&col_ht->table[j], col_he, next)
		{
			LM_DBG("    %.*s: %d\n", col_he->key.len, col_he->key.s,
					col_he->u.n);
			if(col_he == col_last)
				break;
		}
	}

	LM_DBG("  entry keys:\n");
	key = table->entry_keys;
	while(key) {
		LM_DBG("    %.*s\n", key->key.len, key->key.s);
		key = key->next;
	}

	type = table->types;
	while(type) {
		LM_DBG("  %.*s keys:\n", type->type.len, type->type.s);
		key = type->keys;
		while(key) {
			LM_DBG("    %.*s\n", key->key.len, key->key.s);
			key = key->next;
		}
		type = type->next;
	}
}

void db_redis_free_tables(km_redis_con_t *con)
{
	// TODO: also free schema hash?
	struct str_hash_table *ht;
	struct str_hash_table *col_ht;
	struct str_hash_entry *he;
	struct str_hash_entry *he_b;
	struct str_hash_entry *col_he;
	struct str_hash_entry *col_he_b;
	struct str_hash_entry *last;
	struct str_hash_entry *col_last;
	redis_table_t *table;
	redis_key_t *key, *tmpkey;
	redis_type_t *type, *tmptype;
	int i, j;

	ht = &con->tables;
	for(i = 0; i < ht->size; ++i) {
		last = (&ht->table[i])->prev;
		clist_foreach_safe(&ht->table[i], he, he_b, next)
		{
			table = (redis_table_t *)he->u.p;

			col_ht = &table->columns;
			for(j = 0; j < col_ht->size; ++j) {
				col_last = (&col_ht->table[j])->prev;
				clist_foreach_safe(&col_ht->table[j], col_he, col_he_b, next)
				{
					pkg_free(col_he->key.s);
					if(col_he == col_last) {
						pkg_free(col_he);
						break;
					} else {
						pkg_free(col_he);
					}
				}
			}
			pkg_free(col_ht->table);

			key = table->entry_keys;
			while(key) {
				tmpkey = key;
				key = key->next;
				pkg_free(tmpkey);
			}

			type = table->types;
			while(type) {
				key = type->keys;
				while(key) {
					tmpkey = key;
					key = key->next;
					pkg_free(tmpkey);
				}
				tmptype = type;
				type = type->next;
				pkg_free(tmptype);
			}
			pkg_free(table);
			pkg_free(he->key.s);
			if(he == last) {
				pkg_free(he);
				break;
			} else {
				pkg_free(he);
			}
		}
	}
	pkg_free(ht->table);
}

static redis_key_t *db_redis_create_key(str *key)
{
	redis_key_t *e;
	e = (redis_key_t *)pkg_malloc(sizeof(redis_key_t));
	if(!e) {
		LM_ERR("Failed to allocate memory for key entry\n");
		return NULL;
	}
	memset(e, 0, sizeof(redis_key_t));
	e->key.s = key->s;
	e->key.len = key->len;
	return e;
}

static redis_type_t *db_redis_create_type(str *type)
{
	redis_type_t *e;
	e = (redis_type_t *)pkg_malloc(sizeof(redis_type_t));
	if(!e) {
		LM_ERR("Failed to allocate memory for table type\n");
		return NULL;
	}
	e->type.s = type->s;
	e->type.len = type->len;
	e->next = NULL;
	e->keys = NULL;
	return e;
}

static struct str_hash_entry *db_redis_create_table(str *table)
{
	struct str_hash_entry *e;
	redis_table_t *t;

	LM_DBG("creating schema hash entry for table '%.*s'", table->len, table->s);

	e = (struct str_hash_entry *)pkg_malloc(sizeof(struct str_hash_entry));
	if(!e) {
		LM_ERR("Failed to allocate memory for table entry\n");
		return NULL;
	}
	memset(e, 0, sizeof(struct str_hash_entry));

	if(pkg_str_dup(&e->key, table) != 0) {
		LM_ERR("Failed to allocate memory for table name\n");
		pkg_free(e);
		return NULL;
	}
	e->flags = 0;

	t = (redis_table_t *)pkg_malloc(sizeof(redis_table_t));
	if(!t) {
		LM_ERR("Failed to allocate memory for table data\n");
		pkg_free(e->key.s);
		pkg_free(e);
		return NULL;
	}
	t->entry_keys = NULL;
	t->types = NULL;

	if(str_hash_alloc(&t->columns, REDIS_HT_SIZE) != 0) {
		LM_ERR("Failed to allocate memory for table schema hashtable\n");
		pkg_free(e->key.s);
		pkg_free(e);
		pkg_free(t);
		return NULL;
	}
	str_hash_init(&t->columns);

	e->u.p = t;
	return e;
}

static struct str_hash_entry *db_redis_create_column(str *col, str *type)
{
	struct str_hash_entry *e;
	e = (struct str_hash_entry *)pkg_malloc(sizeof(struct str_hash_entry));
	if(!e) {
		LM_ERR("Failed to allocate memory for column entry\n");
		return NULL;
	}
	if(pkg_str_dup(&e->key, col) != 0) {
		LM_ERR("Failed to allocate memory for column name\n");
		pkg_free(e);
		return NULL;
	}
	e->flags = 0;
	switch(type->s[0]) {
		case 's':
		case 'S':
			e->u.n = DB1_STRING;
			break;
		case 'i':
		case 'I':
			e->u.n = DB1_INT;
			break;
		case 'u':
		case 'U':
			/* uint and ubigint */
			if(type->len > 1 && (type->s[1] == 'b' || type->s[1] == 'B')) {
				e->u.n = DB1_UBIGINT;
			} else {
				e->u.n = DB1_UINT;
			}
			break;
		case 't':
		case 'T':
			e->u.n = DB1_DATETIME;
			break;
		case 'd':
		case 'D':
			e->u.n = DB1_DOUBLE;
			break;
		case 'b':
		case 'B':
			/* blob and bigint */
			if(type->len > 1 && (type->s[1] == 'i' || type->s[1] == 'I')) {
				e->u.n = DB1_BIGINT;
			} else {
				e->u.n = DB1_BLOB;
			}
			break;
		default:
			LM_ERR("Invalid schema column type '%.*s', expecting one of "
				   "string, int, timestamp, double, blob\n",
					type->len, type->s);
			pkg_free(e->key.s);
			pkg_free(e);
			return NULL;
	}
	return e;
}

int db_redis_parse_keys(km_redis_con_t *con)
{
	char *p, *q;
	char *start;
	char *end;

	str table_name = str_init("");
	str type_name;
	str column_name;
	str version_code;

	struct str_hash_entry *table_entry = NULL;
	redis_table_t *table = NULL;
	redis_type_t *type = NULL;
	redis_type_t *type_target = NULL;
	redis_key_t *key = NULL;
	redis_key_t **key_target = NULL;
	redis_key_t *key_location = NULL;

	enum
	{
		DBREDIS_KEYS_TABLE_ST,
		DBREDIS_KEYS_TYPE_ST,
		DBREDIS_KEYS_COLUMN_ST,
		DBREDIS_KEYS_END_ST
	} state;

	if(!redis_keys.len) {
		LM_ERR("Failed to parse empty 'keys' mod-param, please define it!\n");
		return -1;
	}

	type_target = NULL;
	key_location = NULL;
	end = redis_keys.s + redis_keys.len;
	p = start = redis_keys.s;
	state = DBREDIS_KEYS_TABLE_ST;
	do {
		type = NULL;
		key = NULL;
		switch(state) {
			case DBREDIS_KEYS_TABLE_ST:
				while(p != end && *p != '=')
					++p;
				if(p == end) {
					LM_ERR("Invalid table definition, expecting "
						   "<table>=<definition>\n");
					goto err;
				}
				table_name.s = start;
				table_name.len = p - start;

				version_code = (str){"", 0};
				q = memchr(table_name.s, ':', table_name.len);
				if(q) {
					version_code = table_name;
					version_code.len = q - table_name.s + 1;
					table_name.s = q + 1;
					table_name.len -= version_code.len;
				}

				state = DBREDIS_KEYS_TYPE_ST;
				start = ++p;
				LM_DBG("found table name '%.*s'\n", table_name.len,
						table_name.s);

				table_entry = str_hash_get(
						&con->tables, table_name.s, table_name.len);
				if(!table_entry) {
					LM_ERR("No table schema found for table '%.*s', fix config"
						   " by adding one to the 'schema' mod-param!\n",
							table_name.len, table_name.s);
					goto err;
				}
				table = table_entry->u.p;
				table->version_code = version_code;
				break;
			case DBREDIS_KEYS_TYPE_ST:
				if(!table) {
					LM_ERR("invalid definition, table not set\n");
					goto err;
				}
				while(p != end && *p != ':')
					++p;
				if(p == end) {
					LM_ERR("Invalid type definition, expecting "
						   "<type>:<definition>\n");
					goto err;
				}
				type_name.s = start;
				type_name.len = p - start;
				state = DBREDIS_KEYS_COLUMN_ST;
				start = ++p;
				LM_DBG("found type name '%.*s' for table '%.*s'\n",
						type_name.len, type_name.s, table_name.len,
						table_name.s);
				if(type_name.len == REDIS_DIRECT_PREFIX_LEN
						&& !strncmp(type_name.s, REDIS_DIRECT_PREFIX,
								type_name.len)) {
					key_target = &table->entry_keys;
				} else {
					type = db_redis_create_type(&type_name);
					if(!type)
						goto err;
					if(!table->types) {
						table->types = type_target = type;
					} else {
						if(!type_target) {
							LM_ERR("Internal error accessing null "
								   "type_target\n");
							goto err;
						}
						type_target->next = type;
						type_target = type_target->next;
					}
					key_target = &type->keys;
				}
				break;
			case DBREDIS_KEYS_COLUMN_ST:
				while(p != end && *p != ',' && *p != '&' && *p != ';')
					++p;
				if(p == end) {
					state = DBREDIS_KEYS_END_ST;
				} else if(*p == ',') {
					state = DBREDIS_KEYS_COLUMN_ST;
				} else if(*p == '&') {
					state = DBREDIS_KEYS_TYPE_ST;
				} else if(*p == ';') {
					state = DBREDIS_KEYS_TABLE_ST;
				}
				column_name.s = start;
				column_name.len = p - start;
				start = ++p;

				if(!column_name.len)
					break;

				/*
                LM_DBG("found column name '%.*s' in type '%.*s' for table '%.*s'\n",
                        column_name.len, column_name.s,
                        type_name.len, type_name.s,
                        table_name.len, table_name.s);
                */
				key = db_redis_create_key(&column_name);
				if(!key)
					goto err;
				if(*key_target == NULL) {
					*key_target = key_location = key;
				} else {
					if(!key_location) {
						LM_ERR("Internal error, null key_location pointer\n");
						goto err;
					}
					key_location->next = key;
					key_location = key_location->next;
				}
				break;
			case DBREDIS_KEYS_END_ST:
				LM_DBG("done parsing keys definition\n");
				return 0;
		}
	} while(p != end);

	return 0;

err:
	if(type)
		pkg_free(type);
	if(key)
		pkg_free(key);
	db_redis_free_tables(con);
	return -1;
}


int db_redis_parse_schema(km_redis_con_t *con)
{
	DIR *srcdir;
	FILE *fin;
	struct dirent *dent;
	char *dir_name;

	str table_name = str_init("");
	str column_name;
	str type_name;

	struct str_hash_entry *table_entry = NULL;
	struct str_hash_entry *column_entry = NULL;
	redis_table_t *table = NULL;

	char full_path[_POSIX_PATH_MAX + 1];
	int path_len;
	struct stat fstat;
	unsigned char c;
	int cc;

	enum
	{
		DBREDIS_SCHEMA_COLUMN_ST,
		DBREDIS_SCHEMA_TYPE_ST,
		DBREDIS_SCHEMA_VERSION_ST,
		DBREDIS_SCHEMA_END_ST,
	} state;

	char buf[4096];
	char *bufptr;


	srcdir = NULL;
	fin = NULL;
	dir_name = NULL;

	//LM_DBG("parsing schema '%.*s'\n", redis_schema.len, redis_schema.s);
	if(!redis_schema_path.len) {
		LM_ERR("Failed to parse empty 'schema_path' mod-param, please define "
			   "it!\n");
		return -1;
	}

	dir_name = (char *)pkg_malloc((redis_schema_path.len + 1) * sizeof(char));
	if(!dir_name) {
		LM_ERR("Failed to allocate memory for schema directory name\n");
		goto err;
	}
	strncpy(dir_name, redis_schema_path.s, redis_schema_path.len);
	dir_name[redis_schema_path.len] = '\0';
	srcdir = opendir(dir_name);
	if(!srcdir) {
		LM_ERR("Failed to open schema directory '%s'\n", dir_name);
		goto err;
	}

	if(str_hash_alloc(&con->tables, REDIS_HT_SIZE) != 0) {
		LM_ERR("Failed to allocate memory for tables hashtable\n");
		goto err;
	}
	str_hash_init(&con->tables);

	while((dent = readdir(srcdir)) != NULL) {
		path_len = redis_schema_path.len;
		if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0) {
			continue;
		}
		if((path_len + strlen(dent->d_name) + 1) > _POSIX_PATH_MAX) {
			LM_WARN("Redis schema path '%.*s/%s' is too long, skipping...\n",
					redis_schema_path.len, redis_schema_path.s, dent->d_name);
			continue;
		}
		snprintf(full_path, sizeof(full_path), "%s/%s", dir_name, dent->d_name);

		if(stat(full_path, &fstat) < 0) {
			LM_ERR("Failed to stat schema file %s\n", full_path);
			continue;
		}
		if(!S_ISREG(fstat.st_mode)) {
			LM_DBG("skipping schema file '%s' as it's not a regular file\n",
					full_path);
			continue;
		}

		LM_DBG("reading schema full path '%s'\n", full_path);

		fin = fopen(full_path, "r");
		if(!fin) {
			LM_ERR("Failed to open redis schema file '%s'\n", full_path);
			continue;
		}

		table_name.s = dent->d_name;
		table_name.len = strlen(table_name.s);
		table_entry = str_hash_get(&con->tables, table_name.s, table_name.len);
		if(table_entry) {
			// should not happen, as this would require two files with same name
			LM_WARN("Found duplicate table schema definition '%.*s', "
					"skipping...\n",
					table_name.len, table_name.s);
			fclose(fin);
			continue;
		}
		table_entry = db_redis_create_table(&table_name);
		if(!table_entry)
			goto err;
		str_hash_add(&con->tables, table_entry);
		table = table_entry->u.p;

		state = DBREDIS_SCHEMA_COLUMN_ST;
		memset(buf, 0, sizeof(buf));
		bufptr = buf;
		do {
			if(bufptr - buf > sizeof(buf)) {
				LM_ERR("Schema line too long in file %s\n", full_path);
				goto err;
			}

			cc = fgetc(fin);
			c = (unsigned char)cc;

			if(c == '\r')
				continue;
			//LM_DBG("parsing char %c, buf is '%s' at pos %lu\n", c, buf, bufpos);
			switch(state) {
				case DBREDIS_SCHEMA_COLUMN_ST:
					if(cc == EOF) {
						LM_ERR("Unexpected end of file in schema column name "
							   "of file %s\n",
								full_path);
						goto err;
					}
					if(c != '\n' && c != '/') {
						*bufptr = c;
						bufptr++;
						continue;
					}
					if(c == '\n') {
						if(bufptr == buf) {
							// trailing comma, skip
							state = DBREDIS_SCHEMA_VERSION_ST;
							continue;
						} else {
							LM_ERR("Invalid column definition, expecting "
								   "<column>/<type>\n");
							goto err;
						}
					}
					column_name.s = buf;
					column_name.len = bufptr - buf;
					bufptr++;
					state = DBREDIS_SCHEMA_TYPE_ST;
					LM_DBG("found column name '%.*s'\n", column_name.len,
							column_name.s);
					break;
				case DBREDIS_SCHEMA_TYPE_ST:
					if(cc == EOF) {
						LM_ERR("Unexpected end of file in schema column type "
							   "of file %s\n",
								full_path);
						goto err;
					}
					if(c != '\n' && c != ',') {
						*bufptr = c;
						bufptr++;
						continue;
					}
					type_name.s = buf + column_name.len + 1;
					type_name.len = bufptr - type_name.s;

					if(c == '\n') {
						state = DBREDIS_SCHEMA_VERSION_ST;
					} else {
						state = DBREDIS_SCHEMA_COLUMN_ST;
					}
					/*
                    LM_DBG("found column type '%.*s' with len %d for column name '%.*s' in table '%.*s'\n",
                            type_name.len, type_name.s,
                            type_name.len,
                            column_name.len, column_name.s,
                            table_name.len, table_name.s);
                    */
					column_entry = str_hash_get(
							&table->columns, column_name.s, column_name.len);
					if(column_entry) {
						LM_ERR("Found duplicate column definition '%.*s' in "
							   "schema definition of table '%.*s', remove one "
							   "from schema!\n",
								column_name.len, column_name.s, table_name.len,
								table_name.s);
						goto err;
					}
					column_entry =
							db_redis_create_column(&column_name, &type_name);
					if(!column_entry) {
						goto err;
					}
					str_hash_add(&table->columns, column_entry);
					memset(buf, 0, sizeof(buf));
					bufptr = buf;
					break;
				case DBREDIS_SCHEMA_VERSION_ST:
					if(c != '\n' && cc != EOF) {
						*bufptr = c;
						bufptr++;
						continue;
					}
					*bufptr = '\0';
					table->version = atoi(buf);
					state = DBREDIS_SCHEMA_END_ST;
					break;
				case DBREDIS_SCHEMA_END_ST:
					goto fileend;
					break;
			}
		} while(cc != EOF);

	fileend:
		fclose(fin);
		fin = NULL;
	}


	closedir(srcdir);
	pkg_free(dir_name);

	return 0;
err:
	if(fin)
		fclose(fin);
	if(srcdir)
		closedir(srcdir);
	if(dir_name)
		pkg_free(dir_name);

	db_redis_free_tables(con);
	return -1;
}

int db_redis_keys_spec(char *spec)
{
	size_t len = strlen(spec);

	if(redis_keys.len == 0) {
		redis_keys.s = (char *)pkg_malloc(len * sizeof(char));
		if(!redis_keys.s) {
			LM_ERR("Failed to allocate memory for keys spec\n");
			goto err;
		}
	} else {
		redis_keys.s =
				(char *)pkg_realloc(redis_keys.s, redis_keys.len + 1 + len);
		if(!redis_keys.s) {
			LM_ERR("Failed to reallocate memory for keys spec\n");
			goto err;
		}
		redis_keys.s[redis_keys.len] = ';';
		redis_keys.len++;
	}

	strncpy(redis_keys.s + redis_keys.len, spec, len);
	redis_keys.len += len;

	return 0;

err:
	if(redis_keys.len) {
		pkg_free(redis_keys.s);
	}
	return -1;
}

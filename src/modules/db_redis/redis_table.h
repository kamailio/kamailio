/**
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


#ifndef _REDIS_TABLE_H_
#define _REDIS_TABLE_H_

#include "db_redis_mod.h"
#include "redis_connection.h"

typedef struct redis_key redis_key_t;
struct redis_key {
    str key;
    redis_key_t *next;
};

typedef struct redis_type redis_type_t;
struct redis_type {
    str type;
    redis_type_t *next;
    redis_key_t *keys;
};

typedef struct redis_table redis_table_t;
struct redis_table {
    int version;
    str version_code;
    redis_key_t *entry_keys;
    redis_type_t *types;
    struct str_hash_table columns;
};

int db_redis_schema_get_column_type(km_redis_con_t *con, const str *table_name, const str *col_name);
void db_redis_print_all_tables(km_redis_con_t *con);
void db_redis_print_table(km_redis_con_t *con, char *name);
void db_redis_free_tables(km_redis_con_t *con);
int db_redis_parse_schema(km_redis_con_t *con);
int db_redis_parse_keys(km_redis_con_t *con);

int db_redis_key_add_string(redis_key_t* *list, const char* entry, int len);
int db_redis_key_add_str(redis_key_t **list, const str* entry);
int db_redis_key_prepend_string(redis_key_t **list, const char* entry, int len);
int db_redis_key_list2arr(redis_key_t *list, char ***arr);
redis_key_t * db_redis_key_shift(redis_key_t **list);
void db_redis_key_free(redis_key_t **list);

int db_redis_keys_spec(char *spec);

#endif /* _REDIS_TABLE_H_ */

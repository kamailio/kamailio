/*
 * PV Headers
 *
 * Copyright (C) 2018 Kirill Solomko <ksolomko@sipwise.com>
 *
 * This file is part of SIP Router, a free SIP server.
 *
 * SIP Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "pv_headers.h"
#include "pvh_hash.h"
#include "pvh_str.h"

struct str_hash_table skip_headers;
struct str_hash_table split_headers;
struct str_hash_table single_headers;

int pvh_str_hash_init(struct str_hash_table *ht, str *keys, char *desc)
{
	char split[header_name_size][header_value_size];
	int idx = 0, d_size = 0;
	str val = STR_NULL;

	if(pvh_split_values(keys, split, &d_size, 0) < 0) {
		LM_ERR("could not parse %s param\n", desc);
		return -1;
	}

	if(str_hash_alloc(ht, d_size + 1) < 0) {
		PKG_MEM_ERROR;
		return -1;
	}
	str_hash_init(ht);

	for(idx = 0; idx < d_size; idx++) {
		val.s = split[idx];
		val.len = strlen(split[idx]);
		if(pvh_str_hash_add_key(ht, &val) < 0) {
			LM_ERR("cannot add a hash key=>%s", desc);
			return -1;
		}
	}

	return 1;
}

int pvh_str_hash_add_key(struct str_hash_table *ht, str *key)
{
	struct str_hash_entry *e = NULL;
	int e_size;

	if(ht->table == NULL || key == NULL || key->len == 0)
		return -1;

	e_size = sizeof(struct str_hash_entry) + sizeof(char) * key->len;
	e = pkg_malloc(e_size);
	if(e == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(e, 0, e_size);

	if(pvh_str_new(&e->key, key->len + 1) < 0)
		goto err;
	pvh_str_copy(&e->key, key, key->len + 1);

	str_hash_add(ht, e);
	return 1;

err:
	pvh_str_free(&e->key);
	return -1;
}

int pvh_str_hash_free(struct str_hash_table *ht)
{
	struct str_hash_entry *e = NULL;
	struct str_hash_entry *bak = NULL;
	int r;

	if(ht == NULL)
		return -1;

	if(ht->table) {
		for(r = 0; r < ht->size; r++) {
			clist_foreach_safe(&ht->table[r], e, bak, next)
			{
				pvh_str_free(&e->key);
				pkg_free(e);
			}
		}
		pkg_free(ht->table);
	}

	return 1;
}

int pvh_skip_header(str *hname)
{
	if(hname == NULL)
		return 0;

	if(str_hash_get(&skip_headers, hname->s, hname->len))
		return 1;

	return 0;
}

int pvh_single_header(str *hname)
{
	if(hname == NULL)
		return 0;

	if(str_hash_get(&single_headers, hname->s, hname->len))
		return 1;

	return 0;
}

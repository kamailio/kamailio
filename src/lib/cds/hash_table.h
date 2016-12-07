/* 
 * Copyright (C) 2005 iptelorg GmbH
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __HASH_TABLE_H
#define __HASH_TABLE_H

typedef struct ht_statistic {
	int find_cnt;
	/** count of comparations during find operations */
	int cmp_cnt;
	/** count of finds which started in empty slot (-> no compares) */
	int nocmp_cnt;
	/** count of finds returning NULL */
	int missed_cnt;
} ht_statistic_t;

typedef const void* ht_key_t;
typedef void* ht_data_t;

typedef unsigned int (*hash_func_t)(ht_key_t k);
typedef int (*key_cmp_func_t)(ht_key_t a, ht_key_t b);

typedef struct ht_element {
	ht_key_t key;
	ht_data_t data;
	struct ht_element *next;
} ht_element_t;

typedef struct ht_cslot {
	ht_element_t *first;
	ht_element_t *last;
	int cnt;
} ht_cslot_t;

typedef struct hash_table {
	hash_func_t hash;
	key_cmp_func_t cmp;
	ht_cslot_t *cslots;
	int size;

	int find_cnt;
	int cmp_cnt;
	int nocmp_cnt;
	int missed_cnt;
} hash_table_t;

int ht_init(hash_table_t *ht, hash_func_t hash_func, key_cmp_func_t cmp_keys, int size);
void ht_destroy(hash_table_t *ht);
int ht_add(hash_table_t *ht, ht_key_t key, ht_data_t data);
ht_data_t ht_remove(hash_table_t *ht, ht_key_t key);
ht_data_t ht_find(hash_table_t *ht, ht_key_t key);
void ht_get_statistic(hash_table_t *ht, ht_statistic_t *s);
void ht_clear_statistic(hash_table_t *ht);

/* traversing through whole hash table */
typedef struct {
	hash_table_t *ht;
	int slot_pos;
	ht_element_t *current;
} ht_traversal_info_t;

ht_element_t *get_first_ht_element(hash_table_t *ht, ht_traversal_info_t *info);
ht_element_t *get_next_ht_element(ht_traversal_info_t *info);

/* hash functions */
unsigned int rshash(const char* str, unsigned int len);

#endif

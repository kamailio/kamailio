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

#include <cds/hash_table.h>
#include <cds/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ht_init(hash_table_t *ht, hash_func_t hash_func, key_cmp_func_t cmp_keys, int size)
{
	if (!ht) return -1;
	if ((!hash_func) || (!cmp_keys)) return -1;

	ht->cslots = (ht_cslot_t*)cds_malloc(size * sizeof(ht_cslot_t));
	if (!ht->cslots) return -1;
	memset(ht->cslots, 0, size * sizeof(ht_cslot_t));

	ht->size = size;
	ht->hash = hash_func;
	ht->cmp = cmp_keys;

	ht->find_cnt = 0;
	ht->cmp_cnt = 0;
	ht->nocmp_cnt = 0;
	ht->missed_cnt = 0;
	return 0;
}

void ht_destroy(hash_table_t *ht)
{
	ht_element_t *e, *n;
	int i;
	
	if (!ht) return;
	if (ht->cslots) {
		for (i = 0; i < ht->size; i++) {
			e = ht->cslots[i].first;
			while (e) {
				n = e->next;
				cds_free(e);
				e = n;
			}
		}
		cds_free(ht->cslots);
	}
	ht->cslots = NULL;
}

int ht_add(hash_table_t *ht, ht_key_t key, ht_data_t data)
{
	int h;
	ht_element_t *new_e;
	
	if (!ht) return -1;
	new_e = (ht_element_t*)cds_malloc(sizeof(ht_element_t));
	if (!new_e) return -1;
	new_e->next = NULL;
	new_e->key = key;
	new_e->data = data;
	
	h = ht->hash(key) % ht->size;
	if (h < 0) h = -h;
	
	if (!ht->cslots[h].last) {
		ht->cslots[h].first = new_e;
	}
	else {
		ht->cslots[h].last->next = new_e;
	}

	ht->cslots[h].cnt++;
	ht->cslots[h].last = new_e;
	return 0;
}

ht_data_t ht_find(hash_table_t *ht, ht_key_t key)
{
	int h;
	ht_element_t *e;

	if (!ht) return NULL;
	
	ht->find_cnt++;	//monitor
	
	h = ht->hash(key) % ht->size;
	if (h < 0) h = -h;
	e = ht->cslots[h].first;
	if (!e) ht->nocmp_cnt++;	//monitor
	while (e) {
		ht->cmp_cnt++;	//monitor
		if (ht->cmp(e->key, key) == 0) return e->data;
		e = e->next;
	}
	
	ht->missed_cnt++;	//monitor
	return NULL;
}

ht_data_t ht_remove(hash_table_t *ht, ht_key_t key)
{
	int h;
	ht_element_t *e,*p;
	ht_data_t data;
	
	if (!ht) return NULL;
	h = ht->hash(key) % ht->size;
	if (h < 0) h = -h;
	e = ht->cslots[h].first;
	p = NULL;
	while (e) {
		if (ht->cmp(e->key, key) == 0) {
			if (p) p->next = e->next;
			else ht->cslots[h].first = e->next;
			ht->cslots[h].cnt--;
			if (!e->next) ht->cslots[h].last = p;
			data = e->data;
			cds_free(e);
			return data;
		}
		p = e;
		e = e->next;
	}
	return NULL;
}

void ht_get_statistic(hash_table_t *ht, ht_statistic_t *s)
{
	if (!s) return;
	if (!ht) {
		s->find_cnt = 0;
		s->cmp_cnt = 0;
		s->nocmp_cnt = 0;
		s->missed_cnt = 0;
	}
	else {
		s->find_cnt = ht->find_cnt;
		s->cmp_cnt = ht->cmp_cnt;
		s->nocmp_cnt = ht->nocmp_cnt;
		s->missed_cnt = ht->missed_cnt;
	}
}

void ht_clear_statistic(hash_table_t *ht)
{
	if (!ht) return;
	
	ht->find_cnt = 0;
	ht->cmp_cnt = 0;
	ht->nocmp_cnt = 0;
	ht->missed_cnt = 0;
}

/* --------- hash table traversing functions -------- */

ht_element_t *get_first_ht_element(hash_table_t *ht, ht_traversal_info_t *info)
{
	int i;
	if (!info) return NULL;
	info->ht = ht;
	info->current = NULL;
	for (i = 0; i < ht->size; i++) {
		if (ht->cslots[i].first) {
			info->current = ht->cslots[i].first;
			break;
		}
	}
	info->slot_pos = i;
	return info->current;
}

ht_element_t *get_next_ht_element(ht_traversal_info_t *info)
{
	int i;
	if (!info) return NULL;

	if (info->current) info->current = info->current->next;
	
	if (info->current) return info->current;
	else {
		for (i = info->slot_pos + 1; i < info->ht->size; i++) {
			if (info->ht->cslots[i].first) {
				info->current = info->ht->cslots[i].first;
				break;
			}
		}
		info->slot_pos = i;
	}
	return info->current;
}

/* --------- HASH functions -------- */

unsigned int rshash(const char* str, unsigned int len)
{
	unsigned int b = 378551;
	unsigned int a = 63689;
	unsigned int hash = 0;
	unsigned int i = 0;

	for(i = 0; i < len; str++, i++) {
		hash = hash * a + (*str);
		a = a * b;
	}

	return (hash & 0x7FFFFFFF);
}


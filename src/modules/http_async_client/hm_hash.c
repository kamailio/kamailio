/**
 * Copyright 2016 (C) Federico Cabiddu <federico.cabiddu@gmail.com>
 * Copyright 2016 (C) Giacomo Vacca <giacomo.vacca@gmail.com>
 * Copyright 2016 (C) Orange - Camille Oudot <camille.oudot@orange.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*! \file
 * \brief  Kamailio http_async_client :: Hash functions
 * \ingroup http_async_client
 */


#include "hm_hash.h"

extern int hash_size;

/*!
 * \brief Initialize the global http multi table
 * \param size size of the table
 * \return 0 on success, -1 on failure
 */
int init_http_m_table(unsigned int size)
{
	unsigned int i;

	hm_table = (struct http_m_table *)shm_malloc(
			sizeof(struct http_m_table) + size * sizeof(struct http_m_entry));
	if(hm_table == 0) {
		LM_ERR("no more shm mem\n");
		return -1;
	}

	memset(hm_table, 0, sizeof(struct http_m_table));
	hm_table->size = size;
	hm_table->entries = (struct http_m_entry *)(hm_table + 1);

	for(i = 0; i < size; i++) {
		memset(&(hm_table->entries[i]), 0, sizeof(struct http_m_entry));
	}

	LM_DBG("hash table %p initialized with size %d\n", hm_table, size);
	return 0;
}

unsigned int build_hash_key(void *p)
{
	str hash_str;
	char pointer_str[20];

	unsigned int hash;

	hash_str.len = snprintf(pointer_str, 20, "%p", p);
	if(hash_str.len <= 0 || hash_str.len >= 20) {
		LM_ERR("failed to print the pointer address\n");
		return 0;
	}
	LM_DBG("received id %p (%d)-> %s (%d)\n", p, (int)sizeof(p), pointer_str,
			hash_str.len);

	hash_str.s = pointer_str;

	hash = core_hash(&hash_str, 0, hash_size);

	LM_DBG("hash for %p is %d\n", p, hash);

	return hash;
}

struct http_m_cell *build_http_m_cell(void *p)
{
	struct http_m_cell *cell = NULL;
	int len;

	len = sizeof(struct http_m_cell);
	cell = (struct http_m_cell *)shm_malloc(len);
	if(cell == 0) {
		LM_ERR("no more shm mem\n");
		return 0;
	}

	memset(cell, 0, len);

	cell->hmt_entry = build_hash_key(p);
	cell->easy = p;

	LM_DBG("hash id for %p is %d\n", p, cell->hmt_entry);

	return cell;
}

void link_http_m_cell(struct http_m_cell *cell)
{
	struct http_m_entry *hmt_entry;

	hmt_entry = &(hm_table->entries[cell->hmt_entry]);

	LM_DBG("linking new cell %p to table %p [%u]\n", cell, hm_table,
			cell->hmt_entry);
	if(hmt_entry->first == 0) {
		hmt_entry->first = cell;
		hmt_entry->first = hmt_entry->last = cell;
	} else {
		hmt_entry->last->next = cell;
		cell->prev = hmt_entry->last;
		hmt_entry->last = cell;
	}

	return;
}

struct http_m_cell *http_m_cell_lookup(CURL *p)
{
	struct http_m_entry *hmt_entry;
	struct http_m_cell *current_cell;

	unsigned int entry_idx;

	entry_idx = build_hash_key(p);

	hmt_entry = &(hm_table->entries[entry_idx]);

	for(current_cell = hmt_entry->first; current_cell;
			current_cell = current_cell->next) {
		if(current_cell->easy == p) {
			LM_DBG("http_m_cell with easy=%p found on table entry %u\n\n", p,
					entry_idx);
			return current_cell;
		}
	}

	/* not found */
	LM_DBG("No http_m_cell with easy=%p found on table entry %u", p, entry_idx);
	return 0;
}

void free_http_m_cell(struct http_m_cell *cell)
{
	if(!cell)
		return;

	if(cell->params.headers) {
		if(cell->params.headers)
			curl_slist_free_all(cell->params.headers);
	}

	if(cell->reply) {
		if(cell->reply->result) {
			if(cell->reply->result->s) {
				shm_free(cell->reply->result->s);
			}
			shm_free(cell->reply->result);
		}
		shm_free(cell->reply);
	}

	if(cell->url)
		shm_free(cell->url);

	shm_free(cell);
}

/**
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stddef.h>
#include <regex.h>

#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../hashes.h"
#include "../../ut.h"

#include "ds_ht.h"


#define ds_compute_hash(_s)        core_case_hash(_s,0,0)
#define ds_get_entry(_h,_size)    (_h)&((_size)-1)


ds_cell_t* ds_cell_new(str *cid, str *duid, int dset, unsigned int cellid)
{
	ds_cell_t *cell;
	unsigned int msize;

	msize = sizeof(ds_cell_t) + (cid->len + duid->len + 2)*sizeof(char);

	cell = (ds_cell_t*)shm_malloc(msize);
	if(cell==NULL)
	{
		LM_ERR("no more shm\n");
		return NULL;
	}

	memset(cell, 0, msize);
	cell->cellid = cellid;
	cell->dset = dset;
	cell->callid.len = cid->len;
	cell->callid.s = (char*)cell + sizeof(ds_cell_t);
	memcpy(cell->callid.s, cid->s, cid->len);
	cell->callid.s[cid->len] = '\0';

	cell->duid.len = duid->len;
	cell->duid.s = cell->callid.s + cell->callid.len + 1;
	memcpy(cell->duid.s, duid->s, duid->len);
	cell->duid.s[duid->len] = '\0';
	return cell;
}

int ds_cell_free(ds_cell_t *cell)
{
	if(cell==NULL)
		return -1;
	shm_free(cell);
	return 0;
}



ds_ht_t *ds_ht_init(unsigned int htsize, int expire, int initexpire)
{
	int i;
	ds_ht_t *dsht = NULL;

	dsht = (ds_ht_t*)shm_malloc(sizeof(ds_ht_t));
	if(dsht==NULL)
	{
		LM_ERR("no more shm\n");
		return NULL;
	}
	memset(dsht, 0, sizeof(ds_ht_t));
	dsht->htsize = htsize;
	dsht->htexpire = expire;
	dsht->htinitexpire = initexpire;

	dsht->entries = (ds_entry_t*)shm_malloc(dsht->htsize*sizeof(ds_entry_t));
	if(dsht->entries==NULL)
	{
		LM_ERR("no more shm.\n");
		shm_free(dsht);
		dsht = NULL;
		return NULL;
	}
	memset(dsht->entries, 0, dsht->htsize*sizeof(ds_entry_t));

	for(i=0; i<dsht->htsize; i++)
	{
		if(lock_init(&dsht->entries[i].lock)==0)
		{
			LM_ERR("cannot initalize lock[%d]\n", i);
			i--;
			while(i>=0)
			{
				lock_destroy(&dsht->entries[i].lock);
				i--;
			}
			shm_free(dsht->entries);
			shm_free(dsht);
			dsht = NULL;
			return NULL;
		}
	}

	return dsht;
}

int ds_ht_destroy(ds_ht_t *dsht)
{
	int i;
	ds_cell_t *it, *it0;

	if(dsht==NULL)
		return -1;

	for(i=0; i<dsht->htsize; i++)
	{
		/* free entries */
		it = dsht->entries[i].first;
		while(it)
		{
			it0 = it;
			it = it->next;
			ds_cell_free(it0);
		}
		/* free locks */
		lock_destroy(&dsht->entries[i].lock);
	}
	shm_free(dsht->entries);
	shm_free(dsht);
	dsht = NULL;
	return 0;
}

int ds_ht_clear_slots(ds_ht_t *dsht)
{
	int i;
	ds_cell_t *it, *it0;

	if(dsht==NULL)
		return -1;

	for(i=0; i<dsht->htsize; i++)
	{
		lock_get(&dsht->entries[i].lock);
		/* free entries */
		it = dsht->entries[i].first;
		while(it)
		{
			it0 = it;
			it = it->next;
			ds_cell_free(it0);
		}
		dsht->entries[i].first = NULL;
		dsht->entries[i].esize = 0;
		lock_release(&dsht->entries[i].lock);
	}
	return 0;
}


int ds_add_cell(ds_ht_t *dsht, str *cid, str *duid, int dset)
{
	unsigned int idx;
	unsigned int hid;
	ds_cell_t *it, *prev, *cell;
	time_t now;

	if(dsht==NULL || dsht->entries==NULL)
	{
		LM_ERR("invalid parameters.\n");
		return -1;
	}

	hid = ds_compute_hash(cid);
	
	idx = ds_get_entry(hid, dsht->htsize);

	now = time(NULL);
	prev = NULL;
	lock_get(&dsht->entries[idx].lock);
	it = dsht->entries[idx].first;
	while(it!=NULL && it->cellid < hid)
	{
		prev = it;
		it = it->next;
	}
	while(it!=NULL && it->cellid == hid)
	{
		if(cid->len==it->callid.len 
				&& strncmp(cid->s, it->callid.s, cid->len)==0)
		{
			lock_release(&dsht->entries[idx].lock);
			LM_WARN("call-id already in hash table [%.*s].\n",
					cid->len, cid->s);
			return -2;
		}
		prev = it;
		it = it->next;
	}
	/* add */
	cell = ds_cell_new(cid, duid, dset, hid);
	if(cell == NULL)
	{
		LM_ERR("cannot create new cell.\n");
		lock_release(&dsht->entries[idx].lock);
		return -1;
	}
	cell->expire = now + dsht->htexpire;
	cell->initexpire = now + dsht->htinitexpire;
	if(prev==NULL)
	{
		if(dsht->entries[idx].first!=NULL)
		{
			cell->next = dsht->entries[idx].first;
			dsht->entries[idx].first->prev = cell;
		}
		dsht->entries[idx].first = cell;
	} else {
		cell->next = prev->next;
		cell->prev = prev;
		if(prev->next)
			prev->next->prev = cell;
		prev->next = cell;
	}
	dsht->entries[idx].esize++;
	lock_release(&dsht->entries[idx].lock);
	return 0;
}

int ds_unlock_cell(ds_ht_t *dsht, str *cid)
{
	unsigned int idx;
	unsigned int hid;

	if(dsht==NULL || dsht->entries==NULL)
		return -1;

	hid = ds_compute_hash(cid);
	
	idx = ds_get_entry(hid, dsht->htsize);

	/* head test and return */
	if(dsht->entries[idx].first==NULL)
		return 0;
	
	lock_release(&dsht->entries[idx].lock);
	return 0;
}

ds_cell_t* ds_get_cell(ds_ht_t *dsht, str *cid)
{
	unsigned int idx;
	unsigned int hid;
	ds_cell_t *it;

	if(dsht==NULL || dsht->entries==NULL)
		return 0;

	hid = ds_compute_hash(cid);
	
	idx = ds_get_entry(hid, dsht->htsize);

	/* head test and return */
	if(dsht->entries[idx].first==NULL)
		return 0;
	
	lock_get(&dsht->entries[idx].lock);
	it = dsht->entries[idx].first;
	while(it!=NULL && it->cellid < hid)
		it = it->next;
	while(it!=NULL && it->cellid == hid)
	{
		if(cid->len==it->callid.len 
				&& strncmp(cid->s, it->callid.s, cid->len)==0)
		{
			/* found */
			return it;
		}
		it = it->next;
	}
	lock_release(&dsht->entries[idx].lock);
	return 0;
}


int ds_del_cell(ds_ht_t *dsht, str *cid)
{
	unsigned int idx;
	unsigned int hid;
	ds_cell_t *it;

	if(dsht==NULL || dsht->entries==NULL)
		return -1;

	hid = ds_compute_hash(cid);
	
	idx = ds_get_entry(hid, dsht->htsize);

	/* head test and return */
	if(dsht->entries[idx].first==NULL)
		return 0;
	
	lock_get(&dsht->entries[idx].lock);
	it = dsht->entries[idx].first;
	while(it!=NULL && it->cellid < hid)
		it = it->next;
	while(it!=NULL && it->cellid == hid)
	{
		if(cid->len==it->callid.len 
				&& strncmp(cid->s, it->callid.s, cid->len)==0)
		{
			/* found */
			if(it->prev==NULL)
				dsht->entries[idx].first = it->next;
			else
				it->prev->next = it->next;
			if(it->next)
				it->next->prev = it->prev;
			dsht->entries[idx].esize--;
			lock_release(&dsht->entries[idx].lock);
			ds_cell_free(it);
			return 0;
		}
		it = it->next;
	}
	lock_release(&dsht->entries[idx].lock);
	return 0;
}

int ds_ht_dbg(ds_ht_t *dsht)
{
	int i;
	ds_cell_t *it;

	for(i=0; i<dsht->htsize; i++)
	{
		lock_get(&dsht->entries[i].lock);
		LM_ERR("htable[%d] -- <%d>\n", i, dsht->entries[i].esize);
		it = dsht->entries[i].first;
		while(it)
		{
			LM_ERR("\tcell: %.*s\n", it->callid.len, it->callid.s);
			LM_ERR("\tduid: %.*s\n", it->duid.len, it->duid.s);
			LM_ERR("\thid: %u expire: %u initexpire: %u\n", it->cellid,
					(unsigned int)it->expire, (unsigned int)it->initexpire);
			LM_ERR("\tdset:%d\n", it->dset);
			it = it->next;
		}
		lock_release(&dsht->entries[i].lock);
	}
	return 0;
}



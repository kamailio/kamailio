/**
 * $Id$
 *
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../hash_func.h"

#include "ht_api.h"


#define ht_compute_hash(_s)        core_case_hash(_s,0,0)
#define ht_get_entry(_h,_size)    (_h)&((_size)-1)


ht_t *_ht_root = NULL;


ht_cell_t* ht_cell_new(str *name, int type, int_str *val, unsigned int cellid)
{
	ht_cell_t *cell;
	unsigned int msize;

	msize = sizeof(ht_cell_t) + (name->len + 1)*sizeof(char);

	if(type&AVP_VAL_STR)
		msize += (val->s.len + 1)*sizeof(char);

	cell = (ht_cell_t*)shm_malloc(msize);
	if(cell==NULL)
	{
		LM_ERR("no more shm\n");
		return NULL;
	}

	memset(cell, 0, msize);
	cell->msize = msize;
	cell->cellid = cellid;
	cell->flags = type&AVP_VAL_STR;
	cell->name.len = name->len;
	cell->name.s = (char*)cell + sizeof(ht_cell_t);
	memcpy(cell->name.s, name->s, name->len);
	cell->name.s[name->len] = '\0';
	if(type&AVP_VAL_STR)
	{
		cell->value.s.s = (char*)cell->name.s + name->len + 1;
		cell->value.s.len = val->s.len;
		memcpy(cell->value.s.s, val->s.s, val->s.len);
		cell->value.s.s[val->s.len] = '\0';
	} else {
		cell->value.n = val->n;
	}
	return cell;
}

int ht_cell_free(ht_cell_t *cell)
{
	if(cell==NULL)
		return -1;
	shm_free(cell);
	return 0;
}


int ht_cell_pkg_free(ht_cell_t *cell)
{
	if(cell==NULL)
		return -1;
	pkg_free(cell);
	return 0;
}


int ht_init(int size)
{
	int i;
	if(size > 1<<14)
		size = 1<<14;

	_ht_root = (ht_t*)shm_malloc(sizeof(ht_t));
	if(_ht_root==NULL)
	{
		LM_ERR("no more shm\n");
		return -1;
	}
	memset(_ht_root, 0, sizeof(ht_t));

	_ht_root->entries = (ht_entry_t*)shm_malloc(size*sizeof(ht_entry_t));
	if(_ht_root->entries==NULL)
	{
		LM_ERR("no more shm.\n");
		shm_free(_ht_root);
		_ht_root = NULL;
		return -1;
	}

	for(i=0; i<size; i++)
	{
		if(lock_init(&_ht_root->entries[i].lock)==0)
		{
			LM_ERR("cannot initalize lock[%d]\n", i);
			i--;
			while(i>=0)
			{
				lock_destroy(&_ht_root->entries[i].lock);
				i--;
			}
			shm_free(_ht_root->entries);
			shm_free(_ht_root);
			_ht_root = NULL;
			return -1;

		}
	}
	_ht_root->htsize = size;
	return 0;
}

int ht_destroy(void)
{
	int i;
	ht_cell_t *it, *it0;

	if(_ht_root==NULL || _ht_root->entries==NULL)
		return -1;

	for(i=0; i<_ht_root->htsize; i++)
	{
		/* free entries */
		it = _ht_root->entries[i].first;
		while(it)
		{
			it0 = it;
			it = it->next;
			ht_cell_free(it0);
		}
		/* free locks */
		lock_destroy(&_ht_root->entries[i].lock);
	}
	shm_free(_ht_root->entries);
	shm_free(_ht_root);
	_ht_root = NULL;
	return 0;
}


int ht_set_cell(str *name, int type, int_str *val)
{
	unsigned int idx;
	unsigned int hid;
	ht_cell_t *it, *prev, *cell;

	if(_ht_root==NULL || _ht_root->entries==NULL)
		return -1;

	hid = ht_compute_hash(name);
	
	idx = ht_get_entry(hid, _ht_root->htsize);

	prev = NULL;
	lock_get(&_ht_root->entries[idx].lock);
	it = _ht_root->entries[idx].first;
	while(it!=NULL && it->cellid < hid)
	{
		prev = it;
		it = it->next;
	}
	while(it!=NULL && it->cellid == hid)
	{
		if(name->len==it->name.len 
				&& strncmp(name->s, it->name.s, name->len)==0)
		{
			/* update value */
			if(it->flags&AVP_VAL_STR)
			{
				if(type&AVP_VAL_STR)
				{
					if(it->value.s.len >= val->s.len)
					{
						/* copy */
						it->value.s.len = val->s.len;
						memcpy(it->value.s.s, val->s.s, val->s.len);
					} else {
						/* new */
						cell = ht_cell_new(name, type, val, hid);
						if(cell == NULL)
						{
							LM_ERR("cannot create new cell\n");
							lock_release(&_ht_root->entries[idx].lock);
							return -1;
						}
						cell->next = it->next;
						cell->prev = it->prev;
						if(it->prev)
							it->prev->next = cell;
						if(it->next)
							it->next->prev = cell;
						ht_cell_free(it);
					}
				} else {
					it->flags &= ~AVP_VAL_STR;
					it->value.n = val->n;
				}
				lock_release(&_ht_root->entries[idx].lock);
				return 0;
			} else {
				if(type&AVP_VAL_STR)
				{
					/* new */
					cell = ht_cell_new(name, type, val, hid);
					if(cell == NULL)
					{
						LM_ERR("cannot create new cell.\n");
						lock_release(&_ht_root->entries[idx].lock);
						return -1;
					}
					cell->next = it->next;
					cell->prev = it->prev;
					if(it->prev)
						it->prev->next = cell;
					if(it->next)
						it->next->prev = cell;
					ht_cell_free(it);
				} else {
					it->value.n = val->n;
				}
				lock_release(&_ht_root->entries[idx].lock);
				return 0;
			}
		}
		prev = it;
		it = it->next;
	}
	/* add */
	cell = ht_cell_new(name, type, val, hid);
	if(cell == NULL)
	{
		LM_ERR("cannot create new cell.\n");
		lock_release(&_ht_root->entries[idx].lock);
		return -1;
	}
	if(prev==NULL)
	{
		if(_ht_root->entries[idx].first!=NULL)
		{
			cell->next = _ht_root->entries[idx].first;
			_ht_root->entries[idx].first->prev = cell;
		}
		_ht_root->entries[idx].first = cell;
	} else {
		cell->next = prev->next;
		cell->prev = prev;
		if(prev->next)
			prev->next->prev = cell;
		prev->next = cell;
	}
	_ht_root->entries[idx].esize++;
	lock_release(&_ht_root->entries[idx].lock);
	return 0;
}

int ht_del_cell(str *name)
{
	unsigned int idx;
	unsigned int hid;
	ht_cell_t *it;

	if(_ht_root==NULL || _ht_root->entries==NULL)
		return -1;

	hid = ht_compute_hash(name);
	
	idx = ht_get_entry(hid, _ht_root->htsize);

	/* head test and return */
	if(_ht_root->entries[idx].first==NULL)
		return 0;
	
	lock_get(&_ht_root->entries[idx].lock);
	it = _ht_root->entries[idx].first;
	while(it!=NULL && it->cellid < hid)
		it = it->next;
	while(it!=NULL && it->cellid == hid)
	{
		if(name->len==it->name.len 
				&& strncmp(name->s, it->name.s, name->len)==0)
		{
			/* found */
			if(it->prev==NULL)
				_ht_root->entries[idx].first = it->next;
			else
				it->prev->next = it->next;
			if(it->next)
				it->next->prev = it->prev;
			_ht_root->entries[idx].esize--;
			lock_release(&_ht_root->entries[idx].lock);
			ht_cell_free(it);
			return 0;
		}
		it = it->next;
	}
	lock_release(&_ht_root->entries[idx].lock);
	return 0;
}

ht_cell_t* ht_cell_pkg_copy(str *name, ht_cell_t *old)
{
	unsigned int idx;
	unsigned int hid;
	ht_cell_t *it, *cell;

	if(_ht_root==NULL || _ht_root->entries==NULL)
		return NULL;

	hid = ht_compute_hash(name);
	
	idx = ht_get_entry(hid, _ht_root->htsize);

	/* head test and return */
	if(_ht_root->entries[idx].first==NULL)
		return NULL;
	
	lock_get(&_ht_root->entries[idx].lock);
	it = _ht_root->entries[idx].first;
	while(it!=NULL && it->cellid < hid)
		it = it->next;
	while(it!=NULL && it->cellid == hid)
	{
		if(name->len==it->name.len 
				&& strncmp(name->s, it->name.s, name->len)==0)
		{
			/* found */
			if(old!=NULL)
			{
				if(old->msize>=it->msize)
				{
					memcpy(old, it, it->msize);
					lock_release(&_ht_root->entries[idx].lock);
					return old;
				}
			}
			cell = (ht_cell_t*)pkg_malloc(it->msize);
			if(cell!=NULL)
				memcpy(cell, it, it->msize);
			lock_release(&_ht_root->entries[idx].lock);
			return cell;
		}
		it = it->next;
	}
	lock_release(&_ht_root->entries[idx].lock);
	return NULL;
}

int ht_dbg(void)
{
	int i;
	ht_cell_t *it;

	if(_ht_root==NULL || _ht_root->entries==NULL)
		return -1;

	for(i=0; i<_ht_root->htsize; i++)
	{
		lock_get(&_ht_root->entries[i].lock);
		LM_ERR("htable[%d] -- <%d>\n", i, _ht_root->entries[i].esize);
		it = _ht_root->entries[i].first;
		while(it)
		{
			LM_ERR("\tcell: %.*s\n", it->name.len, it->name.s);
			LM_ERR("\thid: %u msize: %u flags: %d\n", it->cellid, it->msize,
					it->flags);
			if(it->flags&AVP_VAL_STR)
				LM_ERR("\tv-s:%.*s\n", it->value.s.len, it->value.s.s);
			else
				LM_ERR("\tv-i:%d\n", it->value.n);
			it = it->next;
		}
		lock_release(&_ht_root->entries[i].lock);
	}
	return 0;
}


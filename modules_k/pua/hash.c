/*
 * $Id$
 *
 * pua module - presence user agent module
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../hash_func.h"
#include "hash.h" 
#include "pua.h"
#include "send_publish.h"


htable_t* new_htable()
{
	htable_t* H= NULL;
	int i;

	H= (htable_t*)shm_malloc(sizeof(htable_t));
	if(H== NULL)
	{
		LOG(L_ERR, "PUA: new_htable: No more memory\n");
		return NULL;
	}
	memset(H, 0, sizeof(htable_t));

	H->p_records= (hash_entry_t*)shm_malloc(HASH_SIZE* sizeof(hash_entry_t));
	if(H->p_records== NULL)
	{
		LOG(L_ERR, "PUA: new_htable: No more share memory\n");
		goto error;		
	}

	for(i=0; i<HASH_SIZE; i++)
	{
		if(lock_init(&H->p_records[i].lock)== 0)
		{
			LOG(L_CRIT,
				"PUA: new_htable: ERROR initializing lock [%d]\n", i);
			goto error;
		}
		H->p_records[i].entity= (ua_pres_t*)shm_malloc(sizeof(ua_pres_t));
		if(H->p_records[i].entity== NULL)
		{
			LOG(L_ERR, "PUA: new_htable: No more share memory\n");
			goto error;		
		}	
		H->p_records[i].entity->next= NULL;
	}
	return H;

error:

	if(H->p_records)
	{
		for(i=0; i<HASH_SIZE; i++)
		{
			if(H->p_records[i].entity)
				shm_free(H->p_records[i].entity);
			lock_destroy(&_imc_htable[i].lock);

		}
		shm_free(H->p_records);
	}
	shm_free(H);
	return NULL;

}

ua_pres_t* search_htable(str* pres_uri, str* watcher_uri, str id,
		int FLAG, int event, htable_t* H)
{
	ua_pres_t* p= NULL,* L= NULL;
 
	L= H->p_records[core_hash(pres_uri, watcher_uri, HASH_SIZE)].entity;
	DBG("PUA: search_htable: core_hash= %d\n",
			core_hash(pres_uri, watcher_uri, HASH_SIZE) );

	for(p= L->next; p; p=p->next)
	{
		if((p->event== event) && (p->flag & FLAG))
		{
			DBG("PUA: search_htable:pres_uri= %.*s len= %d\n",
					p->pres_uri->len, p->pres_uri->s, p->pres_uri->len);
			DBG("PUA: search_htable:searched uri= %.*s len= %d\n",
					pres_uri->len,pres_uri->s, pres_uri->len);

			if((p->pres_uri->len==pres_uri->len) &&
					(strncmp(p->pres_uri->s, pres_uri->s,pres_uri->len)==0))
			{	
				DBG("PUA: search_htable:found pres_ur\n");
				if(watcher_uri)
				{	
					if(p->watcher_uri->len==watcher_uri->len &&
						(strncmp(p->watcher_uri->s, watcher_uri->s,
								 watcher_uri->len )==0))
					{
						break;
					}

				}
				else
				{
					if(id.s)
					{	
						DBG("PUA: search_htable: compare id\n");
						if(id.len== p->id.len &&
								strncmp(p->id.s, id.s, id.len)==0)
							break;
					}
					else
						break;
				}

			}
		}

	}
	if(p)
		DBG("PUA:search_htable: found record\n");
	else
		DBG("PUA:search_htable: record not found\n");

	return p;
}

void hash_update(ua_pres_t* presentity, int expires, htable_t* H)
{
	ua_pres_t* p= NULL;

	DBG("PUA:hash_update ..\n");

	p= search_htable(presentity->pres_uri, presentity->watcher_uri,
				presentity->id, presentity->flag,presentity->event, H);
	if(p== NULL)
	{
		DBG("PUA:hash_update : no recod found\n");
		return; 
	}

	p->expires= expires+ (int)time(NULL);
	if(p->db_flag& NO_UPDATEDB_FLAG)
		p->db_flag= UPDATEDB_FLAG;

	if(p->watcher_uri)
		p->cseq ++;

}	

void insert_htable(ua_pres_t* presentity , htable_t* H)
{
	ua_pres_t* p= NULL;
	int hash_code= 0;
	
	hash_code= core_hash(presentity->pres_uri,presentity->watcher_uri, 
			HASH_SIZE);

	if(presentity->expires < (int)time(NULL))
	{
		LOG(L_ERR, "PUA: insert_htable: expired information- do not insert\n");
		return;
	}	

	lock_get(&H->p_records[hash_code].lock);
	
	p= search_htable(presentity->pres_uri, presentity->watcher_uri,
				presentity->id, presentity->flag, presentity->event, H);
	if(p) 
	{
		lock_release(&H->p_records[hash_code].lock);
		return; 
	}
	p= H->p_records[hash_code].entity;

	presentity->db_flag= INSERTDB_FLAG;
	presentity->next= p->next;
	
	p->next= presentity;

	lock_release(&H->p_records[hash_code].lock);
}

void delete_htable(ua_pres_t* presentity , htable_t* H)
{ 
	ua_pres_t* p= NULL, *q= NULL;
	
	DBG("PUA:delete_htable...\n");

	p= search_htable(presentity->pres_uri, presentity->watcher_uri,
			presentity->id, presentity->flag, presentity->event, H);
	if(p== NULL)
	{
		return;
	}

	q=H->p_records[core_hash(presentity->pres_uri, 
			presentity->watcher_uri, HASH_SIZE)].entity;
	
	while(q->next!=p)
		q= q->next;
	q->next=p->next;

	shm_free(p);
	p= NULL;

}
	
void destroy_htable(htable_t* H)
{
	ua_pres_t* p= NULL,*q= NULL;
	int i;

	DBG("PUA: destroy htable.. \n");
	for(i=0; i<HASH_SIZE; i++)
	{	
		lock_destroy(&_imc_htable[i].lock);
		p=H->p_records[i].entity;
		while(p->next)
		{
			q=p->next;
			p->next=q->next;
			shm_free(q);
			q= NULL;
		}
		shm_free(p);
	}
    shm_free(H->p_records);
	shm_free(H);
  
  return;
}


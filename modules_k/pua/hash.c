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

void print_ua_pres(ua_pres_t* p)
{
	DBG("PUA:print_ua_pres: \n");
	DBG("\tpres_uri= %.*s   len= %d\n", p->pres_uri->len, p->pres_uri->s, p->pres_uri->len);
	if(p->watcher_uri)
	{	
		DBG("\twatcher_uri= %.*s  len= %d\n", p->watcher_uri->len, p->watcher_uri->s, p->watcher_uri->len);
		DBG("\tcall_id= %.*s   len= %d\n", p->call_id.len, p->call_id.s, p->call_id.len);
	}	
	else
		DBG("\tetag= %.*s - len= %d\n", p->etag.len, p->etag.s, p->etag.len);
	DBG("\texpires= %d\n", p->expires- (int)time(NULL));
}	
htable_t* new_htable()
{
	htable_t* H= NULL;
	int i= 0, j;

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
		for(j=0; j< i; j++)
		{
			if(H->p_records[j].entity)
				shm_free(H->p_records[j].entity);
			lock_destroy(&H->p_records[j].lock);

		}
		shm_free(H->p_records);
	}
	shm_free(H);
	return NULL;

}

ua_pres_t* search_htable(ua_pres_t* pres, unsigned int hash_code)
{
	ua_pres_t* p= NULL,* L= NULL;

	L= HashT->p_records[hash_code].entity;
	DBG("PUA: search_htable: core_hash= %u\n", hash_code);

	for(p= L->next; p; p=p->next)
	{
		if((p->flag & pres->flag) && (p->event & pres->event))
		{
			if((p->pres_uri->len==pres->pres_uri->len) &&
					(strncmp(p->pres_uri->s, pres->pres_uri->s,pres->pres_uri->len)==0))
			{
				if(pres->id.s && pres->id.len) 
				{	
					if(!(pres->id.len== p->id.len &&
						strncmp(p->id.s, pres->id.s,pres->id.len)==0))
							continue;
				}				

				if(pres->watcher_uri)
				{
					if(p->watcher_uri->len==pres->watcher_uri->len &&
						(strncmp(p->watcher_uri->s, pres->watcher_uri->s,
								  pres->watcher_uri->len )==0))
					{
						break;
					}
				}
				else
				{
					if(pres->etag.s)
					{
						if(pres->etag.len== p->etag.len &&
							strncmp(p->etag.s, pres->etag.s,pres->etag.len)==0)
							break;		
					}	
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

void update_htable(ua_pres_t* presentity,time_t desired_expires, 
		int expires,str* etag, unsigned int hash_code)
{
	ua_pres_t* p= NULL;
	DBG("PUA:hash_update ..\n");


	p= search_htable(presentity, hash_code);
	if(p== NULL)
	{
		DBG("PUA:hash_update : no recod found\n");
		return; 
	}
	if(etag)
	{	
		shm_free(p->etag.s);
		p->etag.s= (char*)shm_malloc(etag->len);
		memcpy(p->etag.s, etag->s, etag->len);
		p->etag.len= etag->len;
	}

	p->expires= expires+ (int)time(NULL);
	p->desired_expires= desired_expires;
		
	if(p->db_flag & NO_UPDATEDB_FLAG)
		p->db_flag= UPDATEDB_FLAG;

	if(p->watcher_uri)
		p->cseq ++;

}
/* insert in front; so when searching the most recent result is returned*/
void insert_htable(ua_pres_t* presentity)
{
	ua_pres_t* p= NULL;
	unsigned int hash_code;

	hash_code= core_hash(presentity->pres_uri,presentity->watcher_uri, 
			HASH_SIZE);
	
	lock_get(&HashT->p_records[hash_code].lock);

/*	
 *	useless since always checking before calling insert
	if(get_dialog(presentity, hash_code)!= NULL )
	{
		DBG("PUA: insert_htable: Dialog already found- do not insert\n");
		return; 
	}
*/	
	p= HashT->p_records[hash_code].entity;

	presentity->db_flag= INSERTDB_FLAG;
	presentity->next= p->next;
	
	p->next= presentity;

	lock_release(&HashT->p_records[hash_code].lock);

}

void delete_htable(ua_pres_t* presentity, unsigned int hash_code)
{ 
	ua_pres_t* p= NULL, *q= NULL;
	DBG("PUA:delete_htable...\n");

	p= search_htable(presentity, hash_code);
	if(p== NULL)
		return;

	q=HashT->p_records[hash_code].entity;

	while(q->next!=p)
		q= q->next;
	q->next=p->next;
	
	if(p->etag.s)
		shm_free(p->etag.s);
	shm_free(p);
	p= NULL;

}
	
void destroy_htable()
{
	ua_pres_t* p= NULL,*q= NULL;
	int i;

	DBG("PUA: destroy htable.. \n");
	for(i=0; i<HASH_SIZE; i++)
	{	
		lock_destroy(&HashT->p_records[i].lock);
		p=HashT->p_records[i].entity;
		while(p->next)
		{
			q=p->next;
			p->next=q->next;
			if(q->etag.s)
				shm_free(q->etag.s);
			shm_free(q);
			q= NULL;
		}
		shm_free(p);
	}
    shm_free(HashT->p_records);
	shm_free(HashT);
  
  return;
}

/* must lock the record line before calling this function*/
ua_pres_t* get_dialog(ua_pres_t* dialog, unsigned int hash_code)
{
	ua_pres_t* p= NULL, *L;
	DBG("PUA: get_dialog: core_hash= %u\n", hash_code);

	L= HashT->p_records[hash_code].entity;
	for(p= L->next; p; p=p->next)
	{

		if(p->flag& dialog->flag)
		{
			DBG("PUA: get_dialog: pres_uri= %.*s\twatcher_uri=%.*s\n\t"
					"callid= %.*s\tto_tag= %.*s\tfrom_tag= %.*s\n",
				p->pres_uri->len, p->pres_uri->s, p->watcher_uri->len,
				p->watcher_uri->s,p->call_id.len, p->call_id.s,
				p->to_tag.len, p->to_tag.s, p->from_tag.len, p->from_tag.s);

			DBG("PUA: get_dialog: searched to_tag= %.*s\tfrom_tag= %.*s\n",
				 p->to_tag.len, p->to_tag.s, p->from_tag.len, p->from_tag.s);
	    
			if((p->pres_uri->len== dialog->pres_uri->len) &&
				(strncmp(p->pres_uri->s, dialog->pres_uri->s,p->pres_uri->len)==0)&&
				(p->watcher_uri->len== dialog->watcher_uri->len) &&
 	    		(strncmp(p->watcher_uri->s,dialog->watcher_uri->s,p->watcher_uri->len )==0)&&
				(strncmp(p->call_id.s, dialog->call_id.s, p->call_id.len)== 0) &&
				(strncmp(p->to_tag.s, dialog->to_tag.s, p->to_tag.len)== 0) &&
				(strncmp(p->from_tag.s, dialog->from_tag.s, p->from_tag.len)== 0) )
				{	
					DBG("PUA: get_dialog: FOUND dialog\n");
					break;
				}
		}	
	
	}
		
	return p;
}

int get_record_id(ua_pres_t* dialog, str** rec_id)
{
	unsigned int hash_code;
	ua_pres_t* rec;
	str* id;

	*rec_id= NULL;

	hash_code= core_hash(dialog->pres_uri, dialog->watcher_uri, HASH_SIZE);
	lock_get(&HashT->p_records[hash_code].lock);

	rec= get_dialog(dialog, hash_code);
	if(rec== NULL)
	{
		DBG("PUA:get_record_id: Record not found\n");
		lock_release(&HashT->p_records[hash_code].lock);
		return 0;
	}
	id= (str*)pkg_malloc(sizeof(str));
	if(id== NULL)
	{
		LOG(L_ERR, "PUA: get_record_id: ERROR No more memory\n");
		lock_release(&HashT->p_records[hash_code].lock);
		return -1;
	}
	id->s= (char*)pkg_malloc(rec->id.len* sizeof(char));
	if(id->s== NULL)
	{
		LOG(L_ERR, "PUA: get_record_id: ERROR No more memory\n");
		pkg_free(id);
		lock_release(&HashT->p_records[hash_code].lock);
		return -1;
	}
	memcpy(id->s, rec->id.s, rec->id.len);
	id->len= rec->id.len;
	lock_release(&HashT->p_records[hash_code].lock);

	*rec_id= id;

	return 0;
}

int is_dialog(ua_pres_t* dialog)
{
	int ret_code= 0;
	unsigned int hash_code;
	
	hash_code= core_hash(dialog->pres_uri, dialog->watcher_uri, HASH_SIZE);
	lock_get(&HashT->p_records[hash_code].lock);

	if(get_dialog(dialog, hash_code)== NULL)
		ret_code= -1;
	else
		ret_code= 0;
	lock_release(&HashT->p_records[hash_code].lock);
	
	return ret_code;

}


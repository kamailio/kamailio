/**
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

 /**
  * History
  * ------- 
  * 2003-04-07: a structure for both hashes introduced (ramona)
  * 2005-01-26: double hash removed (ramona)
  *             FIFO operations are kept as a diff list (ramona)
  *             domain hash kept in share memory along with FIFO ops (ramona)
  * 2006-01-30: multidomain support added
  * 2006-03-13: use hash function from core (bogdan)
  */

#include <stdio.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../parser/parse_fline.h"
#include "../../db/db.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../hash_func.h"
#include "../../ut.h"

#include "domains.h"

#define pdt_compute_hash(_s)        core_case_hash(_s,0,0)
#define get_hash_entry(_h,_size)    (_h)&((_size)-1)

pd_op_t* new_pd_op(pd_t *cell, int id, int op)
{
	pd_op_t *pdo;
	
	if(cell==NULL)
	{
		LOG(L_ERR, "PDT:new_pd_op: bad parameters\n");
		return NULL;
	}
	
	pdo = (pd_op_t*)shm_malloc(sizeof(pd_op_t));
	if(pdo==NULL)
	{
		LOG(L_ERR, "PDT:new_pd_op: no more shm\n");
		return NULL;
	}
	memset(pdo, 0, sizeof(pd_op_t));
	pdo->cell = cell;
	pdo->id = id;
	pdo->op = op;

	return pdo;
}

void free_pd_op(pd_op_t *pdo)
{
	if(pdo==NULL)
		return;
	free_cell(pdo->cell);
	shm_free(pdo);
	pdo = NULL;

	return;
}

pd_t* new_cell(str* p, str *d)
{
	pd_t* cell = NULL; 
	
	if(p==NULL || p->s==NULL || d==NULL || d->s==NULL)
	{
		LOG(L_ERR, "PDT:new_cell: bad parameters\n");
		return NULL;
	}
	
	/* the cell is in share memory */
	cell = (pd_t*)shm_malloc(sizeof(pd_t));
    
	/* if there is no space return just NULL */
	if(cell==NULL)
	{
		LOG(L_ERR, "PDT:new_cell: no more shm memory.\n");
		return NULL;
	}
	memset(cell, 0, sizeof(pd_t));
	
	cell->prefix.s = (char*)shm_malloc((1+p->len)*sizeof(char));
	if(cell->prefix.s==NULL)
	{
		shm_free(cell);
		LOG(L_ERR, "PDT:new_cell: no more shm memory\n");
		return NULL;
	}
	strncpy(cell->prefix.s, p->s, p->len);
	cell->prefix.len = p->len;
	cell->prefix.s[p->len] = '\0';

	cell->domain.s = (char*)shm_malloc((1+d->len)*sizeof(char));
	if(cell->domain.s==NULL)
	{
		shm_free(cell->prefix.s);
		shm_free(cell);
		LOG(L_ERR, "PDT:new_cell: no more shm memory!\n");
		return NULL;
	}
	strncpy(cell->domain.s, d->s, d->len);
	cell->domain.len = d->len;
	cell->domain.s[d->len] = '\0';

	cell->dhash = pdt_compute_hash(&cell->domain);
    
	/* return the newly allocated in share memory cell */
	return cell;
}

void free_cell(pd_t* cell)
{
	if(cell==NULL)
		return;	
	
	if(cell->prefix.s)
		shm_free(cell->prefix.s);

	if(cell->domain.s)
		shm_free(cell->domain.s);
		
	shm_free(cell);
}

/* returns a pointer to a hashtable */
pd_t** init_hash_entries(unsigned int hash_size)
{
	pd_t **hash = NULL; 

	/* space for the hash is allocated in share memory */
	hash = (pd_t**)shm_malloc(hash_size*sizeof(pd_t*));
	if(hash == NULL)
	{
		LOG(L_ERR, "PDT:init_hash: no more shm\n");
		return NULL;
	}
	memset(hash, 0, hash_size*sizeof(pd_t*));

	/* the allocated hash */
	return hash;
}
void free_hash_entries(pd_t** hash, unsigned int hash_size)
{
    unsigned int   i;
    pd_t *tmp, *it;
    if(hash==NULL || hash_size<=0)
		return;
    
	for(i=0; i<hash_size; i++)
	{
		it = hash[i];
		while(it != NULL)
		{
	    	tmp = it->n;
			free_cell(it);
			it = tmp;
		}
    }
	shm_free(hash);
}


hash_t* init_hash(int hash_size, str *sdomain)
{
	hash_t* hash = NULL;

	/* space allocated in shared memory */
	hash = (hash_t*)shm_malloc(sizeof(hash_t));
	
	if(hash == NULL)
	{
		LOG(L_ERR, "PDT: pdt_init_hash: no more shm\n");
		return NULL;
	}
	memset(hash, 0, sizeof(hash_t));

	hash->sdomain.s = (char*)shm_malloc((sdomain->len+1)*sizeof(char));
	if(hash->sdomain.s==NULL)
	{
		LOG(L_ERR, "PDT: pdt_init_hash: no more shm\n");
		shm_free(hash);
		return NULL;
	}
	memset(hash->sdomain.s, 0, sdomain->len+1);
	memcpy(hash->sdomain.s, sdomain->s, sdomain->len);
	hash->sdomain.len = sdomain->len;
	
	if( (hash->dhash = init_hash_entries(hash_size)) == NULL )
	{
		shm_free(hash->sdomain.s);
		shm_free(hash);
		LOG(L_ERR, "PDT:pdt_init_hash: no more shm!\n");
		return NULL;
	}
	
	hash->hash_size = hash_size;

	return hash;
}

int set_hash_domain(hash_t *h, str *s)
{
	if(s==NULL || s->s==NULL || h==NULL)
	{
		LOG(L_ERR, "PDT:set_hash_domain(): wrong parameters\n");
		return -1;
	}
	h->sdomain.s = (char*)shm_malloc((s->len+1)*sizeof(char));
	if( h->sdomain.s == NULL )
	{
		LOG(L_ERR, "PDT:set_hash_domain: no more shm!\n");
		return -1;
	}
	memset(h->sdomain.s, 0, s->len+1);
	memcpy(h->sdomain.s, s->s, s->len);
	h->sdomain.len = s->len;
	return 0;
}

void free_hash(hash_t* hash)
{
	pd_op_t *op, *op_t;

	if(hash==NULL)
		return;

	free_hash_entries(hash->dhash, hash->hash_size);
	/* free the allocated sdomain */
	if(hash->sdomain.s!=NULL)
		shm_free(hash->sdomain.s);
	
	/* destroy diff list */
	for( op = hash->diff ; op ; op=op_t) {
		op_t = op->n;
		shm_free(op);
	}

	/* free next element in the list */
	free_hash(hash->next);
	shm_free(hash);
}

hash_list_t* init_hash_list(int hs_two_pow)
{
	hash_list_t* hl = NULL;
	int hash_size;

	if(hs_two_pow>MAX_HSIZE_TWO_POW || hs_two_pow<0)
		hash_size = MAX_HASH_SIZE;
	else
		hash_size = 1<<hs_two_pow;	

	/* space allocated in shared memory */
	hl = (hash_list_t*)shm_malloc(sizeof(hash_list_t));
	if(hl == NULL)
	{
		LOG(L_ERR, "PDT: init_hash_list: no more shm\n");
		return NULL;
	}
	memset(hl, 0, sizeof(hash_list_t));

	if(lock_init(&hl->hl_lock) == 0)
	{
		shm_free(hl);
		LOG(L_ERR, "PDT:init_hash_list: cannot init the hl_lock\n");
		return NULL;
	}
	hl->hash_size = hash_size;
	
	return hl;
}

void free_hash_list(hash_list_t* hl)
{
	if(hl==NULL)
		return;

	if(hl->hash!=NULL)
		free_hash(hl->hash);
	lock_destroy(&hl->hl_lock);
	shm_free(hl);
}


int add_to_hash(hash_t *hash, str *sp, str *sd)
{
	int hash_entry=0;
	unsigned int dhash;
	pd_t *it, *prev, *cell;
	
	if(hash==NULL || sp==NULL || sp->s==NULL
			|| sd==NULL || sd->s==NULL)
	{
		LOG(L_ERR, "PDT: add_to_hash: bad parameters\n");
		return -1;
	}

	dhash = pdt_compute_hash(sd);
	
	hash_entry = get_hash_entry(dhash, hash->hash_size);

	it = hash->dhash[hash_entry];

	prev = NULL;
	while(it!=NULL && it->dhash < dhash)
	{
		prev = it;
		it = it->n;
	}

	/* we need a new entry for this cell */
	cell = new_cell(sp, sd);	
	if(cell == NULL)
		return -1;

	if(prev)
		prev->n=cell;
	else
		hash->dhash[hash_entry]= cell;
	
	cell->p=prev;
	cell->n=it;
	
	if(it)
		it->p=cell;

	return 0;
}


int pdt_add_to_hash(hash_list_t *hl, str* sdomain, str *sp, str *sd)
{
	hash_t *it, *prev, *ph;
	
	if(hl==NULL || sdomain==NULL || sdomain->s==NULL
			|| sp==NULL || sp->s==NULL
			|| sd==NULL || sd->s==NULL)
	{
		LOG(L_ERR, "PDT: pdt_add_to_hash: bad parameters\n");
		return -1;
	}
	lock_get(&hl->hl_lock);

	/* search the it position where to insert new domain */
	it = hl->hash; 
	prev=NULL;
	while(it!=NULL && str_strcmp(&it->sdomain, sdomain)<0)
	{	
		prev=it;
		it=it->next;
	}

	/* add new sdomain, i.e. new entry in the hash list */
	if(it==NULL || str_strcmp(&it->sdomain, sdomain)>0)
	{
		/* !!!! check this hash size setting mode */
		ph = init_hash(hl->hash_size, sdomain);
		if(ph==NULL)
		{
			LOG(L_ERR, "PDT: pdt_add_to_hash: null pointer returned\n");
			goto error1;
		}
		
		if(add_to_hash(ph, sp, sd)<0)
		{
			LOG(L_ERR, "PDT: pdt_add_to_hash: could not add to hash\n");
			goto error;
		}

		if(prev==NULL)
		/* list initially empty */
			hl->hash = ph;
		else
			prev->next = ph;

		ph->next = it;
	} else {
		/* it is the entry of sdomain, just add a new prefix/domain pair
		 * to its hash
		 */
		if(add_to_hash(it, sp, sd)<0)
		{
			LOG(L_ERR, "PDT: pdt_add_to_hash: could not add to hash\n");
			goto error1;
		}
	}
	
	lock_release(&hl->hl_lock);
	return 0;

error:
	free_hash(ph);
error1:
	lock_release(&hl->hl_lock);
	return -1;

}

hash_t* pdt_search_hash(hash_list_t* hl, str *sd)
{
	hash_t* it;

	if(sd==NULL || sd->s==NULL || hl==NULL)
	{
		LOG(L_ERR, "PDT:pdt_search_hash: bad parameters\n");
		return NULL;
	}
	
	lock_get(&hl->hl_lock);

	/* search the it position where to insert new domain */
	it = hl->hash;
	while(it!=NULL && str_strcmp(&it->sdomain, sd)<0)
		it = it->next;

	if(it==NULL || str_strcmp(&it->sdomain, sd)>0)
	{
		lock_release(&hl->hl_lock);
		return NULL;
	}
	
	lock_release(&hl->hl_lock);
	return it;	
}


/* returns -1 if any error
 * returns 1 if does not exist in hash
 * returns 0 if deleted succesfully
 * */
int remove_from_hash(hash_t *hash, str *sd)
{
	int hash_entry=0;
	unsigned int dhash;
	pd_t *it, *prev;

	if(hash==NULL || sd==NULL || sd->s==NULL)
	{
		LOG(L_ERR, "PDT:pdt_remove_from_hash: bad parameters\n");
		return -1; /* error */
	}
	
	/* find the list where the cell must be */
	dhash = pdt_compute_hash(sd);
	hash_entry = get_hash_entry(dhash, hash->hash_size);


	/* first element of the list */	
	it = hash->dhash[hash_entry];

	/* find the cell in the list */
	/* a double linked list in the hash is kept alphabetically
	* or numerical ordered */    
	prev = NULL;
	while(it!=NULL)
	{
		if( it->dhash==dhash && it->domain.len==sd->len
				&& strncasecmp(it->domain.s, sd->s, sd->len)==0)
			break;
		prev = it;
		it = it->n;
	}

	if(it==NULL)
		return 1; /* does not exist in hash, nothing to delete */
	
	/* the prefix/domain pair exists and must be deleted */
	if(prev!=NULL)
		prev->n = it->n;
	else
		hash->dhash[hash_entry] = it->n;

	if(it->n)
		it->n->p = it->p;

	// check who should do it
	// free_cell(it);
	return 0;
	
}

/* returns -1 if any error
 * returns 1 if does not exist in hash
 * returns 0 if deleted succesfully
 * */
int pdt_remove_from_hash_list(hash_list_t *hl, str* sdomain, str *sd)
{
	hash_t *it;
	int ret;

	if(hl==NULL ||
			sd==NULL || sd->s==NULL ||
			sdomain==NULL || sdomain->s==NULL)
	{
		LOG(L_ERR, "PDT: pdt_remove_from_hash: bad parameters\n");
		return -1; /* wrong parameters, error */
	}

	lock_get(&hl->hl_lock);	
	
	/* search the it position where to remove from */
	it = hl->hash;
	while(it!=NULL && str_strcmp(&it->sdomain, sdomain)<0)
		it = it->next;
		
	/* sdomain not found, nothing to delete */
	if(it==NULL || str_strcmp(&it->sdomain, sdomain)>0)
	{
		lock_release(&hl->hl_lock);
		return 1; /* nothing to delete */
	}
	ret = remove_from_hash(it, sd);
	
	lock_release(&hl->hl_lock);
	
	return ret;	
}
/*
int pdt_remove_hash_from_hash_list(hash_list_t *hl, str* sdomain)
{
	hash_t *it, *prev, *ph;

	if(hl==NULL ||
			sdomain==NULL || sdomain->s==NULL)
	{
		LOG(L_ERR, "PDT: pdt_remove_from_hash: bad parameters\n");
		return -1;
	}

	lock_get(&hl->hl_lock);	
	// search the it position where to remove from 
	it = hl->hash;
	prev=NULL;
	while(it!=NULL && str_strcmp(&it->sdomain, sdomain)<0)
	{	
		prev = it;
		it = it->next;
	}

	// sdomain not found, nothing to delete 
	if(it==NULL || str_strcmp(&it->sdomain, sdomain)>0)
	{
		lock_release(&hl->hl_lock);
		return 0;
	}
	
	if(prev!=NULL)
	{
		prev->next = it->next;
		it->next = NULL;
		lock_release(&hl->hl_lock);
		free_hash(it);
		it=NULL;
		return 0;
	}

	// remove first element 
	prev = it->next;
	it->next = NULL;
	lock_release(&hl->hl_lock);
	free_hash(it);
	it=NULL;
	return 0;
}
*/

str* get_prefix(hash_t *ph, str* sd)
{
	int hash_entry;
	unsigned int dhash;
	pd_t* it;
	
	if(ph==NULL || ph->dhash==NULL || ph->hash_size>MAX_HASH_SIZE)
	{
		LOG(L_ERR, "PDT:pdt_get_prefix: bad parameters\n");
		return NULL;
	}

	dhash = pdt_compute_hash(sd);
	hash_entry = get_hash_entry(dhash, ph->hash_size);

	it = ph->dhash[hash_entry];
	while(it!=NULL && it->dhash<=dhash)
	{
		if(it->dhash==dhash && it->domain.len==sd->len
				&& strncasecmp(it->domain.s, sd->s, sd->len)==0)
			return &it->prefix;
		it = it->n;
	}

	return NULL;
}

str* pdt_get_prefix(hash_list_t *hl, str*sdomain, str* sd)
{
	hash_t *it;
	str *d;

	if(hl==NULL ||
			sd==NULL || sd->s==NULL ||
			sdomain==NULL || sdomain->s==NULL)
	{
		LOG(L_ERR, "PDT: pdt_get_prefix: bad parameters\n");
		return NULL;
	}

	lock_get(&hl->hl_lock);
	it = pdt_search_hash(hl, sdomain);
	if(it==NULL)
	{
		lock_release(&hl->hl_lock);
		return NULL;
	}
	d = get_prefix(it, sd);	
	lock_release(&hl->hl_lock);
	return d;	
}

int check_pd(hash_t *ph, str *sp, str *sd)
{
	unsigned int i;
	unsigned int dhash;
	pd_t* it;
	
	if(ph==NULL || sp==NULL || sd==NULL)
	{
		LOG(L_ERR, "PDT:check_pd: bad parameters\n");
		return -1;
	}
	dhash = pdt_compute_hash(sd);
	
	for(i=0; i<ph->hash_size; i++)
	{
		it = ph->dhash[i];
		while(it != NULL)
		{
			if((it->domain.len==sd->len
					&& strncasecmp(it->domain.s, sd->s, sd->len)==0)
				|| (it->prefix.len==sp->len
					&& strncasecmp(it->prefix.s, sp->s, sp->len)==0))
				return 1;
			
	    	it = it->n;
		}
    }

	return 0;
}

/* returns 
 *	1 if domain already exists 
 *  0 if domain does not exist
 *  -1 if any error
 * */
int pdt_check_pd(hash_list_t *hl, str* sdomain, str *sp, str *sd)
{
	hash_t *it;
	int d;

	if(hl==NULL ||
			sd==NULL || sd->s==NULL ||
			sdomain==NULL || sdomain->s==NULL)
	{
		LOG(L_ERR, "PDT: pdt_check_pd: bad parameters\n");
		return -1;
	}
	
	lock_get(&hl->hl_lock);

	/* search the it position */
	it = hl->hash;
	while(it!=NULL && str_strcmp(&it->sdomain, sdomain)<0)
		it = it->next;

	if(it==NULL || str_strcmp(&it->sdomain, sdomain)>0)
	{
		lock_release(&hl->hl_lock);
		return 0;
	}
	
	d = check_pd(it, sp, sd);	
	lock_release(&hl->hl_lock);

	return d;		
}

void pdt_print_hash_list(hash_list_t* hl)
{
	unsigned int i, count;
	pd_t *it;
	hash_t *hash;
	
	hash = hl->hash;
	lock_get(&hl->hl_lock);
	while(hash!=NULL)
	{
		DBG("PDT: print_hash: SDOMAIN=%.*s\n", 
				hash->sdomain.len, hash->sdomain.s);
		for(i=0; i<hash->hash_size; i++)
		{
			it = hash->dhash[i];
			DBG(" PDT:print_hash: entry<%d>:\n", i);
			count = 0;
			while(it!=NULL)
			{
				DBG("  PDT:print_hash: |Domain: %.*s |Code: %.*s | DHash:%u \n", it->domain.len, it->domain.s, it->prefix.len, it->prefix.s, it->dhash);
				it = it->n;
				count++;
			}

			DBG(" PDT:print_hash: ---- hash entry has %d records\n\n", count);
		
		}
		hash = hash->next;
	}
	
	lock_release(&hl->hl_lock);
}



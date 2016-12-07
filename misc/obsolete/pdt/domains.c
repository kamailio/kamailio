/**
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2008 iptelorg GmbH
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

 /**
  * History
  * -------
  * 2003-04-07: a structure for both hashes introduced (ramona)
  * 2005-01-26: double hash removed (ramona)
  *             FIFO operations are kept as a diff list (ramona)
  *             domain hash kept in share memory along with FIFO ops (ramona)
  * 
  */

#include <stdio.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../parser/parse_fline.h"
#include "../../lib/srdb2/db.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"

#include "domains.h"

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

	cell->dhash = pdt_compute_hash(cell->domain.s);
    
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
pd_entry_t* init_hash(unsigned int hash_size)
{
	int i, j;
	pd_entry_t *hash = NULL; 

	/* space for the hash is allocated in share memory */
	hash = (pd_entry_t*)shm_malloc(hash_size*sizeof(pd_entry_t));
	if(hash == NULL)
	{
		LOG(L_ERR, "PDT:init_hash: no more shm\n");
		return NULL;
	}
	memset(hash, 0, hash_size*sizeof(pd_entry_t));

	/* create mutex semaphores for each entry of the hash */
	for(i=0; i<hash_size; i++)
	{
		if(lock_init(&hash[i].lock) == 0)
		{
			LOG(L_ERR, "PDT:init_hash: cannot init the lock\n");
			goto error;
		}
	
		hash[i].e = NULL;
	}
	    
	/* the allocated hash */
	return hash;

error:
	for(j=0; j<i; j++)
		lock_destroy(&hash[j].lock);
	
	shm_free(hash);
	
	return NULL;
}


pdt_hash_t* pdt_init_hash(int hs_two_pow)
{
	pdt_hash_t* hash = NULL;
	int hash_size;

	if(hs_two_pow>MAX_HSIZE_TWO_POW || hs_two_pow<0)
		hash_size = MAX_HASH_SIZE;
	else
		hash_size = 1<<hs_two_pow;	


	/* space for the double_hash is allocated in share memory */
	hash = (pdt_hash_t*)shm_malloc(sizeof(pdt_hash_t));
	if(hash == NULL)
	{
		LOG(L_ERR, "PDT:pdt_init_hash: no more shm\n");
		return NULL;
	}

	if(lock_init(&hash->diff_lock) == 0)
	{
		shm_free(hash);
		LOG(L_ERR, "PDT:pdt_init_hash: cannot init the diff lock\n");
		return NULL;
	}
	
	if( (hash->dhash = init_hash(hash_size)) == NULL )
	{
		lock_destroy(&hash->diff_lock);
		shm_free(hash);
		LOG(L_ERR, "PDT:pdt_init_hash: no more shm!\n");
		return NULL;
	}
	
	hash->hash_size = hash_size;

	return hash;
}


void free_hash(pd_entry_t* hash, unsigned int hash_size)
{
    int   i;
    pd_t *tmp, *it;
    if(hash==NULL || hash_size<=0)
		return;
    
	for(i=0; i<hash_size; i++)
	{
		it = hash[i].e;
		while(it != NULL)
		{
	    	tmp = it->n;
			free_cell(it);
			it = tmp;
		}
		lock_destroy(&hash[i].lock);
    }
	shm_free(hash);
}

void pdt_free_hash(pdt_hash_t* hash)
{
	free_hash(hash->dhash, hash->hash_size);	
	lock_destroy(&hash->diff_lock);
	
	/* todo: destroy diff list */
	
	shm_free(hash);
}

int pdt_add_to_hash(pdt_hash_t *hash, str *sp, str *sd)
{
	int hash_entry=0;
	unsigned int dhash;
	pd_t *it, *tmp;
	pd_t *cell;
	
	if(hash==NULL || sp==NULL || sd==NULL)
	{
		LOG(L_ERR, "PDT:pdt_add_to_hash: bad parameters\n");
		return -1;
	}
    
	dhash = pdt_compute_hash(sd->s);
	hash_entry = get_hash_entry(dhash, hash->hash_size);

	lock_get(&hash->dhash[hash_entry].lock);
	
	it = hash->dhash[hash_entry].e;

	tmp = NULL;
	while(it!=NULL && it->dhash < dhash)
	{
		tmp = it;
		it = it->n;
	}
    
	/* we need a new entry for this cell */
	cell = new_cell(sp, sd);	
	if(cell == NULL)
	{
		lock_release(&hash->dhash[hash_entry].lock);
		return -1;
	}
	

	if(tmp)
		tmp->n=cell;
	else
		hash->dhash[hash_entry].e = cell;
	
	cell->p=tmp;
	cell->n=it;
	
	if(it)
		it->p=cell;

	lock_release(&hash->dhash[hash_entry].lock);

	return 0;
}

int pdt_remove_from_hash(pdt_hash_t *hash, str *sd)
{
	int hash_entry=0;
	unsigned int dhash;
	pd_t *it, *tmp;

	if(sd==NULL)
		return 0;	
		
	if(hash==NULL)
	{
		LOG(L_ERR, "PDT:pdt_remove_from_hash: bad parameters\n");
		return -1;
	}
	
	/* find the list where the cell must be */
	dhash = pdt_compute_hash(sd->s);
	hash_entry = get_hash_entry(dhash, hash->hash_size);


	lock_get(&hash->dhash[hash_entry].lock);
	
	/* first element of the list */	
	it = hash->dhash[hash_entry].e;

	/* find the cell in the list */
	/* a double linked list in the hash is kept alphabetically
	* or numerical ordered */    
	tmp = NULL;
	while(it!=NULL)
	{
		if( it->dhash==dhash && it->domain.len==sd->len
				&& strncasecmp(it->domain.s, sd->s, sd->len)==0)
			break;
		tmp = it;
		it = it->n;
	}
	
	if(it!=NULL)
	{
		if(tmp!=NULL)
			tmp->n = it->n;
		else
			hash->dhash[hash_entry].e = it->n;

		if(it->n)
			it->n->p = it->p;

		free_cell(it);
	}
	
	lock_release(&hash->dhash[hash_entry].lock);

	return 0;
}

str* pdt_get_prefix(pdt_hash_t *ph, str* sd)
{
	int hash_entry;
	unsigned int dhash;
	pd_t* it;
	
	if(ph==NULL || ph->dhash==NULL || ph->hash_size>MAX_HASH_SIZE)
	{
		LOG(L_ERR, "PDT:pdt_get_prefix: bad parameters\n");
		return NULL;
	}

	dhash = pdt_compute_hash(sd->s);
	hash_entry = get_hash_entry(dhash, ph->hash_size);

	lock_get(&ph->dhash[hash_entry].lock);
	
	it = ph->dhash[hash_entry].e;
	while(it!=NULL && it->dhash<=dhash)
	{
		if(it->dhash==dhash && it->domain.len==sd->len
				&& strncasecmp(it->domain.s, sd->s, sd->len)==0)
		{
			lock_release(&ph->dhash[hash_entry].lock);
			return &it->prefix;
		}
		it = it->n;
	}

	lock_release(&ph->dhash[hash_entry].lock);

	return NULL;
}

int pdt_check_pd(pdt_hash_t *ph, str *sp, str *sd)
{
	int i;
	unsigned int dhash;
	pd_t* it;
	
	if(ph==NULL || sp==NULL || sd==NULL)
	{
		LOG(L_ERR, "PDT:pdt_check_pd: bad parameters\n");
		return -1;
	}
    
	dhash = pdt_compute_hash(sd->s);
	
	for(i=0; i<ph->hash_size; i++)
	{
		lock_get(&ph->dhash[i].lock);
		it = ph->dhash[i].e;
		while(it != NULL)
		{
			if((it->domain.len==sd->len
					&& strncasecmp(it->domain.s, sd->s, sd->len)==0)
				|| (it->prefix.len==sp->len
					&& strncasecmp(it->prefix.s, sp->s, sp->len)==0))
			{
				lock_release(&ph->dhash[i].lock);
				return 1;
			}
			
	    	it = it->n;
		}
		lock_release(&ph->dhash[i].lock);
    }

	return 0;
}

void pdt_print_hash(pdt_hash_t* hash)
{
	int i, count;
	pd_t *it;
	
	if(hash==NULL)
	{
		DBG("PDT:pdt_print_hash: empty...\n");
		return;
	}
	
	for(i=0; i<hash->hash_size; i++)
	{
		lock_get(&hash->dhash[i].lock);

		it = hash->dhash[i].e;
		DBG("PDT:pdt_print_hash: entry<%d>:\n", i);
		count = 0;
		while(it!=NULL)
		{
			DBG("PDT:pdt_print_hash: |Domain: %.*s |Code: %.*s | DHash:%u \n",
					it->domain.len, it->domain.s,
					it->prefix.len, it->prefix.s, it->dhash);
			it = it->n;
			count++;
		}

		lock_release(&hash->dhash[i].lock);

		DBG("PDT:pdt_print_hash: ---- has %d records\n\n", count);
		
	}

}

/* be sure s!=NULL */
/* compute a hash value for a string, knowing also the hash dimension */
unsigned int pdt_compute_hash(char* s)
{
	#define h_inc h+=v^(v>>3);
		
	char* p;
	register unsigned v;
	register unsigned h;
	int len;

	len = strlen(s);
	
	h=0;
	for(p=s; p<=(s+len-4); p+=4)
	{
		v=(*p<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
		h_inc;
	}
	
	v=0;
	for(;p<(s+len); p++)
	{
		v<<=8;
		v+=*p;
	}
	h_inc;

	return h;
}



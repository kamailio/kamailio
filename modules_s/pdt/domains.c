/**
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

 /**
  * History
  * -------
  * 2003-04-07: a structure for both hashes introduced (ramona)
  * 
  */

#include <stdio.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../parser/parse_fline.h"
#include "../../db/db.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"

#include "domains.h"

#define get_hash_entry(c,s) (c)&((s)-1)

/* return a new cell stored in share memory for this domain */
dc_t* new_cell(char* domain, code_t code)
{
	dc_t* cell = NULL; 
	
	if(!domain)
		return NULL;
	
	/* the cell is in share memory */
	cell = (dc_t*)shm_malloc(sizeof(dc_t));
    
	/* if there is no space return just NULL */
	if(!cell)
		return NULL;
	
	/* otherwise, fill in the structure fields */
	/* domain name */
	cell->domain = (char*)shm_malloc((1+strlen(domain))*sizeof(char));
	strcpy(cell->domain, domain);

	/* domain code */
	cell->code = code; 

	cell->dhash = compute_hash(domain);
    
	/* return the newly allocated in share memory cell */
	return cell;
}

void free_cell(dc_t* cell)
{
	/* if it is not already NULL */
	if(!cell)
		return;	
	
	if(cell->domain)
		shm_free(cell->domain);
		
	/* free the memory */
	shm_free(cell);
}

entry_t* new_entry(dc_t* cell)
{
    entry_t* e = NULL;

    if(!cell)
	return NULL;
    
    e = (entry_t*)shm_malloc(sizeof(entry_t));
    if(!e)
	return NULL;        

    e->dc = cell;
    e->p = NULL;
    e->n = NULL;

    /* return the newly allocated in share memory entry */
    return e;
}

/* free allocated space for an entry */
void free_entry(entry_t *e, int erase_cell)
{
	if(!e)
		return;
	
	if(erase_cell && e->dc)
		free_cell(e->dc);
	
	shm_free(e);
}

/* returns a pointer to a hashtable */
h_entry_t* init_hash(unsigned int hash_size)
{
	int i, j;
	
	/* initialized to NULL */
	h_entry_t *hash = NULL; 

	/* space for the hash is allocated in share memory */
	hash = (h_entry_t*)shm_malloc(hash_size*sizeof(h_entry_t));
	if(hash == NULL)
		return NULL;

	/* create mutex semaphores for each entry of the hash */
	for(i=0; i<hash_size; i++)
	{
		if(lock_init(&hash[i].lock) == 0)
			goto error;
	
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


double_hash_t* init_double_hash(int hs_two_pow)
{
	double_hash_t* hash = NULL;
	int hash_size;

	if(hs_two_pow>MAX_HSIZE_TWO_POW || hs_two_pow<0)
		hash_size = MAX_HASH_SIZE;
	else
		hash_size = 1<<hs_two_pow;	


	/* space for the double_hash is allocated in share memory */
	hash = (double_hash_t*)shm_malloc(sizeof(double_hash_t));
	if(hash == NULL)
		return NULL;

	if( (hash->dhash = init_hash(hash_size)) == NULL )
	{
		shm_free(hash);
		return NULL;
	}
	
	if( (hash->chash = init_hash(hash_size)) == NULL )
	{
		free_hash(hash->dhash, hash_size, ERASE_CELL);
		shm_free(hash);
		return NULL;
	}

	hash->hash_size = hash_size;

	return hash;
}


void free_hash(h_entry_t* hash, unsigned int hash_size, int do_cell)
{
    int   i;   /* index for hash entries */
    entry_t *tmp, /* just a temporary variable */
			*it;  /* iterator through cells of a has entry */
    if(!hash || hash_size<=0)
		return;
    
	/* free memory occupied by all hash entries */
	for(i=0; i<hash_size; i++)
	{
		/* iterator through the i-th entry of the hash */
		it = hash[i].e;
		
		/* as long as we have a cell */
		while(it != NULL)
		{
			/* retains the next cell */
	    	tmp = it->n;
			free_entry(it, do_cell);
			/* the iterator points up to the next cell */
			it = tmp;
		}
		lock_destroy(&hash[i].lock);
    }

	shm_free(hash);
}

void free_double_hash(double_hash_t* hash)
{
	free_hash(hash->dhash, hash->hash_size, ERASE_CELL);	
	free_hash(hash->chash, hash->hash_size, NOT_ERASE_CELL);
	shm_free(hash);
}

int add_to_double_hash(double_hash_t* hash, dc_t* cell)
{
	if(add_to_hash(hash->dhash, hash->hash_size, cell, DHASH)<0)
		return -1;
	
	if(add_to_hash(hash->chash, hash->hash_size, cell, CHASH)<0)
	{
		remove_from_hash(hash->dhash, hash->hash_size, cell, DHASH);
		return -1;
	}	
	
	return 0;
}

int add_to_hash(h_entry_t* hash, unsigned int hash_size, dc_t* cell, int type)
{
	int hash_entry=0;
	entry_t *it, *tmp;
	entry_t * e;
	
	if(!hash || !cell || hash_size>MAX_HASH_SIZE)
		return -1;
    
	/* find the list where we have to introduce the new cell */
	if(type==DHASH)
		hash_entry = get_hash_entry(cell->dhash, hash_size);
	else 
	if(type==CHASH)
		hash_entry = get_hash_entry(cell->code, hash_size);
	else
		return -1;	


	lock_get(&hash[hash_entry].lock);
	
	/* first element of the list */	
	it = hash[hash_entry].e;

	/* find the place where to insert the new cell */
	/* a double linked list in the hash is kept alphabetically
	 * or numerical ordered */    
	if(type==DHASH)
	{		
		tmp = NULL;
		while(it!=NULL && it->dc->dhash < cell->dhash)
		{
			tmp = it;
			it = it->n;
		}
	}
	else
	{
		tmp = NULL;
		while( it!=NULL && it->dc->code < cell->code )
		{
			tmp = it;
			it = it->n;
		}
	}
    
	/* we need a new entry for this cell */
	e = new_entry(cell);	
	if(e == NULL)
	{
		lock_release(&hash[hash_entry].lock);
		return -1;
	}
	

	if(tmp)
		tmp->n=e;
	else
		hash[hash_entry].e = e;
	
	e->p=tmp;
	e->n=it;
	
	if(it)
	it->p=e;

	lock_release(&hash[hash_entry].lock);

	return 0;
}

int remove_from_double_hash(double_hash_t* hash, dc_t* cell)
{
	if(!cell)
		return 0;	
		
	if(!hash)
		return -1;	

	/* DHASH frees the memory of cell */
	remove_from_hash(hash->dhash, hash->hash_size, cell, DHASH);
	remove_from_hash(hash->chash, hash->hash_size, cell, CHASH);

	return 0;	
}

int remove_from_hash(h_entry_t* hash, unsigned int hash_size, dc_t* cell, int type)
{
	int hash_entry=0;
	entry_t *it, *tmp;
	
	if(!cell)
		return 0;
	
	if(!hash) 
		return -1;
    
	/* find the list where the cell must be */
	if(type==DHASH)
		hash_entry = get_hash_entry(cell->dhash, hash_size);
	else 
		if(type==CHASH)
			hash_entry = get_hash_entry(cell->code, hash_size);
	else
		return -1;	


	lock_get(&hash[hash_entry].lock);
	
	/* first element of the list */	
	it = hash[hash_entry].e;

	/* find the cell in the list */
	/* a double linked list in the hash is kept alphabetically
	* or numerical ordered */    
	tmp = NULL;
	while(it!=NULL && it->dc != cell)
	{
		tmp = it;
		it = it->n;
	}
	
	if(it)
	{
		if(tmp)
			tmp->n = it->n;
		else
			hash[hash_entry].e = it->n;

		if(it->n)
			it->n->p = it->p;

		free_entry(it, (type==DHASH?ERASE_CELL:NOT_ERASE_CELL));
	}
	
	lock_release(&hash[hash_entry].lock);

	return 0;
}

char* get_domain_from_hash(h_entry_t* hash, unsigned int hash_size, code_t code)
{
	int hash_entry;
	entry_t* it;
	
	if(!hash || hash_size>MAX_HASH_SIZE)
		return NULL;
	
	/* find out the list in the hash where this code could be */
	hash_entry = get_hash_entry(code, hash_size);

	lock_get(&hash[hash_entry].lock);
	
	/* parsing the list */
	it = hash[hash_entry].e;
	while(it!=NULL && it->dc->code<code)
			it = it->n;

	lock_release(&hash[hash_entry].lock);

	/* the code does not exist */	
	if(it==NULL || it->dc->code > code )
		return NULL;
	else
		/* returns the associated domain name */	
		return it->dc->domain;	
			
}

dc_t* get_code_from_hash(h_entry_t* hash, unsigned int hash_size, char* domain)
{
	int hash_entry;
	unsigned int dhash;
	entry_t* it;
	
	if(!hash || hash_size>MAX_HASH_SIZE)
		return NULL;
	
	dhash = compute_hash(domain);
	hash_entry = get_hash_entry(dhash, hash_size);

	lock_get(&hash[hash_entry].lock);
	
	/* parsing the list */
	it = hash[hash_entry].e;
	while(it!=NULL && it->dc->dhash<=dhash)
	{
		if(it->dc->dhash == dhash && strcasecmp(it->dc->domain, domain)==0)
		{
			lock_release(&hash[hash_entry].lock);
			return it->dc;
		}
		it = it->n;
	}

	lock_release(&hash[hash_entry].lock);

	return NULL;
}

void print_hash(h_entry_t* hash, unsigned int hash_size)
{
	int i, count;
	entry_t *it;
	
	if(!hash || hash_size>MAX_HASH_SIZE)
		return;

	for(i=0; i<hash_size; i++)
	{
		lock_get(&hash[i].lock);

		it = hash[i].e;
		printf("Entry<%d>:\n", i);
		count = 0;
		while(it!=NULL)
		{
			printf("|Domain: %s |Code: %d | DHash:%u \n",
					it->dc->domain, it->dc->code, it->dc->dhash);
			it = it->n;
			count++;
		}

		lock_release(&hash[i].lock);

		printf("---- has %d records\n\n", count);
		
	}

}

/* be sure s!=NULL */
/* compute a hash value for a string, knowing also the hash dimension */
unsigned int compute_hash(char* s)
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



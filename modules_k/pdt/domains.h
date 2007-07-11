/**
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2005 Voice System SRL (Voice-System.RO)
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
		       
#ifndef _DOMAINS_H_
#define _DOMAINS_H_

#include "../../str.h"

#define MAX_HSIZE_TWO_POW 16
#define MAX_HASH_SIZE 1<<MAX_HSIZE_TWO_POW 

#define PDT_ADD			1
#define PDT_DELETE		2

typedef struct _pd
{
    str prefix;
    str domain;
	int flag;
    unsigned int dhash;
    struct _pd *p;
    struct _pd *n;
} pd_t;

typedef struct _pd_op
{
	pd_t *cell;
	int op;
	int id;
    int count;
    struct _pd_op *p;
    struct _pd_op *n;
} pd_op_t;

typedef struct _hash
{
	str sdomain;
	unsigned int hash_size;
	pd_t** dhash;
	struct _hash *next;

	pd_op_t *diff;
	int max_id;
} hash_t;

typedef struct _hash_list
{
    hash_t	*hash;
	gen_lock_t hl_lock;	

	unsigned int hash_size;
} hash_list_t;

pd_t* new_cell(str* p, str *d);
void free_cell(pd_t *cell);

pd_op_t* new_pd_op(pd_t *cell, int id, int op);
void free_pd_op(pd_op_t *pdo);

pd_t** init_hash_entries(unsigned int hash_size);
void free_hash_entries(pd_t** hash, unsigned int hash_size);

hash_t* init_hash(int hash_size, str *sdomain);
void free_hash(hash_t* hash);

hash_list_t* init_hash_list(int hash_size);
void free_hash_list(hash_list_t* hl);

int add_to_hash(hash_t *hash, str *sp, str *sd);
int pdt_add_to_hash(hash_list_t *hash, str* sdomain, str *sp, str *sd);

hash_t* pdt_search_hash(hash_list_t*, str *d);

int remove_from_hash(hash_t *ph, str *sd);
int pdt_remove_from_hash_list(hash_list_t *hl, str* sdomain, str *sd);

str* pdt_get_prefix(hash_list_t *ph, str *sdomain, str* sd);

int check_pd(hash_t *ph, str *sp, str *sd);
int pdt_check_pd(hash_list_t *hash, str* sdomain, str *sp, str *sd);

void pdt_print_hash_list(hash_list_t *hl);

#endif

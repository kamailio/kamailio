/*
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
		       
#ifndef _DOMAINS_H_
#define _DOMAINS_H_

#include "../../str.h"

#define MAX_HSIZE_TWO_POW 20
#define MAX_HASH_SIZE 1<<MAX_HSIZE_TWO_POW 

#define PDT_ADD			1
#define PDT_DELETE		2

#define get_hash_entry(c,s) (c)&((s)-1)

typedef struct _pd
{
    str prefix;
    str domain;
	int flag;
    unsigned int dhash;
    struct _pd *p;
    struct _pd *n;
} pd_t;

typedef struct _pd_entry
{
    gen_lock_t lock;
    pd_t *e;
} pd_entry_t;

typedef struct _pd_op
{
    pd_t *cell;
	int op;
	int id;
    int count;
    struct _pd_op *p;
    struct _pd_op *n;
} pd_op_t;

typedef struct _pdt_hash
{
	pd_entry_t* dhash;
	unsigned int hash_size;

	pd_op_t *diff;
    gen_lock_t diff_lock;
	int max_id;
	int workers;
} pdt_hash_t;

pd_t* new_cell(str* p, str *d);
void free_cell(pd_t *cell);

pd_op_t* new_pd_op(pd_t *cell, int id, int op);
void free_pd_op(pd_op_t *pdo);

int pdt_add_to_hash(pdt_hash_t *ph, str *sp, str *sd);
int pdt_remove_from_hash(pdt_hash_t *ph, str *sd);

str* pdt_get_prefix(pdt_hash_t *ph, str* sd);
int pdt_check_pd(pdt_hash_t *ph, str *sp, str *sd);

void pdt_print_hash(pdt_hash_t *ph);

pdt_hash_t* pdt_init_hash(int hash_size);
void pdt_free_hash(pdt_hash_t* ph);

unsigned int pdt_compute_hash(char *s);

#endif

/**
 * Copyright (C) 2001-2003 Fhg Fokus
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
		       
#ifndef _DOMAINS_H_
#define _DOMAINS_H_

#define DHASH 0
#define CHASH 1
#define MAX_HSIZE_TWO_POW 20
#define MAX_HASH_SIZE 1<<MAX_HSIZE_TWO_POW 
#define MAX_CODE	0xFFFFFFFF
#define MAX_CODE_10	MAX_CODE/10
#define MAX_CODE_R  MAX_CODE_10*10

#define ERASE_CELL 1
#define NOT_ERASE_CELL 0

typedef unsigned int code_t;

typedef struct _dc {
    char* domain;
    code_t code;
    unsigned int dhash;
} dc_t;

typedef struct _entry {
    dc_t *dc;
    struct _entry *p;
    struct _entry *n;
} entry_t;

typedef struct _h_entry	{
    gen_lock_t lock;
    entry_t * e;
} h_entry_t;

typedef struct _double_hash {
	h_entry_t* dhash;
	h_entry_t* chash;
	unsigned int hash_size;
} double_hash_t;

dc_t* new_cell(char* domain, code_t code);
void free_cell(dc_t* cell);

entry_t* new_entry(dc_t* cell);
void free_entry(entry_t *e, int erase_cell);

h_entry_t* init_hash(unsigned int hash_size);
void free_hash(h_entry_t* hash, unsigned int hash_size, int do_cell);

unsigned int compute_hash(char *s);
int add_to_hash(h_entry_t* hash, unsigned int hash_size, dc_t* cell, int type);
int remove_from_hash(h_entry_t* hash, unsigned int hash_size, dc_t* cell, int type);

char* get_domain_from_hash(h_entry_t* hash, unsigned int hash_size, code_t code);
dc_t* get_code_from_hash(h_entry_t* hash, unsigned int hash_size, char* domain);

void print_hash(h_entry_t* hash, unsigned int hash_size);

/* FIFO function */
int get_domainprefix(FILE *stream, char* response_file);

/* update the new_uri field of the sip_msg structure */
int update_new_uri(struct sip_msg *msg, int code_len, char* host_port);


double_hash_t* init_double_hash(int hash_size);
void free_double_hash(double_hash_t* hash);
int add_to_double_hash(double_hash_t* hash, dc_t* cell);
int remove_from_double_hash(double_hash_t* hash, dc_t* cell);

#endif

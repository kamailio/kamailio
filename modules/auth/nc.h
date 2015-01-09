/*
 * Digest Authentication Module
 * nonce-count (nc) tracking
 *
 * Copyright (C) 2008 iptelorg GmbH
 * 
 * This file is part of Kamailio, a free SIP server.
 * 
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 * 
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*
 * Defines: 
 *  USE_NC   - if not defined no NC specific code will be compiled
 */

#ifndef _nc_h
#define _nc_h

extern int nc_enabled;

/* instead of storing only the 2^k size we store also k
 * for faster operations */
extern unsigned nc_array_k;    /* array size bits (k in 2^k) */
extern unsigned nc_array_size; /* 2^k == 1<<nc_array_bits */

#ifdef USE_NC

#include "nid.h" /* nid_t */
#include "../../atomic_ops.h"


/* default number of maximum in-flight nonces */
#define DEFAULT_NC_ARRAY_SIZE 1024*1024 /* uses 1Mb of memory */
#define MIN_NC_ARRAY_SIZE        102400 /* warn if size < 100k */
#define MAX_NC_ARRAY_SIZE 1024*1024*1024 /* warn if size > 1Gb */

#define MIN_NC_ARRAY_PARTITION   65536 /* warn if a partition is < 65k */


typedef unsigned char nc_t;

int init_nonce_count();
void destroy_nonce_count();


enum nc_check_ret{ 
	NC_OK=0, NC_INV_POOL=-1, NC_ID_OVERFLOW=-2, NC_TOO_BIG=-3, 
	NC_REPLAY=-4 
};

/* check if nonce-count nc w/ index i is expected/valid and record its
 * value */
enum nc_check_ret nc_check_val(nid_t i, unsigned pool, unsigned int nc);

/* re-init the stored nc for nonce id in pool pool_no */
nid_t nc_new(nid_t id, unsigned char pool_no);

#endif /* USE_NC */
#endif /* _nc_h */


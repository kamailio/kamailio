/*
 * Digest Authentication Module
 * nonce id and pool management (stuff common to nonce-count and one
 * time nonces)
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
 *  USE_NC, USE_OT_NONCE  - if neither of them defined no code will be 
 *                          compiled
 */

#ifndef _nid_h
#define _nid_h

extern unsigned nid_pool_no; /* number of index pools */

#if defined USE_NC || defined USE_OT_NONCE

#include "../../atomic_ops.h"
#include "../../pt.h" /* process_no */

/* id incremenet, to avoid cacheline ping-pong and cover all the
 * array locations it should be a number prime with the array size and
 * bigger then the cacheline. Since this is used also for onetime nonces
 * => NID_INC/8 > CACHELINE
 * This number also limits the maximum pool/partition size, since the
 * id overflow check checks if crt_id - nonce_id >= partition_size*NID_INC
 * => maximum partition size is (nid_t)(-1)/NID_INC*/
#define NID_INC 257

#define DEFAULT_NID_POOL_SIZE 1
#define MAX_NID_POOL_SIZE    64 /* max. 6 bits used for the pool no*/

#define CACHELINE_SIZE 256 /* more then most real-word cachelines */

/* if larger tables are needed (see NID_INC comments above), consider
 * switching to unsigned long long */
typedef unsigned int nid_t;

struct pool_index{
	atomic_t id;
	char pad[CACHELINE_SIZE-sizeof(atomic_t)];/* padding to cacheline size */
};

extern struct pool_index* nid_crt;


/* instead of storing only the 2^k size we store also k
 * for faster operations */
extern unsigned int nid_pool_k; /* pools no in bits (k in 2^k) */
extern unsigned int nid_pool_mask; /* mask for computing the current pool*/

int init_nonce_id();
void destroy_nonce_id();


/* get current index in pool p */
#define nid_get(p) \
	atomic_get(&nid_crt[(p)].id)

/* get pool for the current process */
#define nid_get_pool()  (process_no & nid_pool_mask)

/* inc the specified index and return its new value */
#define nid_inc(pool) \
	((nid_t)atomic_add(&nid_crt[(pool)].id, NID_INC))

#endif /* #if defined USE_NC || defined USE_OT_NONCE */
#endif /* _nid_h */

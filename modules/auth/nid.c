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

unsigned int nid_pool_no; /* number of index pools, 2^k */

#if defined USE_NC || defined USE_OT_NONCE

#include "nid.h"
#include "../../dprint.h"
#include "../../bit_scan.h"

struct pool_index* nid_crt=0;


/* instead of storing only the 2^k size we store also k
 * for faster operations */
unsigned int nid_pool_k; /* pools no in bits (k in 2^k) */
unsigned int nid_pool_mask; /* mask for computing the current pool*/



/* returns -1 on error, 0 on success */
int init_nonce_id()
{
	unsigned pool_no, r;
	
	
	if (nid_crt!=0)
		return 0; /* already init */
	if (nid_pool_no==0){
		nid_pool_no=DEFAULT_NID_POOL_SIZE;
	}
	if (nid_pool_no>MAX_NID_POOL_SIZE){
		WARN("auth: nid_pool_no too big, truncating to %d\n",
				MAX_NID_POOL_SIZE);
		nid_pool_no=MAX_NID_POOL_SIZE;
	}
	nid_pool_k=bit_scan_reverse32(nid_pool_no);
	nid_pool_mask=(1<<nid_pool_k)-1;
	pool_no=1UL<<nid_pool_k; /* ROUNDDOWN to 2^k */
	if (pool_no!=nid_pool_no){
		INFO("auth: nid_pool_no rounded down to %d\n", pool_no);
	}
	nid_pool_no=pool_no;
	
	nid_crt=shm_malloc(sizeof(*nid_crt)*nid_pool_no);
	if (nid_crt==0){
		ERR("auth: init_nonce_id: memory allocation failure\n");
		return -1;
	}
	/*  init nc_crt_id with random values */
	for (r=0; r<nid_pool_no; r++)
		atomic_set(&nid_crt[r].id, random());
	return 0;
/*
error:
	destroy_nonce_id();
	return -1;
*/
}



void destroy_nonce_id()
{
	if (nid_crt){
		shm_free(nid_crt);
		nid_crt=0;
	}
}

#endif  /*if  defined USE_NC || defined USE_OT_NONCE */

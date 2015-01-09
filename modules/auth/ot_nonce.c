/*
 * Digest Authentication Module
 * 
 * one-time nonce support
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

int otn_enabled=0;
unsigned otn_in_flight_k;  /* max in-flight nonces order (k in 2^k) */
unsigned otn_in_flight_no; /* 2^k == 1<<oth_in_flight_k */

#ifdef USE_OT_NONCE

#include "ot_nonce.h"
#include "nid.h"
#include "../../dprint.h"
#include "../../bit_scan.h"
#include "../../atomic_ops.h"
#include "../../ut.h" /* ROUNDUP...*/
#include "../../mem/shm_mem.h" /* shm_available() */
#include <stdlib.h> /* random() */
#include <string.h> /* memset() */
#include <assert.h>

static otn_cell_t * otn_array=0;


unsigned otn_partition_size; /* partition==otn_in_flight_no/nid_pool_no*/
unsigned otn_partition_k;    /* k such that 2^k==otn_partition_size */
unsigned otn_partition_mask; /* mask for computing the real idx. inside
							   one partition */


/* returns -1 on error, 0 on success */
int init_ot_nonce()
{
	unsigned long size;
	unsigned long max_mem;
	unsigned orig_array_size;


	if (nid_crt==0){
		BUG("auth: init_ot_nonce: nonce index must be "
				"initialized first (see init_nonce_id())\n");
		return -1;
	}
	orig_array_size=otn_in_flight_no;
	if (otn_in_flight_k==0){
		if (otn_in_flight_no==0){
			otn_in_flight_no=DEFAULT_OTN_IN_FLIGHT;
		}
		otn_in_flight_k=bit_scan_reverse32(otn_in_flight_no);
	}
	size=1UL<<otn_in_flight_k; /* ROUNDDOWN to 2^otn_in_flight_k */
	if (size < MIN_OTN_IN_FLIGHT){
		WARN("auth: one-time-nonce maximum in-flight nonces is very low (%d),"
				" consider increasing otn_in_flight_no to at least %d\n",
				orig_array_size, MIN_OTN_IN_FLIGHT);
	}
	if (size > MAX_OTN_IN_FLIGHT){
		WARN("auth: one-time-nonce maximum in-flight nonces is too high (%d),"
				" consider decreasing otn_in_flight_no to at least %d\n",
				orig_array_size, MAX_OTN_IN_FLIGHT);
	}
	if (size!=otn_in_flight_no){
		if (orig_array_size!=0)
			INFO("auth: otn_in_flight_no rounded down to %ld\n", size);
		else
			INFO("auth: otn_in_flight_no set to %ld\n", size);
	}
	max_mem=shm_available();
	if (size/8 >= max_mem){
		ERR("auth: otn_in_flight_no (%ld) is too big for the configured "
				"amount of shared memory (%ld bytes)\n", size, max_mem);
		return -1;
	}else if (size/8 >= max_mem/2){
		WARN("auth: the currently configured otn_in_flight_no (%ld)  "
				"would use more then 50%% of the available shared"
				" memory(%ld bytes)\n", size, max_mem);
	}
	otn_in_flight_no=size;
	
	if (nid_pool_no>=otn_in_flight_no/(8*sizeof(otn_cell_t))){
		ERR("auth: nid_pool_no (%d) too high for the configured "
				"otn_in_flight_no (%d)\n", nid_pool_no, otn_in_flight_no);
		return -1;
	}
	otn_partition_size=otn_in_flight_no >> nid_pool_k;
	otn_partition_k=otn_in_flight_k-nid_pool_k;
	otn_partition_mask=(1<<otn_partition_k)-1;
	assert(otn_partition_size == otn_in_flight_no/nid_pool_no);
	assert(1<<(otn_partition_k+nid_pool_k) == otn_in_flight_no);
	
	if ((nid_t)otn_partition_size >= ((nid_t)(-1)/NID_INC)){
		ERR("auth: otn_in_flight_no too big, try decreasing it or increasing"
				"the number of pools/partitions, such that "
				"otn_in_flight_no/nid_pool_no < %d\n",
				(unsigned int)((nid_t)(-1)/NID_INC));
		return -1;
	}
	if (otn_partition_size  < MIN_OTN_PARTITION){
		WARN("auth: one-time-nonces in-flight nonces very low,"
				" consider either decreasing nid_pool_no (%d) or "
				" increasing otn_array_size (%d) such that "
				"otn_array_size/nid_pool_no >= %d\n",
				nid_pool_no, orig_array_size, MIN_OTN_PARTITION);
	}
	
	
	/*  array size should be multiple of sizeof(otn_cell_t) since we
	 *  access it as an otn_cell_t array */
	otn_array=shm_malloc(ROUND2TYPE((otn_in_flight_no+7)/8, otn_cell_t));
	if (otn_array==0){
		ERR("auth: init_ot_nonce: memory allocation failure, consider"
				" either decreasing otn_in_flight_no of increasing the"
				" the shared memory ammount\n");
		goto error;
	}
	/* init the otn_array with 1 for each bit, to avoid replay attacks after
	 * ser restarts ) */
	memset(otn_array, 0xff, ROUND2TYPE((otn_in_flight_no+7)/8, otn_cell_t));
	return 0;
error:
	destroy_ot_nonce();
	return -1;
}



void destroy_ot_nonce()
{
	if (otn_array){
		shm_free(otn_array);
		otn_array=0;
	}
}

/* given the nonce id i and pool/partition p, produces a bit index in the
 * array  partition corresponding to p.
 * WARNING: the result is the _bit_ index and not the array cell index
 */
#define get_otn_array_bit_idx(i,p) \
	(((i) & otn_partition_mask)+((p)<<otn_partition_k))

/* get the real array cell corresponding to a certain bit index */
#define get_otn_array_cell_idx(pos) \
	((pos)/(sizeof(otn_cell_t)*8))

/* get the bit position inside an otn_array cell
 * (pos can be obtained from a nonce id with get_otn_array_bit_idx(i, p),
 *  see above) */
#define get_otn_cell_bit(pos) \
	((pos)%(sizeof(otn_cell_t)*8))

/* returns true if the crt_idx > idx with at least  otn_partition_size
 * WARNING: NID_INC * otn_partition_size must fit inside an nidx_t*/
#define  otn_id_check_overflow(id,  pool) \
	((nid_t)(nid_get((pool))-(id)) >= \
	 	((nid_t)NID_INC*otn_partition_size))

/* re-init the stored nc for nonce id in pool p */
nid_t otn_new(nid_t id, unsigned char p)
{
	unsigned int i;
	unsigned  n, b;
	
	n=get_otn_array_bit_idx(id, p); /* n-th bit */
	i=get_otn_array_cell_idx(n);    /* aray index i, corresponding to n */
	b=get_otn_cell_bit(n);          /* bit pos corresponding to n */
	/* new_value = old_value with the corresponding bit zeroed */
#ifdef OTN_CELL_T_LONG
	atomic_and_long((long*)&otn_array[i],  ~((otn_cell_t)1<<b));
#else
	atomic_and_int((int*)&otn_array[i],  ~((otn_cell_t)1<<b));
#endif /* OTN_CELL_T_LONG */
	return id;
}



/* check if nonce w/ index i is expected/valid and if so marked it "seen"
 * returns: 0 - ok, < 0 some error:
 * OTN_INV_POOL      (pool number is invalid/corrupted)
 * OTN_ID_OVERFLOW   (crt_id has overflowed with partition size since the
 *                    id was generated)
 * OTN_REPLAY        (nonce id seen before => replay )
 */
enum otn_check_ret otn_check_id(nid_t id, unsigned pool)
{
	unsigned int i;
	unsigned n, b;
	otn_cell_t v, b_mask;
	
	if (unlikely(pool>=nid_pool_no))
		return OTN_INV_POOL;
	if (unlikely(otn_id_check_overflow(id, pool)))
		return OTN_ID_OVERFLOW;
	n=get_otn_array_bit_idx(id, pool); /* n-th bit */
	i=get_otn_array_cell_idx(n);    /* aray index i, corresponding to n */
	b=get_otn_cell_bit(n);          /* bit pos corresponding to n */
	b_mask= (otn_cell_t)1<<b;
	
#ifdef OTN_CELL_T_LONG
	v=atomic_get_long(&oth_array[i]);
	if (unlikely(v & b_mask))
		return OTN_REPLAY;
	atomic_or_long((long*)&otn_array[i],  b_mask);
#else
	v=atomic_get_int(&otn_array[i]);
	if (unlikely(v & b_mask))
		return OTN_REPLAY;
	atomic_or_int((int*)&otn_array[i],  b_mask);
#endif /* OTN_CELL_T_LONG */
	return 0;
}

#endif /* USE_OT_NONCE */

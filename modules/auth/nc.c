/*
 * Digest Authentication Module
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

int nc_enabled=0;
unsigned nc_array_k;    /* array size bits (k in 2^k) */
unsigned nc_array_size; /* 2^k == 1<<nc_array_bits (in nc_t and not 
					  unsigned ints)*/

#ifdef USE_NC

#include "nc.h"
#include "nid.h"
#include "../../dprint.h"
#include "../../bit_scan.h"
#include "../../atomic_ops.h"
#include "../../ut.h" /* ROUNDUP...*/
#include "../../mem/shm_mem.h" /* shm_available() */
#include <stdlib.h> /* random() */
#include <string.h> /* memset() */
#include <assert.h>

static unsigned int* nc_array=0;


unsigned nc_partition_size; /* array partition == nc_array_size/nc_pool_no*/
unsigned nc_partition_k;    /* k such that 2^k==nc_partition_size */
unsigned nc_partition_mask; /* mask for computing the real idx. inside
							   one partition */


/* returns -1 on error, 0 on success */
int init_nonce_count()
{
	unsigned long size;
	unsigned long max_mem;
	unsigned orig_array_size;


	if (nid_crt==0){
		BUG("auth: init_nonce_count: nonce index must be "
				"initialized first (see init_nonce_id())\n");
		return -1;
	}
	orig_array_size=nc_array_size;
	if (nc_array_k==0){
		if (nc_array_size==0){
			nc_array_size=DEFAULT_NC_ARRAY_SIZE;
		}
		nc_array_k=bit_scan_reverse32(nc_array_size);
	}
	size=1UL<<nc_array_k; /* ROUNDDOWN to 2^nc_array_k */
	if (size < MIN_NC_ARRAY_SIZE){
		WARN("auth: nonce-count in.flight nonces is very low (%d),"
				" consider increasing nc_array_size to at least %d\n",
				orig_array_size, MIN_NC_ARRAY_SIZE);
	}
	if (size > MAX_NC_ARRAY_SIZE){
		WARN("auth: nonce-count in flight nonces is too high (%d),"
				" consider decreasing nc_array_size to at least %d\n",
				orig_array_size, MAX_NC_ARRAY_SIZE);
	}
	if (size!=nc_array_size){
		if (orig_array_size!=0)
			INFO("auth: nc_array_size rounded down to %ld\n", size);
		else
			INFO("auth: nc_array_size set to %ld\n", size);
	}
	max_mem=shm_available();
	if (size*sizeof(nc_t) >= max_mem){
		ERR("auth: nc_array_size (%ld) is too big for the configured "
				"amount of shared memory (%ld bytes <= %ld bytes)\n",
				size, max_mem, size*sizeof(nc_t));
		return -1;
	}else if (size*sizeof(nc_t) >= max_mem/2){
		WARN("auth: the currently configured nc_array_size (%ld)  "
				"would use more then 50%% of the available shared"
				" memory(%ld bytes)\n", size, max_mem);
	}
	nc_array_size=size;
	
	if (nid_pool_no>=nc_array_size){
		ERR("auth: nid_pool_no (%d) too high for the configured "
				"nc_array_size (%d)\n", nid_pool_no, nc_array_size);
		return -1;
	}
	nc_partition_size=nc_array_size >> nid_pool_k;
	nc_partition_k=nc_array_k-nid_pool_k;
	nc_partition_mask=(1<<nc_partition_k)-1;
	assert(nc_partition_size == nc_array_size/nid_pool_no);
	assert(1<<(nc_partition_k+nid_pool_k) == nc_array_size);
	
	if ((nid_t)nc_partition_size >= ((nid_t)(-1)/NID_INC)){
		ERR("auth: nc_array_size too big, try decreasing it or increasing"
				"the number of pools/partitions\n");
		return -1;
	}
	if (nc_partition_size  < MIN_NC_ARRAY_PARTITION){
		WARN("auth: nonce-count in-flight nonces very low,"
				" consider either decreasing nc_pool_no (%d) or "
				" increasing nc_array_size (%d) such that "
				"nc_array_size/nid_pool_no >= %d\n",
				nid_pool_no, orig_array_size, MIN_NC_ARRAY_PARTITION);
	}
	
	
	/*  array size should be multiple of sizeof(unsigned int) since we
	 *  access it as an uint array */
	nc_array=shm_malloc(sizeof(nc_t)*ROUND_INT(nc_array_size));
	if (nc_array==0){
		ERR("auth: init_nonce_count: memory allocation failure, consider"
				" either decreasing nc_array_size of increasing the"
				" the shared memory ammount\n");
		goto error;
	}
	/* int the nc_array with the max nc value to avoid replay attacks after
	 * ser restarts (because the nc is already maxed out => no received
	 * nc will be accepted, until the corresponding cell is reset) */
	memset(nc_array, 0xff, sizeof(nc_t)*ROUND_INT(nc_array_size));
	return 0;
error:
	destroy_nonce_count();
	return -1;
}



void destroy_nonce_count()
{
	if (nc_array){
		shm_free(nc_array);
		nc_array=0;
	}
}

/* given the nonce id i and pool/partition p, produces an index in the
 * nc array corresponding to p.
 * WARNING: the result is  an index in the nc_array converted to nc_t
 * (unsigned char by default), to get the index of the unsigned int in which
 * nc is packed, call get_nc_array_uint_idx(get_nc_array_raw_idx(i,p))).
 */
#define get_nc_array_raw_idx(i,p) \
	(((i)&nc_partition_mask)+((p)<<nc_partition_k))

/* get the real array cell corresponding to a certain index
 * (the index refers to stored nc, but several ncs are stored
 * inside an int => several nc_t values inside and array cell;
 * for example if nc_t is uchar => each real array cell holds 4 nc_t).
 * pos is the "raw" index (e.g. obtained by get_nc_array_raw_idx(i,p)))
 * and the result is the index of the unsigned int cell in which the pos nc
 * is packed.
 */
#define get_nc_array_uint_idx(pos) \
	((pos)/(sizeof(unsigned int)/sizeof(nc_t)))

/* get position inside an int nc_array cell for the raw index pos
 * (pos can be obtained from a nonce id with get_nc_array_raw_idx(i, p),
 *  see above) */
#define get_nc_int_pos(pos) \
	((pos)%(sizeof(unsigned int)/sizeof(nc_t)))

/* returns true if the crt_idx > idx with at least  nc_partition_size
 * WARNING: NID_INC * nc_partition_size must fit inside an nidx_t*/
#define  nc_id_check_overflow(id,  pool) \
	((nid_t)(nid_get((pool))-(id)) >= \
	 	((nid_t)NID_INC*nc_partition_size))

/* re-init the stored nc for nonce id in pool p */
nid_t nc_new(nid_t id, unsigned char p)
{
	unsigned int i;
	unsigned  n, r;
	unsigned int v, new_v;
	
	n=get_nc_array_raw_idx(id, p); /* n-th nc_t */
	i=get_nc_array_uint_idx(n);  /* aray index i, corresponding to n */
	r=get_nc_int_pos(n);  /* byte/short inside the uint corresponding to n */
	/* reset corresponding value to 0 */
	do{
		v=atomic_get_int(&nc_array[i]);
		/* new_value = old_int with the corresponding byte or short zeroed*/
		new_v=v & ~(((1<<(sizeof(nc_t)*8))-1)<< (r*sizeof(nc_t)*8));
	}while(atomic_cmpxchg_int((int*)&nc_array[i], v, new_v)!=v);
	return id;
}



/* check if nonce-count nc w/ index i is expected/valid and if so it 
 * updated the stored nonce-count
 * returns: 0 - ok, < 0 some error:
 * NC_INV_POOL      (pool number is invalid/corrupted)
 * NC_ID_OVERFLOW (crt_id has overflowed with partition size since the
 *                   id was generated)
 * NC_TOO_BIG       (nc value got too big and cannot be held anymore)
 * NC_REPLAY        (nc value is <= the current stored one)
 */
enum nc_check_ret nc_check_val(nid_t id, unsigned pool, unsigned int nc)
{
	unsigned int i;
	unsigned n, r;
	unsigned int v, crt_nc, new_v;
	
	if (unlikely(pool>=nid_pool_no))
		return NC_INV_POOL;
	if (unlikely(nc_id_check_overflow(id, pool)))
		return NC_ID_OVERFLOW;
	if (unlikely(nc>=(1U<<(sizeof(nc_t)*8))))
		return NC_TOO_BIG;
	n=get_nc_array_raw_idx(id, pool); /* n-th nc_t */
	i=get_nc_array_uint_idx(n);  /* aray index i, corresponding to n */
	r=get_nc_int_pos(n); /* byte/short inside the uint corresponding to n */
	do{
		v=atomic_get_int(&nc_array[i]);
		/* get current (stored) nc value */
		crt_nc=(v>>(r*8)) & ((1U<<(sizeof(nc_t)*8))-1);
		if (crt_nc>=nc)
			return NC_REPLAY;
		/* set corresponding array cell byte/short to new nc */
		new_v=(v & ~(((1U<<(sizeof(nc_t)*8))-1)<< (r*8)) )|
				(nc << (r*8));
	}while(atomic_cmpxchg_int((int*)&nc_array[i], v, new_v)!=v);
	return 0;
}

#endif /* USE_NC */

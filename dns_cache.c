/*
 * $Id$
 *
 * resolver related functions
 *
 * Copyright (C) 2006 iptelorg GmbH
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/* History:
 * --------
 *  2006-07-13  created by andrei
 *  2006-10-06  port fix (andrei)
 */

#ifdef USE_DNS_CACHE

#include "globals.h"
#include "dns_cache.h"
#include "dns_wrappers.h"
#include "mem/shm_mem.h"
#include "hashes.h"
#include "clist.h"
#include "locking.h"
#include "atomic_ops.h"
#include "ut.h"
#include "timer.h"
#include "timer_ticks.h"
#include "error.h"
#include "rpc.h"



#define DNS_CACHE_DEBUG /* extra sanity checks and debugging */


#ifndef MAX
	#define MAX(a,b) ( ((a)>(b))?(a):(b))
#endif

#define MAX_DNS_RECORDS 255  /* maximum dns records number  received in a 
							   dns answer*/

#define DNS_HASH_SIZE	1024 /* must be <= 65535 */
#define DEFAULT_DNS_NEG_CACHE_TTL 60 /* 1 min. */
#define DEFAULT_DNS_CACHE_MIN_TTL 0 /* (disabled) */
#define DEFAULT_DNS_CACHE_MAX_TTL ((unsigned int)(-1)) /* (maxint) */
#define DEFAULT_DNS_MAX_MEM 500 /* 500 Kb */
#define DEFAULT_DNS_TIMER_INTERVAL 120  /* 2 min. */
#define DNS_HE_MAX_ADDR 10  /* maxium addresses returne in a hostent struct */
#define MAX_CNAME_CHAIN  10 


static gen_lock_t* dns_hash_lock=0;
static volatile unsigned int *dns_cache_mem_used=0; /* current mem. use */
unsigned int dns_cache_max_mem=DEFAULT_DNS_MAX_MEM; /* maximum memory used for
													 the cached entries */
unsigned int dns_neg_cache_ttl=DEFAULT_DNS_NEG_CACHE_TTL; /* neg. cache ttl */
unsigned int dns_cache_max_ttl=DEFAULT_DNS_CACHE_MAX_TTL; /* maximum ttl */
unsigned int dns_cache_min_ttl=DEFAULT_DNS_CACHE_MIN_TTL; /* minimum ttl */
unsigned int dns_timer_interval=DEFAULT_DNS_TIMER_INTERVAL; /* in s */
int dns_flags=0; /* default flags used for the  dns_*resolvehost 
                    (compatibility wrappers) */

#define LOCK_DNS_HASH()		lock_get(dns_hash_lock)
#define UNLOCK_DNS_HASH()	lock_release(dns_hash_lock)

#define FIX_TTL(t)  (((t)<dns_cache_min_ttl)?dns_cache_min_ttl: \
						(((t)>dns_cache_max_ttl)?dns_cache_max_ttl:(t)))


struct dns_hash_head{
	struct dns_hash_entry* next;
	struct dns_hash_entry* prev;
};

#ifdef DNS_LU_LST
struct dns_lu_lst* dns_last_used_lst=0;
#endif

static struct dns_hash_head* dns_hash=0;


static struct timer_ln* dns_timer_h=0;



static const char* dns_str_errors[]={
	"no error",
	"no more records", /* not an error, but and end condition */
	"unknown error",
	"internal error",
	"bad SRV entry",
	"unresolvable SRV request",
	"bad A or AAAA entry",
	"unresovlable A or AAAA request",
	"invalid ip in A or AAAA record",
	"blacklisted ip",
	"name too long ", /* try again with a shorter name */
	"ip AF mismatch", /* address family mismatch */
	"bug - critical error"
};



/* param: err (negative error number) */
const char* dns_strerror(int err)
{
	err=-err;
	if ((err>=0) && (err<sizeof(dns_str_errors)/sizeof(char*)))
		return dns_str_errors[err];
	return "bug -- bad error number";
}



/* "internal" only, don't use unless you really know waht you're doing */
inline static void dns_destroy_entry(struct dns_hash_entry* e)
{
#ifdef DNS_CACHE_DEBUG
	memset(e, 0, e->total_size);
#endif
	shm_free(e); /* nice having it in one block isn't it? :-) */
}


/* "internal" only, same as above, asumes shm_lock() held (tm optimization) */
inline static void dns_destroy_entry_shm_unsafe(struct dns_hash_entry* e)
{
#ifdef DNS_CACHE_DEBUG
	memset(e, 0, e->total_size);
#endif
	shm_free_unsafe(e); /* nice having it in one block isn't it? :-) */
}



/* dec. the internal refcnt and if 0 deletes the entry */
void dns_hash_put(struct dns_hash_entry* e)
{
	if(e && atomic_dec_and_test(&e->refcnt)){
		/* atomic_sub_long(dns_cache_total_used, e->total_size); */
		dns_destroy_entry(e);
	}
}



/* same as above but uses dns_destroy_unsafe (assumes shm_lock held -- tm
 *  optimization) */
void dns_hash_put_shm_unsafe(struct dns_hash_entry* e)
{
	if(e && atomic_dec_and_test(&e->refcnt)){
		/* atomic_sub_long(dns_cache_total_used, e->total_size); */
		dns_destroy_entry_shm_unsafe(e);
	}
}


inline static int dns_cache_clean(unsigned int no, int expired_only);
inline static int dns_cache_free_mem(unsigned int target, int expired_only);

static ticks_t dns_timer(ticks_t ticks, struct timer_ln* tl, void* data)
{
	if (*dns_cache_mem_used>12*(dns_cache_max_mem/16)){ /* ~ 75% used */
		dns_cache_free_mem(dns_cache_max_mem/2, 1); 
	}else{
		dns_cache_clean(-1, 1); /* all the table, only expired entries */
		/* TODO: better strategy? */
	}
	return (ticks_t)(-1);
}



void destroy_dns_cache()
{
	if (dns_timer_h){
		timer_del(dns_timer_h);
		timer_free(dns_timer_h);
		dns_timer_h=0;
	}
	if (dns_hash_lock){
		lock_destroy(dns_hash_lock);
		lock_dealloc(dns_hash_lock);
		dns_hash_lock=0;
	}
	if (dns_hash){
		shm_free(dns_hash);
		dns_hash=0;
	}
#ifdef DNS_LU_LST
	if (dns_last_used_lst){
		shm_free(dns_last_used_lst);
		dns_last_used_lst=0;
	}
#endif
	if (dns_cache_mem_used){
		shm_free((void*)dns_cache_mem_used);
		dns_cache_mem_used=0;
	}
}



int init_dns_cache()
{
	int r;
	int ret;
	
	ret=0;
	/* sanity check */
	if (E_DNS_CRITICAL>=sizeof(dns_str_errors)/sizeof(char*)){
		LOG(L_CRIT, "BUG: dns_cache_init: bad dns error table\n");
		ret=E_BUG;
		goto error;
	}
	dns_cache_mem_used=shm_malloc(sizeof(*dns_cache_mem_used));
	if (dns_cache_mem_used==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
#ifdef DNS_LU_LST
	dns_last_used_lst=shm_malloc(sizeof(*dns_last_used_lst));
	if (dns_last_used_lst==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
	clist_init(dns_last_used_lst, next, prev);
#endif
	dns_hash=shm_malloc(sizeof(struct dns_hash_head)*DNS_HASH_SIZE);
	if (dns_hash==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
	for (r=0; r<DNS_HASH_SIZE; r++)
		clist_init(&dns_hash[r], next, prev);
	
	dns_hash_lock=lock_alloc();
	if (dns_hash_lock==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
	if (lock_init(dns_hash_lock)==0){
		lock_dealloc(dns_hash_lock);
		dns_hash_lock=0;
		ret=-1;
		goto error;
	}
	
	/* fix options */
	dns_cache_max_mem<<=10; /* Kb */ /* TODO: test with 0 */
	/* fix flags */
	if (dns_try_ipv6==0){
		dns_flags|=DNS_IPV4_ONLY;
	}
	if (dns_flags & DNS_IPV4_ONLY){
		dns_flags&=~(DNS_IPV6_ONLY|DNS_IPV6_FIRST);
	}
			;
	dns_timer_h=timer_alloc();
	if (dns_timer_h==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
	if (dns_timer_interval){
		timer_init(dns_timer_h, dns_timer, 0, 0); /* "slow" timer */
		if (timer_add(dns_timer_h, S_TO_TICKS(dns_timer_interval))<0){
			LOG(L_CRIT, "BUG: dns_cache_init: failed to add the timer\n");
			timer_free(dns_timer_h);
			dns_timer_h=0;
			goto error;
		}
	}
	
	return 0;
error:
	destroy_dns_cache();
	return ret;
}


/* hash function, type is not used (obsolete)
 * params: char* s, int len, int type
 * returns the hash value
 */
#define dns_hash_no(s, len, type) \
	(get_hash1_case_raw((s),(len)) % DNS_HASH_SIZE)



#ifdef DNS_CACHE_DEBUG
#define DEBUG_LU_LST
#ifdef DEBUG_LU_LST

#include <stdlib.h> /* abort() */
#define check_lu_lst(l) ((((l)->next==(l)) || ((l)->prev==(l))) && \
							((l)!=dns_last_used_lst))

#define dbg_lu_lst(txt, l) \
		LOG(L_CRIT, "BUG: %s: crt(%p, %p, %p)," \
					" prev(%p, %p, %p), next(%p, %p, %p)\n", txt, \
					(l), (l)->next, (l)->prev, \
					(l)->prev, (l)->prev->next, (l)->prev->prev, \
					(l)->next, (l)->next->next, (l)->next->prev \
				) 

#define debug_lu_lst( txt, l) \
	do{ \
		if (check_lu_lst((l))){  \
			dbg_lu_lst(txt  " crt:", (l)); \
			abort(); \
		} \
		if (check_lu_lst((l)->next)){ \
			dbg_lu_lst(txt  " next:",  (l)); \
			abort(); \
		} \
		if (check_lu_lst((l)->prev)){ \
			dbg_lu_lst(txt  " prev:", (l)); \
			abort(); \
		} \
	}while(0)

#endif
#endif /* DNS_CACHE_DEBUG */


/* must be called with the DNS_LOCK hold
 * remove and entry from the hash, dec. its refcnt and if not referenced
 * anymore deletes it */
void _dns_hash_remove(struct dns_hash_entry* e)
{
	clist_rm(e, next, prev);
#ifdef DNS_CACHE_DEBUG
	e->next=e->prev=0;
#endif
#ifdef DNS_LU_LST
#ifdef DEBUG_LU_LST
	debug_lu_lst("_dns_hash_remove: pre rm:", &e->last_used_lst);
#endif
	clist_rm(&e->last_used_lst, next, prev);
#ifdef DEBUG_LU_LST
	debug_lu_lst("_dns_hash_remove: post rm:", &e->last_used_lst);
#endif
#ifdef DNS_CACHE_DEBUG
	e->last_used_lst.next=e->last_used_lst.prev=0;
#endif
#endif
	*dns_cache_mem_used-=e->total_size;
	dns_hash_put(e);
}



/* non locking  version (the dns hash must _be_ locked externally)
 * returns 0 when not found, or the entry on success (an entry with a
 * similar name but with a CNAME type will always match).
 * it doesn't increase the internal refcnt
 * returns the entry when found, 0 when not found and sets *err to !=0
 *  on error (e.g. recursive cnames)
 * WARNING: - internal use only
 *          - always check if the returned entry type is CNAME */
inline static struct dns_hash_entry* _dns_hash_find(str* name, int type,
														int* h, int* err)
{
	struct dns_hash_entry* e;
	struct dns_hash_entry* tmp;
	struct dns_hash_entry* ret;
	ticks_t now;
	int cname_chain;
	str cname;
	
	cname_chain=0;
	ret=0;
	now=get_ticks_raw();
	*err=0;
again:
	*h=dns_hash_no(name->s, name->len, type);
	DBG("dns_hash_find(%.*s(%d), %d), h=%d\n", name->len, name->s,
												name->len, type, *h);
	clist_foreach_safe(&dns_hash[*h], e, tmp, next){
		/* automatically remove expired elements */
		if ((s_ticks_t)(now-e->expire)>=0){
				_dns_hash_remove(e);
		}else if ((e->type==type) && (e->name_len==name->len) &&
			(strncasecmp(e->name, name->s, e->name_len)==0)){
			e->last_used=now;
#ifdef DNS_LU_LST
			/* add it at the end */
#ifdef DEBUG_LU_LST
			debug_lu_lst("_dns_hash_find: pre rm:", &e->last_used_lst);
#endif
			clist_rm(&e->last_used_lst, next, prev);
			clist_append(dns_last_used_lst, &e->last_used_lst, next, prev);
#ifdef DEBUG_LU_LST
			debug_lu_lst("_dns_hash_find: post append:", &e->last_used_lst);
#endif
#endif
			return e;
		}else if ((e->type==T_CNAME) && (e->name_len==name->len) &&
			(strncasecmp(e->name, name->s, e->name_len)==0)){
			e->last_used=now;
#ifdef DNS_LU_LST
			/* add it at the end */
#ifdef DEBUG_LU_LST
			debug_lu_lst("_dns_hash_find: cname: pre rm:", &e->last_used_lst);
#endif
			clist_rm(&e->last_used_lst, next, prev);
			clist_append(dns_last_used_lst, &e->last_used_lst, next, prev);
#ifdef DEBUG_LU_LST
			debug_lu_lst("_dns_hash_find: cname: post append:",
							&e->last_used_lst);
#endif
#endif		
			ret=e; /* if this is an unfinished cname chain, we try to
					  return the last cname */
			/* this is a cname => retry using its value */
			if (cname_chain> MAX_CNAME_CHAIN){
				LOG(L_ERR, "ERROR: _dns_hash_find: cname chain too long "
						"or recursive (\"%.*s\")\n", name->len, name->s);
				ret=0; /* error*/
				*err=-1;
				break;
			}
			cname_chain++;
			cname.s=((struct cname_rdata*)e->rr_lst->rdata)->name;
			cname.len= ((struct cname_rdata*)e->rr_lst->rdata)->name_len;
			name=&cname;
			goto again;
		}
	}
	return ret;
}



/* frees cache entries, if expired_only=0 only expired entries will be 
 * removed, else all of them
 * it will process maximum no entries (to process all of them use -1)
 * returns the number of deleted entries
 * This should be called from a timer process*/
inline static int dns_cache_clean(unsigned int no, int expired_only)
{
	struct dns_hash_entry* e;
	ticks_t now;
	unsigned int n;
	unsigned int deleted;
#ifdef DNS_LU_LST
	struct dns_lu_lst* l;
	struct dns_lu_lst* tmp;
#else
	struct dns_hash_entry* t;
	unsigned int h;
	static unsigned int start=0;
#endif
	
	n=0;
	deleted=0;
	now=get_ticks_raw();
	LOCK_DNS_HASH();
#ifdef DNS_LU_LST
	clist_foreach_safe(dns_last_used_lst, l, tmp, next){
		e=(struct dns_hash_entry*)(((char*)l)-
				(char*)&((struct dns_hash_entry*)(0))->last_used_lst);
		if (!expired_only || ((s_ticks_t)(now-e->expire)>=0)){
				_dns_hash_remove(e);
				deleted++;
		}
		n++;
		if (n>=no) break;
	}
#else
	for(h=start; h!=(start+DNS_HASH_SIZE); h++){
		clist_foreach_safe(&dns_hash[h%DNS_HASH_SIZE], e, t, next){
			if  ((s_ticks_t)(now-e->expire)>=0){
				_dns_hash_remove(e);
				deleted++;
			}
			n++;
			if (n>=no) break;
		}
	}
	/* not fair, but faster then random() */
	if (!expired_only){
		for(h=start; h!=(start+DNS_HASH_SIZE); h++){
			clist_foreach_safe(&dns_hash[h%DNS_HASH_SIZE], e, t, next){
				if  ((s_ticks_t)(now-e->expire)>=0){
					_dns_hash_remove(e);
					deleted++;
				}
				n++;
				if (n>=no) goto skip;
			}
		}
	}
skip:
	start=h;
#endif
	UNLOCK_DNS_HASH();
	return deleted;
}



/* frees cache entries, if expired_only=0 only expired entries will be 
 * removed, else all of them
 * it will stop when the dns cache used memory reaches target (to process all 
 * of them use 0)
 * returns the number of deleted entries */
inline static int dns_cache_free_mem(unsigned int target, int expired_only)
{
	struct dns_hash_entry* e;
	ticks_t now;
	unsigned int deleted;
#ifdef DNS_LU_LST
	struct dns_lu_lst* l;
	struct dns_lu_lst* tmp;
#else
	struct dns_hash_entry* t;
	unsigned int h;
	static unsigned int start=0;
#endif
	
	deleted=0;
	now=get_ticks_raw();
	LOCK_DNS_HASH();
#ifdef DNS_LU_LST
	clist_foreach_safe(dns_last_used_lst, l, tmp, next){
		if (*dns_cache_mem_used<=target) break;
		e=(struct dns_hash_entry*)(((char*)l)-
				(char*)&((struct dns_hash_entry*)(0))->last_used_lst);
		if (!expired_only || ((s_ticks_t)(now-e->expire)>=0)){
				_dns_hash_remove(e);
				deleted++;
		}
	}
#else
	for(h=start; h!=(start+DNS_HASH_SIZE); h++){
		clist_foreach_safe(&dns_hash[h%DNS_HASH_SIZE], e, t, next){
			if (*dns_cache_mem_used<=target) 
				goto skip;
			if  ((s_ticks_t)(now-e->expire)>=0){
				_dns_hash_remove(e);
				deleted++;
			}
		}
	}
	/* not fair, but faster then random() */
	if (!expired_only){
		for(h=start; h!=(start+DNS_HASH_SIZE); h++){
			clist_foreach_safe(&dns_hash[h%DNS_HASH_SIZE], e, t, next){
				if (*dns_cache_mem_used<=target) 
					goto skip;
				if  ((s_ticks_t)(now-e->expire)>=0){
					_dns_hash_remove(e);
					deleted++;
				}
			}
		}
	}
skip:
	start=h;
#endif
	UNLOCK_DNS_HASH();
	return deleted;
}



/* locking  version (the dns hash must _not_be locked externally)
 * returns 0 when not found, the searched entry on success (with CNAMEs
 *  followed) or the last CNAME entry from an unfinished CNAME chain, 
 *  if the search matches a CNAME. On error sets *err (e.g. recursive CNAMEs).
 * it increases the internal refcnt => when finished dns_hash_put() must
 *  be called on the returned entry
 *  WARNING: - the return might be a CNAME even if type!=CNAME, see above */
inline static struct dns_hash_entry* dns_hash_get(str* name, int type, int* h,
													int* err)
{
	struct dns_hash_entry* e;
	
	LOCK_DNS_HASH();
	e=_dns_hash_find(name, type, h, err);
	if (e){
		atomic_inc(&e->refcnt);
	}
	UNLOCK_DNS_HASH();
	return e;
}



/* adds a fully created and init. entry (see dns_cache_mk_entry()) to the hash
 * table
 * returns 0 on success, -1 on error */
inline static int dns_cache_add(struct dns_hash_entry* e)
{
	int h;
	
	/* check space */
	/* atomic_add_long(dns_cache_total_used, e->size); */
	if ((*dns_cache_mem_used+e->total_size)>=dns_cache_max_mem){
		LOG(L_WARN, "WARNING: dns_cache_add: cache full, trying to free...\n");
		/* free ~ 12% of the cache */
		dns_cache_free_mem(*dns_cache_mem_used/16*14, 1);
		if ((*dns_cache_mem_used+e->total_size)>=dns_cache_max_mem){
			LOG(L_ERR, "ERROR: dns_cache_add: max. cache mem size exceeded\n");
			return -1;
		}
	}
	atomic_inc(&e->refcnt);
	h=dns_hash_no(e->name, e->name_len, e->type);
	DBG("dns_cache_add: adding %.*s(%d) %d (flags=%0x) at %d\n",
			e->name_len, e->name, e->name_len, e->type, e->err_flags, h);
	LOCK_DNS_HASH();
		*dns_cache_mem_used+=e->total_size; /* no need for atomic ops, written
										 only from within a lock */
		clist_append(&dns_hash[h], e, next, prev);
#ifdef DNS_LU_LST
		clist_append(dns_last_used_lst, &e->last_used_lst, next, prev);
#endif
	UNLOCK_DNS_HASH();
	return 0;
}



/* same as above, but it must be called with the dns hash lock held
 * returns 0 on success, -1 on error */
inline static int dns_cache_add_unsafe(struct dns_hash_entry* e)
{
	int h;
	
	/* check space */
	/* atomic_add_long(dns_cache_total_used, e->size); */
	if ((*dns_cache_mem_used+e->total_size)>=dns_cache_max_mem){
		LOG(L_WARN, "WARNING: dns_cache_add: cache full, trying to free...\n");
		/* free ~ 12% of the cache */
		UNLOCK_DNS_HASH();
		dns_cache_free_mem(*dns_cache_mem_used/16*14, 1);
		LOCK_DNS_HASH();
		if ((*dns_cache_mem_used+e->total_size)>=dns_cache_max_mem){
			LOG(L_ERR, "ERROR: dns_cache_add: max. cache mem size exceeded\n");
			return -1;
		}
	}
	atomic_inc(&e->refcnt);
	h=dns_hash_no(e->name, e->name_len, e->type);
	DBG("dns_cache_add: adding %.*s(%d) %d (flags=%0x) at %d\n",
			e->name_len, e->name, e->name_len, e->type, e->err_flags, h);
	*dns_cache_mem_used+=e->total_size; /* no need for atomic ops, written
										 only from within a lock */
	clist_append(&dns_hash[h], e, next, prev);
#ifdef DNS_LU_LST
	clist_append(dns_last_used_lst, &e->last_used_lst, next, prev);
#endif
	return 0;
}



/* creates a "negative" entry which will be valid for ttl seconds */
inline static struct dns_hash_entry* dns_cache_mk_bad_entry(str* name,
															int type,
															int ttl,
															int flags)
{
	struct dns_hash_entry* e;
	int size;
	ticks_t now;
	
	DBG("dns_cache_mk_bad_entry(%.*s, %d, %d, %d)\n", name->len, name->s,
									type, ttl, flags);
	size=sizeof(struct dns_hash_entry)+name->len-1+1;
	e=shm_malloc(size);
	if (e==0){
		LOG(L_ERR, "ERROR: dns_cache_mk_ip_entry: out of memory\n");
		return 0;
	}
	memset(e, 0, size); /* init with 0*/
	e->total_size=size;
	e->name_len=name->len;
	e->type=type;
	now=get_ticks_raw();
	e->last_used=now;
	e->expire=now+S_TO_TICKS(ttl);
	memcpy(e->name, name->s, name->len);
	e->err_flags=flags;
	return e;
}



/* create a a/aaaa hash entry from a name and ip address
 * returns 0 on error */
inline static struct dns_hash_entry* dns_cache_mk_ip_entry(str* name,
															struct ip_addr* ip)
{
	struct dns_hash_entry* e;
	int size;
	ticks_t now;
	
	/* everything is allocated in one block: dns_hash_entry + name +
	 * + dns_rr + rdata;  dns_rr must start at an aligned adress,
	 * hence we need to round dns_hash_entry+name size to a sizeof(long)
	 * multiple.
	 * Memory image:
	 * struct dns_hash_entry
	 * name (name_len+1 bytes)
	 * padding to multiple of sizeof(long)
	 * dns_rr
	 * rdata  (no padding needed, since for ip is just an array of chars)
	  */
	size=ROUND_POINTER(sizeof(struct dns_hash_entry)+name->len-1+1)+ 
			sizeof(struct dns_rr)+ ip->len;
	e=shm_malloc(size);
	if (e==0){
		LOG(L_ERR, "ERROR: dns_cache_mk_ip_entry: out of memory\n");
		return 0;
	}
	memset(e, 0, size); /* init with 0*/
	e->total_size=size;
	e->name_len=name->len;
	e->type=(ip->af==AF_INET)?T_A:T_AAAA;
	now=get_ticks_raw();
	e->last_used=now;
	e->expire=now-1; /* maximum expire */
	memcpy(e->name, name->s, name->len); /* memset makes sure is 0-term. */
	e->rr_lst=(void*)((char*)e+
				ROUND_POINTER(sizeof(struct dns_hash_entry)+name->len-1+1));
	e->rr_lst->rdata=(void*)((char*)e->rr_lst+sizeof(struct dns_rr));
	e->rr_lst->expire=now-1; /* maximum expire */
	/* no need to align rr_lst->rdata for a or aaaa records */
	memcpy(e->rr_lst->rdata, ip->u.addr, ip->len);
	return e;
}



/* create a dns hash entry from a name and a rdata list (pkg_malloc'ed)
 * (it will use only the type records with the name "name" from the
 *  rdata list with one exception: if a matching CNAME with the same
 *  name is found, the search will stop and this will be the record used)
 * returns 0 on error and removes the used elements from the rdata list*/
inline static struct dns_hash_entry* dns_cache_mk_rd_entry(str* name, int type,
														struct rdata** rd_lst)
{
	struct dns_hash_entry* e;
	struct dns_rr* rr;
	struct dns_rr** tail_rr;
	struct rdata** p;
	struct rdata* tmp_lst;
	struct rdata** tail;
	struct rdata* l;
	int size;
	ticks_t now;
	unsigned int max_ttl;
	unsigned int ttl;
	
#define rec_matches(rec, t, n) /*(struct rdata* record, int type, str* name)*/\
	(	((rec)->name_len==(n)->len) && ((rec)->type==(t)) && \
		(strncasecmp((rec)->name, (n)->s, (n)->len)==0))
	/* init */
	tmp_lst=0;
	tail=&tmp_lst;
	
	
	/* everything is allocated in one block: dns_hash_entry + name +
	 * + dns_rr + rdata_raw+ ....;  dns_rr must start at an aligned adress,
	 * hence we need to round dns_hash_entry+name size to a sizeof(long)
	 * multiple. If rdata type requires it, rdata_raw might need to be also
	 * aligned.
	 * Memory image:
	 * struct dns_hash_entry  (e)
	 * name (name_len+1 bytes)  (&e->name[0])
	 * padding to multiple of sizeof(char*)
	 * dns_rr1 (e->rr_lst)
	 * possible padding: no padding for a_rdata or aaaa_rdata, 
	 *                   multipe of sizeof(short) for srv_rdata,
	 *                   multiple of sizeof(long) for naptr_rdata and others
	 * dns_rr1->rdata  (e->rr_lst->rdata)
	 * padding to multipe of sizeof long
	 * dns_rr2 (e->rr_lst->next)
	 * ....
	 *
	 */
	size=0;
	if (*rd_lst==0)
		return 0;
	/* find the first matching rr, if it's a CNAME use CNAME as type,
	 * if not continue with the original type */
	for(p=rd_lst; *p; p=&(*p)->next){
		if (((*p)->name_len==name->len) &&
				(((*p)->type==type) || ((*p)->type==T_CNAME)) &&
				(strncasecmp((*p)->name, name->s, name->len)==0)){
			type=(*p)->type;
			break;
		}
	}
	/* continue, we found the type we are looking for */
	switch(type){
		case T_A:
			for(; *p;){
				if (!rec_matches((*p), type, name)){ 
					/* skip this record */
					p=&(*p)->next; /* advance */
					continue;
				}
				size+=ROUND_POINTER(sizeof(struct dns_rr)+
										sizeof(struct a_rdata));
				/* add it to our tmp. lst */
				*tail=*p;
				tail=&(*p)->next;
				/* detach it from the rd list */
				*p=(*p)->next;
				/* don't advance p, because the crt. elem. has
				 * just been elimintated */
			}
			break;
		case T_AAAA:
			for(; *p;){
				if (!rec_matches((*p), type, name)){ 
					/* skip this record */
					p=&(*p)->next; /* advance */
					continue;
				}
				/* no padding */
				size+=ROUND_POINTER(sizeof(struct dns_rr)+
											sizeof(struct aaaa_rdata));
				/* add it to our tmp. lst */
				*tail=*p;
				tail=&(*p)->next;
				/* detach it from the rd list */
				*p=(*p)->next;
				/* don't advance p, because the crt. elem. has
				 * just been elimintated */
			}
			break;
		case T_SRV:
			for(; *p;){
				if (!rec_matches((*p), type, name)){ 
					/* skip this record */
					p=&(*p)->next; /* advance */
					continue;
				}
				/* padding to short */
				size+=ROUND_POINTER(ROUND_SHORT(sizeof(struct dns_rr))+
						SRV_RDATA_SIZE(*(struct srv_rdata*)(*p)->rdata));
				/* add it to our tmp. lst */
				*tail=*p;
				tail=&(*p)->next;
				/* detach it from the rd list */
				*p=(*p)->next;
				/* don't advance p, because the crt. elem. has
				 * just been elimintated */
			}
			break;
		case T_NAPTR:
			for(; *p;){
				if (!rec_matches((*p), type, name)){ 
					/* skip this record */
					p=&(*p)->next; /* advance */
					continue;
				}
				/* padding to char* */
				size+=ROUND_POINTER(ROUND_POINTER(sizeof(struct dns_rr))+
						NAPTR_RDATA_SIZE(*(struct naptr_rdata*)(*p)->rdata));
				/* add it to our tmp. lst */
				*tail=*p;
				tail=&(*p)->next;
				/* detach it from the rd list */
				*p=(*p)->next;
				/* don't advance p, because the crt. elem. has
				 * just been elimintated */
			}
			break;
		case T_CNAME:
			for(; *p;){
				if (!rec_matches((*p), type, name)){ 
					/* skip this record */
					p=&(*p)->next; /* advance */
					continue;
				}
				/* no padding */
				size+=ROUND_POINTER(sizeof(struct dns_rr)+
						CNAME_RDATA_SIZE(*(struct cname_rdata*)(*p)->rdata));
				/* add it to our tmp. lst */
				*tail=*p;
				tail=&(*p)->next;
				/* detach it from the rd list */
				*p=(*p)->next;
				/* don't advance p, because the crt. elem. has
				 * just been elimintated */
			}
			break;
		default:
			LOG(L_CRIT, "BUG: dns_cache_mk_rd_entry: type %d not "
							"supported\n", type);
			/* we don't know what to do with it, so don't
			 * add it to the tmp_lst */
			return 0; /* error */
	}
	*tail=0; /* mark the end of our tmp_lst */
	if (size==0){
		DBG("dns_cache_mk_rd_entry: entry %.*s (%d) not found\n",
				name->len, name->s, type);
		return 0;
	}
	/* compute size */
	size+=ROUND_POINTER(sizeof(struct dns_hash_entry)+name->len-1+1);
	e=shm_malloc(size);
	if (e==0){
		LOG(L_ERR, "ERROR: dns_cache_mk_ip_entry: out of memory\n");
		return 0;
	}
	memset(e, 0, size); /* init with 0 */
	e->total_size=size;
	e->name_len=name->len;
	e->type=type;
	now=get_ticks_raw();
	e->last_used=now;
	memcpy(e->name, name->s, name->len); /* memset makes sure is 0-term. */
	e->rr_lst=(struct dns_rr*)((char*)e+
				ROUND_POINTER(sizeof(struct dns_hash_entry)+name->len-1+1));
	tail_rr=&(e->rr_lst);
	rr=e->rr_lst;
	max_ttl=0;
	/* copy the actual data */
	switch(type){
		case T_A:
			for(l=tmp_lst; l; l=l->next){
				ttl=FIX_TTL(l->ttl);
				rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				max_ttl=MAX(max_ttl, ttl);
				rr->rdata=(void*)((char*)rr+sizeof(struct dns_rr));
				memcpy(rr->rdata, l->rdata, sizeof(struct a_rdata));
				rr->next=(void*)((char*)rr+ROUND_POINTER(sizeof(struct dns_rr)+
							sizeof(struct a_rdata)));
				tail_rr=&(rr->next);
				rr=rr->next;
			}
			break;
		case T_AAAA:
			for(l=tmp_lst; l; l=l->next){
				ttl=FIX_TTL(l->ttl);
				rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				max_ttl=MAX(max_ttl, ttl);
				rr->rdata=(void*)((char*)rr+sizeof(struct dns_rr));
				memcpy(rr->rdata, l->rdata, sizeof(struct aaaa_rdata));
				rr->next=(void*)((char*)rr+ROUND_POINTER(sizeof(struct dns_rr)+
							sizeof(struct aaaa_rdata)));
				tail_rr=&(rr->next);
				rr=rr->next;
			}
			break;
		case T_SRV:
			for(l=tmp_lst; l; l=l->next){
				ttl=FIX_TTL(l->ttl);
				rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				max_ttl=MAX(max_ttl, ttl);
				rr->rdata=(void*)((char*)rr+
								ROUND_SHORT(sizeof(struct dns_rr)));
				/* copy the whole srv_rdata block*/
				memcpy(rr->rdata, l->rdata, 
						SRV_RDATA_SIZE(*(struct srv_rdata*)l->rdata) );
				rr->next=(void*)((char*)rr+
							ROUND_POINTER( ROUND_SHORT(sizeof(struct dns_rr))+
										SRV_RDATA_SIZE(
											*(struct srv_rdata*)l->rdata)));
				tail_rr=&(rr->next);
				rr=rr->next;
			}
			break;
		case T_NAPTR:
			for(l=tmp_lst; l; l=l->next){
				ttl=FIX_TTL(l->ttl);
				rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				max_ttl=MAX(max_ttl, ttl);
				rr->rdata=(void*)((char*)rr+
								ROUND_POINTER(sizeof(struct dns_rr)));
				/* copy the whole srv_rdata block*/
				memcpy(rr->rdata, l->rdata, 
						NAPTR_RDATA_SIZE(*(struct naptr_rdata*)l->rdata) );
				/* adjust the string pointer */
				((struct naptr_rdata*)rr->rdata)->flags=
					translate_pointer((char*)rr->rdata, (char*)l->rdata,
							(((struct naptr_rdata*)l->rdata)->flags));
				((struct naptr_rdata*)rr->rdata)->services=
					translate_pointer((char*)rr->rdata, (char*)l->rdata,
							(((struct naptr_rdata*)l->rdata)->services));
				((struct naptr_rdata*)rr->rdata)->regexp=
					translate_pointer((char*)rr->rdata, (char*)l->rdata,
							(((struct naptr_rdata*)l->rdata)->regexp));
				((struct naptr_rdata*)rr->rdata)->repl=
					translate_pointer((char*)rr->rdata, (char*)l->rdata,
							(((struct naptr_rdata*)l->rdata)->repl));
				rr->next=(void*)((char*)rr+
							ROUND_POINTER(ROUND_POINTER(sizeof(struct dns_rr))+
										NAPTR_RDATA_SIZE(
											*(struct naptr_rdata*)l->rdata)));
				tail_rr=&(rr->next);
			}
			break;
		case T_CNAME:
			for(l=tmp_lst; l; l=l->next){
				ttl=FIX_TTL(l->ttl);
				rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				max_ttl=MAX(max_ttl, ttl);
				rr->rdata=(void*)((char*)rr+sizeof(struct dns_rr));
				memcpy(rr->rdata, l->rdata, 
							CNAME_RDATA_SIZE(*(struct cname_rdata*)l->rdata));
				rr->next=(void*)((char*)rr+ROUND_POINTER(sizeof(struct dns_rr)+
							CNAME_RDATA_SIZE(*(struct cname_rdata*)l->rdata)));
				tail_rr=&(rr->next);
				rr=rr->next;
			}
			break;
		default:
			/* do nothing */
			LOG(L_CRIT, "BUG: dns_cache_mk_rd_entry: create: type %d not "
							"supported\n", type);
				;
	}
	*tail_rr=0; /* terminate the list */
	e->expire=now+S_TO_TICKS(max_ttl);
	free_rdata_list(tmp_lst);
	return e;
}



/* structure used only inside dns_cache_mk_rd_entry2 to break
 *  the list of records into records of the same type */
struct tmp_rec{
	struct rdata* rd;
	struct dns_hash_entry* e;
	struct dns_rr* rr;
	struct dns_rr** tail_rr;
	int max_ttl;
	int size;
};



/* create several dns hash entries from a list of rdata structs
 * returns 0 on error */
inline static struct dns_hash_entry* dns_cache_mk_rd_entry2(struct rdata* rd)
{
	struct rdata* l;
	ticks_t now;
	struct tmp_rec rec[MAX_DNS_RECORDS];
	int rec_idx[MAX_DNS_RECORDS];
	int r, i;
	int no_records; /* number of different records */
	unsigned int ttl;
	
	
	no_records=0; 
	rec[0].e=0;
	/* everything is allocated in one block: dns_hash_entry + name +
	 * + dns_rr + rdata_raw+ ....;  dns_rr must start at an aligned adress,
	 * hence we need to round dns_hash_entry+name size to a sizeof(long)
	 * multiple. If rdata type requires it, rdata_raw might need to be also
	 * aligned.
	 * Memory image:
	 * struct dns_hash_entry  (e)
	 * name (name_len+1 bytes)  (&e->name[0])
	 * padding to multiple of sizeof(char*)
	 * dns_rr1 (e->rr_lst)
	 * possible padding: no padding for a_rdata or aaaa_rdata, 
	 *                   multipe of sizeof(short) for srv_rdata,
	 *                   multiple of sizeof(long) for naptr_rdata and others
	 * dns_rr1->rdata  (e->rr_lst->rdata)
	 * padding to multipe of sizeof long
	 * dns_rr2 (e->rr_lst->next)
	 * ....
	 *
	 */
	/* compute size */
	for(l=rd, i=0; l && (i<MAX_DNS_RECORDS); l=l->next, i++){
		for (r=0; r<no_records; r++){
			if ((l->type==rec[r].rd->type) && 
					(l->name_len==rec[r].rd->name_len)
				&& (strncasecmp(l->name, rec[r].rd->name, l->name_len)==0)){
				/* found */
				goto found;
			}
		}
		/* not found, create new */
		if (no_records<MAX_DNS_RECORDS){
			rec[r].rd=l;
			rec[r].e=0;
			rec[r].size=ROUND_POINTER(sizeof(struct dns_hash_entry)+
							rec[r].rd->name_len-1+1);
			no_records++;
		}else{
			LOG(L_ERR, "ERROR: dns_cache_mk_rd_entry2: too many records: %d\n",
						no_records);
			/* skip */
			continue;
		}
found:
		rec_idx[i]=r;
		switch(l->type){
			case T_A:
				/* no padding */
				rec[r].size+=ROUND_POINTER(sizeof(struct dns_rr)+
										sizeof(struct a_rdata));
				break;
			case T_AAAA:
				/* no padding */
				rec[r].size+=ROUND_POINTER(sizeof(struct dns_rr)+
												sizeof(struct aaaa_rdata));
				break;
			case T_SRV:
				/* padding to short */
				rec[r].size+=ROUND_POINTER(ROUND_SHORT(sizeof(struct dns_rr))+
								SRV_RDATA_SIZE(*(struct srv_rdata*)l->rdata));
				break;
			case T_NAPTR:
					/* padding to char* */
				rec[r].size+=ROUND_POINTER(ROUND_POINTER(
												sizeof(struct dns_rr))+
							NAPTR_RDATA_SIZE(*(struct naptr_rdata*)l->rdata));
				break;
			case T_CNAME:
					/* no padding */
				rec[r].size+=ROUND_POINTER(sizeof(struct dns_rr)+
							CNAME_RDATA_SIZE(*(struct cname_rdata*)l->rdata));
				break;
			default:
				LOG(L_CRIT, "BUG: dns_cache_mk_rd_entry: type %d not "
							"supported\n", l->type);
		}
	}
	
	now=get_ticks_raw();
	/* alloc & init the entries */
	for (r=0; r<no_records; r++){
		rec[r].e=shm_malloc(rec[r].size);
		if (rec[r].e==0){
			LOG(L_ERR, "ERROR: dns_cache_mk_ip_entry: out of memory\n");
			goto error;
		}
		memset(rec[r].e, 0, rec[r].size); /* init with 0*/
		rec[r].e->total_size=rec[r].size;
		rec[r].e->name_len=rec[r].rd->name_len;
		rec[r].e->type=rec[r].rd->type;
		rec[r].e->last_used=now;
		/* memset makes sure is 0-term. */
		memcpy(rec[r].e->name, rec[r].rd->name, rec[r].rd->name_len); 
		rec[r].e->rr_lst=(struct dns_rr*)((char*)rec[r].e+
				ROUND_POINTER(sizeof(struct dns_hash_entry)+rec[r].e->name_len
								 -1+1));
		rec[r].tail_rr=&(rec[r].e->rr_lst);
		rec[r].rr=rec[r].e->rr_lst;
		rec[r].max_ttl=0;
		/* link them in a list */
		if (r==0){
			clist_init(rec[r].e, next, prev);
		}else{
			clist_append(rec[0].e, rec[r].e, next, prev);
		}
	}
	/* copy the actual data */
	for(l=rd, i=0; l && (i<MAX_DNS_RECORDS); l=l->next, i++){
		r=rec_idx[i];
		ttl=FIX_TTL(l->ttl);
		switch(l->type){
			case T_A:
				rec[r].rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				rec[r].max_ttl=MAX(rec[r].max_ttl, ttl);
				rec[r].rr->rdata=(void*)((char*)rec[r].rr+
									sizeof(struct dns_rr));
				memcpy(rec[r].rr->rdata, l->rdata, sizeof(struct a_rdata));
				rec[r].rr->next=(void*)((char*)rec[r].rr+
									ROUND_POINTER(sizeof(struct dns_rr)+
									sizeof(struct a_rdata)));
				rec[r].tail_rr=&(rec[r].rr->next);
				rec[r].rr=rec[r].rr->next;
				break;
			case T_AAAA:
				rec[r].rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				rec[r].max_ttl=MAX(rec[r].max_ttl, ttl);
				rec[r].rr->rdata=(void*)((char*)rec[r].rr+
									sizeof(struct dns_rr));
				memcpy(rec[r].rr->rdata, l->rdata, sizeof(struct aaaa_rdata));
				rec[r].rr->next=(void*)((char*)rec[r].rr+
									ROUND_POINTER(sizeof(struct dns_rr)+
									sizeof(struct aaaa_rdata)));
				rec[r].tail_rr=&(rec[r].rr->next);
				rec[r].rr=rec[r].rr->next;
				break;
			case T_SRV:
				rec[r].rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				rec[r].max_ttl=MAX(rec[r].max_ttl, ttl);
				rec[r].rr->rdata=(void*)((char*)rec[r].rr+
								ROUND_SHORT(sizeof(struct dns_rr)));
				/* copy the whole srv_rdata block*/
				memcpy(rec[r].rr->rdata, l->rdata, 
						SRV_RDATA_SIZE(*(struct srv_rdata*)l->rdata) );
				rec[r].rr->next=(void*)((char*)rec[r].rr+
							ROUND_POINTER( ROUND_SHORT(sizeof(struct dns_rr))+
										SRV_RDATA_SIZE(
											*(struct srv_rdata*)l->rdata)));
				rec[r].tail_rr=&(rec[r].rr->next);
				rec[r].rr=rec[r].rr->next;
				break;
			case T_NAPTR:
				rec[r].rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				rec[r].max_ttl=MAX(rec[r].max_ttl, ttl);
				rec[r].rr->rdata=(void*)((char*)rec[r].rr+
								ROUND_POINTER(sizeof(struct dns_rr)));
				/* copy the whole srv_rdata block*/
				memcpy(rec[r].rr->rdata, l->rdata, 
						NAPTR_RDATA_SIZE(*(struct naptr_rdata*)l->rdata) );
				/* adjust the string pointer */
				((struct naptr_rdata*)rec[r].rr->rdata)->flags=
					translate_pointer((char*)rec[r].rr->rdata, (char*)l->rdata,
							(((struct naptr_rdata*)l->rdata)->flags));
				((struct naptr_rdata*)rec[r].rr->rdata)->services=
					translate_pointer((char*)rec[r].rr->rdata, (char*)l->rdata,
							(((struct naptr_rdata*)l->rdata)->services));
				((struct naptr_rdata*)rec[r].rr->rdata)->regexp=
					translate_pointer((char*)rec[r].rr->rdata, (char*)l->rdata,
							(((struct naptr_rdata*)l->rdata)->regexp));
				((struct naptr_rdata*)rec[r].rr->rdata)->repl=
					translate_pointer((char*)rec[r].rr->rdata, (char*)l->rdata,
							(((struct naptr_rdata*)l->rdata)->repl));
				rec[r].rr->next=(void*)((char*)rec[r].rr+
							ROUND_POINTER(ROUND_POINTER(sizeof(struct dns_rr))+
										NAPTR_RDATA_SIZE(
											*(struct naptr_rdata*)l->rdata)));
				rec[r].tail_rr=&(rec[r].rr->next);
				break;
			case T_CNAME:
				rec[r].rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				rec[r].max_ttl=MAX(rec[r].max_ttl, ttl);
				rec[r].rr->rdata=(void*)((char*)rec[r].rr
									+sizeof(struct dns_rr));
				memcpy(rec[r].rr->rdata, l->rdata,
							CNAME_RDATA_SIZE(*(struct cname_rdata*)l->rdata));
				rec[r].rr->next=(void*)((char*)rec[r].rr+
							ROUND_POINTER(sizeof(struct dns_rr)+
							CNAME_RDATA_SIZE(*(struct cname_rdata*)l->rdata)));
				rec[r].tail_rr=&(rec[r].rr->next);
				rec[r].rr=rec[r].rr->next;
				break;
			default:
				/* do nothing */
				;
		}
	}
	for (r=0; r<no_records; r++){
		*rec[r].tail_rr=0; /* terminate the list */
		rec[r].e->expire=now+S_TO_TICKS(rec[r].max_ttl);
	}
	return rec[0].e;
error:
	for (r=0; r<no_records; r++){
		dns_destroy_entry(rec[r].e);
	}
	return 0;
}



inline static struct dns_hash_entry* dns_get_entry(str* name, int type);


#define CACHE_RELEVANT_RECS_ONLY

#ifdef CACHE_RELEVANT_RECS_ONLY
/* internal only: gets related entries from a rdata list, appends them
 * to e (list) and returns:
 *  - e if e is of the requested type
 *  -  if e is a CNAME, tries to get to the end of the CNAME chain and returns
 *      the final entry if the types match or 0 if the chain is unfinished
 *  - 0 on error/not found
 * records is modified (the used records are removed from the list and freed)
 *
 * WARNING: - records must be pkg_malloc'ed
 * Notes:   - if the return is 0 and e->type==T_CNAME, the list will contain
 *            the CNAME chain (the last element being the last CNAME)
 *  */
inline static struct dns_hash_entry* dns_get_related(struct dns_hash_entry* e,
														int type,
														struct rdata** records)
{
	struct dns_hash_entry* ret;
	struct dns_hash_entry* l;
	struct dns_hash_entry* t;
	struct dns_hash_entry* lst_end;
	struct dns_rr* rr;
	static int cname_chain_len=0;
	str tmp;
	
	ret=0;
	l=e;
	DBG("dns_get_related(%p (%.*s, %d), %d, *%p) (%d)\n", e,
			e->name_len, e->name, e->type, type, *records, cname_chain_len);
	clist_init(l, next, prev);
	if (type==e->type){
		ret=e;
		switch(e->type){
			case T_SRV:
				for (rr=e->rr_lst; rr && *records; rr=rr->next){
					tmp.s=((struct srv_rdata*)rr->rdata)->name;
					tmp.len=((struct srv_rdata*)rr->rdata)->name_len;
					if (!(dns_flags&DNS_IPV6_ONLY)){
						t=dns_cache_mk_rd_entry(&tmp, T_A, records);
						if (t){
							if ((t->type==T_CNAME) && *records)
								dns_get_related(t, T_A, records);
							clist_append(l, t, next, prev);
						}
					}
					if (!(dns_flags&DNS_IPV4_ONLY)){
						t=dns_cache_mk_rd_entry(&tmp, T_AAAA, records);
						if (t){
							if ((t->type==T_CNAME) && *records)
								dns_get_related(t, T_AAAA, records);
							clist_append(l, t, next, prev);
						}
					}
				}
				break;
			default:
				/* nothing extra */
				break;
		}
	}else if ((e->type==T_CNAME) && (cname_chain_len<MAX_CNAME_CHAIN)){
		/* only one cname is allowed (rfc2181), so we ignore
		 * the others (we take only the first one) */
		tmp.s=((struct cname_rdata*)e->rr_lst->rdata)->name;
		tmp.len=((struct cname_rdata*)e->rr_lst->rdata)->name_len;
		t=dns_cache_mk_rd_entry(&tmp, type, records);
		if (t){
			if (*records){
				cname_chain_len++;
				ret=dns_get_related(t, type, records);
				cname_chain_len--;
				lst_end=t->prev;
				clist_append_sublist(l, t, lst_end, next, prev);
			}else{
				/* if no more recs, but we found the orig. target anyway,
				 *  return it (e.g. recs are only CNAME x & x A 1.2.3.4 or 
				 *  CNAME & SRV) */
				if (t->type==type)
					ret=t; 
				clist_append(l, t, next, prev);
			}
		}
	}
	return ret;
}
#endif



/* calls the external resolver and populates the cache with the result
 * returns: 0 on error, pointer to hash entry on success
 * WARNING: make sure you use dns_hash_entry_put() when you're
 *  finished with the result)
 * */
inline static struct dns_hash_entry* dns_cache_do_request(str* name, int type)
{
	struct rdata* records;
	struct dns_hash_entry* e;
	struct dns_hash_entry* l;
	struct dns_hash_entry* r;
	struct dns_hash_entry* t;
	struct ip_addr* ip;
	str cname_val;
	char name_buf[MAX_DNS_NAME];
	
	e=0;
	l=0;
	cname_val.s=0;
	
	if (type==T_A){
		if ((ip=str2ip(name))!=0){
				e=dns_cache_mk_ip_entry(name, ip);
				if (e)
					atomic_set(&e->refcnt, 1);/* because we ret. a ref. to it*/
				goto end; /* we do not cache obvious stuff */
		}
	}else if (type==T_AAAA){
		if ((ip=str2ip6(name))!=0){
				e=dns_cache_mk_ip_entry(name, ip);
				if (e)
					atomic_set(&e->refcnt, 1);/* because we ret. a ref. to it*/
				goto end;/* we do not cache obvious stuff */
		}
	}
	if (name->len>=MAX_DNS_NAME){
		LOG(L_ERR, "ERROR: dns_cache_do_request: name too long (%d chars)\n",
					name->len);
		goto end;
	}
	/* null terminate the string, needed by get_record */
	memcpy(name_buf, name->s, name->len);
	name_buf[name->len]=0;
	records=get_record(name_buf, type, RES_AR);
	if (records){
#ifdef CACHE_RELEVANT_RECS_ONLY
		e=dns_cache_mk_rd_entry(name, type, &records);
		if (e){
			l=e;
			e=dns_get_related(l, type, &records);
			/* e should contain the searched entry (if found) and l
			 * all the entries (e and related) */
			if (e){
				atomic_set(&e->refcnt, 1); /* 1 because we return a 
												ref. to it */
			}else{
				/* e==0 => l contains a  cname list => we use the last
				 * cname from the chain for a new resolve attempt (l->prev) */
				/* only one cname record is allowed (rfc2181), so we ignore 
				 * the others (we take only the first one) */
				cname_val.s=
					((struct cname_rdata*)l->prev->rr_lst->rdata)->name;
				cname_val.len=
					((struct cname_rdata*)l->prev->rr_lst->rdata)->name_len;
				DBG("dns_cache_do_request: cname detected: %.*s (%d)\n",
						cname_val.len, cname_val.s, cname_val.len);
			}
			/* add all the records to the hash */
			l->prev->next=0; /* we break the double linked list for easier
								searching */
			LOCK_DNS_HASH(); /* optimization */
			for (r=l; r; r=t){
				t=r->next;
				dns_cache_add_unsafe(r); /* refcnt++ inside */
				if (atomic_get(&r->refcnt)==0){
					/* if cache adding failed and nobody else is interested
					 * destroy this entry */
					dns_destroy_entry(r);
				}
			}
			UNLOCK_DNS_HASH();
			/* if only cnames found => try to resolve the last one */
			if (cname_val.s){ 
				DBG("dns_cache_do_request: dns_get_entry(cname: %.*s (%d))\n",
						cname_val.len, cname_val.s, cname_val.len);
				e=dns_get_entry(&cname_val, type);
			}
		}
#else
		l=dns_cache_mk_rd_entry2(records);
#endif
		free_rdata_list(records);
	}else if (dns_neg_cache_ttl){
		e=dns_cache_mk_bad_entry(name, type, dns_neg_cache_ttl, DNS_BAD_NAME);
		atomic_set(&e->refcnt, 1); /* 1 because we return a ref. to it */
		dns_cache_add(e); /* refcnt++ inside*/
		goto end;
	}
#ifndef CACHE_RELEVANT_RECS_ONLY
	if (l){
		/* add all the records to the cache, but return only the record
		 * we are looking for */
		l->prev->next=0; /* we break the double linked list for easier
							searching */
		LOCK_DNS_HASH(); /* optimization */
		for (r=l; r; r=t){
			t=r->next;
			if (e==0){ /* no entry found yet */
				if (r->type==T_CNAME){
					if ((r->name_len==name->len) && (r->rr_lst) &&
							(strncasecmp(r->name, name->s, name->len)==0)){
						/* update the name with the name from the cname rec. */
						cname_val.s=
								((struct cname_rdata*)r->rr_lst->rdata)->name;
						cname_val.len=
							((struct cname_rdata*)r->rr_lst->rdata)->name_len;
						name=&cname_val;
					}
				}else if ((r->type==type) && (r->name_len==name->len) &&
							(strncasecmp(r->name, name->s, name->len)==0)){
					e=r;
					atomic_set(&e->refcnt, 1); /* 1 because we return a ref. 
												  to it */
				}
			}
			dns_cache_add_unsafe(r); /* refcnt++ inside */
			if (atomic_get(&r->refcnt)==0){
				/* if cache adding failed and nobody else is interested
				 * destroy this entry */
				dns_destroy_entry(r);
			}
		}
		UNLOCK_DNS_HASH();
		if ((e==0) && (cname_val.s)){ /* not found, but found a cname */
			/* only one cname is allowed (rfc2181), so we ignore the
			 * others (we take only the first one) */
			e=dns_get_entry(&cname_val, type);
		}
	}
#endif
end:
	return e;
}



/* tries to lookup (name, type) in the hash and if not found tries to make
 *  a dns request
 *  return: 0 on error, pointer to a dns_hash_entry on success
 *  WARNING: when *   not needed anymore dns_hash_put() must be called! */
inline static struct dns_hash_entry* dns_get_entry(str* name, int type)
{
	int h;
	struct dns_hash_entry* e;
	str cname_val;
	int err;
	static int rec_cnt=0; /* recursion protection */
	
	e=0;
	if (rec_cnt>MAX_CNAME_CHAIN){
		LOG(L_WARN, "WARNING: dns_get_entry: CNAME chain too long or"
				" recursive CNAMEs (\"%.*s\")\n", name->len, name->s);
		goto error;
	}
	rec_cnt++;
	e=dns_hash_get(name, type, &h, &err);
	if ((e==0) && ((err) || ((e=dns_cache_do_request(name, type))==0))){
		goto error;
	}else if ((e->type==T_CNAME) && (type!=T_CNAME)){
		/* cname found instead which couldn't be resolved with the cached
		 * info => try a dns request */
		/* only one cname record is allowed (rfc2181), so we ignore 
		 * the others (we take only the first one) */
		cname_val.s= ((struct cname_rdata*)e->rr_lst->rdata)->name;
		cname_val.len=((struct cname_rdata*)e->rr_lst->rdata)->name_len;
		dns_hash_put(e); /* not interested in the cname anymore */
		if ((e=dns_cache_do_request(&cname_val, type))==0)
			goto error; /* could not resolve cname */
	}
	/* found */
	if ((e->rr_lst==0) || e->err_flags){
		/* negative cache => not resolvable */
		dns_hash_put(e);
		e=0;
	}
error:
	rec_cnt--;
	return e;
}



/* gets the first non-expired, good record starting with record no
 * from the dns_hash_entry struct e
 * params:       e   - dns_hash_entry struct
 *               *no - it must contain the start record number (0 initially);
 *                      it will be filled with the returned record number
 *               now - current time/ticks value
 * returns pointer to the rr on success and sets no to the rr number
 *         0 on error and fills the error flags
 *
 * Example usage:
 * list all non-expired non-bad-marked ips for name:
 * e=dns_get_entry(name, T_A);
 * if (e){
 *    *no=0;
 *    now=get_ticks_raw();
 *    while(rr=dns_entry_get_rr(e, no, now){
 *       DBG("address %d\n", *no);
 *       *no++;  ( get the next address next time )
 *     }
 *  }
 */
inline static struct dns_rr* dns_entry_get_rr(	struct dns_hash_entry* e,
											 unsigned char* no, ticks_t now)
{
	struct dns_rr* rr;
	int n;
	int flags;
	
	flags=0;
	for(rr=e->rr_lst, n=0;rr && (n<*no);rr=rr->next, n++);/* skip *no records*/
	for(;rr;rr=rr->next){
		if ((s_ticks_t)(now-e->expire)>=0) /* expired entry */
			continue;
		if (rr->err_flags){ /* bad rr */
			continue;
		}
		/* everything is ok now */
		*no=n;
		return rr;
	}
	*no=n;
	return 0;
}



/* gethostbyname compatibility: converts a dns_hash_entry structure 
 * to a statical internal hostent structure
 * returns a pointer to the internal hostent structure on success or
 *          0 on error 
 */
struct hostent* dns_entry2he(struct dns_hash_entry* e)
{
	static struct hostent he;
	static char hostname[256];
	static char* p_aliases[1];
	static char* p_addr[DNS_HE_MAX_ADDR+1];
	static char address[16*DNS_HE_MAX_ADDR]; /* max 10 ipv6 addresses */
	int af, len;
	struct dns_rr* rr;
	unsigned char rr_no;
	ticks_t now;
	int i;
	
	switch(e->type){
		case T_A:
			af=AF_INET;
			len=4;
			break;
		case T_AAAA:
			af=AF_INET6;
			len=16;
			break;
		default:
			LOG(L_CRIT, "BUG: dns_entry2he: wrong entry type %d for %.*s\n",
					e->type, e->name_len, e->name);
			return 0;
	}
	
	
	rr_no=0;
	now=get_ticks_raw();
	rr=dns_entry_get_rr(e, &rr_no, now);
	for(i=0; rr && (i<DNS_HE_MAX_ADDR); i++, 
							rr=dns_entry_get_rr(e, &rr_no, now)){
				p_addr[i]=&address[i*len];
				memcpy(p_addr[i], ((struct a_rdata*)rr->rdata)->ip, len);
	}
	if (i==0){
		DBG("DEBUG: dns_entry2he: no good records found (%d) for %.*s (%d)\n",
				rr_no, e->name_len, e->name, e->type);
		return 0; /* no good record found */
	}
	
	p_addr[i]=0; /* mark the end of the addresses */
	p_aliases[0]=0; /* no aliases */
	memcpy(hostname, e->name, e->name_len);
	hostname[e->name_len]=0;
	
	he.h_addrtype=af;
	he.h_length=len;
	he.h_addr_list=p_addr;
	he.h_aliases=p_aliases;
	he.h_name=hostname;
	
	return &he;
}



/* gethostbyname compatibility: performs an a_lookup and returns a pointer 
 * to a statical internal hostent structure
 * returns 0 on success, <0 on error (see the error codes)
 */
struct hostent* dns_a_get_he(str* name)
{
	struct dns_hash_entry* e;
	struct ip_addr* ip;
	struct hostent* he;
	
	e=0;
	if ((ip=str2ip(name))!=0){
		return ip_addr2he(name, ip);
	}
	if ((e=dns_get_entry(name, T_A))==0)
		return 0;
	/* found */
	he=dns_entry2he(e);
	dns_hash_put(e);
	return he;
}



/* gethostbyname compatibility: performs an aaaa_lookup and returns a pointer 
 * to a statical internal hostent structure
 * returns 0 on success, <0 on error (see the error codes)
 */
struct hostent* dns_aaaa_get_he(str* name)
{
	struct dns_hash_entry* e;
	struct ip_addr* ip;
	struct hostent* he;
	
	e=0;
	if ((ip=str2ip6(name))!=0){
		return ip_addr2he(name, ip);
	}
	if ((e=dns_get_entry(name, T_AAAA))==0)
			return 0;
	/* found */
	he=dns_entry2he(e);
	dns_hash_put(e);
	return he;
}



/* returns 0 on success, -1 on error (rr type does not contain an ip) */
inline static int dns_rr2ip(int type, struct dns_rr* rr, struct ip_addr* ip)
{
	switch(type){
		case T_A:
			ip->af=AF_INET;
			ip->len=4;
			memcpy(ip->u.addr, ((struct a_rdata*)rr->rdata)->ip, 4);
			return 0;
			break;
		case T_AAAA:
			ip->af=AF_INET6;
			ip->len=16;
			memcpy(ip->u.addr, ((struct aaaa_rdata*)rr->rdata)->ip6, 16);
			return 0;
			break;
	}
	return -1;
}



/* gethostbyname compatibility:
 * performs an a or aaaa dns lookup, returns 0 on error and a pointer to a
 *          static hostent structure on success
 *  flags:  - none set: tries first an a_lookup and if it fails an aaaa_lookup
 *          - DNS_IPV6_FIRST: tries first an aaaa_lookup and then an a_lookup
 *          - DNS_IPV4_ONLY: tries only an a_lookup
 *          - DNS_IPV6_ONLY: tries only an aaaa_lookup
 */
struct hostent* dns_get_he(str* name, int flags)
{
	struct hostent* he;
	
	
	if ((flags&(DNS_IPV6_FIRST|DNS_IPV6_ONLY))){
		he=dns_aaaa_get_he(name);
		if (he) return he;
	}else{
		he=dns_a_get_he(name);
		if (he) return he;
	}
	if (flags&DNS_IPV6_FIRST){
		he=dns_a_get_he(name);
	}else if (!(flags&(DNS_IPV6_ONLY|DNS_IPV4_ONLY))){
		he=dns_aaaa_get_he(name);
	}
	return he;
}



/* sip_resolvehost helper: gets the first good  hostent/port combination
 * returns 0 on error, pointer to static hostent structure on success
 *           (and sets port)*/
struct hostent* dns_srv_get_he(str* name, unsigned short* port, int flags)
{
	struct dns_hash_entry* e;
	struct dns_rr* rr;
	str rr_name;
	struct hostent* he;
	ticks_t now;
	unsigned char rr_no;
	
	rr=0;
	he=0;
	now=get_ticks_raw();
	if ((e=dns_get_entry(name, T_SRV))==0)
			goto error;
	/* look inside the RRs for a good one (not expired or marked bad)  */
	rr_no=0;
	while( (rr=dns_entry_get_rr(e, &rr_no, now))!=0){
		/* everything is ok now, we can try to resolve the ip */
		rr_name.s=((struct srv_rdata*)rr->rdata)->name;
		rr_name.len=((struct srv_rdata*)rr->rdata)->name_len;
		if ((he=dns_get_he(&rr_name, flags))!=0){
				/* success, at least one good ip found */
				*port=((struct srv_rdata*)rr->rdata)->port;
				goto end;
		}
		rr_no++; /* try from the next record, the current one was not good */
	}
	/* if we reach this point => error, we couldn't find any good rr */
end:
	if (e) dns_hash_put(e);
error:
	return he;
}



struct hostent* dns_resolvehost(char* name)
{
	str host;
	
	if ((use_dns_cache==0) || (dns_hash==0)){ /* not init yet */
		return _resolvehost(name);
	}
	host.s=name;
	host.len=strlen(name);
	return dns_get_he(&host, dns_flags);
}



/* resolves a host name trying SRV lookup if *port==0 or normal A/AAAA lookup
 * if *port!=0.
 * when performing SRV lookup (*port==0) it will use proto to look for
 * tcp or udp hosts, otherwise proto is unused; if proto==0 => no SRV lookup
 * returns: hostent struct & *port filled with the port from the SRV record;
 *  0 on error
 */
struct hostent* dns_sip_resolvehost(str* name, unsigned short* port, int proto)
{
	struct hostent* he;
	struct ip_addr* ip;
	static char tmp[MAX_DNS_NAME]; /* tmp. buff. for SRV lookups */
	int len;
	str srv_name;

	if ((use_dns_cache==0) || (dns_hash==0)){ 
		/* not init or off => use normal, non-cached version */
		return _sip_resolvehost(name, port, proto);
	}
	len=0;
	/* try SRV if no port specified (draft-ietf-sip-srv-06) */
	if ((port)&&(*port==0)){
		*port=(proto==PROTO_TLS)?SIPS_PORT:SIP_PORT; /* just in case we don't
														find another */
		if ((name->len+SRV_MAX_PREFIX_LEN+1)>MAX_DNS_NAME){
			LOG(L_WARN, "WARNING: dns_sip_resolvehost: domain name too long"
						" (%d), unable to perform SRV lookup\n", name->len);
		}else{
			/* check if it's an ip address */
			if ( ((ip=str2ip(name))!=0)
#ifdef	USE_IPV6
				  || ((ip=str2ip6(name))!=0)
#endif
				){
				/* we are lucky, this is an ip address */
				return ip_addr2he(name,ip);
			}
			
			switch(proto){
				case PROTO_NONE: /* no proto specified, use udp */
					goto skip_srv;
				case PROTO_UDP:
					memcpy(tmp, SRV_UDP_PREFIX, SRV_UDP_PREFIX_LEN);
					memcpy(tmp+SRV_UDP_PREFIX_LEN, name->s, name->len);
					tmp[SRV_UDP_PREFIX_LEN + name->len] = '\0';
					len=SRV_UDP_PREFIX_LEN + name->len;
					break;
				case PROTO_TCP:
					memcpy(tmp, SRV_TCP_PREFIX, SRV_TCP_PREFIX_LEN);
					memcpy(tmp+SRV_TCP_PREFIX_LEN, name->s, name->len);
					tmp[SRV_TCP_PREFIX_LEN + name->len] = '\0';
					len=SRV_TCP_PREFIX_LEN + name->len;
					break;
				case PROTO_TLS:
					memcpy(tmp, SRV_TLS_PREFIX, SRV_TLS_PREFIX_LEN);
					memcpy(tmp+SRV_TLS_PREFIX_LEN, name->s, name->len);
					tmp[SRV_TLS_PREFIX_LEN + name->len] = '\0';
					len=SRV_TLS_PREFIX_LEN + name->len;
					break;
				default:
					LOG(L_CRIT, "BUG: sip_resolvehost: unknown proto %d\n",
							proto);
					return 0;
			}

			srv_name.s=tmp;
			srv_name.len=len;
			if ((he=dns_srv_get_he(&srv_name, port, dns_flags))!=0)
				return he;
		}
	}
skip_srv:
	if (name->len >= MAX_DNS_NAME) {
		LOG(L_ERR, "dns_sip_resolvehost: domain name too long\n");
		return 0;
	}
	he=dns_get_he(name, dns_flags);
	return he;
}



/* performs an a lookup, fills the dns_entry pointer and the ip addr.
 *  (with the first good ip). if *e ==0 does the a lookup, and changes it
 *   to the result, if not it uses the current value and tries to use 
 *   the rr_no record from it.
 * params:  e - must contain the "in-use" dns_hash_entry pointer (from
 *               a previous call) or *e==0 (for the first call)
 *          name - host name for which we do the lookup (required only
 *                  when *e==0)
 *          ip   - will be filled with the first good resolved ip started
 *                 at *rr_no
 *          rr_no - record number to start searching for a good ip from
 *                  (e.g. value from previous call + 1), filled on return
 *                  with the number of the record corresponding to the 
 *                  returned ip
 * returns 0 on success, <0 on error (see the error codes),
 *         fills e, ip and rr_no
 *          On end of records (when use to iterate on all the ips) it
 *          will return E_DNS_EOR (you should not log an error for this
 *          value, is just a signal that the address list end has been reached)
 * WARNING: dns_hash_put(*e) must be called when you don't need
 *          the entry anymore and *e!=0 (failling to do so => mem. leak)
 * Example:
 *  dns_entry=0;
 *  ret=dns_a_get_ip(&dns_entry, name, &ip, &rr_no);  -- get the first rr.
 *  ...
 *  rr_no++;
 *  while((ret>=0) && dns_entry)
 *     dns_a_get_ip(&dns_entry, name, &ip, &rr_no); -- get the next rr
 *   if (ret!=-E_DNS_EOR) ERROR(....);
 *  ...
 *  dns_hash_put(dns_entry); -- finished with the entry
 */
int dns_a_resolve(struct dns_hash_entry** e, unsigned char* rr_no,
					str* name, struct ip_addr* ip)
{
	struct dns_rr* rr;
	int ret;
	ticks_t now;
	struct ip_addr* tmp;
	
	rr=0;
	ret=-E_DNS_NO_IP; 
	if (*e==0){ /* do lookup */
		/* if ip don't set *e */
		if ((tmp=str2ip(name))!=0){
			*ip=*tmp;
			*rr_no=0;
			return 0;
		}
		if ((*e=dns_get_entry(name, T_A))==0)
			goto error;
		/* found */
		*rr_no=0;
		ret=-E_DNS_BAD_IP_ENTRY;
	}
	now=get_ticks_raw();
	rr=dns_entry_get_rr(*e, rr_no, now);
	if (rr){
		/* everything is ok now, we can try to "convert" the ip */
		dns_rr2ip((*e)->type, rr, ip);
		ret=0;
	}else{
		ret=-E_DNS_EOR;
	}
error:
	DBG("dns_a_resovle(%.*s, %d) returning %d\n",
			name->len, name->s, *rr_no, ret);
	return ret;
}



/* lookup, fills the dns_entry pointer and the ip addr.
 *  (with the first good ip). if *e ==0 does the a lookup, and changes it
 *   to the result, if not it uses the current value and tries to use 
 * Same as dns_a_resolve but for aaaa records (see above).
 */
int dns_aaaa_resolve(struct dns_hash_entry** e, unsigned char* rr_no, 
						str* name, struct ip_addr* ip)
{
	struct dns_rr* rr;
	int ret;
	ticks_t now;
	struct ip_addr* tmp;
	
	rr=0;
	ret=-E_DNS_NO_IP; 
	if (*e==0){ /* do lookup */
		/* if ip don't set *e */
		if ((tmp=str2ip6(name))!=0){
			*ip=*tmp;
			*rr_no=0;
			return 0;
		}
		if ((*e=dns_get_entry(name, T_AAAA))==0)
			goto error;
		/* found */
		*rr_no=0;
		ret=-E_DNS_BAD_IP_ENTRY;
	}
	now=get_ticks_raw();
	rr=dns_entry_get_rr(*e, rr_no, now);
	if (rr){
		/* everything is ok now, we can try to "convert" the ip */
		dns_rr2ip((*e)->type, rr, ip);
		ret=0;
	}else{
		ret=-E_DNS_EOR; /* no more records */
	}
error:
	return ret;
}



/* performs an a or aaaa dns lookup, returns <0 on error (see the
 *  dns error codes) and 0 on success
 *  flags:  - none set: tries first an a_lookup and if it fails an aaaa_lookup
 *          - DNS_IPV6_FIRST: tries first an aaaa_lookup and then an a_lookup
 *          - DNS_IPV4_ONLY: tries only an a_lookup
 *          - DNS_IPV6_ONLY: tries only an aaaa_lookup
 *  see dns_a_resolve() for the rest of the params., examples a.s.o
 *  WARNING: don't forget dns_hash_put(*e) when e is not needed anymore
 */
int dns_ip_resolve(struct dns_hash_entry** e, unsigned char* rr_no, 
					str* name, struct ip_addr* ip, int flags)
{
	int ret;
	
	if ((flags&(DNS_IPV6_FIRST|DNS_IPV6_ONLY))){
		ret=dns_aaaa_resolve(e, rr_no, name, ip);
		if (ret>=0) return ret;
	}else{
		ret=dns_a_resolve(e, rr_no, name, ip);
		if (ret>=0) return ret;
	}
	if (flags&DNS_IPV6_FIRST){
		ret=dns_a_resolve(e, rr_no, name, ip);
	}else if (!(flags&(DNS_IPV6_ONLY|DNS_IPV4_ONLY))){
		ret=dns_aaaa_resolve(e, rr_no, name, ip);
	}
	return ret;
}



/*  gets the first srv record starting at rr_no
 *  (similar to dns_a_resolve but for srv, sets host, port)
 */
int dns_srv_resolve(struct dns_hash_entry** e, unsigned char* rr_no,
					str* name, str* host, unsigned short* port)
{
	struct dns_rr* rr;
	int ret;
	ticks_t now;
	
	rr=0;
	ret=-E_DNS_NO_SRV; 
	if (*e==0){
		if ((*e=dns_get_entry(name, T_SRV))==0)
			goto error;
		/* found it */
		*rr_no=0;
		ret=-E_DNS_BAD_SRV_ENTRY;
	}
	now=get_ticks_raw();
	rr=dns_entry_get_rr(*e, rr_no, now);
	if (rr){
		host->s=((struct srv_rdata*)rr->rdata)->name;
		host->len=((struct srv_rdata*)rr->rdata)->name_len;
		*port=((struct srv_rdata*)rr->rdata)->port;
		ret=0;
	}else{
		ret=-E_DNS_EOR; /* no more records */
	}
error:
	return ret;
}



/*  gets the first srv record starting at h->srv_no, resolve it
 *   and get the first ip address (starting at h->ip_no)
 *  (similar to dns_a_resolve but for srv, sets host, port)
 *  WARNING: don't forget to init h prior to calling this function the first
 *   time and dns_srv_handle_put(h), even if error is returned
 */
int dns_srv_resolve_ip(struct dns_srv_handle* h,
					str* name, struct ip_addr* ip, unsigned short* port,
					int flags)
{
	int ret;
	str host;
	
	host.len=0;
	host.s=0;
	do{
		if (h->a==0){ 
			if ((ret=dns_srv_resolve(&h->srv, &h->srv_no,
													name, &host, port))<0)
				goto error;
			h->port=*port; /* store new port */
		}else{
			*port=h->port; /* return the stored port */
		}
		if ((ret=dns_ip_resolve(&h->a, &h->ip_no, &host, ip, flags))<0){
			/* couldn't find any good ip for this record, try the next one */
			h->srv_no++;
			if (h->a){
				dns_hash_put(h->a);
				h->a=0;
			}
		}else if (h->a==0){
			/* this was an ip, try the next srv record in the future */
			h->srv_no++;
		}
	}while(ret<0);
error:
	DBG("dns_srv_resolve_ip(\"%.*s\", %d, %d), ret=%d, ip=%s\n", 
			name->len, name->s, h->srv_no, h->ip_no, ret, ip_addr2a(ip));
	return ret;
}



/* resolves a host name trying SRV lookup if *port==0 or normal A/AAAA lookup
 * if *port!=0.
 * when performing SRV lookup (*port==0) it will use proto to look for
 * tcp or udp hosts, otherwise proto is unused; if proto==0 => no SRV lookup
 * dns_res_h must be initialized prior to  calling this function and can be
 * used to get the subsequent ips
 * returns:  <0 on error
 *            0 on success and it fills *ip, *port, dns_sip_resolve_h
 * WARNING: when finished, dns_sip_resolve_put(dns_res_h) must be called!
 */
int dns_sip_resolve(struct dns_srv_handle* h,  str* name,
						struct ip_addr* ip, unsigned short* port, int proto,
						int flags)
{
	static char tmp[MAX_DNS_NAME]; /* tmp. buff. for SRV lookups */
	int len;
	str srv_name;
	struct ip_addr* tmp_ip;
	int ret;
	struct hostent* he;

	if (dns_hash==0){ /* not init => use normal, non-cached version */
		LOG(L_WARN, "WARNING: dns_sip_resolve: called before dns cache"
					" initialization\n");
		h->srv=h->a=0;
		he=_sip_resolvehost(name, port, proto);
		if (he){
			hostent2ip_addr(ip, he, 0);
			return 0;
		}
		return -E_DNS_NO_SRV;
	}
	len=0;
	if ((h->srv==0) && (h->a==0)){
		h->port=(proto==PROTO_TLS)?SIPS_PORT:SIP_PORT; /* just in case we
														don't find another */
		if (port){
			if (*port==0){
				/* try SRV if initial call & no port specified
				 * (draft-ietf-sip-srv-06) */
				if ((name->len+SRV_MAX_PREFIX_LEN+1)>MAX_DNS_NAME){
					LOG(L_WARN, "WARNING: dns_sip_resolvehost: domain name too"
								" long (%d), unable to perform SRV lookup\n",
								name->len);
				}else{
					/* check if it's an ip address */
					if ( ((tmp_ip=str2ip(name))!=0)
#ifdef	USE_IPV6
						  || ((tmp_ip=str2ip6(name))!=0)
#endif
						){
						/* we are lucky, this is an ip address */
#ifdef	USE_IPV6
						if (((flags&DNS_IPV4_ONLY) && (tmp_ip->af==AF_INET6))||
							((flags&DNS_IPV6_ONLY) && (tmp_ip->af==AF_INET))){
							return -E_DNS_AF_MISMATCH;
						}
#endif
						*ip=*tmp_ip;
						*port=h->port;
						return 0;
					}
					
					switch(proto){
						case PROTO_NONE: /* no proto specified, use udp */
							goto skip_srv;
						case PROTO_UDP:
							memcpy(tmp, SRV_UDP_PREFIX, SRV_UDP_PREFIX_LEN);
							memcpy(tmp+SRV_UDP_PREFIX_LEN, name->s, name->len);
							tmp[SRV_UDP_PREFIX_LEN + name->len] = '\0';
							len=SRV_UDP_PREFIX_LEN + name->len;
							break;
						case PROTO_TCP:
							memcpy(tmp, SRV_TCP_PREFIX, SRV_TCP_PREFIX_LEN);
							memcpy(tmp+SRV_TCP_PREFIX_LEN, name->s, name->len);
							tmp[SRV_TCP_PREFIX_LEN + name->len] = '\0';
							len=SRV_TCP_PREFIX_LEN + name->len;
							break;
						case PROTO_TLS:
							memcpy(tmp, SRV_TLS_PREFIX, SRV_TLS_PREFIX_LEN);
							memcpy(tmp+SRV_TLS_PREFIX_LEN, name->s, name->len);
							tmp[SRV_TLS_PREFIX_LEN + name->len] = '\0';
							len=SRV_TLS_PREFIX_LEN + name->len;
							break;
						default:
							LOG(L_CRIT, "BUG: sip_resolvehost: "
									"unknown proto %d\n", proto);
							return -E_DNS_CRITICAL;
					}
					srv_name.s=tmp;
					srv_name.len=len;
					
					if ((ret=dns_srv_resolve_ip(h, &srv_name, ip,
															port, flags))>=0)
					{
						DBG("dns_sip_resolve(%.*s, %d, %d), srv0, ret=%d\n", 
							name->len, name->s, h->srv_no, h->ip_no, ret);
						return ret;
					}
				}
			}else{ /* if (*port==0) */
				h->port=*port; /* store initial port */
			}
		} /* if (port) */
	}else if (h->srv){
			srv_name.s=h->srv->name;
			srv_name.len=h->srv->name_len;
			/* continue srv resolving */
			ret=dns_srv_resolve_ip(h, &srv_name, ip, port, flags);
			DBG("dns_sip_resolve(%.*s, %d, %d), srv, ret=%d\n", 
					name->len, name->s, h->srv_no, h->ip_no, ret);
			return ret;
	}
skip_srv:
	if (name->len >= MAX_DNS_NAME) {
		LOG(L_ERR, "dns_sip_resolve: domain name too long\n");
		return -E_DNS_NAME_TOO_LONG;
	}
	ret=dns_ip_resolve(&h->a, &h->ip_no, name, ip, flags);
	if (port)
		*port=h->port;
	DBG("dns_sip_resolve(%.*s, %d, %d), ip, ret=%d\n", 
			name->len, name->s, h->srv_no, h->ip_no, ret);
	return ret;
}



/* performs an a lookup and fills ip with the first good ip address
 * returns 0 on success, <0 on error (see the error codes)
 */
int dns_a_get_ip(str* name, struct ip_addr* ip)
{
	struct dns_hash_entry* e;
	int ret;
	unsigned char rr_no;
	
	e=0;
	rr_no=0;
	ret=dns_a_resolve(&e, &rr_no, name, ip);
	if (e) dns_hash_put(e);
	return ret;
}



int dns_aaaa_get_ip(str* name, struct ip_addr* ip)
{
	struct dns_hash_entry* e;
	int ret;
	unsigned char rr_no;
	
	e=0;
	rr_no=0;
	ret=dns_aaaa_resolve(&e, &rr_no, name, ip);
	if (e) dns_hash_put(e);
	return ret;
}



/* performs an a or aaaa dns lookup, returns <0 on error (see the
 *  dns error codes) and 0 on success
 *  flags:  - none set: tries first an a_lookup and if it fails an aaaa_lookup
 *          - DNS_IPV6_FIRST: tries first an aaaa_lookup and then an a_lookup
 *          - DNS_IPV4_ONLY: tries only an a_lookup
 *          - DNS_IPV6_ONLY: tries only an aaaa_lookup
 */
int dns_get_ip(str* name, struct ip_addr* ip, int flags)
{
	int ret;
	struct dns_hash_entry* e;
	unsigned char rr_no;
	
	e=0;
	rr_no=0;
	ret=dns_ip_resolve(&e, &rr_no, name, ip, flags);
	if (e)
		dns_hash_put(e);
	return ret;
}



/* fast "inline" version, gets the first good ip:port */
int dns_srv_get_ip(str* name, struct ip_addr* ip, unsigned short* port,
						int flags)
{
	int ret;
	struct dns_srv_handle h;
	
	dns_srv_handle_init(&h);
	ret=dns_srv_resolve_ip(&h, name, ip, port, flags);
	dns_srv_handle_put(&h);
	return ret;
}



/* rpc functions */
void dns_cache_mem_info(rpc_t* rpc, void* ctx)
{
	rpc->add(ctx, "dd",  *dns_cache_mem_used, dns_cache_max_mem);
}


void dns_cache_debug(rpc_t* rpc, void* ctx)
{
	int h;
	struct dns_hash_entry* e;
	ticks_t now;
	
	now=get_ticks_raw();
	LOCK_DNS_HASH();
		for (h=0; h<DNS_HASH_SIZE; h++){
			clist_foreach(&dns_hash[h], e, next){
				rpc->add(ctx, "sdddddd", 
								e->name, e->type, e->total_size, e->refcnt.val,
								(s_ticks_t)(e->expire-now)<0?-1:
									TICKS_TO_S(e->expire-now),
								TICKS_TO_S(now-e->last_used),
								e->err_flags);
			}
		}
	UNLOCK_DNS_HASH();
}



/* rpc functions */
void dns_cache_debug_all(rpc_t* rpc, void* ctx)
{
	int h;
	struct dns_hash_entry* e;
	struct dns_rr* rr;
	struct ip_addr ip;
	int i;
	ticks_t now;
	
	now=get_ticks_raw();
	LOCK_DNS_HASH();
		for (h=0; h<DNS_HASH_SIZE; h++){
			clist_foreach(&dns_hash[h], e, next){
				for (i=0, rr=e->rr_lst; rr; i++, rr=rr->next){
					rpc->add(ctx, "sddddddd", 
								e->name, e->type, i, e->total_size,
								e->refcnt.val,
								(s_ticks_t)(e->expire-now)<0?-1:
									TICKS_TO_S(e->expire-now),
								TICKS_TO_S(now-e->last_used),
								e->err_flags);
					switch(e->type){
						case T_A:
						case T_AAAA:
							if (dns_rr2ip(e->type, rr, &ip)==0){
								rpc->add(ctx, "ss", "ip", ip_addr2a(&ip) );
							}else{
								rpc->add(ctx, "ss", "ip", "<error: bad rr>");
							}
							break;
						case T_SRV:
							rpc->add(ctx, "ss", "srv", 
									((struct srv_rdata*)(rr->rdata))->name);
							break;
						case T_NAPTR:
							rpc->add(ctx, "ss", "naptr", 
									((struct naptr_rdata*)(rr->rdata))->flags);
							break;
						case T_CNAME:
							rpc->add(ctx, "ss", "cname", 
									((struct cname_rdata*)(rr->rdata))->name);
							break;
						default:
							rpc->add(ctx, "ss", "unknown", "?");
					}
					rpc->add(ctx, "dd",
								(s_ticks_t)(rr->expire-now)<0?-1:
									TICKS_TO_S(rr->expire-now),
							rr->err_flags);
				}
			}
		}
	UNLOCK_DNS_HASH();
}


#endif

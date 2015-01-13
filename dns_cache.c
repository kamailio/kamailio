/*
 * resolver related functions
 *
 * Copyright (C) 2006 iptelorg GmbH
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
 */

/*!
 * \file
 * \brief Kamailio core :: DNS cache handling
 * \ingroup core
 * Module: \ref core
 */


#ifdef USE_DNS_CACHE

#ifdef DNS_SRV_LB
#include <stdlib.h> /* FIXME: rand() */
#endif
#include <string.h>

#include "globals.h"
#include "cfg_core.h"
#include "dns_cache.h"
#include "dns_wrappers.h"
#include "compiler_opt.h"
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
#include "rand/fastrand.h"
#ifdef USE_DNS_CACHE_STATS
#include "pt.h"
#endif



#define DNS_CACHE_DEBUG /* extra sanity checks and debugging */


#ifndef MAX
	#define MAX(a,b) ( ((a)>(b))?(a):(b))
#endif

#define MAX_DNS_RECORDS 255  /* maximum dns records number  received in a
							   dns answer*/

#define DNS_HASH_SIZE	1024 /* must be <= 65535 */
#define DEFAULT_DNS_TIMER_INTERVAL 120  /* 2 min. */
#define DNS_HE_MAX_ADDR 10  /* maxium addresses returne in a hostent struct */
#define MAX_CNAME_CHAIN  10
#define SPACE_FORMAT "    " /* format of view output */
#define DNS_SRV_ZERO_W_CHANCE	1000 /* one in a 1000*weight_sum chance for
										selecting a 0-weight record */

int dns_cache_init=1;	/* if 0, the DNS cache is not initialized at startup */
static gen_lock_t* dns_hash_lock=0;
static volatile unsigned int *dns_cache_mem_used=0; /* current mem. use */
unsigned int dns_timer_interval=DEFAULT_DNS_TIMER_INTERVAL; /* in s */
int dns_flags=0; /* default flags used for the  dns_*resolvehost
                    (compatibility wrappers) */

#ifdef USE_DNS_CACHE_STATS
struct t_dns_cache_stats* dns_cache_stats=0;
#endif

#define LOCK_DNS_HASH()		lock_get(dns_hash_lock)
#define UNLOCK_DNS_HASH()	lock_release(dns_hash_lock)

#define FIX_TTL(t) \
	(((t)<cfg_get(core, core_cfg, dns_cache_min_ttl))? \
		cfg_get(core, core_cfg, dns_cache_min_ttl): \
		(((t)>cfg_get(core, core_cfg, dns_cache_max_ttl))? \
			cfg_get(core, core_cfg, dns_cache_max_ttl): \
			(t)))


struct dns_hash_head{
	struct dns_hash_entry* next;
	struct dns_hash_entry* prev;
};

#ifdef DNS_LU_LST
struct dns_lu_lst* dns_last_used_lst=0;
#endif

static struct dns_hash_head* dns_hash=0;


static struct timer_ln* dns_timer_h=0;

#ifdef DNS_WATCHDOG_SUPPORT
static atomic_t *dns_servers_up = NULL;
#endif



static const char* dns_str_errors[]={
	"no error",
	"no more records", /* not an error, but and end condition */
	"unknown error",
	"internal error",
	"bad SRV entry",
	"unresolvable SRV request",
	"bad A or AAAA entry",
	"unresolvable A or AAAA request",
	"invalid ip in A or AAAA record",
	"blacklisted ip",
	"name too long ", /* try again with a shorter name */
	"ip AF mismatch", /* address family mismatch */
	"unresolvable NAPTR request",
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
#ifdef DNS_WATCHDOG_SUPPORT
	/* do not clean the hash table if the servers are down */
	if (atomic_get(dns_servers_up) == 0)
		return (ticks_t)(-1);
#endif
	if (*dns_cache_mem_used>12*(cfg_get(core, core_cfg, dns_cache_max_mem)/16)){ /* ~ 75% used */
		dns_cache_free_mem(cfg_get(core, core_cfg, dns_cache_max_mem)/2, 1);
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
#ifdef DNS_WATCHDOG_SUPPORT
	if (dns_servers_up){
		shm_free(dns_servers_up);
		dns_servers_up=0;
	}
#endif
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
#ifdef USE_DNS_CACHE_STATS
	if (dns_cache_stats)
		shm_free(dns_cache_stats);
#endif
	if (dns_cache_mem_used){
		shm_free((void*)dns_cache_mem_used);
		dns_cache_mem_used=0;
	}
}

/* set the value of dns_flags */
void fix_dns_flags(str *gname, str *name)
{
	/* restore the original value of dns_cache_flags first
	 * (DNS_IPV4_ONLY may have been set only because dns_try_ipv6
	 * was disabled, and the flag must be cleared when
	 * dns_try_ipv6 is enabled) (Miklos)
	 */
	dns_flags = cfg_get(core, core_cfg, dns_cache_flags) & 7;

	if (cfg_get(core, core_cfg, dns_try_ipv6)==0){
		dns_flags|=DNS_IPV4_ONLY;
	}
	if (dns_flags & DNS_IPV4_ONLY){
		dns_flags&=~(DNS_IPV6_ONLY|DNS_IPV6_FIRST);
	}
	if (cfg_get(core, core_cfg, dns_srv_lb)){
#ifdef DNS_SRV_LB
		dns_flags|=DNS_SRV_RR_LB;
#else
		LM_WARN("SRV loadbalaning is set, but"
					" support for it is not compiled -- ignoring\n");
#endif
	}
	if (cfg_get(core, core_cfg, dns_try_naptr)) {
#ifndef USE_NAPTR
	LM_WARN("NAPTR support is enabled, but"
				" support for it is not compiled -- ignoring\n");
#endif
		dns_flags|=DNS_TRY_NAPTR;
	}
}

/* fixup function for use_dns_failover
 * verifies that use_dns_cache is set to 1
 */
int use_dns_failover_fixup(void *handle, str *gname, str *name, void **val)
{
	if ((int)(long)(*val) && !cfg_get(core, handle, use_dns_cache)) {
		LM_ERR("DNS cache is turned off, failover cannot be enabled. "
			"(set use_dns_cache to 1)\n");
		return -1;
	}
	return 0;
}

/* fixup function for use_dns_cache
 * verifies that dns_cache_init is set to 1
 */
int use_dns_cache_fixup(void *handle, str *gname, str *name, void **val)
{
	if ((int)(long)(*val) && !dns_cache_init) {
		LM_ERR("DNS cache is turned off by dns_cache_init=0, "
			"it cannot be enabled runtime.\n");
		return -1;
	}
	if (((int)(long)(*val)==0) && cfg_get(core, handle, use_dns_failover)) {
		LM_ERR("DNS failover depends on use_dns_cache, set use_dns_failover "
			"to 0 before disabling the DNS cache\n");
		return -1;
	}
	return 0;
}

/* KByte to Byte conversion */
int dns_cache_max_mem_fixup(void *handle, str *gname, str *name, void **val)
{
	unsigned int    u;

	u = ((unsigned int)(long)(*val))<<10;
	(*val) = (void *)(long)u;
	return 0;
}

int init_dns_cache()
{
	int r;
	int ret;

	if (dns_cache_init==0) {
		/* the DNS cache is turned off */
		default_core_cfg.use_dns_cache=0;
		default_core_cfg.use_dns_failover=0;
		return 0;
	}

	ret=0;
	/* sanity check */
	if (E_DNS_CRITICAL>=sizeof(dns_str_errors)/sizeof(char*)){
		LM_CRIT("bad dns error table\n");
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

#ifdef DNS_WATCHDOG_SUPPORT
	dns_servers_up=shm_malloc(sizeof(atomic_t));
	if (dns_servers_up==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
	atomic_set(dns_servers_up, 1);
#endif

	/* fix options */
	default_core_cfg.dns_cache_max_mem<<=10; /* Kb */ /* TODO: test with 0 */
	if (default_core_cfg.use_dns_cache==0)
		default_core_cfg.use_dns_failover=0; /* cannot work w/o dns_cache support */
	/* fix flags */
	fix_dns_flags(NULL, NULL);

	dns_timer_h=timer_alloc();
	if (dns_timer_h==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
	if (dns_timer_interval){
		timer_init(dns_timer_h, dns_timer, 0, 0); /* "slow" timer */
		if (timer_add(dns_timer_h, S_TO_TICKS(dns_timer_interval))<0){
			LM_CRIT("failed to add the timer\n");
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

#ifdef USE_DNS_CACHE_STATS
int init_dns_cache_stats(int iproc_num)
{
	/* do not initialize the stats array if the DNS cache will not be used */
	if (dns_cache_init==0) return 0;

	/* if it is already initialized */
	if (dns_cache_stats)
		shm_free(dns_cache_stats);

	dns_cache_stats=shm_malloc(sizeof(*dns_cache_stats) * iproc_num);
	if (dns_cache_stats==0){
		return E_OUT_OF_MEM;
	}
	memset(dns_cache_stats, 0, sizeof(*dns_cache_stats) * iproc_num);

	return 0;
}
#endif

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
		LM_CRIT("%s: crt(%p, %p, %p)," \
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
inline static void _dns_hash_remove(struct dns_hash_entry* e)
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
#ifdef DNS_WATCHDOG_SUPPORT
	int servers_up;

	servers_up = atomic_get(dns_servers_up);
#endif

	cname_chain=0;
	ret=0;
	now=get_ticks_raw();
	*err=0;
again:
	*h=dns_hash_no(name->s, name->len, type);
#ifdef DNS_CACHE_DEBUG
	LM_DBG("(%.*s(%d), %d), h=%d\n", name->len, name->s, name->len, type, *h);
#endif
	clist_foreach_safe(&dns_hash[*h], e, tmp, next){
		if (
#ifdef DNS_WATCHDOG_SUPPORT
			/* remove expired elements only when the dns servers are up */
			servers_up &&
#endif
			/* automatically remove expired elements */
			((e->ent_flags & DNS_FLAG_PERMANENT) == 0) &&
			((s_ticks_t)(now-e->expire)>=0)
		) {
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
		}else if ((e->type==T_CNAME) &&
					!((e->rr_lst==0) || (e->ent_flags & DNS_FLAG_BAD_NAME)) &&
					(e->name_len==name->len) &&
					(strncasecmp(e->name, name->s, e->name_len)==0)){
			/*if CNAME matches and CNAME is entry is not a neg. cache entry
			  (could be produced by a specific CNAME lookup)*/
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
				LM_ERR("cname chain too long or recursive (\"%.*s\")\n",
						name->len, name->s);
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
		if (((e->ent_flags & DNS_FLAG_PERMANENT) == 0)
			&& (!expired_only || ((s_ticks_t)(now-e->expire)>=0))
		) {
				_dns_hash_remove(e);
				deleted++;
		}
		n++;
		if (n>=no) break;
	}
#else
	for(h=start; h!=(start+DNS_HASH_SIZE); h++){
		clist_foreach_safe(&dns_hash[h%DNS_HASH_SIZE], e, t, next){
			if (((e->ent_flags & DNS_FLAG_PERMANENT) == 0)
				&& ((s_ticks_t)(now-e->expire)>=0)
			) {
				_dns_hash_remove(e);
				deleted++;
			}
			n++;
			if (n>=no) goto skip;
		}
	}
	/* not fair, but faster then random() */
	if (!expired_only){
		for(h=start; h!=(start+DNS_HASH_SIZE); h++){
			clist_foreach_safe(&dns_hash[h%DNS_HASH_SIZE], e, t, next){
				if ((e->ent_flags & DNS_FLAG_PERMANENT) == 0) {
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
		if (((e->ent_flags & DNS_FLAG_PERMANENT) == 0)
			&& (!expired_only || ((s_ticks_t)(now-e->expire)>=0))
		) {
				_dns_hash_remove(e);
				deleted++;
		}
	}
#else
	for(h=start; h!=(start+DNS_HASH_SIZE); h++){
		clist_foreach_safe(&dns_hash[h%DNS_HASH_SIZE], e, t, next){
			if (*dns_cache_mem_used<=target)
				goto skip;
			if (((e->ent_flags & DNS_FLAG_PERMANENT) == 0)
				&& ((s_ticks_t)(now-e->expire)>=0)
			) {
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
				if (((e->ent_flags & DNS_FLAG_PERMANENT) == 0)
					&& ((s_ticks_t)(now-e->expire)>=0)
				) {
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
	if ((*dns_cache_mem_used+e->total_size)>=cfg_get(core, core_cfg, dns_cache_max_mem)){
#ifdef USE_DNS_CACHE_STATS
		dns_cache_stats[process_no].dc_lru_cnt++;
#endif
		LM_WARN("cache full, trying to free...\n");
		/* free ~ 12% of the cache */
		dns_cache_free_mem(*dns_cache_mem_used/16*14,
					!cfg_get(core, core_cfg, dns_cache_del_nonexp));
		if ((*dns_cache_mem_used+e->total_size)>=cfg_get(core, core_cfg, dns_cache_max_mem)){
			LM_ERR("max. cache mem size exceeded\n");
			return -1;
		}
	}
	atomic_inc(&e->refcnt);
	h=dns_hash_no(e->name, e->name_len, e->type);
#ifdef DNS_CACHE_DEBUG
	LM_DBG("adding %.*s(%d) %d (flags=%0x) at %d\n",
			e->name_len, e->name, e->name_len, e->type, e->ent_flags, h);
#endif
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
	if ((*dns_cache_mem_used+e->total_size)>=cfg_get(core, core_cfg, dns_cache_max_mem)){
#ifdef USE_DNS_CACHE_STATS
		dns_cache_stats[process_no].dc_lru_cnt++;
#endif
		LM_WARN("cache full, trying to free...\n");
		/* free ~ 12% of the cache */
		UNLOCK_DNS_HASH();
		dns_cache_free_mem(*dns_cache_mem_used/16*14,
					!cfg_get(core, core_cfg, dns_cache_del_nonexp));
		LOCK_DNS_HASH();
		if ((*dns_cache_mem_used+e->total_size)>=cfg_get(core, core_cfg, dns_cache_max_mem)){
			LM_ERR("max. cache mem size exceeded\n");
			return -1;
		}
	}
	atomic_inc(&e->refcnt);
	h=dns_hash_no(e->name, e->name_len, e->type);
#ifdef DNS_CACHE_DEBUG
	LM_DBG("adding %.*s(%d) %d (flags=%0x) at %d\n",
			e->name_len, e->name, e->name_len, e->type, e->ent_flags, h);
#endif
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

#ifdef DNS_CACHE_DEBUG
	LM_DBG("(%.*s, %d, %d, %d)\n", name->len, name->s, type, ttl, flags);
#endif
	size=sizeof(struct dns_hash_entry)+name->len-1+1;
	e=shm_malloc(size);
	if (e==0){
		LM_ERR("out of memory\n");
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
	e->ent_flags=flags;
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
		LM_ERR("out of memory\n");
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

/* creates an srv hash entry from the given parameters
 * returns 0 on error */
static struct dns_hash_entry* dns_cache_mk_srv_entry(str* name,
							unsigned short priority,
							unsigned short weight,
							unsigned short port,
							str* rr_name,
							int ttl)
{
	struct dns_hash_entry* e;
	int size;
	ticks_t now;

	/* everything is allocated in one block: dns_hash_entry + name +
	 * + dns_rr + rdata;  dns_rr must start at an aligned adress,
	 * hence we need to round dns_hash_entry+name size to a sizeof(long),
	 * and similarly, dns_rr must be rounded to sizeof(short).
	 * multiple.
	 * Memory image:
	 * struct dns_hash_entry
	 * name (name_len+1 bytes)
	 * padding to multiple of sizeof(long)
	 * dns_rr
	 * padding to multiple of sizeof(short)
	 * rdata
	  */
	size=ROUND_POINTER(sizeof(struct dns_hash_entry)+name->len-1+1) +
		ROUND_SHORT(sizeof(struct dns_rr)) +
		sizeof(struct srv_rdata)-1 +
		rr_name->len+1;

	e=shm_malloc(size);
	if (e==0){
		LM_ERR("out of memory\n");
		return 0;
	}
	memset(e, 0, size); /* init with 0*/
	e->total_size=size;
	e->name_len=name->len;
	e->type=T_SRV;
	now=get_ticks_raw();
	e->last_used=now;
	e->expire=now+S_TO_TICKS(ttl);
	memcpy(e->name, name->s, name->len); /* memset makes sure is 0-term. */
	e->rr_lst=(void*)((char*)e+
				ROUND_POINTER(sizeof(struct dns_hash_entry)+name->len-1+1));
	e->rr_lst->rdata=(void*)((char*)e->rr_lst+ROUND_SHORT(sizeof(struct dns_rr)));
	e->rr_lst->expire=e->expire;
	((struct srv_rdata*)e->rr_lst->rdata)->priority = priority;
	((struct srv_rdata*)e->rr_lst->rdata)->weight = weight;
	((struct srv_rdata*)e->rr_lst->rdata)->port = port;
	((struct srv_rdata*)e->rr_lst->rdata)->name_len = rr_name->len;
	memcpy(((struct srv_rdata*)e->rr_lst->rdata)->name, rr_name->s, rr_name->len);
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
	int i;

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
		case T_TXT:
			for(; *p;){
				if (!rec_matches((*p), type, name)){
					/* skip this record */
					p=&(*p)->next; /* advance */
					continue;
				}
				/* padding to char* (because of txt[]->cstr*/
				size+=ROUND_POINTER(ROUND_POINTER(sizeof(struct dns_rr))+
						TXT_RDATA_SIZE(*(struct txt_rdata*)(*p)->rdata));
				/* add it to our tmp. lst */
				*tail=*p;
				tail=&(*p)->next;
				/* detach it from the rd list */
				*p=(*p)->next;
				/* don't advance p, because the crt. elem. has
				 * just been elimintated */
			}
			break;
		case T_EBL:
			for(; *p;){
				if (!rec_matches((*p), type, name)){
					/* skip this record */
					p=&(*p)->next; /* advance */
					continue;
				}
				/* padding to char* (because of the char* pointers */
				size+=ROUND_POINTER(ROUND_POINTER(sizeof(struct dns_rr))+
						EBL_RDATA_SIZE(*(struct ebl_rdata*)(*p)->rdata));
				/* add it to our tmp. lst */
				*tail=*p;
				tail=&(*p)->next;
				/* detach it from the rd list */
				*p=(*p)->next;
				/* don't advance p, because the crt. elem. has
				 * just been elimintated */
			}
			break;
		case T_PTR:
			for(; *p;){
				if (!rec_matches((*p), type, name)){
					/* skip this record */
					p=&(*p)->next; /* advance */
					continue;
				}
				/* no padding */
				size+=ROUND_POINTER(sizeof(struct dns_rr)+
						PTR_RDATA_SIZE(*(struct ptr_rdata*)(*p)->rdata));
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
			LM_CRIT("type %d not supported\n", type);
			/* we don't know what to do with it, so don't
			 * add it to the tmp_lst */
			return 0; /* error */
	}
	*tail=0; /* mark the end of our tmp_lst */
	if (size==0){
#ifdef DNS_CACHE_DEBUG
		LM_DBG("entry %.*s (%d) not found\n", name->len, name->s, type);
#endif
		return 0;
	}
	/* compute size */
	size+=ROUND_POINTER(sizeof(struct dns_hash_entry)+name->len-1+1);
	e=shm_malloc(size);
	if (e==0){
		LM_ERR("out of memory\n");
		return 0;
	}
	memset(e, 0, size); /* init with 0 */
	clist_init(e, next, prev);
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
				/* copy the whole naptr_rdata block*/
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
				rr=rr->next;
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
		case T_TXT:
			for(l=tmp_lst; l; l=l->next){
				ttl=FIX_TTL(l->ttl);
				rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				max_ttl=MAX(max_ttl, ttl);
				rr->rdata=(void*)((char*)rr+
							ROUND_POINTER(sizeof(struct dns_rr)));
				memcpy(rr->rdata, l->rdata,
							TXT_RDATA_SIZE(*(struct txt_rdata*)l->rdata));
				/* adjust the string pointers */
				for (i=0; i<((struct txt_rdata*)l->rdata)->cstr_no; i++){
					((struct txt_rdata*)rr->rdata)->txt[i].cstr=
						translate_pointer((char*)rr->rdata, (char*)l->rdata,
								((struct txt_rdata*)l->rdata)->txt[i].cstr);
				}
				rr->next=(void*)((char*)rr+
						ROUND_POINTER(ROUND_POINTER(sizeof(struct dns_rr))+
							TXT_RDATA_SIZE(*(struct txt_rdata*)l->rdata)));
				tail_rr=&(rr->next);
				rr=rr->next;
			}
			break;
		case T_EBL:
			for(l=tmp_lst; l; l=l->next){
				ttl=FIX_TTL(l->ttl);
				rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				max_ttl=MAX(max_ttl, ttl);
				rr->rdata=(void*)((char*)rr+
							ROUND_POINTER(sizeof(struct dns_rr)));
				memcpy(rr->rdata, l->rdata,
							EBL_RDATA_SIZE(*(struct ebl_rdata*)l->rdata));
				/* adjust the string pointers */
				((struct ebl_rdata*)rr->rdata)->separator=
					translate_pointer((char*)rr->rdata, (char*)l->rdata,
								((struct ebl_rdata*)l->rdata)->separator);
				((struct ebl_rdata*)rr->rdata)->separator=
						translate_pointer((char*)rr->rdata, (char*)l->rdata,
								((struct ebl_rdata*)l->rdata)->separator);
				((struct ebl_rdata*)rr->rdata)->apex=
						translate_pointer((char*)rr->rdata, (char*)l->rdata,
								((struct ebl_rdata*)l->rdata)->apex);
				rr->next=(void*)((char*)rr+
						ROUND_POINTER(ROUND_POINTER(sizeof(struct dns_rr))+
							EBL_RDATA_SIZE(*(struct ebl_rdata*)l->rdata)));
				tail_rr=&(rr->next);
				rr=rr->next;
			}
			break;
		case T_PTR:
			for(l=tmp_lst; l; l=l->next){
				ttl=FIX_TTL(l->ttl);
				rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				max_ttl=MAX(max_ttl, ttl);
				rr->rdata=(void*)((char*)rr+sizeof(struct dns_rr));
				memcpy(rr->rdata, l->rdata,
							PTR_RDATA_SIZE(*(struct ptr_rdata*)l->rdata));
				rr->next=(void*)((char*)rr+ROUND_POINTER(sizeof(struct dns_rr)+
							PTR_RDATA_SIZE(*(struct ptr_rdata*)l->rdata)));
				tail_rr=&(rr->next);
				rr=rr->next;
			}
			break;
		default:
			/* do nothing */
			LM_CRIT("type %d not supported\n", type);
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
	int r, i, j;
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
			LM_ERR("too many records: %d\n", no_records);
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
			case T_TXT:
					/* padding to char* (because of txt[]->cstr)*/
				rec[r].size+=ROUND_POINTER(ROUND_POINTER(
												sizeof(struct dns_rr))+
							TXT_RDATA_SIZE(*(struct txt_rdata*)l->rdata));
				break;
			case T_EBL:
					/* padding to char* (because of char* pointers)*/
				rec[r].size+=ROUND_POINTER(ROUND_POINTER(
												sizeof(struct dns_rr))+
							EBL_RDATA_SIZE(*(struct ebl_rdata*)l->rdata));
				break;
			case T_PTR:
					/* no padding */
				rec[r].size+=ROUND_POINTER(sizeof(struct dns_rr)+
							PTR_RDATA_SIZE(*(struct ptr_rdata*)l->rdata));
				break;
			default:
				LM_CRIT("type %d not supported\n", l->type);
		}
	}

	now=get_ticks_raw();
	/* alloc & init the entries */
	for (r=0; r<no_records; r++){
		rec[r].e=shm_malloc(rec[r].size);
		if (rec[r].e==0){
			LM_ERR("out of memory\n");
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
				rec[r].rr=rec[r].rr->next;
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
			case T_TXT:
				rec[r].rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				rec[r].max_ttl=MAX(rec[r].max_ttl, ttl);
				rec[r].rr->rdata=(void*)((char*)rec[r].rr+
									ROUND_POINTER(sizeof(struct dns_rr)));
				memcpy(rec[r].rr->rdata, l->rdata,
							TXT_RDATA_SIZE(*(struct txt_rdata*)l->rdata));
				/* adjust the string pointers */
				for (j=0; j<((struct txt_rdata*)l->rdata)->cstr_no; j++){
					((struct txt_rdata*)rec[r].rr->rdata)->txt[j].cstr=
						translate_pointer((char*)rec[r].rr->rdata,
								(char*)l->rdata,
								((struct txt_rdata*)l->rdata)->txt[j].cstr);
				}
				rec[r].rr->next=(void*)((char*)rec[r].rr+
						ROUND_POINTER(ROUND_POINTER(sizeof(struct dns_rr))+
							TXT_RDATA_SIZE(*(struct txt_rdata*)l->rdata)));
				rec[r].tail_rr=&(rec[r].rr->next);
				rec[r].rr=rec[r].rr->next;
				break;
			case T_EBL:
				rec[r].rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				rec[r].max_ttl=MAX(rec[r].max_ttl, ttl);
				rec[r].rr->rdata=(void*)((char*)rec[r].rr+
									ROUND_POINTER(sizeof(struct dns_rr)));
				memcpy(rec[r].rr->rdata, l->rdata,
							EBL_RDATA_SIZE(*(struct ebl_rdata*)l->rdata));
				/* adjust the string pointers */
				((struct ebl_rdata*)rec[r].rr->rdata)->separator=
					translate_pointer((char*)rec[r].rr->rdata,
							(char*)l->rdata,
							((struct ebl_rdata*)l->rdata)->separator);
				((struct ebl_rdata*)rec[r].rr->rdata)->apex=
					translate_pointer((char*)rec[r].rr->rdata,
							(char*)l->rdata,
							((struct ebl_rdata*)l->rdata)->apex);
				rec[r].rr->next=(void*)((char*)rec[r].rr+
						ROUND_POINTER(ROUND_POINTER(sizeof(struct dns_rr))+
							EBL_RDATA_SIZE(*(struct ebl_rdata*)l->rdata)));
				rec[r].tail_rr=&(rec[r].rr->next);
				rec[r].rr=rec[r].rr->next;
				break;
			case T_PTR:
				rec[r].rr->expire=now+S_TO_TICKS(ttl); /* maximum expire */
				rec[r].max_ttl=MAX(rec[r].max_ttl, ttl);
				rec[r].rr->rdata=(void*)((char*)rec[r].rr
									+sizeof(struct dns_rr));
				memcpy(rec[r].rr->rdata, l->rdata,
							PTR_RDATA_SIZE(*(struct ptr_rdata*)l->rdata));
				rec[r].rr->next=(void*)((char*)rec[r].rr+
							ROUND_POINTER(sizeof(struct dns_rr)+
							PTR_RDATA_SIZE(*(struct ptr_rdata*)l->rdata)));
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
#ifdef DNS_CACHE_DEBUG
	LM_DBG("(%p (%.*s, %d), %d, *%p) (%d)\n", e,
			e->name_len, e->name, e->type, type, *records, cname_chain_len);
#endif
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
							lst_end=t->prev; /* needed for clist_append*/
							clist_append_sublist(l, t, lst_end, next, prev);
						}
					}
					if (!(dns_flags&DNS_IPV4_ONLY)){
						t=dns_cache_mk_rd_entry(&tmp, T_AAAA, records);
						if (t){
							if ((t->type==T_CNAME) && *records)
								dns_get_related(t, T_AAAA, records);
							lst_end=t->prev; /* needed for clist_append*/
							clist_append_sublist(l, t, lst_end, next, prev);
						}
					}
				}
				break;
#ifdef USE_NAPTR
			case T_NAPTR:
#ifdef NAPTR_CACHE_ALL_ARS
				if (*records)
						dns_cache_mk_rd_entry2(*records);
#else
				for (rr=e->rr_lst; rr && *records; rr=rr->next){
					if (naptr_get_sip_proto((struct naptr_rdata*)rr->rdata)>0){
						tmp.s=((struct naptr_rdata*)rr->rdata)->repl;
						tmp.len=((struct naptr_rdata*)rr->rdata)->repl_len;
						t=dns_cache_mk_rd_entry(&tmp, T_SRV, records);
						if (t){
							if (*records)
								dns_get_related(t, T_SRV, records);
							lst_end=t->prev; /* needed for clist_append*/
							clist_append_sublist(l, t, lst_end, next, prev);
						}
					}
				}
#endif /* NAPTR_CACHE_ALL_ARS */
#endif /* USE_NAPTR */
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
	struct dns_hash_entry* old;
	str rec_name;
	int add_record, h, err;

	e=0;
	l=0;
	cname_val.s=0;
	old = NULL;

#ifdef USE_DNS_CACHE_STATS
	if (dns_cache_stats)
		dns_cache_stats[process_no].dns_req_cnt++;
#endif /* USE_DNS_CACHE_STATS */

	if (type==T_A){
		if (str2ip6(name)!=0)
			goto end;
		if ((ip=str2ip(name))!=0){
				e=dns_cache_mk_ip_entry(name, ip);
				if (likely(e))
					atomic_set(&e->refcnt, 1);/* because we ret. a ref. to it*/
				goto end; /* we do not cache obvious stuff */
		}
	}
	else if (type==T_AAAA){
		if (str2ip(name)!=0)
			goto end;
		if ((ip=str2ip6(name))!=0){
				e=dns_cache_mk_ip_entry(name, ip);
				if (likely(e))
					atomic_set(&e->refcnt, 1);/* because we ret. a ref. to it*/
				goto end;/* we do not cache obvious stuff */
		}
	}
#ifdef DNS_WATCHDOG_SUPPORT
	if (atomic_get(dns_servers_up)==0)
		goto end; /* the servers are down, needless to perform the query */
#endif
	if (name->len>=MAX_DNS_NAME){
		LM_ERR("name too long (%d chars)\n", name->len);
		goto end;
	}
	/* null terminate the string, needed by get_record */
	memcpy(name_buf, name->s, name->len);
	name_buf[name->len]=0;
	records=get_record(name_buf, type, RES_AR);
	if (records){
#ifdef CACHE_RELEVANT_RECS_ONLY
		e=dns_cache_mk_rd_entry(name, type, &records);
		if (likely(e)){
			l=e;
			e=dns_get_related(l, type, &records);
			/* e should contain the searched entry (if found) and l
			 * all the entries (e and related) */
			if (likely(e)){
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
				LM_DBG("cname detected: %.*s (%d)\n",
						cname_val.len, cname_val.s, cname_val.len);
			}
			/* add all the records to the hash */
			l->prev->next=0; /* we break the double linked list for easier
								searching */
			LOCK_DNS_HASH(); /* optimization */
			for (r=l; r; r=t){
				t=r->next;
				/* add the new record to the cache by default */
				add_record = 1;
				if (cfg_get(core, core_cfg, dns_cache_rec_pref) > 0) {
					/* check whether there is an old record with the
					 * same type in the cache */
					rec_name.s = r->name;
					rec_name.len = r->name_len;
					old = _dns_hash_find(&rec_name, r->type, &h, &err);
					if (old) {
						if (old->type != r->type) {
							/* probably CNAME found */
							old = NULL;

						} else if (old->ent_flags & DNS_FLAG_PERMANENT) {
							/* never overwrite permanent entries */
							add_record = 0;

						} else if ((old->ent_flags & DNS_FLAG_BAD_NAME) == 0) {
							/* Non-negative, non-permanent entry found with
							 * the same type. */
							add_record =
								/* prefer new records */
								((cfg_get(core, core_cfg, dns_cache_rec_pref) == 2)
								/* prefer the record with the longer lifetime */
								|| ((cfg_get(core, core_cfg, dns_cache_rec_pref) == 3)
									&& TICKS_LT(old->expire, r->expire)));
						}
					}
				}
				if (add_record) {
					dns_cache_add_unsafe(r); /* refcnt++ inside */
					if (atomic_get(&r->refcnt)==0){
						/* if cache adding failed and nobody else is interested
						 * destroy this entry */
						dns_destroy_entry(r);
					}
					if (old) {
						_dns_hash_remove(old);
						old = NULL;
					}
				} else {
					if (old) {
						if (r == e) {
							/* this entry has to be returned */
							e = old;
							atomic_inc(&e->refcnt);
						}
						old = NULL;
					}
					dns_destroy_entry(r);
				}
			}
			UNLOCK_DNS_HASH();
			/* if only cnames found => try to resolve the last one */
			if (cname_val.s){
				LM_DBG("dns_get_entry(cname: %.*s (%d))\n",
						cname_val.len, cname_val.s, cname_val.len);
				e=dns_get_entry(&cname_val, type);
			}
		}
#else
		l=dns_cache_mk_rd_entry2(records);
#endif
		free_rdata_list(records);
	}else if (cfg_get(core, core_cfg, dns_neg_cache_ttl)){
		e=dns_cache_mk_bad_entry(name, type, 
				cfg_get(core, core_cfg, dns_neg_cache_ttl), DNS_FLAG_BAD_NAME);
		if (likely(e)) {
			atomic_set(&e->refcnt, 1); /* 1 because we return a ref. to it */
			dns_cache_add(e); /* refcnt++ inside*/
		}
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

			/* add the new record to the cache by default */
			add_record = 1;
			if (cfg_get(core, core_cfg, dns_cache_rec_pref) > 0) {
				/* check whether there is an old record with the
				 * same type in the cache */
				rec_name.s = r->name;
				rec_name.len = r->name_len;
				old = _dns_hash_find(&rec_name, r->type, &h, &err);
				if (old) {
					if (old->type != r->type) {
						/* probably CNAME found */
						old = NULL;

					} else if (old->ent_flags & DNS_FLAG_PERMANENT) {
						/* never overwrite permanent entries */
						add_record = 0;

					} else if ((old->ent_flags & DNS_FLAG_BAD_NAME) == 0) {
						/* Non-negative, non-permanent entry found with
						 * the same type. */
						add_record =
							/* prefer new records */
							((cfg_get(core, core_cfg, dns_cache_rec_pref) == 2)
							/* prefer the record with the longer lifetime */
							|| ((cfg_get(core, core_cfg, dns_cache_rec_pref) == 3)
								&& TICKS_LT(old->expire, r->expire)));
					}
				}
			}
			if (add_record) {
				dns_cache_add_unsafe(r); /* refcnt++ inside */
				if (atomic_get(&r->refcnt)==0){
					/* if cache adding failed and nobody else is interested
					 * destroy this entry */
					dns_destroy_entry(r);
				}
				if (old) {
					_dns_hash_remove(old);
					old = NULL;
				}
			} else {
				if (old) {
					if (r == e) {
						/* this entry has to be returned */
						e = old;
						atomic_inc(&e->refcnt);
					}
					old = NULL;
				}
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
 *  WARNING: when not needed anymore dns_hash_put() must be called! */
inline static struct dns_hash_entry* dns_get_entry(str* name, int type)
{
	int h;
	struct dns_hash_entry* e;
	str cname_val;
	int err;
	static int rec_cnt=0; /* recursion protection */

	e=0;
	if (rec_cnt>MAX_CNAME_CHAIN){
		LM_WARN("CNAME chain too long or recursive CNAMEs (\"%.*s\")\n",
				name->len, name->s);
		goto error;
	}
	rec_cnt++;
	e=dns_hash_get(name, type, &h, &err);
#ifdef USE_DNS_CACHE_STATS
	if (e) {
		if ((e->ent_flags & DNS_FLAG_BAD_NAME) && dns_cache_stats)
			/* negative DNS cache hit */
			dns_cache_stats[process_no].dc_neg_hits_cnt++;
		else if (((e->ent_flags & DNS_FLAG_BAD_NAME) == 0)
				&& dns_cache_stats
		) /* DNS cache hit */
			dns_cache_stats[process_no].dc_hits_cnt++;

		if (dns_cache_stats)
			dns_cache_stats[process_no].dns_req_cnt++;
	}
#endif /* USE_DNS_CACHE_STATS */

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
	if ((e->rr_lst==0) || (e->ent_flags & DNS_FLAG_BAD_NAME)){
		/* negative cache => not resolvable */
		dns_hash_put(e);
		e=0;
	}
error:
	rec_cnt--;
	return e;
}



/* gets the first non-expired record starting with record no
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
 *       LM_DBG("address %d\n", *no);
 *       *no++;  ( get the next address next time )
 *     }
 *  }
 */
inline static struct dns_rr* dns_entry_get_rr(	struct dns_hash_entry* e,
											 unsigned char* no, ticks_t now)
{
	struct dns_rr* rr;
	int n;
#ifdef DNS_WATCHDOG_SUPPORT
	int servers_up;

	servers_up = atomic_get(dns_servers_up);
#endif

	for(rr=e->rr_lst, n=0;rr && (n<*no);rr=rr->next, n++);/* skip *no records*/
	for(;rr;rr=rr->next){
		if (
#ifdef DNS_WATCHDOG_SUPPORT
			/* check the expiration time only when the servers are up */
			servers_up &&
#endif
			((e->ent_flags & DNS_FLAG_PERMANENT) == 0) &&
			((s_ticks_t)(now-rr->expire)>=0) /* expired rr */
		)
			continue;
		/* everything is ok now */
		*no=n;
		return rr;
	}
	*no=n;
	return 0;
}


#ifdef DNS_SRV_LB

#define srv_reset_tried(p)	(*(p)=0)
#define srv_marked(p, i)	(*(p)&(1UL<<(i)))
#define srv_mark_tried(p, i)	\
	do{ \
		(*(p)|=(1UL<<(i))); \
	}while(0)

#define srv_next_rr(n, f, i) srv_mark_tried(f, i)

/* returns a random number between 0 and max inclusive (0<=r<=max) */
inline static unsigned dns_srv_random(unsigned max)
{
	return fastrand_max(max);
}

/* for a SRV record it will return the next entry to be tried according
 * to the RFC2782 server selection mechanism
 * params:
 *    e     is a dns srv hash entry
 *    no    is the start index of the current group (a group is a set of SRV
 *          rrs with the same priority)
 *    tried is a bitmap where the tried srv rrs of the same priority are
 *          marked
 *    now - current time/ticks value
 * returns pointer to the rr on success and sets no to the rr number
 *         0 on error and fills the error flags
 * WARNING: unlike dns_entry_get_rr() this will always return another
 *           another rr automatically (*no must not be incremented)
 *
 * Example usage:
 * list all non-expired, non-bad-marked, never tried before srv records
 * using the rfc2782 algo:
 * e=dns_get_entry(name, T_SRV);
 * if (e){
 *    no=0;
 *    srv_reset_tried(&tried);
 *    now=get_ticks_raw();
 *    while(rr=dns_srv_get_nxt_rr(e, &tried, &no, now){
 *       LM_DBG("address %d\n", *no);
 *     }
 *  }
 *
 */
inline static struct dns_rr* dns_srv_get_nxt_rr(struct dns_hash_entry* e,
											 srv_flags_t* tried,
											 unsigned char* no, ticks_t now)
{
#define MAX_SRV_GRP_IDX		(sizeof(srv_flags_t)*8)
	struct dns_rr* rr;
	struct dns_rr* start_grp;
	int n;
	unsigned sum;
	unsigned prio;
	unsigned rand_w;
	int found;
	int saved_idx;
	int zero_weight; /* number of records with 0 weight */
	int i, idx;
	struct r_sums_entry{
			unsigned r_sum;
			struct dns_rr* rr;
			}r_sums[MAX_SRV_GRP_IDX];
#ifdef DNS_WATCHDOG_SUPPORT
	int servers_up;

	servers_up = atomic_get(dns_servers_up);
#endif

	memset(r_sums, 0, sizeof(struct r_sums_entry) * MAX_SRV_GRP_IDX);
	rand_w=0;
	for(rr=e->rr_lst, n=0;rr && (n<*no);rr=rr->next, n++);/* skip *no records*/

retry:
	if (unlikely(rr==0))
		goto no_more_rrs;
	start_grp=rr;
	prio=((struct srv_rdata*)start_grp->rdata)->priority;
	sum=0;
	saved_idx=-1;
	zero_weight = 0;
	found=0;
	for (idx=0;rr && (prio==((struct srv_rdata*)rr->rdata)->priority) &&
						(idx < MAX_SRV_GRP_IDX); idx++, rr=rr->next){
		if ((
#ifdef DNS_WATCHDOG_SUPPORT
			/* check the expiration time only when the servers are up */
			servers_up &&
#endif
			((e->ent_flags & DNS_FLAG_PERMANENT) == 0) &&
			((s_ticks_t)(now-rr->expire)>=0) /* expired entry */) ||
				(srv_marked(tried, idx)) ) /* already tried */{
			r_sums[idx].r_sum=0; /* 0 sum, to skip over it */
			r_sums[idx].rr=0;    /* debug: mark it as unused */
			continue;
		}
		/* special case, 0 weight records should be "first":
		 * remember the first rr int the "virtual" list: A 0 weight must
		 *  come first if present, else get the first one */
		if ((saved_idx==-1) || (((struct srv_rdata*)rr->rdata)->weight==0)){
			saved_idx=idx;
		}
		zero_weight += (((struct srv_rdata*)rr->rdata)->weight == 0);
		sum+=((struct srv_rdata*)rr->rdata)->weight;
		r_sums[idx].r_sum=sum;
		r_sums[idx].rr=rr;
		found++;
	}
	if (found==0){
		/* try in the next priority group */
		n+=idx; /* next group start idx, last rr */
		srv_reset_tried(tried);
		goto retry;
	}else if ((found==1) || (sum==0) ||
				(((rand_w=(dns_srv_random(sum-1)+1))==1) && zero_weight &&
					(dns_srv_random(DNS_SRV_ZERO_W_CHANCE)==0))){
		/* 1. if only one found, avoid a useless random() call
		      and select it (saved_idx will point to it).
		 * 2. if the sum of weights is 0 (all have 0 weight) or
		 * 3. rand_w==1 and there are records with 0 weight and
		 *    random(probab. of selecting a 0-weight)
		 *     immediately select a 0 weight record.
		 *  (this takes care of the 0-weight at the beginning requirement) */
		i=saved_idx; /* saved idx contains either first 0 weight or first
						valid record */
		goto found;
	}
	/* if we are here => rand_w is not 0 and we have at least 2 valid options
	 * => we can safely iterate on the whole r_sums[] whithout any other
	 * extra checks */
	for (i=0; (i<idx) && (r_sums[i].r_sum<rand_w); i++);
found:
#ifdef DNS_CACHE_DEBUG
	LM_DBG("(%p, %lx, %d, %u): selected %d/%d in grp. %d"
			" (rand_w=%d, rr=%p rd=%p p=%d w=%d rsum=%d)\n",
		e, (unsigned long)*tried, *no, now, i, idx, n, rand_w, r_sums[i].rr,
		(r_sums[i].rr)?r_sums[i].rr->rdata:0,
		(r_sums[i].rr&&r_sums[i].rr->rdata)?((struct srv_rdata*)r_sums[i].rr->rdata)->priority:0,
		(r_sums[i].rr&&r_sums[i].rr->rdata)?((struct srv_rdata*)r_sums[i].rr->rdata)->weight:0,
		r_sums[i].r_sum);
#endif
	/* i is the winner */
	*no=n; /* grp. start */
	srv_mark_tried(tried, i); /* mark it */
	return r_sums[i].rr;
no_more_rrs:
	*no=n;
	return 0;
}
#endif /* DNS_SRV_LB */



/* gethostbyname compatibility: converts a dns_hash_entry structure
 * to a statical internal hostent structure
 * returns a pointer to the internal hostent structure on success or
 *          0 on error
 */
inline static struct hostent* dns_entry2he(struct dns_hash_entry* e)
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
			LM_CRIT("wrong entry type %d for %.*s\n",
					e->type, e->name_len, e->name);
			return 0;
	}


	rr_no=0;
	now=get_ticks_raw();
	/* if the entry has already expired use the time at the end of lifetime */
	if (unlikely((s_ticks_t)(now-e->expire)>=0)) now=e->expire-1;
	rr=dns_entry_get_rr(e, &rr_no, now);
	for(i=0; rr && (i<DNS_HE_MAX_ADDR); i++,
							rr=dns_entry_get_rr(e, &rr_no, now)){
				p_addr[i]=&address[i*len];
				memcpy(p_addr[i], ((struct a_rdata*)rr->rdata)->ip, len);
	}
	if (i==0){
		LM_DBG("no good records found (%d) for %.*s (%d)\n",
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
inline static struct hostent* dns_a_get_he(str* name)
{
	struct dns_hash_entry* e;
	struct ip_addr* ip;
	struct hostent* he;

	e=0;
	if (str2ip6(name)!=0)
		return 0;
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
inline static struct hostent* dns_aaaa_get_he(str* name)
{
	struct dns_hash_entry* e;
	struct ip_addr* ip;
	struct hostent* he;

	e=0;
	if (str2ip(name)!=0)
		return 0;
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
        struct hostent* ret;
	if ((cfg_get(core, core_cfg, use_dns_cache)==0) || (dns_hash==0)){ /* not init yet */
		ret =  _resolvehost(name);
		if(unlikely(!ret)){
			/* increment dns error counter */
			if(counters_initialized())
				counter_inc(dns_cnts_h.failed_dns_req);
		}
		return ret;
	}
	host.s=name;
	host.len=strlen(name);
	return dns_get_he(&host, dns_flags);
}




#if 0
/* resolves a host name trying  NAPTR,  SRV, A & AAAA lookups, for details
 *  see dns_sip_resolve()
 *  FIXME: this version will return only the first ip
 * returns: hostent struct & *port filled with the port from the SRV record;
 *  0 on error
 */
struct hostent* dns_sip_resolvehost(str* name, unsigned short* port,
										char* proto)
{
	struct dns_srv_handle h;
	struct ip_addr ip;
	int ret;

	if ((cfg_get(core, core_cfg, use_dns_cache==0)) || (dns_hash==0)){
		/* not init or off => use normal, non-cached version */
		return _sip_resolvehost(name, port, proto);
	}
	dns_srv_handle_init(&h);
	ret=dns_sip_resolve(&h, name, &ip, port, proto, dns_flags);
	dns_srv_handle_put(&h);
	if (ret>=0)
		return ip_addr2he(name, &ip);
	return 0;
}
#endif



/* resolves a host name trying SRV lookup if *port==0 or normal A/AAAA lookup
 * if *port!=0.
 * when performing SRV lookup (*port==0) it will use proto to look for
 * tcp or udp hosts, otherwise proto is unused; if proto==0 => no SRV lookup
 * returns: hostent struct & *port filled with the port from the SRV record;
 *  0 on error
 */
struct hostent* dns_srv_sip_resolvehost(str* name, unsigned short* port,
										char* proto)
{
	struct hostent* he;
	struct ip_addr* ip;
	static char tmp[MAX_DNS_NAME]; /* tmp. buff. for SRV lookups */
	str srv_name;
	char srv_proto;

	if ((cfg_get(core, core_cfg, use_dns_cache)==0) || (dns_hash==0)){
		/* not init or off => use normal, non-cached version */
		return _sip_resolvehost(name, port, proto);
	}
	if (proto){ /* makes sure we have a protocol set*/
		if (*proto==0)
			*proto=srv_proto=PROTO_UDP; /* default */
		else
			srv_proto=*proto;
	}else{
		srv_proto=PROTO_UDP;
	}
	/* try SRV if no port specified (draft-ietf-sip-srv-06) */
	if ((port)&&(*port==0)){
		*port=(srv_proto==PROTO_TLS)?SIPS_PORT:SIP_PORT; /* just in case we
														 don't find another */
		if ((name->len+SRV_MAX_PREFIX_LEN+1)>MAX_DNS_NAME){
			LM_WARN("domain name too long (%d), unable to perform SRV lookup\n",
						name->len);
		}else{
			/* check if it's an ip address */
			if ( ((ip=str2ip(name))!=0)
				  || ((ip=str2ip6(name))!=0)
				){
				/* we are lucky, this is an ip address */
				return ip_addr2he(name,ip);
			}

			if(srv_proto==PROTO_WS || srv_proto==PROTO_WS) {
				/* no srv records for web sockets */
				return 0;
			}

			switch(srv_proto){
				case PROTO_UDP:
				case PROTO_TCP:
				case PROTO_TLS:
				case PROTO_SCTP:
					create_srv_name(srv_proto, name, tmp);
					break;
				default:
					LM_CRIT("unknown proto %d\n", (int)srv_proto);
					return 0;
			}

			srv_name.s=tmp;
			srv_name.len=strlen(tmp);
			if ((he=dns_srv_get_he(&srv_name, port, dns_flags))!=0)
				return he;
		}
	}
/*skip_srv:*/
	if (name->len >= MAX_DNS_NAME) {
		LM_ERR("domain name too long\n");
		return 0;
	}
	he=dns_get_he(name, dns_flags);
	return he;
}



#ifdef USE_NAPTR
/* iterates over a naptr rr list, returning each time a "good" naptr record
 * is found.( srv type, no regex and a supported protocol)
 * params:
 *         naptr_head - naptr dns_rr list head
 *         tried      - bitmap used to keep track of the already tried records
 *                      (no more then sizeof(tried)*8 valid records are
 *                      ever walked
 *         srv_name   - if succesfull, it will be set to the selected record
 *                      srv name (naptr repl.)
 *         proto      - if succesfull it will be set to the selected record
 *                      protocol
 * returns  0 if no more records found or a pointer to the selected record
 *  and sets  protocol and srv_name
 * WARNING: when calling first time make sure you run first
 *           naptr_iterate_init(&tried)
 */
struct naptr_rdata* dns_naptr_sip_iterate(struct dns_rr* naptr_head,
											naptr_bmp_t* tried,
											str* srv_name, char* proto)
{
	int i, idx;
	struct dns_rr* l;
	struct naptr_rdata* naptr;
	struct naptr_rdata* naptr_saved;
	char saved_proto;
	char naptr_proto;

	idx=0;
	naptr_proto=PROTO_NONE;
	naptr_saved=0;
	saved_proto=0;
	i=0;
	for(l=naptr_head; l && (i<MAX_NAPTR_RRS); l=l->next){
		naptr=(struct naptr_rdata*) l->rdata;
		if (naptr==0){
			LM_CRIT("null rdata\n");
			goto end;
		}
		/* check if valid and get proto */
		if ((naptr_proto=naptr_get_sip_proto(naptr))<=0) continue;
		if (*tried& (1<<i)){
			i++;
			continue; /* already tried */
		}
#ifdef DNS_CACHE_DEBUG
		LM_DBG("found a valid sip NAPTR rr %.*s, proto %d\n",
				naptr->repl_len, naptr->repl, (int)naptr_proto);
#endif
		if ((naptr_proto_supported(naptr_proto))){
			if (naptr_choose(&naptr_saved, &saved_proto,
								naptr, naptr_proto))
				idx=i;
			}
		i++;
	}
	if (naptr_saved){
		/* found something */
#ifdef DNS_CACHE_DEBUG
		LM_DBG("choosed NAPTR rr %.*s, proto %d tried: 0x%x\n",
			naptr_saved->repl_len, naptr_saved->repl, (int)saved_proto, *tried);
#endif
		*tried|=1<<idx;
		*proto=saved_proto;
		srv_name->s=naptr_saved->repl;
		srv_name->len=naptr_saved->repl_len;
		return naptr_saved;
	}
end:
	return 0;
}



/* resolves a host name trying NAPTR lookup if *proto==0 and *port==0, SRV
 * lookup if *port==0 or normal A/AAAA lookup
 * if *port!=0.
 * when performing SRV lookup (*port==0) it will use proto to look for
 * tcp or udp hosts; if proto==0 => no SRV lookup
 * returns: hostent struct & *port filled with the port from the SRV record;
 *  0 on error
 */
struct hostent* dns_naptr_sip_resolvehost(str* name, unsigned short* port,
										char* proto)
{
	struct hostent* he;
	struct ip_addr* tmp_ip;
	naptr_bmp_t tried_bmp;
	struct dns_hash_entry* e;
	char n_proto;
	char origproto;
	str srv_name;

	if(proto) {
		origproto=*proto;
	} else {
		origproto=PROTO_NONE;
	}
	he=0;
	if (dns_hash==0){ /* not init => use normal, non-cached version */
		LM_WARN("called before dns cache initialization\n");
		return _sip_resolvehost(name, port, proto);
	}
	if (proto && port && (*proto==0) && (*port==0)){
		*proto=PROTO_UDP; /* just in case we don't find another */
		/* check if it's an ip address */
		if ( ((tmp_ip=str2ip(name))!=0)
			  || ((tmp_ip=str2ip6(name))!=0)
			){
			/* we are lucky, this is an ip address */
			if (((dns_flags&DNS_IPV4_ONLY) && (tmp_ip->af==AF_INET6))||
				((dns_flags&DNS_IPV6_ONLY) && (tmp_ip->af==AF_INET))){
				return 0;
			}
			*port=SIP_PORT;
			return ip_addr2he(name, tmp_ip);
		}
		/* do naptr lookup */
		if ((e=dns_get_entry(name, T_NAPTR))==0)
			goto naptr_not_found;
		naptr_iterate_init(&tried_bmp);
		while(dns_naptr_sip_iterate(e->rr_lst, &tried_bmp,
												&srv_name, &n_proto)){
			if ((he=dns_srv_get_he(&srv_name, port, dns_flags))!=0){
#ifdef DNS_CACHE_DEBUG
				LM_DBG("(%.*s, %d, %d) srv, ret=%p\n",
							name->len, name->s, (int)*port, (int)*proto, he);
#endif
				dns_hash_put(e);
				*proto=n_proto;
				return he;
			}
		}
		/* no acceptable naptr record found, fallback to srv */
		dns_hash_put(e);
	}
naptr_not_found:
	if(proto) *proto = origproto;
	he = no_naptr_srv_sip_resolvehost(name,port,proto);
	/* fallback all the way down to A/AAAA */
	if (he==0) {
		he=dns_get_he(name,dns_flags);
	}
   return he;
}
#endif /* USE_NAPTR */



/* resolves a host name trying NAPTR lookup if *proto==0 and *port==0, SRV
 * lookup if *port==0 or normal A/AAAA lookup
 * if *port!=0.
 * when performing SRV lookup (*port==0) it will use proto to look for
 * tcp or udp hosts; if proto==0 => no SRV lookup
 * returns: hostent struct & *port filled with the port from the SRV record;
 *  0 on error
 */
struct hostent* dns_sip_resolvehost(str* name, unsigned short* port,
										char* proto)
{
#ifdef USE_NAPTR
	if (dns_flags&DNS_TRY_NAPTR)
		return dns_naptr_sip_resolvehost(name, port, proto);
#endif
	return dns_srv_sip_resolvehost(name, port, proto);
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
 *          On end of records (when used to iterate on all the ips) it
 *          will return E_DNS_EOR (you should not log an error for this
 *          value, is just a signal that the address list end has been reached)
 * Note: either e or name must be different from 0 (name.s !=0 also)
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
inline static int dns_a_resolve( struct dns_hash_entry** e,
								 unsigned char* rr_no,
								 str* name,
								 struct ip_addr* ip)
{
	struct dns_rr* rr;
	int ret;
	ticks_t now;
	struct ip_addr* tmp;

	rr=0;
	ret=-E_DNS_NO_IP;
	if (*e==0){ /* do lookup */
		/* if ip don't set *e */
		if (str2ip6(name)!=0)
			goto error;
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
	/* if the entry has already expired use the time at the end of lifetime */
	if (unlikely((s_ticks_t)(now-(*e)->expire)>=0)) now=(*e)->expire-1;
	rr=dns_entry_get_rr(*e, rr_no, now);
	if (rr){
		/* everything is ok now, we can try to "convert" the ip */
		dns_rr2ip((*e)->type, rr, ip);
		ret=0;
	}else{
		ret=-E_DNS_EOR;
	}
error:
	LM_DBG("(%.*s, %d) returning %d\n", name->len, name->s, *rr_no, ret);
	return ret;
}


/* lookup, fills the dns_entry pointer and the ip addr.
 *  (with the first good ip). if *e ==0 does the a lookup, and changes it
 *   to the result, if not it uses the current value and tries to use
 * Same as dns_a_resolve but for aaaa records (see above).
 */
inline static int dns_aaaa_resolve( struct dns_hash_entry** e,
									unsigned char* rr_no,
									str* name,
									struct ip_addr* ip)
{
	struct dns_rr* rr;
	int ret;
	ticks_t now;
	struct ip_addr* tmp;

	rr=0;
	ret=-E_DNS_NO_IP;
	if (*e==0){ /* do lookup */
		/* if ip don't set *e */
		if (str2ip(name)!=0)
			goto error;
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
	/* if the entry has already expired use the time at the end of lifetime */
	if (unlikely((s_ticks_t)(now-(*e)->expire)>=0)) now=(*e)->expire-1;
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
inline static int dns_ip_resolve(	struct dns_hash_entry** e,
									unsigned char* rr_no,
									str* name,
									struct ip_addr* ip,
									int flags)
{
	int ret, orig_ret;
	str host;
	struct dns_hash_entry* orig;

	ret=-E_DNS_NO_IP;
	if (*e==0){ /* first call */
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
	}else if ((*e)->type==T_A){
		/* continue A resolving */
		/* retrieve host name from the hash entry  (ignore name which might
		  be null when continuing a srv lookup) */
		host.s=(*e)->name;
		host.len=(*e)->name_len;
		ret=dns_a_resolve(e, rr_no, &host, ip);
		if (ret>=0) return ret;
		if (!(flags&(DNS_IPV6_ONLY|DNS_IPV6_FIRST|DNS_IPV4_ONLY))){
			/* not found, try with AAAA */
			orig_ret=ret;
			orig=*e;
			*e=0;
			*rr_no=0;
			ret=dns_aaaa_resolve(e, rr_no, &host, ip);
			if (ret==-E_DNS_NO_IP && orig_ret==-E_DNS_EOR)
				ret=orig_ret;
			/* delay original record release until we're finished with host*/
			dns_hash_put(orig);
		}
	}else if ((*e)->type==T_AAAA){
		/* retrieve host name from the hash entry  (ignore name which might
		  be null when continuing a srv lookup) */
		host.s=(*e)->name;
		host.len=(*e)->name_len;
		/* continue AAAA resolving */
		ret=dns_aaaa_resolve(e, rr_no, &host, ip);
		if (ret>=0) return ret;
		if ((flags&DNS_IPV6_FIRST) && !(flags&DNS_IPV6_ONLY)){
			/* not found, try with A */
			orig_ret=ret;
			orig=*e;
			*e=0;
			*rr_no=0;
			ret=dns_a_resolve(e, rr_no, &host, ip);
			if (ret==-E_DNS_NO_IP && orig_ret==-E_DNS_EOR)
				ret=orig_ret;
			/* delay original record release until we're finished with host*/
			dns_hash_put(orig);
		}
	}else{
		LM_CRIT("invalid record type %d\n", (*e)->type);
	}
	return ret;
}



/*  gets the first srv record starting at rr_no
 *  Next call will return the next record a.s.o.
 *  (similar to dns_a_resolve but for srv, sets host, port and automatically
 *   switches to the next record in the future)
 *
 *   if DNS_SRV_LB and tried!=NULL will do random weight based selection
 *   for choosing between SRV RRs with the same priority (as described in
 *    RFC2782).
 *   If tried==NULL or DNS_SRV_LB is not defined => always returns next
 *    record in the priority order and for records with the same priority
 *     the record with the higher weight (from the remaining ones)
 */
inline static int dns_srv_resolve_nxt(struct dns_hash_entry** e,
#ifdef DNS_SRV_LB
						srv_flags_t* tried,
#endif
						unsigned char* rr_no,
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
#ifdef DNS_SRV_LB
		if (tried)
			srv_reset_tried(tried);
#endif
		ret=-E_DNS_BAD_SRV_ENTRY;
	}
	now=get_ticks_raw();
	/* if the entry has already expired use the time at the end of lifetime */
	if (unlikely((s_ticks_t)(now-(*e)->expire)>=0)) now=(*e)->expire-1;
#ifdef DNS_SRV_LB
	if (tried){
		rr=dns_srv_get_nxt_rr(*e, tried, rr_no, now);
	}else
#endif
	{
		rr=dns_entry_get_rr(*e, rr_no, now);
		(*rr_no)++; /* try next record next time */
	}
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
inline static int dns_srv_resolve_ip(struct dns_srv_handle* h,
					str* name, struct ip_addr* ip, unsigned short* port,
					int flags)
{
	int ret;
	str host;

	host.len=0;
	host.s=0;
	do{
		if (h->a==0){
#ifdef DNS_SRV_LB
			if ((ret=dns_srv_resolve_nxt(&h->srv,
								(flags & DNS_SRV_RR_LB)?&h->srv_tried_rrs:0,
								&h->srv_no,
								name, &host, port))<0)
				goto error;
#else
			if ((ret=dns_srv_resolve_nxt(&h->srv, &h->srv_no,
								name, &host, port))<0)
				goto error;
#endif
			h->port=*port; /* store new port */
		}else{
			*port=h->port; /* return the stored port */
		}
		if ((ret=dns_ip_resolve(&h->a, &h->ip_no, &host, ip, flags))<0){
			/* couldn't find any good ip for this record, try the next one */
			if (h->a){
				dns_hash_put(h->a);
				h->a=0;
			}
		}else if (h->a==0){
			/* this was an ip, try the next srv record in the future */
		}
	}while(ret<0);
error:
#ifdef DNS_CACHE_DEBUG
	LM_DBG("(\"%.*s\", %d, %d), ret=%d, ip=%s\n",
			name->len, name->s, h->srv_no, h->ip_no, ret,
			ip?ZSW(ip_addr2a(ip)):"");
#endif
	return ret;
}



/* resolves a host name trying SRV lookup if *port==0 or normal A/AAAA lookup
 * if *port!=0.
 * when performing SRV lookup (*port==0) it will use proto to look for
 * tcp or udp hosts, otherwise proto is unused; if proto==0 => no SRV lookup
 * h must be initialized prior to  calling this function and can be used to
 * get the subsequent ips
 * returns:  <0 on error
 *            0 on success and it fills *ip, *port, *h
 */
inline static int dns_srv_sip_resolve(struct dns_srv_handle* h,  str* name,
						struct ip_addr* ip, unsigned short* port, char* proto,
						int flags)
{
	struct dns_srv_proto srv_proto_list[PROTO_LAST];
	static char tmp[MAX_DNS_NAME]; /* tmp. buff. for SRV lookups */
	str srv_name;
	struct ip_addr* tmp_ip;
	int ret;
	struct hostent* he;
	size_t i,list_len;
	char origproto;

	origproto = *proto;
	if (dns_hash==0){ /* not init => use normal, non-cached version */
		LM_WARN("called before dns cache initialization\n");
		h->srv=h->a=0;
		he=_sip_resolvehost(name, port, proto);
		if (he){
			hostent2ip_addr(ip, he, 0);
			return 0;
		}
		return -E_DNS_NO_SRV;
	}
	if ((h->srv==0) && (h->a==0)){ /* first call */
		if (proto && *proto==0){ /* makes sure we have a protocol set*/
			*proto=PROTO_UDP; /* default */
		}
		h->port=(*proto==PROTO_TLS)?SIPS_PORT:SIP_PORT; /* just in case we
														don't find another */
		h->proto=*proto; /* store initial protocol */
		if (port){
			if (*port==0){
				/* try SRV if initial call & no port specified
				 * (draft-ietf-sip-srv-06) */
				if ((name->len+SRV_MAX_PREFIX_LEN+1)>MAX_DNS_NAME){
					LM_WARN("domain name too long (%d), unable to perform SRV lookup\n",
								name->len);
				}else{
					/* check if it's an ip address */
					if ( ((tmp_ip=str2ip(name))!=0)
						  || ((tmp_ip=str2ip6(name))!=0)
						){
						/* we are lucky, this is an ip address */
						if (((flags&DNS_IPV4_ONLY) && (tmp_ip->af==AF_INET6))||
							((flags&DNS_IPV6_ONLY) && (tmp_ip->af==AF_INET))){
							return -E_DNS_AF_MISMATCH;
						}
						*ip=*tmp_ip;
						*port=h->port;
						/* proto already set */
						return 0;
					}

					/* looping on the ordered list until we found a protocol what has srv record */
					list_len = create_srv_pref_list(&origproto, srv_proto_list);
					for (i=0; i<list_len;i++) {
						switch (srv_proto_list[i].proto) {
							case PROTO_UDP:
							case PROTO_TCP:
							case PROTO_TLS:
							case PROTO_SCTP:
								create_srv_name(srv_proto_list[i].proto, name, tmp);
								break;
							default:
								LM_CRIT("unknown proto %d\n", (int)srv_proto_list[i].proto);
								return -E_DNS_CRITICAL;
						}
						srv_name.s=tmp;
						srv_name.len=strlen(tmp);
						if ((ret=dns_srv_resolve_ip(h, &srv_name, ip, port, flags))>=0)
						{
							h->proto = *proto = srv_proto_list[i].proto;
#ifdef DNS_CACHE_DEBUG
							LM_DBG("(%.*s, %d, %d), srv0, ret=%d\n",
								name->len, name->s, h->srv_no, h->ip_no, ret);
#endif
							return ret;
						}
					}
				}
			}else{ /* if (*port==0) */
				h->port=*port; /* store initial port */
				/* proto already set */
			}
		} /* if (port) */
	}else if (h->srv){
			srv_name.s=h->srv->name;
			srv_name.len=h->srv->name_len;
			/* continue srv resolving */
			ret=dns_srv_resolve_ip(h, &srv_name, ip, port, flags);
			if (proto)
				*proto=h->proto;
			LM_DBG("(%.*s, %d, %d), srv, ret=%d\n",
					name->len, name->s, h->srv_no, h->ip_no, ret);
			return ret;
	}
	if (name->len >= MAX_DNS_NAME) {
		LM_ERR("domain name too long\n");
		return -E_DNS_NAME_TOO_LONG;
	}
	ret=dns_ip_resolve(&h->a, &h->ip_no, name, ip, flags);
	if (port)
		*port=h->port;
	if (proto)
		*proto=h->proto;
#ifdef DNS_CACHE_DEBUG
	LM_DBG("(%.*s, %d, %d), ip, ret=%d\n",
			name->len, name->s, h->srv_no, h->ip_no, ret);
#endif
	return ret;
}



#ifdef USE_NAPTR
/* resolves a host name trying:
 * - NAPTR lookup if the address is not an ip and proto!=0, port!=0
 *    *port==0 and *proto=0 and if flags allow NAPTR lookups
 * -SRV lookup if  port!=0 and *port==0
 * - normal A/AAAA lookup if *port!=0, or port==0
 * when performing SRV lookup (*port==0) it will use proto to look for
 * tcp or udp hosts, otherwise proto is unused; if proto==0 => no SRV lookup
 * h must be initialized prior to  calling this function and can be used to
 * get the subsequent ips
 * returns:  <0 on error
 *            0 on success and it fills *ip, *port, dns_sip_resolve_h
 * WARNING: when finished, dns_sip_resolve_put(h) must be called!
 */
inline static int dns_naptr_sip_resolve(struct dns_srv_handle* h,  str* name,
						struct ip_addr* ip, unsigned short* port, char* proto,
						int flags)
{
	struct hostent* he;
	struct ip_addr* tmp_ip;
	naptr_bmp_t tried_bmp;
	struct dns_hash_entry* e;
	char n_proto, origproto;
	str srv_name;
	int ret;

	ret=-E_DNS_NO_NAPTR;
	origproto=*proto;
	if (dns_hash==0){ /* not init => use normal, non-cached version */
		LM_WARN("called before dns cache initialization\n");
		h->srv=h->a=0;
		he=_sip_resolvehost(name, port, proto);
		if (he){
			hostent2ip_addr(ip, he, 0);
			return 0;
		}
		return -E_DNS_NO_NAPTR;
	}
	if (((h->srv==0) && (h->a==0)) && /* first call */
			 proto && port && (*proto==0) && (*port==0)){
		*proto=PROTO_UDP; /* just in case we don't find another */

		/* check if it's an ip address */
		if ( ((tmp_ip=str2ip(name))!=0)
			  || ((tmp_ip=str2ip6(name))!=0)
			){
			/* we are lucky, this is an ip address */
			if (((flags&DNS_IPV4_ONLY) && (tmp_ip->af==AF_INET6))||
				((flags&DNS_IPV6_ONLY) && (tmp_ip->af==AF_INET))){
				return -E_DNS_AF_MISMATCH;
			}
			*ip=*tmp_ip;
			h->port=SIP_PORT;
			h->proto=*proto;
			*port=h->port;
			return 0;
		}
		/* do naptr lookup */
		if ((e=dns_get_entry(name, T_NAPTR))==0)
			goto naptr_not_found;
		naptr_iterate_init(&tried_bmp);
		while(dns_naptr_sip_iterate(e->rr_lst, &tried_bmp,
												&srv_name, &n_proto)){
			dns_srv_handle_init(h); /* make sure h does not contain garbage
									from previous dns_srv_sip_resolve calls */
			if ((ret=dns_srv_resolve_ip(h, &srv_name, ip, port, flags))>=0){
#ifdef DNS_CACHE_DEBUG
				LM_DBG("(%.*s, %d, %d), srv0, ret=%d\n",
								name->len, name->s, h->srv_no, h->ip_no, ret);
#endif
				dns_hash_put(e);
				*proto=n_proto;
				h->proto=*proto;
				return ret;
			}
		}
		/* no acceptable naptr record found, fallback to srv */
		dns_hash_put(e);
		dns_srv_handle_init(h); /* make sure h does not contain garbage
								from previous dns_srv_sip_resolve calls */
	}
naptr_not_found:
	*proto=origproto;
	return dns_srv_sip_resolve(h, name, ip, port, proto, flags);
}
#endif /* USE_NAPTR */



/* resolves a host name trying:
 * - NAPTR lookup if the address is not an ip and proto!=0, port!=0
 *    *port==0 and *proto=0 and if flags allow NAPTR lookups
 * -SRV lookup if  port!=0 and *port==0
 * - normal A/AAAA lookup if *port!=0, or port==0
 * when performing SRV lookup (*port==0) it will use proto to look for
 * tcp or udp hosts, otherwise proto is unused; if proto==0 => no SRV lookup
 * h must be initialized prior to  calling this function and can be used to
 * get the subsequent ips
 * returns:  <0 on error
 *            0 on success and it fills *ip, *port, dns_sip_resolve_h
 */
int dns_sip_resolve(struct dns_srv_handle* h,  str* name,
						struct ip_addr* ip, unsigned short* port, char* proto,
						int flags)
{
#ifdef USE_NAPTR
	if (flags&DNS_TRY_NAPTR)
		return dns_naptr_sip_resolve(h, name, ip, port, proto, flags);
#endif
	return dns_srv_sip_resolve(h, name, ip, port, proto, flags);
}

/* performs an a lookup and fills ip with the first good ip address
 * returns 0 on success, <0 on error (see the error codes)
 */
inline static int dns_a_get_ip(str* name, struct ip_addr* ip)
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


inline static int dns_aaaa_get_ip(str* name, struct ip_addr* ip)
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


#ifdef DNS_WATCHDOG_SUPPORT
/* sets the state of the DNS servers:
 * 1: at least one server is up
 * 0: all the servers are down
 */
void dns_set_server_state(int state)
{
	atomic_set(dns_servers_up, state);
}

/* returns the state of the DNS servers */
int dns_get_server_state(void)
{
	return atomic_get(dns_servers_up);
}
#endif /* DNS_WATCHDOG_SUPPORT */

/* rpc functions */
void dns_cache_mem_info(rpc_t* rpc, void* ctx)
{
	if (!cfg_get(core, core_cfg, use_dns_cache)){
		rpc->fault(ctx, 500, "dns cache support disabled (see use_dns_cache)");
		return;
	}
	rpc->add(ctx, "dd",  *dns_cache_mem_used, cfg_get(core, core_cfg, dns_cache_max_mem));
}


void dns_cache_debug(rpc_t* rpc, void* ctx)
{
	int h;
	struct dns_hash_entry* e;
	ticks_t now;

	if (!cfg_get(core, core_cfg, use_dns_cache)){
		rpc->fault(ctx, 500, "dns cache support disabled (see use_dns_cache)");
		return;
	}
	now=get_ticks_raw();
	LOCK_DNS_HASH();
		for (h=0; h<DNS_HASH_SIZE; h++){
			clist_foreach(&dns_hash[h], e, next){
				rpc->add(ctx, "sdddddd",
								e->name, e->type, e->total_size, e->refcnt.val,
								(s_ticks_t)(e->expire-now)<0?-1:
									TICKS_TO_S(e->expire-now),
								TICKS_TO_S(now-e->last_used),
								e->ent_flags);
			}
		}
	UNLOCK_DNS_HASH();
}


#ifdef USE_DNS_CACHE_STATS
static unsigned long  stat_sum(int ivar, int breset)
{
	unsigned long isum=0;
	int i1=0;

	for (; i1 < get_max_procs(); i1++)
		switch (ivar) {
			case 0:
				isum+=dns_cache_stats[i1].dns_req_cnt;
				if (breset)
					dns_cache_stats[i1].dns_req_cnt=0;
				break;
			case 1:
				isum+=dns_cache_stats[i1].dc_hits_cnt;
				if (breset)
					dns_cache_stats[i1].dc_hits_cnt=0;
				break;
			case 2:
				isum+=dns_cache_stats[i1].dc_neg_hits_cnt;
				if (breset)
					dns_cache_stats[i1].dc_neg_hits_cnt=0;
				break;
			case 3:
				isum+=dns_cache_stats[i1].dc_lru_cnt;
				if (breset)
					dns_cache_stats[i1].dc_lru_cnt=0;
				break;
		}

	return isum;
}


void dns_cache_stats_get(rpc_t* rpc, void* c)
{
	char *name=NULL;
	void *handle;
	int found=0,i=0;
	int reset=0;
	char* dns_cache_stats_names[] = {
		"dns_req_cnt",
		"dc_hits_cnt",
		"dc_neg_hits_cnt",
		"dc_lru_cnt",
		NULL
	};


	if (!cfg_get(core, core_cfg, use_dns_cache)) {
		rpc->fault(c, 500, "dns cache support disabled");
		return;
	}
	if (rpc->scan(c, "s", &name) < 0)
		return;
	if (rpc->scan(c, "d", &reset) < 0)
		return;
	if (!strcasecmp(name, DNS_CACHE_ALL_STATS)) {
		/* dump all the dns cache stat values */
		rpc->add(c, "{", &handle);
		for (i=0; dns_cache_stats_names[i]; i++)
			rpc->struct_add(handle, "d",
							dns_cache_stats_names[i],
							stat_sum(i, reset));

		found=1;
	} else {
		for (i=0; dns_cache_stats_names[i]; i++)
			if (!strcasecmp(dns_cache_stats_names[i], name)) {
				rpc->add(c, "{", &handle);
				rpc->struct_add(handle, "d",
								dns_cache_stats_names[i],
								stat_sum(i, reset));
				found=1;
				break;
			}
	}
	if(!found)
		rpc->fault(c, 500, "unknown dns cache stat parameter");

	return;
}
#endif /* USE_DNS_CACHE_STATS */

/* rpc functions */
void dns_cache_debug_all(rpc_t* rpc, void* ctx)
{
	int h;
	struct dns_hash_entry* e;
	struct dns_rr* rr;
	struct ip_addr ip;
	int i;
	ticks_t now;

	if (!cfg_get(core, core_cfg, use_dns_cache)){
		rpc->fault(ctx, 500, "dns cache support disabled (see use_dns_cache)");
		return;
	}
	now=get_ticks_raw();
	LOCK_DNS_HASH();
		for (h=0; h<DNS_HASH_SIZE; h++){
			clist_foreach(&dns_hash[h], e, next){
				for (i=0, rr=e->rr_lst; rr; i++, rr=rr->next){
					rpc->add(ctx, "sddddddd",
								e->name, (int)e->type, i, (int)e->total_size,
								(int)e->refcnt.val,
								(int)(s_ticks_t)(e->expire-now)<0?-1:
									TICKS_TO_S(e->expire-now),
								(int)TICKS_TO_S(now-e->last_used),
								(int)e->ent_flags);
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
							rpc->add(ctx, "ss", "naptr ",
								((struct naptr_rdata*)(rr->rdata))->flags);
							break;
						case T_CNAME:
							rpc->add(ctx, "ss", "cname",
									((struct cname_rdata*)(rr->rdata))->name);
							break;
						case T_TXT:
							rpc->add(ctx, "ss", "txt",
								((struct txt_rdata*)(rr->rdata))->cstr_no?
								((struct txt_rdata*)(rr->rdata))->txt[0].cstr:
								"");
							break;
						case T_EBL:
							rpc->add(ctx, "ss", "ebl",
								((struct ebl_rdata*)(rr->rdata))->apex);
							break;
						case T_PTR:
							rpc->add(ctx, "ss", "ptr",
								((struct ptr_rdata*)(rr->rdata))->ptrdname);
							break;
						default:
							rpc->add(ctx, "ss", "unknown", "?");
					}
					rpc->add(ctx, "d",
								(int)(s_ticks_t)(rr->expire-now)<0?-1:
									TICKS_TO_S(rr->expire-now));
				}
			}
		}
	UNLOCK_DNS_HASH();
}


static char *print_type(unsigned short type)
{
	switch (type) {
		case T_A:
			return "A";
		case T_AAAA:
			return "AAAA";
		case T_SRV:
			return "SRV";
		case T_NAPTR:
			return "NAPTR";
		case T_CNAME:
			return "CNAME";
		case T_TXT:
			return "TXT";
		case T_EBL:
			return "EBL";
		case T_PTR:
			return "PTR";
		default:
			return "unknown";
	}
}


/** convert string type to dns integer T_*.
 * used for rpc type translation.
 * @return T_* on success, -1 on error.
 */
static int dns_get_type(str* s)
{
	char *t;
	int len;
	
	t=s->s;
	len=s->len;
	/* skip over a T_ or t_ prefix */
	if ((len>2) && (t[0]=='T' || t[0]=='t') && (t[1]=='_')){
		t+=2;
		len-=2;
	}
	switch(len){
		case 1:
			if (t[0]=='A' || t[0]=='a')
				return T_A;
			break;
		case 4:
			if (strncasecmp(t, "AAAA", len)==0)
				return T_AAAA;
			break;
		case 3:
			if (strncasecmp(t, "SRV", len)==0)
				return T_SRV;
			else if (strncasecmp(t, "TXT", len)==0)
				return T_TXT;
			else if (strncasecmp(t, "EBL", len)==0)
				return T_EBL;
			else if (strncasecmp(t, "PTR", len)==0)
				return T_PTR;
			break;
		case 5:
			if (strncasecmp(t, "NAPTR", len)==0)
				return T_NAPTR;
			else if (strncasecmp(t, "CNAME", len)==0)
				return T_CNAME;
			break;
	}
	return -1;
}


/** rpc-prints a dns cache entry.
  */
void dns_cache_print_entry(rpc_t* rpc, void* ctx, struct dns_hash_entry* e)
{
	int expires;
	struct dns_rr* rr;
	struct ip_addr ip;
	ticks_t now;
	str s;
	int i;

	now=get_ticks_raw();
	expires = (s_ticks_t)(e->expire-now)<0?-1: TICKS_TO_S(e->expire-now);
	
	rpc->rpl_printf(ctx, "%sname: %s", SPACE_FORMAT, e->name);
	rpc->rpl_printf(ctx, "%stype: %s", SPACE_FORMAT, print_type(e->type));
	rpc->rpl_printf(ctx, "%ssize (bytes): %d", SPACE_FORMAT,
						e->total_size);
	rpc->rpl_printf(ctx, "%sreference counter: %d", SPACE_FORMAT,
						e->refcnt.val);
	if (e->ent_flags & DNS_FLAG_PERMANENT) {
		rpc->rpl_printf(ctx, "%spermanent: yes", SPACE_FORMAT);
	} else {
		rpc->rpl_printf(ctx, "%spermanent: no", SPACE_FORMAT);
		rpc->rpl_printf(ctx, "%sexpires in (s): %d", SPACE_FORMAT, expires);
	}
	rpc->rpl_printf(ctx, "%slast used (s): %d", SPACE_FORMAT,
						TICKS_TO_S(now-e->last_used));
	rpc->rpl_printf(ctx, "%snegative entry: %s", SPACE_FORMAT,
						(e->ent_flags & DNS_FLAG_BAD_NAME) ? "yes" : "no");
	
	for (rr=e->rr_lst; rr; rr=rr->next) {
		switch(e->type) {
			case T_A:
			case T_AAAA:
				if (dns_rr2ip(e->type, rr, &ip)==0){
				  rpc->rpl_printf(ctx, "%srr ip: %s", SPACE_FORMAT,
									ip_addr2a(&ip) );
				}else{
				  rpc->rpl_printf(ctx, "%srr ip: <error: bad rr>", 
									SPACE_FORMAT);
				}
				break;
			case T_SRV:
				rpc->rpl_printf(ctx, "%srr name: %s", SPACE_FORMAT,
							((struct srv_rdata*)(rr->rdata))->name);
				rpc->rpl_printf(ctx, "%srr port: %d", SPACE_FORMAT,
							((struct srv_rdata*)(rr->rdata))->port);
				rpc->rpl_printf(ctx, "%srr priority: %d", SPACE_FORMAT,
						((struct srv_rdata*)(rr->rdata))->priority);
				rpc->rpl_printf(ctx, "%srr weight: %d", SPACE_FORMAT,
							((struct srv_rdata*)(rr->rdata))->weight);
				break;
			case T_NAPTR:
				rpc->rpl_printf(ctx, "%srr order: %d", SPACE_FORMAT,
							((struct naptr_rdata*)(rr->rdata))->order);
				rpc->rpl_printf(ctx, "%srr preference: %d", SPACE_FORMAT,
							((struct naptr_rdata*)(rr->rdata))->pref);
				s.s = ((struct naptr_rdata*)(rr->rdata))->flags;
				s.len = ((struct naptr_rdata*)(rr->rdata))->flags_len;
				rpc->rpl_printf(ctx, "%srr flags: %.*s", SPACE_FORMAT,
									s.len, s.s);
				s.s=((struct naptr_rdata*)(rr->rdata))->services;
				s.len=((struct naptr_rdata*)(rr->rdata))->services_len;
				rpc->rpl_printf(ctx, "%srr service: %.*s", SPACE_FORMAT,
									s.len, s.s);
				s.s = ((struct naptr_rdata*)(rr->rdata))->regexp;
				s.len = ((struct naptr_rdata*)(rr->rdata))->regexp_len;
				rpc->rpl_printf(ctx, "%srr regexp: %.*s", SPACE_FORMAT,
									s.len, s.s);
				s.s = ((struct naptr_rdata*)(rr->rdata))->repl;
				s.len = ((struct naptr_rdata*)(rr->rdata))->repl_len;
				rpc->rpl_printf(ctx, "%srr replacement: %.*s", 
									SPACE_FORMAT, s.len, s.s);
				break;
			case T_CNAME:
				rpc->rpl_printf(ctx, "%srr name: %s", SPACE_FORMAT,
							((struct cname_rdata*)(rr->rdata))->name);
				break;
			case T_TXT:
				for (i=0; i<((struct txt_rdata*)(rr->rdata))->cstr_no;
						i++){
					rpc->rpl_printf(ctx, "%stxt[%d]: %s", SPACE_FORMAT, i,
						((struct txt_rdata*)(rr->rdata))->txt[i].cstr);
				}
				break;
			case T_EBL:
				rpc->rpl_printf(ctx, "%srr position: %d", SPACE_FORMAT,
							((struct ebl_rdata*)(rr->rdata))->position);
				rpc->rpl_printf(ctx, "%srr separator: %s", SPACE_FORMAT,
							((struct ebl_rdata*)(rr->rdata))->separator);
				rpc->rpl_printf(ctx, "%srr apex: %s", SPACE_FORMAT,
							((struct ebl_rdata*)(rr->rdata))->apex);
				break;
			case T_PTR:
				rpc->rpl_printf(ctx, "%srr name: %s", SPACE_FORMAT,
							((struct ptr_rdata*)(rr->rdata))->ptrdname);
				break;
			default:
				rpc->rpl_printf(ctx, "%sresource record: unknown",
									SPACE_FORMAT);
		}
		if ((e->ent_flags & DNS_FLAG_PERMANENT) == 0)
			rpc->rpl_printf(ctx, "%srr expires in (s): %d", SPACE_FORMAT,
						(s_ticks_t)(rr->expire-now)<0?-1 : 
						TICKS_TO_S(rr->expire-now));
	}
}



/* dumps the content of the cache in a human-readable format */
void dns_cache_view(rpc_t* rpc, void* ctx)
{
	int h;
	struct dns_hash_entry* e;
	ticks_t now;

	if (!cfg_get(core, core_cfg, use_dns_cache)){
		rpc->fault(ctx, 500, "dns cache support disabled (see use_dns_cache)");
		return;
	}
	now=get_ticks_raw();
	LOCK_DNS_HASH();
	for (h=0; h<DNS_HASH_SIZE; h++){
		clist_foreach(&dns_hash[h], e, next){
			if (((e->ent_flags & DNS_FLAG_PERMANENT) == 0)
				&& TICKS_LT(e->expire, now)
			) {
				continue;
			}
			rpc->rpl_printf(ctx, "{\n");
			dns_cache_print_entry(rpc, ctx, e);
			rpc->rpl_printf(ctx, "}");
		}
	}
	UNLOCK_DNS_HASH();
}


/* Delete all the entries from the cache.
 * If del_permanent is 0, then only the
 * non-permanent entries are deleted.
 */
void dns_cache_flush(int del_permanent)
{
	int h;
	struct dns_hash_entry* e;
	struct dns_hash_entry* tmp;

	LM_DBG("removing elements from the cache\n");
	LOCK_DNS_HASH();
		for (h=0; h<DNS_HASH_SIZE; h++){
			clist_foreach_safe(&dns_hash[h], e, tmp, next){
				if (del_permanent || ((e->ent_flags & DNS_FLAG_PERMANENT) == 0))
					_dns_hash_remove(e);
			}
		}
	UNLOCK_DNS_HASH();
}

/* deletes all the non-permanent entries from the cache */
void dns_cache_delete_all(rpc_t* rpc, void* ctx)
{
	if (!cfg_get(core, core_cfg, use_dns_cache)){
		rpc->fault(ctx, 500, "dns cache support disabled (see use_dns_cache)");
		return;
	}
	dns_cache_flush(0);
	rpc->rpl_printf(ctx, "OK");
}

/* deletes all the entries from the cache,
 * even the permanent ones */
void dns_cache_delete_all_force(rpc_t* rpc, void* ctx)
{
	if (!cfg_get(core, core_cfg, use_dns_cache)){
		rpc->fault(ctx, 500, "dns cache support disabled (see use_dns_cache)");
		return;
	}
	dns_cache_flush(1);
	rpc->rpl_printf(ctx, "OK");
}

/* clones an entry and extends its memory area to hold a new rr.
 * if rdata_size>0 the new dns_rr struct is initialized, but the rdata is
 * only filled with 0.
 */
static struct dns_hash_entry *dns_cache_clone_entry(struct dns_hash_entry *e,
													int rdata_size,
													int ttl,
													struct dns_rr **_new_rr)
{
	struct dns_hash_entry *new;
	struct dns_rr *rr, *last_rr, *new_rr;
	int size, rounded_size, rr_size;
	ticks_t now;
	int i;

	now=get_ticks_raw();
	size = e->total_size;
	if (rdata_size) {
		/* we have to extend the entry */
		rounded_size = ROUND_POINTER(size); /* size may not have been 
												rounded previously */
		switch (e->type) {
			case T_A:
			case T_AAAA:
			case T_CNAME:
				rr_size = sizeof(struct dns_rr);
				break;
			case T_SRV:
				rr_size = ROUND_SHORT(sizeof(struct dns_rr));
				break;
			case T_NAPTR:
				rr_size = ROUND_POINTER(sizeof(struct dns_rr));
				break;
			case T_TXT:
				rr_size = ROUND_POINTER(sizeof(struct dns_rr));
				break;
			case T_EBL:
				rr_size = ROUND_POINTER(sizeof(struct dns_rr));
				break;
			case T_PTR:
				rr_size = sizeof(struct dns_rr);
				break;
			default:
				LM_ERR("type %d not supported\n", e->type);
				return NULL;
		}
	} else {
		rounded_size = size; /* no need to round the size, we just clone
								the entry without extending it */
		rr_size = 0;
	}

	new=shm_malloc(rounded_size+rr_size+rdata_size);
	if (!new) {
		LM_ERR("out of memory\n");
		return NULL;
	}
	memset(new, 0, rounded_size+rr_size+rdata_size);
	/* clone the entry */
	memcpy(new, e, size);
	/* fix the values and pointers */
	new->next = new->prev = NULL;
#ifdef DNS_LU_LST
	new->last_used_lst.next = new->last_used_lst.prev = NULL;
#endif
	new->rr_lst = (struct dns_rr*)translate_pointer((char*)new, (char*)e,
														(char*)new->rr_lst);
	atomic_set(&new->refcnt, 0);
	new->last_used = now;
	/* expire and total_size are fixed later if needed */
	/* fix the pointers inside the rr structures */
	last_rr = NULL;
	for (rr=new->rr_lst; rr; rr=rr->next) {
		rr->rdata = (void*)translate_pointer((char*)new, (char*)e, 
												(char*)rr->rdata);
		if (rr->next)
			rr->next = (struct dns_rr*)translate_pointer((char*)new, (char*)e,
												(char*)rr->next);
		else
			last_rr = rr;

		switch(e->type){
			case T_NAPTR:
				/* there are pointers inside the NAPTR rdata stucture */
				((struct naptr_rdata*)rr->rdata)->flags =
					translate_pointer((char*)new, (char*)e,
						((struct naptr_rdata*)rr->rdata)->flags);

				((struct naptr_rdata*)rr->rdata)->services =
					translate_pointer((char*)new, (char*)e,
						((struct naptr_rdata*)rr->rdata)->services);

				((struct naptr_rdata*)rr->rdata)->regexp =
					translate_pointer((char*)new, (char*)e,
						((struct naptr_rdata*)rr->rdata)->regexp);

				((struct naptr_rdata*)rr->rdata)->repl =
					translate_pointer((char*)new, (char*)e,
						((struct naptr_rdata*)rr->rdata)->repl);
				break;
			case T_TXT:
				/* there are pointers inside the TXT structure */
				for (i=0; i<((struct txt_rdata*)rr->rdata)->cstr_no; i++){
					((struct txt_rdata*)rr->rdata)->txt[i].cstr=
						translate_pointer((char*) new, (char*) e,
							((struct txt_rdata*)rr->rdata)->txt[i].cstr);
				}
				break;
			case T_EBL:
				/* there are pointers inside the EBL structure */
				((struct ebl_rdata*)rr->rdata)->separator =
					translate_pointer((char*)new, (char*)e,
							((struct ebl_rdata*)rr->rdata)->separator);
				((struct ebl_rdata*)rr->rdata)->apex =
					translate_pointer((char*)new, (char*)e,
							((struct ebl_rdata*)rr->rdata)->apex);
				break;
		}
	}


	if (rdata_size) {
		/* set the pointer to the new rr structure */
		new_rr = (void*)((char*)new + rounded_size);
		new_rr->rdata = (void*)((char*)new_rr+rr_size);
		new_rr->expire = now + S_TO_TICKS(ttl);
		/* link the rr to the previous one */
		last_rr->next = new_rr;

		/* fix the total_size and expires values */
		new->total_size=rounded_size+rr_size+rdata_size;
		new->expire = MAX(new->expire, new_rr->expire);


		if (_new_rr)
			*_new_rr = new_rr;
	} else {
		if (_new_rr)
			*_new_rr = NULL;
	}

	return new;
}


/* Adds a new record to the cache.
 * If there is an existing record with the same name and value
 * (ip address in case of A/AAAA record, name in case of SRV record)
 * only the remaining fields are updated.
 * 
 * Note that permanent records cannot be overwritten unless
 * the new record is also permanent. A permanent record
 * completely replaces a non-permanent one.
 *
 * Currently only A, AAAA, and SRV records are supported.
 */
int dns_cache_add_record(unsigned short type,
			str *name,
			int ttl,
			str *value,
			int priority,
			int weight,
			int port,
			int flags)
{
	struct dns_hash_entry *old=NULL, *new=NULL;
	struct dns_rr *rr;
	str rr_name;
	struct ip_addr *ip_addr;
	ticks_t expire;
	int err, h;
	int size;
	struct dns_rr	*new_rr, **rr_p, **rr_iter;
	struct srv_rdata	*srv_rd;

	/* eliminate gcc warnings */
	ip_addr = 0;
	size = 0;
	rr_name.s = NULL;
	rr_name.len = 0;

	if (!cfg_get(core, core_cfg, use_dns_cache)){
		LM_ERR("dns cache support disabled (see use_dns_cache)\n");
		return -1;
	}
	
	if ((type != T_A) && (type != T_AAAA) && (type != T_SRV)) {
		LM_ERR("rr type %d is not implemented\n", type);
		return -1;
	}

	if ((flags & DNS_FLAG_BAD_NAME) == 0) {
		/* fix-up the values */
		switch(type) {
		case T_A:
			ip_addr = str2ip(value);
			if (!ip_addr) {
				LM_ERR("Malformed ip address: %.*s\n",
					value->len, value->s);
				return -1;
			}
			break;
		case T_AAAA:
			ip_addr = str2ip6(value);
			if (!ip_addr) {
				LM_ERR("Malformed ip address: %.*s\n",
					value->len, value->s);
				return -1;
			}
			break;
		case T_SRV:
			rr_name = *value;
			break;
		}
	}

	/* check whether there is a matching entry in the cache */
	old = dns_hash_get(name, type, &h, &err);
	if (old && old->type!=type) {
		/* probably we found a CNAME instead of the specified type,
		it is not needed */
		dns_hash_put(old);
		old=NULL;
	}

	if (old
		&& (old->ent_flags & DNS_FLAG_PERMANENT)
		&& ((flags & DNS_FLAG_PERMANENT) == 0)
	) {
		LM_ERR("A non-permanent record cannot overwrite "
				"a permanent entry\n");
		goto error;
	}
	/* prepare the entry */
	if (flags & DNS_FLAG_BAD_NAME) {
		/* negative entry */
		new = dns_cache_mk_bad_entry(name, type, ttl, flags);
		if (!new) {
			LM_ERR("Failed to create a negative "
					"DNS cache entry\n");
			goto error;
		}
	} else {
		if (!old
			|| (old->ent_flags & DNS_FLAG_BAD_NAME)
			|| (((old->ent_flags & DNS_FLAG_PERMANENT) == 0)
				&& (flags & DNS_FLAG_PERMANENT))
		) {
			/* There was no matching entry in the hash table,
			 * the entry is a negative record with inefficient space,
			 * or a permanent entry overwrites a non-permanent one.
			 * Let us create a new one.
			 */
			switch(type) {
			case T_A:
			case T_AAAA:
				new = dns_cache_mk_ip_entry(name, ip_addr);
				if (!new) {
					LM_ERR("Failed to create an A/AAAA record\n");
					goto error;
				}
				/* fix the expiration time, dns_cache_mk_ip_entry() sets it 
				 * to now-1 */
				expire = get_ticks_raw() + S_TO_TICKS(ttl);
				new->expire = expire;
				new->rr_lst->expire = expire;
				break;
			case T_SRV:
				new = dns_cache_mk_srv_entry(name, priority, weight, port,
												&rr_name, ttl);
				if (!new) {
					LM_ERR("Failed to create an SRV record\n");
					goto error;
				}
			}
			new->ent_flags = flags;
		} else {
			/* we must modify the entry, so better to clone it, modify the new 
			 * one, and replace the old with the new entry in the hash table,
			 * because the entry might be in use (even if the dns hash is 
			 * locked). The old entry will be removed from the hash and 
			 * automatically destroyed when its refcnt will be 0*/

			/* check whether there is an rr with the same value */
			for (rr=old->rr_lst; rr; rr=rr->next)
				if ((((type == T_A) || (type == T_AAAA)) &&
					(memcmp(ip_addr->u.addr, ((struct a_rdata*)rr->rdata)->ip,
										ip_addr->len)==0))
				|| ((type == T_SRV) &&
					(((struct srv_rdata*)rr->rdata)->name_len == rr_name.len)&&
					(memcmp(rr_name.s, ((struct srv_rdata*)rr->rdata)->name,
										rr_name.len)==0)))
				break;

			if (rr) {
				/* the rr was found in the list */
				new = dns_cache_clone_entry(old, 0, 0, 0);
				if (!new) {
					LM_ERR("Failed to clone an existing "
							"DNS cache entry\n");
					goto error;
				}
				/* let the rr point to the new structure */
				rr = (struct dns_rr*)translate_pointer((char*)new, (char*)old,
														(char*)rr);
				new_rr = rr;

				if (type == T_SRV) {
					/* fix the priority, weight, and port */
					((struct srv_rdata*)rr->rdata)->priority = priority;
					((struct srv_rdata*)rr->rdata)->weight = weight;
					((struct srv_rdata*)rr->rdata)->port = port;
				}

				/* fix the expire value */
				rr->expire = get_ticks_raw() + S_TO_TICKS(ttl);
				new->expire = 0;
				for (rr=new->rr_lst; rr; rr=rr->next)
					new->expire = MAX(new->expire, rr->expire);
			} else {
				/* there was no matching rr, extend the structure with a new
				 * one */
				switch(type) {
				case T_A:
					size = sizeof(struct a_rdata);
					break;
				case T_AAAA:
					size = sizeof(struct aaaa_rdata);
					break;
				case T_SRV:
					size = sizeof(struct srv_rdata)-1 +
						rr_name.len+1;
					break;
				}
				new = dns_cache_clone_entry(old, size, ttl, &rr);
				if (!new) {
					LM_ERR("Failed to clone an existing "
							"DNS cache entry\n");
					goto error;
				}
				new_rr = rr;

				switch(type) {
				case T_A:
				case T_AAAA:
					memcpy(rr->rdata, ip_addr->u.addr, ip_addr->len);
					break;
				case T_SRV:
					((struct srv_rdata*)rr->rdata)->priority = priority;
					((struct srv_rdata*)rr->rdata)->weight = weight;
					((struct srv_rdata*)rr->rdata)->port = port;
					((struct srv_rdata*)rr->rdata)->name_len = rr_name.len;
					memcpy(((struct srv_rdata*)rr->rdata)->name, rr_name.s, 
									rr_name.len);
				}
				/* maximum expire value has been already fixed by 
				 * dns_cache_clone_entry() */
			}

			if (type == T_SRV) {
				/* SRV records must be ordered by their priority and weight.
				 * With modifying an exising rr, or adding new rr to the DNS entry,
				 * the ordered list might got broken which needs to be fixed.
				 */
				rr_p = NULL;
				for (	rr_iter = &new->rr_lst;
					*rr_iter;
					rr_iter = &((*rr_iter)->next)
				) {
					if (*rr_iter == new_rr) {
						rr_p = rr_iter;
						continue;
					}
					srv_rd = (struct srv_rdata*)(*rr_iter)->rdata;
					if ((priority < srv_rd->priority) ||
						((priority == srv_rd->priority)	&& (weight > srv_rd->weight))
					)
						break; /* insert here */
				}

				if (!rr_p)
					for (	rr_p = rr_iter;
						*rr_p && (*rr_p != new_rr);
						rr_p = &((*rr_p)->next)
					);
				if (!rr_p) {
					LM_ERR("Failed to correct the orderd list of SRV resource records\n");
					goto error;
				}

				if (*rr_iter != new_rr->next) {
					/* unlink rr from the list */
					*rr_p = (*rr_p)->next;
					/* link it before *rr_iter */
					new_rr->next = *rr_iter;
					*rr_iter = new_rr;
				}
			}
		}
	}

	LOCK_DNS_HASH();
	if (dns_cache_add_unsafe(new)) {
		LM_ERR("Failed to add the entry to the cache\n");
		UNLOCK_DNS_HASH();
		goto error;
	} else {
		/* remove the old entry from the list */
		if (old)
			_dns_hash_remove(old);
	}
	UNLOCK_DNS_HASH();

	if (old)
		dns_hash_put(old);
	return 0;

error:
	/* leave the old entry in the list, and free the new one */
	if (old)
		dns_hash_put(old);
	if (new)
		dns_destroy_entry(new);
	return -1;
}


/* deletes a record from the cache */
static void dns_cache_delete_record(rpc_t* rpc, void* ctx, unsigned short type)
{
	struct dns_hash_entry *e;
	str name;
	int err, h, found=0, permanent=0;

	if (!cfg_get(core, core_cfg, use_dns_cache)){
		rpc->fault(ctx, 500, "dns cache support disabled (see use_dns_cache)");
		return;
	}
	
	if (rpc->scan(ctx, "S", &name) < 1)
		return;

	LOCK_DNS_HASH();

	e=_dns_hash_find(&name, type, &h, &err);
	if (e && (e->type==type)) {
		if ((e->ent_flags & DNS_FLAG_PERMANENT) == 0)
			_dns_hash_remove(e);
		else
			permanent = 1;
		found = 1;
	}

	UNLOCK_DNS_HASH();

	if (permanent)
		rpc->fault(ctx, 400, "Permanent entries cannot be deleted");
	else if (!found)
		rpc->fault(ctx, 400, "Not found");
}

/* Delete a single record from the cache,
 * i.e. the record with the same name and value
 * (ip address in case of A/AAAA record, name in case of SRV record).
 *
 * Currently only A, AAAA, and SRV records are supported.
 */
int dns_cache_delete_single_record(unsigned short type,
			str *name,
			str *value,
			int flags)
{
	struct dns_hash_entry *old=NULL, *new=NULL;
	struct dns_rr *rr, **next_p;
	str rr_name;
	struct ip_addr *ip_addr;
	int err, h;

	/* eliminate gcc warnings */
	rr_name.s = NULL;
	rr_name.len = 0;
	ip_addr = 0;

	if (!cfg_get(core, core_cfg, use_dns_cache)){
		LM_ERR("dns cache support disabled (see use_dns_cache)\n");
		return -1;
	}
	
	if ((type != T_A) && (type != T_AAAA) && (type != T_SRV)) {
		LM_ERR("rr type %d is not implemented\n", type);
		return -1;
	}

	if ((flags & DNS_FLAG_BAD_NAME) == 0) {
		/* fix-up the values */
		switch(type) {
		case T_A:
			ip_addr = str2ip(value);
			if (!ip_addr) {
				LM_ERR("Malformed ip address: %.*s\n",
					value->len, value->s);
				return -1;
			}
			break;
		case T_AAAA:
			ip_addr = str2ip6(value);
			if (!ip_addr) {
				LM_ERR("Malformed ip address: %.*s\n",
					value->len, value->s);
				return -1;
			}
			break;
		case T_SRV:
			rr_name = *value;
			break;
		}
	}

	/* check whether there is a matching entry in the cache */
	if ((old = dns_hash_get(name, type, &h, &err)) == NULL)
		goto not_found;

	if ((old->type != type) /* may be CNAME */
		|| (old->ent_flags != flags)
	)
		goto not_found;

	if (flags & DNS_FLAG_BAD_NAME) /* negative record, there is no value */
		goto delete;

	/* check whether there is an rr with the same value */
	for (rr=old->rr_lst, next_p=&old->rr_lst;
		rr;
		next_p=&rr->next, rr=rr->next
	)
		if ((((type == T_A) || (type == T_AAAA)) &&
			(memcmp(ip_addr->u.addr, ((struct a_rdata*)rr->rdata)->ip,
								ip_addr->len)==0))
		|| ((type == T_SRV) &&
			(((struct srv_rdata*)rr->rdata)->name_len == rr_name.len) &&
			(memcmp(rr_name.s, ((struct srv_rdata*)rr->rdata)->name,
								rr_name.len)==0)))
		break;

	if (!rr)
		goto not_found;

	if ((rr == old->rr_lst) && (rr->next == NULL)) {
		/* There is a single rr value, hence the whole
		 * hash entry can be deleted */
		goto delete;
	} else {
		/* we must modify the entry, so better to clone it, modify the new 
		* one, and replace the old with the new entry in the hash table,
		* because the entry might be in use (even if the dns hash is 
		* locked). The old entry will be removed from the hash and 
		* automatically destroyed when its refcnt will be 0*/
		new = dns_cache_clone_entry(old, 0, 0, 0);
		if (!new) {
			LM_ERR("Failed to clone an existing DNS cache entry\n");
			dns_hash_put(old);
			return -1;
		}
		/* let rr and next_p point to the new structure */
		rr = (struct dns_rr*)translate_pointer((char*)new,
						(char*)old,
						(char*)rr);
		next_p = (struct dns_rr**)translate_pointer((char*)new,
						(char*)old,
						(char*)next_p);
		/* unlink rr from the list. The memory will be freed
		 * when the whole record is freed */
		*next_p = rr->next;
	}

delete:
	LOCK_DNS_HASH();
	if (new) {
		/* delete the old entry only if the new one can be added */
		if (dns_cache_add_unsafe(new)) {
			LM_ERR("Failed to add the entry to the cache\n");
			UNLOCK_DNS_HASH();
			if (old)
				dns_hash_put(old);
			return -1;
		} else {
			/* remove the old entry from the list */
			if (old)
				_dns_hash_remove(old);
		}
	} else if (old) {
		_dns_hash_remove(old);
	}
	UNLOCK_DNS_HASH();

	if (old)
		dns_hash_put(old);
	return 0;

not_found:
	LM_ERR("No matching record found\n");
	if (old)
		dns_hash_put(old);
	return -1;
}

/* performs  a dns lookup over rpc */
void dns_cache_rpc_lookup(rpc_t* rpc, void* ctx)
{
	struct dns_hash_entry *e;
	str name;
	str type;
	int t;

	if (!cfg_get(core, core_cfg, use_dns_cache)){
		rpc->fault(ctx, 500, "dns cache support disabled (see use_dns_cache)");
		return;
	}
	
	if (rpc->scan(ctx, "SS", &type, &name) < 1)
		return;
	t=dns_get_type(&type);
	if (t<0){
		rpc->fault(ctx, 400, "Invalid type");
		return;
	}
	e=dns_get_entry(&name, t);
	if (e==0){
		rpc->fault(ctx, 400, "Not found");
		return;
	}
	dns_cache_print_entry(rpc, ctx, e);
	dns_hash_put(e);
}



/* wrapper functions for adding and deleting records */
void dns_cache_add_a(rpc_t* rpc, void* ctx)
{
	str	name;
	int	ttl;
	str	ip;
	int	flags;

	if (rpc->scan(ctx, "SdSd", &name, &ttl, &ip, &flags) < 4)
		return;

	if (dns_cache_add_record(T_A,
				&name,
				ttl,
				&ip,
				0 /* priority */,
				0 /* weight */,
				0 /* port */,
				flags)
	)
		rpc->fault(ctx, 400, "Failed to add the entry to the cache");
}


void dns_cache_add_aaaa(rpc_t* rpc, void* ctx)
{
	str	name;
	int	ttl;
	str	ip;
	int	flags;

	if (rpc->scan(ctx, "SdSd", &name, &ttl, &ip, &flags) < 4)
		return;

	if (dns_cache_add_record(T_AAAA,
				&name,
				ttl,
				&ip,
				0 /* priority */,
				0 /* weight */,
				0 /* port */,
				flags)
	)
		rpc->fault(ctx, 400, "Failed to add the entry to the cache");
}

void dns_cache_add_srv(rpc_t* rpc, void* ctx)
{
	str	name;
	int	ttl, priority, weight, port;
	str	rr_name;
	int	flags;

	if (rpc->scan(ctx, "SddddSd", &name, &ttl, &priority, &weight, &port,
					&rr_name, &flags) < 7
	)
		return;

	if (dns_cache_add_record(T_SRV,
				&name,
				ttl,
				&rr_name,
				priority,
				weight,
				port,
				flags)
	)
		rpc->fault(ctx, 400, "Failed to add the entry to the cache");
}




void dns_cache_delete_a(rpc_t* rpc, void* ctx)
{
	dns_cache_delete_record(rpc, ctx, T_A);
}


void dns_cache_delete_aaaa(rpc_t* rpc, void* ctx)
{
	dns_cache_delete_record(rpc, ctx, T_AAAA);
}


void dns_cache_delete_srv(rpc_t* rpc, void* ctx)
{
	dns_cache_delete_record(rpc, ctx, T_SRV);
}


void dns_cache_delete_naptr(rpc_t* rpc, void* ctx)
{
	dns_cache_delete_record(rpc, ctx, T_NAPTR);
}


void dns_cache_delete_cname(rpc_t* rpc, void* ctx)
{
	dns_cache_delete_record(rpc, ctx, T_CNAME);
}


void dns_cache_delete_txt(rpc_t* rpc, void* ctx)
{
	dns_cache_delete_record(rpc, ctx, T_TXT);
}

void dns_cache_delete_ebl(rpc_t* rpc, void* ctx)
{
	dns_cache_delete_record(rpc, ctx, T_EBL);
}

void dns_cache_delete_ptr(rpc_t* rpc, void* ctx)
{
	dns_cache_delete_record(rpc, ctx, T_PTR);
}



#ifdef DNS_WATCHDOG_SUPPORT
/* sets the DNS server states */
void dns_set_server_state_rpc(rpc_t* rpc, void* ctx)
{
	int	state;

	if (!cfg_get(core, core_cfg, use_dns_cache)){
		rpc->fault(ctx, 500, "dns cache support disabled (see use_dns_cache)");
		return;
	}
	if (rpc->scan(ctx, "d", &state) < 1)
		return;
	dns_set_server_state(state);
}

/* prints the DNS server state */
void dns_get_server_state_rpc(rpc_t* rpc, void* ctx)
{
	if (!cfg_get(core, core_cfg, use_dns_cache)){
		rpc->fault(ctx, 500, "dns cache support disabled (see use_dns_cache)");
		return;
	}
	rpc->add(ctx, "d", dns_get_server_state());
}
#endif /* DNS_WATCHDOG_SUPPORT */

#endif

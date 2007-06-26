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
 *  2006-07-29  created by andrei
 *  2007-05-39  added hooks for add; more locks to reduce contention (andrei)
 *  2007-06-26  added hooks for search (andrei)
 */


#ifdef USE_DST_BLACKLIST

#include "dst_blacklist.h"
#include "mem/shm_mem.h"
#include "hashes.h"
#include "locking.h"
#include "timer.h"
#include "timer_ticks.h"
#include "ip_addr.h"
#include "error.h"
#include "rpc.h"
#include "compiler_opt.h"




struct dst_blst_entry{
	struct dst_blst_entry* next;
	ticks_t expire;
	unsigned short port;
	unsigned char proto;
	unsigned char flags; /* contains the address type + error flags */
	unsigned char ip[4]; /* 4 for ipv4, 16 for ipv6 */ 
};

#define DST_BLST_ENTRY_SIZE(b) \
		(sizeof(struct dst_blst_entry)+((b).flags&BLST_IS_IPV6)*12)


#define DST_BLST_HASH_SIZE		1024
#define DEFAULT_BLST_TIMEOUT		60  /* 1 min. */
#define DEFAULT_BLST_MAX_MEM	250 /* 1 Kb FIXME (debugging)*/
#define DEFAULT_BLST_TIMER_INTERVAL		60 /* 1 min */


/* lock method */
#ifdef GEN_LOCK_T_UNLIMITED
#define BLST_LOCK_PER_BUCKET
#elif defined GEN_LOCK_SET_T_UNLIMITED
#define BLST_LOCK_SET
#else
#define BLST_ONE_LOCK
#endif


#ifdef BLST_LOCK_PER_BUCKET
/* lock included in the hash bucket */
#define LOCK_BLST(h)		lock_get(&dst_blst_hash[(h)].lock)
#define UNLOCK_BLST(h)		lock_release(&dst_blst_hash[(h)].lock)
#elif defined BLST_LOCK_SET
static gen_lock_set_t* blst_lock_set=0;
#define LOCK_BLST(h)		lock_set_get(blst_lock_set, (h))
#define UNLOCK_BLST(h)		lock_set_release(blst_lock_set, (h))
#else
/* use only one lock */
static gen_lock_t* blst_lock=0;
#define LOCK_BLST(h)		lock_get(blst_lock)
#define UNLOCK_BLST(h)		lock_release(blst_lock)
#endif




#define BLST_HASH_STATS

#ifdef BLST_HASH_STATS
#define BLST_HASH_STATS_DEC(h) dst_blst_hash[(h)].entries--
#define BLST_HASH_STATS_INC(h) dst_blst_hash[(h)].entries++
#else
#define BLST_HASH_STATS_DEC(h) do{}while(0)
#define BLST_HASH_STATS_INC(h) do{}while(0)
#endif

struct dst_blst_lst_head{
	struct dst_blst_entry* first;
#ifdef BLST_LOCK_PER_BUCKET
	gen_lock_t	lock;
#endif
#ifdef BLST_HASH_STATS
	unsigned int entries;
#endif
};

static struct timer_ln* blst_timer_h=0;

static volatile unsigned int* blst_mem_used=0;
unsigned int  blst_max_mem=DEFAULT_BLST_MAX_MEM; /* maximum memory used
													for the blacklist entries*/
unsigned int blst_timeout=DEFAULT_BLST_TIMEOUT;
unsigned int blst_timer_interval=DEFAULT_BLST_TIMER_INTERVAL;
struct dst_blst_lst_head* dst_blst_hash=0;



#ifdef DST_BLACKLIST_HOOKS

/* there 2 types of callbacks supported: on add new entry to the blacklist
 *  (DST_BLACKLIST_ADD_CB) and on blacklist search (DST_BLACKLIST_SEARCH_CB).
 *  Both of them take a struct dest_info* and a flags pointer as parameters 
 *   (unsigned char*). The flags can be changed.
 *  A callback should return one of:
 *    DST_BLACKLIST_CONTINUE - do nothing, let other callbacks run
 *    DST_BLACKLIST_ACCEPT   - for blacklist add: force accept immediately,
 *                             for blacklist search: force match and use
 *                              the flags as the blacklist search return.
 *                              ( so the flags should be set to some valid
 *                                non zero BLST flags value )
 *   DST_BLACKLIST_DENY      - for blacklist add: don't allow adding the
 *                              destination to the blacklist.
 *                             for blacklist search: force return not found
 */

#define MAX_BLST_HOOKS 1

struct blst_callbacks_lst{
	struct blacklist_hook* hooks;
	unsigned int max_hooks;
	int last_idx;
};

static struct blst_callbacks_lst blst_add_cb;
static struct blst_callbacks_lst blst_search_cb;

static int init_blst_callback_lst(struct blst_callbacks_lst*  cb_lst, int max)
{

	cb_lst->max_hooks=MAX_BLST_HOOKS;
	cb_lst->last_idx=0;
	cb_lst->hooks=pkg_malloc(cb_lst->max_hooks*sizeof(struct blacklist_hook));
	if (cb_lst->hooks==0)
		goto error;
	memset(cb_lst->hooks, 0, cb_lst->max_hooks*sizeof(struct blacklist_hook));
	return 0;
error:
	return -1;
}


static void destroy_blst_callback_lst(struct blst_callbacks_lst* cb_lst)
{
	int r;
	if (cb_lst && cb_lst->hooks){
		for (r=0; r<cb_lst->last_idx; r++){
			if (cb_lst->hooks[r].destroy)
				cb_lst->hooks[r].destroy();
		}
		pkg_free(cb_lst->hooks);
		cb_lst->hooks=0;
		cb_lst->last_idx=0;
		cb_lst->max_hooks=0;
	}
}


static void destroy_blacklist_hooks()
{
	destroy_blst_callback_lst(&blst_add_cb);
	destroy_blst_callback_lst(&blst_search_cb);
}


static int init_blacklist_hooks()
{

	if (init_blst_callback_lst(&blst_add_cb, MAX_BLST_HOOKS)!=0)
		goto error;
	if (init_blst_callback_lst(&blst_search_cb, MAX_BLST_HOOKS)!=0)
		goto error;
	return 0;
error:
	LOG(L_ERR, "blacklist_hooks: failure initializing internal lists\n");
	destroy_blacklist_hooks();
	return -1;
}




/* allocates a new hook
 * returns 0 on success and -1 on error
 * must be called from mod init (from the main process, before forking)*/
int register_blacklist_hook(struct blacklist_hook *h, int type)
{
	struct blst_callbacks_lst* cb_lst;
	struct blacklist_hook* tmp;
	int new_max_hooks;
	
	switch(type){
		case DST_BLACKLIST_ADD_CB:
			cb_lst=&blst_add_cb;
			break;
		case DST_BLACKLIST_SEARCH_CB:
			cb_lst=&blst_search_cb;
			break;
		default:
			BUG("register_blacklist_hook: invalid type %d\n", type);
			goto error;
	}
	if (cb_lst==0 || cb_lst->hooks==0 || cb_lst->max_hooks==0){
		BUG("register_blacklist_hook: intialization error\n");
		goto error;
	}

	if (cb_lst->last_idx >= cb_lst->max_hooks){
		new_max_hooks=2*cb_lst->max_hooks;
		tmp=pkg_realloc(cb_lst->hooks, 
				new_max_hooks*sizeof(struct blacklist_hook));
		if (tmp==0){
			goto error;
		}
		cb_lst->hooks=tmp;
		/* init the new chunk (but not the current entry which is 
		 * overwritten anyway) */
		memset(&cb_lst->hooks[cb_lst->max_hooks+1], 0, 
					(new_max_hooks-cb_lst->max_hooks-1)*
						sizeof(struct blacklist_hook));
		cb_lst->max_hooks=new_max_hooks;
	}
	cb_lst->hooks[cb_lst->last_idx]=*h;
	cb_lst->last_idx++;
	return 0;
error:
	return -1;
}


inline static int blacklist_run_hooks(struct blst_callbacks_lst *cb_lst,
							struct dest_info* si, unsigned char* flags)
{
	int r;
	int ret;
	
	ret=DST_BLACKLIST_CONTINUE; /* default, if no hook installed accept 
								blacklist operation */
	if (likely(cb_lst->last_idx==0))
		return ret;
	for (r=0; r<cb_lst->last_idx; r++){
		ret=cb_lst->hooks[r].on_blst_add(si, flags);
		if (ret!=DST_BLACKLIST_CONTINUE) break;
	}
	return ret;
}


#endif /* DST_BLACKLIST_HOOKS */


inline static void blst_destroy_entry(struct dst_blst_entry* e)
{
	shm_free(e);
}


static ticks_t blst_timer(ticks_t ticks, struct timer_ln* tl, void* data);


inline static void dst_blst_entry2ip(struct ip_addr* ip,
										struct dst_blst_entry* e)
{
	if (e->flags & BLST_IS_IPV6){
		ip->af=AF_INET6;
		ip->len=16;
	}else{
		ip->af=AF_INET;
		ip->len=4;
	}
	memcpy(ip->u.addr, e->ip, ip->len);
}



inline static unsigned short dst_blst_hash_no(unsigned char proto,
											  struct ip_addr* ip,
											  unsigned short port)
{
	str s1;
	str s2;
	
	s1.s=(char*)ip->u.addr;
	s1.len=ip->len;
	s2.s=(char*)&port;
	s2.len=sizeof(unsigned short);
	return get_hash2_raw(&s1, &s2)%DST_BLST_HASH_SIZE;
}



void destroy_dst_blacklist()
{
	int r;
	struct dst_blst_entry** crt;
	struct dst_blst_entry** tmp;
	struct dst_blst_entry* e;
	
	if (blst_timer_h){
		timer_del(blst_timer_h);
		timer_free(blst_timer_h);
		blst_timer_h=0;
	}
#ifdef BLST_LOCK_PER_BUCKET
		for(r=0; r<DST_BLST_HASH_SIZE; r++)
			lock_destroy(&dst_blst_hash[r].lock);
#elif defined BLST_LOCK_SET
		if (blst_lock_set){
			lock_set_destroy(blst_lock_set);
			lock_set_dealloc(blst_lock_set);
			blst_lock_set=0;
		}
#else
	if (blst_lock){
		lock_destroy(blst_lock);
		lock_dealloc(blst_lock);
		blst_lock=0;
	}
#endif
	
	if (dst_blst_hash){
		for(r=0; r<DST_BLST_HASH_SIZE; r++){
			for (crt=&dst_blst_hash[r].first, tmp=&(*crt)->next; *crt; 
					crt=tmp, tmp=&(*crt)->next){
			e=*crt;
			*crt=(*crt)->next;
			blst_destroy_entry(e);
			}
		}
		shm_free(dst_blst_hash);
		dst_blst_hash=0;
	}
	if (blst_mem_used){
		shm_free((void*)blst_mem_used);
		blst_mem_used=0;
	}
#ifdef DST_BLACKLIST_HOOKS
	destroy_blacklist_hooks();
#endif
}



int init_dst_blacklist()
{
	int ret;
#ifdef BLST_LOCK_PER_BUCKET
	int r;
#endif
	
	ret=-1;
#ifdef DST_BLACKLIST_HOOKS
	if (init_blacklist_hooks()!=0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
#endif
	blst_mem_used=shm_malloc(sizeof(*blst_mem_used));
	if (blst_mem_used==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
	*blst_mem_used=0;
	dst_blst_hash=shm_malloc(sizeof(struct dst_blst_lst_head) *
											DST_BLST_HASH_SIZE);
	if (dst_blst_hash==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
	memset(dst_blst_hash, 0, sizeof(struct dst_blst_lst_head) *
								DST_BLST_HASH_SIZE);
#ifdef BLST_LOCK_PER_BUCKET
	for (r=0; r<DST_BLST_HASH_SIZE; r++){
		if (lock_init(&dst_blst_hash[r].lock)==0){
			ret=-1;
			goto error;
		}
	}
#elif defined BLST_LOCK_SET
	blst_lock_set=lock_set_alloc(DST_BLST_HASH_SIZE);
	if (blst_lock_set==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
	if (lock_set_init(blst_lock_set)==0){
		lock_set_dealloc(blst_lock_set);
		blst_lock_set=0;
		ret=-1;
		goto error;
	}
#else /* BLST_ONE_LOCK */
	blst_lock=lock_alloc();
	if (blst_lock==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
	if (lock_init(blst_lock)==0){
		lock_dealloc(blst_lock);
		blst_lock=0;
		ret=-1;
		goto error;
	}
#endif /* BLST*LOCK*/
	blst_timer_h=timer_alloc();
	if (blst_timer_h==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
	/* fix options */
	blst_max_mem<<=10; /* in Kb */ /* TODO: test with 0 */
	if (blst_timer_interval){
		timer_init(blst_timer_h, blst_timer, 0 ,0); /* slow timer */
		if (timer_add(blst_timer_h, S_TO_TICKS(blst_timer_interval))<0){
			LOG(L_CRIT, "BUG: init_dst_blacklist: failed to add the timer\n");
			timer_free(blst_timer_h);
			blst_timer_h=0;
			goto error;
		}
	}
	return 0;
error:
	destroy_dst_blacklist();
	return ret;
}


/* must be called with the lock held
 * struct dst_blst_entry** head, struct dst_blst_entry* e */
#define dst_blacklist_lst_add(head, e)\
do{ \
	(e)->next=*(head); \
	*(head)=(e); \
}while(0)



/* must be called with the lock held
 * returns a pointer to the blacklist entry if found, 0 otherwise
 * it also deletes expired elements (expire<=now) as it searches
 * proto==PROTO_NONE = wildcard */
inline static struct dst_blst_entry* _dst_blacklist_lst_find(
												struct dst_blst_entry** head,
												struct ip_addr* ip,
												unsigned char proto,
												unsigned short port,
												ticks_t now)
{
	struct dst_blst_entry** crt;
	struct dst_blst_entry** tmp;
	struct dst_blst_entry* e;
	unsigned char type;
	
	type=(ip->af==AF_INET6)*BLST_IS_IPV6;
	for (crt=head, tmp=&(*head)->next; *crt; crt=tmp, tmp=&(*crt)->next){
		e=*crt;
		prefetch_loc_r((*crt)->next, 1);
		/* remove old expired entries */
		if ((s_ticks_t)(now-(*crt)->expire)>=0){
			*crt=(*crt)->next;
			*blst_mem_used-=DST_BLST_ENTRY_SIZE(*e);
			blst_destroy_entry(e);
		}else if ((e->port==port) && ((e->flags & BLST_IS_IPV6)==type) &&
				((e->proto==PROTO_NONE) || (proto==PROTO_NONE) ||
					(e->proto==proto)) && 
					(memcmp(ip->u.addr, e->ip, ip->len)==0)){
			return e;
		}
	}
	return 0;
}



/* frees all the expired entries until either there are no more of them
 *  or the total memory used is <= target (to free all of them use -1 for 
 *  targer)
 *  params:   target  - free expired entries until no more then taget memory 
 *                      is used  (use 0 to free all of them)
 *            delta   - consider an entry expired if it expires after delta
 *                      ticks from now
 *            timeout - exit after timeout ticks
 *
 *  returns: number of deleted entries
 *  This function should be called periodically from a timer
 */
inline static int dst_blacklist_clean_expired(unsigned int target,
									  ticks_t delta,
									  ticks_t timeout)
{
	static unsigned int start=0;
	unsigned int h;
	struct dst_blst_entry** crt;
	struct dst_blst_entry** tmp;
	struct dst_blst_entry* e;
	ticks_t start_time;
	ticks_t now;
	int no=0;
	int i;
	
	now=start_time=get_ticks_raw();
	for(h=start; h!=(start+DST_BLST_HASH_SIZE); h++){
		i=h%DST_BLST_HASH_SIZE;
		if (dst_blst_hash[i].first){
			LOCK_BLST(i);
			for (crt=&dst_blst_hash[i].first, tmp=&(*crt)->next;
					*crt; crt=tmp, tmp=&(*crt)->next){
				e=*crt;
				prefetch_loc_r((*crt)->next, 1);
				if ((s_ticks_t)(now+delta-(*crt)->expire)>=0){
					*crt=(*crt)->next;
					*blst_mem_used-=DST_BLST_ENTRY_SIZE(*e);
					blst_destroy_entry(e);
					BLST_HASH_STATS_DEC(i);
					no++;
					if (*blst_mem_used<=target){
						UNLOCK_BLST(i);
						goto skip;
					}
				}
			}
			UNLOCK_BLST(i);
			/* check for timeout only "between" hash cells */
			now=get_ticks_raw();
			if ((now-start_time)>=timeout){
				DBG("_dst_blacklist_clean_expired_unsafe: timeout: %d > %d\n",
						TICKS_TO_MS(now-start_time), TICKS_TO_MS(timeout));
				goto skip;
			}
		}
	}
skip:
	start=h; /* next time we start where we left */
	if (no){
		DBG("dst_blacklist_clean_expired, %d entries removed\n", no);
	}
	return no;
}



/* timer */
static ticks_t blst_timer(ticks_t ticks, struct timer_ln* tl, void* data)
{
	dst_blacklist_clean_expired(0, 0, 2); /*spend max. 2 ticks*/
	return (ticks_t)(-1);
}



/* adds a proto ip:port combination to the blacklist
 * returns 0 on success, -1 on error (blacklist full -- would use more then
 *  blst:_max_mem, or out of shm. mem.)
 */
inline static int dst_blacklist_add_ip(unsigned char err_flags, 
									unsigned char proto,
									struct ip_addr* ip, unsigned short port)
{
	int size;
	struct dst_blst_entry* e;
	unsigned short hash;
	ticks_t now;
	int ret;
	
	ret=0;
	if (ip->af==AF_INET){
		err_flags&=~BLST_IS_IPV6; /* make sure the ipv6 flag is reset */
		size=sizeof(struct dst_blst_entry);
	}else{
		err_flags|=BLST_IS_IPV6;
		size=sizeof(struct dst_blst_entry)+12 /* ipv6 addr - 4 */;
	}
	now=get_ticks_raw();
	hash=dst_blst_hash_no(proto, ip, port);
	/* check if the entry already exists */
	LOCK_BLST(hash);
		e=_dst_blacklist_lst_find(&dst_blst_hash[hash].first, ip, proto,
																port, now);
		if (e){
			e->flags|=err_flags;
			e->expire=now+S_TO_TICKS(blst_timeout); /* update the timeout */
		}else{
			if (unlikely((*blst_mem_used+size)>=blst_max_mem)){
				UNLOCK_BLST(hash);
				/* first try to free some memory  (~ 12%), but don't
				 * spend more then 250 ms*/
				dst_blacklist_clean_expired(*blst_mem_used/16*14, 0, 
															MS_TO_TICKS(250));
				if (unlikely(*blst_mem_used+size>=blst_max_mem)){
					ret=-1;
					goto error;
				}
				LOCK_BLST(hash);
			}
			e=shm_malloc(size);
			if (e==0){
				UNLOCK_BLST(hash);
				ret=E_OUT_OF_MEM;
				goto error;
			}
			*blst_mem_used+=size;
			e->flags=err_flags;
			e->proto=proto;
			e->port=port;
			memcpy(e->ip, ip->u.addr, ip->len);
			e->expire=now+S_TO_TICKS(blst_timeout); /* update the timeout */
			e->next=0;
			dst_blacklist_lst_add(&dst_blst_hash[hash].first, e);
			BLST_HASH_STATS_INC(hash);
		}
	UNLOCK_BLST(hash);
error:
	return ret;
}



/* if no blacklisted returns 0, else returns the blacklist flags */
inline static int dst_is_blacklisted_ip(unsigned char proto,
										struct ip_addr* ip,
										unsigned short port)
{
	struct dst_blst_entry* e;
	unsigned short hash;
	ticks_t now;
	int ret=0;
	
	ret=0;
	now=get_ticks_raw();
	hash=dst_blst_hash_no(proto, ip, port);
	if (unlikely(dst_blst_hash[hash].first)){
		LOCK_BLST(hash);
			e=_dst_blacklist_lst_find(&dst_blst_hash[hash].first, ip, proto,
										port, now);
			if (e){
				ret=e->flags;
			}
		UNLOCK_BLST(hash);
	}
	return ret;
}



int dst_blacklist_add(unsigned char err_flags,  struct dest_info* si)
{
	struct ip_addr ip;

#ifdef DST_BLACKLIST_HOOKS
	if (unlikely (blacklist_run_hooks(&blst_add_cb, si, &err_flags) ==
					DST_BLACKLIST_DENY))
		return 0;
#endif
	su2ip_addr(&ip, &si->to);
	return dst_blacklist_add_ip(err_flags, si->proto, &ip,
								su_getport(&si->to));
}



int dst_is_blacklisted(struct dest_info* si)
{
	struct ip_addr ip;
#ifdef DST_BLACKLIST_HOOKS
	unsigned char err_flags;
	int action;
#endif
	su2ip_addr(&ip, &si->to);

#ifdef DST_BLACKLIST_HOOKS
	err_flags=0;
	if (unlikely((action=(blacklist_run_hooks(&blst_search_cb, si, &err_flags))
					) != DST_BLACKLIST_CONTINUE)){
		if (action==DST_BLACKLIST_DENY)
			return 0;
		else  /* if (action==DST_BLACKLIST_ACCEPT) */
			return err_flags;
	}
#endif
	return dst_is_blacklisted_ip(si->proto, &ip, su_getport(&si->to));
}



/* rpc functions */
void dst_blst_mem_info(rpc_t* rpc, void* ctx)
{
	rpc->add(ctx, "dd",  *blst_mem_used, blst_max_mem);
}



static char* get_proto_name(unsigned char proto)
{
	switch(proto){
		case PROTO_NONE:
			return "*";
		case PROTO_UDP:
			return "udp";
		case PROTO_TCP:
			return "tcp";
		case PROTO_TLS:
			return "tls";
		default:
			return "unknown";
	}
}



/* only for debugging, it helds the lock too long for "production" use */
void dst_blst_debug(rpc_t* rpc, void* ctx)
{
	int h;
	struct dst_blst_entry* e;
	ticks_t now;
	struct ip_addr ip;
	
	now=get_ticks_raw();
		for(h=0; h<DST_BLST_HASH_SIZE; h++){
			LOCK_BLST(h);
			for(e=dst_blst_hash[h].first; e; e=e->next){
				dst_blst_entry2ip(&ip, e);
				rpc->add(ctx, "ssddd", get_proto_name(e->proto), 
										ip_addr2a(&ip), e->port, 
										(s_ticks_t)(now-e->expire)<=0?
										TICKS_TO_S(e->expire-now):
										-TICKS_TO_S(now-e->expire) ,
										e->flags);
			}
			UNLOCK_BLST(h);
		}
}

/* only for debugging, it helds the lock too long for "production" use */
void dst_blst_hash_stats(rpc_t* rpc, void* ctx)
{
	int h;
	struct dst_blst_entry* e;
#ifdef BLST_HASH_STATS
	int n;
	
	n=0;
#endif
	
		for(h=0; h<DST_BLST_HASH_SIZE; h++){
#ifdef BLST_HASH_STATS
			LOCK_BLST(h);
			for(e=dst_blst_hash[h].first; e; e=e->next) n++;
			UNLOCK_BLST(h);
			rpc->add(ctx, "dd", h, n);
#else
			rpc->add(ctx, "dd", h, dst_blst_hash[h].entries);
#endif
		}
}


#endif /* USE_DST_BLACKLIST */


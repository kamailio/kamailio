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
 * \brief Kamailio core :: resolver related functions
 * \ingroup core
 * Module: \ref core
 */


#ifdef USE_DST_BLACKLIST

#include "dst_blacklist.h"
#include "globals.h"
#include "cfg_core.h"
#include "mem/shm_mem.h"
#include "hashes.h"
#include "locking.h"
#include "timer.h"
#include "timer_ticks.h"
#include "ip_addr.h"
#include "error.h"
#include "rpc.h"
#include "compiler_opt.h"
#include "resolve.h" /* for str2ip */
#ifdef USE_DST_BLACKLIST_STATS
#include "pt.h"
#endif




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

int dst_blacklist_init=1; /* if 0, the dst blacklist is not initialized at startup */
static struct timer_ln* blst_timer_h=0;

static volatile unsigned int* blst_mem_used=0;
unsigned int blst_timer_interval=DEFAULT_BLST_TIMER_INTERVAL;
struct dst_blst_lst_head* dst_blst_hash=0;

#ifdef USE_DST_BLACKLIST_STATS
struct t_dst_blacklist_stats* dst_blacklist_stats=0;
#endif

/* blacklist per protocol event ignore mask array */
unsigned blst_proto_imask[PROTO_LAST+1];

#ifdef DST_BLACKLIST_HOOKS

/* there 2 types of callbacks supported: on add new entry to the blacklist
 *  (DST_BLACKLIST_ADD_CB) and on blacklist search (DST_BLACKLIST_SEARCH_CB).
 *  Both of them take a struct dest_info*, a flags pointer(unsigned char*),
 *  and a struct sip_msg* as parameters. The flags can be changed.
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
	LM_ERR("failure initializing internal lists\n");
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

	if (dst_blacklist_init==0) {
		LM_ERR("blacklist is turned off, "
			"the hook cannot be registered\n");
		goto error;
	}

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
							struct dest_info* si, unsigned char* flags,
							struct sip_msg* msg)
{
	int r;
	int ret;

	ret=DST_BLACKLIST_CONTINUE; /* default, if no hook installed accept
								blacklist operation */
	if (likely(cb_lst->last_idx==0))
		return ret;
	for (r=0; r<cb_lst->last_idx; r++){
		ret=cb_lst->hooks[r].on_blst_action(si, flags, msg);
		if (ret!=DST_BLACKLIST_CONTINUE) break;
	}
	return ret;
}


#endif /* DST_BLACKLIST_HOOKS */


/** init per protocol blacklist event ignore masks.
 * @return 0 on success, < 0 on error.
 */
int blst_init_ign_masks(void)
{
	if ((PROTO_UDP > PROTO_LAST) || (PROTO_TCP > PROTO_LAST) ||
		(PROTO_TLS > PROTO_LAST) || (PROTO_SCTP > PROTO_LAST)){
		BUG("protocol array too small\n");
		return -1;
	}
	blst_proto_imask[PROTO_UDP]=cfg_get(core, core_cfg, blst_udp_imask);
	blst_proto_imask[PROTO_TCP]=cfg_get(core, core_cfg, blst_tcp_imask);
	blst_proto_imask[PROTO_TLS]=cfg_get(core, core_cfg, blst_tls_imask);
	blst_proto_imask[PROTO_SCTP]=cfg_get(core, core_cfg, blst_sctp_imask);
	blst_proto_imask[PROTO_NONE]=blst_proto_imask[PROTO_UDP];
	return 0;
}



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
	}else
	{
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
	struct dst_blst_entry* e;

	if (blst_timer_h){
		timer_del(blst_timer_h);
		timer_free(blst_timer_h);
		blst_timer_h=0;
	}
#ifdef BLST_LOCK_PER_BUCKET
	if (dst_blst_hash)
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
			crt=&dst_blst_hash[r].first;
			while(*crt){
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

#ifdef USE_DST_BLACKLIST_STATS
	if (dst_blacklist_stats)
		shm_free(dst_blacklist_stats);
#endif
}



int init_dst_blacklist()
{
	int ret;
#ifdef BLST_LOCK_PER_BUCKET
	int r;
#endif

	if (dst_blacklist_init==0) {
		/* the dst blacklist is turned off */
		default_core_cfg.use_dst_blacklist=0;
		return 0;
	}

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
	default_core_cfg.blst_max_mem<<=10; /* in Kb */ /* TODO: test with 0 */
	if (blst_timer_interval){
		timer_init(blst_timer_h, blst_timer, 0 ,0); /* slow timer */
		if (timer_add(blst_timer_h, S_TO_TICKS(blst_timer_interval))<0){
			LM_CRIT("failed to add the timer\n");
			timer_free(blst_timer_h);
			blst_timer_h=0;
			goto error;
		}
	}
	if (blst_init_ign_masks() < 0){
		ret=E_BUG;
		goto error;
	}
	return 0;
error:
	destroy_dst_blacklist();
	return ret;
}

#ifdef USE_DST_BLACKLIST_STATS
int init_dst_blacklist_stats(int iproc_num)
{
	/* do not initialize the stats array if the dst blacklist will not be used */
	if (dst_blacklist_init==0) return 0;

	/* if it is already initialized */
	if (dst_blacklist_stats)
		shm_free(dst_blacklist_stats);

	dst_blacklist_stats=shm_malloc(sizeof(*dst_blacklist_stats) * iproc_num);
	if (dst_blacklist_stats==0){
		return E_OUT_OF_MEM;
	}
	memset(dst_blacklist_stats, 0, sizeof(*dst_blacklist_stats) * iproc_num);

	return 0;
}
#endif

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
												unsigned short hash,
												struct ip_addr* ip,
												unsigned char proto,
												unsigned short port,
												ticks_t now)
{
	struct dst_blst_entry** crt;
	struct dst_blst_entry** tmp;
	struct dst_blst_entry* e;
	struct dst_blst_entry** head;
	unsigned char type;

	head=&dst_blst_hash[hash].first;
	type=(ip->af==AF_INET6)*BLST_IS_IPV6;
	for (crt=head, tmp=&(*head)->next; *crt; crt=tmp, tmp=&(*crt)->next){
		e=*crt;
		prefetch_loc_r((*crt)->next, 1);
		/* remove old expired entries */
		if ((s_ticks_t)(now-(*crt)->expire)>=0){
			*crt=(*crt)->next;
			tmp=crt;
			*blst_mem_used-=DST_BLST_ENTRY_SIZE(*e);
			BLST_HASH_STATS_DEC(hash);
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



/* must be called with the lock held
 * returns 1 if a matching entry was deleted, 0 otherwise
 * it also deletes expired elements (expire<=now) as it searches
 * proto==PROTO_NONE = wildcard */
inline static int _dst_blacklist_del(
												unsigned short hash,
												struct ip_addr* ip,
												unsigned char proto,
												unsigned short port,
												ticks_t now)
{
	struct dst_blst_entry** crt;
	struct dst_blst_entry** tmp;
	struct dst_blst_entry* e;
	struct dst_blst_entry** head;
	unsigned char type;
	
	head=&dst_blst_hash[hash].first;
	type=(ip->af==AF_INET6)*BLST_IS_IPV6;
	for (crt=head, tmp=&(*head)->next; *crt; crt=tmp, tmp=&(*crt)->next){
		e=*crt;
		prefetch_loc_r((*crt)->next, 1);
		/* remove old expired entries */
		if ((s_ticks_t)(now-(*crt)->expire)>=0){
			*crt=(*crt)->next;
			tmp=crt;
			*blst_mem_used-=DST_BLST_ENTRY_SIZE(*e);
			BLST_HASH_STATS_DEC(hash);
			blst_destroy_entry(e);
		}else if ((e->port==port) && ((e->flags & BLST_IS_IPV6)==type) &&
				((e->proto==PROTO_NONE) || (proto==PROTO_NONE) ||
					(e->proto==proto)) && 
					(memcmp(ip->u.addr, e->ip, ip->len)==0)){
			*crt=(*crt)->next;
			tmp=crt;
			*blst_mem_used-=DST_BLST_ENTRY_SIZE(*e);
			BLST_HASH_STATS_DEC(hash);
			blst_destroy_entry(e);
			return 1;
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
					tmp=crt;
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
				LM_DBG("timeout: %d > %d\n",
						TICKS_TO_MS(now-start_time), TICKS_TO_MS(timeout));
				goto skip;
			}
		}
	}
skip:
	start=h; /* next time we start where we left */
	if (no){
		LM_DBG("%d entries removed\n", no);
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
									struct ip_addr* ip, unsigned short port,
									ticks_t timeout)
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
		e=_dst_blacklist_lst_find(hash, ip, proto, port, now);
		if (e){
			e->flags|=err_flags;
			e->expire=now+timeout; /* update the timeout */
		}else{
			if (unlikely((*blst_mem_used+size) >=
					cfg_get(core, core_cfg, blst_max_mem))){
#ifdef USE_DST_BLACKLIST_STATS
				dst_blacklist_stats[process_no].bkl_lru_cnt++;
#endif
				UNLOCK_BLST(hash);
				/* first try to free some memory  (~ 12%), but don't
				 * spend more then 250 ms*/
				dst_blacklist_clean_expired(*blst_mem_used/16*14, 0,
															MS_TO_TICKS(250));
				if (unlikely(*blst_mem_used+size >=
						cfg_get(core, core_cfg, blst_max_mem))){
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
			e->expire=now+timeout; /* update the timeout */
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
	int ret;

	ret=0;
	now=get_ticks_raw();
	hash=dst_blst_hash_no(proto, ip, port);
	if (unlikely(dst_blst_hash[hash].first)){
		LOCK_BLST(hash);
			e=_dst_blacklist_lst_find(hash, ip, proto, port, now);
			if (e){
				ret=e->flags;
			}
		UNLOCK_BLST(hash);
	}
	return ret;
}



/** add dst to the blacklist, specifying the timeout.
 * @param err_flags - reason (bitmap)
 * @param si - destination (protocol, ip and port)
 * @param msg - sip message that triggered the blacklisting (can be 0 if 
 *               not known)
 * @param timeout - timeout in ticks
 * @return 0 on success, -1 on error
 */
int dst_blacklist_force_add_to(unsigned char err_flags,  struct dest_info* si,
								struct sip_msg* msg, ticks_t timeout)
{
	struct ip_addr ip;

#ifdef DST_BLACKLIST_HOOKS
	if (unlikely (blacklist_run_hooks(&blst_add_cb, si, &err_flags, msg) ==
					DST_BLACKLIST_DENY))
		return 0;
#endif
	su2ip_addr(&ip, &si->to);
	return dst_blacklist_add_ip(err_flags, si->proto, &ip,
								su_getport(&si->to), timeout);
}



/** add dst to the blacklist, specifying the timeout.
 * (like @function dst_blacklist_force_add_to)= above, but uses 
 * (proto, sockaddr_union) instead of struct dest_info)
 */
int dst_blacklist_force_su_to(unsigned char err_flags, unsigned char proto,
								union sockaddr_union* dst,
								struct sip_msg* msg, ticks_t timeout)
{
	struct ip_addr ip;
#ifdef DST_BLACKLIST_HOOKS
	struct dest_info si;
	
	init_dest_info(&si);
	si.to=*dst;
	si.proto=proto;
	if (unlikely (blacklist_run_hooks(&blst_add_cb, &si, &err_flags, msg) ==
					DST_BLACKLIST_DENY))
		return 0;
#endif
	su2ip_addr(&ip, dst);
	return dst_blacklist_add_ip(err_flags, proto, &ip,
								su_getport(dst), timeout);
}



int dst_is_blacklisted(struct dest_info* si, struct sip_msg* msg)
{
	int ires;
	struct ip_addr ip;
#ifdef DST_BLACKLIST_HOOKS
	unsigned char err_flags;
	int action;
#endif
	su2ip_addr(&ip, &si->to);

#ifdef DST_BLACKLIST_HOOKS
	err_flags=0;
	if (unlikely((action=(blacklist_run_hooks(&blst_search_cb, si, &err_flags, msg))
					) != DST_BLACKLIST_CONTINUE)){
		if (action==DST_BLACKLIST_DENY)
			return 0;
		else  /* if (action==DST_BLACKLIST_ACCEPT) */
			return err_flags;
	}
#endif
	ires=dst_is_blacklisted_ip(si->proto, &ip, su_getport(&si->to));
#ifdef USE_DST_BLACKLIST_STATS
	if (ires)
		dst_blacklist_stats[process_no].bkl_hit_cnt++;
#endif
	return ires;
}



/* returns 1 if the entry was deleted, 0 if not found */
int dst_blacklist_del(struct dest_info* si, struct sip_msg* msg)
{
	unsigned short hash;
	struct ip_addr ip;
	ticks_t now;
	int ret;
	unsigned short port;
	
	ret=0;
	su2ip_addr(&ip, &si->to);
	port=su_getport(&si->to);
	now=get_ticks_raw();
	hash=dst_blst_hash_no(si->proto, &ip, port);
	if (unlikely(dst_blst_hash[hash].first)){
		LOCK_BLST(hash);
			ret=_dst_blacklist_del(hash, &ip, si->proto, port, now);
		UNLOCK_BLST(hash);
	}
	return ret;
}



/* rpc functions */
void dst_blst_mem_info(rpc_t* rpc, void* ctx)
{
	if (!cfg_get(core, core_cfg, use_dst_blacklist)){
		rpc->fault(ctx, 500, "dst blacklist support disabled");
		return;
	}
	rpc->add(ctx, "dd",  *blst_mem_used, cfg_get(core, core_cfg, blst_max_mem));
}




#ifdef USE_DST_BLACKLIST_STATS

static unsigned long  stat_sum(int ivar, int breset) {
	unsigned long isum=0;
	int i1=0;

	for (; i1 < get_max_procs(); i1++)
		switch (ivar) {
			case 0:
				isum+=dst_blacklist_stats[i1].bkl_hit_cnt;
				if (breset)
					dst_blacklist_stats[i1].bkl_hit_cnt=0;
				break;
			case 1:
				isum+=dst_blacklist_stats[i1].bkl_lru_cnt;
				if (breset)
					dst_blacklist_stats[i1].bkl_lru_cnt=0;
				break;
		}

		return isum;
}


void dst_blst_stats_get(rpc_t* rpc, void* c)
{
	char *name=NULL;
	void *handle;
	int found=0,i=0;
	int reset=0;
	char* dst_blacklist_stats_names[] = {
		"bkl_hit_cnt",
		"bkl_lru_cnt",
		NULL
	};
	
	if (!cfg_get(core, core_cfg, use_dst_blacklist)){
		rpc->fault(c, 500, "dst blacklist support disabled");
		return;
	}
	if (rpc->scan(c, "s", &name) < 0)
		return;
	if (rpc->scan(c, "d", &reset) < 0)
		return;
	if (!strcasecmp(name, DST_BLACKLIST_ALL_STATS)) {
		/* dump all the dns cache stat values */
		rpc->add(c, "{", &handle);
		for (i=0; dst_blacklist_stats_names[i]; i++)
			rpc->struct_add(handle, "d",
							dst_blacklist_stats_names[i],
							stat_sum(i, reset));

		found=1;
	} else {
		for (i=0; dst_blacklist_stats_names[i]; i++)
			if (!strcasecmp(dst_blacklist_stats_names[i], name)) {
			rpc->add(c, "{", &handle);
			rpc->struct_add(handle, "d",
							dst_blacklist_stats_names[i],
							stat_sum(i, reset));
			found=1;
			break;
			}
	}
	if(!found)
		rpc->fault(c, 500, "unknown dst blacklist stat parameter");

	return;
}
#endif /* USE_DST_BLACKLIST_STATS */

/* only for debugging, it helds the lock too long for "production" use */
void dst_blst_debug(rpc_t* rpc, void* ctx)
{
	int h;
	struct dst_blst_entry* e;
	ticks_t now;
	struct ip_addr ip;

	if (!cfg_get(core, core_cfg, use_dst_blacklist)){
		rpc->fault(ctx, 500, "dst blacklist support disabled");
		return;
	}
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
	if (!cfg_get(core, core_cfg, use_dst_blacklist)){
		rpc->fault(ctx, 500, "dst blacklist support disabled");
		return;
	}
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

/* dumps the content of the blacklist in a human-readable format */
void dst_blst_view(rpc_t* rpc, void* ctx)
{
	int h;
	int expires;
	struct dst_blst_entry* e;
	ticks_t now;
	struct ip_addr ip;

	if (!cfg_get(core, core_cfg, use_dst_blacklist)){
		rpc->fault(ctx, 500, "dst blacklist support disabled");
		return;
	}
	now=get_ticks_raw();
	for(h=0; h<DST_BLST_HASH_SIZE; h++) {
		LOCK_BLST(h);
		for(e=dst_blst_hash[h].first; e; e=e->next) {
			expires = (s_ticks_t)(now-e->expire)<=0?
			           TICKS_TO_S(e->expire-now): -TICKS_TO_S(now-e->expire);
			/* don't include expired entries into view report */
			if (expires < 0) {
				continue;
			}
			dst_blst_entry2ip(&ip, e);
			rpc->rpl_printf(ctx, "{\n    protocol: %s", get_proto_name(e->proto));
			rpc->rpl_printf(ctx, "    ip: %s", ip_addr2a(&ip));
			rpc->rpl_printf(ctx, "    port: %d", e->port);
			rpc->rpl_printf(ctx, "    expires in (s): %d", expires); 
			rpc->rpl_printf(ctx, "    flags: %d\n}", e->flags);
		}
		UNLOCK_BLST(h);
	}
}


/* deletes all the entries from the blacklist except the permanent ones
 * (which are marked with BLST_PERMANENT)
 */
void dst_blst_flush(void)
{
	int h;
	struct dst_blst_entry* e;
	struct dst_blst_entry** crt;
	struct dst_blst_entry** tmp;

	for(h=0; h<DST_BLST_HASH_SIZE; h++){
		LOCK_BLST(h);
		for (crt=&dst_blst_hash[h].first, tmp=&(*crt)->next;
				*crt; crt=tmp, tmp=&(*crt)->next){
			e=*crt;
			prefetch_loc_r((*crt)->next, 1);
			if (!(e->flags &  BLST_PERMANENT)){
				*crt=(*crt)->next;
				tmp=crt;
				*blst_mem_used-=DST_BLST_ENTRY_SIZE(*e);
				blst_destroy_entry(e);
				BLST_HASH_STATS_DEC(h);
			}
		}
		UNLOCK_BLST(h);
	}
}

/* rpc wrapper function for dst_blst_flush() */
void dst_blst_delete_all(rpc_t* rpc, void* ctx)
{
	if (!cfg_get(core, core_cfg, use_dst_blacklist)){
		rpc->fault(ctx, 500, "dst blacklist support disabled");
		return;
	}
	dst_blst_flush();
}

/* Adds a new entry to the blacklist */
void dst_blst_add(rpc_t* rpc, void* ctx)
{
	str ip;
	int port, proto, flags;
	unsigned char err_flags;
	struct ip_addr *ip_addr;

	if (!cfg_get(core, core_cfg, use_dst_blacklist)){
		rpc->fault(ctx, 500, "dst blacklist support disabled");
		return;
	}
	if (rpc->scan(ctx, "Sddd", &ip, &port, &proto, &flags) < 4)
		return;

	err_flags = (unsigned char)flags;
	/* sanity checks */
	if ((unsigned char)proto > PROTO_SCTP) {
		rpc->fault(ctx, 400, "Unknown protocol");
		return;
	}

	if (err_flags & BLST_IS_IPV6) {
		/* IPv6 address is specified */
		ip_addr = str2ip6(&ip);
	} else {
		/* try IPv4 first, than IPv6 */
		ip_addr = str2ip(&ip);
		if (!ip_addr) {
			ip_addr = str2ip6(&ip);
			err_flags |= BLST_IS_IPV6;
		}
	}
	if (!ip_addr) {
		rpc->fault(ctx, 400, "Malformed ip address");
		return;
	}

	if (dst_blacklist_add_ip(err_flags, proto, ip_addr, port, 
				    S_TO_TICKS(cfg_get(core, core_cfg, blst_timeout))))
		rpc->fault(ctx, 400, "Failed to add the entry to the blacklist");
}

/* fixup function for use_dst_blacklist
 * verifies that dst_blacklist_init is set to 1
 */
int use_dst_blacklist_fixup(void *handle, str *gname, str *name, void **val)
{
	if ((int)(long)(*val) && !dst_blacklist_init) {
		LM_ERR("dst blacklist is turned off by dst_blacklist_init=0, "
			"it cannot be enabled runtime.\n");
		return -1;
	}
	return 0;
}

/* KByte to Byte conversion */
int blst_max_mem_fixup(void *handle, str *gname, str *name, void **val)
{
	unsigned int	u;

	u = ((unsigned int)(long)(*val))<<10;
	(*val) = (void *)(long)u;
	return 0;
}



/** re-inint per child blst_proto_ign_mask array. */
void blst_reinit_ign_masks(str* gname, str* name)
{
	blst_init_ign_masks();
}


#endif /* USE_DST_BLACKLIST */


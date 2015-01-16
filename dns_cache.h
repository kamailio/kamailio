/*
 * resolver/dns related functions, dns cache and failover
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


/**
 * @file
 * @brief Kamailio core :: resolver/dns related functions, dns cache and failover
 * @author andrei
 * @ingroup core
 * Module: @ref core
 */



#ifndef __dns_cache_h
#define __dns_cache_h

#include "str.h"
#include "config.h" /* MAX_BRANCHES */
#include "timer.h"
#include "ip_addr.h"
#include "atomic_ops.h"
#include "resolve.h"


#if defined(USE_DNS_FAILOVER) && !defined(USE_DNS_CACHE)
#error "DNS FAILOVER requires DNS CACHE support (define USE_DNS_CACHE)"
#endif

#if defined(DNS_WATCHDOG_SUPPORT) && !defined(USE_DNS_CACHE)
#error "DNS WATCHDOG requires DNS CACHE support (define USE_DNS_CACHE)"
#endif

#define DEFAULT_DNS_NEG_CACHE_TTL 60 /* 1 min. */
#define DEFAULT_DNS_CACHE_MIN_TTL 0 /* (disabled) */
#define DEFAULT_DNS_CACHE_MAX_TTL ((unsigned int)(-1)) /* (maxint) */
#define DEFAULT_DNS_MAX_MEM 500 /* 500 Kb */

/** @brief uncomment the define below for SRV weight based load balancing */
#define DNS_SRV_LB

#define DNS_LU_LST

/** @brief dns functions return them as negative values (e.g. return -E_DNS_NO_IP)
 *
 * listed in the order of importance ( if more errors, only the most important
 * is returned)
 */
enum dns_errors{
					E_DNS_OK=0,
					E_DNS_EOR, /**< no more records (not an error)
					              -- returned only by the dns_resolve*
								  functions when called iteratively,; it
								  signals the end of the ip/records list */
					E_DNS_UNKNOWN /**< unkown error */,
					E_DNS_INTERNAL_ERR /**< internal error */,
					E_DNS_BAD_SRV_ENTRY,
					E_DNS_NO_SRV /**< unresolvable srv record */,
					E_DNS_BAD_IP_ENTRY,
					E_DNS_NO_IP /**< unresolvable a or aaaa records*/,
					E_DNS_BAD_IP /**< the ip is invalid */,
					E_DNS_BLACKLIST_IP /**< the ip is blacklisted */,
					E_DNS_NAME_TOO_LONG /**< try again with a shorter name */,
					E_DNS_AF_MISMATCH /**< ipv4 or ipv6 only requested, but
										 name contains an ip addr. of the
										 opossite type */ ,
					E_DNS_NO_NAPTR /**< unresolvable naptr record */,
					E_DNS_CRITICAL /**< critical error, marks the end
									  of the error table (always last) */
};



/** @brief return a short string, printable error description (err <=0) */
const char* dns_strerror(int err);

/** @brief dns entry flags,
 * shall be on the power of 2 */
/*@{ */
#define DNS_FLAG_BAD_NAME	1 /**< error flag: unresolvable */
#define DNS_FLAG_PERMANENT	2 /**< permanent record, never times out,
					never deleted, never overwritten
					unless explicitely requested */
/*@} */

/** @name dns requests flags */
/*@{ */
#define DNS_NO_FLAGS	0
#define DNS_IPV4_ONLY	1
#define DNS_IPV6_ONLY	2
#define DNS_IPV6_FIRST	4
#define DNS_SRV_RR_LB	8  /**< SRV RR weight based load balancing */
#define DNS_TRY_NAPTR	16 /**< enable naptr lookup */
/*@} */


/** @name ip blacklist error flags */
/*@{ */
#define IP_ERR_BAD_DST      2 /* destination is marked as bad (e.g. bad ip) */
#define IP_ERR_SND          3 /* send error while using this as destination */
#define IP_ERR_TIMEOUT      4 /* timeout waiting for a response */
#define IP_ERR_TCP_CON      5 /* could not establish tcp connection */
/*@} */


/** @brief stripped down dns rr 
	@note name, type and class are not needed, contained in struct dns_query */
struct dns_rr{
	struct dns_rr* next;
	void* rdata; /**< depends on the type */
	ticks_t expire; /**< = ttl + crt_time */
};



#ifdef DNS_LU_LST
struct dns_lu_lst{  /* last used ordered list */
	struct dns_lu_lst* next;
	struct dns_lu_lst* prev;
};
#endif

struct dns_hash_entry{
	/* hash table links */
	struct dns_hash_entry* next;
	struct dns_hash_entry* prev;
#ifdef DNS_LU_LST
	struct dns_lu_lst last_used_lst;
#endif
	struct dns_rr* rr_lst;
	atomic_t refcnt;
	ticks_t last_used;
	ticks_t expire; /* when the whole entry will expire */
	int total_size;
	unsigned short type;
	unsigned char ent_flags; /* entry flags: unresolvable/permanent */
	unsigned char name_len; /* can be maximum 255 bytes */
	char name[1]; /* variable length, name, null terminated
	                 (actual lenght = name_len +1)*/
};


/* to fit in the limit of MAX_BRANCHES */
#if MAX_BRANCHES_LIMIT < 32
typedef unsigned int srv_flags_t;
#else
typedef unsigned long long srv_flags_t;
#endif

struct dns_srv_handle{
	struct dns_hash_entry* srv; /**< srv entry */
	struct dns_hash_entry* a;   /**< a or aaaa current entry */
#ifdef DNS_SRV_LB
	srv_flags_t srv_tried_rrs;
#endif
	unsigned short port; /**< current port */
	unsigned char srv_no; /**< current record no. in the srv entry */
	unsigned char ip_no;  /**< current record no. in the a/aaaa entry */
	unsigned char proto;  /**< protocol number */
};



const char* dns_strerror(int err);

void fix_dns_flags(str *gname, str *name);
int use_dns_failover_fixup(void *handle, str *gname, str *name, void **val);
int use_dns_cache_fixup(void *handle, str *gname, str *name, void **val);
int dns_cache_max_mem_fixup(void *handle, str *gname, str *name, void **val);
int init_dns_cache(void);
#ifdef USE_DNS_CACHE_STATS
int init_dns_cache_stats(int iproc_num);
#define DNS_CACHE_ALL_STATS "dc_all_stats"
#endif
void destroy_dns_cache(void);


void dns_hash_put(struct dns_hash_entry* e);
void dns_hash_put_shm_unsafe(struct dns_hash_entry* e);

inline static void dns_srv_handle_put(struct dns_srv_handle* h)
{
	if (h){
		if (h->srv){
			dns_hash_put(h->srv);
			h->srv=0;
		}
		if (h->a){
			dns_hash_put(h->a);
			h->a=0;
		}
	}
}



/** @brief use it when copying, it manually increases the ref cound */
inline static void dns_srv_handle_ref(struct dns_srv_handle *h)
{
	if (h){
		if (h->srv)
			atomic_inc(&h->srv->refcnt);
		if (h->a)
			atomic_inc(&h->a->refcnt);
	}
}



/** @brief safe copy increases the refcnt, src must not change while in this function
 * WARNING: the copy must be dns_srv_handle_put ! */
inline static void dns_srv_handle_cpy(struct dns_srv_handle* dst,
										struct dns_srv_handle* src)
{
	dns_srv_handle_ref(src);
	*dst=*src;
}



/** @brief same as above but assume shm_lock held (for internal tm use only) */
inline static void dns_srv_handle_put_shm_unsafe(struct dns_srv_handle* h)
{
	if (h){
		if (h->srv){
			dns_hash_put_shm_unsafe(h->srv);
			h->srv=0;
		}
		if (h->a){
			dns_hash_put_shm_unsafe(h->a);
			h->a=0;
		}
	}
}



/** @brief get "next" ip next time a dns_srv_handle function is called
 * params: h   - struct dns_srv_handler
 *         err - return code of the last dns_*_resolve* call
 * returns: 0 if it doesn't make sense to try another record,
 * 1 otherwise
 */
inline static int dns_srv_handle_next(struct dns_srv_handle* h, int err)
{
	if (err<0) return 0;
	h->ip_no++;
	return (h->srv || h->a);
}



inline static void dns_srv_handle_init(struct dns_srv_handle* h)
{
	h->srv=h->a=0;
	h->srv_no=h->ip_no=0;
	h->port=0;
	h->proto=0;
#ifdef DNS_SRV_LB
	h->srv_tried_rrs=0;
#endif
}



/** @brief performes a srv query on name
 * Params:  name  - srv query target (e.g. _sip._udp.foo.bar)
 *          ip    - result: first good ip found
 *          port  - result: corresponding port number
 *          flags - resolve options (like ipv4 only, ipv6 prefered a.s.o)
 * Returns: < 0 on error (can be passed to dns_strerror(), 0 on success
 */
int dns_srv_get_ip(str* name, struct ip_addr* ip, unsigned short* port,
					int flags);

/** @brief performs an A, AAAA (or both) query/queries
 * Params:  name  - query target (e.g. foo.bar)
 *          ip    - result: first good ip found
 *          flags - resolve options (like ipv4 only, ipv6 prefered a.s.o)
 * Returns: < 0 on error (can be passed to dns_strerror(), 0 on success
 */
int dns_get_ip(str* name, struct ip_addr* ip, int flags);

struct hostent* dns_srv_get_he(str* name, unsigned short* port, int flags);
struct hostent* dns_get_he(str* name, int flags);


/** @brief resolve name to an ip, using srv record. Can be called multiple times
 * to iterate on all the possible ips, e.g :
 * dns_srv_handle_init(h);
 * ret_code=dns_sip_resolve(h,...);
 *  while( dns_srv_handle_next(h, ret_code){ ret_code=dns_sip_resolve(h...); }
 * dns_srv_handle_put(h);
 * WARNING: dns_srv_handle_init() must be called to initialize h and
 *  dns_srv_handle_put(h) must be called when h is no longer needed
 */
int dns_sip_resolve(struct dns_srv_handle* h,  str* name, struct ip_addr* ip,
					unsigned short* port, char* proto, int flags);

/** @brief same as above, but fills su intead of changing port and filling an ip */
inline static int dns_sip_resolve2su(struct dns_srv_handle* h,
									 union sockaddr_union* su,
									 str* name, unsigned short port,
									 char* proto, int flags)
{
	struct ip_addr ip;
	int ret;

	ret=dns_sip_resolve(h, name, &ip, &port, proto, flags);
	if (ret>=0)
		init_su(su, &ip, port);
	return ret;
}

/** @brief Delete all the entries from the cache.
 * If del_permanent is 0, then only the
 * non-permanent entries are deleted.
 */
void dns_cache_flush(int del_permanent);

#ifdef DNS_WATCHDOG_SUPPORT
/** @brief sets the state of the DNS servers:
 * 1: at least one server is up
 * 0: all the servers are down
 */
void dns_set_server_state(int state);

/** @brief returns the state of the DNS servers */
int dns_get_server_state(void);
#endif /* DNS_WATCHDOG_SUPPORT */

/** @brief Adds a new record to the cache.
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
			int flags);

/** @brief Delete a single record from the cache,
 * i.e. the record with the same name and value
 * (ip address in case of A/AAAA record, name in case of SRV record).
 *
 * Currently only A, AAAA, and SRV records are supported.
 */
int dns_cache_delete_single_record(unsigned short type,
			str *name,
			str *value,
			int flags);


#endif

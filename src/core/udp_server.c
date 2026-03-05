/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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


/** Kamailio core :: udp send and loop-receive functions.
 * @file udp_server.c
 * @ingroup core
 * Module: @ref core
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <errno.h>
#include <arpa/inet.h>
#ifdef __linux__
#include <linux/types.h>
#include <linux/errqueue.h>
#endif
#include <pthread.h>


#include "udp_server.h"
#include "compiler_opt.h"
#include "globals.h"
#include "config.h"
#include "dprint.h"
#include "receive.h"
#include "mem/mem.h"
#include "pt.h"
#include "action.h"
#include "ip_addr.h"
#include "socket_info.h"
#include "cfg/cfg_struct.h"
#include "events.h"
#include "async_task.h"
#include "stun.h"
#ifdef USE_RAW_SOCKS
#include "raw_sock.h"
#endif /* USE_RAW_SOCKS */
#ifdef USE_MCAST
#include <net/if.h>
#endif /* USE_MCAST */
#include "locking.h"
#include "rpc.h"
#include "rpc_lookup.h"


#define UDP_ACCEPT_PROXY_HAPROXY 1
#define UDP_ACCEPT_PROXY_SIMPLE 2
#define UDP_ACCEPT_PROXY_BOTH 3

int ksr_udp_accept_proxy = 0;

#define UDP_PROXY_HT_SIZE 4093
#define UDP_PROXY_HT_LIFETIME 7200 // 2 hours

struct udp_ht_link
{
	struct udp_ht_link *next;
	struct udp_ht_link *prev;
};

struct udp_ht_entry
{
	union sockaddr_union real_peer;
	union sockaddr_union proxy_peer;

	time_t accessed; // last access time

	// each entry is linked in two hash tables
	// using a doubly-linked circular list
	struct udp_ht_link real_link;
	struct udp_ht_link proxy_link;
};

gen_lock_t *udp_proxy_ht_lock;
struct udp_ht_link
		*udp_proxy_ht_real; // hashed by real_peer, linked by real_link
struct udp_ht_link
		*udp_proxy_ht_proxy; // hashed by proxy_peer, linked by proxy_link


static unsigned int sockaddr_hash(const union sockaddr_union *addr)
{
	unsigned int len = sockaddru_len(*addr);
	const unsigned char *ptr = (const unsigned char *)addr;
	unsigned int hash = 0;

	while(len >= sizeof(unsigned int)) {
		hash ^= *((const unsigned int *)ptr);
		len -= sizeof(unsigned int);
		ptr += sizeof(unsigned int);
	}
	while(len) {
		hash ^= *ptr;
		len--;
		ptr++;
	}

	return hash % UDP_PROXY_HT_SIZE;
}


static inline void ht_list_add(
		struct udp_ht_link *link, struct udp_ht_link *head)
{
	head->next->prev = link;
	link->next = head->next;
	link->prev = head;
	head->next = link;
}


static inline void ht_list_remove(struct udp_ht_link *link)
{
	link->next->prev = link->prev;
	link->prev->next = link->next;
}


static const char *udp_ht_dump_doc[] = {
		"Dump the contents of the UDP proxy hash table.", 0};

static const char *udp_ht_stats_doc[] = {
		"Report statistics about the UDP proxy hash table.", 0};

static const char *udp_ht_clean_doc[] = {
		"Remove expired entries from the UDP proxy hash table.", 0};

static const char *udp_ht_flush_doc[] = {
		"Flush (empty out) the UDP proxy hash table.", 0};


static void udp_ht_dump(rpc_t *rpc, void *c)
{
	unsigned int i;

	lock_get(udp_proxy_ht_lock);

	for(i = 0; i < UDP_PROXY_HT_SIZE; i++) {
		struct udp_ht_link *head = &udp_proxy_ht_real[i];
		struct udp_ht_link *link;
		for(link = head->next; link != head; link = link->next) {
			struct udp_ht_entry *entry =
					ksr_container_of(link, struct udp_ht_entry, real_link);
			void *h;
			if(rpc->add(c, "{", &h) < 0) {
				rpc->fault(c, 500, "Internal error while adding to array");
				goto error;
			}
			if(rpc->struct_add(h, "sst", "proxyaddr",
					   su2a(&entry->proxy_peer, sizeof(entry->proxy_peer)),
					   "realaddr",
					   su2a(&entry->real_peer, sizeof(entry->real_peer)),
					   "lastaccess", entry->accessed)
					< 0) {
				rpc->fault(c, 500, "Internal error while adding struct");
				goto error;
			}
		}
	}

error:
	lock_release(udp_proxy_ht_lock);
}


static void udp_ht_stats(rpc_t *rpc, void *c)
{
	void *h;
	unsigned int i;
	unsigned int real_buckets = 0, proxy_buckets = 0;
	unsigned int real_count = 0, proxy_count = 0;

	lock_get(udp_proxy_ht_lock);

	for(i = 0; i < UDP_PROXY_HT_SIZE; i++) {
		struct udp_ht_link *head = &udp_proxy_ht_real[i];
		if(head->next == head)
			continue;
		real_buckets++;

		struct udp_ht_link *link;
		for(link = head->next; link != head; link = link->next)
			real_count++;
	}

	for(i = 0; i < UDP_PROXY_HT_SIZE; i++) {
		struct udp_ht_link *head = &udp_proxy_ht_proxy[i];
		if(head->next == head)
			continue;
		proxy_buckets++;

		struct udp_ht_link *link;
		for(link = head->next; link != head; link = link->next)
			proxy_count++;
	}

	if(rpc->add(c, "{", &h) < 0) {
		rpc->fault(c, 500, "Internal error while adding to array");
		goto error;
	}
	if(rpc->struct_add(h, "uuuu", "real_buckets", real_buckets, "proxy_buckets",
			   proxy_buckets, "real_items", real_count, "proxy_items",
			   proxy_count)
			< 0) {
		rpc->fault(c, 500, "Internal error while adding struct");
		goto error;
	}

error:
	lock_release(udp_proxy_ht_lock);
}


static void udp_ht_clean(rpc_t *rpc, void *c)
{
	unsigned int i;
	unsigned int count = 0;
	time_t now = time(NULL);
	time_t cutoff = now - UDP_PROXY_HT_LIFETIME;

	lock_get(udp_proxy_ht_lock);

	for(i = 0; i < UDP_PROXY_HT_SIZE; i++) {
		struct udp_ht_link *head = &udp_proxy_ht_real[i];
		struct udp_ht_link *link, *next;

		for(link = head->next; link != head; link = next) {
			struct udp_ht_entry *entry = ksr_container_of(
					head->next, struct udp_ht_entry, real_link);
			next = link->next;

			if(entry->accessed >= cutoff)
				continue;

			ht_list_remove(&entry->real_link);
			ht_list_remove(&entry->proxy_link);
			shm_free(entry);
			count++;
		}
	}

	lock_release(udp_proxy_ht_lock);

	rpc->rpl_printf(
			c, "%u expired hash table entries have been removed.", count);
}


static void udp_ht_flush(rpc_t *rpc, void *c)
{
	unsigned int i;
	unsigned int count = 0;

	lock_get(udp_proxy_ht_lock);

	for(i = 0; i < UDP_PROXY_HT_SIZE; i++) {
		struct udp_ht_link *head = &udp_proxy_ht_real[i];
		while(head->next != head) {
			struct udp_ht_entry *entry = ksr_container_of(
					head->next, struct udp_ht_entry, real_link);
			ht_list_remove(&entry->real_link);
			ht_list_remove(&entry->proxy_link);
			shm_free(entry);
			count++;
		}
	}

	lock_release(udp_proxy_ht_lock);

	rpc->rpl_printf(c, "%u hash table entries have been flushed.", count);
}


// clang-format off
static rpc_export_t udp_rpc[] = {
	{"udp.proxy.dump",	udp_ht_dump,	udp_ht_dump_doc,	RET_ARRAY},
	{"udp.proxy.stats",	udp_ht_stats,	udp_ht_stats_doc,	0},
	{"udp.proxy.clean",	udp_ht_clean,	udp_ht_clean_doc,	0},
	{"udp.proxy.flush",	udp_ht_flush,	udp_ht_flush_doc,	0},
	{0}
};
// clang-format on


int udp_main_init(void)
{
	unsigned int i;

	if(ksr_udp_accept_proxy == 0)
		return 0;
	if(ksr_udp_accept_proxy < 0
			|| ksr_udp_accept_proxy > UDP_ACCEPT_PROXY_BOTH) {
		LM_ERR("Invalid value for 'udp_accept_proxy'.\n");
		return -1;
	}

	if(rpc_register_array(udp_rpc)) {
		LM_ERR("failed to register UDP RPC commands\n");
		return -1;
	}

	udp_proxy_ht_lock = lock_alloc();
	if(!udp_proxy_ht_lock) {
		SHM_MEM_ERROR;
		return -1;
	}
	if(!lock_init(udp_proxy_ht_lock)) {
		SHM_MEM_ERROR;
		return -1;
	}

	udp_proxy_ht_real = (struct udp_ht_link *)shm_malloc(
			sizeof(struct udp_ht_link) * UDP_PROXY_HT_SIZE);
	if(!udp_proxy_ht_real) {
		SHM_MEM_ERROR;
		return -1;
	}

	udp_proxy_ht_proxy = (struct udp_ht_link *)shm_malloc(
			sizeof(struct udp_ht_link) * UDP_PROXY_HT_SIZE);
	if(!udp_proxy_ht_proxy) {
		SHM_MEM_ERROR;
		return -1;
	}

	// initialise circular lists
	for(i = 0; i < UDP_PROXY_HT_SIZE; i++) {
		udp_proxy_ht_real[i] = (struct udp_ht_link){
				.next = &udp_proxy_ht_real[i], .prev = &udp_proxy_ht_real[i]};
		udp_proxy_ht_proxy[i] = (struct udp_ht_link){
				.next = &udp_proxy_ht_proxy[i], .prev = &udp_proxy_ht_proxy[i]};
	}

	return 0;
}


// Generic lookup function that can be used to do lookups in both hash tables
// with both keys, given the offsets to the struct members.
// The compiler will optimize away this indirection and produce code identical
// to having two distinct specific lookup functions.
static inline struct udp_ht_entry *sockaddr_ht_get_generic(
		const union sockaddr_union *addr, struct udp_ht_link *head,
		size_t link_offset, size_t peer_offset)
{
	time_t now = time(NULL);
	time_t cutoff = now - UDP_PROXY_HT_LIFETIME;
	struct udp_ht_link *link, *next;

	for(link = head->next; link != head; link = next) {
		struct udp_ht_entry *entry =
				(struct udp_ht_entry *)(((char *)link) - link_offset);
		union sockaddr_union *peer =
				(union sockaddr_union *)(((char *)entry) + peer_offset);

		next = link->next;

		if(su_cmp(addr, peer)) {
			entry->accessed = now;
			return entry;
		}

		if(entry->accessed >= cutoff)
			continue;

		// remove expired entry
		ht_list_remove(&entry->proxy_link);
		ht_list_remove(&entry->real_link);
		shm_free(entry);
	}

	return NULL;
}

// given proxy address, returns pointer to entry. unlocked
struct udp_ht_entry *sockaddr_ht_get_by_proxy(
		const union sockaddr_union *addr, unsigned int hash)
{
	return sockaddr_ht_get_generic(addr, &udp_proxy_ht_proxy[hash],
			offsetof(struct udp_ht_entry, proxy_link),
			offsetof(struct udp_ht_entry, proxy_peer));
}


// given real peer address, returns pointer to entry. unlocked
struct udp_ht_entry *sockaddr_ht_get_by_real(
		const union sockaddr_union *addr, unsigned int hash)
{
	return sockaddr_ht_get_generic(addr, &udp_proxy_ht_real[hash],
			offsetof(struct udp_ht_entry, real_link),
			offsetof(struct udp_ht_entry, real_peer));
}


// given proxy address, returns peer address
static union sockaddr_union sockaddr_ht_lookup_real(
		const union sockaddr_union *addr, unsigned int hash)
{
	union sockaddr_union ret = {0};
	struct udp_ht_entry *entry;

	lock_get(udp_proxy_ht_lock);

	entry = sockaddr_ht_get_by_proxy(addr, hash);
	if(entry)
		ret = entry->real_peer;

	lock_release(udp_proxy_ht_lock);

	return ret;
}


// given real peer address, returns proxy address
static union sockaddr_union sockaddr_ht_lookup_proxy(
		const union sockaddr_union *addr, unsigned int hash)
{
	union sockaddr_union ret = {0};
	struct udp_ht_entry *entry;

	lock_get(udp_proxy_ht_lock);

	entry = sockaddr_ht_get_by_real(addr, hash);
	if(entry)
		ret = entry->proxy_peer;

	lock_release(udp_proxy_ht_lock);

	return ret;
}


static void sockaddr_ht_insert(union sockaddr_union *proxy,
		union sockaddr_union *real, unsigned int proxy_hash)
{
	unsigned int real_hash = sockaddr_hash(real);
	struct udp_ht_entry *entry;

	lock_get(udp_proxy_ht_lock);

	// first see if the proxy address is already known
	entry = sockaddr_ht_get_by_proxy(proxy, proxy_hash);
	if(entry) {
		// proxy entry already present. has address changed?
		if(!su_cmp(real, &entry->real_peer)) {
			// update real peer
			// remove previous peer entry
			ht_list_remove(&entry->real_link);

			// update value
			entry->real_peer = *real;

			// insert into new hash bucket
			ht_list_add(&entry->real_link, &udp_proxy_ht_real[real_hash]);
		}
	}
	// now see if maybe the real address is already known
	else if((entry = sockaddr_ht_get_by_real(real, real_hash))) {
		// real entry already present. has address changed?
		if(!su_cmp(proxy, &entry->proxy_peer)) {
			// update proxy address
			// remove previous proxy entry
			ht_list_remove(&entry->proxy_link);

			// update value
			entry->proxy_peer = *proxy;

			// insert into new hash bucket
			ht_list_add(&entry->proxy_link, &udp_proxy_ht_proxy[proxy_hash]);
		}
	} else {
		// no entry exist: make new one
		entry = shm_malloc(sizeof(*entry));
		if(!entry)
			SHM_MEM_ERROR;
		else {
			entry->proxy_peer = *proxy;
			entry->real_peer = *real;
			entry->accessed = time(NULL);

			// insert into tables
			ht_list_add(&entry->proxy_link, &udp_proxy_ht_proxy[proxy_hash]);
			ht_list_add(&entry->real_link, &udp_proxy_ht_real[real_hash]);
		}
	}

	lock_release(udp_proxy_ht_lock);
}


static char *resolve_proxy_proto_haproxy(
		char *buf, unsigned int *len, union sockaddr_union *fromaddr)
{
	if(!(ksr_udp_accept_proxy & UDP_ACCEPT_PROXY_HAPROXY))
		return NULL;

	// is this a known flow? only the first datagram may have the header
	union sockaddr_union proxy_addr = *fromaddr;
	unsigned int proxy_hash = sockaddr_hash(fromaddr);

	union sockaddr_union real_addr =
			sockaddr_ht_lookup_real(fromaddr, proxy_hash);
	if(real_addr.s.sa_family) {
		LM_DBG("Received UDP from known proxy flow %s, substituting real peer "
			   "address %s\n",
				su2a(fromaddr, sizeof(*fromaddr)),
				su2a(&real_addr, sizeof(real_addr)));
		// possible clobber of address while still returning NULL
		*fromaddr = real_addr;
	}
	// continue checking for header even if flow is known

	if(*len < 16)
		return NULL;
	if(memcmp(buf, "\x0d\x0a\x0d\x0a\x00\x0d\x0a\x51\x55\x49\x54\x0a", 12))
		return NULL;

	LM_DBG("Received UDP using HAproxy protocol v2 from %s\n",
			su2a(fromaddr, sizeof(*fromaddr)));

	uint8_t ver_cmd = buf[12];
	if((ver_cmd & 0xf0) != 0x20)
		return NULL; // must be version 2

	uint8_t fam = buf[13];
	uint16_t addr_len = ntohs(*((uint16_t *)(buf + 14)));
	if(*len < addr_len + 16)
		return NULL; // too short

	// got enough information to skip the header
	*len -= 16 + addr_len;
	buf += 16 + addr_len;

	if((ver_cmd & 0xf) != 0x1)
		return buf; // not proxy

	if((fam & 0xf) != 0x2)
		return buf; // not UDP

	switch((fam & 0xf0)) {
		case 0x10: // IPv4
			if(addr_len != 12)
				return buf; // invalid length
			memset(fromaddr, 0, sizeof(*fromaddr));
			fromaddr->sin.sin_family = AF_INET;
			fromaddr->sin.sin_addr.s_addr = *((in_addr_t *)(buf - 12));
			fromaddr->sin.sin_port = *((uint16_t *)(buf - 4));
#ifdef HAVE_SOCKADDR_SA_LEN
			fromaddr->s.sa_len = sizeof(fromaddr->sin);
#endif
			break;

		case 0x20: // IPv6
			if(addr_len != 36)
				return buf; // invalid length
			memset(fromaddr, 0, sizeof(*fromaddr));
			fromaddr->sin6.sin6_family = AF_INET6;
			fromaddr->sin6.sin6_addr = *((struct in6_addr *)(buf - 36));
			fromaddr->sin6.sin6_port = *((uint16_t *)(buf - 4));
#ifdef HAVE_SOCKADDR_SA_LEN
			fromaddr->s.sa_len = sizeof(fromaddr->sin6);
#endif
			break;

		default:
			break;
	}

	LM_DBG("Peer address reported via HAproxy protocol v2 is %s\n",
			su2a(fromaddr, sizeof(*fromaddr)));

	// remember flow
	sockaddr_ht_insert(&proxy_addr, fromaddr, proxy_hash);

	return buf;
}

static char *resolve_proxy_proto_simple(
		char *buf, unsigned int *len, union sockaddr_union *fromaddr)
{
	if(!(ksr_udp_accept_proxy & UDP_ACCEPT_PROXY_SIMPLE))
		return NULL;
	if(*len < 38)
		return NULL;
	if(memcmp(buf, "\x56\xec", 2))
		return NULL;

	union sockaddr_union proxy_addr = *fromaddr;

	memset(fromaddr, 0, sizeof(*fromaddr));

	struct in6_addr *addr = ((struct in6_addr *)(buf + 2));

	// check for 4-in-6 address
	if(memcmp(addr, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff", 12)) {
		fromaddr->sin6.sin6_family = AF_INET6;
		fromaddr->sin6.sin6_addr = *addr;
		fromaddr->sin6.sin6_port = *((uint16_t *)(buf + 34));
#ifdef HAVE_SOCKADDR_SA_LEN
		fromaddr->s.sa_len = sizeof(fromaddr->sin6);
#endif
	} else {
		fromaddr->sin.sin_family = AF_INET;
		memcpy(&fromaddr->sin.sin_addr.s_addr, buf + 14, 4);
		fromaddr->sin.sin_port = *((uint16_t *)(buf + 34));
#ifdef HAVE_SOCKADDR_SA_LEN
		fromaddr->s.sa_len = sizeof(fromaddr->sin);
#endif
	}

	LM_DBG("Received UDP using simple proxy protocol from %s, reported real "
		   "peer address is %s\n",
			su2a(&proxy_addr, sizeof(proxy_addr)),
			su2a(fromaddr, sizeof(*fromaddr)));

	// remember flow
	sockaddr_ht_insert(&proxy_addr, fromaddr, sockaddr_hash(&proxy_addr));

	*len -= 38;
	return buf + 38;
}

static char *resolve_proxy_proto(
		char *buf, unsigned int *len, union sockaddr_union *fromaddr)
{
	if(likely(ksr_udp_accept_proxy == 0))
		return buf;

	// save address in case it's rewritten from a known flow
	union sockaddr_union orig_from = *fromaddr;

	char *ret = resolve_proxy_proto_haproxy(buf, len, fromaddr);
	if(ret)
		return ret;

	ret = resolve_proxy_proto_simple(buf, len, &orig_from);
	if(ret) {
		// report back substituted address
		*fromaddr = orig_from;
		return ret;
	}

	return buf;
}


static void resolve_proxy_dest(union sockaddr_union *addr, int *len)
{
	if(likely(ksr_udp_accept_proxy == 0))
		return;

	union sockaddr_union proxy_addr =
			sockaddr_ht_lookup_proxy(addr, sockaddr_hash(addr));
	if(!proxy_addr.s.sa_family)
		return;

	LM_DBG("Sending UDP to %s via HA proxy %s\n", su2a(addr, sizeof(*addr)),
			su2a(&proxy_addr, sizeof(proxy_addr)));

	*addr = proxy_addr;
	*len = sockaddru_len(proxy_addr);
}


#ifdef DBG_MSG_QA
/* message quality assurance -- frequently, bugs in ser have
   been indicated by zero characters or long whitespaces
   in generated messages; this debugging option aborts if
   any such message is sighted
*/
static int dbg_msg_qa(char *buf, int len)
{
#define _DBG_WS_LEN 3
#define _DBG_WS "   "

	char *scan;
	int my_len;
	int space_cnt;
	enum
	{
		QA_ANY,
		QA_SPACE,
		QA_EOL1
	} state;


	/* is there a zero character in there ? */
	if(memchr(buf, 0, len)) {
		LM_CRIT("message with 0 in it\n");
		return 0;
	}

	my_len = len;
	scan = buf;
	state = QA_ANY;
	space_cnt = 0;

	while(my_len) {
		switch(*scan) {
			case ' ':
				if(state == QA_SPACE) {
					space_cnt++;
					if(space_cnt == 4) {
						LM_CRIT("too many spaces\n");
						return 0;
					}
				} else
					space_cnt = 0;
				state = QA_SPACE;
				break;

			case '\r': /* ignore */
				space_cnt = 0;
				break;

			case '\n': /* don't proceed to body on EoH */
				if(state == QA_EOL1)
					goto qa_passed;
				space_cnt = 0;
				state = QA_EOL1;
				break;

			default:
				space_cnt = 0;
				state = QA_ANY;
				break;
		}
		scan++;
		my_len--;
	}


qa_passed:
	return 1;
}

#endif


int probe_max_receive_buffer(int udp_sock)
{
	int optval;
	int ioptval;
	unsigned int ioptvallen;
	int foptval;
	unsigned int foptvallen;
	int voptval;
	unsigned int voptvallen;
	int phase = 0;

	/* jku: try to increase buffer size as much as we can */
	ioptvallen = sizeof(ioptval);
	if(getsockopt(
			   udp_sock, SOL_SOCKET, SO_RCVBUF, (void *)&ioptval, &ioptvallen)
			== -1) {
		LM_ERR("fd: %d getsockopt: %s\n", udp_sock, strerror(errno));
		return -1;
	}
	if(ioptval == 0) {
		LM_DBG("SO_RCVBUF initially set to 0 for fd %d; resetting to %d\n",
				udp_sock, BUFFER_INCREMENT);
		ioptval = BUFFER_INCREMENT;
	} else
		LM_INFO("SO_RCVBUF is initially %d for fd %d\n", ioptval, udp_sock);
	for(optval = ioptval;;) {
		/* increase size; double in initial phase, add linearly later */
		if(phase == 0)
			optval <<= 1;
		else
			optval += BUFFER_INCREMENT;
		if(optval > maxbuffer) {
			if(phase == 1)
				break;
			else {
				phase = 1;
				optval >>= 1;
				continue;
			}
		}
		if(ksr_verbose_startup)
			LM_DBG("trying SO_RCVBUF: %d on fd: %d\n", optval, udp_sock);
		if(setsockopt(udp_sock, SOL_SOCKET, SO_RCVBUF, (void *)&optval,
				   sizeof(optval))
				== -1) {
			/* Solaris returns -1 if asked size too big; Linux ignores */
			LM_DBG("SOL_SOCKET failed for val %d on fd %d, phase %d: %s\n",
					optval, udp_sock, phase, strerror(errno));
			/* if setting buffer size failed and still in the aggressive
			   phase, try less aggressively; otherwise give up
			*/
			if(phase == 0) {
				phase = 1;
				optval >>= 1;
				continue;
			} else
				break;
		}
		/* verify if change has taken effect */
		/* Linux note -- otherwise I would never know that; funny thing: Linux
		   doubles size for which we asked in setsockopt
		*/
		voptvallen = sizeof(voptval);
		if(getsockopt(udp_sock, SOL_SOCKET, SO_RCVBUF, (void *)&voptval,
				   &voptvallen)
				== -1) {
			LM_ERR("fd: %d getsockopt: %s\n", udp_sock, strerror(errno));
			return -1;
		} else {
			if(ksr_verbose_startup)
				LM_DBG("setting SO_RCVBUF on fd %d; val=%d, verify=%d\n",
						udp_sock, optval, voptval);
			if(voptval < optval) {
				LM_DBG("setting SO_RCVBUF on fd %d has no effect\n", udp_sock);
				/* if setting buffer size failed and still in the aggressive
				phase, try less aggressively; otherwise give up
				*/
				if(phase == 0) {
					phase = 1;
					optval >>= 1;
					continue;
				} else
					break;
			}
		}

	} /* for ... */
	foptvallen = sizeof(foptval);
	if(getsockopt(
			   udp_sock, SOL_SOCKET, SO_RCVBUF, (void *)&foptval, &foptvallen)
			== -1) {
		LM_ERR("fd: %d getsockopt: %s\n", udp_sock, strerror(errno));
		return -1;
	}
	LM_INFO("SO_RCVBUF is finally %d on fd %d\n", foptval, udp_sock);

	return 0;

	/* EoJKU */
}

int probe_max_send_buffer(int udp_sock)
{
	int optval;
	int ioptval;
	unsigned int ioptvallen;
	int foptval;
	unsigned int foptvallen;
	int voptval;
	unsigned int voptvallen;
	int phase = 0;

	/* jku: try to increase buffer size as much as we can */
	ioptvallen = sizeof(ioptval);
	if(getsockopt(
			   udp_sock, SOL_SOCKET, SO_SNDBUF, (void *)&ioptval, &ioptvallen)
			== -1) {
		LM_ERR("fd: %d getsockopt: %s\n", udp_sock, strerror(errno));
		return -1;
	}
	if(ioptval == 0) {
		LM_DBG("SO_SNDBUF initially set to 0 for fd %d; resetting to %d\n",
				udp_sock, BUFFER_INCREMENT);
		ioptval = BUFFER_INCREMENT;
	} else
		LM_INFO("SO_SNDBUF is initially %d for fd %d\n", ioptval, udp_sock);
	for(optval = ioptval;;) {
		/* increase size; double in initial phase, add linearly later */
		if(phase == 0)
			optval <<= 1;
		else
			optval += BUFFER_INCREMENT;
		if(optval > maxsndbuffer) {
			if(phase == 1)
				break;
			else {
				phase = 1;
				optval >>= 1;
				continue;
			}
		}
		if(ksr_verbose_startup)
			LM_DBG("trying SO_SNDBUF: %d on fd: %d\n", optval, udp_sock);
		if(setsockopt(udp_sock, SOL_SOCKET, SO_SNDBUF, (void *)&optval,
				   sizeof(optval))
				== -1) {
			/* Solaris returns -1 if asked size too big; Linux ignores */
			LM_DBG("SOL_SOCKET failed for val %d on fd %d, phase %d: %s\n",
					optval, udp_sock, phase, strerror(errno));
			/* if setting buffer size failed and still in the aggressive
			   phase, try less aggressively; otherwise give up
			*/
			if(phase == 0) {
				phase = 1;
				optval >>= 1;
				continue;
			} else
				break;
		}
		/* verify if change has taken effect */
		/* Linux note -- otherwise I would never know that; funny thing: Linux
		   doubles size for which we asked in setsockopt
		*/
		voptvallen = sizeof(voptval);
		if(getsockopt(udp_sock, SOL_SOCKET, SO_SNDBUF, (void *)&voptval,
				   &voptvallen)
				== -1) {
			LM_ERR("fd: %d getsockopt: %s\n", udp_sock, strerror(errno));
			return -1;
		} else {
			if(ksr_verbose_startup)
				LM_DBG("setting SO_SNDBUF on fd %d; val=%d, verify=%d\n",
						udp_sock, optval, voptval);
			if(voptval < optval) {
				LM_DBG("setting SO_SNDBUF on fd %d has no effect\n", udp_sock);
				/* if setting buffer size failed and still in the aggressive
				phase, try less aggressively; otherwise give up
				*/
				if(phase == 0) {
					phase = 1;
					optval >>= 1;
					continue;
				} else
					break;
			}
		}

	} /* for ... */
	foptvallen = sizeof(foptval);
	if(getsockopt(
			   udp_sock, SOL_SOCKET, SO_SNDBUF, (void *)&foptval, &foptvallen)
			== -1) {
		LM_ERR("fd: %d getsockopt: %s\n", udp_sock, strerror(errno));
		return -1;
	}
	LM_INFO("SO_SNDBUF is finally %d on fd %d\n", foptval, udp_sock);

	return 0;

	/* EoJKU */
}


#ifdef USE_MCAST

/*
 * Set up multicast receiver
 */
static int setup_mcast_rcvr(
		int sock, union sockaddr_union *addr, char *interface)
{
#ifdef HAVE_IP_MREQN
	struct ip_mreqn mreq;
#else
	struct ip_mreq mreq;
#endif
	struct ipv6_mreq mreq6;

	if(addr->s.sa_family == AF_INET) {
		memcpy(&mreq.imr_multiaddr, &addr->sin.sin_addr,
				sizeof(struct in_addr));
#ifdef HAVE_IP_MREQN
		if(interface != 0) {
			mreq.imr_ifindex = if_nametoindex(interface);
		} else {
			mreq.imr_ifindex = 0;
		}
		mreq.imr_address.s_addr = htonl(INADDR_ANY);
#else
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);
#endif

		if(setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))
				== -1) {
			LM_ERR("setsockopt: %s\n", strerror(errno));
			return -1;
		}

	} else if(addr->s.sa_family == AF_INET6) {
		memcpy(&mreq6.ipv6mr_multiaddr, &addr->sin6.sin6_addr,
				sizeof(struct in6_addr));
		if(interface != 0) {
			mreq6.ipv6mr_interface = if_nametoindex(interface);
		} else {
			mreq6.ipv6mr_interface = 0;
		}
#ifdef __OS_linux
		if(setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq6,
#else
		if(setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq6,
#endif
				   sizeof(mreq6))
				== -1) {
			LM_ERR("setsockopt:%s\n", strerror(errno));
			return -1;
		}

	} else {
		LM_ERR("setup_mcast_rcvr: Unsupported protocol family\n");
		return -1;
	}
	return 0;
}

#endif /* USE_MCAST */


int udp_init(struct socket_info *sock_info)
{
	union sockaddr_union *addr;
	int optval;
#ifdef USE_MCAST
	unsigned char m_ttl, m_loop;
#endif
	addr = &sock_info->su;
	if((addr->s.sa_family == AF_INET6)
			&& (sr_bind_ipv6_link_local & KSR_IPV6_LINK_LOCAL_SKIP)
			&& IN6_IS_ADDR_LINKLOCAL(&addr->sin6.sin6_addr)) {
		LM_DBG("skip binding on %s (mode: %d)\n", sock_info->address_str.s,
				sr_bind_ipv6_link_local);
		return 0;
	}

	/*
	addr=(union sockaddr_union*)pkg_malloc(sizeof(union sockaddr_union));
	if (addr==0){
		PKG_MEM_ERROR;
		goto error;
	}
*/
	sock_info->proto = PROTO_UDP;
	if(init_su(addr, &sock_info->address, sock_info->port_no) < 0) {
		LM_ERR("could not init sockaddr_union\n");
		goto error;
	}

	sock_info->socket = socket(AF2PF(addr->s.sa_family), SOCK_DGRAM, 0);
	if(sock_info->socket == -1) {
		LM_ERR("socket: %s\n", strerror(errno));
		goto error;
	}
	/* set sock opts? */
	optval = 1;
	if(setsockopt(sock_info->socket, SOL_SOCKET, SO_REUSEADDR, (void *)&optval,
			   sizeof(optval))
			== -1) {
		LM_ERR("setsockopt: %s\n", strerror(errno));
		goto error;
	}
	/* tos */
	optval = tos;
	if(addr->s.sa_family == AF_INET) {
		if(setsockopt(sock_info->socket, IPPROTO_IP, IP_TOS, (void *)&optval,
				   sizeof(optval))
				== -1) {
			LM_WARN("setsockopt tos: %s\n", strerror(errno));
			/* continue since this is not critical */
		}
	} else if(addr->s.sa_family == AF_INET6) {
		if(setsockopt(sock_info->socket, IPPROTO_IPV6, IPV6_TCLASS,
				   (void *)&optval, sizeof(optval))
				== -1) {
			LM_WARN("setsockopt v6 tos: %s\n", strerror(errno));
			/* continue since this is not critical */
		}
		if(sr_bind_ipv6_link_local & KSR_IPV6_LINK_LOCAL_BIND) {
			LM_INFO("setting scope of %s (bind mode: %d)\n",
					sock_info->address_str.s, sr_bind_ipv6_link_local);
			addr->sin6.sin6_scope_id =
					ipv6_get_netif_scope(sock_info->address_str.s);
		}
	}

#if defined(__OS_linux) && defined(UDP_ERRORS)
	/* Ask for the ability to recvmsg (...,MSG_ERRQUEUE) for immediate
	 * resend when hitting Path MTU limits. */
	optval = 1;
	/* enable error receiving on unconnected sockets */
	if(addr->s.sa_family == AF_INET) {
		if(setsockopt(sock_info->socket, SOL_IP, IP_RECVERR, (void *)&optval,
				   sizeof(optval))
				== -1) {
			LM_ERR("IPV4 setsockopt: %s\n", strerror(errno));
			goto error;
		}
	} else if(addr->s.sa_family == AF_INET6) {
		if(setsockopt(sock_info->socket, SOL_IPV6, IPV6_RECVERR,
				   (void *)&optval, sizeof(optval))
				== -1) {
			LM_ERR("IPv6 setsockopt: %s\n", strerror(errno));
			goto error;
		}
	}
#endif
#if defined(__OS_linux)
	if(addr->s.sa_family == AF_INET) {
		/* If pmtu_discovery=1 then set DF bit and do Path MTU discovery,
		 * disabled by default. Specific to IPv4. If pmtu_discovery=2
		 * then the datagram will be fragmented if needed according to
		 * path MTU, or will set the don't-fragment flag otherwise */
		switch(pmtu_discovery) {
			case 1:
				optval = IP_PMTUDISC_DO;
				break;
			case 2:
				optval = IP_PMTUDISC_WANT;
				break;
			case 0:
			default:
				optval = IP_PMTUDISC_DONT;
				break;
		}
		if(setsockopt(sock_info->socket, IPPROTO_IP, IP_MTU_DISCOVER,
				   (void *)&optval, sizeof(optval))
				== -1) {
			LM_ERR("IPv4 setsockopt: %s\n", strerror(errno));
			goto error;
		}
	} else if(addr->s.sa_family == AF_INET6) {
		/* IPv6 never fragments but sends ICMPv6 Packet too Big,
		 * If pmtu_discovery=1 then set DF bit and do Path MTU discovery,
		 * disabled by default. Specific to IPv6. If pmtu_discovery=2
                 * then the datagram will be fragmented if needed according to
                 * path MTU */
		switch(pmtu_discovery) {
			case 1:
				optval = IPV6_PMTUDISC_DO;
				break;
			case 2:
				optval = IPV6_PMTUDISC_WANT;
				break;
			case 0:
			default:
				optval = IPV6_PMTUDISC_DONT;
				break;
		}
		if(setsockopt(sock_info->socket, IPPROTO_IPV6, IPV6_MTU_DISCOVER,
				   (void *)&optval, sizeof(optval))
				== -1) {
			LM_ERR("IPv6 setsockopt: %s\n", strerror(errno));
			goto error;
		}
	}
#endif

#if defined(__OS_linux)
	if(sock_info->vrfinfo.name.s != NULL && sock_info->vrfinfo.name.len > 0) {
		if(setsockopt(sock_info->socket, SOL_SOCKET, SO_BINDTODEVICE,
				   sock_info->vrfinfo.name.s, sock_info->vrfinfo.name.len)
				== -1) {
			LM_ERR("setsockopt SO_BINDTODEVICE on %.*s failed: %s\n",
					STR_FMT(&sock_info->vrfinfo.name), strerror(errno));
			goto error;
		}
	}
#else
	if(sock_info->vrfinfo.name.s != NULL && sock_info->vrfinfo.name.len > 0) {
		LM_WARN("VRF only supported on linux, skip SO_BINDTODEVICE for %.*s\n",
				STR_FMT(&sock_info->vrfinfo.name));
	}
#endif

#if defined(IP_FREEBIND)
	/* allow bind to non local address.
	 * useful when daemon started before network initialized */
	optval = 1;
	if(_sr_ip_free_bind
			&& setsockopt(sock_info->socket, IPPROTO_IP, IP_FREEBIND,
					   (void *)&optval, sizeof(optval))
					   == -1) {
		LM_WARN("setsockopt freebind failed: %s\n", strerror(errno));
		/* continue since this is not critical */
	}
#endif
#ifdef USE_MCAST
	if((sock_info->flags & SI_IS_MCAST)
			&& (setup_mcast_rcvr(sock_info->socket, addr, sock_info->mcast.s)
					< 0)) {
		goto error;
	}
	/* set the multicast options */
	if(addr->s.sa_family == AF_INET) {
		m_loop = mcast_loopback;
		if(setsockopt(sock_info->socket, IPPROTO_IP, IP_MULTICAST_LOOP, &m_loop,
				   sizeof(m_loop))
				== -1) {
			LM_WARN("setsockopt(IP_MULTICAST_LOOP): %s\n", strerror(errno));
			/* it's only a warning because we might get this error if the
			  network interface doesn't support multicasting -- andrei */
		}
		if(mcast_ttl >= 0) {
			m_ttl = mcast_ttl;
			if(setsockopt(sock_info->socket, IPPROTO_IP, IP_MULTICAST_TTL,
					   &m_ttl, sizeof(m_ttl))
					== -1) {
				LM_WARN("setsockopt (IP_MULTICAST_TTL): %s\n", strerror(errno));
			}
		}
	} else if(addr->s.sa_family == AF_INET6) {
		if(setsockopt(sock_info->socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
				   &mcast_loopback, sizeof(mcast_loopback))
				== -1) {
			LM_WARN("setsockopt (IPV6_MULTICAST_LOOP): %s\n", strerror(errno));
		}
		if(mcast_ttl >= 0) {
			if(setsockopt(sock_info->socket, IPPROTO_IP, IPV6_MULTICAST_HOPS,
					   &mcast_ttl, sizeof(mcast_ttl))
					== -1) {
				LM_WARN("setssckopt (IPV6_MULTICAST_HOPS): %s\n",
						strerror(errno));
			}
		}
	} else {
		LM_ERR("Unsupported protocol family %d\n", addr->s.sa_family);
		goto error;
	}
#endif /* USE_MCAST */

	if(probe_max_receive_buffer(sock_info->socket) == -1)
		goto error;

	if(probe_max_send_buffer(sock_info->socket) == -1)
		goto error;

	if(bind(sock_info->socket, &addr->s, sockaddru_len(*addr)) == -1) {
		LM_ERR("bind(%x, %p, %d) on %s: %s\n", sock_info->socket, &addr->s,
				(unsigned)sockaddru_len(*addr), sock_info->address_str.s,
				strerror(errno));
		if(addr->s.sa_family == AF_INET6) {
			LM_ERR("might be caused by using a link local address, is "
				   "'bind_ipv6_link_local' set?\n");
		}
		goto error;
	}

	/*	pkg_free(addr);*/
	return 0;

error:
	/*	if (addr) pkg_free(addr);*/
	return -1;
}


#define UDP_RCV_PRINTBUF_SIZE 512
#define UDP_RCV_PRINT_LEN 100

/**
 *
 */
int udp_rcv_loop()
{
	unsigned len;
	static char raw_buf[BUF_SIZE + 38
						+ 1]; // 38 = size of "HA proxy v2" binary header
	char *tmp, *buf;
	union sockaddr_union *fromaddr;
	unsigned int fromaddrlen;
	receive_info_t rcvi;
	sr_event_param_t evp = {0};
	char printbuf[UDP_RCV_PRINTBUF_SIZE];
	int i;
	int j;
	int l;

	fromaddr = (union sockaddr_union *)pkg_malloc(sizeof(union sockaddr_union));
	if(fromaddr == 0) {
		PKG_MEM_ERROR;
		goto error;
	}
	memset(fromaddr, 0, sizeof(union sockaddr_union));
	memset(&rcvi, 0, sizeof(receive_info_t));
	/* these do not change, set only once*/
	rcvi.bind_address = bind_address;
	rcvi.dst_port = bind_address->port_no;
	rcvi.dst_ip = bind_address->address;
	rcvi.proto = PROTO_UDP;

	/* initialize the config framework */
	if(cfg_child_init())
		goto error;

	for(;;) {
		fromaddrlen = sizeof(union sockaddr_union);
		len = recvfrom(bind_address->socket, raw_buf, BUF_SIZE, 0,
				(struct sockaddr *)fromaddr, &fromaddrlen);
		if(len == -1) {
			if(errno == EAGAIN) {
				LM_DBG("packet with bad checksum received\n");
				continue;
			}
			LM_ERR("recvfrom:[%d] %s\n", errno, strerror(errno));
			if((errno == EINTR) || (errno == EWOULDBLOCK)
					|| (errno == ECONNREFUSED))
				continue; /* goto skip;*/
			else
				goto error;
		}
		if(ksr_msg_recv_max_size <= len) {
			LOG(cfg_get(core, core_cfg, corelog),
					"read message too large: %d (cfg msg recv max size: %d)\n",
					len, ksr_msg_recv_max_size);
			continue;
		}
		if(fromaddrlen != (unsigned int)sockaddru_len(bind_address->su)) {
			LM_ERR("ignoring data - unexpected from addr len: %u != %u\n",
					fromaddrlen, (unsigned int)sockaddru_len(bind_address->su));
			continue;
		}
		/* we must 0-term the messages, receive_msg expects it */
		raw_buf[len] = 0; /* no need to save the previous char */

		buf = resolve_proxy_proto(raw_buf, &len, fromaddr);

		if(is_printable(L_DBG) && len > 10) {
			j = 0;
			for(i = 0; i < len && i < UDP_RCV_PRINT_LEN
					   && j + 8 < UDP_RCV_PRINTBUF_SIZE;
					i++) {
				if(isprint(buf[i])) {
					printbuf[j++] = buf[i];
				} else {
					l = snprintf(
							printbuf + j, 6, " %02X ", (unsigned char)buf[i]);
					if(l < 0 || l >= 6) {
						LM_ERR("print buffer building failed (%d/%d/%d)\n", l,
								j, i);
						continue; /* skip it */
					}
					j += l;
				}
			}
			LM_DBG("received on udp socket: (%d/%d/%d) [[%.*s]]\n", j, i, len,
					j, printbuf);
		}
		rcvi.src_su = *fromaddr;
		su2ip_addr(&rcvi.src_ip, fromaddr);
		rcvi.src_port = su_getport(fromaddr);

		if(ksr_evrt_received_mode & KSR_EVRT_RECEIVED_DATAIN) {
			if(ksr_evrt_received(buf, &len, &rcvi, KSR_EVRT_RECEIVED_DATAIN)
					< 0) {
				LM_DBG("dropping the received data\n");
				continue;
			}
		}

		if(unlikely(sr_event_enabled(SREV_NET_DGRAM_IN))) {
			void *sredp[3];
			sredp[0] = (void *)buf;
			sredp[1] = (void *)(&len);
			sredp[2] = (void *)(&rcvi);
			evp.data = (void *)sredp;
			if(sr_event_exec(SREV_NET_DGRAM_IN, &evp) < 0) {
				/* data handled by callback - continue to next packet */
				continue;
			}
		}
#ifndef NO_ZERO_CHECKS
		if(!unlikely(sr_event_enabled(SREV_STUN_IN))
				|| (unsigned char)*buf != 0x00) {
			if(len < MIN_UDP_PACKET) {
				tmp = ip_addr2a(&rcvi.src_ip);
				LM_DBG("probing packet received from %s %d\n", tmp,
						htons(rcvi.src_port));
				continue;
			}
		}
#endif
#ifdef DBG_MSG_QA
		if(!dbg_msg_qa(buf, len)) {
			LM_WARN("an incoming message didn't pass test,"
					"  drop it: %.*s\n",
					len, buf);
			continue;
		}
#endif
		if(rcvi.src_port == 0) {
			tmp = ip_addr2a(&rcvi.src_ip);
			LM_INFO("dropping 0 port packet from %s\n", tmp);
			continue;
		}

		/* update the local config */
		cfg_update();
		if(unlikely(sr_event_enabled(SREV_STUN_IN))
				&& (unsigned char)*buf == 0x00) {
			/* stun_process_msg releases buf memory if necessary */
			if((stun_process_msg(buf, len, &rcvi)) != 0) {
				continue; /* some error occurred */
			}
		} else {
			/* receive_msg must free buf too!*/
			receive_msg(buf, len, &rcvi);
		}

		/* skip: do other stuff */
	}
	/*
	if (fromaddr) pkg_free(fromaddr);
	return 0;
	*/

error:
	if(fromaddr)
		pkg_free(fromaddr);
	return -1;
}


/* send buf:len over udp to dst (uses only the to and send_sock dst members)
 * returns the numbers of bytes sent on success (>=0) and -1 on error
 */
int udp_send(struct dest_info *dst, char *buf, unsigned len)
{

	int n;
	int tolen;
	struct ip_addr ip; /* used only on error, for debugging */
#ifdef USE_RAW_SOCKS
	int mtu;
#endif /* USE_RAW_SOCKS */

#ifdef DBG_MSG_QA
	/* aborts on error, does nothing otherwise */
	if(!dbg_msg_qa(buf, len)) {
		LM_ERR("dbg_msg_qa failed\n");
		abort();
	}
#endif

	tolen = sockaddru_len(dst->to);

	resolve_proxy_dest(&dst->to, &tolen);

#ifdef USE_RAW_SOCKS
	if(likely(!(raw_udp4_send_sock >= 0 && cfg_get(core, core_cfg, udp4_raw)
				&& dst->send_sock->address.af == AF_INET))) {
#endif /* USE_RAW_SOCKS */
		/* normal send over udp socket */
	again:
		n = sendto(dst->send_sock->socket, buf, len, 0, &dst->to.s, tolen);
#ifdef XL_DEBUG
		LM_INFO("send status: %d\n", n);
#endif
		if(unlikely(n == -1)) {
			su2ip_addr(&ip, &dst->to);
			LM_ERR("sendto(sock, buf: %p, len: %u, 0, dst: (%s:%d), tolen: %d)"
				   " - err: %s (%d)\n",
					buf, len, ip_addr2a(&ip), su_getport(&dst->to), tolen,
					strerror(errno), errno);
			if(errno == EINTR)
				goto again;
			if(errno == EINVAL) {
				LM_CRIT("invalid sendtoparameters\n"
						"one possible reason is the server is bound to "
						"localhost and\n"
						"attempts to send to the net\n");
			}
		}
#ifdef USE_RAW_SOCKS
	} else {
		/* send over a raw socket */
		mtu = cfg_get(core, core_cfg, udp4_raw_mtu);
	raw_again:
		n = raw_iphdr_udp4_send(raw_udp4_send_sock, buf, len,
				&dst->send_sock->su, &dst->to, mtu);
		if(unlikely(n == -1)) {
			su2ip_addr(&ip, &dst->to);
			LM_ERR("raw_iphdr_udp4_send(%d,%p,%u,...,%s:%d,%d): %s(%d)\n",
					raw_udp4_send_sock, buf, len, ip_addr2a(&ip),
					su_getport(&dst->to), mtu, strerror(errno), errno);
			if(errno == EINTR)
				goto raw_again;
		}
	}
#endif /* USE_RAW_SOCKS */
	return n;
}

/**
 *
 */
typedef struct udpworker_task
{
	char *buf;
	int len;
	receive_info_t rcv;
} udpworker_task_t;

/**
 *
 */
void udpworker_task_exec(void *param)
{
	udpworker_task_t *utp;
	static char buf[BUF_SIZE + 1];
	receive_info_t rcvi;
	int len;
	sr_event_param_t evp = {0};
	char printbuf[UDP_RCV_PRINTBUF_SIZE];
	char *tmp;
	int i;
	int j;
	int l;

	utp = (udpworker_task_t *)param;

	LM_DBG("received task [%p] - msg len [%d]\n", utp, utp->len);
	if(utp->len > BUF_SIZE) {
		LM_ERR("message is too large [%d]\n", utp->len);
		return;
	}

	memcpy(buf, utp->buf, utp->len);
	len = utp->len;
	buf[len] = '\0';
	memcpy(&rcvi, &utp->rcv, sizeof(receive_info_t));
	rcvi.rflags |= RECV_F_INTERNAL;

	if(is_printable(L_DBG) && len > 10) {
		j = 0;
		for(i = 0; i < len && i < UDP_RCV_PRINT_LEN
				   && j + 8 < UDP_RCV_PRINTBUF_SIZE;
				i++) {
			if(isprint(buf[i])) {
				printbuf[j++] = buf[i];
			} else {
				l = snprintf(printbuf + j, 6, " %02X ", (unsigned char)buf[i]);
				if(l < 0 || l >= 6) {
					LM_ERR("print buffer building failed (%d/%d/%d)\n", l, j,
							i);
					continue; /* skip it */
				}
				j += l;
			}
		}
		LM_DBG("received on udp socket: (%d/%d/%d) [[%.*s]]\n", j, i, len, j,
				printbuf);
	}

	if(unlikely(sr_event_enabled(SREV_NET_DGRAM_IN))) {
		void *sredp[3];
		sredp[0] = (void *)buf;
		sredp[1] = (void *)(&len);
		sredp[2] = (void *)(&rcvi);
		evp.data = (void *)sredp;
		if(sr_event_exec(SREV_NET_DGRAM_IN, &evp) < 0) {
			/* data handled by callback - continue to next packet */
			return;
		}
	}
#ifndef NO_ZERO_CHECKS
	if(!unlikely(sr_event_enabled(SREV_STUN_IN))
			|| (unsigned char)*buf != 0x00) {
		if(len < MIN_UDP_PACKET) {
			tmp = ip_addr2a(&rcvi.src_ip);
			LM_DBG("probing packet received from %s %d\n", tmp,
					htons(rcvi.src_port));
			return;
		}
	}
#endif
#ifdef DBG_MSG_QA
	if(!dbg_msg_qa(buf, len)) {
		LM_WARN("an incoming message didn't pass test,"
				"  drop it: %.*s\n",
				len, buf);
		return;
	}
#endif
	if(rcvi.src_port == 0) {
		tmp = ip_addr2a(&rcvi.src_ip);
		LM_INFO("dropping 0 port packet from %s\n", tmp);
		return;
	}

	/* update the local config */
	cfg_update();
	if(unlikely(sr_event_enabled(SREV_STUN_IN))
			&& (unsigned char)*buf == 0x00) {
		/* stun_process_msg releases buf memory if necessary */
		if((stun_process_msg(buf, len, &rcvi)) != 0) {
			return; /* some error occurred */
		}
	} else {
		receive_msg(buf, len, &rcvi);
	}
}

/**
 *
 */
int udpworker_task_send(
		async_wgroup_t *awg, char *buf, int len, receive_info_t *rcv)
{
	async_task_t *at = NULL;
	udpworker_task_t *utp = NULL;
	int dsize;

	dsize = sizeof(async_task_t) + sizeof(udpworker_task_t)
			+ (len + 1) * sizeof(char);
	at = (async_task_t *)shm_malloc(dsize);
	if(at == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(at, 0, dsize);
	at->exec = udpworker_task_exec;
	at->param = (char *)at + sizeof(async_task_t);
	utp = (udpworker_task_t *)at->param;
	utp->buf = (char *)utp + sizeof(udpworker_task_t);
	memcpy(utp->buf, buf, len);
	utp->len = len;
	memcpy(&utp->rcv, rcv, sizeof(receive_info_t));

	return async_task_group_send(awg, at);
}

/**
 *
 */
void *ksr_udp_mtworker(void *si)
{
	socket_info_t *tsock;
	unsigned len;
	char *raw_buf, *buf;
	union sockaddr_union *fromaddr;
	unsigned int fromaddrlen;
	receive_info_t rcvi;
	async_wgroup_t *awg = NULL;
	str gname = str_init("udp");

	tsock = (socket_info_t *)si;

	LM_DBG("initiating udp thread worker [%.*s]\n", tsock->sock_str.len,
			tsock->sock_str.s);

	raw_buf = (char *)malloc((BUF_SIZE + 1) * sizeof(char));
	if(raw_buf == NULL) {
		LM_ERR("failled to allocate thread message buffer\n");
		exit(-1);
	}

	fromaddr = (union sockaddr_union *)malloc(sizeof(union sockaddr_union));
	if(fromaddr == 0) {
		LM_ERR("failled to allocate fromaddr buffer\n");
		exit(-1);
	}
	memset(fromaddr, 0, sizeof(union sockaddr_union));
	memset(&rcvi, 0, sizeof(receive_info_t));
	/* these do not change, set only once */
	rcvi.bind_address = tsock;
	rcvi.dst_port = tsock->port_no;
	rcvi.dst_ip = tsock->address;
	rcvi.proto = PROTO_UDP;

	if(tsock->agroup.agname[0] != '\0') {
		gname.s = tsock->agroup.agname;
		gname.len = strlen(gname.s);
	}
	awg = async_task_group_find(&gname);

	while(1) {
		fromaddrlen = sizeof(union sockaddr_union);
		len = recvfrom(tsock->socket, raw_buf, BUF_SIZE, 0,
				(struct sockaddr *)fromaddr, &fromaddrlen);
		if(len == -1) {
			if(errno == EAGAIN) {
				LM_DBG("packet with bad checksum received\n");
				continue;
			}
			LM_ERR("recvfrom:[%d] %s\n", errno, strerror(errno));
			if((errno == EINTR) || (errno == EWOULDBLOCK)
					|| (errno == ECONNREFUSED)) {
				continue; /* goto skip;*/
			} else {
				LM_ERR("unexpected recvfrom error: %d\n", errno);
				exit(-1);
			}
		}
		if(ksr_msg_recv_max_size <= len) {
			LOG(cfg_get(core, core_cfg, corelog),
					"read message too large: %d\n", len);
			continue;
		}
		if(fromaddrlen != (unsigned int)sockaddru_len(tsock->su)) {
			LM_ERR("ignoring data - unexpected from addr len: %u != %u\n",
					fromaddrlen, (unsigned int)sockaddru_len(tsock->su));
			continue;
		}
		/* it must 0-term the messages, receive_msg expects it */
		raw_buf[len] = 0; /* no need to save the previous char */

		buf = resolve_proxy_proto(raw_buf, &len, fromaddr);

		rcvi.src_su = *fromaddr;
		su2ip_addr(&rcvi.src_ip, fromaddr);
		rcvi.src_port = su_getport(fromaddr);

		if(awg == NULL) {
			if(tsock->agroup.agname[0] != '\0') {
				gname.s = tsock->agroup.agname;
				gname.len = strlen(gname.s);
			}
			awg = async_task_group_find(&gname);
		}
		if(awg != NULL) {
			udpworker_task_send(awg, buf, len, &rcvi);
		} else {
			LM_WARN("workers group [%s] not found\n", gname.s);
		}
	}
}

/**
 *
 */
int ksr_udp_start_mtreceiver(int child_rank, char *agname, int *woneinit)
{
	socket_info_t *si;
	pthread_t *udpthreads = NULL;
	int nrthreads = 0;
	int rc = 0;
	int i = 0;
	int pid;
	char si_desc[MAX_PT_DESC];

	if(udp_listen == NULL) {
		return 0;
	}

	snprintf(si_desc, MAX_PT_DESC, "udp multithreaded receiver (%s)",
			(agname) ? agname : "udp");

	pid = fork_process(child_rank, si_desc, 1);
	if(pid < 0) {
		LM_CRIT("cannot fork\n");
		goto error;
	} else if(pid == 0) {
		/* child */

		/* initialize the config framework */
		if(cfg_child_init()) {
			exit(-1);
		}
		if(*woneinit == 0) {
			if(run_child_one_init_route() < 0) {
				exit(-1);
			}
		}
		if(ksr_wait_worker1_mode != 0) {
			*ksr_wait_worker1_done = 1;
			LM_DBG("child one finished initialization\n");
		}
		/* udp workers */
		for(si = udp_listen; si; si = si->next) {
			if(agname == NULL) {
				nrthreads++;
			} else if((si->agroup.agname[0] != '\0')
					  && (strcmp(agname, si->agroup.agname) == 0)) {
				nrthreads++;
			}
		}
		udpthreads = (pthread_t *)malloc(nrthreads * sizeof(pthread_t));
		if(udpthreads == NULL) {
			LM_ERR("failed to alloc threads array\n");
			exit(-1);
		}
		memset(udpthreads, 0, nrthreads * sizeof(pthread_t));
		i = 0;
		for(si = udp_listen; si; si = si->next) {
			if(!((agname == NULL)
					   || ((si->agroup.agname[0] != '\0')
							   && (strcmp(agname, si->agroup.agname) == 0)))) {
				continue;
			}
			LM_DBG("creating udp thread worker[%d] [%.*s]\n", i,
					si->sock_str.len, si->sock_str.s);
			rc = pthread_create(
					&udpthreads[i], NULL, ksr_udp_mtworker, (void *)si);
			if(rc) {
				LM_ERR("failed to create thread %d\n", i);
				exit(-1);
			}
			i++;
		}
		for(;;) {
			pause();
			cfg_update();
		}
	}
	/* main process */
	if(*woneinit == 0 && ksr_wait_worker1_mode != 0) {
		int wcount = 0;
		while(*ksr_wait_worker1_done == 0) {
			sleep_us(ksr_wait_worker1_usleep);
			wcount++;
			if(ksr_wait_worker1_time <= wcount * ksr_wait_worker1_usleep) {
				LM_ERR("waiting for child one too long - wait "
					   "time: %d\n",
						ksr_wait_worker1_time);
				goto error;
			}
		}
		LM_DBG("child one initialized after %d wait steps\n", wcount);
	}
	*woneinit = 1;

	return 0;
error:
	return -1;
}

/*
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

/** Kamailio core :: Destination blacklists.
 * @file
 * @author andrei, Gergo
 * @ingroup core
 * Module: @ref core
 */

#ifndef dst_black_list_h
#define dst_black_list_h

#include "ip_addr.h"
#include "parser/msg_parser.h"
#include "timer_ticks.h"
#include "cfg_core.h"

#define DEFAULT_BLST_TIMEOUT		60  /**< 1 min. */
#define DEFAULT_BLST_MAX_MEM		250 /**< 250 KB */

/** @name flags: */
/*@{ */

#define BLST_IS_IPV6		1		/**< set if the address is ipv6 */
#define BLST_ERR_SEND		(1<<1)	/**< set if  send is denied/failed */
#define BLST_ERR_CONNECT	(1<<2)	/**< set if connect failed (tcp/tls) */
#define BLST_ICMP_RCVD		(1<<3)	/**< set if icmp error */
#define BLST_ERR_TIMEOUT	(1<<4)	/**< set if sip timeout */
#define BLST_503			(1<<5)	/**< set for 503 replies */
#define BLST_ADM_PROHIBITED	(1<<6)	/**< administratively prohibited */
#define BLST_PERMANENT		(1<<7)  /**< never deleted, never expires */
/*@} */

/* uncomment the define above to enable blacklist callbacks support */
/*#define DST_BLACKLIST_HOOKS*/

#define DST_BLACKLIST_CONTINUE 0 /**< add: do nothing/ignore, search: ignore */
#define DST_BLACKLIST_ACCEPT 1   /**< add: force accept, search: force match */
#define DST_BLACKLIST_DENY  -1   /**< add: deny, search: force no match */

#define DST_BLACKLIST_ADD_CB 1
#define DST_BLACKLIST_SEARCH_CB 2


extern unsigned blst_proto_imask[PROTO_LAST+1];

#ifdef DST_BLACKLIST_HOOKS
struct blacklist_hook{
	/* WARNING: msg might be NULL, and it might point to shared memory
	 * without locking, do not modify it! msg can be used typically for checking
	 * the message flags with isflagset() */
	int (*on_blst_action)(struct dest_info* si, unsigned char* err_flags,
							struct sip_msg* msg);
	/* called before ser shutdown */
	void (*destroy)(void);
};

int register_blacklist_hook(struct blacklist_hook *h, int type);
#endif /* DST_BLACKLIST_HOOKS */

int init_dst_blacklist(void);
#ifdef USE_DST_BLACKLIST_STATS
int init_dst_blacklist_stats(int iproc_num);
#define DST_BLACKLIST_ALL_STATS "bkl_all_stats"
#endif
void destroy_dst_blacklist(void);


/** force add to the blacklist.
 * like function dst_blacklist_add_to, but no ignore mask or
 * blacklist enabled checks are made.
 * @see dst_blacklist_add_to for the parameters and return value.
 */
int dst_blacklist_force_add_to(unsigned char err_flags, struct dest_info* si,
								struct sip_msg* msg, ticks_t timeout);

/** force add to the blacklist, long version.
 * like function dst_blacklist_su_to, but no ignore mask or
 * blacklist enabled checks are made.
 * @see dst_blacklist_su_to for the parameters and return value.
 */
int dst_blacklist_force_su_to(	unsigned char err_flags,
								unsigned char proto,
								union sockaddr_union* dst,
								struct sip_msg* msg,
								ticks_t timeout);


/** checks if blacklist should be used.
  * @param  err_flags - blacklist reason
  * @param si - filled dest_info structure pointer.
  * @return 1 if blacklist is enabled (core_cfg) and the event/error
  *           is not in the ignore list.
  *         0 otherwise
  */
#define should_blacklist(err_flags, si) \
	(cfg_get(core, core_cfg, use_dst_blacklist) && \
		((err_flags) & ~blst_proto_imask[(unsigned)((si)->proto)] & \
		 			   ~(si)->send_flags.blst_imask ))


/** checks if blacklist should be used, long version.
  * @param err_flags - blacklist reason
  * @param snd_flags - snd_flags pointer, can be 0.
  * @param proto - protocol, can be 0 (PROTO_NONE).
  * @param su  - sockaddr_union pointer, can be 0.
  * @return 1 if blacklist is enabled (core_cfg) and the event/error
  *           is not in the ignore list. 0 otherwise
  */
#define should_blacklist_su(err_flags, snd_flags, proto, su) \
	(cfg_get(core, core_cfg, use_dst_blacklist) && \
		((err_flags) & ~blst_proto_imask[(unsigned)(proto)] & \
		 			~((snd_flags)?((snd_flags_t*)(snd_flags))->blst_imask:0)))


/** adds a dst to the blacklist.
 *
 * @param  err_flags - blacklist reason
 * @param si  - dest_info structure (dst).
 * @param msg - sip msg struct. pointer if known, 0 otherwise.
 * @param timeout - timeout in ticks.
 * @return >=0 on success, -1 on error.
 */
#define dst_blacklist_add_to(err_flags, si, msg, timeout) \
	(should_blacklist(err_flags, si)? \
		dst_blacklist_force_add_to((err_flags), (si), (msg), (timeout))\
		: 0)


/** adds a dst to the blacklist, long version.
 * Similar to dst_blacklist_add_to, but uses "unpacked" parameters.
 * @param  err_flags - blacklist reason
 * @param proto - protocol.
 * @param dst  - sockaddr_union pointer.
 * @param snd_flags - snd_flags pointer, can be 0.
 * @param msg - sip msg struct. pointer if known, 0 otherwise.
 * @param timeout - timeout in ticks.
 * @return >=0 on success, -1 on error.
 */
#define dst_blacklist_su_to(err_flags, proto, dst, snd_flags, msg, timeout) \
	(should_blacklist_su(err_flags, snd_flags, proto, dst) ? \
		dst_blacklist_force_su_to((err_flags), (proto), (dst), (msg), \
									(timeout))\
		: 0)


/** adds a dst to the blacklist with default timeout.
 *
 * @param  err_flags - blacklist reason
 * @param si  - dest_info structure (dst).
 * @param msg - sip msg struct. pointer if known, 0 otherwise.
 * @return >=0 on success, -1 on error.
 * @see dst_blacklist_add_to.
 */
#define dst_blacklist_add(err_flags, si, msg) \
	dst_blacklist_add_to(err_flags, si, msg, \
							S_TO_TICKS(cfg_get(core, core_cfg, blst_timeout)))


/** adds a dst to the blacklist with default timeout, long version.
 * Similar to dst_blacklist_add_to, but uses "unpacked" parameters.
 * @param  err_flags - blacklist reason
 * @param proto - protocol.
 * @param dst  - sockaddr_union pointer.
 * @param snd_flags - snd_flags pointer, can be 0.
 * @param msg - sip msg struct. pointer if known, 0 otherwise.
 * @return >=0 on success, -1 on error.
 * @see dst_blacklist_su_to.
 */
#define dst_blacklist_su(err_flags, proto, dst, snd_flags, msg) \
	dst_blacklist_su_to(err_flags, proto, dst, snd_flags, msg, \
							S_TO_TICKS(cfg_get(core, core_cfg, blst_timeout)))

int dst_is_blacklisted(struct dest_info* si, struct sip_msg* msg);

/** delete an entry from the blacklist. */
int dst_blacklist_del(struct dest_info* si, struct sip_msg* msg);

/** deletes all the entries from the blacklist except the permanent ones.
 * (which are marked with BLST_PERMANENT)
 */
void dst_blst_flush(void);

int use_dst_blacklist_fixup(void *handle, str *gname, str *name, void **val);

/** KByte to Byte conversion. */
int blst_max_mem_fixup(void *handle, str *gname, str *name, void **val);

void blst_reinit_ign_masks(str* gname, str* name);

#endif

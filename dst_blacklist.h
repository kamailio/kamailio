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
 */

#ifndef dst_black_list_h
#define dst_black_list_h

#include "ip_addr.h"
#include "parser/msg_parser.h"

/* flags: */
#define BLST_IS_IPV6		1		/* set if the address is ipv6 */
#define BLST_ERR_SEND		(1<<1)	/* set if  send is denied/failed */
#define BLST_ERR_CONNECT	(1<<2)	/* set if connect failed (tcp/tls) */
#define BLST_ICMP_RCVD		(1<<3)	/* set if icmp error */
#define BLST_ERR_TIMEOUT	(1<<4)	/* set if sip timeout */
#define BLST_RESERVED		(1<<5)	/* not used yet */
#define BLST_ADM_PROHIBITED	(1<<6)	/* administratively prohibited */
#define BLST_PERMANENT		(1<<7)  /* never deleted, never expires */

/* uncomment the define above to enable blacklist callbacks support */
/*#define DST_BLACKLIST_HOOKS*/

#define DST_BLACKLIST_CONTINUE 0 /* add: do nothing/ignore, search: ignore */
#define DST_BLACKLIST_ACCEPT 1   /* add: force accept, search: force match */
#define DST_BLACKLIST_DENY  -1   /* add: deny, search: force no match */

#define DST_BLACKLIST_ADD_CB 1
#define DST_BLACKLIST_SEARCH_CB 2

#ifdef DST_BLACKLIST_HOOKS
struct blacklist_hook{
	/* WARNING: msg might be NULL, and it might point to shared memory
	 * without locking, do not modify it! msg can be used typically for checking
	 * the message flags with isflagset() */
	int (*on_blst_action)(struct dest_info* si, unsigned char* err_flags, struct sip_msg* msg);
	/* called before ser shutdown */
	void (*destroy)(void);
};

int register_blacklist_hook(struct blacklist_hook *h, int type);
#endif /* DST_BLACKLIST_HOOKS */

int init_dst_blacklist();
void destroy_dst_blacklist();

int dst_blacklist_add(unsigned char err_flags, struct dest_info* si, struct sip_msg* msg);

int dst_is_blacklisted(struct dest_info* si, struct sip_msg* msg);

/* deletes all the entries from the blacklist except the permanent ones
 * (which are marked with BLST_PERMANENT)
 */
void dst_blst_flush(void);

#endif

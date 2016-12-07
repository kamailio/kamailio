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
/*!
 * \file
 * \brief Kamailio core :: Non-sip callbacks
 * non-sip callbacks, called whenever a message with protocol != SIP/2.0
 * is received (the message must have at least a sip like first line or
 * else they will be dropped before this callbacks are called
 * \ingroup core
 * \author andrei
 * Module: \ref core
 */


#ifndef _nonsip_hooks_h
#define _nonsip_hooks_h

#include "parser/msg_parser.h" /* sip_msg */

#define MAX_NONSIP_HOOKS 1

enum nonsip_msg_returns{ NONSIP_MSG_ERROR=-1, NONSIP_MSG_DROP=0,
						 NONSIP_MSG_PASS,     NONSIP_MSG_ACCEPT };

struct nonsip_hook{
	char* name; /* must be !=0, it has only "debugging" value */
	/* called each time a sip like request (from the first line point of view)
	 * with protocol/version !=  SIP/2.0 is received
	 * return: 0 - drop message immediately, >0 - continue with other hooks,
	 *        <0 - error (drop message)
	 */
	int (*on_nonsip_req)(struct sip_msg* msg);
	/* called before ser shutdown (last minute cleanups) */
	void (*destroy)(void);
};


int init_nonsip_hooks(void);
void destroy_nonsip_hooks(void);
int register_nonsip_msg_hook(struct nonsip_hook *h);
int nonsip_msg_run_hooks(struct sip_msg* msg);

#endif

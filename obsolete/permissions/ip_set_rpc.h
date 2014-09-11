/* 
 * $Id$
 *
 * allow_trusted related functions
 *
 * Copyright (C) 2008 iptelorg GmbH
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _IP_SET_RPC_H
#define _IP_SET_RPC_H 1

#include "ip_set.h"
#include "../../atomic_ops.h"
#include "../../lock_ops.h"
#include "../../rpc.h"

struct ip_set_ref {
	struct ip_set ip_set;
	atomic_t refcnt;
};

struct ip_set_list_item {
	int idx;
	str name;
	gen_lock_t read_lock;
	gen_lock_t write_lock;
	struct ip_set_ref *ip_set;
	struct ip_set ip_set_pending;
};
	    
extern int ip_set_list_malloc(int num, str *names);
extern void ip_set_list_free();
extern struct ip_set_list_item* ip_set_list_find_by_name(str name);

/* RPC functions */

extern const char* rpc_ip_set_clean_doc[];
extern void rpc_ip_set_clean(rpc_t* rpc, void* ctx);

extern const char* rpc_ip_set_add_doc[];
extern void rpc_ip_set_add(rpc_t* rpc, void* ctx);

extern const char* rpc_ip_set_commit_doc[];
extern void rpc_ip_set_commit(rpc_t* rpc, void* ctx);

extern const char* rpc_ip_set_list_doc[];
extern void rpc_ip_set_list(rpc_t* rpc, void* ctx);

extern const char* rpc_ip_set_print_doc[];
extern void rpc_ip_set_print(rpc_t* rpc, void* ctx);

#endif /* _IP_SET_RPC_H */

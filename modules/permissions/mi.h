/*
 *
 * Header file for permissions MI functions
 *
 * Copyright (C) 2006 Juha Heinanen
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


#ifndef _PERMISSIONS_MI_H_
#define _PERMISSIONS_MI_H_


#include "../../lib/kmi/mi.h"
#include "../../rpc.h"


#define MI_TRUSTED_RELOAD "trusted_reload"
#define MI_TRUSTED_DUMP "trusted_dump"

#define MI_ADDRESS_RELOAD "address_reload"
#define MI_ADDRESS_DUMP "address_dump"
#define MI_SUBNET_DUMP "subnet_dump"
#define MI_DOMAIN_DUMP "perm_domain_dump"

#define MI_ALLOW_URI "allow_uri"

struct mi_root* mi_trusted_reload(struct mi_root *cmd, void *param);
void rpc_trusted_reload(rpc_t* rpc, void* c);

struct mi_root* mi_trusted_dump(struct mi_root *cmd, void *param);
void rpc_trusted_dump(rpc_t* rpc, void* c);

struct mi_root* mi_address_reload(struct mi_root *cmd, void *param);
void rpc_address_reload(rpc_t* rpc, void* c);

struct mi_root* mi_address_dump(struct mi_root *cmd, void *param);
void rpc_address_dump(rpc_t* rpc, void* c);

struct mi_root* mi_subnet_dump(struct mi_root *cmd_tree, void *param);
void rpc_subnet_dump(rpc_t* rpc, void* c);

struct mi_root* mi_domain_name_dump(struct mi_root *cmd_tree, void *param);
void rpc_domain_name_dump(rpc_t* rpc, void* c);

struct mi_root* mi_allow_uri(struct mi_root *cmd, void *param);
void rpc_test_uri(rpc_t* rpc, void* c);

#endif

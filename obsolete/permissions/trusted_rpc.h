/* 
 * $Id$
 *
 * allow_trusted related functions
 *
 * Copyright (C) 2003 Juha Heinanen
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

#ifndef _TRUSTED_RPC_H
#define _TRSUTED_RPC_H

#include "../../rpc.h"

extern const char* trusted_reload_doc[];

/*
 * Fifo function to reload trusted table
 */
void trusted_reload(rpc_t* rpc, void* ctx);

extern const char* trusted_dump_doc[];

/*
 * Fifo function to print entries from current hash table
 */
void trusted_dump(rpc_t* rpc, void* ctx);

#endif /* _TRUSTED_RPC_H */

/*
 * Copyright (C) 2012 Carlos Ruiz Díaz (caruizdiaz.com),
 *                    ConexionGroup (www.conexiongroup.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef CNXCC_RPC_H_
#define CNXCC_RPC_H_

void rpc_active_clients(rpc_t *rpc, void *ctx);
void rpc_kill_call(rpc_t *rpc, void *ctx);
void rpc_active_clients(rpc_t *rpc, void *ctx);
void rpc_check_client_stats(rpc_t *rpc, void *ctx);

#endif /* CNXCC_RPC_H_ */

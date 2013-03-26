/*
 * cnxcc_rpc.h
 *
 *  Created on: Dec 6, 2012
 *      Author: carlos
 */

#ifndef CNXCC_RPC_H_
#define CNXCC_RPC_H_

void rpc_active_clients(rpc_t* rpc, void* ctx);
void rpc_kill_call(rpc_t* rpc, void* ctx);
void rpc_active_clients(rpc_t* rpc, void* ctx);
void rpc_check_client_stats(rpc_t* rpc, void* ctx);

#endif /* CNXCC_RPC_H_ */

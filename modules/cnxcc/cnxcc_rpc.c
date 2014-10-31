/*
 * $Id$
 *
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

#include <stdio.h>

#include "../../locking.h"
#include "../../lock_ops.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"

#include "cnxcc_mod.h"

extern data_t _data;

void rpc_kill_call(rpc_t* rpc, void* ctx) {
	call_t *call;
	hash_tables_t *hts;
	str callid;

	if (!rpc->scan(ctx, "S", &callid)) {
		LM_ERR("%s: error reading RPC param\n", __FUNCTION__);
		return;
	}

	if (try_get_call_entry(&callid, &call, &hts) != 0) {
		LM_ERR("%s: call [%.*s] not found\n", __FUNCTION__, callid.len, callid.s);
		rpc->fault(ctx, 404, "CallID Not Found");
		return;
	}

	if (call == NULL) {
		LM_ERR("%s: call [%.*s] is in null state\n", __FUNCTION__, callid.len, callid.s);
		rpc->fault(ctx, 500, "Call is NULL");
		return;
	}

	LM_ALERT("Killing call [%.*s] via XMLRPC request\n", callid.len, callid.s);

	lock_get(&call->lock);

	terminate_call(call);

	lock_release(&call->lock);
}

void rpc_check_client_stats(rpc_t* rpc, void* ctx) {
	call_t *call, *tmp;
	int index	= 0;
	str client_id, rows;
	char row_buffer[512];
	credit_data_t *credit_data;

	if (!rpc->scan(ctx, "S", &client_id)) {
		LM_ERR("%s: error reading RPC param\n", __FUNCTION__);
		return;
	}

	if (try_get_credit_data_entry(&client_id, &credit_data) != 0) {
		LM_ERR("%s: client [%.*s] not found\n", __FUNCTION__, client_id.len, client_id.s);
		rpc->fault(ctx, 404, "Not Found");
		return;
	}

	if (credit_data == NULL) {
		LM_ERR("%s: credit data for client [%.*s] is NULL\n", __FUNCTION__, client_id.len, client_id.s);
		rpc->fault(ctx, 500, "Internal Server Error");
		return;
	}

	lock_get(&credit_data->lock);

	if (credit_data->number_of_calls <= 0) {
		lock_release(&credit_data->lock);
		LM_INFO("No calls for current client\n");
		return;
	}

	rows.len = 0;
	rows.s	 = pkg_malloc(10);

	if (rows.s == NULL)
		goto nomem;

	clist_foreach_safe(credit_data->call_list, call, tmp, next) {
		int row_len = 0;

		memset(row_buffer, 0, sizeof(row_buffer));

		if (credit_data->type == CREDIT_MONEY)
			snprintf(row_buffer, sizeof(row_buffer), "id:%d,confirmed:%s,local_consumed_amount:%f,global_consumed_amount:%f,local_max_amount:%f,global_max_amount:%f,call_id:%.*s,start_timestamp:%d"
								",inip:%d,finp:%d,cps:%f;",
								index,
								call->confirmed ? "yes" : "no",
								call->consumed_amount,
								credit_data->consumed_amount,
								call->max_amount,
								credit_data->max_amount,
								call->sip_data.callid.len, call->sip_data.callid.s,
								call->start_timestamp,
								call->money_based.initial_pulse,
								call->money_based.final_pulse,
								call->money_based.cost_per_second);
		else
			snprintf(row_buffer, sizeof(row_buffer), "id:%d,confirmed:%s,local_consumed_amount:%d,global_consumed_amount:%d,local_max_amount:%d,global_max_amount:%d,call_id:%.*s,start_timestamp:%d;",
								index,
								call->confirmed ? "yes" : "no",
								(int) call->consumed_amount,
								(int) credit_data->consumed_amount,
								(int) call->max_amount,
								(int) credit_data->max_amount,
								call->sip_data.callid.len, call->sip_data.callid.s,
								call->start_timestamp);

		row_len 	= strlen(row_buffer);
		rows.s		= pkg_realloc(rows.s, rows.len + row_len);

		if (rows.s == NULL) {
			lock_release(&credit_data->lock);
			goto nomem;
		}

		memcpy(rows.s + rows.len, row_buffer, row_len);
		rows.len += row_len;

		index++;
	}

	lock_release(&credit_data->lock);

	if (rpc->add(ctx, "S", &rows) < 0) {
		LM_ERR("%s: error creating RPC struct\n", __FUNCTION__);
	}

	if (rows.s != NULL)
		pkg_free(rows.s);

	return;

nomem:
	LM_ERR("No more pkg memory\n");
	rpc->fault(ctx, 500, "No more memory\n");
}

static int iterate_over_table(hash_tables_t *hts, str *result, credit_type_t type) {
	struct str_hash_entry *h_entry, *tmp;
	char row_buffer[512];
	int index = 0;

	lock_get(&hts->lock);

	if (hts->credit_data_by_client->table)
		for(index = 0; index < hts->credit_data_by_client->size; index++)
			clist_foreach_safe(&hts->credit_data_by_client->table[index], h_entry, tmp, next) {
				credit_data_t *credit_data	= (credit_data_t *) h_entry->u.p;
				lock_get(&credit_data->lock);

				int row_len = 0;

				memset(row_buffer, 0, sizeof(row_buffer));

				if (type == CREDIT_TIME) {
					snprintf(row_buffer, sizeof(row_buffer), "client_id:%.*s,"
											"number_of_calls:%d,"
											"concurrent_calls:%d,"
											"type:%d,"
											"max_amount:%d,"
											"consumed_amount:%d;",
											credit_data->call_list->client_id.len, credit_data->call_list->client_id.s,
											credit_data->number_of_calls,
											credit_data->concurrent_calls,
											type,
											(int) credit_data->max_amount,
											(int) credit_data->consumed_amount);
				}
				else if (type == CREDIT_MONEY) {
					snprintf(row_buffer, sizeof(row_buffer), "client_id:%.*s,"
											"number_of_calls:%d,"
											"concurrent_calls:%d,"
											"type:%d,"
											"max_amount:%f,"
											"consumed_amount:%f;",
											credit_data->call_list->client_id.len, credit_data->call_list->client_id.s,
											credit_data->number_of_calls,
											credit_data->concurrent_calls,
											type,
											credit_data->max_amount,
											credit_data->consumed_amount);
				}
				else {
					LM_ERR("Unknown credit type: %d\n", type);
					return -1;
				}

				lock_release(&credit_data->lock);

				row_len 	= strlen(row_buffer);
				result->s	= pkg_realloc(result->s, result->len + row_len);

				if (result->s == NULL) {
					lock_release(&hts->lock);
					goto nomem;
				}

				memcpy(result->s + result->len, row_buffer, row_len);
				result->len += row_len;

			}

	lock_release(&hts->lock);

	return 0;

nomem:
	LM_ERR("No more pkg memory\n");
	return -1;
}

void rpc_active_clients(rpc_t* rpc, void* ctx) {
	str rows;

	rows.s	 = pkg_malloc(10);

	if (rows.s == NULL)
		goto nomem;

	rows.len = 0;

	iterate_over_table(&_data.time, &rows, CREDIT_TIME);
	iterate_over_table(&_data.money, &rows, CREDIT_MONEY);

	if (!rpc->add(ctx, "S", &rows) < 0) {
		LM_ERR("%s: error creating RPC struct\n", __FUNCTION__);
	}

	if (rows.s != NULL)
		pkg_free(rows.s);

	return;

nomem:
	LM_ERR("No more pkg memory\n");
	rpc->fault(ctx, 500, "No more memory\n");
}


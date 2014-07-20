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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>

#include "../../locking.h"
#include "../../lock_ops.h"

#include "cnxcc_mod.h"
#include "cnxcc.h"
#include "cnxcc_check.h"

extern data_t _data;

void check_calls_by_money(unsigned int ticks, void *param)
{
	struct str_hash_entry *h_entry 	= NULL,
			  *tmp		= NULL;
	call_t *tmp_call		= NULL;
	int i;

	lock_get(&_data.money.lock);
	if (_data.money.credit_data_by_client->table)
		for(i = 0; i < _data.money.credit_data_by_client->size; i++)
			clist_foreach_safe(&_data.money.credit_data_by_client->table[i], h_entry, tmp, next)
			{
				credit_data_t *credit_data	= (credit_data_t *) h_entry->u.p;
				call_t *call			= NULL;
				double total_consumed_money	= 0;

				if (i > SAFE_ITERATION_THRESHOLD)
				{
					LM_ERR("Too many iterations for this loop: %d\n", i);
					break;
				}
				lock_get(&credit_data->lock);

				clist_foreach_safe(credit_data->call_list, call, tmp_call, next)
				{
					int consumed_time = 0;

					if (!call->confirmed)
						continue;

					consumed_time 		= get_current_timestamp() - call->start_timestamp;

					if (consumed_time > call->money_based.initial_pulse)
					{
						call->consumed_amount = (call->money_based.cost_per_second * call->money_based.initial_pulse)
												+
												call->money_based.cost_per_second *
												( (consumed_time - call->money_based.initial_pulse) / call->money_based.final_pulse + 1 ) *
												call->money_based.final_pulse;
					}

					total_consumed_money	+= call->consumed_amount;

					if (call->consumed_amount > call->max_amount)
					{
						LM_ALERT("[%.*s] call has exhausted its credit. Breaking the loop\n", call->sip_data.callid.len, call->sip_data.callid.s);
						break;
					}

					LM_DBG("CID [%.*s], start_timestamp [%d], seconds alive [%d], consumed credit [%f]\n",
																			call->sip_data.callid.len, call->sip_data.callid.s,
																			call->start_timestamp,
																			consumed_time,
																			call->consumed_amount
																			);
				}
				
				if (credit_data->concurrent_calls == 0)
				{
					lock_release(&credit_data->lock);
					continue;
				}

				credit_data->consumed_amount	= credit_data->ended_calls_consumed_amount + total_consumed_money;

				LM_DBG("Client [%.*s] | Ended-Calls-Credit-Spent: %f  TotalCredit/MaxCredit: %f/%f\n", credit_data->call_list->client_id.len, credit_data->call_list->client_id.s,
																									credit_data->ended_calls_consumed_amount,
																									credit_data->consumed_amount,
																									credit_data->max_amount);

				if (credit_data->consumed_amount >= credit_data->max_amount)
				{
					lock_release(&_data.money.lock);
					terminate_all_calls(credit_data);
					lock_release(&credit_data->lock);
					return;
				}

				lock_release(&credit_data->lock);
			}

	lock_release(&_data.money.lock);
}

void check_calls_by_time(unsigned int ticks, void *param)
{
	struct str_hash_entry *h_entry 	= NULL,
			*tmp		= NULL;
	call_t *tmp_call		= NULL;
	int i;

	lock_get(&_data.time.lock);

	if (_data.time.credit_data_by_client->table)
		for(i = 0; i < _data.time.credit_data_by_client->size; i++)
			clist_foreach_safe(&_data.time.credit_data_by_client->table[i], h_entry, tmp, next)
			{
				credit_data_t *credit_data	= (credit_data_t *) h_entry->u.p;
				call_t *call			= NULL;
				int total_consumed_secs		= 0;

				lock_get(&credit_data->lock);

				if (i > SAFE_ITERATION_THRESHOLD)
				{
					LM_ERR("Too many iterations for this loop: %d\n", i);
					break;
				}

				LM_DBG("Iterating through calls of client [%.*s]\n", credit_data->call_list->client_id.len, credit_data->call_list->client_id.s);

				clist_foreach_safe(credit_data->call_list, call, tmp_call, next)
				{
					if (!call->confirmed)
						continue;

					call->consumed_amount	= get_current_timestamp() - call->start_timestamp;
					total_consumed_secs	+= call->consumed_amount;

					if (call->consumed_amount > call->max_amount)
					{
						LM_ALERT("[%.*s] call has exhausted its time. Breaking the loop\n", call->sip_data.callid.len, call->sip_data.callid.s);
						break;
					}

					LM_DBG("CID [%.*s], start_timestamp [%d], seconds alive [%d]\n",
																			call->sip_data.callid.len, call->sip_data.callid.s,
																			call->start_timestamp,
																			(int) call->consumed_amount
																			);
				}

				if (credit_data->concurrent_calls == 0)
				{
					lock_release(&credit_data->lock);
					continue;
				}

				credit_data->consumed_amount	= credit_data->ended_calls_consumed_amount + total_consumed_secs;

				LM_DBG("Client [%.*s] | Ended-Calls-Time: %d  TotalTime/MaxTime: %d/%d\n", credit_data->call_list->client_id.len, credit_data->call_list->client_id.s,
																									(int) credit_data->ended_calls_consumed_amount,
																									(int) credit_data->consumed_amount,
																									(int) credit_data->max_amount);

				if (credit_data->consumed_amount >= credit_data->max_amount)
				{				
					lock_release(&_data.time.lock);
					terminate_all_calls(credit_data);
					lock_release(&credit_data->lock);
					return;
				}

				lock_release(&credit_data->lock);
			}

	lock_release(&_data.time.lock);
}

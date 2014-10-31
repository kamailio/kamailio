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
#ifndef _CNXCC_MOD_H
#define _CNXCC_MOD_H

#include "../../locking.h"
#include "../../str_hash.h"
#include "../../parser/parse_rr.h"

#define str_shm_free_if_not_null(_var_) if (_var_.s != NULL)  { shm_free(_var_.s); _var_.s = NULL; _var_.len = 0; }

typedef struct stats {
	unsigned int total;
	unsigned int active;
	unsigned int dropped;
} stats_t;

typedef enum cnxpvtypes {
	CNX_PV_ACTIVE = 1,
	CNX_PV_TOTAL,
	CNX_PV_DROPPED
} cnxpvtypes_t;

typedef enum credit_type {
	CREDIT_TIME,
	CREDIT_MONEY,
	CREDIT_CHANNEL
} credit_type_t;

typedef struct hash_tables {
	struct str_hash_table *credit_data_by_client;
	struct str_hash_table *call_data_by_cid;

	gen_lock_t lock;
} hash_tables_t;

struct redis;

typedef struct data {
	gen_lock_t lock;

	hash_tables_t time;
	hash_tables_t money;
	hash_tables_t channel;

	/*struct str_hash_table *credit_data_by_client;
	struct str_hash_table *call_data_by_cid;*/

	stats_t *stats;

	/*
	 * Call Shutdown Route Number
	 */
	int cs_route_number;

	/*
	 * Dialog flag used to track the call
	 */
	flag_t ctrl_flag;

	int check_period;

	str redis_cnn_str;
	struct {
		char host[40];
		int port;
		int db;
	} redis_cnn_info;
	struct redis *redis;

} data_t;

typedef struct sip_data {
	str callid;
	str from_uri;
	str from_tag;
	str to_uri;
	str to_tag;
} sip_data_t;

typedef struct money_spec_data {
	double cost_per_second;
	int initial_pulse;
	int final_pulse;

} money_spec_data_t;

struct call;
typedef struct call {
	struct call *prev;
	struct call *next;

	gen_lock_t lock;

	char confirmed;
	double max_amount;
	money_spec_data_t money_based;

	unsigned int start_timestamp;
	double consumed_amount;

	unsigned int dlg_h_entry;
	unsigned int dlg_h_id;

	str client_id;

	sip_data_t sip_data;
} call_t;

typedef struct call_array {
	call_t *array;
	int length;

} call_array_t;

typedef struct credit_data {
	gen_lock_t lock;

	double max_amount;
	double consumed_amount;
	double ended_calls_consumed_amount;
	int number_of_calls;
	int concurrent_calls;

	credit_type_t type;

	call_t *call_list;

	char *str_id;
	// flag to mark this instance in the process of being eliminated
	int deallocating:1;
} credit_data_t;


int try_get_call_entry(str *callid, call_t **call, hash_tables_t **hts);
int try_get_credit_data_entry(str *client_id, credit_data_t **credit_data);
int terminate_call(call_t *call);
void terminate_all_calls(credit_data_t *credit_data);

#endif /* _CNXCC_MOD_H */

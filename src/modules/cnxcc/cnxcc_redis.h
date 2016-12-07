/*
 * $Id$
 *
 * Copyright (C) 2014 Carlos Ruiz Díaz (caruizdiaz.com),
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

#ifndef CNXCC_REDIS_H_
#define CNXCC_REDIS_H_

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>

#include "../../str.h"
#include "cnxcc_mod.h"

struct redis {
        int db;
        short port;
        char* ip;
        redisContext *ctxt;
        redisAsyncContext *async_ctxt;
        struct event_base *eb;
};

struct redis *redis_connect(char *ip, int port, int db);
struct redis *redis_connect_async(char *ip, int port, int db);
struct redis *redis_connect_all(char *ip, int port, int db);
int redis_get_int(credit_data_t *credit_data, const char *instruction, const char *key, int *value);
int redis_get_str(credit_data_t *credit_data, const char *instruction, const char *key, str *value);
int redis_get_double(credit_data_t *credit_data, const char *instruction, const char *key, double *value);
int redis_get_or_create_credit_data(credit_data_t *credit_data);
int redis_insert_credit_data(credit_data_t *credit_data);
int redis_insert_int_value(credit_data_t *credit_data, const char* key, int value);
int redis_insert_double_value(credit_data_t *credit_data, const char* key, double value);
int redis_insert_str_value(credit_data_t *credit_data, const char* key, str *value);
int redis_kill_list_member_exists(credit_data_t *credit_data);
int redis_incr_by_int(credit_data_t *credit_data, const char *key, int value);
int redis_incr_by_double(credit_data_t *credit_data, const char *key, double value);
int redis_clean_up_if_last(credit_data_t *credit_data);
int redis_remove_kill_list_member(credit_data_t *credit_data);
int redis_append_kill_list_member(credit_data_t *credit_data);
int redis_publish_to_kill_list(credit_data_t *credit_data);

#endif /* CNXCC_REDIS_H_ */

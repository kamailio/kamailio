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

#include <stdlib.h>
#include "cnxcc_redis.h"
#include "cnxcc_mod.h"

#define DEFAULT_EXPIRE_SECS 70
extern data_t _data;

static int __redis_select_db(redisContext *ctxt, int db);
static int __redis_exec(credit_data_t *credit_data, const char *cmd, redisReply **rpl);
static struct redis *__redis_connect(struct redis *redis);
static void __async_connect_cb(const redisAsyncContext *c, int status);
static void __async_disconnect_cb(const redisAsyncContext *c, int status);
static void __subscription_cb(redisAsyncContext *c, void *r, void *privdata);
static struct redis *__redis_connect_async(struct redis *redis);
static struct redis *__alloc_redis(char *ip, int port, int db);
static void __redis_subscribe_to_kill_list(struct redis *redis) ;


static inline const char *__get_table_name(credit_type_t type) {
	switch(type) {
	case CREDIT_MONEY:
		return "money";
		break;
	case CREDIT_TIME:
		return "time";
		break;
	case CREDIT_CHANNEL:
		return "channel";
		break;
	default:
		LM_ERR("BUG: Something went terribly wrong: invalid credit type\n");
		return NULL;
	}
}

int redis_get_or_create_credit_data(credit_data_t *credit_data) {
	int exists = 0;

	// concurrent_calls is just a dummy key. It can be any of the valid keys
	if (redis_get_int(credit_data, "HEXISTS", "concurrent_calls" , &exists) < 0)
		goto error;

	if (!exists) {  // doesn't exist
		LM_INFO("credit_data with ID=[%s] DOES NOT exist in the cluster, creating it...\n", credit_data->str_id);
		return redis_insert_credit_data(credit_data);
	}

	LM_INFO("credit_data with ID=[%s] DOES exist in the cluster, retrieving it...\n", credit_data->str_id);

	if (redis_get_double(credit_data, "HGET", "consumed_amount", &credit_data->consumed_amount) < 0)
		goto error;

	if (redis_get_double(credit_data, "HGET", "ended_calls_consumed_amount", &credit_data->ended_calls_consumed_amount) < 0)
		goto error;

	if (redis_get_double(credit_data, "HGET", "max_amount", &credit_data->max_amount) < 0)
		goto error;

	if (redis_get_int(credit_data, "HGET", "type", (int *) &credit_data->type) < 0)
		goto error;

	return 1;
error:
	return -1;
}

int redis_insert_credit_data(credit_data_t *credit_data) {
	LM_DBG("Inserting credit_data_t using ID [%s]\n", credit_data->str_id);

	if (redis_insert_int_value(credit_data, "concurrent_calls", credit_data->concurrent_calls) < 0)
		goto error;

	if (redis_insert_double_value(credit_data, "consumed_amount", credit_data->consumed_amount) < 0)
		goto error;

	if (redis_insert_double_value(credit_data, "ended_calls_consumed_amount", credit_data->ended_calls_consumed_amount) < 0)
		goto error;

	if (redis_insert_double_value(credit_data, "max_amount", credit_data->max_amount) < 0)
		goto error;

	if (redis_insert_int_value(credit_data, "number_of_calls", credit_data->number_of_calls) < 0)
		goto error;

	if (redis_insert_int_value(credit_data, "type", credit_data->type) < 0)
		goto error;

	// make sure when don't have any leftover member on the kill list for this new entry
	if (redis_remove_kill_list_member(credit_data) < 0)
		goto error;

	return 1;
error:
	return -1;
}

static struct redis *__alloc_redis(char *ip, int port, int db) {
	struct redis *redis = pkg_malloc(sizeof(struct redis));
	int len = strlen(ip);

	redis->ip = pkg_malloc(len + 1);
	strcpy(redis->ip, ip);

	redis->port = port;
	redis->db = db;
	redis->ctxt = NULL;

	return redis;
}

struct redis *redis_connect_all(char *ip, int port, int db) {
	return __redis_connect_async(__redis_connect(__alloc_redis(ip, port, db)));
}

struct redis *redis_connect(char *ip, int port, int db) {
	return __redis_connect(__alloc_redis(ip, port, db));
}

struct redis *redis_connect_async(char *ip, int port, int db) {
	return __redis_connect_async(__alloc_redis(ip, port, db));
}

static struct redis *__redis_connect_async(struct redis *redis) {
	redis->eb = event_base_new();

	LM_INFO("Connecting (ASYNC) to Redis at %s:%d\n", redis->ip, redis->port);

	redis->async_ctxt = redisAsyncConnect(redis->ip, redis->port);

	if (redis->async_ctxt->err) {
		LM_ERR("%s\n", redis->async_ctxt->errstr);
		return NULL;
	}

	redisLibeventAttach(redis->async_ctxt, redis->eb);

	redisAsyncSetConnectCallback(redis->async_ctxt, __async_connect_cb);
	redisAsyncSetDisconnectCallback(redis->async_ctxt, __async_disconnect_cb);

	redisAsyncCommand(redis->async_ctxt, NULL, NULL, "SELECT %d", redis->db);
	__redis_subscribe_to_kill_list(redis);

	event_base_dispatch(redis->eb);
	return redis;
}

static struct redis *__redis_connect(struct redis *redis) {
	struct timeval timeout = { 1, 500000 }; // 1.5 seconds

	LM_INFO("Connecting to Redis at %s:%d\n", redis->ip, redis->port);

	if (redis->ctxt)
		redisFree(redis->ctxt);

	redis->ctxt = redisConnectWithTimeout(redis->ip, redis->port, timeout);

	if (redis->ctxt == NULL || redis->ctxt->err) {
		if (!redis->ctxt)
			LM_ERR("Connection error: can't allocate Redis context\n");
		else {
			LM_ERR("Connection error: %s\n", redis->ctxt->errstr);
			redisFree(redis->ctxt);
		}

		return NULL;
	}

	if (!__redis_select_db(redis->ctxt, redis->db))
		return NULL;

	return redis;
}

static int __redis_select_db(redisContext *ctxt, int db) {
	redisReply *rpl;
	rpl = redisCommand(ctxt, "SELECT %d", db);

	if (!rpl || rpl->type == REDIS_REPLY_ERROR) {
		if (!rpl)
			LM_ERR("%s\n", ctxt->errstr);
		else {
			LM_ERR("%.*s\n", rpl->len, rpl->str);
			freeReplyObject(rpl);
		}
		return -1;
	}

	return 1;
}

static int __redis_exec(credit_data_t *credit_data, const char *cmd, redisReply **rpl) {
	redisReply *rpl_aux;
	char cmd_buffer[1024];

	*rpl = redisCommand(_data.redis->ctxt, cmd);

	if (!(*rpl) || (*rpl)->type == REDIS_REPLY_ERROR) {
		if (!*rpl)
			LM_ERR("%s\n", _data.redis->ctxt->errstr);
		else {
			LM_ERR("%.*s\n", (*rpl)->len, (*rpl)->str);
			freeReplyObject(*rpl);
		}

		// reconnect on error
		__redis_connect(_data.redis);
		return -1;
	}

	if (credit_data == NULL) {
		freeReplyObject(*rpl);
		return 1;
	}

	// this will update the TTL of the key to DEFAULT_EXPIRE_SECS for every r/w.
	// It will guarantee us that if a server crashes, the key will automatically disappear
	// from Redis if no other client is updating the key, leaving us with some level of
	// consistency
	snprintf(cmd_buffer, sizeof(cmd_buffer), "EXPIRE cnxcc:%s:%s %d",
                                                __get_table_name(credit_data->type),
                                                credit_data->str_id,
                                                DEFAULT_EXPIRE_SECS);

	return __redis_exec(NULL, cmd_buffer, &rpl_aux);
}

int redis_incr_by_double(credit_data_t *credit_data, const char *key, double value) {
	redisReply *rpl = NULL;
	int ret = -1;
	char cmd_buffer[1024];

	snprintf(cmd_buffer, sizeof(cmd_buffer), "HINCRBYFLOAT cnxcc:%s:%s %s %f", __get_table_name(credit_data->type), credit_data->str_id, key, value);

	ret = __redis_exec(credit_data, cmd_buffer, &rpl);
	if (ret > 0)
		freeReplyObject(rpl);

	return ret;
}

int redis_get_double(credit_data_t *credit_data, const char *instruction, const char *key, double *value) {
	str str_double = {0, 0};
	char buffer[128];

	if (redis_get_str(credit_data, instruction, key, &str_double) < 0)
		return -1;

	snprintf(buffer, sizeof(buffer), "%.*s", str_double.len, str_double.s);
	*value = atof(buffer);

	LM_DBG("Got DOUBLE value: %s=%f\n", key, *value);

	pkg_free(str_double.s);
	return 1;
}

int redis_incr_by_int(credit_data_t *credit_data, const char *key, int value) {
	redisReply *rpl = NULL;
	int ret = -1;
	char cmd_buffer[1024];

	snprintf(cmd_buffer, sizeof(cmd_buffer), "HINCRBY cnxcc:%s:%s %s %d", __get_table_name(credit_data->type), credit_data->str_id, key, value);

	ret = __redis_exec(credit_data, cmd_buffer, &rpl);
	if (ret > 0)
		freeReplyObject(rpl);

	return ret;
}

int redis_get_int(credit_data_t *credit_data, const char *instruction, const char *key, int *value) {
	redisReply *rpl = NULL;
	char cmd_buffer[1024];
	snprintf(cmd_buffer, sizeof(cmd_buffer), "%s cnxcc:%s:%s %s", instruction, __get_table_name(credit_data->type), credit_data->str_id, key);

	if (__redis_exec(credit_data, cmd_buffer, &rpl) < 0)
		return -1;

	if (rpl->type == REDIS_REPLY_INTEGER)
		*value = rpl->integer;
	else if (rpl->type == REDIS_REPLY_NIL)
		*value = 0;
	else {
		*value = atoi(rpl->str);
	}

	freeReplyObject(rpl);

	LM_DBG("Got INT value: %s=%di\n", key, *value);
	return 1;
}

int redis_get_str(credit_data_t *credit_data, const char *instruction, const char *key, str *value) {
	redisReply *rpl = NULL;
	char cmd_buffer[1024];
	snprintf(cmd_buffer, sizeof(cmd_buffer), "%s cnxcc:%s:%s %s", instruction, __get_table_name(credit_data->type), credit_data->str_id, key);

	value->s = NULL;
	value->len = 0;

	if (__redis_exec(credit_data, cmd_buffer, &rpl) < 0)
		return -1;

	if (rpl->type != REDIS_REPLY_STRING && rpl->type != REDIS_REPLY_NIL) {
		LM_ERR("Redis reply to [%s] is not a string/nil: type[%d]\n", cmd_buffer, rpl->type);
		freeReplyObject(rpl);
		return -1;
	}

	if (rpl->type == REDIS_REPLY_NIL) {
		LM_INFO("Value of %s is (nil)\n", key);
		goto done;
	}

	if (rpl->len <= 0) {
		LM_ERR("RPL len is equal to %d\n", rpl->len);
		goto done;
	}

	value->s = pkg_malloc(rpl->len);
	value->len = rpl->len;
	memcpy(value->s, rpl->str, rpl->len);

done:
	freeReplyObject(rpl);

	LM_INFO("Got STRING value: %s=[%.*s]\n", key, value->len, value->s);
	return 1;
}

int redis_remove_credit_data(credit_data_t *credit_data) {
	redisReply *rpl = NULL;
	char cmd_buffer[1024];
	int ret;

	snprintf(cmd_buffer, sizeof(cmd_buffer), "DEL cnxcc:%s:%s",  __get_table_name(credit_data->type), credit_data->str_id);

	ret = __redis_exec(NULL, cmd_buffer, &rpl);

//	if (ret > 0)
//		freeReplyObject(rpl);

	return ret;
}

int redis_append_kill_list_member(credit_data_t *credit_data) {
	redisReply *rpl = NULL;
	char cmd_buffer[1024];
	int ret;

	snprintf(cmd_buffer, sizeof(cmd_buffer), "SADD cnxcc:kill_list:%s \"%s\"", __get_table_name(credit_data->type), credit_data->str_id);

	ret = __redis_exec(credit_data, cmd_buffer, &rpl);

	if (ret > 0)
		freeReplyObject(rpl);

	return ret;
}

int redis_remove_kill_list_member(credit_data_t *credit_data) {
	redisReply *rpl = NULL;
	char cmd_buffer[1024];
	int ret;

	snprintf(cmd_buffer, sizeof(cmd_buffer), "SREM cnxcc:kill_list:%s \"%s\"", __get_table_name(credit_data->type), credit_data->str_id);

	ret = __redis_exec(credit_data, cmd_buffer, &rpl);

	if (ret > 0)
		freeReplyObject(rpl);

	return ret;
}

int redis_insert_str_value(credit_data_t *credit_data, const char* key, str *value) {
	redisReply *rpl = NULL;
	int ret = -1;
	char cmd_buffer[2048];

	if (value == NULL) {
		LM_ERR("str value is null\n");
		return -1;
	}

	if (value->len == 0) {
		LM_WARN("[%s] value is empty\n", key);
		return 1;
	}

	snprintf(cmd_buffer, sizeof(cmd_buffer), "HSET cnxcc:%s:%s %s \"%.*s\"", __get_table_name(credit_data->type), credit_data->str_id, key, value->len, value->s);

	ret = __redis_exec(credit_data, cmd_buffer, &rpl);
	if (ret > 0)
		freeReplyObject(rpl);

	return ret;
}

int redis_insert_int_value(credit_data_t *credit_data, const char* key, int value) {
	redisReply *rpl = NULL;
	int ret = -1;
	char cmd_buffer[1024];

	snprintf(cmd_buffer, sizeof(cmd_buffer), "HSET cnxcc:%s:%s %s %d", __get_table_name(credit_data->type), credit_data->str_id, key, value);

	ret = __redis_exec(credit_data, cmd_buffer, &rpl);
	if (ret > 0)
		freeReplyObject(rpl);

	return ret;
}

int redis_insert_double_value(credit_data_t *credit_data, const char* key, double value) {
	redisReply *rpl = NULL;
	int ret = -1;
	char cmd_buffer[1024];

	snprintf(cmd_buffer, sizeof(cmd_buffer), "HSET cnxcc:%s:%s %s %f", __get_table_name(credit_data->type), credit_data->str_id, key, value);

	ret = __redis_exec(credit_data, cmd_buffer, &rpl);
	if (ret > 0)
		freeReplyObject(rpl);

	return ret;
}

int redis_kill_list_member_exists(credit_data_t *credit_data) {
	redisReply *rpl;
	int exists = 0;
	char cmd_buffer[1024];

	snprintf(cmd_buffer, sizeof(cmd_buffer), "SISMEMBER cnxcc:kill_list:%s \"%s\"", __get_table_name(credit_data->type), credit_data->str_id);

	if (__redis_exec(credit_data, cmd_buffer, &rpl) < 0)
		return -1;

	exists = rpl->integer;

	freeReplyObject(rpl);

	return exists;
}

int redis_clean_up_if_last(credit_data_t *credit_data) {
	int counter = 0;

	if (redis_get_int(credit_data, "HGET", "number_of_calls", &counter) < 0)
		return -1;

	return counter > 0 ? 1 : redis_remove_credit_data(credit_data);
}

static void __redis_subscribe_to_kill_list(struct redis *redis) {
	redisAsyncCommand(redis->async_ctxt, __subscription_cb, NULL, "SUBSCRIBE cnxcc:kill_list");
}

int redis_publish_to_kill_list(credit_data_t *credit_data) {
	redisReply *rpl;
	char cmd_buffer[1024];
	snprintf(cmd_buffer, sizeof(cmd_buffer), "PUBLISH cnxcc:kill_list %s", credit_data->str_id);

	return __redis_exec(NULL, cmd_buffer, &rpl) < 0;
}

static void __async_connect_cb(const redisAsyncContext *c, int status) {
	if (status != REDIS_OK) {
		LM_ERR("error connecting to Redis db in async mode\n");
		abort();
	}

	LM_INFO("connected to Redis in async mode\n");
}

static void __async_disconnect_cb(const redisAsyncContext *c, int status) {
	LM_ERR("async DB connection was lost\n");
}

static void __subscription_cb(redisAsyncContext *c, void *r, void *privdata) {
	 redisReply *reply = r;
	 str key;
	 credit_data_t *credit_data;

	 if (reply == NULL) {
		 LM_ERR("reply is NULL\n");
		 return;
	 }

	 if ( reply->type != REDIS_REPLY_ARRAY || reply->elements != 3 )
		 return;

	 if (strcmp(reply->element[1]->str, "cnxcc:kill_list" ) != 0)
		 return;

	 if (!reply->element[2]->str)
		 return;

	 key.len = strlen(reply->element[2]->str);

	 if (key.len <= 0) {
		 LM_ERR("Invalid credit_data key\n");
		 return;
	 }

	 key.s = reply->element[2]->str;

	 if (try_get_credit_data_entry(&key, &credit_data) < 0)
		 return;

	 lock_get(&credit_data->lock);

	 if (credit_data->deallocating)
		 goto done; // no need to terminate the calls. They are already being terminated

	 LM_ALERT("Got kill list entry for key [%.*s]\n", key.len, key.s);
	 terminate_all_calls(credit_data);
done:
	 lock_release(&credit_data->lock);

}

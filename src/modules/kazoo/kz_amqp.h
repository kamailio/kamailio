/*
 * $Id$
 *
 * Kazoo module interface
 *
 * Copyright (C) 2010-2014 2600Hz
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
 * History:
 * --------
 * 2014-08  first version (2600hz)
 */

#ifndef KZ_AMQP_H_
#define KZ_AMQP_H_

#include <fcntl.h>
#include <event.h>
#include <sys/timerfd.h>
#include <amqp.h>

#include "../../core/sr_module.h"
#include "../../core/str.h"

#include "const.h"
#include "defs.h"
#include "../../core/fmsg.h"

typedef enum {
	KZ_AMQP_CONNECTION_CLOSED     = 0,
	KZ_AMQP_CONNECTION_OPEN    = 1,
	KZ_AMQP_CONNECTION_FAILURE    = 2
} kz_amqp_connection_state;

typedef enum {
	KZ_AMQP_CMD_PUBLISH     = 1,
	KZ_AMQP_CMD_CALL    = 2,
	KZ_AMQP_CMD_CONSUME = 3,
	KZ_AMQP_CMD_ACK = 4,
	KZ_AMQP_CMD_TARGETED_CONSUMER = 5,
	KZ_AMQP_CMD_PUBLISH_BROADCAST = 6,
	KZ_AMQP_CMD_COLLECT = 7,
	KZ_AMQP_CMD_ASYNC_CALL    = 8,
	KZ_AMQP_CMD_ASYNC_COLLECT    = 9
} kz_amqp_pipe_cmd_type;

typedef enum {
	KZ_AMQP_CHANNEL_CLOSED     = 0,
	KZ_AMQP_CHANNEL_FREE     = 1,
	KZ_AMQP_CHANNEL_PUBLISHING    = 2,
	KZ_AMQP_CHANNEL_BINDED = 3,
	KZ_AMQP_CHANNEL_CALLING    = 4,
	KZ_AMQP_CHANNEL_CONSUMING = 5
} kz_amqp_channel_state;

typedef struct amqp_connection_info kz_amqp_connection_info;
typedef kz_amqp_connection_info *kz_amqp_connection_info_ptr;

extern int dbk_channels;
extern str dbk_node_hostname;
extern str dbk_consumer_event_key;
extern str dbk_consumer_event_subkey;
extern int dbk_consumer_workers;

typedef struct kz_amqp_connection_t {
	kz_amqp_connection_info info;
	char* url;
} kz_amqp_connection, *kz_amqp_connection_ptr;

typedef struct kz_amqp_timer_t {
	struct event *ev;
	struct itimerspec *timer;
	int    fd;
} kz_amqp_timer, *kz_amqp_timer_ptr;

typedef struct kz_amqp_conn_t {
	struct kz_amqp_server_t* server;
	amqp_connection_state_t conn;
	kz_amqp_connection_state state;
	kz_amqp_timer_ptr reconnect;
	kz_amqp_timer_ptr heartbeat;
	amqp_socket_t *socket;
	amqp_channel_t channel_count;
	amqp_channel_t channel_counter;
} kz_amqp_conn, *kz_amqp_conn_ptr;

typedef struct {
	kz_amqp_conn_ptr current;
	kz_amqp_conn_ptr head;
	kz_amqp_conn_ptr tail;
} kz_amqp_conn_pool, *kz_amqp_conn_pool_ptr;


/*
#define AMQP_KZ_CMD_PUBLISH       1
#define AMQP_KZ_CMD_CALL          2
#define AMQP_KZ_CMD_CONSUME       3
*/

typedef struct {
    gen_lock_t lock;
	kz_amqp_pipe_cmd_type type;
	char* exchange;
	char* exchange_type;
	char* routing_key;
	char* reply_routing_key;
	char* queue;
	char* payload;
	char* return_payload;
	char* headers;
	str* message_id;
	int   return_code;
	int   consumer;
	int   server_id;
	uint64_t delivery_tag;
	amqp_channel_t channel;
	struct timeval timeout;

	/* async */
	char *cb_route;
	char *err_route;
	unsigned int t_hash;
	unsigned int t_label;


} kz_amqp_cmd, *kz_amqp_cmd_ptr;

typedef struct {
	str* message_id;

	/* timer */
	struct event *timer_ev;
	int timerfd;

} kz_amqp_cmd_timeout, *kz_amqp_cmd_timeout_ptr;

typedef struct kz_amqp_cmd_entry_t {
	kz_amqp_cmd_ptr cmd;
	struct kz_amqp_cmd_entry_t* next;
} kz_amqp_cmd_entry, *kz_amqp_cmd_entry_ptr;

typedef struct kz_amqp_cmd_table_t {
	kz_amqp_cmd_entry_ptr entries;
	gen_lock_t lock;
} kz_amqp_cmd_table, *kz_amqp_cmd_table_ptr;


typedef struct {
	char* payload;
	uint64_t delivery_tag;
	amqp_channel_t channel;
	char* event_key;
	char* event_subkey;
	str* message_id;
	str* routing_key;
	kz_amqp_cmd_ptr cmd;
} kz_amqp_consumer_delivery, *kz_amqp_consumer_delivery_ptr;

typedef struct {
	amqp_bytes_t name;
	amqp_bytes_t type;
	amqp_boolean_t passive;
	amqp_boolean_t durable;
	amqp_boolean_t auto_delete;
	amqp_boolean_t internal;
} kz_amqp_exchange, *kz_amqp_exchange_ptr;

typedef struct {
	amqp_bytes_t name;
	amqp_boolean_t passive;
	amqp_boolean_t durable;
	amqp_boolean_t exclusive;
	amqp_boolean_t auto_delete;
} kz_amqp_queue, *kz_amqp_queue_ptr;

typedef struct kz_amqp_routings_t {
	amqp_bytes_t routing;
	struct kz_amqp_routings_t* next;
} kz_amqp_routings, *kz_amqp_routings_ptr;

typedef struct kz_amqp_exchange_binding_t {
	kz_amqp_exchange_ptr from_exchange;
	kz_amqp_routings_ptr routing;
	struct kz_amqp_exchange_binding_t* next;
} kz_amqp_exchange_binding, *kz_amqp_exchange_binding_ptr;

typedef struct {
//	amqp_bytes_t exchange;
//	amqp_bytes_t exchange_type;
	kz_amqp_exchange_ptr exchange;
	kz_amqp_exchange_binding_ptr exchange_bindings;
	kz_amqp_queue_ptr queue;
	kz_amqp_routings_ptr queue_bindings;
//	amqp_bytes_t routing_key;
//	amqp_bytes_t queue;
	amqp_bytes_t event_key;
	amqp_bytes_t event_subkey;
//	amqp_boolean_t passive;
//	amqp_boolean_t durable;
//	amqp_boolean_t exclusive;
//	amqp_boolean_t auto_delete;
	amqp_boolean_t no_ack;
	amqp_boolean_t wait_for_consumer_ack;
	amqp_boolean_t federate;
    amqp_boolean_t consistent_worker;
    str* consistent_worker_key;
} kz_amqp_bind, *kz_amqp_bind_ptr;

typedef struct {
	kz_amqp_cmd_ptr cmd;
	kz_amqp_bind_ptr targeted;
	kz_amqp_bind_ptr consumer;
	amqp_channel_t channel;
	kz_amqp_channel_state state;
	struct timeval timer;
	gen_lock_t lock;
} kz_amqp_channel, *kz_amqp_channel_ptr;

typedef struct kz_amqp_binding_t {
	kz_amqp_bind_ptr bind;
    struct kz_amqp_binding_t* next;
} kz_amqp_binding, *kz_amqp_binding_ptr;

typedef struct {
	kz_amqp_binding_ptr head;
	kz_amqp_binding_ptr tail;
} kz_amqp_bindings, *kz_amqp_bindings_ptr;

typedef struct kz_amqp_server_t {
	int id;
	int channel_index;
	struct kz_amqp_zone_t* zone;
	kz_amqp_connection_ptr connection;
	kz_amqp_conn_ptr producer;
//	kz_amqp_conn_ptr consumer;
	kz_amqp_channel_ptr channels;
//	kz_amqp_channel_ptr consumer_channels;
    struct kz_amqp_server_t* next;
} kz_amqp_server, *kz_amqp_server_ptr;

typedef struct kz_amqp_servers_t {
	kz_amqp_server_ptr head;
	kz_amqp_server_ptr tail;
} kz_amqp_servers, *kz_amqp_servers_ptr;

typedef struct kz_amqp_zone_t {
	char* zone;
	kz_amqp_servers_ptr servers;
    struct kz_amqp_zone_t* next;
} kz_amqp_zone, *kz_amqp_zone_ptr;

typedef struct kz_amqp_zones_t {
	kz_amqp_zone_ptr head;
	kz_amqp_zone_ptr tail;
} kz_amqp_zones, *kz_amqp_zones_ptr;

int kz_amqp_init();
void kz_amqp_destroy();
int kz_amqp_add_connection(modparam_t type, void* val);

int kz_amqp_publish(struct sip_msg* msg, char* exchange, char* routing_key, char* payload);
int kz_amqp_publish_ex(struct sip_msg* msg, char* exchange, char* routing_key, char* payload, char* headers);
int ki_kz_amqp_publish(sip_msg_t* msg, str* exchange, str* routing_key, str* payload, str* headers);
int ki_kz_amqp_publish_hdrs(sip_msg_t* msg, str* exchange, str* routing_key, str* payload);
int kz_amqp_query(struct sip_msg* msg, char* exchange, char* routing_key, char* payload, char* dst, char* headers);
int kz_amqp_query_ex(struct sip_msg* msg, char* exchange, char* routing_key, char* payload, char* headers);
int kz_amqp_subscribe(struct sip_msg* msg, char* payload);
int ki_kz_amqp_subscribe(sip_msg_t* msg, str* payload);
int kz_amqp_subscribe_simple(struct sip_msg* msg, char* exchange, char* exchange_type, char* queue_name, char* routing_key);
int kz_amqp_encode(struct sip_msg* msg, char* unencoded, char* encoded);
int kz_amqp_encode_ex(str* unencoded, pv_value_p dst_val);

int kz_amqp_async_query(struct sip_msg* msg, char* exchange, char* routing_key, char* payload, char* _cb_route, char* _err_route);
int kz_amqp_async_query_ex(struct sip_msg* msg, char* _exchange, char* _routing_key, char* _payload, char* _cb_route, char* _err_route, char* _pub_flags);

//void kz_amqp_generic_consumer_loop(int child_no);
void kz_amqp_manager_loop(int child_no);

int kz_amqp_consumer_proc(kz_amqp_server_ptr server_ptr);
int kz_amqp_publisher_proc(int cmd_pipe);
int kz_amqp_timeout_proc();
int kz_amqp_consumer_worker_proc(int cmd_pipe);

int kz_amqp_handle_server_failure(kz_amqp_conn_ptr connection);

int kz_pv_get_event_payload(struct sip_msg *msg, pv_param_t *param,	pv_value_t *res);
int kz_pv_get_event_routing_key(struct sip_msg *msg, pv_param_t *param,	pv_value_t *res);
int kz_pv_get_last_query_result(struct sip_msg *msg, pv_param_t *param,	pv_value_t *res);
int kz_pv_get_connection_host(struct sip_msg *msg, pv_param_t *param,	pv_value_t *res);

/* callid generator */
int kz_callid_init(void);
int kz_callid_child_init(int rank);
void kz_generate_callid(str* callid);

kz_amqp_zone_ptr kz_amqp_get_primary_zone();
kz_amqp_zone_ptr kz_amqp_get_zones();
kz_amqp_zone_ptr kz_amqp_get_zone(char* zone);
kz_amqp_zone_ptr kz_amqp_add_zone(char* zone);

void kz_amqp_fire_connection_event(char *event, char* host, char* zone);

void kz_amqp_free_pipe_cmd(kz_amqp_cmd_ptr cmd);

void kz_amqp_timer_destroy(kz_amqp_timer_ptr* pTimer);
int kz_amqp_timer_create(kz_amqp_timer_ptr* pTimer, int seconds, void (*callback)(int, short, void *), void *data);
void kz_amqp_heartbeat_proc(int fd, short event, void *arg);

void kz_amqp_queue_free(kz_amqp_queue_ptr exchange);
void kz_amqp_exchange_free(kz_amqp_exchange_ptr exchange);
void kz_amqp_exchange_bindings_free(kz_amqp_exchange_binding_ptr binding);
void kz_amqp_routing_free(kz_amqp_routings_ptr routing);
kz_amqp_queue_ptr kz_amqp_queue_new(str *name);
kz_amqp_exchange_ptr kz_amqp_exchange_new(str *name, str* type);
kz_amqp_routings_ptr kz_amqp_routing_new(char* routing);

int add_amqp_headers (char * headers, amqp_basic_properties_t * props );

static inline int kz_amqp_error(char const *context, amqp_rpc_reply_t x)
{
	amqp_connection_close_t *mconn;
	amqp_channel_close_t *mchan;

	switch (x.reply_type) {
		case AMQP_RESPONSE_NORMAL:
			return 0;

		case AMQP_RESPONSE_NONE:
			LM_ERR("%s: missing RPC reply type!", context);
			break;

		case AMQP_RESPONSE_LIBRARY_EXCEPTION:
			LM_ERR("%s: %s\n", context,  "(end-of-stream)");
			break;

		case AMQP_RESPONSE_SERVER_EXCEPTION:
			switch (x.reply.id) {
				case AMQP_CONNECTION_CLOSE_METHOD:
					mconn = (amqp_connection_close_t *)x.reply.decoded;
					LM_ERR("%s: server connection error %d, message: %.*s",
							context, mconn->reply_code,
							(int)mconn->reply_text.len,
							(char *)mconn->reply_text.bytes);
					break;
				case AMQP_CHANNEL_CLOSE_METHOD:
						mchan = (amqp_channel_close_t *)x.reply.decoded;
					LM_ERR("%s: server channel error %d, message: %.*s",
							context, mchan->reply_code,
							(int)mchan->reply_text.len,
							(char *)mchan->reply_text.bytes);
					break;
				default:
					LM_ERR("%s: unknown server error, method id 0x%08X",
							context, x.reply.id);
					break;
			}
			break;
	}
	return -1;
}


#endif /* KZ_AMQP_H_ */


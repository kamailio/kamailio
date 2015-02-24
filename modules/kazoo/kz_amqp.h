/*
 * kz_amqp.h
 *
 *  Created on: Jul 29, 2014
 *      Author: root
 */

#ifndef KZ_AMQP_H_
#define KZ_AMQP_H_

#include <amqp.h>

#include "../../sr_module.h"

#include "const.h"
#include "defs.h"
#include "../../lib/kcore/faked_msg.h"

typedef struct amqp_connection_info kz_amqp_connection_info;
typedef kz_amqp_connection_info *kz_amqp_connection_info_ptr;

extern int dbk_channels;
extern str dbk_node_hostname;
extern str dbk_consumer_event_key;
extern str dbk_consumer_event_subkey;
extern int dbk_consumer_processes;

typedef struct kz_amqp_connection_t {
	kz_amqp_connection_info info;
	char* url;
    struct kz_amqp_connection_t* next;
} kz_amqp_connection, *kz_amqp_connection_ptr;

typedef struct {
	kz_amqp_connection_ptr current;
	kz_amqp_connection_ptr head;
	kz_amqp_connection_ptr tail;
} kz_amqp_connection_pool, *kz_amqp_connection_pool_ptr;

typedef struct kz_amqp_conn_t {
	kz_amqp_connection_ptr info;
	amqp_connection_state_t conn;
	amqp_socket_t *socket;
	amqp_channel_t channel_count;
	amqp_channel_t channel_counter;
    struct kz_amqp_conn_t* next;
} kz_amqp_conn, *kz_amqp_conn_ptr;

typedef struct {
	kz_amqp_conn_ptr current;
	kz_amqp_conn_ptr head;
	kz_amqp_conn_ptr tail;
} kz_amqp_conn_pool, *kz_amqp_conn_pool_ptr;


#define AMQP_KZ_CMD_PUBLISH       1
#define AMQP_KZ_CMD_CALL          2
#define AMQP_KZ_CMD_CONSUME       3

typedef enum {
	KZ_AMQP_PUBLISH     = 1,
	KZ_AMQP_CALL    = 2,
	KZ_AMQP_CONSUME = 3,
	KZ_AMQP_ACK = 4
} kz_amqp_pipe_cmd_type;

typedef enum {
	KZ_AMQP_CLOSED     = 0,
	KZ_AMQP_FREE     = 1,
	KZ_AMQP_PUBLISHING    = 2,
	KZ_AMQP_BINDED = 3,
	KZ_AMQP_CALLING    = 4,
	KZ_AMQP_CONSUMING = 5
} kz_amqp_channel_state;

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
	int   return_code;
	int   consumer;
	uint64_t delivery_tag;
	amqp_channel_t channel;
	struct timeval timeout;
} kz_amqp_cmd, *kz_amqp_cmd_ptr;

typedef struct {
	char* payload;
	uint64_t delivery_tag;
	amqp_channel_t channel;
	char* event_key;
	char* event_subkey;
} kz_amqp_consumer_delivery, *kz_amqp_consumer_delivery_ptr;

typedef struct {
	amqp_bytes_t exchange;
	amqp_bytes_t exchange_type;
	amqp_bytes_t routing_key;
	amqp_bytes_t queue;
	amqp_bytes_t event_key;
	amqp_bytes_t event_subkey;
	amqp_boolean_t passive;
	amqp_boolean_t durable;
	amqp_boolean_t exclusive;
	amqp_boolean_t auto_delete;
	amqp_boolean_t no_ack;
	amqp_boolean_t wait_for_consumer_ack;
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

int kz_amqp_init();
void kz_amqp_destroy();
int kz_amqp_add_connection(modparam_t type, void* val);

int kz_amqp_publish(struct sip_msg* msg, char* exchange, char* routing_key, char* payload);
int kz_amqp_query(struct sip_msg* msg, char* exchange, char* routing_key, char* payload, char* dst);
int kz_amqp_query_ex(struct sip_msg* msg, char* exchange, char* routing_key, char* payload);
int kz_amqp_subscribe(struct sip_msg* msg, char* payload);
int kz_amqp_subscribe_simple(struct sip_msg* msg, char* exchange, char* exchange_type, char* queue_name, char* routing_key);
int kz_amqp_encode(struct sip_msg* msg, char* unencoded, char* encoded);
int kz_amqp_encode_ex(str* unencoded, pv_value_p dst_val);
//void kz_amqp_presence_consumer_loop(int child_no);
void kz_amqp_consumer_loop(int child_no);

//void kz_amqp_generic_consumer_loop(int child_no);
void kz_amqp_manager_loop(int child_no);

void kz_amqp_consumer_proc(int child_no);
void kz_amqp_publisher_proc(int child_no);
void kz_amqp_timeout_proc(int child_no);

int kz_pv_get_event_payload(struct sip_msg *msg, pv_param_t *param,	pv_value_t *res);
int kz_pv_get_last_query_result(struct sip_msg *msg, pv_param_t *param,	pv_value_t *res);
int kz_pv_get_connection_host(struct sip_msg *msg, pv_param_t *param,	pv_value_t *res);

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

/* Generated code. Do not edit. Edit and re-run codegen.py instead.
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MIT
 *
 * Portions created by Alan Antonuk are Copyright (c) 2012-2013
 * Alan Antonuk. All Rights Reserved.
 *
 * Portions created by VMware are Copyright (c) 2007-2012 VMware, Inc.
 * All Rights Reserved.
 *
 * Portions created by Tony Garnock-Jones are Copyright (c) 2009-2010
 * VMware, Inc. and Tony Garnock-Jones. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ***** END LICENSE BLOCK *****
 */

#ifndef AMQP_FRAMING_H
#define AMQP_FRAMING_H

#include <amqp.h>

AMQP_BEGIN_DECLS

#define AMQP_PROTOCOL_VERSION_MAJOR 0
#define AMQP_PROTOCOL_VERSION_MINOR 9
#define AMQP_PROTOCOL_VERSION_REVISION 1
#define AMQP_PROTOCOL_PORT 5672
#define AMQP_FRAME_METHOD 1
#define AMQP_FRAME_HEADER 2
#define AMQP_FRAME_BODY 3
#define AMQP_FRAME_HEARTBEAT 8
#define AMQP_FRAME_MIN_SIZE 4096
#define AMQP_FRAME_END 206
#define AMQP_REPLY_SUCCESS 200
#define AMQP_CONTENT_TOO_LARGE 311
#define AMQP_NO_ROUTE 312
#define AMQP_NO_CONSUMERS 313
#define AMQP_ACCESS_REFUSED 403
#define AMQP_NOT_FOUND 404
#define AMQP_RESOURCE_LOCKED 405
#define AMQP_PRECONDITION_FAILED 406
#define AMQP_CONNECTION_FORCED 320
#define AMQP_INVALID_PATH 402
#define AMQP_FRAME_ERROR 501
#define AMQP_SYNTAX_ERROR 502
#define AMQP_COMMAND_INVALID 503
#define AMQP_CHANNEL_ERROR 504
#define AMQP_UNEXPECTED_FRAME 505
#define AMQP_RESOURCE_ERROR 506
#define AMQP_NOT_ALLOWED 530
#define AMQP_NOT_IMPLEMENTED 540
#define AMQP_INTERNAL_ERROR 541

/* Function prototypes. */

AMQP_PUBLIC_FUNCTION
char const *
AMQP_CALL amqp_constant_name(int constantNumber);

AMQP_PUBLIC_FUNCTION
amqp_boolean_t
AMQP_CALL amqp_constant_is_hard_error(int constantNumber);

AMQP_PUBLIC_FUNCTION
char const *
AMQP_CALL amqp_method_name(amqp_method_number_t methodNumber);

AMQP_PUBLIC_FUNCTION
amqp_boolean_t
AMQP_CALL amqp_method_has_content(amqp_method_number_t methodNumber);

AMQP_PUBLIC_FUNCTION
int
AMQP_CALL amqp_decode_method(amqp_method_number_t methodNumber,
		   amqp_pool_t *pool,
		   amqp_bytes_t encoded,
		   void **decoded);

AMQP_PUBLIC_FUNCTION
int
AMQP_CALL amqp_decode_properties(uint16_t class_id,
            amqp_pool_t *pool,
            amqp_bytes_t encoded,
            void **decoded);

AMQP_PUBLIC_FUNCTION
int
AMQP_CALL amqp_encode_method(amqp_method_number_t methodNumber,
		   void *decoded,
		   amqp_bytes_t encoded);

AMQP_PUBLIC_FUNCTION
int
AMQP_CALL amqp_encode_properties(uint16_t class_id,
		       void *decoded,
		       amqp_bytes_t encoded);

/* Method field records. */

#define AMQP_CONNECTION_START_METHOD ((amqp_method_number_t) 0x000A000A) /* 10, 10; 655370 */
typedef struct amqp_connection_start_t_ {
  uint8_t version_major;
  uint8_t version_minor;
  amqp_table_t server_properties;
  amqp_bytes_t mechanisms;
  amqp_bytes_t locales;
} amqp_connection_start_t;

#define AMQP_CONNECTION_START_OK_METHOD ((amqp_method_number_t) 0x000A000B) /* 10, 11; 655371 */
typedef struct amqp_connection_start_ok_t_ {
  amqp_table_t client_properties;
  amqp_bytes_t mechanism;
  amqp_bytes_t response;
  amqp_bytes_t locale;
} amqp_connection_start_ok_t;

#define AMQP_CONNECTION_SECURE_METHOD ((amqp_method_number_t) 0x000A0014) /* 10, 20; 655380 */
typedef struct amqp_connection_secure_t_ {
  amqp_bytes_t challenge;
} amqp_connection_secure_t;

#define AMQP_CONNECTION_SECURE_OK_METHOD ((amqp_method_number_t) 0x000A0015) /* 10, 21; 655381 */
typedef struct amqp_connection_secure_ok_t_ {
  amqp_bytes_t response;
} amqp_connection_secure_ok_t;

#define AMQP_CONNECTION_TUNE_METHOD ((amqp_method_number_t) 0x000A001E) /* 10, 30; 655390 */
typedef struct amqp_connection_tune_t_ {
  uint16_t channel_max;
  uint32_t frame_max;
  uint16_t heartbeat;
} amqp_connection_tune_t;

#define AMQP_CONNECTION_TUNE_OK_METHOD ((amqp_method_number_t) 0x000A001F) /* 10, 31; 655391 */
typedef struct amqp_connection_tune_ok_t_ {
  uint16_t channel_max;
  uint32_t frame_max;
  uint16_t heartbeat;
} amqp_connection_tune_ok_t;

#define AMQP_CONNECTION_OPEN_METHOD ((amqp_method_number_t) 0x000A0028) /* 10, 40; 655400 */
typedef struct amqp_connection_open_t_ {
  amqp_bytes_t virtual_host;
  amqp_bytes_t capabilities;
  amqp_boolean_t insist;
} amqp_connection_open_t;

#define AMQP_CONNECTION_OPEN_OK_METHOD ((amqp_method_number_t) 0x000A0029) /* 10, 41; 655401 */
typedef struct amqp_connection_open_ok_t_ {
  amqp_bytes_t known_hosts;
} amqp_connection_open_ok_t;

#define AMQP_CONNECTION_CLOSE_METHOD ((amqp_method_number_t) 0x000A0032) /* 10, 50; 655410 */
typedef struct amqp_connection_close_t_ {
  uint16_t reply_code;
  amqp_bytes_t reply_text;
  uint16_t class_id;
  uint16_t method_id;
} amqp_connection_close_t;

#define AMQP_CONNECTION_CLOSE_OK_METHOD ((amqp_method_number_t) 0x000A0033) /* 10, 51; 655411 */
typedef struct amqp_connection_close_ok_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_connection_close_ok_t;

#define AMQP_CONNECTION_BLOCKED_METHOD ((amqp_method_number_t) 0x000A003C) /* 10, 60; 655420 */
typedef struct amqp_connection_blocked_t_ {
  amqp_bytes_t reason;
} amqp_connection_blocked_t;

#define AMQP_CONNECTION_UNBLOCKED_METHOD ((amqp_method_number_t) 0x000A003D) /* 10, 61; 655421 */
typedef struct amqp_connection_unblocked_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_connection_unblocked_t;

#define AMQP_CHANNEL_OPEN_METHOD ((amqp_method_number_t) 0x0014000A) /* 20, 10; 1310730 */
typedef struct amqp_channel_open_t_ {
  amqp_bytes_t out_of_band;
} amqp_channel_open_t;

#define AMQP_CHANNEL_OPEN_OK_METHOD ((amqp_method_number_t) 0x0014000B) /* 20, 11; 1310731 */
typedef struct amqp_channel_open_ok_t_ {
  amqp_bytes_t channel_id;
} amqp_channel_open_ok_t;

#define AMQP_CHANNEL_FLOW_METHOD ((amqp_method_number_t) 0x00140014) /* 20, 20; 1310740 */
typedef struct amqp_channel_flow_t_ {
  amqp_boolean_t active;
} amqp_channel_flow_t;

#define AMQP_CHANNEL_FLOW_OK_METHOD ((amqp_method_number_t) 0x00140015) /* 20, 21; 1310741 */
typedef struct amqp_channel_flow_ok_t_ {
  amqp_boolean_t active;
} amqp_channel_flow_ok_t;

#define AMQP_CHANNEL_CLOSE_METHOD ((amqp_method_number_t) 0x00140028) /* 20, 40; 1310760 */
typedef struct amqp_channel_close_t_ {
  uint16_t reply_code;
  amqp_bytes_t reply_text;
  uint16_t class_id;
  uint16_t method_id;
} amqp_channel_close_t;

#define AMQP_CHANNEL_CLOSE_OK_METHOD ((amqp_method_number_t) 0x00140029) /* 20, 41; 1310761 */
typedef struct amqp_channel_close_ok_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_channel_close_ok_t;

#define AMQP_ACCESS_REQUEST_METHOD ((amqp_method_number_t) 0x001E000A) /* 30, 10; 1966090 */
typedef struct amqp_access_request_t_ {
  amqp_bytes_t realm;
  amqp_boolean_t exclusive;
  amqp_boolean_t passive;
  amqp_boolean_t active;
  amqp_boolean_t write;
  amqp_boolean_t read;
} amqp_access_request_t;

#define AMQP_ACCESS_REQUEST_OK_METHOD ((amqp_method_number_t) 0x001E000B) /* 30, 11; 1966091 */
typedef struct amqp_access_request_ok_t_ {
  uint16_t ticket;
} amqp_access_request_ok_t;

#define AMQP_EXCHANGE_DECLARE_METHOD ((amqp_method_number_t) 0x0028000A) /* 40, 10; 2621450 */
typedef struct amqp_exchange_declare_t_ {
  uint16_t ticket;
  amqp_bytes_t exchange;
  amqp_bytes_t type;
  amqp_boolean_t passive;
  amqp_boolean_t durable;
  amqp_boolean_t auto_delete;
  amqp_boolean_t internal;
  amqp_boolean_t nowait;
  amqp_table_t arguments;
} amqp_exchange_declare_t;

#define AMQP_EXCHANGE_DECLARE_OK_METHOD ((amqp_method_number_t) 0x0028000B) /* 40, 11; 2621451 */
typedef struct amqp_exchange_declare_ok_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_exchange_declare_ok_t;

#define AMQP_EXCHANGE_DELETE_METHOD ((amqp_method_number_t) 0x00280014) /* 40, 20; 2621460 */
typedef struct amqp_exchange_delete_t_ {
  uint16_t ticket;
  amqp_bytes_t exchange;
  amqp_boolean_t if_unused;
  amqp_boolean_t nowait;
} amqp_exchange_delete_t;

#define AMQP_EXCHANGE_DELETE_OK_METHOD ((amqp_method_number_t) 0x00280015) /* 40, 21; 2621461 */
typedef struct amqp_exchange_delete_ok_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_exchange_delete_ok_t;

#define AMQP_EXCHANGE_BIND_METHOD ((amqp_method_number_t) 0x0028001E) /* 40, 30; 2621470 */
typedef struct amqp_exchange_bind_t_ {
  uint16_t ticket;
  amqp_bytes_t destination;
  amqp_bytes_t source;
  amqp_bytes_t routing_key;
  amqp_boolean_t nowait;
  amqp_table_t arguments;
} amqp_exchange_bind_t;

#define AMQP_EXCHANGE_BIND_OK_METHOD ((amqp_method_number_t) 0x0028001F) /* 40, 31; 2621471 */
typedef struct amqp_exchange_bind_ok_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_exchange_bind_ok_t;

#define AMQP_EXCHANGE_UNBIND_METHOD ((amqp_method_number_t) 0x00280028) /* 40, 40; 2621480 */
typedef struct amqp_exchange_unbind_t_ {
  uint16_t ticket;
  amqp_bytes_t destination;
  amqp_bytes_t source;
  amqp_bytes_t routing_key;
  amqp_boolean_t nowait;
  amqp_table_t arguments;
} amqp_exchange_unbind_t;

#define AMQP_EXCHANGE_UNBIND_OK_METHOD ((amqp_method_number_t) 0x00280033) /* 40, 51; 2621491 */
typedef struct amqp_exchange_unbind_ok_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_exchange_unbind_ok_t;

#define AMQP_QUEUE_DECLARE_METHOD ((amqp_method_number_t) 0x0032000A) /* 50, 10; 3276810 */
typedef struct amqp_queue_declare_t_ {
  uint16_t ticket;
  amqp_bytes_t queue;
  amqp_boolean_t passive;
  amqp_boolean_t durable;
  amqp_boolean_t exclusive;
  amqp_boolean_t auto_delete;
  amqp_boolean_t nowait;
  amqp_table_t arguments;
} amqp_queue_declare_t;

#define AMQP_QUEUE_DECLARE_OK_METHOD ((amqp_method_number_t) 0x0032000B) /* 50, 11; 3276811 */
typedef struct amqp_queue_declare_ok_t_ {
  amqp_bytes_t queue;
  uint32_t message_count;
  uint32_t consumer_count;
} amqp_queue_declare_ok_t;

#define AMQP_QUEUE_BIND_METHOD ((amqp_method_number_t) 0x00320014) /* 50, 20; 3276820 */
typedef struct amqp_queue_bind_t_ {
  uint16_t ticket;
  amqp_bytes_t queue;
  amqp_bytes_t exchange;
  amqp_bytes_t routing_key;
  amqp_boolean_t nowait;
  amqp_table_t arguments;
} amqp_queue_bind_t;

#define AMQP_QUEUE_BIND_OK_METHOD ((amqp_method_number_t) 0x00320015) /* 50, 21; 3276821 */
typedef struct amqp_queue_bind_ok_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_queue_bind_ok_t;

#define AMQP_QUEUE_PURGE_METHOD ((amqp_method_number_t) 0x0032001E) /* 50, 30; 3276830 */
typedef struct amqp_queue_purge_t_ {
  uint16_t ticket;
  amqp_bytes_t queue;
  amqp_boolean_t nowait;
} amqp_queue_purge_t;

#define AMQP_QUEUE_PURGE_OK_METHOD ((amqp_method_number_t) 0x0032001F) /* 50, 31; 3276831 */
typedef struct amqp_queue_purge_ok_t_ {
  uint32_t message_count;
} amqp_queue_purge_ok_t;

#define AMQP_QUEUE_DELETE_METHOD ((amqp_method_number_t) 0x00320028) /* 50, 40; 3276840 */
typedef struct amqp_queue_delete_t_ {
  uint16_t ticket;
  amqp_bytes_t queue;
  amqp_boolean_t if_unused;
  amqp_boolean_t if_empty;
  amqp_boolean_t nowait;
} amqp_queue_delete_t;

#define AMQP_QUEUE_DELETE_OK_METHOD ((amqp_method_number_t) 0x00320029) /* 50, 41; 3276841 */
typedef struct amqp_queue_delete_ok_t_ {
  uint32_t message_count;
} amqp_queue_delete_ok_t;

#define AMQP_QUEUE_UNBIND_METHOD ((amqp_method_number_t) 0x00320032) /* 50, 50; 3276850 */
typedef struct amqp_queue_unbind_t_ {
  uint16_t ticket;
  amqp_bytes_t queue;
  amqp_bytes_t exchange;
  amqp_bytes_t routing_key;
  amqp_table_t arguments;
} amqp_queue_unbind_t;

#define AMQP_QUEUE_UNBIND_OK_METHOD ((amqp_method_number_t) 0x00320033) /* 50, 51; 3276851 */
typedef struct amqp_queue_unbind_ok_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_queue_unbind_ok_t;

#define AMQP_BASIC_QOS_METHOD ((amqp_method_number_t) 0x003C000A) /* 60, 10; 3932170 */
typedef struct amqp_basic_qos_t_ {
  uint32_t prefetch_size;
  uint16_t prefetch_count;
  amqp_boolean_t global;
} amqp_basic_qos_t;

#define AMQP_BASIC_QOS_OK_METHOD ((amqp_method_number_t) 0x003C000B) /* 60, 11; 3932171 */
typedef struct amqp_basic_qos_ok_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_basic_qos_ok_t;

#define AMQP_BASIC_CONSUME_METHOD ((amqp_method_number_t) 0x003C0014) /* 60, 20; 3932180 */
typedef struct amqp_basic_consume_t_ {
  uint16_t ticket;
  amqp_bytes_t queue;
  amqp_bytes_t consumer_tag;
  amqp_boolean_t no_local;
  amqp_boolean_t no_ack;
  amqp_boolean_t exclusive;
  amqp_boolean_t nowait;
  amqp_table_t arguments;
} amqp_basic_consume_t;

#define AMQP_BASIC_CONSUME_OK_METHOD ((amqp_method_number_t) 0x003C0015) /* 60, 21; 3932181 */
typedef struct amqp_basic_consume_ok_t_ {
  amqp_bytes_t consumer_tag;
} amqp_basic_consume_ok_t;

#define AMQP_BASIC_CANCEL_METHOD ((amqp_method_number_t) 0x003C001E) /* 60, 30; 3932190 */
typedef struct amqp_basic_cancel_t_ {
  amqp_bytes_t consumer_tag;
  amqp_boolean_t nowait;
} amqp_basic_cancel_t;

#define AMQP_BASIC_CANCEL_OK_METHOD ((amqp_method_number_t) 0x003C001F) /* 60, 31; 3932191 */
typedef struct amqp_basic_cancel_ok_t_ {
  amqp_bytes_t consumer_tag;
} amqp_basic_cancel_ok_t;

#define AMQP_BASIC_PUBLISH_METHOD ((amqp_method_number_t) 0x003C0028) /* 60, 40; 3932200 */
typedef struct amqp_basic_publish_t_ {
  uint16_t ticket;
  amqp_bytes_t exchange;
  amqp_bytes_t routing_key;
  amqp_boolean_t mandatory;
  amqp_boolean_t immediate;
} amqp_basic_publish_t;

#define AMQP_BASIC_RETURN_METHOD ((amqp_method_number_t) 0x003C0032) /* 60, 50; 3932210 */
typedef struct amqp_basic_return_t_ {
  uint16_t reply_code;
  amqp_bytes_t reply_text;
  amqp_bytes_t exchange;
  amqp_bytes_t routing_key;
} amqp_basic_return_t;

#define AMQP_BASIC_DELIVER_METHOD ((amqp_method_number_t) 0x003C003C) /* 60, 60; 3932220 */
typedef struct amqp_basic_deliver_t_ {
  amqp_bytes_t consumer_tag;
  uint64_t delivery_tag;
  amqp_boolean_t redelivered;
  amqp_bytes_t exchange;
  amqp_bytes_t routing_key;
} amqp_basic_deliver_t;

#define AMQP_BASIC_GET_METHOD ((amqp_method_number_t) 0x003C0046) /* 60, 70; 3932230 */
typedef struct amqp_basic_get_t_ {
  uint16_t ticket;
  amqp_bytes_t queue;
  amqp_boolean_t no_ack;
} amqp_basic_get_t;

#define AMQP_BASIC_GET_OK_METHOD ((amqp_method_number_t) 0x003C0047) /* 60, 71; 3932231 */
typedef struct amqp_basic_get_ok_t_ {
  uint64_t delivery_tag;
  amqp_boolean_t redelivered;
  amqp_bytes_t exchange;
  amqp_bytes_t routing_key;
  uint32_t message_count;
} amqp_basic_get_ok_t;

#define AMQP_BASIC_GET_EMPTY_METHOD ((amqp_method_number_t) 0x003C0048) /* 60, 72; 3932232 */
typedef struct amqp_basic_get_empty_t_ {
  amqp_bytes_t cluster_id;
} amqp_basic_get_empty_t;

#define AMQP_BASIC_ACK_METHOD ((amqp_method_number_t) 0x003C0050) /* 60, 80; 3932240 */
typedef struct amqp_basic_ack_t_ {
  uint64_t delivery_tag;
  amqp_boolean_t multiple;
} amqp_basic_ack_t;

#define AMQP_BASIC_REJECT_METHOD ((amqp_method_number_t) 0x003C005A) /* 60, 90; 3932250 */
typedef struct amqp_basic_reject_t_ {
  uint64_t delivery_tag;
  amqp_boolean_t requeue;
} amqp_basic_reject_t;

#define AMQP_BASIC_RECOVER_ASYNC_METHOD ((amqp_method_number_t) 0x003C0064) /* 60, 100; 3932260 */
typedef struct amqp_basic_recover_async_t_ {
  amqp_boolean_t requeue;
} amqp_basic_recover_async_t;

#define AMQP_BASIC_RECOVER_METHOD ((amqp_method_number_t) 0x003C006E) /* 60, 110; 3932270 */
typedef struct amqp_basic_recover_t_ {
  amqp_boolean_t requeue;
} amqp_basic_recover_t;

#define AMQP_BASIC_RECOVER_OK_METHOD ((amqp_method_number_t) 0x003C006F) /* 60, 111; 3932271 */
typedef struct amqp_basic_recover_ok_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_basic_recover_ok_t;

#define AMQP_BASIC_NACK_METHOD ((amqp_method_number_t) 0x003C0078) /* 60, 120; 3932280 */
typedef struct amqp_basic_nack_t_ {
  uint64_t delivery_tag;
  amqp_boolean_t multiple;
  amqp_boolean_t requeue;
} amqp_basic_nack_t;

#define AMQP_TX_SELECT_METHOD ((amqp_method_number_t) 0x005A000A) /* 90, 10; 5898250 */
typedef struct amqp_tx_select_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_tx_select_t;

#define AMQP_TX_SELECT_OK_METHOD ((amqp_method_number_t) 0x005A000B) /* 90, 11; 5898251 */
typedef struct amqp_tx_select_ok_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_tx_select_ok_t;

#define AMQP_TX_COMMIT_METHOD ((amqp_method_number_t) 0x005A0014) /* 90, 20; 5898260 */
typedef struct amqp_tx_commit_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_tx_commit_t;

#define AMQP_TX_COMMIT_OK_METHOD ((amqp_method_number_t) 0x005A0015) /* 90, 21; 5898261 */
typedef struct amqp_tx_commit_ok_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_tx_commit_ok_t;

#define AMQP_TX_ROLLBACK_METHOD ((amqp_method_number_t) 0x005A001E) /* 90, 30; 5898270 */
typedef struct amqp_tx_rollback_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_tx_rollback_t;

#define AMQP_TX_ROLLBACK_OK_METHOD ((amqp_method_number_t) 0x005A001F) /* 90, 31; 5898271 */
typedef struct amqp_tx_rollback_ok_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_tx_rollback_ok_t;

#define AMQP_CONFIRM_SELECT_METHOD ((amqp_method_number_t) 0x0055000A) /* 85, 10; 5570570 */
typedef struct amqp_confirm_select_t_ {
  amqp_boolean_t nowait;
} amqp_confirm_select_t;

#define AMQP_CONFIRM_SELECT_OK_METHOD ((amqp_method_number_t) 0x0055000B) /* 85, 11; 5570571 */
typedef struct amqp_confirm_select_ok_t_ {
  char dummy; /* Dummy field to avoid empty struct */
} amqp_confirm_select_ok_t;

/* Class property records. */
#define AMQP_CONNECTION_CLASS (0x000A) /* 10 */
typedef struct amqp_connection_properties_t_ {
  amqp_flags_t _flags;
  char dummy; /* Dummy field to avoid empty struct */
} amqp_connection_properties_t;

#define AMQP_CHANNEL_CLASS (0x0014) /* 20 */
typedef struct amqp_channel_properties_t_ {
  amqp_flags_t _flags;
  char dummy; /* Dummy field to avoid empty struct */
} amqp_channel_properties_t;

#define AMQP_ACCESS_CLASS (0x001E) /* 30 */
typedef struct amqp_access_properties_t_ {
  amqp_flags_t _flags;
  char dummy; /* Dummy field to avoid empty struct */
} amqp_access_properties_t;

#define AMQP_EXCHANGE_CLASS (0x0028) /* 40 */
typedef struct amqp_exchange_properties_t_ {
  amqp_flags_t _flags;
  char dummy; /* Dummy field to avoid empty struct */
} amqp_exchange_properties_t;

#define AMQP_QUEUE_CLASS (0x0032) /* 50 */
typedef struct amqp_queue_properties_t_ {
  amqp_flags_t _flags;
  char dummy; /* Dummy field to avoid empty struct */
} amqp_queue_properties_t;

#define AMQP_BASIC_CLASS (0x003C) /* 60 */
#define AMQP_BASIC_CONTENT_TYPE_FLAG (1 << 15)
#define AMQP_BASIC_CONTENT_ENCODING_FLAG (1 << 14)
#define AMQP_BASIC_HEADERS_FLAG (1 << 13)
#define AMQP_BASIC_DELIVERY_MODE_FLAG (1 << 12)
#define AMQP_BASIC_PRIORITY_FLAG (1 << 11)
#define AMQP_BASIC_CORRELATION_ID_FLAG (1 << 10)
#define AMQP_BASIC_REPLY_TO_FLAG (1 << 9)
#define AMQP_BASIC_EXPIRATION_FLAG (1 << 8)
#define AMQP_BASIC_MESSAGE_ID_FLAG (1 << 7)
#define AMQP_BASIC_TIMESTAMP_FLAG (1 << 6)
#define AMQP_BASIC_TYPE_FLAG (1 << 5)
#define AMQP_BASIC_USER_ID_FLAG (1 << 4)
#define AMQP_BASIC_APP_ID_FLAG (1 << 3)
#define AMQP_BASIC_CLUSTER_ID_FLAG (1 << 2)
typedef struct amqp_basic_properties_t_ {
  amqp_flags_t _flags;
  amqp_bytes_t content_type;
  amqp_bytes_t content_encoding;
  amqp_table_t headers;
  uint8_t delivery_mode;
  uint8_t priority;
  amqp_bytes_t correlation_id;
  amqp_bytes_t reply_to;
  amqp_bytes_t expiration;
  amqp_bytes_t message_id;
  uint64_t timestamp;
  amqp_bytes_t type;
  amqp_bytes_t user_id;
  amqp_bytes_t app_id;
  amqp_bytes_t cluster_id;
} amqp_basic_properties_t;

#define AMQP_TX_CLASS (0x005A) /* 90 */
typedef struct amqp_tx_properties_t_ {
  amqp_flags_t _flags;
  char dummy; /* Dummy field to avoid empty struct */
} amqp_tx_properties_t;

#define AMQP_CONFIRM_CLASS (0x0055) /* 85 */
typedef struct amqp_confirm_properties_t_ {
  amqp_flags_t _flags;
  char dummy; /* Dummy field to avoid empty struct */
} amqp_confirm_properties_t;

/* API functions for methods */

AMQP_PUBLIC_FUNCTION amqp_channel_open_ok_t * AMQP_CALL amqp_channel_open(amqp_connection_state_t state, amqp_channel_t channel);
AMQP_PUBLIC_FUNCTION amqp_channel_flow_ok_t * AMQP_CALL amqp_channel_flow(amqp_connection_state_t state, amqp_channel_t channel, amqp_boolean_t active);
AMQP_PUBLIC_FUNCTION amqp_exchange_declare_ok_t * AMQP_CALL amqp_exchange_declare(amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t exchange, amqp_bytes_t type, amqp_boolean_t passive, amqp_boolean_t durable, amqp_table_t arguments);
AMQP_PUBLIC_FUNCTION amqp_exchange_delete_ok_t * AMQP_CALL amqp_exchange_delete(amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t exchange, amqp_boolean_t if_unused);
AMQP_PUBLIC_FUNCTION amqp_exchange_bind_ok_t * AMQP_CALL amqp_exchange_bind(amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t destination, amqp_bytes_t source, amqp_bytes_t routing_key, amqp_table_t arguments);
AMQP_PUBLIC_FUNCTION amqp_exchange_unbind_ok_t * AMQP_CALL amqp_exchange_unbind(amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t destination, amqp_bytes_t source, amqp_bytes_t routing_key, amqp_table_t arguments);
AMQP_PUBLIC_FUNCTION amqp_queue_declare_ok_t * AMQP_CALL amqp_queue_declare(amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t queue, amqp_boolean_t passive, amqp_boolean_t durable, amqp_boolean_t exclusive, amqp_boolean_t auto_delete, amqp_table_t arguments);
AMQP_PUBLIC_FUNCTION amqp_queue_bind_ok_t * AMQP_CALL amqp_queue_bind(amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t queue, amqp_bytes_t exchange, amqp_bytes_t routing_key, amqp_table_t arguments);
AMQP_PUBLIC_FUNCTION amqp_queue_purge_ok_t * AMQP_CALL amqp_queue_purge(amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t queue);
AMQP_PUBLIC_FUNCTION amqp_queue_delete_ok_t * AMQP_CALL amqp_queue_delete(amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t queue, amqp_boolean_t if_unused, amqp_boolean_t if_empty);
AMQP_PUBLIC_FUNCTION amqp_queue_unbind_ok_t * AMQP_CALL amqp_queue_unbind(amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t queue, amqp_bytes_t exchange, amqp_bytes_t routing_key, amqp_table_t arguments);
AMQP_PUBLIC_FUNCTION amqp_basic_qos_ok_t * AMQP_CALL amqp_basic_qos(amqp_connection_state_t state, amqp_channel_t channel, uint32_t prefetch_size, uint16_t prefetch_count, amqp_boolean_t global);
AMQP_PUBLIC_FUNCTION amqp_basic_consume_ok_t * AMQP_CALL amqp_basic_consume(amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t queue, amqp_bytes_t consumer_tag, amqp_boolean_t no_local, amqp_boolean_t no_ack, amqp_boolean_t exclusive, amqp_table_t arguments);
AMQP_PUBLIC_FUNCTION amqp_basic_cancel_ok_t * AMQP_CALL amqp_basic_cancel(amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t consumer_tag);
AMQP_PUBLIC_FUNCTION amqp_basic_recover_ok_t * AMQP_CALL amqp_basic_recover(amqp_connection_state_t state, amqp_channel_t channel, amqp_boolean_t requeue);
AMQP_PUBLIC_FUNCTION amqp_tx_select_ok_t * AMQP_CALL amqp_tx_select(amqp_connection_state_t state, amqp_channel_t channel);
AMQP_PUBLIC_FUNCTION amqp_tx_commit_ok_t * AMQP_CALL amqp_tx_commit(amqp_connection_state_t state, amqp_channel_t channel);
AMQP_PUBLIC_FUNCTION amqp_tx_rollback_ok_t * AMQP_CALL amqp_tx_rollback(amqp_connection_state_t state, amqp_channel_t channel);
AMQP_PUBLIC_FUNCTION amqp_confirm_select_ok_t * AMQP_CALL amqp_confirm_select(amqp_connection_state_t state, amqp_channel_t channel);

AMQP_END_DECLS

#endif /* AMQP_FRAMING_H */

/*
 * MIT License
 *
 * Portions created by ONEm Communications Ltd. are Copyright (c) 2016
 * ONEm Communications Ltd. All Rights Reserved.
 *
 * Portions created by ng-voice are Copyright (c) 2016
 * ng-voice. All Rights Reserved.
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
 */

#include "rabbitmq.h"
#include "../../core/sr_module.h"
#include "../../core/route_struct.h"
#include "../../core/str.h"
#include "../../core/mod_fix.h"
#include "../../core/pvar.h"
#include "../../core/lvalue.h"
#include "../../core/kemi.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <uuid/uuid.h>

#include <stdint.h>
#include <amqp_tcp_socket.h>
#include <amqp.h>
#include <amqp_framing.h>

#include <assert.h>

#include "utils.h"

MODULE_VERSION

static int rabbitmq_publish(struct sip_msg *, char *, char *, char *, char *);
static int rabbitmq_publish_consume(
		struct sip_msg *, char *, char *, char *, char *, char *);
static int mod_init(void);
static int mod_child_init(int);

static amqp_socket_t *amqp_sock = NULL;
static amqp_connection_state_t amqp_conn = NULL;

/* module parameters */
static struct amqp_connection_info amqp_info;
static char *amqp_url = RABBITMQ_DEFAULT_AMQP_URL;
static int max_reconnect_attempts = 1;
static int timeout_sec = 1;
static int timeout_usec = 0;
static int direct_reply_to = 0;

/* module helper functions */
static int rabbitmq_connect(amqp_connection_state_t *conn);
static int rabbitmq_disconnect(amqp_connection_state_t *conn);
static int rabbitmq_reconnect(amqp_connection_state_t *conn);

/* module fixup functions */
static int rbmq_fixup_params(void **param, int param_no)
{
	if(param_no == 5) {
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writeble\n");
			return -1;
		}
		return 0;
	} else {
		return fixup_spve_all(param, param_no);
	}

	return -1;
}

static int rbmq_fixup_free_params(void **param, int param_no)
{
	if(param_no == 5) {
		return fixup_free_pvar_null(param, 1);
	} else {
		return fixup_free_spve_all(param, param_no);
	}

	return -1;
}

/* module commands */
static cmd_export_t cmds[] = {
	{"rabbitmq_publish", (cmd_function)rabbitmq_publish, 4, fixup_spve_all,
			fixup_free_spve_all, REQUEST_ROUTE},
	{"rabbitmq_publish_consume", (cmd_function)rabbitmq_publish_consume, 5,
			rbmq_fixup_params, rbmq_fixup_free_params, REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

/* module parameters */
static param_export_t params[] = {
	{"url", PARAM_STRING, &amqp_url},
	{"timeout_sec", PARAM_INT, &timeout_sec},
	{"timeout_usec", PARAM_INT, &timeout_usec},
	{"direct_reply_to", PARAM_INT, &direct_reply_to},
	{0, 0, 0}
};

/* module exports */
struct module_exports exports = {
	"rabbitmq",				/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,					/* RPC method exports */
	0,					/* exported pseudo-variables */
	0,					/* response handling function */
	mod_init,			/* module initialization function */
	mod_child_init,		/* per-child init function */
	0					/* module destroy function */
};

/* module init */
static int mod_init(void)
{
	if(amqp_parse_url(amqp_url, &amqp_info) == AMQP_STATUS_BAD_URL) {
		LM_ERR("FAIL parsing url: '%s'\n", amqp_url);
		return -1;
	} else {
		LM_INFO("SUCCESS parsing url: '%s'\n", amqp_url);
	}

	return 0;
}


/* module child init */
static int mod_child_init(int rank)
{
	// main and tcp manager process
	if(rank == PROC_MAIN || rank == PROC_TCP_MAIN) {
		return 0;
	}

	// routing process
	if(rabbitmq_connect(&amqp_conn) != RABBITMQ_OK) {
		LM_ERR("FAIL rabbitmq_connect()");
		return -1;
	}
	LM_DBG("SUCCESS initialization of rabbitmq module in child [%d]\n", rank);

	return 0;
}

/* module helper functions */
static int ki_rabbitmq_publish(sip_msg_t *msg, str *exchange, str *routingkey,
	str *contenttype, str *messagebody)
{
	int reconnect_attempts = 0;
	int log_ret;

reconnect:
	// open channel
	amqp_channel_open(amqp_conn, 1);
	log_ret =
			log_on_amqp_error(amqp_get_rpc_reply(amqp_conn), "amqp_channel_open()");

	// open channel - failed
	if(log_ret != AMQP_RESPONSE_NORMAL) {
		// reconnect - debug
		LM_ERR("FAIL: rabbitmq_reconnect(), attempts=%d\n", reconnect_attempts);

		// reconnect
		if(reconnect_attempts < max_reconnect_attempts) {
			// reconnect - debug
			LM_ERR("RETRY: rabbitmq_reconnect()\n");

			// reconnect - success
			if(rabbitmq_reconnect(&amqp_conn) == RABBITMQ_OK) {
				// reconnect - debug
				LM_ERR("SUCCESS: rabbitmq_reconnect()\n");
			}
			reconnect_attempts++;

			// reconnect - goto
			goto reconnect;
		}

		// reconnect - close channel
		amqp_channel_close(amqp_conn, 1, AMQP_REPLY_SUCCESS);

		// reconnect - return error
		return RABBITMQ_ERR_CHANNEL;
	}

	// alloc properties
	amqp_basic_properties_t props;
	props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG
				   | AMQP_BASIC_CORRELATION_ID_FLAG;
	props.content_type = amqp_cstring_bytes(contenttype->s);
	props.delivery_mode = 2; /* persistent delivery mode */
	props.correlation_id = amqp_cstring_bytes("1");

	// publish
	if(log_on_error(amqp_basic_publish(amqp_conn, 1, amqp_cstring_bytes(exchange->s),
							amqp_cstring_bytes(routingkey->s), 0, 0, &props,
							amqp_cstring_bytes(messagebody->s)),
			   "amqp_basic_publish()")
			!= AMQP_RESPONSE_NORMAL) {
		// debug
		LM_ERR("FAIL: amqp_basic_publish()\n");

		// cleanup
		amqp_maybe_release_buffers(amqp_conn);
		amqp_channel_close(amqp_conn, 1, AMQP_REPLY_SUCCESS);

		// error
		return RABBITMQ_ERR_PUBLISH;
	}

	// debug
	LM_DBG("SUCCESS: amqp_basic_publish()\n");

	// cleanup
	amqp_maybe_release_buffers(amqp_conn);
	amqp_channel_close(amqp_conn, 1, AMQP_REPLY_SUCCESS);

	// success
	return RABBITMQ_OK;
}

/* module helper functions */
static int rabbitmq_publish(struct sip_msg *msg, char *in_exchange,
		char *in_routingkey, char *in_contenttype, char *in_messagebody)
{
	str exchange, routingkey, messagebody, contenttype;

	// sanity checks
	if(get_str_fparam(&exchange, msg, (fparam_t *)in_exchange) < 0) {
		LM_ERR("failed to get exchange\n");
		return -1;
	}

	if(get_str_fparam(&routingkey, msg, (fparam_t *)in_routingkey) < 0) {
		LM_ERR("failed to get kouting key\n");
		return -1;
	}

	if(get_str_fparam(&messagebody, msg, (fparam_t *)in_messagebody) < 0) {
		LM_ERR("failed to get message body\n");
		return -1;
	}

	if(get_str_fparam(&contenttype, msg, (fparam_t *)in_contenttype) < 0) {
		LM_ERR("failed to get content type\n");
		return -1;
	}

	return ki_rabbitmq_publish(msg, &exchange, &routingkey, &contenttype,
			&messagebody);
}

static int rabbitmq_publish_consume_helper(sip_msg_t *msg, str *exchange,
		str *routingkey, str *contenttype, str *messagebody,
		pv_spec_t *dst)
{
	pv_value_t val;
	amqp_frame_t frame;
	amqp_basic_deliver_t *d;
	amqp_basic_properties_t *p;
	int result = RABBITMQ_OK;
	int reconnect_attempts = 0;
	int log_ret;
	size_t body_target;
	size_t body_received;

	struct timeval tv;
	tv.tv_sec = timeout_sec;
	tv.tv_usec = timeout_usec;

	amqp_queue_declare_ok_t *reply_to;

	uuid_t uuid;
	char uuid_buffer[40];
	char reply_to_buffer[64];

reconnect:
	// open channel
	amqp_channel_open(amqp_conn, 1);
	log_ret =
			log_on_amqp_error(amqp_get_rpc_reply(amqp_conn), "amqp_channel_open()");

	// open channel - failed
	if(log_ret != AMQP_RESPONSE_NORMAL) {
		// reconnect - debug
		LM_ERR("FAIL: rabbitmq_reconnect(), attempts=%d\n", reconnect_attempts);

		// reconnect
		if(reconnect_attempts < max_reconnect_attempts) {
			// reconnect - debug
			LM_ERR("RETRY: rabbitmq_reconnect()\n");

			// reconnect - success
			if(rabbitmq_reconnect(&amqp_conn) == RABBITMQ_OK) {
				// reconnect - debug
				LM_ERR("SUCCESS: rabbitmq_reconnect()\n");
			}
			reconnect_attempts++;

			// reconnect - goto
			goto reconnect;
		}

		// reconnect - close channel
		amqp_channel_close(amqp_conn, 1, AMQP_REPLY_SUCCESS);

		// reconnect - return error
		return RABBITMQ_ERR_CHANNEL;
	}

	// alloc reply_to queue
	if(direct_reply_to == 1) {
		reply_to = amqp_queue_declare(amqp_conn, 1,
				amqp_cstring_bytes("amq.rabbitmq.reply-to"), 0, 0, 1, 1,
				amqp_empty_table);
	} else {
		uuid_generate_random(uuid);
		uuid_unparse(uuid, uuid_buffer);

		strcpy(reply_to_buffer, "kamailio-");
		strcat(reply_to_buffer, uuid_buffer);

		reply_to =
				amqp_queue_declare(amqp_conn, 1, amqp_cstring_bytes(reply_to_buffer),
						0, 0, 1, 1, amqp_empty_table);
	}

	if(log_on_amqp_error(amqp_get_rpc_reply(amqp_conn), "amqp_queue_declare()")
			!= AMQP_RESPONSE_NORMAL) {
		LM_ERR("FAIL: amqp_queue_declare()\n");
		amqp_channel_close(amqp_conn, 1, AMQP_REPLY_SUCCESS);
		return RABBITMQ_ERR_QUEUE;
	}

	LM_INFO("reply_to = %.*s\n", (int)reply_to->queue.len,
			(char *)reply_to->queue.bytes);

	// alloc request properties
	amqp_basic_properties_t props;
	props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG
				   | AMQP_BASIC_REPLY_TO_FLAG | AMQP_BASIC_CORRELATION_ID_FLAG;
	props.content_type = amqp_cstring_bytes(contenttype->s);
	props.delivery_mode = 2; /* persistent delivery mode */
	props.reply_to = reply_to->queue;
	if(props.reply_to.bytes == NULL) {
		LM_ERR("Out of memory while copying queue name");
		amqp_channel_close(amqp_conn, 1, AMQP_REPLY_SUCCESS);
		return -1;
	}
	props.correlation_id = amqp_cstring_bytes("1");

	// start consume
	amqp_basic_consume(amqp_conn, 1, reply_to->queue, amqp_empty_bytes, 0, 1, 0,
			amqp_empty_table);
	if(log_on_amqp_error(amqp_get_rpc_reply(amqp_conn), "amqp_basic_consume()")
			!= AMQP_RESPONSE_NORMAL) {
		LM_ERR("FAIL: amqp_basic_consume()\n");
		amqp_channel_close(amqp_conn, 1, AMQP_REPLY_SUCCESS);
		return RABBITMQ_ERR_CONSUME;
	}

	// publish
	if(log_on_error(amqp_basic_publish(amqp_conn, 1, amqp_cstring_bytes(exchange->s),
							amqp_cstring_bytes(routingkey->s), 0, 0, &props,
							amqp_cstring_bytes(messagebody->s)),
			   "amqp_basic_publish()")
			!= AMQP_RESPONSE_NORMAL) {
		LM_ERR("FAIL: amqp_basic_publish()\n");
		amqp_channel_close(amqp_conn, 1, AMQP_REPLY_SUCCESS);
		return RABBITMQ_ERR_PUBLISH;
	}

	// consume frame
	for(;;) {
		amqp_maybe_release_buffers(amqp_conn);
		result = amqp_simple_wait_frame_noblock(amqp_conn, &frame, &tv);
		if(result < 0) {
			LM_ERR("amqp_simple_wait_frame_noblock() error: %d\n", result);
			result = -1;
			break;
		} else {
			result = RABBITMQ_OK;
		}

		LM_DBG("Frame type: %u channel: %u\n", frame.frame_type, frame.channel);
		if(frame.frame_type != AMQP_FRAME_METHOD) {
			continue;
		}

		LM_DBG("Method: %s\n", amqp_method_name(frame.payload.method.id));
		if(frame.payload.method.id != AMQP_BASIC_DELIVER_METHOD) {
			continue;
		}

		d = (amqp_basic_deliver_t *)frame.payload.method.decoded;
		LM_DBG("Delivery: %u exchange: %.*s routingkey: %.*s\n",
				(unsigned)d->delivery_tag, (int)d->exchange.len,
				(char *)d->exchange.bytes, (int)d->routing_key.len,
				(char *)d->routing_key.bytes);

		result = amqp_simple_wait_frame_noblock(amqp_conn, &frame, &tv);
		if(result < 0) {
			LM_ERR("amqp_simple_wait_frame_noblock() error: %d\n", result);
			result = -1;
			break;
		} else {
			result = RABBITMQ_OK;
		}

		if(frame.frame_type != AMQP_FRAME_HEADER) {
			LM_ERR("Expected header!");
			result = -1;
			break;
		}

		p = (amqp_basic_properties_t *)frame.payload.properties.decoded;
		if(p->_flags & AMQP_BASIC_CONTENT_TYPE_FLAG) {
			LM_DBG("Content-type: %.*s\n", (int)p->content_type.len,
					(char *)p->content_type.bytes);
		}

		body_target = (size_t)frame.payload.properties.body_size;
		body_received = 0;

		while(body_received < body_target) {
			result = amqp_simple_wait_frame_noblock(amqp_conn, &frame, &tv);
			if(result < 0) {
				LM_ERR("amqp_simple_wait_frame_noblock() error: %d\n", result);
				result = -1;
				break;
			} else {
				result = RABBITMQ_OK;
			}

			if(frame.frame_type != AMQP_FRAME_BODY) {
				LM_ERR("Expected body!");
				result = -1;
				break;
			}

			body_received += frame.payload.body_fragment.len;
			assert(body_received <= body_target);

			val.rs.s = (char *)frame.payload.body_fragment.bytes;
			val.rs.len = (int)frame.payload.body_fragment.len;

			LM_DBG("RPC Call result: %.*s\n", val.rs.len, val.rs.s);
			val.flags = PV_VAL_STR;
			dst->setf(msg, &dst->pvp, (int)EQ_T, &val);
		}

		// amqp_simple_wait_frame <= 0
		if(body_received != body_target) {
			LM_ERR("body_received != body_target'\n");
			result = -1;
			break;
		}

		// received reply
		break;
	}

	amqp_channel_close(amqp_conn, 1, AMQP_REPLY_SUCCESS);

	return result;
}

static int ki_rabbitmq_publish_consume(sip_msg_t *msg, str *exchange,
		str *routingkey, str *contenttype, str *messagebody, str *dpv)
{
	pv_spec_t *dst;

	dst = pv_cache_get(dpv);

	if(dst==NULL) {
		LM_ERR("failed getting pv: %.*s\n", dpv->len, dpv->s);
		return -1;
	}
	if(dst->setf == NULL) {
		LM_ERR("result pvar is not writeble: %.*s\n", dpv->len, dpv->s);
		return -1;
	}

	return rabbitmq_publish_consume_helper(msg, exchange, routingkey,
			contenttype, messagebody, dst);
}

static int rabbitmq_publish_consume(struct sip_msg *msg, char *in_exchange,
		char *in_routingkey, char *in_contenttype, char *in_messagebody,
		char *reply)
{
	str exchange, routingkey, messagebody, contenttype;
	pv_spec_t *dst;

	// sanity checks
	if(get_str_fparam(&exchange, msg, (fparam_t *)in_exchange) < 0) {
		LM_ERR("failed to get exchange\n");
		return -1;
	}

	if(get_str_fparam(&routingkey, msg, (fparam_t *)in_routingkey) < 0) {
		LM_ERR("failed to get kouting key\n");
		return -1;
	}

	if(get_str_fparam(&messagebody, msg, (fparam_t *)in_messagebody) < 0) {
		LM_ERR("failed to get message body\n");
		return -1;
	}

	if(get_str_fparam(&contenttype, msg, (fparam_t *)in_contenttype) < 0) {
		LM_ERR("failed to get content type\n");
		return -1;
	}

	dst = (pv_spec_t *)reply;

	return rabbitmq_publish_consume_helper(msg, &exchange, &routingkey,
			&contenttype, &messagebody, dst);
}

static int rabbitmq_connect(amqp_connection_state_t *conn)
{
	int ret;
	int log_ret;
	//	amqp_rpc_reply_t reply;

	// establish a new connection to RabbitMQ server
	*conn = amqp_new_connection();
	log_ret = log_on_amqp_error(
			amqp_get_rpc_reply(*conn), "amqp_new_connection()");
	if(log_ret != AMQP_RESPONSE_NORMAL && log_ret != AMQP_RESPONSE_NONE) {
		return RABBITMQ_ERR_CONNECT;
	}

	amqp_sock = amqp_tcp_socket_new(*conn);
	if(!amqp_sock) {
		LM_ERR("FAIL: create TCP amqp_sock");
		amqp_destroy_connection(*conn);
		return RABBITMQ_ERR_SOCK;
	}

	ret = amqp_socket_open(amqp_sock, amqp_info.host, amqp_info.port);
	if(ret != AMQP_STATUS_OK) {
		LM_ERR("FAIL: open TCP sock, amqp_status=%d", ret);
		// amqp_destroy_connection(*conn);
		return RABBITMQ_ERR_SOCK;
	}

	log_ret = log_on_amqp_error(
			amqp_login(*conn, amqp_info.vhost, 0, 131072, 0,
					AMQP_SASL_METHOD_PLAIN, amqp_info.user, amqp_info.password),
			"amqp_login()");
	if(log_ret != AMQP_RESPONSE_NORMAL && log_ret != AMQP_RESPONSE_NONE) {
		LM_ERR("FAIL: amqp_login()\n");
		// amqp_destroy_connection(*conn);
		return RABBITMQ_ERR_CONSUME;
	}

	return RABBITMQ_OK;
}

static int rabbitmq_disconnect(amqp_connection_state_t *conn)
{
	int log_ret;

	// sanity checks
	if(!*conn) {
		return RABBITMQ_ERR_NULL;
	}

	/*
	log_ret = log_on_amqp_error(amqp_connection_close(*conn, AMQP_REPLY_SUCCESS), "amqp_connection_close()");
	if (log_ret != AMQP_RESPONSE_NORMAL && log_ret != AMQP_RESPONSE_NONE) {
		LM_ERR("FAIL: amqp_connection_close()\n");
		return RABBITMQ_ERR_CONNECT;
	}
*/

	log_ret = log_on_error(
			amqp_destroy_connection(*conn), "amqp_destroy_connection()");
	if(log_ret != AMQP_RESPONSE_NORMAL && log_ret != AMQP_RESPONSE_NONE) {
		LM_ERR("FAIL: amqp_destroy_connection()\n");
		return RABBITMQ_ERR_CONNECT;
	}

	return RABBITMQ_OK;
}
static int rabbitmq_reconnect(amqp_connection_state_t *conn)
{
	int ret;

	// sanity checks
	if(!*conn) {
		return RABBITMQ_ERR_NULL;
	}

	// disconnect from old RabbitMQ server
	if((ret = rabbitmq_disconnect(conn)) != RABBITMQ_OK) {
		LM_NOTICE("FAIL rabbitmq_disconnect() in rabbitmq_reconnect()\n");
		return ret;
	}

	// connect to new RabbitMQ server
	if((ret = rabbitmq_connect(conn)) != RABBITMQ_OK) {
		LM_NOTICE("FAIL rabbitmq_connect() in rabbitmq_reconnect()\n");
		return ret;
	}

	return RABBITMQ_OK;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_rabbitmq_exports[] = {
	{ str_init("rabbitmq"), str_init("publish"),
		SR_KEMIP_INT, ki_rabbitmq_publish,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("rabbitmq"), str_init("publish_consume"),
		SR_KEMIP_INT, ki_rabbitmq_publish_consume,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_rabbitmq_exports);
	return 0;
}

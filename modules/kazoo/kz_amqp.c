#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <amqp.h>
#include <amqp_framing.h>
#include <amqp_tcp_socket.h>
#include <json.h>
#include <uuid/uuid.h>
#include "../../mem/mem.h"
#include "../../timer_proc.h"
#include "../../sr_module.h"
#include "../../pvar.h"
#include "../../mod_fix.h"
#include "../../lvalue.h"


#include "kz_amqp.h"
#include "kz_json.h"

#define RET_AMQP_ERROR 2

kz_amqp_connection_pool_ptr kz_pool = NULL;
kz_amqp_bindings_ptr kz_bindings = NULL;
int bindings_count = 0;

static unsigned long rpl_query_routing_key_count = 0;

typedef struct json_object *json_obj_ptr;

kz_amqp_channel_ptr channels = NULL;
int channel_index = 0;
extern int *kz_pipe_fds;

extern struct timeval kz_sock_tv;
extern struct timeval kz_amqp_tv;
extern struct timeval kz_qtimeout_tv;
extern struct timeval kz_ack_tv;
extern struct timeval kz_timer_tv;

extern int dbk_internal_loop_count;
extern int dbk_consumer_loop_count;
extern int dbk_consumer_ack_loop_count;


extern int dbk_single_consumer_on_reconnect;
extern int dbk_consume_messages_on_reconnect;

extern pv_spec_t kz_query_timeout_spec;

const amqp_bytes_t kz_amqp_empty_bytes = { 0, NULL };
const amqp_table_t kz_amqp_empty_table = { 0, NULL };

static char *kz_amqp_str_dup(str *src)
{
	char *res;

	if (!src || !src->s)
		return NULL;
	if (!(res = (char *) shm_malloc(src->len + 1)))
		return NULL;
	strncpy(res, src->s, src->len);
	res[src->len] = 0;
	return res;
}

static char *kz_amqp_string_dup(char *src)
{
	char *res;
	int sz;
	if (!src )
		return NULL;

	sz = strlen(src);
	if (!(res = (char *) shm_malloc(sz + 1)))
		return NULL;
	strncpy(res, src, sz);
	res[sz] = 0;
	return res;
}

char *kz_amqp_bytes_dup(amqp_bytes_t bytes)
{
	char *res;
	int sz;
	if (!bytes.bytes )
		return NULL;

	sz = bytes.len;
	if (!(res = (char *) shm_malloc(sz + 1)))
		return NULL;
	strncpy(res, bytes.bytes, sz);
	res[sz] = 0;
	return res;
}

void kz_amqp_bytes_free(amqp_bytes_t bytes)
{
  shm_free(bytes.bytes);
}

amqp_bytes_t kz_amqp_bytes_malloc_dup(amqp_bytes_t src)
{
  amqp_bytes_t result = {0, 0};
  result.len = src.len;
  result.bytes = shm_malloc(src.len+1);
  if (result.bytes != NULL) {
    memcpy(result.bytes, src.bytes, src.len);
    ((char*)result.bytes)[result.len] = '\0';
  }
  return result;
}

amqp_bytes_t kz_amqp_bytes_dup_from_string(char *src)
{
	return kz_amqp_bytes_malloc_dup(amqp_cstring_bytes(src));
}

amqp_bytes_t kz_amqp_bytes_dup_from_str(str *src)
{
	return kz_amqp_bytes_malloc_dup(amqp_cstring_bytes(src->s));
}


void kz_amqp_free_consumer_delivery(kz_amqp_consumer_delivery_ptr ptr)
{
	if(ptr == NULL)
		return;
	if(ptr->payload)
		shm_free(ptr->payload);
	if(ptr->event_key)
		shm_free(ptr->event_key);
	if(ptr->event_subkey)
		shm_free(ptr->event_subkey);
	shm_free(ptr);
}

void kz_amqp_free_bind(kz_amqp_bind_ptr bind)
{
	if(bind == NULL)
		return;
	if(bind->exchange.bytes)
		kz_amqp_bytes_free(bind->exchange);
	if(bind->exchange_type.bytes)
		kz_amqp_bytes_free(bind->exchange_type);
	if(bind->queue.bytes)
		kz_amqp_bytes_free(bind->queue);
	if(bind->routing_key.bytes)
		kz_amqp_bytes_free(bind->routing_key);
	if(bind->event_key.bytes)
		kz_amqp_bytes_free(bind->event_key);
	if(bind->event_subkey.bytes)
		kz_amqp_bytes_free(bind->event_subkey);
	shm_free(bind);
}

void kz_amqp_free_connection(kz_amqp_connection_ptr conn)
{
	if(!conn)
		return;

	if(conn->url)
		shm_free(conn->url);
	shm_free(conn);
}


void kz_amqp_free_pipe_cmd(kz_amqp_cmd_ptr cmd)
{
	if(cmd == NULL)
		return;
	if (cmd->exchange)
		shm_free(cmd->exchange);
	if (cmd->exchange_type)
		shm_free(cmd->exchange_type);
	if (cmd->queue)
		shm_free(cmd->queue);
	if (cmd->routing_key)
		shm_free(cmd->routing_key);
	if (cmd->reply_routing_key)
		shm_free(cmd->reply_routing_key);
	if (cmd->payload)
		shm_free(cmd->payload);
	if (cmd->return_payload)
		shm_free(cmd->return_payload);
	lock_release(&cmd->lock);
	lock_destroy(&cmd->lock);
	shm_free(cmd);
}

kz_amqp_cmd_ptr kz_amqp_alloc_pipe_cmd()
{
	kz_amqp_cmd_ptr cmd = (kz_amqp_cmd_ptr)shm_malloc(sizeof(kz_amqp_cmd));
	if(cmd == NULL) {
		LM_ERR("failed to allocate kz_amqp_cmd in process %d\n", getpid());
		return NULL;
	}
	memset(cmd, 0, sizeof(kz_amqp_cmd));
	if(lock_init(&cmd->lock)==NULL)  {
		LM_ERR("cannot init the lock for pipe command in process %d\n", getpid());
		lock_dealloc(&cmd->lock);
		kz_amqp_free_pipe_cmd(cmd);
		return NULL;
	}
	lock_get(&cmd->lock);
	return cmd;
}

kz_amqp_bind_ptr kz_amqp_bind_alloc_ex(str* exchange, str* exchange_type, str* queue, str* routing_key, str* event_key, str* event_subkey )
{
    kz_amqp_bind_ptr bind = NULL;

    bind = (kz_amqp_bind_ptr)shm_malloc(sizeof(kz_amqp_bind));
	if(bind == NULL) {
		LM_ERR("error allocation memory for bind alloc\n");
		goto error;
	}
	memset(bind, 0, sizeof(kz_amqp_bind));

	if(exchange != NULL) {
		bind->exchange = kz_amqp_bytes_dup_from_str(exchange);
	    if (bind->exchange.bytes == NULL) {
			LM_ERR("Out of memory allocating for exchange\n");
			goto error;
	    }
	}

	if(exchange_type != NULL) {
		bind->exchange_type = kz_amqp_bytes_dup_from_str(exchange_type);
	    if (bind->exchange_type.bytes == NULL) {
			LM_ERR("Out of memory allocating for exchange type\n");
			goto error;
	    }
	}

	if(queue != NULL) {
		bind->queue = kz_amqp_bytes_dup_from_str(queue);
	    if (bind->queue.bytes == NULL) {
			LM_ERR("Out of memory allocating for queue\n");
			goto error;
	    }
	}

	if(routing_key != NULL) {
		bind->routing_key = kz_amqp_bytes_dup_from_str(routing_key);
	    if (bind->routing_key.bytes == NULL) {
			LM_ERR("Out of memory allocating for routing key\n");
			goto error;
	    }
	}

	if(event_key != NULL) {
		bind->event_key = kz_amqp_bytes_dup_from_str(event_key);
	    if (bind->event_key.bytes == NULL) {
			LM_ERR("Out of memory allocating for routing key\n");
			goto error;
	    }
	}

	if(event_subkey != NULL) {
		bind->event_subkey = kz_amqp_bytes_dup_from_str(event_subkey);
	    if (bind->event_subkey.bytes == NULL) {
			LM_ERR("Out of memory allocating for routing key\n");
			goto error;
	    }
	}

	return bind;

error:
	kz_amqp_free_bind(bind);
    return NULL;
}

kz_amqp_bind_ptr kz_amqp_bind_alloc(str* exchange, str* exchange_type, str* queue, str* routing_key )
{
	return kz_amqp_bind_alloc_ex(exchange, exchange_type, queue, routing_key, NULL, NULL );
}

void kz_amqp_init_connection_pool() {
	if(kz_pool == NULL) {
		kz_pool = (kz_amqp_connection_pool_ptr) shm_malloc(sizeof(kz_amqp_connection_pool));
		memset(kz_pool, 0, sizeof(kz_amqp_connection_pool));
	}
}

int kz_amqp_bind_init_targeted_channel(int idx )
{
    kz_amqp_bind_ptr bind = NULL;
    str rpl_exch = str_init("targeted");
    str rpl_exch_type = str_init("direct");
    int ret = -1;
    char serverid[512];

    bind = (kz_amqp_bind_ptr)shm_malloc(sizeof(kz_amqp_bind));
	if(bind == NULL) {
		LM_ERR("error allocation memory for reply\n");
		goto error;
	}
	memset(bind, 0, sizeof(kz_amqp_bind));

	bind->exchange = kz_amqp_bytes_dup_from_str(&rpl_exch);
	bind->exchange_type = kz_amqp_bytes_dup_from_str(&rpl_exch_type);

    sprintf(serverid, "kamailio@%.*s-<%d-%d>", dbk_node_hostname.len, dbk_node_hostname.s, my_pid(), idx);
    bind->queue = kz_amqp_bytes_dup_from_string(serverid);

    sprintf(serverid, "kamailio@%.*s-<%d>-targeted-%d", dbk_node_hostname.len, dbk_node_hostname.s, my_pid(), idx);
    bind->routing_key = kz_amqp_bytes_dup_from_string(serverid);

    if (bind->exchange.bytes == NULL || bind->routing_key.bytes == NULL || bind->queue.bytes == NULL) {
		LM_ERR("Out of memory allocating for exchange/routing_key\n");
		goto error;
    }

    channels[idx].targeted = bind;
    return 0;
 error:
	kz_amqp_free_bind(bind);
    return ret;
}

int kz_amqp_init() {
	int i;
	kz_amqp_init_connection_pool();
	if(kz_bindings == NULL) {
		kz_bindings = (kz_amqp_bindings_ptr) shm_malloc(sizeof(kz_amqp_bindings));
		memset(kz_bindings, 0, sizeof(kz_amqp_bindings));
	}
	if(channels == NULL) {
		channels = shm_malloc(dbk_channels * sizeof(kz_amqp_channel));
		memset(channels, 0, dbk_channels * sizeof(kz_amqp_channel));
		for(i=0; i < dbk_channels; i++) {
			channels[i].channel = i+1;
			if(lock_init(&channels[i].lock)==NULL) {
				LM_ERR("could not initialize locks for channels\n");
				return 0;
			}
			if(kz_amqp_bind_init_targeted_channel(i)) {
				LM_ERR("could not initialize targeted channels\n");
				return 0;
			}
		}
	}
	return 1;
}

void kz_amqp_destroy() {
	int i;
	if(channels != NULL) {
		for(i=0; i < dbk_channels; i++) {
			if(channels[i].targeted != NULL) {
				kz_amqp_free_bind(channels[i].targeted);
			}
		}
		shm_free(channels);
	}


	if(kz_bindings != NULL) {
		kz_amqp_binding_ptr binding = kz_bindings->head;
		while(binding != NULL) {
			kz_amqp_binding_ptr free = binding;
			binding = binding->next;
			if(free->bind != NULL) {
				kz_amqp_free_bind(free->bind);
			}
			shm_free(free);
		}
		shm_free(kz_bindings);
	}

	if(kz_pool != NULL) {
		kz_amqp_connection_ptr conn = kz_pool->head;
		while(conn != NULL) {
			kz_amqp_connection_ptr tofree = conn;
			conn = conn->next;
			kz_amqp_free_connection(tofree);
		}
		shm_free(kz_pool);
	}


}

#define KZ_URL_MAX_SIZE 50
static char* KZ_URL_ROOT = "/";

int kz_amqp_add_connection(modparam_t type, void* val)
{
	kz_amqp_init_connection_pool();

	char* url = (char*) val;
	int len = strlen(url);
	if(len > KZ_URL_MAX_SIZE) {
		LM_ERR("connection url exceeds max size %d\n", KZ_URL_MAX_SIZE);
		return -1;
	}

	kz_amqp_connection_ptr newConn = shm_malloc(sizeof(kz_amqp_connection));
	memset(newConn, 0, sizeof(kz_amqp_connection));

	newConn->url = shm_malloc( (KZ_URL_MAX_SIZE + 1) * sizeof(char) );
	memset(newConn->url, 0, (KZ_URL_MAX_SIZE + 1) * sizeof(char));
	// maintain compatibility
	if (!strncmp((char*)val, "kazoo://", 8)) {
		sprintf(newConn->url, "amqp://%s", (char*)(url+(8*sizeof(char))) );
	} else {
		strcpy(newConn->url, url);
		newConn->url[len] = '\0';
	}

    if(amqp_parse_url(newConn->url, &newConn->info) == AMQP_STATUS_BAD_URL) {
        LM_ERR("ERROR PARSING URL \"%s\"\n", newConn->url);
    	goto error;
    }


    if(newConn->info.vhost == NULL) {
    	newConn->info.vhost = KZ_URL_ROOT;
    } else if(newConn->info.vhost[0] == '/' && strlen(newConn->info.vhost) == 1) { // bug in amqp_parse_url ?
    	newConn->info.vhost++;
    }

	if(kz_pool->head == NULL)
		kz_pool->head = newConn;

	if(kz_pool->tail != NULL)
		kz_pool->tail->next = newConn;

	kz_pool->tail = newConn;

	return 0;

error:
	kz_amqp_free_connection(newConn);
	return -1;

}

void kz_amqp_connection_close(kz_amqp_conn_ptr rmq) {
    LM_DBG("Close rmq connection\n");
    if (!rmq)
    	return;

    if (rmq->conn) {
		LM_DBG("close connection:  %d rmq(%p)->conn(%p)\n", getpid(), (void *)rmq, rmq->conn);
		kz_amqp_error("closing connection", amqp_connection_close(rmq->conn, AMQP_REPLY_SUCCESS));
		if (amqp_destroy_connection(rmq->conn) < 0) {
			LM_ERR("cannot destroy connection\n");
		}
		rmq->conn = NULL;
		rmq->socket = NULL;
		rmq->channel_count = 0;
    }

}

void kz_amqp_channel_close(kz_amqp_conn_ptr rmq, amqp_channel_t channel) {
    LM_DBG("Close rmq channel\n");
    if (!rmq)
    	return;

	LM_DBG("close channel: %d rmq(%p)->channel(%d)\n", getpid(), (void *)rmq, channel);
	kz_amqp_error("closing channel", amqp_channel_close(rmq->conn, channel, AMQP_REPLY_SUCCESS));
}

int kz_amqp_connection_open(kz_amqp_conn_ptr rmq) {
	rmq->channel_count = rmq->channel_counter = 0;
    if (!(rmq->conn = amqp_new_connection())) {
    	LM_DBG("Failed to create new AMQP connection\n");
    	goto error;
    }

    rmq->socket = amqp_tcp_socket_new(rmq->conn);
    if (!rmq->socket) {
    	LM_DBG("Failed to create TCP socket to AMQP broker\n");
    	goto error;
    }

    if (amqp_socket_open(rmq->socket, rmq->info->info.host, rmq->info->info.port)) {
    	LM_DBG("Failed to open TCP socket to AMQP broker\n");
    	goto error;
    }

    if (kz_amqp_error("Logging in", amqp_login(rmq->conn,
					   rmq->info->info.vhost,
					   0,
					   131072,
					   0,
					   AMQP_SASL_METHOD_PLAIN,
					   rmq->info->info.user,
					   rmq->info->info.password))) {

    	LM_ERR("Login to AMQP broker failed!\n");
    	goto error;
    }

    return 0;

 error:
    kz_amqp_connection_close(rmq);
    return -1;
}

int kz_amqp_channel_open(kz_amqp_conn_ptr rmq, amqp_channel_t channel) {
	if(rmq == NULL) {
		LM_DBG("rmq == NULL \n");
		return -1;
	}

    amqp_channel_open(rmq->conn, channel);
    if (kz_amqp_error("Opening channel", amqp_get_rpc_reply(rmq->conn))) {
    	LM_ERR("Failed to open channel AMQP %d!\n", channel);
    	return -1;
    }

    return 0;
}

kz_amqp_conn_ptr kz_amqp_get_connection() {
	return NULL;

	/*
	kz_amqp_conn_ptr ptr = NULL;
	if(kz_pool == NULL) {
		return NULL;
	}
//	lock_get(&kz_pool->lock);

	ptr = kz_pool->head;

	if(kz_pool->current != NULL) {
		ptr = kz_pool->current;
	}

	if(ptr->socket == NULL )
	{
	while(ptr != NULL) {
		if(kz_amqp_connection_open(ptr) == 0) {
			kz_pool->current = ptr;
			break;
		}
		ptr = ptr->next;
	}
	}

//	lock_release(&kz_pool->lock);

   	return ptr;
   	*/
}

kz_amqp_conn_ptr kz_amqp_get_next_connection() {
	return NULL;
	/*
	kz_amqp_conn_ptr ptr = NULL;
	if(kz_pool == NULL) {
		return NULL;
	}

	if(kz_pool->current != NULL) {
		ptr = kz_pool->current->next;
	}

	if(ptr == NULL) {
		ptr = kz_pool->head;
	}

	while(ptr != NULL) {
		if(kz_amqp_connection_open(ptr) == 0) {
			kz_pool->current = ptr;
			break;
		}
		ptr = ptr->next;
	}


   	return ptr;
   	*/
}

int kz_amqp_open_next_connection(kz_amqp_conn_ptr ptr) {
	if(ptr == NULL) {
		LM_ERR("OPEN CONNECTION PTR == NULL\n");
		return -1;
	}

	if(kz_pool == NULL) {
		LM_ERR("OPEN CONNECTION POOL == NULL\n");
		return -2;
	}

	if(ptr->info == NULL) {
		ptr->info = kz_pool->head;
	} else {
		ptr->info = ptr->info->next;
		if(ptr->info == NULL) {
			ptr->info = kz_pool->head;
		}
	}

	while(ptr->conn == NULL) {
		if(kz_amqp_connection_open(ptr) == 0) {
			break;
		}
		ptr->info = ptr->info->next;
		if(ptr->info == NULL) {
			LM_INFO("all connections tried, restarting from head\n");
			sleep(3);
			ptr->info = kz_pool->head;
		}
	}


   	return 0;
}


int kz_amqp_consume_error(amqp_connection_state_t conn)
{
	amqp_frame_t frame;
	int ret = 0;
	amqp_rpc_reply_t reply;

	if (AMQP_STATUS_OK != amqp_simple_wait_frame_noblock(conn, &frame, &kz_amqp_tv)) {
		// should i ignore this or close the connection?
		LM_ERR("ERROR ON SIMPLE_WAIT_FRAME\n");
		return ret;
	}

	if (AMQP_FRAME_METHOD == frame.frame_type) {
		switch (frame.payload.method.id) {
		case AMQP_BASIC_ACK_METHOD:
			/* if we've turned publisher confirms on, and we've published a message
			 * here is a message being confirmed
			 */
			ret = 1;
			break;
		case AMQP_BASIC_RETURN_METHOD:
			/* if a published message couldn't be routed and the mandatory flag was set
			 * this is what would be returned. The message then needs to be read.
			 */
			{
				ret = 1;
			amqp_message_t message;
			reply = amqp_read_message(conn, frame.channel, &message, 0);
			if (AMQP_RESPONSE_NORMAL != reply.reply_type) {
				LM_ERR("AMQP_BASIC_RETURN_METHOD read_message\n");
				break;
			}

			LM_DBG("Received this message : %.*s\n", (int) message.body.len, (char*)message.body.bytes);
			amqp_destroy_message(&message);
			}
			break;

		case AMQP_CHANNEL_CLOSE_METHOD:
			/* a channel.close method happens when a channel exception occurs, this
			 * can happen by publishing to an exchange that doesn't exist for example
			 *
			 * In this case you would need to open another channel redeclare any queues
			 * that were declared auto-delete, and restart any consumers that were attached
			 * to the previous channel
			 */
			LM_ERR("AMQP_CHANNEL_CLOSE_METHOD\n");
			if(frame.channel > 0)
				channels[frame.channel-1].state = KZ_AMQP_CLOSED;
			break;

		case AMQP_CONNECTION_CLOSE_METHOD:
			/* a connection.close method happens when a connection exception occurs,
			 * this can happen by trying to use a channel that isn't open for example.
			 *
			 * In this case the whole connection must be restarted.
			 */
			break;

		default:
			LM_ERR("An unexpected method was received %d\n", frame.payload.method.id);
			break;
		}
	};

	return ret;
}

void kz_amqp_add_payload_common_properties(json_obj_ptr json_obj, char* server_id, str* unique) {
    char node_name[512];


    json_object_object_add(json_obj, BLF_JSON_APP_NAME,
			   json_object_new_string(NAME));
    json_object_object_add(json_obj, BLF_JSON_APP_VERSION,
			   json_object_new_string(VERSION));
    sprintf(node_name, "kamailio@%.*s", dbk_node_hostname.len, dbk_node_hostname.s);
    json_object_object_add(json_obj, BLF_JSON_NODE,
			   json_object_new_string(node_name));
    json_object_object_add(json_obj, BLF_JSON_MSG_ID,
			   json_object_new_string_len(unique->s, unique->len));

}

int kz_amqp_pipe_send(str *str_exchange, str *str_routing_key, str *str_payload)
{
	int ret = 1;
    json_obj_ptr json_obj = NULL;
    kz_amqp_cmd_ptr cmd = NULL;

    str unique_string = { 0, 0 };
    char serverid[512];

    uuid_t id;
    char uuid_buffer[40];

    uuid_generate_random(id);
    uuid_unparse_lower(id, uuid_buffer);
    unique_string.s = uuid_buffer;
    unique_string.len = strlen(unique_string.s);

    sprintf(serverid, "kamailio@%.*s-<%d>-script-%lu", dbk_node_hostname.len, dbk_node_hostname.s, my_pid(), rpl_query_routing_key_count++);


    /* parse json  and add extra fields */
    json_obj = kz_json_parse(str_payload->s);
    if (json_obj == NULL)
	goto error;

    kz_amqp_add_payload_common_properties(json_obj, serverid, &unique_string);

    char *payload = (char *)json_object_to_json_string(json_obj);

	cmd = (kz_amqp_cmd_ptr)shm_malloc(sizeof(kz_amqp_cmd));
	if(cmd == NULL) {
		LM_ERR("failed to allocate kz_amqp_cmd in process %d\n", getpid());
		goto error;
	}
	memset(cmd, 0, sizeof(kz_amqp_cmd));
	cmd->exchange = kz_amqp_str_dup(str_exchange);
	cmd->routing_key = kz_amqp_str_dup(str_routing_key);
	cmd->payload = kz_amqp_string_dup(payload);
	if(cmd->payload == NULL || cmd->routing_key == NULL || cmd->exchange == NULL) {
		LM_ERR("failed to allocate kz_amqp_cmd parameters in process %d\n", getpid());
		goto error;
	}
	if(lock_init(&cmd->lock)==NULL)
	{
		LM_ERR("cannot init the lock for publishing in process %d\n", getpid());
		lock_dealloc(&cmd->lock);
		goto error;
	}
	lock_get(&cmd->lock);
	cmd->type = KZ_AMQP_PUBLISH;
	cmd->consumer = getpid();
	if (write(kz_pipe_fds[1], &cmd, sizeof(cmd)) != sizeof(cmd)) {
		LM_ERR("failed to publish message to amqp in process %d, write to command pipe: %s\n", getpid(), strerror(errno));
	} else {
		lock_get(&cmd->lock);
		ret = 1;//cmd->return_code;
	}

	error:

	if(cmd)
		kz_amqp_free_pipe_cmd(cmd);

    if(json_obj)
    	json_object_put(json_obj);

	return ret;
}

int kz_amqp_pipe_send_receive(str *str_exchange, str *str_routing_key, str *str_payload, struct timeval* kz_timeout, json_obj_ptr* json_ret )
{
	int ret = 1;
    json_obj_ptr json_obj = NULL;
    kz_amqp_cmd_ptr cmd = NULL;
    json_obj_ptr json_body = NULL;

    str unique_string = { 0, 0 };
    char serverid[512];

    uuid_t id;
    char uuid_buffer[40];

    uuid_generate_random(id);
    uuid_unparse_lower(id, uuid_buffer);
    unique_string.s = uuid_buffer;
    unique_string.len = strlen(unique_string.s);

    sprintf(serverid, "kamailio@%.*s-<%d>-script-%lu", dbk_node_hostname.len, dbk_node_hostname.s, my_pid(), rpl_query_routing_key_count++);


    /* parse json  and add extra fields */
    json_obj = kz_json_parse(str_payload->s);
    if (json_obj == NULL)
	goto error;

    kz_amqp_add_payload_common_properties(json_obj, serverid, &unique_string);

    char *payload = (char *)json_object_to_json_string(json_obj);

	cmd = (kz_amqp_cmd_ptr)shm_malloc(sizeof(kz_amqp_cmd));
	if(cmd == NULL) {
		LM_ERR("failed to allocate kz_amqp_cmd in process %d\n", getpid());
		goto error;
	}
	memset(cmd, 0, sizeof(kz_amqp_cmd));
	cmd->exchange = kz_amqp_str_dup(str_exchange);
	cmd->routing_key = kz_amqp_str_dup(str_routing_key);
	cmd->reply_routing_key = kz_amqp_string_dup(serverid);
	cmd->payload = kz_amqp_string_dup(payload);

	cmd->timeout = *kz_timeout;

	if(cmd->payload == NULL || cmd->routing_key == NULL || cmd->exchange == NULL) {
		LM_ERR("failed to allocate kz_amqp_cmd parameters in process %d\n", getpid());
		goto error;
	}
	if(lock_init(&cmd->lock)==NULL)
	{
		LM_ERR("cannot init the lock for publishing in process %d\n", getpid());
		lock_dealloc(&cmd->lock);
		goto error;
	}
	lock_get(&cmd->lock);
	cmd->type = KZ_AMQP_CALL;
	cmd->consumer = getpid();
	if (write(kz_pipe_fds[1], &cmd, sizeof(cmd)) != sizeof(cmd)) {
		LM_ERR("failed to publish message to amqp in process %d, write to command pipe: %s\n", getpid(), strerror(errno));
	} else {
		lock_get(&cmd->lock);
		switch(cmd->return_code) {
		case AMQP_RESPONSE_NORMAL:
			json_body = kz_json_parse(cmd->return_payload);
		    if (json_body == NULL)
			goto error;
		    *json_ret = json_body;
		    ret = 0;
		    break;

		default:
			ret = -1;
			break;
		}
	}

 error:
	if(cmd)
		kz_amqp_free_pipe_cmd(cmd);

    if(json_obj)
    	json_object_put(json_obj);

    return ret;
}

int kz_amqp_publish(struct sip_msg* msg, char* exchange, char* routing_key, char* payload)
{
	  str json_s;
	  str exchange_s;
	  str routing_key_s;

		if (fixup_get_svalue(msg, (gparam_p)exchange, &exchange_s) != 0) {
			LM_ERR("cannot get exchange string value\n");
			return -1;
		}

		if (fixup_get_svalue(msg, (gparam_p)routing_key, &routing_key_s) != 0) {
			LM_ERR("cannot get routing_key string value\n");
			return -1;
		}

		if (fixup_get_svalue(msg, (gparam_p)payload, &json_s) != 0) {
			LM_ERR("cannot get json string value : %s\n", payload);
			return -1;
		}

		struct json_object *j = json_tokener_parse(json_s.s);

		if (is_error(j)) {
			LM_ERR("empty or invalid JSON payload : %.*s\n", json_s.len, json_s.s);
			return -1;
		}

		json_object_put(j);

		return kz_amqp_pipe_send(&exchange_s, &routing_key_s, &json_s );


};


char* last_payload_result = NULL;

int kz_pv_get_last_query_result(struct sip_msg *msg, pv_param_t *param,	pv_value_t *res)
{
	return last_payload_result == NULL ? pv_get_null(msg, param, res) : pv_get_strzval(msg, param, res, last_payload_result);
}


int kz_amqp_query_ex(struct sip_msg* msg, char* exchange, char* routing_key, char* payload)
{
	  str json_s;
	  str exchange_s;
	  str routing_key_s;
	  struct timeval kz_timeout = kz_qtimeout_tv;

	  if(last_payload_result)
		pkg_free(last_payload_result);

	  last_payload_result = NULL;

		if (fixup_get_svalue(msg, (gparam_p)exchange, &exchange_s) != 0) {
			LM_ERR("cannot get exchange string value\n");
			return -1;
		}

		if (fixup_get_svalue(msg, (gparam_p)routing_key, &routing_key_s) != 0) {
			LM_ERR("cannot get routing_key string value\n");
			return -1;
		}

		if (fixup_get_svalue(msg, (gparam_p)payload, &json_s) != 0) {
			LM_ERR("cannot get json string value : %s\n", payload);
			return -1;
		}

		struct json_object *j = json_tokener_parse(json_s.s);

		if (is_error(j)) {
			LM_ERR("empty or invalid JSON payload : %*.s\n", json_s.len, json_s.s);
			return -1;
		}

		json_object_put(j);

		if(kz_query_timeout_spec.type != PVT_NONE) {
			pv_value_t pv_val;
			if(pv_get_spec_value( msg, &kz_query_timeout_spec, &pv_val) == 0) {
				if((pv_val.flags & PV_VAL_INT) && pv_val.ri != 0 ) {
					kz_timeout.tv_usec = (pv_val.ri % 1000) * 1000;
					kz_timeout.tv_sec = pv_val.ri / 1000;
					LM_DBG("setting timeout to %i,%i\n", (int) kz_timeout.tv_sec, (int) kz_timeout.tv_usec);
				}
			}
		}

		json_obj_ptr ret = NULL;
		int res = kz_amqp_pipe_send_receive(&exchange_s, &routing_key_s, &json_s, &kz_timeout, &ret );

		if(res != 0) {
			return -1;
		}

		char* strjson = (char*)json_object_to_json_string(ret);
		int len = strlen(strjson);
		char* value = pkg_malloc(len+1);
		memcpy(value, strjson, len);
		value[len] = '\0';
		last_payload_result = value;
		json_object_put(ret);

		return 1;
};

int kz_amqp_query(struct sip_msg* msg, char* exchange, char* routing_key, char* payload, char* dst)
{

	  pv_spec_t *dst_pv;
	  pv_value_t dst_val;

	  int result = kz_amqp_query_ex(msg, exchange, routing_key, payload);
	  if(result == -1)
		  return result;

		dst_pv = (pv_spec_t *)dst;
		if(last_payload_result != NULL) {
			dst_val.rs.s = last_payload_result;
			dst_val.rs.len = strlen(last_payload_result);
			dst_val.flags = PV_VAL_STR;
		} else {
			dst_val.rs.s = NULL;
			dst_val.rs.len = 0;
			dst_val.ri = 0;
			dst_val.flags = PV_VAL_NULL;
		}
		dst_pv->setf(msg, &dst_pv->pvp, (int)EQ_T, &dst_val);

		return 1;
};

int kz_amqp_subscribe_simple(struct sip_msg* msg, char* exchange, char* exchange_type, char* queue, char* routing_key)
{
	str exchange_s;
	str exchange_type_s;
	str queue_s;
	str routing_key_s;

	if (fixup_get_svalue(msg, (gparam_p)exchange, &exchange_s) != 0) {
		LM_ERR("cannot get exchange string value\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_p)exchange_type, &exchange_type_s) != 0) {
		LM_ERR("cannot get exchange type string value\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_p)queue, &queue_s) != 0) {
		LM_ERR("cannot get queue string value\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_p)routing_key, &routing_key_s) != 0) {
		LM_ERR("cannot get routing_key string value\n");
		return -1;
	}

	kz_amqp_bind_ptr bind = kz_amqp_bind_alloc(&exchange_s, &exchange_type_s, &queue_s, &routing_key_s);
	if(bind == NULL) {
		LM_ERR("Could not allocate bind struct\n");
		goto error;
	}

	bind->auto_delete = 1;
	bind->no_ack = 1;

	kz_amqp_binding_ptr binding = shm_malloc(sizeof(kz_amqp_binding));
	if(binding == NULL) {
		LM_ERR("Could not allocate binding struct\n");
		goto error;
	}
	memset(binding, 0, sizeof(kz_amqp_binding));

	if(kz_bindings->head == NULL)
		kz_bindings->head = binding;

	if(kz_bindings->tail != NULL)
		kz_bindings->tail->next = binding;

	kz_bindings->tail = binding;
	binding->bind = bind;
	bindings_count++;

    return 1;

error:
    if(binding != NULL)
    	shm_free(binding);

	return -1;

}

int kz_amqp_subscribe(struct sip_msg* msg, char* payload)
{
	str exchange_s;
	str exchange_type_s;
	str queue_s;
	str routing_key_s;
	str payload_s;
	str key_s;
	str subkey_s;
	int passive = 0;
	int durable = 0;
	int exclusive = 0;
	int auto_delete = 1;
	int no_ack = 1;
	int wait_for_consumer_ack = 1;

    json_obj_ptr json_obj = NULL;
	struct json_object* tmpObj = NULL;

	if (fixup_get_svalue(msg, (gparam_p)payload, &payload_s) != 0) {
		LM_ERR("cannot get payload value\n");
		return -1;
	}

    json_obj = kz_json_parse(payload_s.s);
    if (json_obj == NULL)
	return -1;
    

    json_extract_field("exchange", exchange_s);
    json_extract_field("type", exchange_type_s);
    json_extract_field("queue", queue_s);
    json_extract_field("routing", routing_key_s);
    json_extract_field("event_key", key_s);
    json_extract_field("event_subkey", subkey_s);

    tmpObj = kz_json_get_object(json_obj, "passive");
    if(tmpObj != NULL) {
    	passive = json_object_get_int(tmpObj);
    }

    tmpObj = kz_json_get_object(json_obj, "durable");
    if(tmpObj != NULL) {
    	durable = json_object_get_int(tmpObj);
    }

    tmpObj = kz_json_get_object(json_obj, "exclusive");
    if(tmpObj != NULL) {
    	exclusive = json_object_get_int(tmpObj);
    }

    tmpObj = kz_json_get_object(json_obj, "auto_delete");
    if(tmpObj != NULL) {
    	auto_delete = json_object_get_int(tmpObj);
    }

    tmpObj = kz_json_get_object(json_obj, "no_ack");
    if(tmpObj != NULL) {
    	no_ack = json_object_get_int(tmpObj);
    }

    tmpObj = kz_json_get_object(json_obj, "wait_for_consumer_ack");
    if(tmpObj != NULL) {
    	wait_for_consumer_ack = json_object_get_int(tmpObj);
    }


	kz_amqp_bind_ptr bind = kz_amqp_bind_alloc(&exchange_s, &exchange_type_s, &queue_s, &routing_key_s);
	if(bind == NULL) {
		LM_ERR("Could not allocate bind struct\n");
		goto error;
	}

	bind->durable = durable;
	bind->passive = passive;
	bind->exclusive = exclusive;
	bind->auto_delete = auto_delete;
	bind->no_ack = no_ack;
	bind->wait_for_consumer_ack = wait_for_consumer_ack;


	kz_amqp_binding_ptr binding = shm_malloc(sizeof(kz_amqp_binding));
	if(binding == NULL) {
		LM_ERR("Could not allocate binding struct\n");
		goto error;
	}
	memset(binding, 0, sizeof(kz_amqp_binding));

	if(kz_bindings->head == NULL)
		kz_bindings->head = binding;

	if(kz_bindings->tail != NULL)
		kz_bindings->tail->next = binding;

	kz_bindings->tail = binding;
	binding->bind = bind;
	bindings_count++;

    if(json_obj != NULL)
       	json_object_put(json_obj);

    return 1;

error:
    if(binding != NULL)
    	shm_free(binding);

    if(json_obj != NULL)
       	json_object_put(json_obj);

	return -1;

}


#define KEY_SAFE(C)  ((C >= 'a' && C <= 'z') || \
                      (C >= 'A' && C <= 'Z') || \
                      (C >= '0' && C <= '9') || \
                      (C == '-' || C == '~'  || C == '_'))

#define HI4(C) (C>>4)
#define LO4(C) (C & 0x0F)

#define hexint(C) (C < 10?('0' + C):('A'+ C - 10))

char *kz_amqp_util_encode(const str * key, char *dest) {
    if ((key->len == 1) && (key->s[0] == '#' || key->s[0] == '*')) {
	*dest++ = key->s[0];
	return dest;
    }
    char *p, *end;
    for (p = key->s, end = key->s + key->len; p < end; p++) {
	if (KEY_SAFE(*p)) {
	    *dest++ = *p;
	} else if (*p == '.') {
	    memcpy(dest, "\%2E", 3);
	    dest += 3;
	} else if (*p == ' ') {
	    *dest++ = '+';
	} else {
	    *dest++ = '%';
	    sprintf(dest, "%c%c", hexint(HI4(*p)), hexint(LO4(*p)));
	    dest += 2;
	}
    }
    *dest = '\0';
    return dest;
}

int kz_amqp_encode_ex(str* unencoded, pv_value_p dst_val)
{
	char routing_key_buff[256];
	memset(routing_key_buff,0, sizeof(routing_key_buff));
	kz_amqp_util_encode(unencoded, routing_key_buff);

	int len = strlen(routing_key_buff);
	dst_val->rs.s = pkg_malloc(len+1);
	memcpy(dst_val->rs.s, routing_key_buff, len);
	dst_val->rs.s[len] = '\0';
	dst_val->rs.len = len;
	dst_val->flags = PV_VAL_STR | PV_VAL_PKG;

	return 1;

}

int kz_amqp_encode(struct sip_msg* msg, char* unencoded, char* encoded)
{
    str unencoded_s;
	pv_spec_t *dst_pv;
	pv_value_t dst_val;
	dst_pv = (pv_spec_t *)encoded;

	if (fixup_get_svalue(msg, (gparam_p)unencoded, &unencoded_s) != 0) {
		LM_ERR("cannot get unencoded string value\n");
		return -1;
	}

	kz_amqp_encode_ex(&unencoded_s, &dst_val);
	dst_pv->setf(msg, &dst_pv->pvp, (int)EQ_T, &dst_val);

	if(dst_val.flags & PV_VAL_PKG)
		pkg_free(dst_val.rs.s);
	else if(dst_val.flags & PV_VAL_SHM)
		shm_free(dst_val.rs.s);


	return 1;

}

int get_channel_index() {
	int n;
	for(n=channel_index; n < dbk_channels; n++)
		if(channels[n].state == KZ_AMQP_FREE) {
			channel_index = n+1;
			return n;
		}
	if(channel_index == 0) {
		LM_ERR("max channels (%d) reached. please exit kamailio and change kazoo amqp_max_channels param", dbk_channels);
		return -1;
	}
	channel_index = 0;
	return get_channel_index();
}

int kz_amqp_bind_targeted_channel(kz_amqp_conn_ptr kz_conn, int idx )
{
    kz_amqp_bind_ptr bind = channels[idx].targeted;
    amqp_queue_declare_ok_t *r = NULL;
    int ret = -1;

    r = amqp_queue_declare(kz_conn->conn, channels[idx].channel, bind->queue, 0, 0, 1, 1, kz_amqp_empty_table);
    if (kz_amqp_error("Declaring queue", amqp_get_rpc_reply(kz_conn->conn)))
    {
		goto error;
    }

	amqp_exchange_declare(kz_conn->conn, channels[idx].channel, bind->exchange, bind->exchange_type, 0, 0, kz_amqp_empty_table);
    if (kz_amqp_error("Declaring exchange", amqp_get_rpc_reply(kz_conn->conn)))
    {
		ret = -RET_AMQP_ERROR;
		goto error;
    }

    if (amqp_queue_bind(kz_conn->conn, channels[idx].channel, bind->queue, bind->exchange, bind->routing_key, kz_amqp_empty_table) < 0
	    || kz_amqp_error("Binding queue", amqp_get_rpc_reply(kz_conn->conn)))
    {
		goto error;
    }

    if (amqp_basic_consume(kz_conn->conn, channels[idx].channel, bind->queue, kz_amqp_empty_bytes, 0, 1, 1, kz_amqp_empty_table) < 0
	    || kz_amqp_error("Consuming", amqp_get_rpc_reply(kz_conn->conn)))
    {
		goto error;
    }

    return 0;
 error:
    return ret;
}

int kz_amqp_bind_targeted_channel_ex(kz_amqp_conn_ptr kz_conn, int loopcount, int idx )
{
    kz_amqp_bind_ptr bind = NULL;
    amqp_queue_declare_ok_t *r = NULL;
    str rpl_exch = str_init("targeted");
    str rpl_exch_type = str_init("direct");
    int ret = -1;
    char serverid[512];

    bind = (kz_amqp_bind_ptr)shm_malloc(sizeof(kz_amqp_bind));
	if(bind == NULL) {
		LM_ERR("error allocation memory for reply\n");
		goto error;
	}
	memset(bind, 0, sizeof(kz_amqp_bind));

	bind->exchange = kz_amqp_bytes_dup_from_str(&rpl_exch);
	bind->exchange_type = kz_amqp_bytes_dup_from_str(&rpl_exch_type);

    sprintf(serverid, "kamailio@%.*s-<%d-%d-%d>", dbk_node_hostname.len, dbk_node_hostname.s, my_pid(), loopcount, idx);
    bind->queue = kz_amqp_bytes_dup_from_string(serverid);

    sprintf(serverid, "kamailio@%.*s-<%d-%d>-targeted-%d", dbk_node_hostname.len, dbk_node_hostname.s, my_pid(), loopcount, idx);
    bind->routing_key = kz_amqp_bytes_dup_from_string(serverid);


    if (bind->exchange.bytes == NULL || bind->routing_key.bytes == NULL || bind->queue.bytes == NULL) {
		LM_ERR("Out of memory allocating for exchange/routing_key\n");
		goto error;
    }

    r = amqp_queue_declare(kz_conn->conn, channels[idx].channel, bind->queue, 0, 0, 1, 1, kz_amqp_empty_table);
    if (kz_amqp_error("Declaring queue", amqp_get_rpc_reply(kz_conn->conn)))
    {
		goto error;
    }

	amqp_exchange_declare(kz_conn->conn, channels[idx].channel, bind->exchange, bind->exchange_type, 0, 0, kz_amqp_empty_table);
    if (kz_amqp_error("Declaring exchange", amqp_get_rpc_reply(kz_conn->conn)))
    {
		ret = -RET_AMQP_ERROR;
		goto error;
    }

    if (amqp_queue_bind(kz_conn->conn, channels[idx].channel, bind->queue, bind->exchange, bind->routing_key, kz_amqp_empty_table) < 0
	    || kz_amqp_error("Binding queue", amqp_get_rpc_reply(kz_conn->conn)))
    {
		goto error;
    }

    if (amqp_basic_consume(kz_conn->conn, channels[idx].channel, bind->queue, kz_amqp_empty_bytes, 0, 1, 1, kz_amqp_empty_table) < 0
	    || kz_amqp_error("Consuming", amqp_get_rpc_reply(kz_conn->conn)))
    {
		goto error;
    }

    channels[idx].targeted = bind;
    return 0;
 error:
	kz_amqp_free_bind(bind);
    return ret;
}

int kz_amqp_bind_targeted_channels(kz_amqp_conn_ptr kz_conn , int loopcount)
{
	int i, ret;
	for(i = 0; i < dbk_channels; i++) {
		ret = kz_amqp_bind_targeted_channel_ex(kz_conn, loopcount, i);
		if(ret != 0)
			return ret;
	}
	return 0;
}

int kz_amqp_bind_consumer_ex(kz_amqp_conn_ptr kz_conn, kz_amqp_bind_ptr bind, int idx, kz_amqp_channel_ptr chan)
{
    int ret = -1;

    amqp_queue_declare(kz_conn->conn, chan[idx].channel, bind->queue, bind->passive, bind->durable, bind->exclusive, bind->auto_delete, kz_amqp_empty_table);
    if (kz_amqp_error("Declaring queue", amqp_get_rpc_reply(kz_conn->conn)))
    {
		ret = -RET_AMQP_ERROR;
		goto error;
    }

	amqp_exchange_declare(kz_conn->conn, chan[idx].channel, bind->exchange, bind->exchange_type, 0, 0, kz_amqp_empty_table);
    if (kz_amqp_error("Declaring exchange", amqp_get_rpc_reply(kz_conn->conn)))
    {
		ret = -RET_AMQP_ERROR;
		goto error;
    }

    LM_DBG("QUEUE BIND\n");
    if (amqp_queue_bind(kz_conn->conn, chan[idx].channel, bind->queue, bind->exchange, bind->routing_key, kz_amqp_empty_table) < 0
	    || kz_amqp_error("Binding queue", amqp_get_rpc_reply(kz_conn->conn)))
    {
		ret = -RET_AMQP_ERROR;
		goto error;
    }

    LM_DBG("BASIC CONSUME\n");
    if (amqp_basic_consume(kz_conn->conn, chan[idx].channel, bind->queue, kz_amqp_empty_bytes, 0, bind->no_ack, 0, kz_amqp_empty_table) < 0
	    || kz_amqp_error("Consuming", amqp_get_rpc_reply(kz_conn->conn)))
    {
		ret = -RET_AMQP_ERROR;
		goto error;
    }

    chan[idx].state = KZ_AMQP_CONSUMING;
	chan[idx].consumer = bind;
    ret = idx;
 error:

    return ret;
}


int kz_amqp_bind_consumer(kz_amqp_conn_ptr kz_conn, kz_amqp_bind_ptr bind)
{
    int ret = -1;

    int	idx = get_channel_index();

    amqp_queue_declare(kz_conn->conn, channels[idx].channel, bind->queue, bind->passive, bind->durable, bind->exclusive, bind->auto_delete, kz_amqp_empty_table);
    if (kz_amqp_error("Declaring queue", amqp_get_rpc_reply(kz_conn->conn)))
    {
		ret = -RET_AMQP_ERROR;
		goto error;
    }

	amqp_exchange_declare(kz_conn->conn, channels[idx].channel, bind->exchange, bind->exchange_type, 0, 0, kz_amqp_empty_table);
    if (kz_amqp_error("Declaring exchange", amqp_get_rpc_reply(kz_conn->conn)))
    {
		ret = -RET_AMQP_ERROR;
		goto error;
    }

    LM_DBG("QUEUE BIND\n");
    if (amqp_queue_bind(kz_conn->conn, channels[idx].channel, bind->queue, bind->exchange, bind->routing_key, kz_amqp_empty_table) < 0
	    || kz_amqp_error("Binding queue", amqp_get_rpc_reply(kz_conn->conn)))
    {
		ret = -RET_AMQP_ERROR;
		goto error;
    }

    LM_DBG("BASIC CONSUME\n");
    if (amqp_basic_consume(kz_conn->conn, channels[idx].channel, bind->queue, kz_amqp_empty_bytes, 0, bind->no_ack, 0, kz_amqp_empty_table) < 0
	    || kz_amqp_error("Consuming", amqp_get_rpc_reply(kz_conn->conn)))
    {
		ret = -RET_AMQP_ERROR;
		goto error;
    }

    channels[idx].state = KZ_AMQP_CONSUMING;
	channels[idx].consumer = bind;
    ret = idx;
 error:

    return ret;
}

int kz_amqp_send_ex(kz_amqp_conn_ptr kz_conn, kz_amqp_cmd_ptr cmd, kz_amqp_channel_state state, int idx)
{
	amqp_bytes_t exchange;
	amqp_bytes_t routing_key;
	amqp_bytes_t payload;
	int ret = -1;
    json_obj_ptr json_obj = NULL;

	amqp_basic_properties_t props;
	memset(&props, 0, sizeof(amqp_basic_properties_t));
	props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG;
	props.content_type = amqp_cstring_bytes("application/json");

	if(idx == -1) {
		idx = get_channel_index();
		if(idx == -1) {
			LM_ERR("Failed to get channel index to publish\n");
			goto error;
		}
	}

    exchange = amqp_bytes_malloc_dup(amqp_cstring_bytes(cmd->exchange));
    routing_key = amqp_bytes_malloc_dup(amqp_cstring_bytes(cmd->routing_key));
    payload = amqp_bytes_malloc_dup(amqp_cstring_bytes(cmd->payload));

    json_obj = kz_json_parse(cmd->payload);
    if (json_obj == NULL)
    	goto error;

    if(kz_json_get_object(json_obj, BLF_JSON_SERVERID) == NULL) {
        json_object_object_add(json_obj, BLF_JSON_SERVERID, json_object_new_string((char*)channels[idx].targeted->routing_key.bytes));
    	amqp_bytes_free(payload);
        payload = amqp_bytes_malloc_dup(amqp_cstring_bytes((char*)json_object_to_json_string(json_obj)));
    }

	int amqpres = amqp_basic_publish(kz_conn->conn, channels[idx].channel, exchange, routing_key, 0, 0, &props, payload);
	if ( amqpres != AMQP_STATUS_OK ) {
		LM_ERR("Failed to publish\n");
		ret = -1;
		goto error;
	}

	if ( kz_amqp_error("Publishing",  amqp_get_rpc_reply(kz_conn->conn)) ) {
		LM_ERR("Failed to publish\n");
		ret = -1;
		goto error;
	}
	gettimeofday(&channels[idx].timer, NULL);
	channels[idx].state = state;
	channels[idx].cmd = cmd;

	ret = idx;

	error:

	if(json_obj)
    	json_object_put(json_obj);

	if(exchange.bytes)
		amqp_bytes_free(exchange);
	if(routing_key.bytes)
		amqp_bytes_free(routing_key);
	if(payload.bytes)
		amqp_bytes_free(payload);

	return ret;
}

int kz_amqp_send(kz_amqp_conn_ptr kz_conn, kz_amqp_cmd_ptr cmd)
{
	return kz_amqp_send_ex(kz_conn, cmd, KZ_AMQP_PUBLISHING , -1);
}


int kz_amqp_send_receive_ex(kz_amqp_conn_ptr kz_conn, kz_amqp_cmd_ptr cmd, int idx )
{
//	int newidx = kz_amqp_bind_channel_ex(kz_conn, cmd, idx);
//	if(newidx >= 0)
//		return kz_amqp_send_ex(kz_conn, cmd, KZ_AMQP_CALLING, newidx);
		return kz_amqp_send_ex(kz_conn, cmd, KZ_AMQP_CALLING, idx);
//	return newidx;
}

int kz_amqp_send_receive(kz_amqp_conn_ptr kz_conn, kz_amqp_cmd_ptr cmd )
{
	return kz_amqp_send_receive_ex(kz_conn, cmd, -1 );
}

char* eventData = NULL;

int kz_pv_get_event_payload(struct sip_msg *msg, pv_param_t *param,	pv_value_t *res)
{
	return eventData == NULL ? pv_get_null(msg, param, res) : pv_get_strzval(msg, param, res, eventData);
}

int kz_amqp_consumer_fire_event(char *eventkey)
{
	struct sip_msg *fmsg;
	struct run_act_ctx ctx;
	int rtb, rt;

	LM_DBG("searching event_route[%s]\n", eventkey);
	rt = route_get(&event_rt, eventkey);
	if (rt < 0 || event_rt.rlist[rt] == NULL)
	{
		LM_DBG("route %s does not exist\n", eventkey);
		return -2;
	}
	LM_DBG("executing event_route[%s] (%d)\n", eventkey, rt);
	if(faked_msg_init()<0)
		return -2;
	fmsg = faked_msg_next();
	rtb = get_route_type();
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);
	run_top_route(event_rt.rlist[rt], fmsg, 0);
	set_route_type(rtb);
	return 0;

}

void kz_amqp_consumer_event(int child_no, char *payload, char* event_key, char* event_subkey)
{
    json_obj_ptr json_obj = NULL;
    str ev_name = {0, 0}, ev_category = {0, 0};
    char buffer[512];
    char * p;

    eventData = payload;

    json_obj = kz_json_parse(payload);
    if (json_obj == NULL)
		return;

    char* key = (event_key == NULL ? dbk_consumer_event_key.s : event_key);
    char* subkey = (event_subkey == NULL ? dbk_consumer_event_subkey.s : event_subkey);

    json_extract_field(key, ev_category);
    json_extract_field(subkey, ev_name);

    sprintf(buffer, "kazoo:consumer-event-%.*s-%.*s",ev_category.len, ev_category.s, ev_name.len, ev_name.s);
    for (p=buffer ; *p; ++p) *p = tolower(*p);
    for (p=buffer ; *p; ++p) if(*p == '_') *p = '-';
    if(kz_amqp_consumer_fire_event(buffer) != 0) {
        sprintf(buffer, "kazoo:consumer-event-%.*s",ev_category.len, ev_category.s);
        for (p=buffer ; *p; ++p) *p = tolower(*p);
        for (p=buffer ; *p; ++p) if(*p == '_') *p = '-';
        if(kz_amqp_consumer_fire_event(buffer) != 0) {
            sprintf(buffer, "kazoo:consumer-event-%s-%s", key, subkey);
            for (p=buffer ; *p; ++p) *p = tolower(*p);
            for (p=buffer ; *p; ++p) if(*p == '_') *p = '-';
            if(kz_amqp_consumer_fire_event(buffer) != 0) {
                sprintf(buffer, "kazoo:consumer-event-%s", key);
                for (p=buffer ; *p; ++p) *p = tolower(*p);
                for (p=buffer ; *p; ++p) if(*p == '_') *p = '-';
				if(kz_amqp_consumer_fire_event(buffer) != 0) {
					sprintf(buffer, "kazoo:consumer-event");
					if(kz_amqp_consumer_fire_event(buffer) != 0) {
						LM_ERR("kazoo:consumer-event not found");
					}
				}
            }
        }
    }
	if(json_obj)
    	json_object_put(json_obj);

	eventData = NULL;
}

void kz_amqp_consumer_loop(int child_no)
{

	LM_DBG("starting consumer %d\n", child_no);
	close(kz_pipe_fds[child_no*2+1]);
	int data_pipe = kz_pipe_fds[child_no*2];
//	int back_idx = (dbk_consumer_processes+1)*2+1;

	fd_set fdset;
    int selret;

    while(1) {
    	FD_ZERO(&fdset);
    	FD_SET(data_pipe, &fdset);

    	selret = select(FD_SETSIZE, &fdset, NULL, NULL, NULL);

    	if (selret < 0) {
    		LM_ERR("select() failed: %s\n", strerror(errno));
    	} else if (!selret) {
    	} else {
			if(FD_ISSET(data_pipe, &fdset)) {
				kz_amqp_consumer_delivery_ptr ptr;
				if(read(data_pipe, &ptr, sizeof(ptr)) == sizeof(ptr)) {
					LM_DBG("consumer %d received payload %s\n", child_no, ptr->payload);
					kz_amqp_consumer_event(child_no, ptr->payload, ptr->event_key, ptr->event_subkey);
					/*
					if(ptr->channel > 0 && ptr->delivery_tag > 0) {
						kz_amqp_cmd_ptr cmd = kz_amqp_alloc_pipe_cmd();
						cmd->type = KZ_AMQP_ACK;
						cmd->channel = ptr->channel;
						cmd->delivery_tag = ptr->delivery_tag;
						if (write(kz_pipe_fds[back_idx], &cmd, sizeof(cmd)) != sizeof(cmd)) {
							LM_ERR("failed to send ack to AMQP Manager in process %d, write to command pipe: %s\n", getpid(), strerror(errno));
						}
					}
					*/
					kz_amqp_free_consumer_delivery(ptr);
				}
			}
    	}
	}
	LM_DBG("exiting consumer %d\n", child_no);
}

int check_timeout(struct timeval *now, struct timeval *start, struct timeval *timeout)
{
	struct timeval chk;
	chk.tv_sec = now->tv_sec - start->tv_sec;
	chk.tv_usec = now->tv_usec - start->tv_usec;
	if(chk.tv_usec >= timeout->tv_usec)
		if(chk.tv_sec >= timeout->tv_sec)
			return 1;
	return 0;
}

int consumer = 1;

void kz_amqp_send_consumer_event_ex(char* payload, char* event_key, char* event_subkey, amqp_channel_t channel, uint64_t delivery_tag, int nextConsumer)
{
	kz_amqp_consumer_delivery_ptr ptr = (kz_amqp_consumer_delivery_ptr) shm_malloc(sizeof(kz_amqp_consumer_delivery));
	if(ptr == NULL) {
		LM_ERR("NO MORE SHARED MEMORY!");
		return;
	}
	memset(ptr, 0, sizeof(kz_amqp_consumer_delivery));
	ptr->channel = channel;
	ptr->delivery_tag = delivery_tag;
	ptr->payload = payload;
	ptr->event_key = event_key;
	ptr->event_subkey = event_subkey;
	if (write(kz_pipe_fds[(consumer+2)*2+1], &ptr, sizeof(ptr)) != sizeof(ptr)) {
		LM_ERR("failed to send payload to consumer %d : %s\nPayload %s\n", consumer, strerror(errno), payload);
	}

	if(nextConsumer) {
		consumer++;
		if(consumer > dbk_consumer_processes) {
			consumer = 1;
		}
	}
}

void kz_amqp_send_consumer_event(char* payload, int nextConsumer)
{
	kz_amqp_send_consumer_event_ex(payload, NULL, NULL, 0, 0, nextConsumer);
}

void kz_amqp_fire_connection_event(char *event, char* host)
{
	char* payload = (char*)shm_malloc(512);
	sprintf(payload, "{ \"%.*s\" : \"connection\", \"%.*s\" : \"%s\", \"host\" : \"%s\" }",
			dbk_consumer_event_key.len, dbk_consumer_event_key.s,
			dbk_consumer_event_subkey.len, dbk_consumer_event_subkey.s,
			event, host
			);
	kz_amqp_send_consumer_event(payload, 1);
}

void kz_amqp_manager_loop(int child_no)
{
	LM_DBG("starting manager %d\n", child_no);
	close(kz_pipe_fds[child_no*2+1]);
	close(kz_pipe_fds[(dbk_consumer_processes+1)*2+1]);
	int data_pipe = kz_pipe_fds[child_no*2];
	int back_pipe = kz_pipe_fds[(dbk_consumer_processes+1)*2];
    fd_set fdset;
    int i, idx;
    int selret;
	int INTERNAL_READ, CONSUME, ACK_READ,  OK;
	int INTERNAL_READ_COUNT , INTERNAL_READ_MAX_LOOP;
	int CONSUMER_READ_COUNT , CONSUMER_READ_MAX_LOOP;
	int ACK_READ_COUNT , ACK_READ_MAX_LOOP;
	char* payload;
	int channel_res;
    kz_amqp_conn_ptr kzconn;
	kz_amqp_cmd_ptr cmd;
    int loopcount = 0;
    int firstLoop = dbk_consume_messages_on_reconnect;


    while(1) {

        INTERNAL_READ_MAX_LOOP = dbk_internal_loop_count;
        CONSUMER_READ_MAX_LOOP = dbk_consumer_loop_count;
        ACK_READ_MAX_LOOP = dbk_consumer_ack_loop_count;

    	OK = 1;

    	while(1) {
    		kzconn = kz_amqp_get_next_connection();
    		if(kzconn != NULL)
    			break;
    		LM_DBG("Connection failed : all servers down?");
    		sleep(3);
    	}

    	kz_amqp_fire_connection_event("open", kzconn->info->info.host);

    	loopcount++;
    	for(i=0,channel_res=0; i < dbk_channels && channel_res == 0; i++) {
    		/* start cleanup */
    		channels[i].state = KZ_AMQP_CLOSED;
    		channels[i].consumer = NULL;
    		if(channels[i].targeted != NULL) {
    			kz_amqp_free_bind(channels[i].targeted);
    			channels[i].targeted = NULL;
    		}
    		cmd = channels[i].cmd;
			if(cmd != NULL) {
				channels[i].cmd = NULL;
				cmd->return_code = -1;
				lock_release(&cmd->lock);
			}
    		/* end cleanup */
    		channel_res = kz_amqp_channel_open(kzconn, channels[i].channel);
    		if(channel_res == 0) {
    			kz_amqp_bind_targeted_channel_ex(kzconn, loopcount, i);
				channels[i].state = KZ_AMQP_FREE;
    		}
    	}
    	channel_index = 0;
    	/* bind consumers */
    	if(kz_bindings != NULL) {
    		kz_amqp_binding_ptr binding = kz_bindings->head;
    		while(binding != NULL) {
    			kz_amqp_bind_consumer(kzconn, binding->bind);
    			binding = binding->next;
    		}
    	}

    	firstLoop = dbk_consume_messages_on_reconnect;
    	while(OK) {
        	INTERNAL_READ = 1;
    		CONSUME = 1;
    		ACK_READ = 1;


    		ACK_READ_COUNT = 0;
        	while(ACK_READ && ACK_READ_COUNT < ACK_READ_MAX_LOOP) {
        		ACK_READ_COUNT++;
				FD_ZERO(&fdset);
				FD_SET(back_pipe, &fdset);
				selret = select(FD_SETSIZE, &fdset, NULL, NULL, &kz_ack_tv);
				if (selret < 0) {
					LM_ERR("select() failed: %s\n", strerror(errno));
					break;
				} else if (!selret) {
					ACK_READ=0;
				} else {
					if(FD_ISSET(back_pipe, &fdset)) {
						if(read(back_pipe, &cmd, sizeof(cmd)) == sizeof(cmd)) {
							switch (cmd->type) {
								case KZ_AMQP_ACK:
									if(amqp_basic_ack(kzconn->conn, cmd->channel, cmd->delivery_tag, 0 ) < 0) {
										LM_ERR("AMQP ERROR TRYING TO ACK A MSG RETURNED FROM CONSUMER\n");
										OK = CONSUME = 0;
									}
									kz_amqp_free_pipe_cmd(cmd);
									break;
								default:
									LM_DBG("unknown pipe cmd %d\n", cmd->type);
									break;
							}
						}
					}
				}
        	}
    		INTERNAL_READ_COUNT = 0;
        	while(INTERNAL_READ && INTERNAL_READ_COUNT < INTERNAL_READ_MAX_LOOP) {
        		INTERNAL_READ_COUNT++;
				FD_ZERO(&fdset);
				FD_SET(data_pipe, &fdset);
				selret = select(FD_SETSIZE, &fdset, NULL, NULL, &kz_sock_tv);
				if (selret < 0) {
					LM_ERR("select() failed: %s\n", strerror(errno));
					break;
				} else if (!selret) {
					INTERNAL_READ=0;
				} else {
					if(FD_ISSET(data_pipe, &fdset)) {
						if(read(data_pipe, &cmd, sizeof(cmd)) == sizeof(cmd)) {
							switch (cmd->type) {
							case KZ_AMQP_PUBLISH:
								idx = kz_amqp_send(kzconn, cmd);
								if(idx >= 0) {
									cmd->return_code = AMQP_RESPONSE_NORMAL;
								} else {
									cmd->return_code = -1;
									OK = INTERNAL_READ = CONSUME = 0;
									LM_ERR("ERROR SENDING PUBLISH");
								}
								channels[idx].state = KZ_AMQP_FREE;
								channels[idx].cmd = NULL;
								lock_release(&cmd->lock);
								break;
							case KZ_AMQP_CALL:
								idx = kz_amqp_send_receive(kzconn, cmd);
								if(idx < 0) {
									channels[idx].state = KZ_AMQP_FREE;
									channels[idx].cmd = NULL;
									cmd->return_code = -1;
									lock_release(&cmd->lock);
									LM_ERR("ERROR SENDING QUERY");
									OK = INTERNAL_READ = CONSUME = 0;
								} else {
									gettimeofday(&channels[idx].timer, NULL);
								}
								break;
							default:
								LM_DBG("unknown pipe cmd %d\n", cmd->type);
								break;
							}
						}
					}
				}
        	}

    		CONSUMER_READ_COUNT = 0;
    	    while(CONSUME && (CONSUMER_READ_COUNT < CONSUMER_READ_MAX_LOOP || firstLoop)) {
        		payload = NULL;
        		CONSUMER_READ_COUNT++;
				amqp_envelope_t envelope;
				amqp_maybe_release_buffers(kzconn->conn);
				amqp_rpc_reply_t reply = amqp_consume_message(kzconn->conn, &envelope, &kz_amqp_tv, 0);
				switch(reply.reply_type) {
				case AMQP_RESPONSE_LIBRARY_EXCEPTION:
					switch(reply.library_error) {
					case AMQP_STATUS_HEARTBEAT_TIMEOUT:
						LM_ERR("AMQP_STATUS_HEARTBEAT_TIMEOUT\n");
						OK = CONSUME = 0;
						break;
					case AMQP_STATUS_TIMEOUT:
						CONSUME = 0;
						break;
					case AMQP_STATUS_UNEXPECTED_STATE:
						LM_DBG("AMQP_STATUS_UNEXPECTED_STATE\n");
						OK = CONSUME = kz_amqp_consume_error(kzconn->conn);
						break;
					default:
						OK = CONSUME = 0;
						break;
					};
					break;

				case AMQP_RESPONSE_NORMAL:
					idx = envelope.channel-1;
					switch(channels[idx].state) {
					case KZ_AMQP_CALLING:
						channels[idx].cmd->return_payload = kz_amqp_bytes_dup(envelope.message.body);
						channels[idx].cmd->return_code = AMQP_RESPONSE_NORMAL;
						lock_release(&channels[idx].cmd->lock);
						channels[idx].state = KZ_AMQP_FREE;
						channels[idx].cmd = NULL;
						break;
					case KZ_AMQP_CONSUMING:
						kz_amqp_send_consumer_event_ex(kz_amqp_bytes_dup(envelope.message.body),
								kz_amqp_bytes_dup(channels[idx].consumer->event_key),
								kz_amqp_bytes_dup(channels[idx].consumer->event_subkey),
								channels[idx].consumer->no_ack ? 0 : envelope.channel,
								channels[idx].consumer->no_ack ? 0 : envelope.delivery_tag,
								(firstLoop && dbk_single_consumer_on_reconnect) ? 0 : 1);
						if(!channels[idx].consumer->no_ack ) {
							if(channels[idx].consumer->wait_for_consumer_ack) {
								LM_DBG("MSG ACK delayed until return from consumer\n");
							} else {
								if(amqp_basic_ack(kzconn->conn, envelope.channel, envelope.delivery_tag, 0 ) < 0) {
									LM_ERR("AMQP ERROR TRYING TO ACK A MSG\n");
									OK = CONSUME = 0;
								}
							}
						}
						break;
					default:
						break;
					}
					break;
				case AMQP_RESPONSE_SERVER_EXCEPTION:
					LM_ERR("AMQP_RESPONSE_SERVER_EXCEPTION in consume\n");
					OK = CONSUME = 0;
					break;

				default:
					LM_ERR("UNHANDLED AMQP_RESPONSE in consume\n");
					OK = CONSUME = 0;
					break;
				};
				amqp_destroy_envelope(&envelope);
    	    }

			/* check timeouts */
			if(OK && (!firstLoop)) {
				struct timeval now;
				gettimeofday(&now, NULL);
				for(i=0; i < dbk_channels; i++) {
					if(channels[i].state == KZ_AMQP_CALLING
							&& channels[i].cmd != NULL
							&& check_timeout(&now, &channels[i].timer, &channels[i].cmd->timeout)) {
						cmd = channels[i].cmd;
						channels[i].state = KZ_AMQP_FREE;
						channels[i].cmd = NULL;
						cmd->return_code = -1;
						lock_release(&cmd->lock);
						// rebind ??
						LM_ERR("QUERY TIMEOUT");
					}
				}
			}
			firstLoop = 0;
    	}
    	kz_amqp_connection_close(kzconn);
    	kz_amqp_fire_connection_event("closed", kzconn->info->info.host);
    }
}


/* check timeouts */
void kz_amqp_timeout_proc(int child_no)
{
	kz_amqp_cmd_ptr cmd;
	int i;
    while(1) {
		struct timeval now;
		usleep(kz_timer_tv.tv_usec);
		for(i=0; i < dbk_channels; i++) {
			gettimeofday(&now, NULL);
			if(channels[i].state == KZ_AMQP_CALLING
					&& channels[i].cmd != NULL
					&& check_timeout(&now, &channels[i].timer, &channels[i].cmd->timeout)) {
				lock_get(&channels[i].lock);
				if(channels[i].cmd != NULL)
				{
					cmd = channels[i].cmd;
					LM_DBG("Kazoo Query timeout - %s\n", cmd->payload);
					cmd->return_code = -1;
					lock_release(&cmd->lock);
					channels[i].cmd = NULL;
					channels[i].state = KZ_AMQP_FREE;
				}
				lock_release(&channels[i].lock);
			}
		}
	}
}

void kz_amqp_publisher_proc(int child_no)
{
	LM_DBG("starting publisher %d\n", child_no);
	close(kz_pipe_fds[child_no*2+1]);
	int data_pipe = kz_pipe_fds[child_no*2];
	LM_DBG("publisher started %d\n", child_no);
    fd_set fdset;
    int idx, i;
    int selret;
	int OK;
    kz_amqp_conn_ptr kzconn;
	kz_amqp_cmd_ptr cmd;
	int channel_res;

    kzconn = (kz_amqp_conn_ptr)pkg_malloc(sizeof(kz_amqp_conn));
    if(kzconn == NULL)
    {
    	LM_ERR("NO MORE PACKAGE MEMORY\n");
    	return;
    }
    memset(kzconn, 0, sizeof(kz_amqp_conn));

    while(1) {
    	OK = 1;
   		if(kz_amqp_open_next_connection(kzconn)) {
   			LM_ERR("Error opening connection\n");
   			sleep(3);
   			continue;
   		}
    	kz_amqp_fire_connection_event("open", kzconn->info->info.host);

    	for(i=0,channel_res=0; i < dbk_channels && channel_res == 0; i++) {
			/* start cleanup */
			channels[i].state = KZ_AMQP_CLOSED;
			cmd = channels[i].cmd;
			if(cmd != NULL) {
				channels[i].cmd = NULL;
				cmd->return_code = -1;
				lock_release(&cmd->lock);
			}
			/* end cleanup */

			/* bind targeted channels */
			channel_res = kz_amqp_channel_open(kzconn, channels[i].channel);
			if(channel_res == 0) {
				channels[i].state = KZ_AMQP_FREE;
			}

    	}

    	while(OK) {
			FD_ZERO(&fdset);
			FD_SET(data_pipe, &fdset);
			selret = select(FD_SETSIZE, &fdset, NULL, NULL, NULL);
			if (selret < 0) {
				LM_ERR("select() failed: %s\n", strerror(errno));
				continue;
			} else if (!selret) {
				continue;
			} else {
				if(FD_ISSET(data_pipe, &fdset) && read(data_pipe, &cmd, sizeof(cmd)) == sizeof(cmd)) {
					switch (cmd->type) {
						case KZ_AMQP_PUBLISH:
							idx = kz_amqp_send(kzconn, cmd);
							if(idx >= 0) {
								cmd->return_code = AMQP_RESPONSE_NORMAL;
							} else {
								cmd->return_code = -1;
								OK = 0;
								LM_ERR("ERROR SENDING PUBLISH");
							}
							channels[idx].cmd = NULL;
							lock_release(&cmd->lock);
							channels[idx].state = KZ_AMQP_FREE;
							break;
						case KZ_AMQP_CALL:
							idx = kz_amqp_send_receive(kzconn, cmd);
							if(idx < 0) {
								channels[idx].cmd = NULL;
								cmd->return_code = -1;
								lock_release(&cmd->lock);
								channels[idx].state = KZ_AMQP_FREE;
								LM_ERR("ERROR SENDING QUERY");
								OK = 0;
							}
							break;
						default:
							LM_DBG("unknown pipe cmd %d\n", cmd->type);
							break;
					}
				}
        	}
    	}
    	kz_amqp_connection_close(kzconn);
    	kz_amqp_fire_connection_event("closed", kzconn->info->info.host);
    }
}

void kz_amqp_consumer_proc(int child_no)
{
	LM_DBG("starting consumer %d\n", child_no);
	close(kz_pipe_fds[child_no*2+1]);
    int i, idx;
	int OK;
	char* payload;
	int channel_res;
    kz_amqp_conn_ptr kzconn;
    kz_amqp_channel_ptr consumer_channels = NULL;

    kzconn = (kz_amqp_conn_ptr)pkg_malloc(sizeof(kz_amqp_conn));
    if(kzconn == NULL)
    {
    	LM_ERR("NO MORE PACKAGE MEMORY\n");
    	return;
    }
    memset(kzconn, 0, sizeof(kz_amqp_conn));

    consumer_channels = (kz_amqp_channel_ptr)pkg_malloc(sizeof(kz_amqp_channel)*bindings_count);
    if(consumer_channels == NULL)
    {
    	LM_ERR("NO MORE PACKAGE MEMORY\n");
    	return;
    }
	for(i=0; i < bindings_count; i++)
		consumer_channels[i].channel = dbk_channels + i + 1;

    while(1) {
    	OK = 1;
   		if(kz_amqp_open_next_connection(kzconn)) {
   			LM_ERR("Error opening connection\n");
   			sleep(3);
   			continue;
   		}
    	kz_amqp_fire_connection_event("open", kzconn->info->info.host);

    	/* reset channels */

    	for(i=0,channel_res=0; i < dbk_channels && channel_res == 0; i++) {
			/* start cleanup */
			channels[i].consumer = NULL;

			/*
			if(channels[i].targeted != NULL) {
				kz_amqp_free_bind(channels[i].targeted);
				channels[i].targeted = NULL;
			}
			*/

			/* end cleanup */

			/* bind targeted channels */
			channel_res = kz_amqp_channel_open(kzconn, channels[i].channel);
			if(channel_res == 0) {
				kz_amqp_bind_targeted_channel(kzconn, i);
			}
    	}

    	for(i=0,channel_res=0; i < bindings_count && channel_res == 0; i++) {
			/* start cleanup */
    		consumer_channels[i].consumer = NULL;
			/* end cleanup */

			/* bind targeted channels */
			channel_res = kz_amqp_channel_open(kzconn, consumer_channels[i].channel);
    	}

    	i = 0;
		/* bind consumers */
		if(kz_bindings != NULL) {
			kz_amqp_binding_ptr binding = kz_bindings->head;
			while(binding != NULL) {
				kz_amqp_bind_consumer_ex(kzconn, binding->bind, i, consumer_channels);
				binding = binding->next;
				i++;
			}
		}

		LM_DBG("CONSUMER INIT DONE\n");

		while(OK) {
			payload = NULL;
			amqp_envelope_t envelope;
			amqp_maybe_release_buffers(kzconn->conn);
			amqp_rpc_reply_t reply = amqp_consume_message(kzconn->conn, &envelope, NULL, 0);
			switch(reply.reply_type) {
			case AMQP_RESPONSE_LIBRARY_EXCEPTION:
				switch(reply.library_error) {
				case AMQP_STATUS_HEARTBEAT_TIMEOUT:
					LM_ERR("AMQP_STATUS_HEARTBEAT_TIMEOUT\n");
					OK=0;
					break;
				case AMQP_STATUS_TIMEOUT:
					break;
				case AMQP_STATUS_UNEXPECTED_STATE:
					LM_DBG("AMQP_STATUS_UNEXPECTED_STATE\n");
					OK = kz_amqp_consume_error(kzconn->conn);
					break;
				default:
					OK = 0;
					break;
				};
				break;

			case AMQP_RESPONSE_NORMAL:
				idx = envelope.channel-1;
				if(idx < dbk_channels) {
					switch(channels[idx].state) {
						case KZ_AMQP_CALLING:
							lock_get(&channels[idx].lock);
							if(channels[idx].cmd != NULL) {
								channels[idx].cmd->return_payload = kz_amqp_bytes_dup(envelope.message.body);
								channels[idx].cmd->return_code = AMQP_RESPONSE_NORMAL;
								lock_release(&channels[idx].cmd->lock);
								channels[idx].cmd = NULL;
								channels[idx].state = KZ_AMQP_FREE;
							}
							lock_release(&channels[idx].lock);
							break;
						default:
							LM_DBG("ignoring received payload on consumer - %.*s\n", (int) envelope.message.body.len, (char*)envelope.message.body.bytes);
							break;
					}
				} else {
					idx = idx - dbk_channels;
					kz_amqp_send_consumer_event_ex(kz_amqp_bytes_dup(envelope.message.body),
						kz_amqp_bytes_dup(consumer_channels[idx].consumer->event_key),
						kz_amqp_bytes_dup(consumer_channels[idx].consumer->event_subkey),
						0, 0, 1);
					if(!consumer_channels[idx].consumer->no_ack ) {
						if(amqp_basic_ack(kzconn->conn, envelope.channel, envelope.delivery_tag, 0 ) < 0) {
							LM_ERR("AMQP ERROR TRYING TO ACK A MSG\n");
							OK = 0;
						}
					}
				}
				break;
			case AMQP_RESPONSE_SERVER_EXCEPTION:
				LM_ERR("AMQP_RESPONSE_SERVER_EXCEPTION in consume\n");
				OK = 0;
				break;

			default:
				LM_ERR("UNHANDLED AMQP_RESPONSE in consume\n");
				OK = 0;
				break;
			};
			amqp_destroy_envelope(&envelope);
		}

    	kz_amqp_connection_close(kzconn);
    	kz_amqp_fire_connection_event("closed", kzconn->info->info.host);

    }
}

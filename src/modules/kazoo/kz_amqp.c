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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <amqp.h>
#include <amqp_framing.h>
#include <amqp_tcp_socket.h>
#include <amqp_ssl_socket.h>
#include <json.h>
#include <uuid/uuid.h>
#include "../../core/mem/mem.h"
#include "../../core/timer_proc.h"
#include "../../core/sr_module.h"
#include "../../core/pvar.h"
#include "../../core/mod_fix.h"
#include "../../core/lvalue.h"
#include "../tm/tm_load.h"
#include "../../core/route.h"
#include "../../core/receive.h"
#include "../../core/action.h"
#include "../../core/script_cb.h"


#include "kz_amqp.h"
#include "kz_json.h"
#include "kz_hash.h"

#define RET_AMQP_ERROR 2

struct tm_binds tmb;

kz_amqp_bindings_ptr kz_bindings = NULL;
int bindings_count = 0;

static unsigned long rpl_query_routing_key_count = 0;

typedef struct json_object *json_obj_ptr;

extern pv_spec_t kz_query_result_spec;

extern int *kz_worker_pipes;
extern int kz_cmd_pipe;

extern struct timeval kz_amqp_tv;
extern struct timeval kz_qtimeout_tv;
extern struct timeval kz_timer_tv;
extern struct timeval kz_amqp_connect_timeout_tv;

extern str kz_amqps_ca_cert;
extern str kz_amqps_cert;
extern str kz_amqps_key;
extern int kz_amqps_verify_peer;
extern int kz_amqps_verify_hostname;

extern pv_spec_t kz_query_timeout_spec;

const amqp_bytes_t kz_amqp_empty_bytes = { 0, NULL };
const amqp_table_t kz_amqp_empty_table = { 0, NULL };

kz_amqp_zones_ptr kz_zones = NULL;
kz_amqp_zone_ptr kz_primary_zone = NULL;


amqp_exchange_declare_ok_t * AMQP_CALL kz_amqp_exchange_declare(amqp_connection_state_t state, amqp_channel_t channel, kz_amqp_exchange_ptr exchange, amqp_table_t arguments)
{
	LM_DBG("declare exchange %.*s , %.*s\n",
	        (int)exchange->name.len, (char*)exchange->name.bytes,
	        (int)exchange->type.len, (char*)exchange->type.bytes);

#if AMQP_VERSION_MAJOR == 0 && AMQP_VERSION_MINOR < 6
	return amqp_exchange_declare(state, channel, exchange->name, exchange->type,
	                             exchange->passive, exchange->durable,
	                             arguments);
#else
	return amqp_exchange_declare(state, channel, exchange->name, exchange->type,
	                             exchange->passive, exchange->durable,
	                             exchange->auto_delete, exchange->internal,
	                             arguments);
#endif
}

amqp_queue_declare_ok_t * AMQP_CALL kz_amqp_queue_declare(amqp_connection_state_t state, amqp_channel_t channel, kz_amqp_queue_ptr queue, amqp_table_t arguments)
{
	return amqp_queue_declare(state, channel, queue->name, queue->passive, queue->durable, queue->exclusive, queue->auto_delete, arguments);
}

amqp_queue_bind_ok_t * AMQP_CALL kz_amqp_queue_bind(amqp_connection_state_t state, amqp_channel_t channel, kz_amqp_exchange_ptr exchange, kz_amqp_queue_ptr queue, kz_amqp_routings_ptr queue_bindings, amqp_table_t arguments)
{
	amqp_queue_bind_ok_t *ret = amqp_queue_bind(state, channel, queue->name, exchange->name, queue_bindings->routing, arguments);

   	if(ret >= 0 && queue_bindings->next) {
   		return kz_amqp_queue_bind(state, channel, exchange, queue, queue_bindings->next, arguments);
   	} else {
   		return ret;
   	}
}

int set_non_blocking(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0)
		return flags;
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0)
		return -1;

	return 0;
}

static inline str* kz_str_dup(str* src)
{
	char *dst_char = (char*)shm_malloc(sizeof(str)+src->len+1);
	if (!dst_char) {
		LM_ERR("error allocating shared memory for str");
		return NULL;
	}
	str* dst = (str*)dst_char;
	dst->s = dst_char+sizeof(str);

	memcpy(dst->s, src->s, src->len);
	dst->len = src->len;
	dst->s[dst->len] = '\0';
	return dst;
}

static inline str* kz_str_dup_from_char(char* src)
{
	int len = strlen(src);
	char *dst_char = (char*)shm_malloc(sizeof(str)+len+1);
	if (!dst_char) {
		LM_ERR("error allocating shared memory for str");
		return NULL;
	}
	str* dst = (str*)dst_char;
	dst->s = dst_char+sizeof(str);

	memcpy(dst->s, src, len);
	dst->len = len;
	dst->s[dst->len] = '\0';
	return dst;
}


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

static char *kz_local_amqp_str_dup(str *src)
{
	char *res;

	if (!src || !src->s)
		return NULL;
	if (!(res = (char *) pkg_malloc(src->len + 1)))
		return NULL;
	strncpy(res, src->s, src->len);
	res[src->len] = 0;
	return res;
}


static inline str* kz_local_str_dup(str* src)
{
	char *dst_char = (char*)pkg_malloc(sizeof(str)+src->len+1);
	if (!dst_char) {
		LM_ERR("error allocating shared memory for str");
		return NULL;
	}
	str* dst = (str*)dst_char;
	dst->s = dst_char+sizeof(str);

	memcpy(dst->s, src->s, src->len);
	dst->len = src->len;
	dst->s[dst->len] = '\0';
	return dst;
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

static inline str* kz_str_from_amqp_bytes(amqp_bytes_t src)
{
	char *dst_char = (char*)shm_malloc(sizeof(str)+src.len+1);
	if (!dst_char) {
		LM_ERR("error allocating shared memory for str");
		return NULL;
	}
	str* dst = (str*)dst_char;
	dst->s = dst_char+sizeof(str);

	memcpy(dst->s, src.bytes, src.len);
	dst->len = src.len;
	dst->s[dst->len] = '\0';
	return dst;
}

char *kz_local_amqp_bytes_dup(amqp_bytes_t bytes)
{
	char *res;
	int sz;
	if (!bytes.bytes )
		return NULL;

	sz = bytes.len;
	if (!(res = (char *) pkg_malloc(sz + 1)))
		return NULL;
	strncpy(res, bytes.bytes, sz);
	res[sz] = 0;
	return res;
}

void kz_amqp_bytes_free(amqp_bytes_t bytes)
{
	if(bytes.bytes)
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

void kz_local_amqp_bytes_free(amqp_bytes_t bytes)
{
	if(bytes.bytes)
		pkg_free(bytes.bytes);
}

amqp_bytes_t kz_local_amqp_bytes_malloc_dup(amqp_bytes_t src)
{
  amqp_bytes_t result = {0, 0};
  result.len = src.len;
  result.bytes = pkg_malloc(src.len+1);
  if (result.bytes != NULL) {
    memcpy(result.bytes, src.bytes, src.len);
    ((char*)result.bytes)[result.len] = '\0';
  }
  return result;
}

amqp_bytes_t kz_local_amqp_bytes_dup_from_string(char *src)
{
	return kz_local_amqp_bytes_malloc_dup(amqp_cstring_bytes(src));
}

amqp_bytes_t kz_amqp_bytes_dup_from_string(char *src)
{
	return kz_amqp_bytes_malloc_dup(amqp_cstring_bytes(src));
}

amqp_bytes_t kz_amqp_bytes_dup_from_str(str *src)
{
	return kz_amqp_bytes_malloc_dup(amqp_cstring_bytes(src->s));
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
	if (cmd->message_id)
		shm_free(cmd->message_id);
	if (cmd->cb_route)
		shm_free(cmd->cb_route);
	if (cmd->err_route)
		shm_free(cmd->err_route);
	lock_release(&cmd->lock);
	lock_destroy(&cmd->lock);
	shm_free(cmd);
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
	if(ptr->message_id)
		shm_free(ptr->message_id);
	if(ptr->routing_key)
		shm_free(ptr->routing_key);
	if(ptr->cmd)
		kz_amqp_free_pipe_cmd(ptr->cmd);
	shm_free(ptr);
}

void kz_amqp_free_bind(kz_amqp_bind_ptr bind)
{
	if(bind == NULL)
		return;
	if(bind->exchange)
		kz_amqp_exchange_free(bind->exchange);
	if(bind->exchange_bindings)
		kz_amqp_exchange_bindings_free(bind->exchange_bindings);
	if(bind->queue)
		kz_amqp_queue_free(bind->queue);
	if(bind->queue_bindings)
		kz_amqp_routing_free(bind->queue_bindings);
	if(bind->event_key.bytes)
		kz_amqp_bytes_free(bind->event_key);
	if(bind->event_subkey.bytes)
		kz_amqp_bytes_free(bind->event_subkey);
	if(bind->consistent_worker_key)
		shm_free(bind->consistent_worker_key);
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

kz_amqp_bind_ptr kz_amqp_bind_alloc(kz_amqp_exchange_ptr exchange, kz_amqp_exchange_binding_ptr exchange_bindings, kz_amqp_queue_ptr queue, kz_amqp_routings_ptr queue_bindings, str* event_key, str* event_subkey )
{
	kz_amqp_bind_ptr bind = NULL;

	bind = (kz_amqp_bind_ptr)shm_malloc(sizeof(kz_amqp_bind));
	if(bind == NULL) {
		LM_ERR("error allocation memory for bind alloc\n");
		goto error;
	}
	memset(bind, 0, sizeof(kz_amqp_bind));

	bind->exchange = exchange;
	bind->queue = queue;
	bind->exchange_bindings = exchange_bindings;
	bind->queue_bindings = queue_bindings;

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

kz_amqp_zone_ptr kz_amqp_get_primary_zone() {
	if(kz_primary_zone == NULL) {
		kz_primary_zone = (kz_amqp_zone_ptr) shm_malloc(sizeof(kz_amqp_zone));
		memset(kz_primary_zone, 0, sizeof(kz_amqp_zone));
		kz_primary_zone->zone = shm_malloc(dbk_primary_zone_name.len+1);
		strcpy(kz_primary_zone->zone, dbk_primary_zone_name.s);
		kz_primary_zone->zone[dbk_primary_zone_name.len] = '\0';
		kz_primary_zone->servers = (kz_amqp_servers_ptr) shm_malloc(sizeof(kz_amqp_servers));
		memset(kz_primary_zone->servers, 0, sizeof(kz_amqp_servers));
	}
	return kz_primary_zone;
}

kz_amqp_zone_ptr kz_amqp_get_zones() {
	if(kz_zones == NULL) {
		kz_zones = (kz_amqp_zones_ptr) shm_malloc(sizeof(kz_amqp_zones));
		kz_zones->head = kz_zones->tail = kz_amqp_get_primary_zone();
	}
	return kz_zones->head;
}

kz_amqp_zone_ptr kz_amqp_get_zone(char* zone) {
	kz_amqp_zone_ptr ret = NULL;
	for(ret = kz_amqp_get_zones(); ret != NULL; ret = ret->next)
		if(!strcmp(ret->zone, zone))
			return ret;
	return NULL;
}

kz_amqp_zone_ptr kz_amqp_add_zone(char* zone) {
	kz_amqp_zone_ptr zone_ptr = (kz_amqp_zone_ptr) shm_malloc(sizeof(kz_amqp_zone));
	memset(zone_ptr, 0, sizeof(kz_amqp_zone));
	zone_ptr->zone = shm_malloc(strlen(zone)+1);
	strcpy(zone_ptr->zone, zone);
	zone_ptr->zone[strlen(zone)] = '\0';
	zone_ptr->servers = (kz_amqp_servers_ptr) shm_malloc(sizeof(kz_amqp_servers));
	memset(zone_ptr->servers, 0, sizeof(kz_amqp_servers));
	kz_zones->tail->next = zone_ptr;
	kz_zones->tail = zone_ptr;
	return zone_ptr;
}

kz_amqp_queue_ptr kz_amqp_targeted_queue(char *name)
{
	str queue = str_init(name);
	return kz_amqp_queue_new(&queue);
}

int kz_amqp_bind_init_targeted_channel(kz_amqp_server_ptr server, int idx )
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

	bind->exchange = kz_amqp_exchange_new(&rpl_exch, &rpl_exch_type);
	if(bind->exchange == NULL) {
		LM_ERR("error allocation memory for reply\n");
		goto error;
	}

    sprintf(serverid, "kamailio@%.*s-<%d-%d>", dbk_node_hostname.len, dbk_node_hostname.s, server->id, idx);
    bind->queue = kz_amqp_targeted_queue(serverid);
	if(bind->queue == NULL) {
		LM_ERR("error allocation memory for reply\n");
		goto error;
	}

    sprintf(serverid, "kamailio@%.*s-<%d>-targeted-%d", dbk_node_hostname.len, dbk_node_hostname.s, server->id, idx);
    bind->queue_bindings = kz_amqp_routing_new(serverid);

    if (bind->queue_bindings == NULL) {
		LM_ERR("Out of memory allocating for exchange/routing_key\n");
		goto error;
    }

    server->channels[idx].targeted = bind;
    return 0;
 error:
	kz_amqp_free_bind(bind);
    return ret;
}

int kz_tm_bind()
{
	load_tm_f  load_tm;

	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0)))
	{
		LOG(L_ERR, "cannot import load_tm\n");
		return 0;
	}
	if (load_tm( &tmb )==-1)
		return 0;
	return 1;
}


int kz_amqp_init() {
	int i;
	kz_amqp_zone_ptr g;
	kz_amqp_server_ptr s;

	if(!kz_hash_init())
		return 0;

	if(!kz_tm_bind())
		return 0;

	if(kz_bindings == NULL) {
		kz_bindings = (kz_amqp_bindings_ptr) shm_malloc(sizeof(kz_amqp_bindings));
		memset(kz_bindings, 0, sizeof(kz_amqp_bindings));
	}

	for (g = kz_amqp_get_zones(); g != NULL; g = g->next) {
		for (s = g->servers->head; s != NULL; s = s->next) {
			if(s->channels == NULL) {
				s->channels = shm_malloc(dbk_channels * sizeof(kz_amqp_channel));
				memset(s->channels, 0, dbk_channels * sizeof(kz_amqp_channel));
				for(i=0; i < dbk_channels; i++) {
					s->channels[i].channel = i+1;
					if(lock_init(&s->channels[i].lock)==NULL) {
						LM_ERR("could not initialize locks for channels\n");
						return 0;
					}
					if(kz_amqp_bind_init_targeted_channel(s, i)) {
						LM_ERR("could not initialize targeted channels\n");
						return 0;
					}
				}
			}
		}
	}
	return 1;
}

void kz_amqp_destroy_connection(kz_amqp_connection_ptr s)
{
	kz_amqp_free_connection(s);
}

void kz_amqp_destroy_connection_ptr(kz_amqp_conn_ptr s)
{
}

void kz_amqp_destroy_channels(kz_amqp_server_ptr server_ptr)
{
    int i;
	if(server_ptr->channels == NULL) {
		return;
	}
	for(i=0; i < dbk_channels; i++) {
		if(server_ptr->channels[i].targeted != NULL) {
			kz_amqp_free_bind(server_ptr->channels[i].targeted);
		}
	}
	shm_free(server_ptr->channels);
	server_ptr->channels = NULL;
}

kz_amqp_server_ptr kz_amqp_destroy_server(kz_amqp_server_ptr server_ptr)
{
    kz_amqp_server_ptr next = server_ptr->next;
	kz_amqp_destroy_connection(server_ptr->connection);
	kz_amqp_destroy_channels(server_ptr);
	if (server_ptr->producer) { shm_free(server_ptr->producer); }
	shm_free(server_ptr);
	return next;
}

kz_amqp_zone_ptr kz_amqp_destroy_zone(kz_amqp_zone_ptr zone_ptr)
{
	kz_amqp_zone_ptr next = zone_ptr->next;
	kz_amqp_server_ptr server_ptr = zone_ptr->servers->head;
	while(server_ptr) {
	    server_ptr = kz_amqp_destroy_server(server_ptr);
	}
	shm_free(zone_ptr->zone);
	shm_free(zone_ptr->servers);
	shm_free(zone_ptr);
	return next;
}

void kz_amqp_destroy_zones()
{
	kz_amqp_zone_ptr g = kz_amqp_get_zones();
	while(g) {
		g = kz_amqp_destroy_zone(g);
	}
	shm_free(kz_zones);
	kz_zones = NULL;
	kz_primary_zone = NULL;
}

void kz_amqp_destroy() {
	kz_amqp_destroy_zones();
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
	kz_hash_destroy();
}

#define KZ_URL_MAX_SIZE 512
static char* KZ_URL_ROOT = "/";

int kz_amqp_add_connection(modparam_t type, void* val)
{
	kz_amqp_zone_ptr zone_ptr = NULL;

	char* url = (char*) val;
	int len = strlen(url);
	if(len > KZ_URL_MAX_SIZE) {
		LM_ERR("connection url exceeds max size %d\n", KZ_URL_MAX_SIZE);
		return -1;
	}

	if(!strncmp(url, "zone=", 5)) {
		char* ptr = strchr(url, ';');
		char* zone_str_ptr = url+(5*sizeof(char));
		if(ptr == NULL) {
			LM_ERR("missing ';' at the end of zone name '%s'\n", url);
			return -1;
		}
		*ptr = '\0';
		if(strlen(zone_str_ptr) == 0) {
			LM_ERR("invalid zone name '%s'\n", url);
			return -1;
		}
		zone_ptr = kz_amqp_get_zone(zone_str_ptr);
		if(zone_ptr == NULL) {
			zone_ptr = kz_amqp_add_zone(zone_str_ptr);
			if(zone_ptr == NULL) {
				LM_ERR("unable to add zone %s\n", zone_str_ptr);
				return -1;
			}
		}
		url = ++ptr;

	} else {
		zone_ptr = kz_amqp_get_zones();
	}


	kz_amqp_connection_ptr newConn = shm_malloc(sizeof(kz_amqp_connection));
	memset(newConn, 0, sizeof(kz_amqp_connection));

	newConn->url = shm_malloc( (KZ_URL_MAX_SIZE + 1) * sizeof(char) );
	memset(newConn->url, 0, (KZ_URL_MAX_SIZE + 1) * sizeof(char));
	// maintain compatibility
	if (!strncmp(url, "kazoo://", 8)) {
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
#if AMQP_VERSION_MAJOR == 0 && AMQP_VERSION_MINOR < 6
    } else if(newConn->info.vhost[0] == '/' && strlen(newConn->info.vhost) == 1) { // bug in amqp_parse_url ?
    	newConn->info.vhost++;
#endif
    }

	kz_amqp_server_ptr server_ptr = (kz_amqp_server_ptr)shm_malloc(sizeof(kz_amqp_server));
	server_ptr->connection = newConn;
	server_ptr->id = ++kz_server_counter;
	server_ptr->zone = zone_ptr;
	if(zone_ptr->servers->tail) {
		zone_ptr->servers->tail->next = server_ptr;
		zone_ptr->servers->tail = server_ptr;
	} else {
		zone_ptr->servers->head = server_ptr;
		zone_ptr->servers->tail = server_ptr;
	}

	return 0;

error:
	kz_amqp_free_connection(newConn);
	return -1;

}

void kz_amqp_connection_close(kz_amqp_conn_ptr rmq) {
    LM_DBG("Close rmq connection\n");
    if (!rmq)
    	return;

    if(rmq->heartbeat)
        kz_amqp_timer_destroy(&rmq->heartbeat);

    kz_amqp_fire_connection_event("closed", rmq->server->connection->info.host, rmq->server->zone->zone);

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
    rmq->state = KZ_AMQP_CONNECTION_CLOSED;

}

void kz_amqp_channel_close(kz_amqp_conn_ptr rmq, amqp_channel_t channel) {
    LM_DBG("Close rmq channel\n");
    if (!rmq)
    	return;

	LM_DBG("close channel: %d rmq(%p)->channel(%d)\n", getpid(), (void *)rmq, channel);
	kz_amqp_error("closing channel", amqp_channel_close(rmq->conn, channel, AMQP_REPLY_SUCCESS));
}

int kz_ssl_initialized = 0;

int kz_amqp_connection_open_ssl(kz_amqp_conn_ptr rmq) {

    if(!kz_ssl_initialized) {
	    kz_ssl_initialized = 1;
	    amqp_set_initialize_ssl_library(1);
    }

    if (!(rmq->conn = amqp_new_connection())) {
	LM_ERR("Failed to create new AMQP connection\n");
	goto error;
    }

    rmq->socket = amqp_ssl_socket_new(rmq->conn);
    if (!rmq->socket) {
	LM_ERR("Failed to create SSL socket to AMQP broker\n");
	goto nosocket;
    }

    if (kz_amqps_ca_cert.s) {
      if (amqp_ssl_socket_set_cacert(rmq->socket, kz_amqps_ca_cert.s)) {
	LM_ERR("Failed to set CA certificate for amqps connection\n");
	goto nosocket;
      }
    }

    if (kz_amqps_cert.s && kz_amqps_key.s) {
      if (amqp_ssl_socket_set_key(rmq->socket, kz_amqps_cert.s, kz_amqps_key.s)) {
	LM_ERR("Failed to set client key/certificate for amqps connection\n");
	goto nosocket;
      }
    }

#if AMQP_VERSION_MAJOR == 0 && AMQP_VERSION_MINOR < 8
    amqp_ssl_socket_set_verify(rmq->socket, kz_amqps_verify_peer | kz_amqps_verify_hostname);
#else
    amqp_ssl_socket_set_verify_peer(rmq->socket, kz_amqps_verify_peer);
    amqp_ssl_socket_set_verify_hostname(rmq->socket, kz_amqps_verify_hostname);
#endif

    if (amqp_socket_open_noblock(rmq->socket, rmq->server->connection->info.host, rmq->server->connection->info.port, &kz_amqp_connect_timeout_tv)) {
	LM_ERR("Failed to open SSL socket to AMQP broker : %s : %i\n",
	rmq->server->connection->info.host, rmq->server->connection->info.port);
	goto nosocket;
    }

    if (kz_amqp_error("Logging in", amqp_login(rmq->conn,
					   rmq->server->connection->info.vhost,
					   0,
					   131072,
					   dbk_use_hearbeats,
					   AMQP_SASL_METHOD_PLAIN,
					   rmq->server->connection->info.user,
					   rmq->server->connection->info.password))) {

	LM_ERR("Login to AMQP broker failed!\n");
	goto error;
    }

	rmq->state = KZ_AMQP_CONNECTION_OPEN;
	return 0;

nosocket:
    if (amqp_destroy_connection(rmq->conn) < 0) {
	LM_ERR("cannot destroy connection\n");
    }

    rmq->conn = NULL;
    return -1;

 error:
    kz_amqp_connection_close(rmq);
    return -1;
}

int kz_amqp_connection_open(kz_amqp_conn_ptr rmq) {
	rmq->state = KZ_AMQP_CONNECTION_CLOSED;
	rmq->channel_count = rmq->channel_counter = 0;

	if(rmq->server->connection->info.ssl)
		return kz_amqp_connection_open_ssl(rmq);

    rmq->channel_count = rmq->channel_counter = 0;
    if (!(rmq->conn = amqp_new_connection())) {
    	LM_DBG("Failed to create new AMQP connection\n");
    	goto error;
    }

    rmq->socket = amqp_tcp_socket_new(rmq->conn);
    if (!rmq->socket) {
    	LM_DBG("Failed to create TCP socket to AMQP broker\n");
    	goto nosocket;
    }

    if (amqp_socket_open_noblock(rmq->socket, rmq->server->connection->info.host, rmq->server->connection->info.port, &kz_amqp_connect_timeout_tv)) {
    	LM_DBG("Failed to open TCP socket to AMQP broker\n");
    	goto nosocket;
    }

    if (kz_amqp_error("Logging in", amqp_login(rmq->conn,
					   rmq->server->connection->info.vhost,
					   0,
					   131072,
					   dbk_use_hearbeats,
					   AMQP_SASL_METHOD_PLAIN,
					   rmq->server->connection->info.user,
					   rmq->server->connection->info.password))) {

    	LM_ERR("Login to AMQP broker failed!\n");
    	goto error;
    }

	rmq->state = KZ_AMQP_CONNECTION_OPEN;
	return 0;

nosocket:
	if (amqp_destroy_connection(rmq->conn) < 0) {
		LM_ERR("cannot destroy connection\n");
	}
	rmq->conn = NULL;
	return -1;

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

int kz_amqp_consume_error(kz_amqp_conn_ptr ptr)
{
	amqp_connection_state_t conn = ptr->conn;
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
				ptr->server->channels[frame.channel-1].state = KZ_AMQP_CHANNEL_CLOSED;
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

    if(kz_json_get_object(json_obj, BLF_JSON_APP_NAME) == NULL)
    	json_object_object_add(json_obj, BLF_JSON_APP_NAME, json_object_new_string(kz_app_name.s));

    if(kz_json_get_object(json_obj, BLF_JSON_APP_VERSION) == NULL)
    	json_object_object_add(json_obj, BLF_JSON_APP_VERSION, json_object_new_string(VERSION));

    if(kz_json_get_object(json_obj, BLF_JSON_NODE) == NULL) {
        sprintf(node_name, "kamailio@%.*s", dbk_node_hostname.len, dbk_node_hostname.s);
        json_object_object_add(json_obj, BLF_JSON_NODE, json_object_new_string(node_name));
    }

    if(kz_json_get_object(json_obj, BLF_JSON_MSG_ID) == NULL)
    	json_object_object_add(json_obj, BLF_JSON_MSG_ID, json_object_new_string_len(unique->s, unique->len));

}

int kz_amqp_pipe_send(str *str_exchange, str *str_routing_key, str *str_payload)
{
	int ret = 1;
    json_obj_ptr json_obj = NULL;
    kz_amqp_cmd_ptr cmd = NULL;
	char* s_routing_key = NULL;
	char* s_local_routing_key = NULL;
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


	// check routing
	s_local_routing_key  = kz_local_amqp_str_dup(str_routing_key);
	s_routing_key = s_local_routing_key;
	if(!strncmp(s_routing_key, "consumer://", 11)) {
	    char* ptr_consumer = s_routing_key+11;
		char* ptr = strchr(ptr_consumer, '/');
		if(ptr == NULL) {
			LM_ERR("failed to get consumer key in process %d\n", getpid());
			goto error;
		}
		*ptr = '\0';
		ptr++;
		if(strlen(ptr_consumer) == 0) {
			LM_ERR("invalid length. failed to get consumer key in process %d\n", getpid());
			goto error;
		}
		cmd->server_id = atoi(ptr_consumer);
		s_routing_key = ptr;
	}


	cmd->exchange = kz_amqp_str_dup(str_exchange);
	cmd->routing_key = kz_amqp_string_dup(s_routing_key);
	cmd->payload = kz_amqp_string_dup(payload);
	if(cmd->payload == NULL || cmd->routing_key == NULL || cmd->exchange == NULL) {
		LM_ERR("failed to allocate kz_amqp_cmd parameters in process %d\n", getpid());
		goto error;
	}
	if(lock_init(&cmd->lock)==NULL)
	{
		LM_ERR("cannot init the lock for publishing in process %d\n", getpid());
		goto error;
	}
	lock_get(&cmd->lock);
	cmd->type = KZ_AMQP_CMD_PUBLISH;
	cmd->consumer = getpid();
	if (write(kz_cmd_pipe, &cmd, sizeof(cmd)) != sizeof(cmd)) {
		LM_ERR("failed to publish message to amqp in process %d, write to command pipe: %s\n", getpid(), strerror(errno));
	} else {
		lock_get(&cmd->lock);
		ret = cmd->return_code;
	}

	error:

	if(cmd)
		kz_amqp_free_pipe_cmd(cmd);

    if(json_obj)
    	json_object_put(json_obj);

    if(s_local_routing_key)
    	pkg_free(s_local_routing_key);

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
	cmd->message_id = kz_str_dup(&unique_string);

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
	cmd->type = KZ_AMQP_CMD_CALL;
	cmd->consumer = getpid();
	if (write(kz_cmd_pipe, &cmd, sizeof(cmd)) != sizeof(cmd)) {
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

		if (routing_key_s.len > MAX_ROUTING_KEY_SIZE) {
			LM_ERR("routing_key size (%d) > max %d\n", routing_key_s.len, MAX_ROUTING_KEY_SIZE);
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

int kz_amqp_async_query(struct sip_msg* msg, char* _exchange, char* _routing_key, char* _payload, char* _cb_route, char* _err_route)
{
	  str json_s;
	  str exchange_s;
	  str routing_key_s;
	  str cb_route_s;
	  str err_route_s;
	  struct timeval kz_timeout = kz_qtimeout_tv;
      int ret = -1;
      json_obj_ptr json_obj = NULL;
	  kz_amqp_cmd_ptr cmd = NULL;
	  unsigned int hash_index = 0;
	  unsigned int label = 0;
	  tm_cell_t *t = 0;

	  str unique_string = { 0, 0 };
	  char serverid[512];

	  uuid_t id;
	  char uuid_buffer[40];

	  if (fixup_get_svalue(msg, (gparam_p)_exchange, &exchange_s) != 0) {
		  LM_ERR("cannot get exchange string value\n");
		  goto error;
	  }

	  if (fixup_get_svalue(msg, (gparam_p)_routing_key, &routing_key_s) != 0) {
		  LM_ERR("cannot get routing_key string value\n");
		  goto error;
	  }

	  if (fixup_get_svalue(msg, (gparam_p)_payload, &json_s) != 0) {
		  LM_ERR("cannot get json string value : %s\n", _payload);
		  goto error;
	  }

	  if (routing_key_s.len > MAX_ROUTING_KEY_SIZE) {
		  LM_ERR("routing_key size (%d) > max %d\n", routing_key_s.len, MAX_ROUTING_KEY_SIZE);
		  return -1;
	  }

	  json_obj = json_tokener_parse(json_s.s);

	  if (is_error(json_obj)) {
		  LM_ERR("empty or invalid JSON payload : %*.s\n", json_s.len, json_s.s);
		  goto error;
	  }

	  if (fixup_get_svalue(msg, (gparam_p)_cb_route, &cb_route_s) != 0) {
		  LM_ERR("cannot get cb_route value\n");
		  return -1;
	  }

	  if (fixup_get_svalue(msg, (gparam_p)_err_route, &err_route_s) != 0) {
		  LM_ERR("cannot get err_route value\n");
		  return -1;
	  }

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

	  t = tmb.t_gett();
	  if (t==NULL || t==T_UNDEFINED) {
		  if(tmb.t_newtran(msg)<0) {
			  LM_ERR("cannot create the transaction\n");
			  goto error;
		  }
		  t = tmb.t_gett();
		  if (t==NULL || t==T_UNDEFINED) {
			  LM_ERR("cannot look up the transaction\n");
			  goto error;
		  }
	  }

	  if (tmb.t_suspend(msg, &hash_index, &label) < 0) {
		  LM_ERR("t_suspend() failed\n");
		  goto error;
	  }

	  uuid_generate_random(id);
	  uuid_unparse_lower(id, uuid_buffer);
	  unique_string.s = uuid_buffer;
	  unique_string.len = strlen(unique_string.s);
	  sprintf(serverid, "kamailio@%.*s-<%d>-script-%lu", dbk_node_hostname.len, dbk_node_hostname.s, my_pid(), rpl_query_routing_key_count++);
	  kz_amqp_add_payload_common_properties(json_obj, serverid, &unique_string);

		cmd = (kz_amqp_cmd_ptr)shm_malloc(sizeof(kz_amqp_cmd));
		if(cmd == NULL) {
			LM_ERR("failed to allocate kz_amqp_cmd in process %d\n", getpid());
			goto error;
		}
		memset(cmd, 0, sizeof(kz_amqp_cmd));
		cmd->exchange = kz_amqp_str_dup(&exchange_s);
		cmd->routing_key = kz_amqp_str_dup(&routing_key_s);
		cmd->reply_routing_key = kz_amqp_string_dup(serverid);
		cmd->payload = kz_amqp_string_dup((char *)json_object_to_json_string(json_obj));
		cmd->message_id = kz_str_dup(&unique_string);
		cmd->timeout = kz_timeout;
		cmd->cb_route = kz_amqp_str_dup(&cb_route_s);
		cmd->err_route = kz_amqp_str_dup(&err_route_s);
		cmd->t_hash = hash_index;
		cmd->t_label = label;

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
		cmd->type = KZ_AMQP_CMD_ASYNC_CALL;
		cmd->consumer = getpid();
		if (write(kz_cmd_pipe, &cmd, sizeof(cmd)) != sizeof(cmd)) {
			LM_ERR("failed to publish message to amqp in process %d, write to command pipe: %s\n", getpid(), strerror(errno));
			goto error;
		}
		ret = 0;
		goto exit;

error:
		if(cmd)
			kz_amqp_free_pipe_cmd(cmd);

		if(hash_index | label)
			tmb.t_cancel_suspend(hash_index, label);

exit:
	    if(json_obj)
	    	json_object_put(json_obj);

	    return ret;
};

void kz_amqp_reset_last_result()
{
	if(last_payload_result)
		pkg_free(last_payload_result);
	last_payload_result = NULL;
}

void kz_amqp_set_last_result(char* json)
{
	kz_amqp_reset_last_result();
	int len = strlen(json);
	char* value = pkg_malloc(len+1);
	memcpy(value, json, len);
	value[len] = '\0';
	last_payload_result = value;
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

		if (routing_key_s.len > MAX_ROUTING_KEY_SIZE) {
			LM_ERR("routing_key size (%d) > max %d\n", routing_key_s.len, MAX_ROUTING_KEY_SIZE);
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

void kz_amqp_queue_free(kz_amqp_queue_ptr queue)
{
	if(queue->name.bytes)
		kz_amqp_bytes_free(queue->name);

	shm_free(queue);
}

kz_amqp_queue_ptr kz_amqp_queue_new(str *name)
{
	kz_amqp_queue_ptr queue = (kz_amqp_queue_ptr) shm_malloc(sizeof(kz_amqp_queue));
	if(queue == NULL) {
		LM_ERR("NO MORE SHARED MEMORY!");
		return NULL;
	}
	memset(queue, 0, sizeof(kz_amqp_queue));
	queue->auto_delete = 1;

    if(name != NULL) {
    	queue->name = kz_amqp_bytes_dup_from_str(name);
		if (queue->name.bytes == NULL) {
			LM_ERR("Out of memory allocating for exchange\n");
			goto error;
		}
    }

    return queue;

error:
    	kz_amqp_queue_free(queue);
    	return NULL;
}

kz_amqp_queue_ptr kz_amqp_queue_from_json(str *name, json_object* json_obj)
{
	json_object* tmpObj = NULL;
	kz_amqp_queue_ptr queue = kz_amqp_queue_new(name);

	if(queue == NULL) {
		LM_ERR("NO MORE SHARED MEMORY!");
		return NULL;
	}

	tmpObj = kz_json_get_object(json_obj, "passive");
	if(tmpObj != NULL) {
		queue->passive = json_object_get_int(tmpObj);
	}

	tmpObj = kz_json_get_object(json_obj, "durable");
	if(tmpObj != NULL) {
		queue->durable = json_object_get_int(tmpObj);
	}

 	tmpObj = kz_json_get_object(json_obj, "exclusive");
	if(tmpObj != NULL) {
		queue->exclusive = json_object_get_int(tmpObj);
	}

	tmpObj = kz_json_get_object(json_obj, "auto_delete");
	if(tmpObj != NULL) {
		queue->auto_delete = json_object_get_int(tmpObj);
	}

	return queue;

}

void kz_amqp_exchange_free(kz_amqp_exchange_ptr exchange)
{
	if(exchange->name.bytes)
		kz_amqp_bytes_free(exchange->name);

	if(exchange->type.bytes)
		kz_amqp_bytes_free(exchange->type);

	shm_free(exchange);
}

kz_amqp_exchange_ptr kz_amqp_exchange_new(str *name, str* type)
{
	kz_amqp_exchange_ptr exchange = (kz_amqp_exchange_ptr) shm_malloc(sizeof(kz_amqp_exchange));
	if(exchange == NULL) {
		LM_ERR("NO MORE SHARED MEMORY!");
		return NULL;
	}
	memset(exchange, 0, sizeof(kz_amqp_exchange));
	exchange->name = kz_amqp_bytes_dup_from_str(name);
	if (exchange->name.bytes == NULL) {
		LM_ERR("Out of memory allocating for exchange\n");
		goto error;
	}

	exchange->type = kz_amqp_bytes_dup_from_str(type);
	if (exchange->type.bytes == NULL) {
		LM_ERR("Out of memory allocating for exchange type\n");
		goto error;
	}

	LM_DBG("NEW exchange %.*s : %.*s : %.*s : %.*s\n",
	        (int)name->len, (char*)name->s,
	        (int)type->len, (char*)type->s,
		(int)exchange->name.len, (char*)exchange->name.bytes,
		(int)exchange->type.len, (char*)exchange->type.bytes);

	return exchange;

error:

	kz_amqp_exchange_free(exchange);
	return NULL;
}

kz_amqp_exchange_ptr kz_amqp_exchange_from_json(str *name, json_object* json_obj)
{
	str type = STR_NULL;
	kz_amqp_exchange_ptr exchange = NULL;
	json_object* tmpObj = NULL;

	json_extract_field("type", type);

	LM_DBG("NEW JSON exchange %.*s : %.*s\n",
	        (int)name->len, (char*)name->s,
	        (int)type.len, (char*)type.s);


	exchange = kz_amqp_exchange_new(name, &type);
	if(exchange == NULL) {
		LM_ERR("NO MORE SHARED MEMORY!");
		return NULL;
	}

	tmpObj = kz_json_get_object(json_obj, "passive");
	if(tmpObj != NULL) {
		exchange->passive = json_object_get_int(tmpObj);
	}

	tmpObj = kz_json_get_object(json_obj, "durable");
	if(tmpObj != NULL) {
		exchange->durable = json_object_get_int(tmpObj);
	}

	tmpObj = kz_json_get_object(json_obj, "auto_delete");
	if(tmpObj != NULL) {
		exchange->auto_delete = json_object_get_int(tmpObj);
	}

	tmpObj = kz_json_get_object(json_obj, "internal");
	if(tmpObj != NULL) {
		exchange->internal = json_object_get_int(tmpObj);
	}

	return exchange;

}

void kz_amqp_routing_free(kz_amqp_routings_ptr routing)
{
	if(routing) {
		if(routing->next)
			kz_amqp_routing_free(routing->next);
		shm_free(routing);
	}
}

kz_amqp_routings_ptr kz_amqp_routing_new(char* routing)
{
	kz_amqp_routings_ptr ptr = (kz_amqp_routings_ptr) shm_malloc(sizeof(kz_amqp_routings));
	memset(ptr, 0, sizeof(kz_amqp_routings));

	ptr->routing = kz_amqp_bytes_dup_from_string(routing);
	return ptr;
}

kz_amqp_routings_ptr kz_amqp_routing_from_json(json_object* json_obj)
{
	kz_amqp_routings_ptr r, r1 = NULL, ret = NULL;
	char *routing;
	int len, n;

	if(json_obj == NULL)
    	return ret;

   	switch(kz_json_get_type(json_obj))
   	{
   	case json_type_string:
   		routing = (char*)json_object_get_string(json_obj);
		ret = kz_amqp_routing_new(routing);
		break;

   	case json_type_array:
		len = json_object_array_length(json_obj);
		for(n=0; n < len; n++) {
			routing = (char*)json_object_get_string(json_object_array_get_idx(json_obj, n));
			r = kz_amqp_routing_new(routing);
			if(r1 != NULL) {
				r1->next = r;
			} else {
				ret = r;
			}
			r1 = r;
		}
		break;

   	default:
   		LM_DBG("type not handled in routing");
   		break;
   	}
   	return ret;
}

void kz_amqp_exchange_bindings_free(kz_amqp_exchange_binding_ptr binding)
{
	if(binding) {
		if(binding->next)
			kz_amqp_exchange_bindings_free(binding->next);
		kz_amqp_exchange_free(binding->from_exchange);
		kz_amqp_routing_free(binding->routing);
		shm_free(binding);
	}

}

kz_amqp_exchange_binding_ptr kz_amqp_exchange_binding_from_json(json_object* JObj)
{
//	struct json_object* tmpObj = NULL;
	struct json_object* routingObj = NULL;
	kz_amqp_exchange_ptr exchange;
	kz_amqp_exchange_binding_ptr prv = NULL;
	kz_amqp_exchange_binding_ptr ret = NULL;
	if(JObj != NULL) {
		json_foreach(JObj, k, v) {
			str name = {k, strlen(k)};
			LM_DBG("exchange binding1 %s, %i , %s,  %i : %.*s\n", k, (int) strlen(k), name.s, name.len, name.len, name.s);
			exchange = kz_amqp_exchange_from_json(&name, v);
			LM_DBG("exchange binding2 %s, %i : %.*s\n", k, (int) strlen(k), name.len, name.s);
			LM_DBG("exchange binding3 %.*s : %.*s\n",
					        (int)exchange->name.len, (char*)exchange->name.bytes,
					        (int)exchange->type.len, (char*)exchange->type.bytes);

		    routingObj = kz_json_get_object(v, "routing");
		    if(routingObj != NULL) {
		    	kz_amqp_exchange_binding_ptr binding = (kz_amqp_exchange_binding_ptr) shm_malloc(sizeof(kz_amqp_exchange_binding));
		    	memset(binding, 0, sizeof(kz_amqp_exchange_binding));
		    	binding->from_exchange = exchange;
		    	binding->routing = kz_amqp_routing_from_json(routingObj);
		    	if(binding->routing == NULL) {
		    		LM_DBG("invalid routing");
		    		kz_amqp_exchange_bindings_free(binding);
		    		binding = NULL;
		    	} else {
			    	if(ret == NULL)
			    		ret = binding;
			    	if(prv != NULL)
			    		prv->next = binding;
		    	}
		    } else {
		    	kz_amqp_exchange_free(exchange);
		    }
		}
	}

	return ret;
}

int kz_amqp_subscribe(struct sip_msg* msg, char* payload)
{
	str exchange_s = STR_NULL;
	str queue_s = STR_NULL;
	str payload_s = STR_NULL;
	str key_s = STR_NULL;
	str subkey_s = STR_NULL;
	int no_ack = 1;
	int federate = 0;
	int consistent_worker = 0;
	str* consistent_worker_key = NULL;
	int wait_for_consumer_ack = 1;
	kz_amqp_queue_ptr queue = NULL;
	kz_amqp_exchange_ptr exchange = NULL;
	kz_amqp_exchange_binding_ptr exchange_binding = NULL;
	kz_amqp_routings_ptr routing = NULL;
	str* event_key = NULL;
    str* event_subkey = NULL;



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
	json_extract_field("queue", queue_s);
	json_extract_field("event_key", key_s);
	json_extract_field("event_subkey", subkey_s);

	if(key_s.len != 0)
		event_key = &key_s;

	if(subkey_s.len != 0)
		event_subkey = &subkey_s;

	tmpObj = kz_json_get_object(json_obj, "no_ack");
	if(tmpObj != NULL) {
		no_ack = json_object_get_int(tmpObj);
	}

	tmpObj = kz_json_get_object(json_obj, "wait_for_consumer_ack");
	if(tmpObj != NULL) {
		wait_for_consumer_ack = json_object_get_int(tmpObj);
	}

	tmpObj = kz_json_get_object(json_obj, "federate");
	if(tmpObj != NULL) {
		federate = json_object_get_int(tmpObj);
	}

	tmpObj = kz_json_get_object(json_obj, "consistent-worker");
	if(tmpObj != NULL) {
		consistent_worker = json_object_get_int(tmpObj);
	}

	tmpObj = kz_json_get_object(json_obj, "consistent-worker-key");
	if(tmpObj != NULL) {
		consistent_worker_key = kz_str_dup_from_char((char*)json_object_get_string(tmpObj));
	}

	tmpObj = kz_json_get_object(json_obj, "exchange-bindings");
	if(tmpObj != NULL) {
		exchange_binding = kz_amqp_exchange_binding_from_json(tmpObj);
	}

	tmpObj = kz_json_get_object(json_obj, "routing");
	if(tmpObj != NULL) {
		routing = kz_amqp_routing_from_json(tmpObj);
	}

	if(routing == NULL) {
		LM_INFO("creating empty routing key : %s\n", payload_s.s);
		routing = kz_amqp_routing_new("");
	}

	tmpObj = kz_json_get_object(json_obj, "exchange-def");
	if(tmpObj == NULL) {
		tmpObj = json_obj;
	}
	exchange = kz_amqp_exchange_from_json(&exchange_s, tmpObj);

	tmpObj = kz_json_get_object(json_obj, "queue-def");
	if(tmpObj == NULL) {
		tmpObj = json_obj;
	}
	queue = kz_amqp_queue_from_json(&queue_s, tmpObj);

	kz_amqp_bind_ptr bind = kz_amqp_bind_alloc(exchange, exchange_binding, queue, routing, event_key, event_subkey);
	if(bind == NULL) {
		LM_ERR("Could not allocate bind struct\n");
		goto error;
	}

	bind->no_ack = no_ack;
	bind->wait_for_consumer_ack = wait_for_consumer_ack;
	bind->federate = federate;
	bind->consistent_worker = consistent_worker;
	bind->consistent_worker_key = consistent_worker_key;


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

int kz_amqp_subscribe_simple(struct sip_msg* msg, char* exchange, char* exchange_type, char* queue, char* routing_key)
{
	str exchange_s;
	str exchange_type_s;
	str queue_s;
	str routing_key_s;
	kz_amqp_exchange_ptr exchange_ptr = NULL;
	kz_amqp_queue_ptr queue_ptr = NULL;
	kz_amqp_routings_ptr routing_ptr = NULL;


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

	exchange_ptr = kz_amqp_exchange_new(&exchange_s, &exchange_type_s);
	queue_ptr = kz_amqp_queue_new(&queue_s);
	routing_ptr = kz_amqp_routing_new(routing_key_s.s);

	kz_amqp_bind_ptr bind = kz_amqp_bind_alloc(exchange_ptr, NULL, queue_ptr, routing_ptr, NULL, NULL);
	if(bind == NULL) {
		LM_ERR("Could not allocate bind struct\n");
		goto error;
	}

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



#define KEY_SAFE(C)  ((C >= 'a' && C <= 'z') || \
                      (C >= 'A' && C <= 'Z') || \
                      (C >= '0' && C <= '9') || \
                      (C == '-' || C == '~'  || C == '_'))

#define HI4(C) (C>>4)
#define LO4(C) (C & 0x0F)

#define hexint(C) (C < 10?('0' + C):('A'+ C - 10))

void kz_amqp_util_encode(const str * key, char *pdest) {
    char *p, *end;
	char *dest = pdest;
    if ((key->len == 1) && (key->s[0] == '#' || key->s[0] == '*')) {
    	*dest++ = key->s[0];
    	return;
    }
    for (p = key->s, end = key->s + key->len; p < end && ((dest - pdest) < MAX_ROUTING_KEY_SIZE); p++) {
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
}

int kz_amqp_encode_ex(str* unencoded, pv_value_p dst_val)
{
	char routing_key_buff[MAX_ROUTING_KEY_SIZE+1];
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

	if (unencoded_s.len > MAX_ROUTING_KEY_SIZE) {
		LM_ERR("routing_key size (%d) > max %d\n", unencoded_s.len, MAX_ROUTING_KEY_SIZE);
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

int get_channel_index(kz_amqp_server_ptr server) {
	int n;
	for(n=server->channel_index; n < dbk_channels; n++)
		if(server->channels[n].state == KZ_AMQP_CHANNEL_FREE) {
			server->channel_index = n+1;
			return n;
		}
	if(server->channel_index == 0) {
		LM_ERR("max channels (%d) reached. please exit kamailio and change kazoo amqp_max_channels param", dbk_channels);
		return -1;
	}
	server->channel_index = 0;
	return get_channel_index(server);
}

int kz_amqp_bind_targeted_channel(kz_amqp_conn_ptr kz_conn, int idx )
{
    kz_amqp_bind_ptr bind = kz_conn->server->channels[idx].targeted;
    int ret = -1;

	kz_amqp_exchange_declare(kz_conn->conn, kz_conn->server->channels[idx].channel, bind->exchange, kz_amqp_empty_table);
    if (kz_amqp_error("Declaring targeted exchange", amqp_get_rpc_reply(kz_conn->conn)))
    {
		ret = -RET_AMQP_ERROR;
		goto error;
    }

    kz_amqp_queue_declare(kz_conn->conn, kz_conn->server->channels[idx].channel, bind->queue, kz_amqp_empty_table);
    if (kz_amqp_error("Declaring targeted queue", amqp_get_rpc_reply(kz_conn->conn)))
    {
		goto error;
    }

    if (kz_amqp_queue_bind(kz_conn->conn, kz_conn->server->channels[idx].channel, bind->exchange, bind->queue, bind->queue_bindings, kz_amqp_empty_table) < 0
	    || kz_amqp_error("Binding targeted queue", amqp_get_rpc_reply(kz_conn->conn)))
    {
		goto error;
    }

    if (amqp_basic_consume(kz_conn->conn, kz_conn->server->channels[idx].channel, bind->queue->name, kz_amqp_empty_bytes, 0, 1, 0, kz_amqp_empty_table) < 0
	    || kz_amqp_error("Consuming targeted queue", amqp_get_rpc_reply(kz_conn->conn)))
    {
		goto error;
    }

    return 0;
 error:
    return ret;
}

int kz_amqp_bind_exchange(kz_amqp_conn_ptr kz_conn, amqp_channel_t channel, kz_amqp_exchange_ptr exchange, kz_amqp_exchange_binding_ptr bindings)
{
	while(bindings != NULL) {
		LM_DBG("DECLARE EXH BIND FOR %.*s\n", (int)exchange->name.len, (char*)exchange->name.bytes);
		LM_DBG("DECLARE EXH BIND TO %.*s\n", (int)bindings->from_exchange->name.len, (char*)bindings->from_exchange->name.bytes);

		kz_amqp_exchange_declare(kz_conn->conn, channel, bindings->from_exchange, kz_amqp_empty_table);
		if (kz_amqp_error("Declaring binded exchange", amqp_get_rpc_reply(kz_conn->conn)))
			return -RET_AMQP_ERROR;

		kz_amqp_routings_ptr routings = bindings->routing;
		while(routings) {
			if (amqp_exchange_bind(kz_conn->conn, channel, exchange->name, bindings->from_exchange->name, routings->routing, kz_amqp_empty_table) < 0
				|| kz_amqp_error("Binding exchange", amqp_get_rpc_reply(kz_conn->conn)))
				return -RET_AMQP_ERROR;
			routings = routings->next;
		}
		bindings = bindings->next;
	}
	return 0;

}

int kz_amqp_bind_consumer(kz_amqp_conn_ptr kz_conn, kz_amqp_bind_ptr bind, int idx, kz_amqp_channel_ptr chan)
{
    int ret = -1;

    LM_DBG("BINDING CONSUMER %i TO %.*s\n", idx,  (int)bind->exchange->name.len,  (char*)bind->exchange->name.bytes);
	kz_amqp_exchange_declare(kz_conn->conn, chan[idx].channel, bind->exchange, kz_amqp_empty_table);
	if (kz_amqp_error("Declaring consumer exchange", amqp_get_rpc_reply(kz_conn->conn)))
	{
		ret = -RET_AMQP_ERROR;
		goto error;
	}

	if((ret =  kz_amqp_bind_exchange(kz_conn, chan[idx].channel, bind->exchange, bind->exchange_bindings)) != 0)
		goto error;

	kz_amqp_queue_declare(kz_conn->conn, chan[idx].channel, bind->queue, kz_amqp_empty_table);
	if (kz_amqp_error("Declaring consumer queue", amqp_get_rpc_reply(kz_conn->conn)))
	{
		ret = -RET_AMQP_ERROR;
		goto error;
	}

	if (kz_amqp_queue_bind(kz_conn->conn, chan[idx].channel, bind->exchange, bind->queue, bind->queue_bindings, kz_amqp_empty_table) < 0
		|| kz_amqp_error("Binding consumer queue", amqp_get_rpc_reply(kz_conn->conn)))
	{
		ret = -RET_AMQP_ERROR;
		goto error;
    }

    if (amqp_basic_consume(kz_conn->conn, chan[idx].channel, bind->queue->name, kz_amqp_empty_bytes, 0, bind->no_ack, 0, kz_amqp_empty_table) < 0
	    || kz_amqp_error("Consuming", amqp_get_rpc_reply(kz_conn->conn)))
    {
		ret = -RET_AMQP_ERROR;
		goto error;
    }

    chan[idx].state = KZ_AMQP_CHANNEL_CONSUMING;
	chan[idx].consumer = bind;
    ret = idx;
 error:
     return ret;
}

int kz_amqp_send_ex(kz_amqp_server_ptr srv, kz_amqp_cmd_ptr cmd, kz_amqp_channel_state state, int idx)
{
	amqp_bytes_t exchange = {0,0};
	amqp_bytes_t routing_key = {0,0};
	amqp_bytes_t payload = {0,0};
	int ret = -1;
    json_obj_ptr json_obj = NULL;

	amqp_basic_properties_t props;
	memset(&props, 0, sizeof(amqp_basic_properties_t));
	props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG;
	props.content_type = amqp_cstring_bytes("application/json");

	if(idx == -1) {
		idx = get_channel_index(srv);
		if(idx == -1) {
			LM_ERR("Failed to get channel index to publish\n");
			goto error;
		}
	}

    exchange = amqp_bytes_malloc_dup(amqp_cstring_bytes(cmd->exchange));
    routing_key = amqp_bytes_malloc_dup(amqp_cstring_bytes(cmd->routing_key));
    payload = amqp_bytes_malloc_dup(amqp_cstring_bytes(cmd->payload));

    json_obj = kz_json_parse(cmd->payload);
    if (json_obj == NULL) {
	    LM_ERR("error parsing json when publishing %s\n", cmd->payload);
	    goto error;
    }

    if(kz_json_get_object(json_obj, BLF_JSON_SERVERID) == NULL) {
        json_object_object_add(json_obj, BLF_JSON_SERVERID, json_object_new_string((char*)srv->channels[idx].targeted->queue_bindings->routing.bytes));
    	amqp_bytes_free(payload);
        payload = amqp_bytes_malloc_dup(amqp_cstring_bytes((char*)json_object_to_json_string(json_obj)));
    }

	int amqpres = amqp_basic_publish(srv->producer->conn, srv->channels[idx].channel, exchange, routing_key, 0, 0, &props, payload);
	if ( amqpres != AMQP_STATUS_OK ) {
		LM_ERR("Failed to publish %i : %s\n", amqpres, amqp_error_string2(amqpres));
		ret = -1;
		goto error;
	}

	if ( kz_amqp_error("Publishing",  amqp_get_rpc_reply(srv->producer->conn)) ) {
		LM_ERR("Failed to publish\n");
		ret = -1;
		goto error;
	}
	gettimeofday(&srv->channels[idx].timer, NULL);
	srv->channels[idx].state = state;
	srv->channels[idx].cmd = cmd;

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

int kz_amqp_send(kz_amqp_server_ptr srv, kz_amqp_cmd_ptr cmd)
{
	return kz_amqp_send_ex(srv, cmd, KZ_AMQP_CHANNEL_PUBLISHING , -1);
}


int kz_amqp_send_receive_ex(kz_amqp_server_ptr srv, kz_amqp_cmd_ptr cmd, int idx )
{
	return kz_amqp_send_ex(srv, cmd, KZ_AMQP_CHANNEL_CALLING, idx);
}

int kz_amqp_send_receive(kz_amqp_server_ptr srv, kz_amqp_cmd_ptr cmd )
{
	return kz_amqp_send_receive_ex(srv, cmd, -1 );
}

char* eventData = NULL;
char* eventKey = NULL;

int kz_pv_get_event_payload(struct sip_msg *msg, pv_param_t *param,	pv_value_t *res)
{
	return eventData == NULL ? pv_get_null(msg, param, res) : pv_get_strzval(msg, param, res, eventData);
}

int kz_pv_get_event_routing_key(struct sip_msg *msg, pv_param_t *param,	pv_value_t *res)
{
	return eventKey == NULL ? pv_get_null(msg, param, res) : pv_get_strzval(msg, param, res, eventKey);
}

int kz_amqp_consumer_fire_event(char *eventkey)
{
	sip_msg_t *fmsg;
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
	fmsg = faked_msg_get_next();
	rtb = get_route_type();
	set_route_type(REQUEST_ROUTE);
	if (exec_pre_script_cb(fmsg, REQUEST_CB_TYPE)!=0 ) {
		init_run_actions_ctx(&ctx);
		run_top_route(event_rt.rlist[rt], fmsg, 0);
		exec_post_script_cb(fmsg, REQUEST_CB_TYPE);
		ksr_msg_env_reset();
	}
	set_route_type(rtb);
	return 0;
}

void kz_amqp_consumer_event(kz_amqp_consumer_delivery_ptr Evt)
{
    json_obj_ptr json_obj = NULL;
    str ev_name = {0, 0}, ev_category = {0, 0};
    char buffer[512];
    char * p;

    eventData = Evt->payload;
    if(Evt->routing_key) {
    	eventKey = Evt->routing_key->s;
    }

    json_obj = kz_json_parse(Evt->payload);
    if (json_obj == NULL)
		return;

    char* key = (Evt->event_key == NULL ? dbk_consumer_event_key.s : Evt->event_key);
    char* subkey = (Evt->event_subkey == NULL ? dbk_consumer_event_subkey.s : Evt->event_subkey);

    json_extract_field(key, ev_category);
    if(ev_category.len == 0 && Evt->event_key) {
	    ev_category.s = Evt->event_key;
	    ev_category.len = strlen(Evt->event_key);
    }

    json_extract_field(subkey, ev_name);
    if(ev_name.len == 0 && Evt->event_subkey) {
	    ev_name.s = Evt->event_subkey;
	    ev_name.len = strlen(Evt->event_subkey);
    }

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
	eventKey = NULL;
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

int consumer = 0;

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
	if (write(kz_worker_pipes[consumer], &ptr, sizeof(ptr)) != sizeof(ptr)) {
		LM_ERR("failed to send payload to consumer %d : %s\nPayload %s\n", consumer, strerror(errno), payload);
	}

	if(nextConsumer) {
		consumer++;
		if(consumer >= dbk_consumer_workers) {
			consumer = 0;
		}
	}
}

void kz_amqp_send_consumer_event(char* payload, int nextConsumer)
{
	kz_amqp_send_consumer_event_ex(payload, NULL, NULL, 0, 0, nextConsumer);
}

void kz_amqp_fire_connection_event(char *event, char* host, char* zone)
{
	char* payload = (char*)shm_malloc(512);
	sprintf(payload, "{ \"%.*s\" : \"connection\", \"%.*s\" : \"%s\", \"host\" : \"%s\", \"zone\" : \"%s\" }",
			dbk_consumer_event_key.len, dbk_consumer_event_key.s,
			dbk_consumer_event_subkey.len, dbk_consumer_event_subkey.s,
			event, host, zone
			);
	kz_amqp_send_consumer_event(payload, 1);
}

void kz_amqp_cb_ok(kz_amqp_cmd_ptr cmd)
{
	int n = route_lookup(&main_rt, cmd->cb_route);
	if(n==-1) {
		/* route block not found in the configuration file */
		return;
	}
	struct action *a = main_rt.rlist[n];
	tmb.t_continue(cmd->t_hash, cmd->t_label, a);
	ksr_msg_env_reset();
}

void kz_amqp_cb_error(kz_amqp_cmd_ptr cmd)
{
	int n = route_lookup(&main_rt, cmd->err_route);
	if(n==-1) {
		/* route block not found in the configuration file */
		return;
	}
	struct action *a = main_rt.rlist[n];
	tmb.t_continue(cmd->t_hash, cmd->t_label, a);
	ksr_msg_env_reset();
}

int kz_send_worker_error_event(kz_amqp_cmd_ptr cmd)
{
	cmd->return_code = -1;
	kz_amqp_consumer_delivery_ptr ptr = (kz_amqp_consumer_delivery_ptr) shm_malloc(sizeof(kz_amqp_consumer_delivery));
	if(ptr == NULL) {
		LM_ERR("NO MORE SHARED MEMORY!");
		return 0;
	}
	memset(ptr, 0, sizeof(kz_amqp_consumer_delivery));
	ptr->cmd = cmd;

	consumer++;
	if(consumer >= dbk_consumer_workers) {
		consumer = 0;
	}

	if (write(kz_worker_pipes[consumer], &ptr, sizeof(ptr)) != sizeof(ptr)) {
		LM_ERR("failed to send payload to consumer %d : %s\nPayload %s\n", consumer, strerror(errno), cmd->payload);
		kz_amqp_free_consumer_delivery(ptr);
		return 0;
	}

	return 1;

}

void kz_amqp_cmd_timeout_cb(int fd, short event, void *arg)
{
	kz_amqp_cmd_timeout_ptr cmd = (kz_amqp_cmd_timeout_ptr) arg;
	kz_amqp_cmd_ptr retrieved_cmd = kz_cmd_retrieve(cmd->message_id);
	if(retrieved_cmd != NULL) {
		LM_DBG("amqp message timeout for exchange '%s' with routing key '%s' and message id '%.*s'\n"
				, retrieved_cmd ->exchange, retrieved_cmd ->routing_key
				, retrieved_cmd ->message_id->len, retrieved_cmd ->message_id->s
				);
		if(retrieved_cmd->type == KZ_AMQP_CMD_ASYNC_CALL) {
			kz_send_worker_error_event(retrieved_cmd);
		} else {
			retrieved_cmd->return_code = -1;
			lock_release(&retrieved_cmd->lock);
		}
	}
	close(cmd->timerfd);
	event_del(cmd->timer_ev);
	pkg_free(cmd->timer_ev);
	pkg_free(cmd->message_id);
	pkg_free(cmd);

}


int kz_amqp_start_cmd_timer(kz_amqp_cmd_ptr cmd)
{
	kz_amqp_cmd_timeout_ptr timeout_cmd = pkg_malloc(sizeof(kz_amqp_cmd_timeout));
	if(timeout_cmd == NULL) {
		LM_ERR("Could not allocate memory for kz_amqp_cmd_timeout_ptr\n");
		goto error;
	}

	timeout_cmd->message_id = kz_local_str_dup(cmd->message_id);
	if(timeout_cmd->message_id == NULL) {
		LM_ERR("Could not allocate memory for kz_amqp_cmd_timeout_ptr message_id\n");
		goto error;
	}

	int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if (timerfd == -1) {
		LM_ERR("Could not create timerfd\n");
		goto error;
	}

	timeout_cmd->timerfd = timerfd;
	struct itimerspec *itime = pkg_malloc(sizeof(struct itimerspec));
	if(itime == NULL){
		LM_ERR("Could not set timer\n");
		goto error;
	}
	itime->it_interval.tv_sec = 0;
	itime->it_interval.tv_nsec = 0;

	itime->it_value.tv_sec = cmd->timeout.tv_sec;
	itime->it_value.tv_nsec = cmd->timeout.tv_usec * 1000;
	if (timerfd_settime(timerfd, 0, itime, NULL) == -1) {
		LM_ERR("Could not set timer\n");
		goto error;
	}
	pkg_free(itime);
	struct event *timer_ev = pkg_malloc(sizeof(struct event));
	if(timer_ev == NULL) {
		LM_ERR("Could not allocate timer_ev\n");
		goto error;
	}
	event_set(timer_ev, timerfd, EV_READ, kz_amqp_cmd_timeout_cb, timeout_cmd);
	if(event_add(timer_ev, NULL) == -1) {
		LM_ERR("event_add failed while setting request timer (%s)\n", strerror(errno));
		pkg_free(timer_ev);
		goto error;
	}
	timeout_cmd->timer_ev = timer_ev;
	return 1;

error:
	if(timeout_cmd) {
		if(timeout_cmd->message_id)
			pkg_free(timeout_cmd->message_id);
		pkg_free(timeout_cmd);
	}
	return 0;
}


/* check timeouts */
int kz_amqp_timeout_proc()
{
	kz_amqp_cmd_ptr cmd;
	kz_amqp_zone_ptr g = NULL;
	kz_amqp_server_ptr s = NULL;
	int i;
    while(1) {
		struct timeval now;
		usleep(kz_timer_tv.tv_usec);
		for (g = kz_amqp_get_zones(); g != NULL; g = g->next) {
			for (s = g->servers->head; s != NULL; s = s->next) {
				for(i=0; i < dbk_channels; i++) {
					gettimeofday(&now, NULL);
					if(s->channels[i].state == KZ_AMQP_CHANNEL_CALLING
							&& s->channels[i].cmd != NULL
							&& check_timeout(&now, &s->channels[i].timer, &s->channels[i].cmd->timeout)) {
						lock_get(&s->channels[i].lock);
						if(s->channels[i].cmd != NULL)
						{
							cmd = s->channels[i].cmd;
							LM_DBG("Kazoo Query timeout - %s\n", cmd->payload);
							cmd->return_code = -1;
							lock_release(&cmd->lock);
							s->channels[i].cmd = NULL;
							s->channels[i].state = KZ_AMQP_CHANNEL_FREE;
						}
						lock_release(&s->channels[i].lock);
					}
				}
			}
		}
	}
    return 0;
}

int kz_amqp_connect(kz_amqp_conn_ptr rmq)
{
	int i,channel_res;
	kz_amqp_cmd_ptr cmd;
	if(rmq->state != KZ_AMQP_CONNECTION_CLOSED) {
		kz_amqp_connection_close(rmq);
	}

	if(kz_amqp_connection_open(rmq) != 0)
		goto error;

	kz_amqp_fire_connection_event("open", rmq->server->connection->info.host, rmq->server->zone->zone);
	for(i=0,channel_res=0; i < dbk_channels && channel_res == 0; i++) {
			/* start cleanup */
			rmq->server->channels[i].state = KZ_AMQP_CHANNEL_CLOSED;
			cmd = rmq->server->channels[i].cmd;
			if(cmd != NULL) {
				rmq->server->channels[i].cmd = NULL;
				cmd->return_code = -1;
				lock_release(&cmd->lock);
			}
			/* end cleanup */

			/* bind targeted channels */
			channel_res = kz_amqp_channel_open(rmq, rmq->server->channels[i].channel);
			if(channel_res == 0) {
				rmq->server->channels[i].state = KZ_AMQP_CHANNEL_FREE;
			}
    	}

	if(dbk_use_hearbeats > 0) {
		if(kz_amqp_timer_create(&rmq->heartbeat, dbk_use_hearbeats, kz_amqp_heartbeat_proc, rmq) != 0) {
			LM_ERR("could not schedule heartbeats for the connection\n");
		}
	}

	return 0;

error:
	kz_amqp_handle_server_failure(rmq);
	return -1;

}

void kz_amqp_reconnect_cb(int fd, short event, void *arg)
{
	LM_DBG("attempting to reconnect now.\n");
	kz_amqp_conn_ptr connection = (kz_amqp_conn_ptr)arg;

	kz_amqp_timer_destroy(&connection->reconnect);

	if (connection->state == KZ_AMQP_CONNECTION_OPEN) {
		LM_WARN("trying to connect an already connected server.\n");
		return;
	}

	kz_amqp_connect(connection);
}

int kz_amqp_handle_server_failure(kz_amqp_conn_ptr connection)
{
	int res = 0;

	if(connection->state != KZ_AMQP_CONNECTION_CLOSED)
		connection->state = KZ_AMQP_CONNECTION_FAILURE;

	if((res = kz_amqp_timer_create(&connection->reconnect, 5, kz_amqp_reconnect_cb, connection)) != 0) {
		LM_ERR("could not reschedule connection. No further attempts will be made to reconnect this server.\n");
	}
	return res;
}

int kz_amqp_publisher_send(kz_amqp_cmd_ptr cmd)
{
    int idx;
	int sent = 0;
	kz_amqp_zone_ptr g;
	kz_amqp_server_ptr s;
	kz_amqp_zone_ptr primary = kz_amqp_get_primary_zone();
	for (g = kz_amqp_get_zones(); g != NULL && sent == 0; g = g->next) {
		for (s = g->servers->head; s != NULL && sent == 0; s = s->next) {
			if(cmd->server_id == s->id || (cmd->server_id == 0 && g == primary)) {
				if(s->producer->state == KZ_AMQP_CONNECTION_OPEN) {
					if(cmd->type == KZ_AMQP_CMD_PUBLISH
							|| cmd->type == KZ_AMQP_CMD_PUBLISH_BROADCAST
							|| cmd->type == KZ_AMQP_CMD_ASYNC_CALL)
					{
						idx = kz_amqp_send(s, cmd);
						if(idx >= 0) {
							cmd->return_code = AMQP_RESPONSE_NORMAL;
							s->channels[idx].state = KZ_AMQP_CHANNEL_FREE;
							sent = 1;
						} else {
							cmd->return_code = -1;
							s->channels[idx].state = KZ_AMQP_CHANNEL_CLOSED;
							LM_ERR("error sending publish to zone : %s , connection id : %d, uri : %s", s->zone->zone, s->id, s->connection->url);
							kz_amqp_handle_server_failure(s->producer);
						}
						s->channels[idx].cmd = NULL;
					} else if(cmd->type == KZ_AMQP_CMD_CALL) {
						idx = kz_amqp_send_receive(s, cmd);
						if(idx < 0) {
							s->channels[idx].cmd = NULL;
							cmd->return_code = -1;
							s->channels[idx].state = KZ_AMQP_CHANNEL_CLOSED;
							LM_ERR("error sending query to zone : %s , connection id : %d, uri : %s", s->zone->zone, s->id, s->connection->url);
							kz_amqp_handle_server_failure(s->producer);
						} else {
							s->channels[idx].state = KZ_AMQP_CHANNEL_FREE;
							sent = 1;
						}
					}
				}
			}
		}
		if(cmd->type == KZ_AMQP_CMD_PUBLISH_BROADCAST) {
			sent = 0;
		}
	}
	return sent;
}


void kz_amqp_publisher_connect()
{
	kz_amqp_zone_ptr g;
	kz_amqp_server_ptr s;
	for (g = kz_amqp_get_zones(); g != NULL; g = g->next) {
		for (s = g->servers->head; s != NULL; s = s->next) {
			if(s->producer == NULL) {
				s->producer = (kz_amqp_conn_ptr) shm_malloc(sizeof(kz_amqp_conn));
				memset(s->producer, 0, sizeof(kz_amqp_conn));
				s->producer->server = s;
			}
			kz_amqp_connect(s->producer);
		}
	}
}

void kz_amqp_publisher_proc_cb(int fd, short event, void *arg)
{
	kz_amqp_cmd_ptr cmd;
	kz_amqp_cmd_ptr retrieved_cmd;
	if (read(fd, &cmd, sizeof(cmd)) != sizeof(cmd)) {
		LM_ERR("failed to read from command pipe: %s\n", strerror(errno));
		return;
	}

	switch(cmd->type) {
	case KZ_AMQP_CMD_PUBLISH:
		kz_amqp_publisher_send(cmd);
		lock_release(&cmd->lock);
		break;

	case KZ_AMQP_CMD_CALL:
		if(kz_amqp_publisher_send(cmd) < 0) {
			lock_release(&cmd->lock);
		} else {
			if(!kz_cmd_store(cmd)) {
				cmd->return_code = -1;
				lock_release(&cmd->lock);
			} else {
				if(!kz_amqp_start_cmd_timer(cmd)) {
					cmd->return_code = -1;
					lock_release(&cmd->lock);
				}
			}
		}
		break;

	case KZ_AMQP_CMD_TARGETED_CONSUMER:
		retrieved_cmd = kz_cmd_retrieve(cmd->message_id);
		if(retrieved_cmd == NULL) {
			LM_DBG("amqp message id %.*s not found.\n", cmd->message_id->len, cmd->message_id->s);
			kz_amqp_free_pipe_cmd(cmd);
		} else {
			retrieved_cmd->return_code = cmd->return_code;
			retrieved_cmd->return_payload = cmd->return_payload;
			cmd->return_payload = NULL;
			lock_release(&retrieved_cmd->lock);
			kz_amqp_free_pipe_cmd(cmd);
		}
		break;

	case KZ_AMQP_CMD_PUBLISH_BROADCAST:
		kz_amqp_publisher_send(cmd);
		lock_release(&cmd->lock);
		break;

	case KZ_AMQP_CMD_ASYNC_CALL:
		if(kz_amqp_publisher_send(cmd) < 0) {
			kz_amqp_cb_error(cmd);
		} else {
			if(!kz_cmd_store(cmd)) {
				kz_amqp_cb_error(cmd);
			} else {
				if(!kz_amqp_start_cmd_timer(cmd)) {
					kz_amqp_cb_error(cmd);
				}
			}
		}
		break;

	case KZ_AMQP_CMD_COLLECT:
		break;

	case KZ_AMQP_CMD_ASYNC_COLLECT:
		break;

	default:
		break;

	}
}

int kz_amqp_publisher_proc(int cmd_pipe)
{
	event_init();
	struct event pipe_ev;
	set_non_blocking(cmd_pipe);
	event_set(&pipe_ev, cmd_pipe, EV_READ | EV_PERSIST, kz_amqp_publisher_proc_cb, &pipe_ev);
	event_add(&pipe_ev, NULL);

	kz_amqp_publisher_connect();

	event_dispatch();
	return 0;
}

char* maybe_add_consumer_key(int server_id, amqp_bytes_t body)
{
	char* payload = kz_amqp_bytes_dup(body);
    json_obj_ptr json_obj = kz_json_parse(payload );
    if (json_obj == NULL) {
        return payload ;
    }

	json_object* server_id_obj = kz_json_get_object(json_obj, BLF_JSON_SERVERID);
    if(server_id_obj == NULL) {
    	return payload;
    }
    char buffer[100];
    const char* server_id_str = json_object_get_string(server_id_obj);
    if(server_id_str && strlen(server_id_str) > 0) {
    	sprintf(buffer, "consumer://%d/%s", server_id, server_id_str);
        json_object_object_del(json_obj, BLF_JSON_SERVERID);
    	json_object_object_add(json_obj, BLF_JSON_SERVERID, json_object_new_string(buffer));
    } else {
        json_object_object_del(json_obj, BLF_JSON_SERVERID);
    }
    shm_free(payload);
    payload = kz_amqp_bytes_dup(amqp_cstring_bytes((char*)json_object_to_json_string(json_obj)));
   	json_object_put(json_obj);
    return payload;
}

void kz_send_targeted_cmd(int server_id, amqp_bytes_t body)
{
    char buffer[100];
    char* server_id_str = NULL;
    kz_amqp_cmd_ptr cmd = NULL;
    json_object* JObj = NULL;
	char* payload = kz_local_amqp_bytes_dup(body);

	if(payload == NULL) {
		LM_ERR("error allocating message payload\n");
		goto error;
	}

	json_obj_ptr json_obj = kz_json_parse(payload );
    if (json_obj == NULL) {
		LM_ERR("error parsing json payload\n");
		goto error;
    }

	cmd = (kz_amqp_cmd_ptr)shm_malloc(sizeof(kz_amqp_cmd));
	if(cmd == NULL) {
		LM_ERR("failed to allocate kz_amqp_cmd in process %d\n", getpid());
		goto error;
	}
	memset(cmd, 0, sizeof(kz_amqp_cmd));
	if(lock_init(&cmd->lock)==NULL)
	{
		LM_ERR("cannot init the lock for targeted delivery in process %d\n", getpid());
		goto error;
	}

	cmd->type = KZ_AMQP_CMD_TARGETED_CONSUMER;
	cmd->return_code = AMQP_RESPONSE_NORMAL;

    JObj = kz_json_get_object(json_obj, BLF_JSON_SERVERID);
    if(JObj != NULL) {
    	server_id_str = (char*) json_object_get_string(JObj);
        if(server_id_str && strlen(server_id_str) > 0) {
        	sprintf(buffer, "consumer://%d/%s", server_id, server_id_str);
            json_object_object_del(json_obj, BLF_JSON_SERVERID);
        	json_object_object_add(json_obj, BLF_JSON_SERVERID, json_object_new_string(buffer));
        } else {
            json_object_object_del(json_obj, BLF_JSON_SERVERID);
        }
    }

    cmd->return_payload = kz_amqp_string_dup((char*)json_object_to_json_string(json_obj));

    JObj = kz_json_get_object(json_obj, BLF_JSON_MSG_ID);
    if(JObj != NULL) {
    	cmd->message_id = kz_str_dup_from_char((char*)json_object_get_string(JObj));
    }

	if (write(kz_cmd_pipe, &cmd, sizeof(cmd)) != sizeof(cmd)) {
		LM_ERR("failed to publish message to amqp in process %d, write to command pipe: %s\n", getpid(), strerror(errno));
	} else {
		cmd = NULL;
	}

error:
	if(json_obj)
		json_object_put(json_obj);

    if(payload)
    	pkg_free(payload);

    if(cmd)
    	kz_amqp_free_pipe_cmd(cmd);

}

void kz_amqp_send_worker_event(kz_amqp_server_ptr server_ptr, amqp_envelope_t* envelope, kz_amqp_bind_ptr bind)
{
    char buffer[100];
    kz_amqp_cmd_ptr cmd = NULL;
    kz_amqp_consumer_delivery_ptr ptr = NULL;
    json_obj_ptr json_obj = NULL;
    json_object* JObj = NULL;
    str* message_id = NULL;
    int idx = envelope->channel-1;
    int worker = 0;
    int _kz_server_id = server_ptr->id;
    int msg_size = envelope->message.body.len;
    char *json_data = pkg_malloc(msg_size + 1);
    if(!json_data) {
    	LM_ERR("no more package memory available. needed %d\n", msg_size + 1);
    	return;
    }
    memset(json_data, 0, msg_size + 1);
    memcpy(json_data, (char*)envelope->message.body.bytes, msg_size);
    json_obj = kz_json_parse(json_data);
    pkg_free(json_data);
    if (json_obj == NULL) {
    	LM_ERR("error parsing json body\n");
    	return;
    }

    json_object_object_add(json_obj, BLF_JSON_BROKER_ZONE, json_object_new_string(server_ptr->zone->zone));
    json_object_object_add(json_obj, BLF_JSON_AMQP_RECEIVED, json_object_new_int(time(NULL)));


    JObj = kz_json_get_object(json_obj, BLF_JSON_SERVERID);
    if(JObj != NULL) {
        const char* _kz_server_id_str = json_object_get_string(JObj);
        if(_kz_server_id_str && strlen(_kz_server_id_str) > 0) {
        	sprintf(buffer, "consumer://%d/%s", _kz_server_id, _kz_server_id_str);
            json_object_object_del(json_obj, BLF_JSON_SERVERID);
        	json_object_object_add(json_obj, BLF_JSON_SERVERID, json_object_new_string(buffer));
        } else {
            json_object_object_del(json_obj, BLF_JSON_SERVERID);
        }
    }

    json_object_object_add(json_obj, BLF_JSON_BROKER_ZONE, json_object_new_string(server_ptr->zone->zone));

    JObj = kz_json_get_object(json_obj, BLF_JSON_MSG_ID);
    if(JObj != NULL) {
    	message_id = kz_str_dup_from_char((char*)json_object_get_string(JObj));
    	if(message_id == NULL) {
    		LM_ERR("Error allocating memory for message_id copy\n");
    		goto error;
    	}
		if(idx < dbk_channels) {
			cmd = kz_cmd_retrieve(message_id);
			if(cmd)
				cmd->return_code = AMQP_RESPONSE_NORMAL;
		}
    }

	ptr = (kz_amqp_consumer_delivery_ptr) shm_malloc(sizeof(kz_amqp_consumer_delivery));
	if(ptr == NULL) {
		LM_ERR("NO MORE SHARED MEMORY!");
		goto error;
	}
	memset(ptr, 0, sizeof(kz_amqp_consumer_delivery));
	ptr->channel = envelope->channel;
	ptr->delivery_tag = envelope->delivery_tag;
	ptr->payload = kz_amqp_string_dup((char*)json_object_to_json_string(json_obj));
	ptr->cmd = cmd;
	ptr->message_id = message_id;
	ptr->routing_key = kz_str_from_amqp_bytes(envelope->routing_key);

	if(bind) {
		ptr->event_key = kz_amqp_bytes_dup(bind->event_key);
		ptr->event_subkey = kz_amqp_bytes_dup(bind->event_subkey);
	}

    if(bind && bind->consistent_worker) {
        str rk;
    	if(bind->consistent_worker_key != NULL &&
    			(JObj = kz_json_get_object(json_obj, bind->consistent_worker_key->s)) != NULL) {
    		rk.s = (char*)json_object_get_string(JObj);
    		rk.len = strlen(rk.s);
    	} else {
    		rk.s = (char*)envelope->routing_key.bytes;
    		rk.len = (int)envelope->routing_key.len;
    	}
        worker = core_hash(&rk, NULL, dbk_consumer_workers);
        LM_DBG("computed worker for %.*s is %d\n", rk.len, rk.s, worker);
    } else {
        consumer++;
        if(consumer >= dbk_consumer_workers) {
            consumer = 0;
        }
        worker = consumer;
    }

	if (write(kz_worker_pipes[worker], &ptr, sizeof(ptr)) != sizeof(ptr)) {
		LM_ERR("failed to send payload to consumer %d : %s\nPayload %s\n", consumer, strerror(errno), ptr->payload);
		goto error;
	}

	json_object_put(json_obj);

	return;

error:
	if(ptr)
		kz_amqp_free_consumer_delivery(ptr);

	if(json_obj)
		json_object_put(json_obj);

}


int kz_amqp_consumer_proc(kz_amqp_server_ptr server_ptr)
{
    int i, idx;
	int OK;
//	char* payload;
	int channel_res;
	kz_amqp_conn_ptr consumer = NULL;
	kz_amqp_channel_ptr consumer_channels = NULL;
	int channel_base = 0;

	if(server_ptr->zone == kz_amqp_get_primary_zone())
		channel_base = dbk_channels;

	consumer = (kz_amqp_conn_ptr)pkg_malloc(sizeof(kz_amqp_conn));
    if(consumer == NULL)
    {
    	LM_ERR("NO MORE PACKAGE MEMORY\n");
    	return 1;
    }
    memset(consumer, 0, sizeof(kz_amqp_conn));
    consumer->server = server_ptr;

    consumer_channels = (kz_amqp_channel_ptr)pkg_malloc(sizeof(kz_amqp_channel)*bindings_count);
    if(consumer_channels == NULL)
    {
    	LM_ERR("NO MORE PACKAGE MEMORY\n");
    	return 1;
    }
	for(i=0; i < bindings_count; i++)
		consumer_channels[i].channel = channel_base + i + 1;

    while(1) {
    	OK = 1;
   		if(kz_amqp_connection_open(consumer)) {
   			sleep(3);
   			continue;
   		}
    	kz_amqp_fire_connection_event("open", server_ptr->connection->info.host, server_ptr->zone->zone);

    	/* reset channels */

		/* bind targeted channels */
    	for(i=0,channel_res=0; i < channel_base && channel_res == 0; i++) {
			channel_res = kz_amqp_channel_open(consumer, server_ptr->channels[i].channel);
			if(channel_res == 0) {
				kz_amqp_bind_targeted_channel(consumer, i);
			}
    	}

		/*  cleanup consumer channels */
    	for(i=0,channel_res=0; i < bindings_count && channel_res == 0; i++) {
    		consumer_channels[i].consumer = NULL;
    	}

    	i = 0;
		/* bind consumers */
		if(kz_bindings != NULL) {
			kz_amqp_binding_ptr binding = kz_bindings->head;
			while(binding != NULL && OK) {
				if(binding->bind->federate || server_ptr->zone == kz_amqp_get_primary_zone()) {
					channel_res = kz_amqp_channel_open(consumer, consumer_channels[i].channel);
					if(channel_res == 0) {
						kz_amqp_bind_consumer(consumer, binding->bind, i, consumer_channels);
						consumer_channels[i].state = KZ_AMQP_CHANNEL_BINDED;
						i++;
					} else {
						LM_ERR("Error opening channel %d in server %s\n", i, server_ptr->connection->url);
						OK = 0;
					}
				}
				binding = binding->next;
			}
		}

		LM_DBG("CONSUMER INIT DONE\n");

		while(OK) {
			amqp_envelope_t envelope;
			amqp_maybe_release_buffers(consumer->conn);
			amqp_rpc_reply_t reply = amqp_consume_message(consumer->conn, &envelope, NULL, 0);
			switch(reply.reply_type) {
			case AMQP_RESPONSE_LIBRARY_EXCEPTION:
				OK=0;
				switch(reply.library_error) {
				case AMQP_STATUS_HEARTBEAT_TIMEOUT:
					LM_ERR("AMQP_STATUS_HEARTBEAT_TIMEOUT\n");
					break;
				case AMQP_STATUS_TIMEOUT:
					break;
				case AMQP_STATUS_UNEXPECTED_STATE:
					LM_DBG("AMQP_STATUS_UNEXPECTED_STATE\n");
					kz_amqp_consume_error(consumer);
					break;
				default:
					LM_ERR("AMQP_RESPONSE_LIBRARY_EXCEPTION %i\n", reply.library_error);
					break;
				};
				break;

			case AMQP_RESPONSE_NORMAL:
				idx = envelope.channel-1;
				if(idx < channel_base) {
					kz_amqp_send_worker_event(server_ptr, &envelope, NULL);
				} else {
					idx = idx - channel_base;
					if(!consumer_channels[idx].consumer->no_ack ) {
						if(amqp_basic_ack(consumer->conn, envelope.channel, envelope.delivery_tag, 0 ) < 0) {
							LM_ERR("AMQP ERROR TRYING TO ACK A MSG\n");
							OK = 0;
						}
					}
					if(OK)
						kz_amqp_send_worker_event(server_ptr, &envelope, consumer_channels[idx].consumer);
				}
				amqp_destroy_envelope(&envelope);
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
		}

    	kz_amqp_connection_close(consumer);

    }
    return 0;
}

void kz_amqp_consumer_worker_cb(int fd, short event, void *arg)
{
	kz_amqp_cmd_ptr cmd = NULL;
	kz_amqp_consumer_delivery_ptr Evt;
	if (read(fd, &Evt, sizeof(Evt)) != sizeof(Evt)) {
		LM_ERR("failed to read from command pipe: %s\n", strerror(errno));
		return;
	}

	LM_DBG("consumer %d received payload %s\n", my_pid(), Evt->payload);

	if(Evt->cmd) {
		cmd =Evt->cmd;
		if(cmd->type == KZ_AMQP_CMD_ASYNC_CALL ) {
			if(cmd->return_code == AMQP_RESPONSE_NORMAL) {
				kz_amqp_set_last_result(Evt->payload);
				kz_amqp_cb_ok(cmd);
			} else {
				kz_amqp_reset_last_result();
				kz_amqp_cb_error(cmd);
				LM_DBG("run error exiting consumer %d\n", my_pid());
			}
		} else {
			cmd->return_payload = Evt->payload;
			Evt->payload = NULL;
			Evt->cmd = NULL;
			lock_release(&cmd->lock);
		}
	} else {
//		kz_amqp_consumer_event(Evt->payload, Evt->event_key, Evt->event_subkey);
		kz_amqp_consumer_event(Evt);
	}

	kz_amqp_free_consumer_delivery(Evt);
	LM_DBG("exiting consumer %d\n", my_pid());
}

int kz_amqp_consumer_worker_proc(int cmd_pipe)
{
	event_init();
	struct event pipe_ev;
	set_non_blocking(cmd_pipe);
	event_set(&pipe_ev, cmd_pipe, EV_READ | EV_PERSIST, kz_amqp_consumer_worker_cb, &pipe_ev);
	event_add(&pipe_ev, NULL);
	event_dispatch();
	return 0;
}

void kz_amqp_timer_destroy(kz_amqp_timer_ptr* pTimer)
{
	if(!pTimer)
		return;
	kz_amqp_timer_ptr timer = *pTimer;
	if (timer->ev != NULL) {
		event_del(timer->ev);
		pkg_free(timer->ev);
		timer->ev = NULL;
	}
	close(timer->fd);
	pkg_free(timer->timer);
	pkg_free(timer);
	*pTimer = NULL;
}

int kz_amqp_timer_create(kz_amqp_timer_ptr* pTimer, int seconds, void (*callback)(int, short, void *), void *data)
{
	kz_amqp_timer_ptr timer = NULL;
	struct itimerspec *itime = NULL;
	struct event *timer_ev = NULL;
	int timerfd = 0;

	timer = (kz_amqp_timer_ptr) pkg_malloc(sizeof(kz_amqp_timer));
	if (!timer) {
		LM_ERR("could not allocate timer struct.\n");
		goto error;
	}
	memset(timer, 0, sizeof(kz_amqp_timer));

	timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if (timerfd == -1) {
		LM_ERR("could not create timer.\n");
		goto error;
	}

	itime = pkg_malloc(sizeof(struct itimerspec));
	if (!itime) {
		LM_ERR("could not allocate itimerspec struct.\n");
		goto error;
	}
	itime->it_interval.tv_sec = 0;
	itime->it_interval.tv_nsec = 0;
	itime->it_value.tv_sec = seconds;
	itime->it_value.tv_nsec = 0;

	if (timerfd_settime(timerfd, 0, itime, NULL) == -1) {
		LM_ERR("could not set timer for %i seconds in %i\n", seconds, timerfd);
		goto error;
	}

	LM_DBG("timerfd value is %d\n", timerfd);
	timer_ev = pkg_malloc(sizeof(struct event));
	if (!timer_ev) {
		LM_ERR("could not allocate event struct.\n");
		goto error;
	}
	event_set(timer_ev, timerfd, EV_READ | EV_PERSIST, callback, data);
	if (event_add(timer_ev, NULL) == -1) {
		LM_ERR("event_add failed while creating timer (%s).\n", strerror(errno));
		goto error;
	}

	timer->ev = timer_ev;
	timer->timer = itime;
	timer->fd = timerfd;
	*pTimer = timer;

	return 0;

error:

	if (timer_ev)
		pkg_free(timer_ev);

	if (itime)
		pkg_free(itime);

	if (timerfd > 0)
		close(timerfd);

	if (timer)
		pkg_free(timer);

	*pTimer = NULL;

	return -1;
}

void kz_amqp_heartbeat_proc(int fd, short event, void *arg)
{
	int res;
	amqp_frame_t heartbeat;
	kz_amqp_conn_ptr connection = (kz_amqp_conn_ptr) arg;
	LM_DBG("sending heartbeat to zone : %s , connection id : %d\n", connection->server->zone->zone, connection->server->id);
	if (connection->state != KZ_AMQP_CONNECTION_OPEN) {
		kz_amqp_timer_destroy(&connection->heartbeat);
		return;
	}
	heartbeat.channel = 0;
	heartbeat.frame_type = AMQP_FRAME_HEARTBEAT;
	res = amqp_send_frame(connection->conn, &heartbeat);
	if (res != AMQP_STATUS_OK) {
		LM_ERR("error sending heartbeat to zone : %s , connection id : %d\n", connection->server->zone->zone, connection->server->id);
		kz_amqp_timer_destroy(&connection->heartbeat);
		kz_amqp_handle_server_failure(connection);
		return;
	}
	timerfd_settime(connection->heartbeat->fd, 0, connection->heartbeat->timer, NULL);
}

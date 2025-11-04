/*
 * Copyright (C) 2025 1&1 AG (www.1und1.de)
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
 */

#include <stdlib.h>

#include "redis_sentinels.h"
#include "db_redis_mod.h"

sentinel_config_t db_redis_sc;
struct reply_list replica_list = {NULL, NULL, 0};


static int is_text(const redisReply *r)
{
	return r
		   && (r->type == REDIS_REPLY_STRING || r->type == REDIS_REPLY_STATUS);
}

static int str_eq_reply(const redisReply *r, const char *s)
{
	return is_text(r) && r->len == (int)strlen(s)
		   && memcmp(r->str, s, r->len) == 0;
}

static redisReply *get_field(redisReply *r, const char *field)
{
	if(!r)
		return NULL;

	if(r->type == REDIS_REPLY_ARRAY) {
		// Treat as alternating field/value list
		for(size_t i = 0; i + 1 < r->elements; i += 2) {
			redisReply *k = r->element[i];
			redisReply *v = r->element[i + 1];
			if(str_eq_reply(k, field))
				return v;
		}
		return NULL;
	}

	return NULL;
}

static int replica_list_init(redisReply *reply, struct reply_list *list)
{
	if(!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements == 0) {
		LM_ERR("Invalid reply for replica list\n");
		return -1;
	}

	if(!list) {
		LM_ERR("reply_list pointer is NULL\n");
		return -1;
	}

	list->count = 0;
	list->head = NULL;
	list->tail = NULL;

	for(size_t i = 0; i < reply->elements; i++) {
		struct reply_node *node;

		/*
		* TODO: Itroduced here for testing purposes only.
		* These flags are returned by the `SENTINEL REPLICAS` command and indicate the health/status of Redis instances.
		* - "s_down": The instance is considered down by this Sentinel (subjectively down).
		* - "o_down": The instance is considered down by a quorum of Sentinels (objectively down).
		* - "disconnected": The Sentinel lost the connection with the instance.
		*/
		redisReply *status_reply = get_field(reply->element[i], "flags");
		if(!status_reply || !is_text(status_reply)
				|| strstr(status_reply->str, "_down")) {
			LM_DBG("Discard %s for replica selection algo, reason: s_down or "
				   "o_down flag\n",
					reply->element[i]->element[1]->str);
			continue;
		}

		status_reply = get_field(reply->element[i], "master-link-status");
		if(!status_reply || !str_eq_reply(status_reply, "ok")) {
			LM_DBG("Discard %s for replica selection algo, reason: non-ok "
				   "master-link-status\n",
					reply->element[i]->element[1]->str);
			continue;
		}

		node = (struct reply_node *)pkg_malloc(sizeof(struct reply_node));
		if(!node) {
			LM_ERR("Failed to allocate memory for reply_node\n");
			// Free already allocated nodes
			struct reply_node *cur = list->head;
			while(cur) {
				struct reply_node *tmp = cur;
				cur = cur->next;
				pkg_free(tmp);
			}
			return -1;
		}
		node->reply = reply->element[i];
		node->next = NULL;
		list->count++;
		LM_DBG("Accept %s for replica selection algo \n",
				reply->element[i]->element[1]->str);
		if(!list->head) {
			list->head = node;
			list->tail = node;
		} else {
			list->tail->next = node;
			list->tail = node;
		}
	}

	return 0;
}

int replica_list_free(struct reply_list *list)
{
	if(!list) {
		return -1;
	}

	struct reply_node *current = list->head;
	while(current) {
		struct reply_node *temp = current;
		current = current->next;
		pkg_free(temp);
	}
	list->head = NULL;
	list->tail = NULL;
	list->count = 0;
	return 0;
}

static redisReply *replica_list_pop(struct reply_list *list, size_t idx)
{
	struct reply_node *current = NULL;
	struct reply_node *prev = NULL;
	size_t current_idx = 0;
	if(!list || idx >= list->count) {
		return NULL;
	}

	current = list->head;

	//traverse the list to the idx-th node and remove it from the list also return it
	while(current && current_idx < idx) {
		prev = current;
		current = current->next;
		current_idx++;
	}
	if(!current) {
		return NULL;
	}
	//remove the node from the list
	if(prev) {
		prev->next = current->next;
	} else {
		list->head = current->next;
	}
	if(current == list->tail) {
		list->tail = prev;
	}
	list->count--;
	return current->reply;
}

static int validate_role(
		km_redis_con_t *con, const char *host, int port, redis_role_t role)
{
	redisReply *reply = NULL;
	const char *role_str = NULL;
	struct timeval tv = {0, 500000};

	LM_DBG("Trying to validate %s: %s:%d\n",
			role == REDIS_ROLE_MASTER ? "master" : "replica", host, port);

	// Connect to Redis server
	con->con = redisConnectWithTimeout(host, port, tv);
	if(!con->con || con->con->err) {
		LM_INFO("Cannot open connection to %s:%d (%s)\n", host, port,
				con->con ? con->con->errstr : "no error info provided");
		goto err;
	}

	// Authenticate if needed
	if(db_redis_authenticate(con->con, con->id->password) != 0) {
		LM_INFO("Authentication error\n");
		goto err;
	}

	// Issue ROLE command
	reply = redisCommand(con->con, "ROLE");
	if(!reply) {
		LM_INFO("Failed to run ROLE command.\n");
		goto err;
	}

	// Check reply format
	if(reply->type != REDIS_REPLY_ARRAY || reply->elements < 1) {
		LM_INFO("Unexpected reply format from ROLE command.\n");
		goto err;
	}

	// Validate role
	role_str = reply->element[0]->str;
	if(role == REDIS_ROLE_MASTER && strcmp(role_str, "master") != 0) {
		LM_INFO("Server is not a master (got '%s').\n", role_str);
		goto err;
	}
	if(role == REDIS_ROLE_REPLICA && strcmp(role_str, "slave") != 0) {
		LM_INFO("Server is not a replica (got '%s').\n", role_str);
		goto err;
	}

	LM_INFO("Successfully validated %s %s:%d\n",
			role == REDIS_ROLE_MASTER ? "master" : "replica", host, port);

	if(con->con) {
		redisFree(con->con);
		con->con = NULL;
	}
	if(reply)
		freeReplyObject(reply);
	return 0;

err:
	if(con->con) {
		redisFree(con->con);
		con->con = NULL;
	}
	if(reply)
		freeReplyObject(reply);
	return -1;
}

static int set_con_details(km_redis_con_t *con, const char *host, int port)
{
	// Free existing connection if any
	if(con->id->host) {
		pkg_free(con->id->host);
		con->id->host = NULL;
	}
	// Update connection details
	con->id->host = pkg_malloc(strlen(host) + 1);
	if(!con->id->host) {
		LM_ERR("Failed to allocate memory for host string\n");
		return -1;
	}
	strcpy(con->id->host, host);
	con->id->port = (unsigned short)port;

	LM_DBG("Set connection details to %s:%d\n", con->id->host, con->id->port);
	return 0;
}

// "user=user;password=password;sentinels=host1:port1|host2:port2")
int parse_sentinel_config(char *spec)
{
	param_t *pit = NULL;
	param_hooks_t phooks;
	char *sentinels;
	str s;

	LM_DBG("Parsing sentinels config: %s\n", spec);
	s.s = spec;
	s.len = strlen(spec);
	if(s.s[s.len - 1] == ';')
		s.len--;

	if(parse_params(&s, CLASS_ANY, &phooks, &pit) < 0) {
		LM_ERR("Failed parsing params value\n");
		goto error;
	}

	db_redis_sc.attrs = pit;
	db_redis_sc.spec = spec;
	for(pit = db_redis_sc.attrs; pit; pit = pit->next) {
		if(pit->name.len == 4 && strncmp(pit->name.s, "user", 4) == 0) {
			db_redis_sc.user = shm_malloc(pit->body.len + 1);
			snprintf(db_redis_sc.user, pit->body.len + 1, "%.*s", pit->body.len,
					pit->body.s);
		} else if(pit->name.len == 8
				  && strncmp(pit->name.s, "password", 8) == 0) {
			db_redis_sc.password = shm_malloc(pit->body.len + 1);
			snprintf(db_redis_sc.password, pit->body.len + 1, "%.*s",
					pit->body.len, pit->body.s);
		} else if(pit->name.len == 9
				  && strncmp(pit->name.s, "sentinels", 9) == 0) {
			sentinels = shm_malloc(pit->body.len + 1);
			snprintf(sentinels, pit->body.len + 1, "%.*s", pit->body.len,
					pit->body.s);
			if(db_redis_add_sentinels(sentinels) < 0) {
				LM_ERR("Failed to add sentinels\n");
				shm_free(sentinels);
				goto error;
			}
			shm_free(sentinels);
		}
	}

	return 0;
error:
	if(db_redis_sc.user)
		shm_free(db_redis_sc.user);
	if(db_redis_sc.password)
		shm_free(db_redis_sc.password);
	if(pit != NULL)
		free_params(pit);
	return -1;
}

int db_redis_add_sentinels(char *spec)
{
	char *sentinel = strtok(spec, "|");
	unsigned int port = 0;
	str port_str;
	redis_sentinel_t *elem = NULL;

	while(sentinel) {
		char *colon = strchr(sentinel, ':');

		if(!colon) {
			LM_ERR("Invalid sentinel format, expected 'host:port', got: %s\n",
					sentinel);
			goto error;
		}
		*colon = '\0'; // Split in-place
		port_str.s = colon + 1;
		port_str.len = strlen(port_str.s);
		if(str2int(&port_str, &port) != 0) {
			LM_ERR("Invalid port: %s\n", port_str.s);
			goto error;
		}

		elem = shm_malloc(sizeof(redis_sentinel_t));
		if(!elem) {
			LM_ERR("Failed to allocate memory for sentinel element\n");
			goto error;
		}
		elem->host = shm_malloc(strlen(sentinel) + 1);
		if(!elem->host) {
			LM_ERR("Failed to allocate memory for sentinel host\n");
			shm_free(elem);
			goto error;
		}

		strcpy(elem->host, sentinel);
		elem->port = port;
		elem->next = NULL;

		if(db_redis_sc.sentinel_list == NULL) {
			db_redis_sc.sentinel_list = elem;
			db_redis_sc.sentinel_list_tail = elem;
		} else {
			db_redis_sc.sentinel_list_tail->next = elem;
			db_redis_sc.sentinel_list_tail = elem;
		}

		sentinel = strtok(NULL, "|");
	}

	LM_DBG("Adding dburl sentinels:\n");

	for(elem = db_redis_sc.sentinel_list; elem; elem = elem->next) {
		LM_DBG("Sentinel: %s:%u\n", elem->host, elem->port);
	}

	return 0;
error:
	return -1;
}

/* Authenticate to Redis if a password was provided */
int db_redis_authenticate(redisContext *ctx, const char *password)
{
	redisReply *reply = NULL;

	if(!ctx) {
		LM_ERR("No Redis context");
		goto err;
	}
	if(!password) {
		password = db_redis_db_pass;
	}
	if(!password) {
		return 0;
	}

	reply = redisCommand(ctx, "AUTH %s", password);
	if(!reply) {
		LM_ERR("AUTH error: %s\n", ctx->errstr);
		goto err;
	}
	if(reply->type == REDIS_REPLY_ERROR) {
		LM_ERR("AUTH failed: %s\n", reply->str);
		goto err;
	}
	freeReplyObject(reply);
	return 0;

err:
	if(reply)
		freeReplyObject(reply);
	return -1;
}

int db_redis_select_master(redisContext *sentinel_ctx, km_redis_con_t *con)
{
	redisReply *reply = redisCommand(sentinel_ctx,
			"SENTINEL get-master-addr-by-name %s", db_redis_master_name);

	if(!reply) {
		LM_ERR("Failed to run SENTINEL get-master-addr-by-name.\n");
		return -1;
	}

	// Expect reply as array: [IP, port]
	if(reply->type != REDIS_REPLY_ARRAY || reply->elements != 2) {
		LM_ERR("Unexpected reply format or number of elements from "
			   "Sentinel.\n");
		goto err;
	}
	if(validate_role(con, reply->element[0]->str, atoi(reply->element[1]->str),
			   REDIS_ROLE_MASTER)
			!= 0) {
		LM_ERR("Failed to validate master %s:%s\n", reply->element[0]->str,
				reply->element[1]->str);
		goto err;
	}
	if(set_con_details(
			   con, reply->element[0]->str, atoi(reply->element[1]->str))
			!= 0) {
		goto err;
	}
	freeReplyObject(reply);
	return 0;

err:
	if(reply)
		freeReplyObject(reply);
	return -1;
}

int db_redis_select_replica(redisContext *sentinel_ctx, km_redis_con_t *con)
{
	int idx, port, replica_found = 0;
	redisReply *reply = NULL;
	redisReply *replica_reply = NULL;
	const char *host = NULL;

	reply = redisCommand(
			sentinel_ctx, "SENTINEL replicas %s", db_redis_master_name);

	if(!reply) {
		LM_ERR("Failed to run SENTINEL replicas %s", db_redis_master_name);
		goto err;
	}

	if(!(reply->type == REDIS_REPLY_ARRAY && reply->elements > 0)) {
		LM_ERR("No replicas found or unexpected reply format\n");
		goto err;
	}

	if(replica_list_init(reply, &replica_list) != 0) {
		LM_ERR("Failed to initialize replica list\n");
		goto err;
	}

	while(replica_list.count > 0) {
		// load on replica later on: round robin or random
		idx = rand() % replica_list.count;
		replica_reply = replica_list_pop(&replica_list, idx);
		if(!(replica_reply->type == REDIS_REPLY_ARRAY
				   && replica_reply->elements >= 6)) {
			LM_ERR("Unexpected replica reply format\n");
			goto err;
		}

		host = replica_reply->element[3]->str;
		port = atoi(replica_reply->element[5]->str);

		if(validate_role(con, host, port, REDIS_ROLE_REPLICA) == 0) {
			replica_found = 1;
			using_master_read_only = 0;
			// Successfully validated a replica, set connection details
			if(set_con_details(con, host, port) != 0)
				goto err;
			break;
		} else {
			continue; // Try next replica
		}
	}

	if(!replica_found) {
		LM_ERR("Could not validate any replica server, defaulting to master\n");
		if(db_redis_select_master(sentinel_ctx, con) != 0) {
			goto err;
		} else {
			LM_INFO("Successfully connected to master as no valid replicas "
					"found\n");
			using_master_read_only = 1;
			last_seen_time = time(NULL);
		}
	}

	if(reply)
		freeReplyObject(reply);
	return 0;

err:
	if(reply)
		freeReplyObject(reply);
	return -1;
}

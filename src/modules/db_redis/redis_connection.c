/*
 * Copyright (C) 2018 Andreas Granig (sipwise.com)
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

#include "db_redis_mod.h"
#include "redis_connection.h"
#include "redis_table.h"

extern int db_redis_verbosity;

static void print_query(redis_key_t *query) {
    redis_key_t *k;

    LM_DBG("Query dump:\n");
    for (k = query; k; k = k->next) {
        LM_DBG("  %s\n", k->key.s);
    }
}

static int db_redis_push_query(km_redis_con_t *con, redis_key_t *query) {

    redis_command_t *cmd = NULL;
    redis_command_t *tmp = NULL;
    redis_key_t *new_query = NULL;

    if (!query)
        return 0;

    cmd = (redis_command_t*)pkg_malloc(sizeof(redis_command_t));
    if (!cmd) {
        LM_ERR("Failed to allocate memory for redis command\n");
        goto err;
    }

    // duplicate query, as original one might be free'd after being
    // appended
    while(query) {
         if (db_redis_key_add_str(&new_query, &query->key) != 0) {
            LM_ERR("Failed to duplicate query\n");
            goto err;
        }
        query = query->next;
    }

    cmd->query = new_query;
    cmd->next = NULL;

    if (!con->command_queue) {
        con->command_queue = cmd;
    } else {
        tmp = con->command_queue;
        while (tmp->next)
            tmp = tmp->next;
        tmp->next = cmd;
    }

    return 0;

err:
    if (new_query) {
        db_redis_key_free(&new_query);
    }
    if (cmd) {
        pkg_free(cmd);
    }
    return -1;
}

static redis_key_t* db_redis_shift_query(km_redis_con_t *con) {
    redis_command_t *cmd;
    redis_key_t *query;

    query = NULL;
    cmd = con->command_queue;

    if (cmd) {
        query = cmd->query;
        con->command_queue = cmd->next;
        pkg_free(cmd);
    }

    return query;
}

int db_redis_connect(km_redis_con_t *con) {
    struct timeval tv;
    redisReply *reply;
    int db;

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    db = atoi(con->id->database);
    reply = NULL;

    // TODO: introduce require_master mod-param and check if we're indeed master
    // TODO: on carrier, if we have db fail-over, the currently connected
    // redis server will become slave without dropping connections?

    LM_DBG("connecting to redis at %s:%d\n", con->id->host, con->id->port);
    con->con = redisConnectWithTimeout(con->id->host, con->id->port, tv);

    if (!con->con) {
        LM_ERR("cannot open connection: %.*s\n", con->id->url.len, con->id->url.s);
        goto err;
    }
    if (con->con->err) {
        LM_ERR("cannot open connection to %.*s: %s\n", con->id->url.len, con->id->url.s,
            con->con->errstr);
        goto err;
    }

    if (con->id->password) {
        reply = redisCommand(con->con, "AUTH %s", con->id->password);
        if (!reply) {
            LM_ERR("cannot authenticate connection %.*s: %s\n",
                    con->id->url.len, con->id->url.s, con->con->errstr);
            goto err;
        }
        if (reply->type == REDIS_REPLY_ERROR) {
            LM_ERR("cannot authenticate connection %.*s: %s\n",
                    con->id->url.len, con->id->url.s, reply->str);
            goto err;
        }
        freeReplyObject(reply); reply = NULL;
    }

    reply = redisCommand(con->con, "PING");
    if (!reply) {
        LM_ERR("cannot ping server on connection %.*s: %s\n",
                con->id->url.len, con->id->url.s, con->con->errstr);
        goto err;
    }
    if (reply->type == REDIS_REPLY_ERROR) {
        LM_ERR("cannot ping server on connection %.*s: %s\n",
                con->id->url.len, con->id->url.s, reply->str);
        goto err;
    }
    freeReplyObject(reply); reply = NULL;

    reply = redisCommand(con->con, "SELECT %i", db);
    if (!reply) {
        LM_ERR("cannot select db on connection %.*s: %s\n",
                con->id->url.len, con->id->url.s, con->con->errstr);
        goto err;
    }
    if (reply->type == REDIS_REPLY_ERROR) {
        LM_ERR("cannot select db on connection %.*s: %s\n",
                con->id->url.len, con->id->url.s, reply->str);
        goto err;
    }
    freeReplyObject(reply); reply = NULL;
    LM_DBG("connection opened to %.*s\n", con->id->url.len, con->id->url.s);

    return 0;

err:
    if (reply)
        freeReplyObject(reply);
    if (con->con) {
        redisFree(con->con);
        con->con = NULL;
    }
    return -1;
}

/*! \brief
 * Create a new connection structure,
 * open the redis connection and set reference count to 1
 */
km_redis_con_t* db_redis_new_connection(const struct db_id* id) {
    km_redis_con_t *ptr = NULL;

    if (!id) {
        LM_ERR("invalid id parameter value\n");
        return 0;
    }

    ptr = (km_redis_con_t*)pkg_malloc(sizeof(km_redis_con_t));
    if (!ptr) {
        LM_ERR("no private memory left\n");
        return 0;
    }
    memset(ptr, 0, sizeof(km_redis_con_t));
    ptr->id = (struct db_id*)id;

    /*
    LM_DBG("trying to initialize connection to '%.*s' with schema path '%.*s' and keys '%.*s'\n",
            id->url.len, id->url.s,
            redis_schema_path.len, redis_schema_path.s,
            redis_keys.len, redis_keys.s);
    */
    LM_DBG("trying to initialize connection to '%.*s'\n",
            id->url.len, id->url.s);
    if (db_redis_parse_schema(ptr) != 0) {
        LM_ERR("failed to parse 'schema' module parameter\n");
        goto err;
    }
    if (db_redis_parse_keys(ptr) != 0) {
        LM_ERR("failed to parse 'keys' module parameter\n");
        goto err;
    }

    if(db_redis_verbosity > 0) db_redis_print_all_tables(ptr);

    ptr->ref = 1;
    ptr->append_counter = 0;

    if (db_redis_connect(ptr) != 0) {
        LM_ERR("Failed to connect to redis db\n");
        goto err;
    }

    LM_DBG("connection opened to %.*s\n", id->url.len, id->url.s);

    return ptr;

 err:
    if (ptr) {
        if (ptr->con) {
            redisFree(ptr->con);
        }
        pkg_free(ptr);
    }
    return 0;
}


/*! \brief
 * Close the connection and release memory
 */
void db_redis_free_connection(struct pool_con* con) {
    km_redis_con_t * _c;

    LM_DBG("freeing db_redis connection\n");

    if (!con) return;

    _c = (km_redis_con_t*) con;

    if (_c->id) free_db_id(_c->id);
    if (_c->con) {
        redisFree(_c->con);
    }

    db_redis_free_tables(_c);
    pkg_free(_c);
}

void *db_redis_command_argv(km_redis_con_t *con, redis_key_t *query) {
    char **argv = NULL;
    int argc;

    print_query(query);

    argc = db_redis_key_list2arr(query, &argv);
    if (argc < 0) {
        LM_ERR("Failed to allocate memory for query array\n");
        return NULL;
    }
    LM_DBG("query has %d args\n", argc);

    redisReply *reply = redisCommandArgv(con->con, argc, (const char**)argv, NULL);
    if (con->con->err == REDIS_ERR_EOF) {
        if (db_redis_connect(con) != 0) {
            LM_ERR("Failed to reconnect to redis db\n");
            pkg_free(argv);
            if (con->con) {
                redisFree(con->con);
                con->con = NULL;
            }
            return NULL;
        }
        reply = redisCommandArgv(con->con, argc, (const char**)argv, NULL);
    }
    pkg_free(argv);
    return reply;
}

int db_redis_append_command_argv(km_redis_con_t *con, redis_key_t *query, int queue) {
    char **argv = NULL;
    int ret, argc;

    print_query(query);

    if (queue > 0 && db_redis_push_query(con, query) != 0) {
        LM_ERR("Failed to queue redis command\n");
        return -1;
    }

    argc = db_redis_key_list2arr(query, &argv);
    if (argc < 0) {
        LM_ERR("Failed to allocate memory for query array\n");
        return -1;
    }
    LM_DBG("query has %d args\n", argc);

    ret = redisAppendCommandArgv(con->con, argc, (const char**)argv, NULL);

    // this should actually never happen, because if all replies
    // are properly consumed for the previous command, it won't send
    // out a new query until redisGetReply is called
    if (con->con->err == REDIS_ERR_EOF) {
        if (db_redis_connect(con) != 0) {
            LM_ERR("Failed to reconnect to redis db\n");
            pkg_free(argv);
            if (con->con) {
                redisFree(con->con);
                con->con = NULL;
            }
            return ret;
        }
        ret = redisAppendCommandArgv(con->con, argc, (const char**)argv, NULL);
    }
    pkg_free(argv);
    if (!con->con->err) {
        con->append_counter++;
    }
    return ret;
}

int db_redis_get_reply(km_redis_con_t *con, void **reply) {
    int ret;
    redis_key_t *query;

    if (!con || !con->con) {
        LM_ERR("Internal error passing null connection\n");
        return -1;
    }

    *reply = NULL;
    ret = redisGetReply(con->con, reply);
    if (con->con->err == REDIS_ERR_EOF) {
        LM_DBG("redis connection is gone, try reconnect\n");
        con->append_counter = 0;
        if (db_redis_connect(con) != 0) {
            LM_ERR("Failed to reconnect to redis db\n");
            if (con->con) {
                redisFree(con->con);
                con->con = NULL;
            }
            return -1;
        }
        // take commands from oldest to newest and re-do again,
        // but don't queue them once again in retry-mode
        while ((query = db_redis_shift_query(con))) {
            LM_DBG("re-queueing appended command\n");
            if (db_redis_append_command_argv(con, query, 0) != 0) {
                LM_ERR("Failed to re-queue redis command");
                return -1;
            }
            db_redis_key_free(&query);
        }
        ret = redisGetReply(con->con, reply);
        if (con->con->err != REDIS_ERR_EOF) {
            con->append_counter--;
        }
    } else {
        LM_DBG("get_reply successful, removing query\n");
        query = db_redis_shift_query(con);
        db_redis_key_free(&query);
        con->append_counter--;
    }
    return ret;
}

void db_redis_free_reply(redisReply **reply) {
    if (reply && *reply) {
        freeReplyObject(*reply);
        *reply = NULL;
    }
}

void db_redis_consume_replies(km_redis_con_t *con) {
    redisReply *reply = NULL;
    redis_key_t *query;
    while (con->append_counter > 0 && con->con && !con->con->err) {
        LM_DBG("consuming outstanding reply %u", con->append_counter);
        db_redis_get_reply(con, (void**)&reply);
        if (reply) {
            freeReplyObject(reply);
            reply = NULL;
        }
    }
    while ((query = db_redis_shift_query(con))) {
        LM_DBG("consuming queued command\n");
        db_redis_key_free(&query);
    }
}

const char *db_redis_get_error(km_redis_con_t *con) {
    if (con && con->con && con->con->errstr[0]) {
        return con->con->errstr;
    } else {
        return "<broken redis connection>";
    }
}

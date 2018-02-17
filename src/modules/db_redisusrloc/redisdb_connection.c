/* 
 * Copyright (C) 2017 plivo (plivo.com)
 * Author : Surendra Tiwari (surendratiwari3@gmail.com) 
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

#include "../../core/mem/mem.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "redisdb_connection.h"
#include <stdlib.h>
/*! \brief
 * Create a new connection structure,
 * open the redis connection and set reference count to 1
 */
km_redis_con_t* db_redis_new_connection(const struct db_id* id)
{
	km_redis_con_t *ptr;	
	char string_redis_con_url[80];
	const char token_char[4] = "://@";
	struct timeval timeout = { 0, 200000}; // 200 mili seconds  
        char *token;
        char *redis_socket_info[5];
        int i = 0;
	if (!id) {
		LM_ERR("invalid parameter value\n");
		return 0;
	}
	snprintf(string_redis_con_url,id->url.len+1,"%s",id->url.s);
	ptr = (km_redis_con_t*)pkg_malloc(sizeof(km_redis_con_t));
	if (!ptr) {
		LM_ERR("no private memory left\n");
		return 0;
	}

	memset(ptr, 0, sizeof(km_redis_con_t));
	ptr->ref = 1;

   	/* get the first token */
   	token = strtok(string_redis_con_url, token_char);
   	/* walk through other tokens */
   	while( token != NULL ) 
   	{
      		redis_socket_info[i]=token;
      		i++;
      		printf( " %s\n", token );
      		token = strtok(NULL, token_char);
   	}
	LM_INFO("redis server url is %s",id->url.s);
	
	ptr->con = redisConnectWithTimeout(redis_socket_info[2], atoi(redis_socket_info[3]),timeout);
	if (ptr->con != NULL && ptr->con->err) {
    	LM_ERR("Error: %s\n", ptr->con->errstr);
   	 // handle error
	} else {
    	LM_INFO("Connected to Redis\n");
	}

	ptr->reply = redisCommand(ptr->con, "AUTH %s",redis_socket_info[1]);
	freeReplyObject(ptr->reply);

	ptr->id = (struct db_id*)id;
	return ptr;

}


/*! \brief
 * Close the connection and release memory
 */
void db_redis_free_connection(struct pool_con* con)
{
	struct km_redis_con * _c;

        if (!con) return;

        _c = (struct km_redis_con*) con;

        if (_c->id) free_db_id(_c->id);
        if (_c->con) {
                redisFree(_c->con);
             //   pkg_free(_c->con);
        }
        pkg_free(_c);
	
}

/**
 * Copyright 2016 (C) Federico Cabiddu <federico.cabiddu@gmail.com>
 * Copyright 2016 (C) Giacomo Vacca <giacomo.vacca@gmail.com>
 * Copyright 2016 (C) Orange - Camille Oudot <camille.oudot@orange.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*! \file
 * \brief  Kamailio http_async_client :: Multi interface
 * \ingroup http_async_client
 */


#ifndef _HTTP_MULTI_
#define _HTTP_MULTI_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/poll.h>
#include <curl/curl.h>
#include <event2/event.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include "../../core/counters.h"
#include "hm_hash.h"


extern stat_var *requests;
extern stat_var *replies;
extern stat_var *errors;
extern stat_var *timeouts;
extern int tls_version;
extern int curl_verbose;

void set_curl_mem_callbacks(void);
int init_http_multi();
int multi_timer_cb(CURLM *multi, long timeout_ms, struct http_m_global *g);
void timer_cb(int fd, short kind, void *userp);
int sock_cb(CURL *e, curl_socket_t s, int what, void *cbp, void *sockp);
int check_mcode(CURLMcode code, char *error);
int new_request(str *query, http_m_params_t *query_params, http_multi_cbe_t cb, void *param);
void check_multi_info(struct http_m_global *g);
void setsock(struct http_m_cell *cell, curl_socket_t s, CURL* e, int act);
void addsock(curl_socket_t s, CURL *easy, int action, struct http_m_global *g);
void event_cb(int fd, short kind, void *userp);
void reply_error(struct http_m_cell *cell);

#endif

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
 * \brief  Kamailio http_async_client :: Hash functions
 * \ingroup http_async_client
 */


#ifndef _HM_HASH_
#define _HM_HASH_

#include <curl/curl.h>
#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/ut.h"
#include "../../core/hashes.h"

extern struct http_m_table *hm_table;

typedef struct http_m_reply
{
	long retcode;
	str *result;
	char error[CURL_ERROR_SIZE];
} http_m_reply_t;

typedef void (*http_multi_cbe_t)(struct http_m_reply *reply, void *param);

typedef struct http_m_request
{
	http_multi_cbe_t cb;
	char *url;
	str *result;
	void *param;
} http_m_request_t;

typedef struct http_m_global
{
	struct event_base *evbase;
	struct event *timer_event;
	CURLM *multi;
	int still_running;
} http_m_global_t;

typedef struct hm_params {
	int timeout;
	int tls_verify_host;
	int tls_verify_peer;
	struct curl_slist* headers;
	int method;
	char* tls_client_cert;
	char* tls_client_key;
	char* tls_ca_path;
	str body;
	
	unsigned int authmethod;
	char* username;
	char* password;
	int tcp_keepalive;
	int tcp_ka_idle;
	int tcp_ka_interval;
} http_m_params_t;

typedef struct http_m_cell
{
	struct http_m_cell	*next;
	struct http_m_cell	*prev;
	//unsigned int 		hmt_id;
	unsigned int 		hmt_entry;

	struct http_m_global *global;

	CURL *easy;
	curl_socket_t sockfd;
	int action;
	http_m_params_t params;

	struct event *ev;
	int evset;

	char *url;
	char error[CURL_ERROR_SIZE];

	http_multi_cbe_t cb;
	void *param;

	struct http_m_reply *reply;
} http_m_cell_t;

typedef struct http_m_entry
{
	struct http_m_cell 	*first;
	struct http_m_cell 	*last;
} http_m_entry_t;

/*! main http multi table */
typedef struct http_m_table
{
	unsigned int 		size;
	struct http_m_entry	*entries;
} http_m_table_t;

int init_http_m_table(unsigned int size);
struct http_m_cell* build_http_m_cell(void *p);
void link_http_m_cell(struct http_m_cell *cell);
struct http_m_cell *http_m_cell_lookup(CURL *p);
void free_http_m_cell(struct http_m_cell *cell);

static inline void unlink_http_m_cell(struct http_m_cell *hmt_cell)
{
	struct http_m_entry	*hmt_entry;

	if (hmt_cell) {
		hmt_entry = &(hm_table->entries[hmt_cell->hmt_entry]);
		if (hmt_cell->next)
			hmt_cell->next->prev = hmt_cell->prev;
		else
			hmt_entry->last = hmt_cell->prev;
		if (hmt_cell->prev)
			hmt_cell->prev->next = hmt_cell->next;
		else
			hmt_entry->first = hmt_cell->next;

		hmt_cell->next = hmt_cell->prev = 0;
	}
	return;
}
#endif

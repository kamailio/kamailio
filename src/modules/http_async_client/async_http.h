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
 * \brief  Kamailio http_async_client :: Async HTTP
 * \ingroup http_async_client
 */


#ifndef _ASYNC_HTTP_
#define _ASYNC_HTTP_

#define RC_AVP_NAME "http_rc"
#define RC_AVP_NAME_LENGTH 7
#define RB_AVP_NAME "http_rb"
#define RB_AVP_NAME_LENGTH 7
#define ERROR_AVP_NAME "http_error"
#define ERROR_AVP_NAME_LENGTH 10
#define MAX_ID_LEN 32
#define MAX_CBNAME_LEN	64

#include <curl/curl.h>
#include <event2/event.h>

#include "../../core/pvar.h"

#include "http_multi.h"

extern int num_workers;

extern int http_timeout; /* query timeout in ms */
extern int tcp_keepalive; 
extern int tcp_ka_idle; 
extern int tcp_ka_interval; 

extern struct sip_msg *ah_reply;
extern str ah_error;

extern int tls_verify_host;
extern int tls_verify_peer;
extern char* tls_client_cert;
extern char* tls_client_key;
extern char* tls_ca_path;

extern unsigned int default_authmethod;

typedef struct async_http_worker {
	int notication_socket[2];
	struct event_base *evbase;
	struct event *socket_event;
	struct http_m_global *g;
} async_http_worker_t;

extern async_http_worker_t *workers;

typedef enum {
	AH_METH_DEFAULT = 0,
	AH_METH_GET, AH_METH_POST, AH_METH_PUT, AH_METH_DELETE
} async_http_method_t;

struct header_list {
	char ** t;
	int len;
};

struct query_params {
	async_http_method_t method:3;
	unsigned int tls_verify_peer:1;
	unsigned int tls_verify_host:1;
	unsigned int suspend_transaction:1; /* (create and) suspend the current transaction */
	unsigned int call_route:1;          /* call script route on reply */

	unsigned int timeout;
	struct header_list headers;
	char* tls_client_cert;
	char* tls_client_key;
	char* tls_ca_path;
	str body;

	unsigned int authmethod;
	char* username;
	char* password;
	unsigned int tcp_keepalive;
	unsigned int tcp_ka_idle;
	unsigned int tcp_ka_interval;
};

extern struct query_params ah_params;

typedef struct async_query {
	str query;
	char id[MAX_ID_LEN+1];
	unsigned int tindex;
	unsigned int tlabel;
	struct query_params query_params;
	char cbname[MAX_CBNAME_LEN];
	int cbname_len;
} async_query_t;

int async_http_init_sockets(async_http_worker_t *worker);
int async_http_init_worker(int prank, async_http_worker_t* worker);
void async_http_run_worker(async_http_worker_t* worker);

int async_send_query(sip_msg_t *msg, str *query, str *cbname);
int async_push_query(async_query_t *aq);

void notification_socket_cb(int fd, short event, void *arg);
int init_socket(async_http_worker_t* worker);
void async_http_cb(struct http_m_reply *reply, void *param);
void init_query_params(struct query_params*);
void set_query_params(struct query_params*);

int header_list_add(struct header_list *hl, str* hdr);
int query_params_set_method(struct query_params *qp, str *meth);

static inline void free_async_query(async_query_t *aq)
{
	if (!aq)
		return;
	LM_DBG("freeing query %p\n", aq);
	if (aq->query.s && aq->query.len) {
		shm_free(aq->query.s);
		aq->query.s=0;
		aq->query.len=0;
	}

	if(aq->query_params.headers.t) {
		while(aq->query_params.headers.len--)
			shm_free(aq->query_params.headers.t[aq->query_params.headers.len]);
		shm_free(aq->query_params.headers.t);
	}

	if (aq->query_params.tls_client_cert) {
		shm_free(aq->query_params.tls_client_cert);
		aq->query_params.tls_client_cert = NULL;
	}

	if (aq->query_params.tls_client_key) {
		shm_free(aq->query_params.tls_client_key);
		aq->query_params.tls_client_key = NULL;
	}

	if (aq->query_params.tls_ca_path) {
		shm_free(aq->query_params.tls_ca_path);
		aq->query_params.tls_ca_path = NULL;
	}

	if (aq->query_params.body.s && aq->query_params.body.len > 0) {
		shm_free(aq->query_params.body.s);
		aq->query_params.body.s = NULL;
		aq->query_params.body.len = 0;
	}

	if (aq->query_params.username) {
		shm_free(aq->query_params.username);
		aq->query_params.username = NULL;
	}

	if (aq->query_params.password) {
		shm_free(aq->query_params.password);
		aq->query_params.password = NULL;
	}

	shm_free(aq);
}

#endif

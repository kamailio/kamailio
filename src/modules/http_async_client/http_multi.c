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
 * \brief  Kamailio http_async_client :: multi interface
 * \ingroup http_async_client
 */


#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/ut.h"
#include "../../core/hashes.h"
#include "http_multi.h"

extern int hash_size;
/*! global http multi table */
struct http_m_table *hm_table = 0;
struct http_m_global *g = 0;

/* 0: shm, 1:system malloc */
int curl_memory_manager = 0;

/* Update the event timer after curl_multi library calls */
int multi_timer_cb(CURLM *multi, long timeout_ms, struct http_m_global *g)
{
	struct timeval timeout;
	(void)multi; /* unused */

	timeout.tv_sec = timeout_ms / 1000;
	timeout.tv_usec = (timeout_ms % 1000) * 1000;
	LM_DBG("multi_timer_cb: Setting timeout to %ld ms\n", timeout_ms);
	evtimer_add(g->timer_event, &timeout);
	return 0;
}
/* Called by libevent when our timeout expires */
void timer_cb(int fd, short kind, void *userp)
{
	struct http_m_global *g = (struct http_m_global *)userp;
	CURLMcode rc;
	(void)fd;
	(void)kind;

	char error[CURL_ERROR_SIZE];

	LM_DBG("timeout on socket %d\n", fd);

	rc = curl_multi_socket_action(
			g->multi, CURL_SOCKET_TIMEOUT, 0, &g->still_running);

	if(check_mcode(rc, error) < 0) {
		LM_ERR("curl_multi_socket_action error: %s", error);
	}

	check_multi_info(g);
}
/* Called by libevent when we get action on a multi socket */
void event_cb(int fd, short kind, void *userp)
{
	struct http_m_global *g;
	CURLMcode rc;
	CURL *easy = (CURL *)userp;
	struct http_m_cell *cell;

	cell = http_m_cell_lookup(easy);
	if(cell == NULL) {
		LM_INFO("Cell for handler %p not found in table\n", easy);
		return;
	}

	g = cell->global;
	int action = (kind & EV_READ ? CURL_CSELECT_IN : 0)
				 | (kind & EV_WRITE ? CURL_CSELECT_OUT : 0);

	LM_DBG("activity %d on socket %d: action %d\n", kind, fd, action);
	if(kind == EV_TIMEOUT) {
		LM_DBG("handle %p timeout on socket %d (cell=%p, param=%p)\n",
				cell->easy, fd, cell, cell->param);
		update_stat(timeouts, 1);
		const char *error = "TIMEOUT";

		strncpy(cell->error, error, strlen(error) + 1);

		reply_error(cell);

		easy = cell->easy;
		/* we are going to remove the cell and the handle here:
		   pass NULL as sockptr */
		curl_multi_assign(g->multi, cell->sockfd, NULL);

		LM_DBG("cleaning up cell %p\n", cell);
		if(cell->evset && cell->ev) {
			LM_DBG("freeing event %p\n", cell->ev);
			event_del(cell->ev);
			event_free(cell->ev);
			cell->ev = NULL;
			cell->evset = 0;
		}
		unlink_http_m_cell(cell);
		free_http_m_cell(cell);

		LM_DBG("removing handle %p\n", easy);
		curl_multi_remove_handle(g->multi, easy);
		curl_easy_cleanup(easy);
		rc = curl_multi_socket_action(
				g->multi, CURL_SOCKET_TIMEOUT, 0, &g->still_running);

	} else {
		LM_DBG("performing action %d on socket %d\n", action, fd);
		rc = curl_multi_socket_action(g->multi, fd, action, &g->still_running);
		LM_DBG("action %d on socket %d performed\n", action, fd);

		if(rc == CURLM_CALL_MULTI_PERFORM) {
			LM_DBG("received CURLM_CALL_MULTI_PERFORM, performing action "
				   "again\n");
			rc = curl_multi_socket_action(
					g->multi, fd, action, &g->still_running);
		}
		if(check_mcode(rc, cell->error) < 0) {
			LM_ERR("error: %s\n", cell->error);
			reply_error(cell);
			curl_multi_remove_handle(g->multi, easy);
			curl_easy_cleanup(easy);
		}
	}

	check_multi_info(g);
	if(g->still_running <= 0) {
		LM_DBG("last transfer done, kill timeout\n");
		if(evtimer_pending(g->timer_event, NULL)) {
			evtimer_del(g->timer_event);
		}
	}
}

/* CURLMOPT_SOCKETFUNCTION */
int sock_cb(CURL *e, curl_socket_t s, int what, void *cbp, void *sockp)
{
	struct http_m_global *g = (struct http_m_global *)cbp;
	struct http_m_cell *cell = (struct http_m_cell *)sockp;
	const char *whatstr[] = {"none", "IN", "OUT", "INOUT", "REMOVE"};

	LM_DBG("socket callback: s=%d e=%p what=%s\n", s, e, whatstr[what]);
	if(what == CURL_POLL_REMOVE) {
		/* if cell is NULL the handle has been removed by the event callback for timeout */
		if(cell) {
			if(cell->evset && cell->ev) {
				LM_DBG("freeing event %p\n", cell->ev);
				event_del(cell->ev);
				event_free(cell->ev);
				cell->ev = NULL;
				cell->evset = 0;
			}
		} else {
			LM_DBG("REMOVE action without cell, handler timed out.\n");
		}
	} else {
		if(!cell) {
			LM_DBG("Adding data: %s\n", whatstr[what]);
			addsock(s, e, what, g);
		} else {
			LM_DBG("Changing action from %s to %s\n", whatstr[cell->action],
					whatstr[what]);
			setsock(cell, s, e, what);
		}
	}
	return 0;
}
int check_mcode(CURLMcode code, char *error)
{
	const char *s;
	if(CURLM_OK != code && CURLM_CALL_MULTI_PERFORM != code) {
		switch(code) {
			case CURLM_BAD_HANDLE:
				s = "CURLM_BAD_HANDLE";
				break;
			case CURLM_BAD_EASY_HANDLE:
				s = "CURLM_BAD_EASY_HANDLE";
				break;
			case CURLM_OUT_OF_MEMORY:
				s = "CURLM_OUT_OF_MEMORY";
				break;
			case CURLM_INTERNAL_ERROR:
				s = "CURLM_INTERNAL_ERROR";
				break;
			case CURLM_UNKNOWN_OPTION:
				s = "CURLM_UNKNOWN_OPTION";
				break;
			case CURLM_LAST:
				s = "CURLM_LAST";
				break;
			case CURLM_BAD_SOCKET:
				s = "CURLM_BAD_SOCKET";
				break;
			default:
				s = "CURLM_unknown";
				break;
		}
		LM_ERR("ERROR: %s\n", s);
		strncpy(error, s, strlen(s) + 1);
		return -1;
	}
	return 0;
}

/* CURLOPT_DEBUGFUNCTION */
int debug_cb(CURL *handle, curl_infotype type, char *data, size_t size,
		void *userptr)
{
	char *prefix;
	switch(type) {
		case CURLINFO_TEXT:
			prefix = "[cURL]";
			break;
		case CURLINFO_HEADER_IN:
			prefix = "[cURL hdr in]";
			break;
		case CURLINFO_HEADER_OUT:
			prefix = "[cURL hdr out]";
			break;
		case CURLINFO_DATA_IN:
		case CURLINFO_DATA_OUT:
		case CURLINFO_SSL_DATA_OUT:
		case CURLINFO_SSL_DATA_IN:
		default:
			return 0;
			break;
	}
	LM_INFO("%s %.*s" /* cURL includes final \n */, prefix, (int)size, data);
	return 0;
}
/* CURLOPT_WRITEFUNCTION */
size_t write_cb(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t realsize = size * nmemb;
	struct http_m_cell *cell;
	CURL *easy = (CURL *)data;
	int old_len;

	LM_DBG("data received: %.*s [%d]\n", (int)realsize, (char *)ptr,
			(int)realsize);

	cell = http_m_cell_lookup(easy);
	if(cell == NULL) {
		LM_ERR("Cell for handler %p not found in table\n", easy);
		return -1;
	}

	if(cell->reply == NULL) {
		cell->reply =
				(struct http_m_reply *)shm_malloc(sizeof(struct http_m_reply));
		if(cell->reply == NULL) {
			LM_ERR("Cannot allocate shm memory for reply\n");
			return -1;
		}
		memset(cell->reply, 0, sizeof(struct http_m_reply));
		cell->reply->result = (str *)shm_malloc(sizeof(str));
		if(cell->reply->result == NULL) {
			LM_ERR("Cannot allocate shm memory for reply's result\n");
			shm_free(cell->reply);
			return -1;
		}
		memset(cell->reply->result, 0, sizeof(str));
	}

	old_len = cell->reply->result->len;
	cell->reply->result->len += realsize;
	cell->reply->result->s = (char *)shm_reallocxf(
			cell->reply->result->s, cell->reply->result->len);
	if(cell->reply->result->s == NULL) {
		LM_ERR("Cannot allocate shm memory for reply's result\n");
		shm_free(cell->reply->result);
		shm_free(cell->reply);
		cell->reply = NULL;
		return -1;
	}
	strncpy(cell->reply->result->s + old_len, ptr,
			cell->reply->result->len - old_len);

	if(cell->easy == NULL) { /* TODO: when does this happen? */
		LM_DBG("cell %p easy handler is null\n", cell);
	} else {
		LM_DBG("getting easy handler info (%p)\n", cell->easy);
		curl_easy_getinfo(
				cell->easy, CURLINFO_HTTP_CODE, &cell->reply->retcode);
	}

	return realsize;
}

void reply_error(struct http_m_cell *cell)
{
	struct http_m_reply *reply;
	LM_DBG("replying error for  cell=%p\n", cell);

	reply = (struct http_m_reply *)pkg_malloc(sizeof(struct http_m_reply));
	if(reply == NULL) {
		LM_ERR("Cannot allocate pkg memory for reply's result\n");
		return;
	}
	memset(reply, 0, sizeof(struct http_m_reply));
	reply->result = NULL;
	reply->retcode = 0;

	if(cell) {
		strncpy(reply->error, cell->error, strlen(cell->error));
		reply->error[strlen(cell->error)] = '\0';
	} else {
		reply->error[0] = '\0';
	}

	if(cell) {
		cell->cb(reply, cell->param);
	}

	pkg_free(reply);

	return;
}

static void *curl_shm_malloc(size_t size)
{
	void *p = shm_malloc(size);
	return p;
}
static void curl_shm_free(void *ptr)
{
	if(ptr)
		shm_free(ptr);
}

static void *curl_shm_realloc(void *ptr, size_t size)
{
	void *p = shm_realloc(ptr, size);

	return p;
}

static void *curl_shm_calloc(size_t nmemb, size_t size)
{
	void *p = shm_malloc(nmemb * size);
	if(p)
		memset(p, '\0', nmemb * size);

	return p;
}

static char *curl_shm_strdup(const char *cp)
{
	char *p = shm_char_dup(cp);

	return p;
}

void set_curl_mem_callbacks(void)
{
	CURLcode rc;

	switch(curl_memory_manager) {
		case 0:
			LM_DBG("Setting shm memory callbacks for cURL\n");
			rc = curl_global_init_mem(CURL_GLOBAL_ALL, curl_shm_malloc,
					curl_shm_free, curl_shm_realloc, curl_shm_strdup,
					curl_shm_calloc);
			if(rc != 0) {
				LM_ERR("Cannot set memory callbacks for cURL: %d\n", rc);
			}
			break;
		case 1:
			LM_DBG("Initilizing cURL with sys malloc\n");
			rc = curl_global_init(CURL_GLOBAL_ALL);
			if(rc != 0) {
				LM_ERR("Cannot initialize cURL: %d\n", rc);
			}
			break;
		default:
			LM_ERR("invalid memory manager: %d\n", curl_memory_manager);
			break;
	}
}

int init_http_multi(struct event_base *evbase, struct http_m_global *wg)
{
	g = wg;
	g->evbase = evbase;


	g->multi = curl_multi_init();
	LM_DBG("curl_multi %p initialized on global %p (evbase %p)\n", g->multi, g,
			evbase);

	g->timer_event = evtimer_new(g->evbase, timer_cb, g);

	/* setup the generic multi interface options we want */
	curl_multi_setopt(g->multi, CURLMOPT_SOCKETFUNCTION, sock_cb);
	curl_multi_setopt(g->multi, CURLMOPT_SOCKETDATA, g);
	curl_multi_setopt(g->multi, CURLMOPT_TIMERFUNCTION, multi_timer_cb);
	curl_multi_setopt(g->multi, CURLMOPT_TIMERDATA, g);
	curl_multi_setopt(g->multi, CURLMOPT_PIPELINING, CURLPIPE_NOTHING);

	return init_http_m_table(hash_size);
}

int new_request(str *query, http_m_params_t *query_params, http_multi_cbe_t cb,
		void *param)
{

	LM_DBG("received query %.*s with timeout %d, tls_verify_peer %d, "
		   "tls_verify_host %d (param=%p)\n",
			query->len, query->s, query_params->timeout,
			query_params->tls_verify_peer, query_params->tls_verify_host,
			param);

	CURL *easy;
	CURLMcode rc;

	struct http_m_cell *cell;

	update_stat(requests, 1);

	easy = NULL;
	cell = NULL;

	easy = curl_easy_init();
	if(!easy) {
		LM_ERR("curl_easy_init() failed!\n");
		update_stat(errors, 1);
		return -1;
	}

	cell = build_http_m_cell(easy);
	if(!cell) {
		LM_ERR("cannot create cell!\n");
		update_stat(errors, 1);
		LM_DBG("cleaning up curl handler %p\n", easy);
		curl_easy_cleanup(easy);
		return -1;
	}

	link_http_m_cell(cell);

	cell->global = g;
	cell->easy = easy;
	cell->error[0] = '\0';
	cell->params = *query_params;
	cell->param = param;
	cell->cb = cb;
	cell->url = (char *)shm_malloc(query->len + 1);
	if(cell->url == 0) {
		LM_ERR("no more shm mem\n");
		goto error;
	}
	strncpy(cell->url, query->s, query->len);
	cell->url[query->len] = '\0';

	curl_easy_setopt(cell->easy, CURLOPT_URL, cell->url);
	curl_easy_setopt(cell->easy, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(cell->easy, CURLOPT_WRITEDATA, easy);
	if(curl_verbose) {
		curl_easy_setopt(cell->easy, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(cell->easy, CURLOPT_DEBUGFUNCTION, debug_cb);
	}
	if(cell->params.follow_redirect) {
		curl_easy_setopt(cell->easy, CURLOPT_FOLLOWLOCATION, 1L);
	}
	curl_easy_setopt(cell->easy, CURLOPT_ERRORBUFFER, cell->error);
	curl_easy_setopt(cell->easy, CURLOPT_PRIVATE, cell);
	curl_easy_setopt(
			cell->easy, CURLOPT_SSL_VERIFYPEER, cell->params.tls_verify_peer);
	curl_easy_setopt(cell->easy, CURLOPT_SSL_VERIFYHOST,
			cell->params.tls_verify_host ? 2 : 0);
	curl_easy_setopt(cell->easy, CURLOPT_SSLVERSION, tls_version);

	if(cell->params.tls_client_cert) {
		curl_easy_setopt(
				cell->easy, CURLOPT_SSLCERT, cell->params.tls_client_cert);
	}

	if(cell->params.tls_client_key) {
		curl_easy_setopt(
				cell->easy, CURLOPT_SSLKEY, cell->params.tls_client_key);
	}

	if(cell->params.tls_ca_path) {
		curl_easy_setopt(cell->easy, CURLOPT_CAPATH, cell->params.tls_ca_path);
	}

	curl_easy_setopt(cell->easy, CURLOPT_HEADER, 1);
	if(cell->params.headers) {
		curl_easy_setopt(cell->easy, CURLOPT_HTTPHEADER, cell->params.headers);
	}

	if(cell->params.body.s && cell->params.body.len) {
		curl_easy_setopt(
				cell->easy, CURLOPT_POSTFIELDSIZE, (long)cell->params.body.len);
		curl_easy_setopt(
				cell->easy, CURLOPT_COPYPOSTFIELDS, cell->params.body.s);
	}

	switch(cell->params.method) {
		case 1:
			curl_easy_setopt(cell->easy, CURLOPT_CUSTOMREQUEST, "GET");
			break;
		case 2:
			curl_easy_setopt(cell->easy, CURLOPT_CUSTOMREQUEST, "POST");
			break;
		case 3:
			curl_easy_setopt(cell->easy, CURLOPT_CUSTOMREQUEST, "PUT");
			break;
		case 4:
			curl_easy_setopt(cell->easy, CURLOPT_CUSTOMREQUEST, "DELETE");
			break;
		default:
			break;
	}

	if(cell->params.username) {
		curl_easy_setopt(cell->easy, CURLOPT_USERNAME, cell->params.username);
		curl_easy_setopt(cell->easy, CURLOPT_HTTPAUTH, cell->params.authmethod);

		LM_DBG("set username to %s [authmethod %u]\n", cell->params.username,
				cell->params.authmethod);
	}

	if(cell->params.password) {
		curl_easy_setopt(cell->easy, CURLOPT_PASSWORD, cell->params.password);
	}

	/* enable tcp keepalives for the handler */
	if(cell->params.tcp_keepalive) {
#ifdef CURLOPT_TCP_KEEPALIVE
		LM_DBG("Enabling TCP keepalives\n");
		curl_easy_setopt(cell->easy, CURLOPT_TCP_KEEPALIVE, 1L);

		if(cell->params.tcp_ka_idle) {
			curl_easy_setopt(
					cell->easy, CURLOPT_TCP_KEEPIDLE, cell->params.tcp_ka_idle);
			LM_DBG("CURLOPT_TCP_KEEPIDLE set to %d\n",
					cell->params.tcp_ka_idle);
		}

		if(cell->params.tcp_ka_interval) {
			curl_easy_setopt(cell->easy, CURLOPT_TCP_KEEPINTVL,
					cell->params.tcp_ka_interval);
			LM_DBG("CURLOPT_TCP_KEEPINTERVAL set to %d\n",
					cell->params.tcp_ka_interval);
		}
#else
		LM_DBG("tcp_keepalive configured, but installed cURL version doesn't "
			   "include CURLOPT_TCP_KEEPINTERVAL.\n");
#endif
	}

	LM_DBG("Adding easy %p to multi %p (%.*s)\n", cell->easy, g->multi,
			query->len, query->s);
	rc = curl_multi_add_handle(g->multi, cell->easy);
	if(check_mcode(rc, cell->error) < 0) {
		LM_ERR("error adding curl handler: %s\n", cell->error);
		goto error;
	}
	/* note that the add_handle() will set a time-out to trigger very soon so
	 *      that the necessary socket_action() call will be called by this app */
	return 0;

error:
	update_stat(errors, 1);
	if(easy) {
		LM_DBG("cleaning up curl handler %p\n", easy);
		curl_easy_cleanup(easy);
	}
	free_http_m_cell(cell);
	return -1;
}

/* Check for completed transfers, and remove their easy handles */
void check_multi_info(struct http_m_global *g)
{
	char *eff_url;
	CURLMsg *msg;
	int msgs_left;
	CURL *easy;
	CURLcode res;

	struct http_m_cell *cell;
	double tmp_time;

	LM_DBG("REMAINING: %d\n", g->still_running);
	while((msg = curl_multi_info_read(g->multi, &msgs_left))) {
		if(msg->msg == CURLMSG_DONE) {
			easy = msg->easy_handle;
			res = msg->data.result;
			curl_easy_getinfo(easy, CURLINFO_PRIVATE, &cell);
			curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &eff_url);


			LM_DBG("DONE: %s => (%d) %s\n", eff_url, res, cell->error);

			cell = http_m_cell_lookup(easy);
			if(msg->data.result != 0) {
				LM_ERR("handle %p returned error %d: %s\n", easy, res,
						cell->error);
				update_stat(errors, 1);
				reply_error(cell);
			} else {

				if(curl_easy_getinfo(cell->easy, CURLINFO_TOTAL_TIME, &tmp_time)
						== CURLE_OK)
					cell->reply->time.total = (uint32_t)(tmp_time * 1000000);
				if(curl_easy_getinfo(
						   cell->easy, CURLINFO_NAMELOOKUP_TIME, &tmp_time)
						== CURLE_OK)
					cell->reply->time.lookup = (uint32_t)(tmp_time * 1000000);
				if(curl_easy_getinfo(
						   cell->easy, CURLINFO_CONNECT_TIME, &tmp_time)
						== CURLE_OK)
					cell->reply->time.connect = (uint32_t)(tmp_time * 1000000);
				if(curl_easy_getinfo(
						   cell->easy, CURLINFO_REDIRECT_TIME, &tmp_time)
						== CURLE_OK)
					cell->reply->time.redirect = (uint32_t)(tmp_time * 1000000);
				if(curl_easy_getinfo(
						   cell->easy, CURLINFO_APPCONNECT_TIME, &tmp_time)
						== CURLE_OK)
					cell->reply->time.appconnect =
							(uint32_t)(tmp_time * 1000000);
				if(curl_easy_getinfo(
						   cell->easy, CURLINFO_PRETRANSFER_TIME, &tmp_time)
						== CURLE_OK)
					cell->reply->time.pretransfer =
							(uint32_t)(tmp_time * 1000000);
				if(curl_easy_getinfo(
						   cell->easy, CURLINFO_STARTTRANSFER_TIME, &tmp_time)
						== CURLE_OK)
					cell->reply->time.starttransfer =
							(uint32_t)(tmp_time * 1000000);

				cell->reply->error[0] = '\0';
				cell->cb(cell->reply, cell->param);

				LM_DBG("reply: [%d] %.*s [%d]\n", (int)cell->reply->retcode,
						cell->reply->result->len, cell->reply->result->s,
						cell->reply->result->len);
				update_stat(replies, 1);
			}

			if(cell != 0) {
				LM_DBG("cleaning up cell %p\n", cell);
				unlink_http_m_cell(cell);
				free_http_m_cell(cell);
			}

			LM_DBG("Removing handle %p\n", easy);
			curl_multi_remove_handle(g->multi, easy);
			curl_easy_cleanup(easy);
		}
	}
}

/* set cell's socket information and assign an event to the socket */
void setsock(struct http_m_cell *cell, curl_socket_t s, CURL *e, int act)
{

	struct timeval timeout;

	int kind = (act & CURL_POLL_IN ? EV_READ : 0)
			   | (act & CURL_POLL_OUT ? EV_WRITE : 0) | EV_PERSIST;
	struct http_m_global *g = cell->global;
	cell->sockfd = s;
	cell->action = act;
	cell->easy = e;
	if(cell->evset && cell->ev) {
		event_del(cell->ev);
		event_free(cell->ev);
		cell->ev = NULL;
		cell->evset = 0;
	}
	cell->ev = event_new(g->evbase, cell->sockfd, kind, event_cb, e);
	LM_DBG("added event %p to socket %d\n", cell->ev, cell->sockfd);
	cell->evset = 1;


	timeout.tv_sec = cell->params.timeout / 1000;
	timeout.tv_usec = (cell->params.timeout % 1000) * 1000;

	event_add(cell->ev, &timeout);
}


/* assign a socket to the multi handler */
void addsock(curl_socket_t s, CURL *easy, int action, struct http_m_global *g)
{
	struct http_m_cell *cell;

	cell = http_m_cell_lookup(easy);
	if(!cell)
		return;
	setsock(cell, s, cell->easy, action);
	curl_multi_assign(g->multi, s, cell);
}

/**
 * Copyright (C) 2019-2026 Alexey Volokitin (didww.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm.h"
#include "../../core/utils/srjson.h"

#include "lh_async.h"
#include "lh_worker.h"
#include "lh_format.h"
#include "log_http.h"
#include "curl_defs.h"

#define LH_TIME_BUF_SIZE 32
#define LH_BODY_INIT_SIZE 4096

static lh_queue_t lh_queue = STAILQ_HEAD_INITIALIZER(lh_queue);
/* number of messages included in the request in flight */
static unsigned int lh_current_batch_size = 0;

static void *lh_srjson_malloc(size_t sz)
{
	return pkg_malloc(sz);
}

static void lh_srjson_free(void *ptr)
{
	pkg_free(ptr);
}

int lh_async_send(struct group_key *key, async_param_value_t *values, int size)
{
	int async_size =
			sizeof(async_log_entry_t) + sizeof(async_log_param_t) * size;
	int i = 0;
	char *p;
	async_log_entry_t *msg;
	async_log_param_t **link;
	struct log_grp_param *param;

	SLIST_FOREACH(param, &key->params.default_params.head, next)
	{
		async_size += param->name.len + 1;
		if(values[i].type == LPT_STRING)
			async_size += values[i].u.string.len + 1;
		i++;
	}
	SLIST_FOREACH(param, &key->params.params.head, next)
	{
		async_size += param->name.len + 1;
		if(values[i].type == LPT_STRING)
			async_size += values[i].u.string.len + 1;
		i++;
	}

	msg = (async_log_entry_t *)shm_malloc(async_size);
	if(msg == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}

	msg->size = size;
	SLIST_INIT(&msg->head);
	link = &SLIST_FIRST(&msg->head);
	p = (char *)msg + sizeof(async_log_entry_t);
	param = SLIST_FIRST(&key->params.default_params.head);
	for(i = 0; i < size; i++) {
		async_log_param_t *np = (async_log_param_t *)p;
		if(!param)
			param = SLIST_FIRST(&key->params.params.head);
		p += sizeof(async_log_param_t);

		np->name.s = p;
		np->name.len = param->name.len;
		memcpy(np->name.s, param->name.s, param->name.len);
		np->name.s[np->name.len] = '\0';
		p += np->name.len + 1;

		np->value = values[i];
		if(np->value.type == LPT_STRING) {
			np->value.u.string.s = p;
			memcpy(np->value.u.string.s, values[i].u.string.s,
					values[i].u.string.len);
			np->value.u.string.s[np->value.u.string.len] = '\0';
			p += np->value.u.string.len + 1;
		}

		/* append to the tail of the list */
		SLIST_NEXT(np, next) = 0;
		*link = np;
		link = &SLIST_NEXT(np, next);
		param = SLIST_NEXT(param, next);
	}

	if(lh_worker_send(msg)) {
		return 1;
	}

	shm_free(msg);
	return -1;
}

/* format unix time in milliseconds as RFC3339 with ms precision (UTC) */
static int lh_time_rfc3339(uint64_t ms, char *buf, size_t len)
{
	time_t sec = (time_t)(ms / 1000);
	struct tm tm_;
	size_t n;

	if(!gmtime_r(&sec, &tm_))
		return -1;
	n = strftime(buf, len, "%Y-%m-%dT%H:%M:%S", &tm_);
	if(!n)
		return -1;
	n += snprintf(buf + n, len - n, ".%03uZ", (unsigned int)(ms % 1000));
	return (int)n;
}

bool lh_add_message_to_queue(async_log_entry_t *message)
{
	srjson_doc_t jdoc;
	srjson_Hooks hooks = {lh_srjson_malloc, lh_srjson_free};
	async_log_param_t *param;
	struct lh_message *jmsg;
	char *jmsg_str;

	if(lh_queue_size && get_stat_val(lh_stat_queue_size) > lh_queue_size) {
		update_stat(lh_stat_drop_total, 1);
		LM_ERR("number of queued messages exceeds %u, dropping\n",
				lh_queue_size);
		/* trigger sending if we started to drop messages */
		return true;
	}

	srjson_InitDoc(&jdoc, &hooks);
	jdoc.root = srjson_CreateObject(&jdoc);
	if(!jdoc.root) {
		PKG_MEM_ERROR;
		goto done;
	}

	SLIST_FOREACH(param, &message->head, next)
	{
		srjson_t *value = NULL;
		char buf[INET6_ADDRSTRLEN > LH_TIME_BUF_SIZE ? INET6_ADDRSTRLEN
													 : LH_TIME_BUF_SIZE];

		switch(param->value.type) {
			case LPT_STRING:
				if(!param->value.u.string.len)
					continue;
				value = srjson_CreateStr(&jdoc, param->value.u.string.s,
						param->value.u.string.len);
				break;
			case LPT_NUMBER:
				value = srjson_CreateNumber(&jdoc, param->value.u.num);
				break;
			case LPT_IPADDR:
				if(!inet_ntop(param->value.u.addr.af,
						   param->value.u.addr.u.addr, buf, sizeof(buf)))
					continue;
				value = srjson_CreateString(&jdoc, buf);
				break;
			case LPT_BOOL:
				value = srjson_CreateBool(&jdoc, param->value.u.cond);
				break;
			case LPT_DATE:
				if(lh_time_rfc3339(param->value.u.date, buf, sizeof(buf)) < 0)
					continue;
				value = srjson_CreateString(&jdoc, buf);
				break;
			case LPT_FLOAT:
				value = srjson_CreateNumber(&jdoc, param->value.u.flp);
				break;
			default:
				continue;
		}
		if(!value) {
			PKG_MEM_ERROR;
			srjson_DestroyDoc(&jdoc);
			goto done;
		}
		srjson_AddStrItemToObject(
				&jdoc, jdoc.root, param->name.s, param->name.len, value);
	}

	jmsg_str = srjson_PrintUnformatted(&jdoc, jdoc.root);
	srjson_DestroyDoc(&jdoc);
	if(!jmsg_str) {
		LM_ERR("failed to serialize log entry to json\n");
		goto done;
	}

	jmsg = pkg_mallocxz(sizeof(struct lh_message));
	if(!jmsg) {
		PKG_MEM_ERROR;
		pkg_free(jmsg_str);
		goto done;
	}
	jmsg->msg_str.s = jmsg_str;
	jmsg->msg_str.len = strlen(jmsg_str);

	STAILQ_INSERT_TAIL(&lh_queue, jmsg, next);
	update_stat(lh_stat_queue_size, 1);

done:
	return (get_stat_val(lh_stat_queue_size) > lh_batch_size);
}

static void lh_free_message(struct lh_message *jmsg)
{
	if(jmsg->msg_str.s)
		pkg_free(jmsg->msg_str.s);
	pkg_free(jmsg);
}

void lh_free_queue(void)
{
	while(!STAILQ_EMPTY(&lh_queue)) {
		struct lh_message *jmsg = STAILQ_FIRST(&lh_queue);

		STAILQ_REMOVE_HEAD(&lh_queue, next);
		lh_free_message(jmsg);
	}

	reset_stat(lh_stat_queue_size);
}

void lh_send_queue_messages(void)
{
	struct lh_target *target = lh_get_next_target();

	if(!target)
		return;

	if(lh_curl_send_message(target, 0) != 0) {
		update_stat(lh_stat_err_total, 1);
		LM_ERR("failed to start sending log messages\n");
	}
}

void lh_send_queue_messages_fin(bool success)
{
	if(success) {
		while(!STAILQ_EMPTY(&lh_queue) && lh_current_batch_size) {
			struct lh_message *jmsg = STAILQ_FIRST(&lh_queue);
			STAILQ_REMOVE_HEAD(&lh_queue, next);
			update_stat(lh_stat_queue_size, -1);
			--lh_current_batch_size;
			lh_free_message(jmsg);
		}
	}

	lh_current_batch_size = 0;
}

/* discard the http response body */
static size_t lh_write_func(char *ptr, size_t size, size_t nmemb, void *data)
{
	(void)ptr;
	(void)data;
	return size * nmemb;
}

int lh_body_append(lh_body_t *body, const char *data, int len)
{
	char *tmp;
	int size;

	if(len <= 0)
		return 0;
	if(body->len + len + 1 > body->size) {
		size = body->size ? body->size : LH_BODY_INIT_SIZE;
		while(body->len + len + 1 > size)
			size *= 2;
		tmp = pkg_realloc(body->s, size);
		if(!tmp) {
			PKG_MEM_ERROR;
			return -1;
		}
		body->s = tmp;
		body->size = size;
	}
	memcpy(body->s + body->len, data, len);
	body->len += len;
	body->s[body->len] = '\0';
	return 0;
}

enum
{
	LH_BODY_BEGIN,
	LH_BODY_ENTRIES,
	LH_BODY_END,
	LH_BODY_DONE
};

/* reset the body generator to the beginning of the batch */
static void lh_body_rewind(struct curl_conn_handle *conn, unsigned int count)
{
	conn->body.len = 0;
	conn->body_off = 0;
	conn->body_state = LH_BODY_BEGIN;
	conn->next = STAILQ_FIRST(&lh_queue);
	conn->left = count;
}

/* produce the next piece of the body with the selected format:
 * returns 1 when a piece was added, 0 at the end of the body, -1 on error */
static int lh_body_next(struct curl_conn_handle *conn)
{
	switch(conn->body_state) {
		case LH_BODY_BEGIN:
			conn->body_state = LH_BODY_ENTRIES;
			if(lh_format->body_begin && lh_format->body_begin(&conn->body) < 0)
				return -1;
			return 1;
		case LH_BODY_ENTRIES:
			if(!conn->next || !conn->left) {
				conn->body_state = LH_BODY_END;
				return 1;
			}
			if(lh_format->body_entry(&conn->body, &conn->next->msg_str) < 0)
				return -1;
			conn->next = STAILQ_NEXT(conn->next, next);
			conn->left--;
			return 1;
		case LH_BODY_END:
			conn->body_state = LH_BODY_DONE;
			if(lh_format->body_end && lh_format->body_end(&conn->body) < 0)
				return -1;
			return 1;
		default:
			return 0;
	}
}

/* chunked transfer: hand the body to curl piece by piece */
static size_t lh_read_func(char *ptr, size_t size, size_t nmemb, void *data)
{
	struct curl_conn_handle *conn = data;
	size_t avail = size * nmemb;
	size_t done = 0;
	int rc;

	while(done < avail) {
		if(conn->body_off < conn->body.len) {
			size_t n = conn->body.len - conn->body_off;
			if(n > avail - done)
				n = avail - done;
			memcpy(ptr + done, conn->body.s + conn->body_off, n);
			conn->body_off += n;
			done += n;
			continue;
		}
		/* current piece consumed, generate the next one */
		conn->body.len = 0;
		conn->body_off = 0;
		rc = lh_body_next(conn);
		if(rc < 0)
			return CURL_READFUNC_ABORT;
		if(rc == 0)
			break;
	}
	return done;
}

/* curl may need to resend the body (e.g. redirect or auth round trip) */
static int lh_seek_func(void *data, curl_off_t offset, int origin)
{
	struct curl_conn_handle *conn = data;

	if(origin != SEEK_SET || offset != 0)
		return CURL_SEEKFUNC_CANTSEEK;
	lh_body_rewind(conn, lh_current_batch_size);
	return CURL_SEEKFUNC_OK;
}

/* content-length transfer: assemble the whole batch in memory */
static int lh_build_body(struct curl_conn_handle *conn)
{
	int rc;

	while((rc = lh_body_next(conn)) > 0)
		;
	return rc;
}

static int lh_curl_setup(struct curl_conn_handle *conn_handle)
{
	char ctype[64];

	easy_setopt(conn_handle->curl, CURLOPT_PRIVATE, conn_handle);
	easy_setopt(conn_handle->curl, CURLOPT_URL, conn_handle->target->url.s);
	easy_setopt(conn_handle->curl, CURLOPT_POST, 1L);
	if(lh_chunked) {
		easy_setopt(conn_handle->curl, CURLOPT_READFUNCTION, lh_read_func);
		easy_setopt(conn_handle->curl, CURLOPT_READDATA, conn_handle);
		easy_setopt(conn_handle->curl, CURLOPT_SEEKFUNCTION, lh_seek_func);
		easy_setopt(conn_handle->curl, CURLOPT_SEEKDATA, conn_handle);
	} else {
		easy_setopt(conn_handle->curl, CURLOPT_POSTFIELDS, conn_handle->body.s);
		easy_setopt(conn_handle->curl, CURLOPT_POSTFIELDSIZE,
				(long)conn_handle->body.len);
	}
	easy_setopt(conn_handle->curl, CURLOPT_WRITEFUNCTION, lh_write_func);
	easy_setopt(conn_handle->curl, CURLOPT_NOSIGNAL, 1L);
	easy_setopt(conn_handle->curl, CURLOPT_TIMEOUT_MS, (long)lh_timeout);
	/* allow self-signed certs */
	easy_setopt(conn_handle->curl, CURLOPT_SSL_VERIFYPEER, 0L);
	if(lh_auth_usrpwd.s) {
		easy_setopt(conn_handle->curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
		easy_setopt(conn_handle->curl, CURLOPT_USERPWD, lh_auth_usrpwd.s);
	}

	snprintf(ctype, sizeof(ctype), "Content-Type: %s", lh_format->content_type);
	conn_handle->headers = curl_slist_append(conn_handle->headers, ctype);
	conn_handle->headers = curl_slist_append(conn_handle->headers, "Expect:");
	if(lh_chunked)
		conn_handle->headers = curl_slist_append(
				conn_handle->headers, "Transfer-Encoding: chunked");
	easy_setopt(conn_handle->curl, CURLOPT_HTTPHEADER, conn_handle->headers);
	return 0;
}

int lh_curl_send_message(
		struct lh_target *target, struct lh_target *failover_target)
{
	struct curl_conn_handle *conn_handle;
	struct lh_message *jmsg;
	unsigned int count = 0;

	if(!get_stat_val(lh_stat_queue_size) || lh_current_batch_size)
		return 0;

	conn_handle = pkg_mallocxz(sizeof(struct curl_conn_handle));
	if(!conn_handle) {
		PKG_MEM_ERROR;
		return -1;
	}
	conn_handle->target = target;
	conn_handle->failover_target = failover_target;

	/* size of the batch: the queue only grows at the tail while the
	 * request is in flight, so the first count entries stay in place */
	STAILQ_FOREACH(jmsg, &lh_queue, next)
	{
		if(lh_batch_size && count >= lh_batch_size)
			break;
		count++;
	}
	lh_body_rewind(conn_handle, count);

	if(!lh_chunked && lh_build_body(conn_handle) < 0) {
		lh_conn_remove(conn_handle);
		return -1;
	}

	conn_handle->curl = curl_easy_init();
	if(!conn_handle->curl) {
		LM_ERR("curl_easy_init() failed\n");
		lh_conn_remove(conn_handle);
		return -1;
	}

	if(lh_curl_setup(conn_handle) < 0 || lh_conn_add(conn_handle) < 0) {
		lh_conn_remove(conn_handle);
		return -1;
	}

	lh_current_batch_size = count;
	return 0;
}

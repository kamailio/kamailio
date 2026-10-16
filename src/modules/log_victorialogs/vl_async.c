/**
 * Copyright (C) 2019-2026 Alexey Volokitin (didww.com)
 * Copyright (C) 2025-2026 Michael Furmur (didww.com)
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

#include "vl_async.h"
#include "vl_worker.h"
#include "log_victorialogs.h"
#include "curl_defs.h"

#define VL_TIME_BUF_SIZE 32
#define VL_HEADER_BUF_SIZE 256

static vl_queue_t vl_queue = STAILQ_HEAD_INITIALIZER(vl_queue);
/* number of messages included in the request in flight */
static unsigned int vl_current_batch_size = 0;

static void *vl_srjson_malloc(size_t sz)
{
	return pkg_malloc(sz);
}

static void vl_srjson_free(void *ptr)
{
	pkg_free(ptr);
}

int vl_async_send(struct group_key *key, async_param_value_t *values, int size)
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

	if(vl_worker_send(msg)) {
		return 1;
	}

	shm_free(msg);
	return -1;
}

/* format unix time in milliseconds as RFC3339 with ms precision (UTC) */
static int vl_format_time(uint64_t ms, char *buf, size_t len)
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

bool vl_add_message_to_queue(async_log_entry_t *message)
{
	srjson_doc_t jdoc;
	srjson_Hooks hooks = {vl_srjson_malloc, vl_srjson_free};
	async_log_param_t *param;
	struct vl_message *jmsg;
	char *jmsg_str;

	if(vl_queue_size && get_stat_val(vl_stat_queue_size) > vl_queue_size) {
		update_stat(vl_stat_drop_total, 1);
		LM_ERR("number of queued messages exceeds %u, dropping\n",
				vl_queue_size);
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
		char buf[INET6_ADDRSTRLEN > VL_TIME_BUF_SIZE ? INET6_ADDRSTRLEN
													 : VL_TIME_BUF_SIZE];

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
				if(vl_format_time(param->value.u.date, buf, sizeof(buf)) < 0)
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

	jmsg = pkg_mallocxz(sizeof(struct vl_message));
	if(!jmsg) {
		PKG_MEM_ERROR;
		pkg_free(jmsg_str);
		goto done;
	}
	jmsg->msg_str.s = jmsg_str;
	jmsg->msg_str.len = strlen(jmsg_str);

	STAILQ_INSERT_TAIL(&vl_queue, jmsg, next);
	update_stat(vl_stat_queue_size, 1);

done:
	return (get_stat_val(vl_stat_queue_size) > vl_batch_size);
}

static void vl_free_message(struct vl_message *jmsg)
{
	if(jmsg->msg_str.s)
		pkg_free(jmsg->msg_str.s);
	pkg_free(jmsg);
}

void vl_free_queue(void)
{
	while(!STAILQ_EMPTY(&vl_queue)) {
		struct vl_message *jmsg = STAILQ_FIRST(&vl_queue);

		STAILQ_REMOVE_HEAD(&vl_queue, next);
		vl_free_message(jmsg);
	}

	reset_stat(vl_stat_queue_size);
}

void vl_send_queue_messages(void)
{
	struct vl_target *target = vl_get_next_target();

	if(!target)
		return;

	if(vl_curl_send_message(target, 0) != 0) {
		update_stat(vl_stat_err_total, 1);
		LM_ERR("failed to start sending log messages\n");
	}
}

void vl_send_queue_messages_fin(bool success)
{
	if(success) {
		while(!STAILQ_EMPTY(&vl_queue) && vl_current_batch_size) {
			struct vl_message *jmsg = STAILQ_FIRST(&vl_queue);
			STAILQ_REMOVE_HEAD(&vl_queue, next);
			update_stat(vl_stat_queue_size, -1);
			--vl_current_batch_size;
			vl_free_message(jmsg);
		}
	}

	vl_current_batch_size = 0;
}

/* discard the http response body */
static size_t vl_write_func(char *ptr, size_t size, size_t nmemb, void *data)
{
	(void)ptr;
	(void)data;
	return size * nmemb;
}

/* build the newline-delimited json body of the next batch */
static int vl_build_body(str *body, unsigned int *count)
{
	struct vl_message *jmsg;
	unsigned int n = 0;
	int len = 0;
	char *p;

	STAILQ_FOREACH(jmsg, &vl_queue, next)
	{
		if(vl_batch_size && n >= vl_batch_size)
			break;
		len += jmsg->msg_str.len + 1;
		n++;
	}

	body->s = pkg_malloc(len + 1);
	if(!body->s) {
		PKG_MEM_ERROR;
		return -1;
	}

	p = body->s;
	n = 0;
	STAILQ_FOREACH(jmsg, &vl_queue, next)
	{
		if(vl_batch_size && n >= vl_batch_size)
			break;
		memcpy(p, jmsg->msg_str.s, jmsg->msg_str.len);
		p += jmsg->msg_str.len;
		*p++ = '\n';
		n++;
	}
	*p = '\0';
	body->len = len;
	*count = n;
	return 0;
}

static struct curl_slist *vl_add_header(
		struct curl_slist *headers, const char *name, str *value)
{
	char buf[VL_HEADER_BUF_SIZE];

	if(!value->s || !value->len)
		return headers;
	if(snprintf(buf, sizeof(buf), "%s: %.*s", name, value->len, value->s)
			>= (int)sizeof(buf)) {
		LM_ERR("header %s value too long\n", name);
		return headers;
	}
	return curl_slist_append(headers, buf);
}

static struct curl_slist *vl_add_header_int(
		struct curl_slist *headers, const char *name, int value)
{
	char buf[VL_HEADER_BUF_SIZE];

	snprintf(buf, sizeof(buf), "%s: %d", name, value);
	return curl_slist_append(headers, buf);
}

static int vl_curl_setup(struct curl_conn_handle *conn_handle)
{
	easy_setopt(conn_handle->curl, CURLOPT_PRIVATE, conn_handle);
	easy_setopt(conn_handle->curl, CURLOPT_URL, conn_handle->target->url.s);
	easy_setopt(conn_handle->curl, CURLOPT_POST, 1L);
	easy_setopt(conn_handle->curl, CURLOPT_POSTFIELDS, conn_handle->body.s);
	easy_setopt(conn_handle->curl, CURLOPT_POSTFIELDSIZE,
			(long)conn_handle->body.len);
	easy_setopt(conn_handle->curl, CURLOPT_WRITEFUNCTION, vl_write_func);
	easy_setopt(conn_handle->curl, CURLOPT_NOSIGNAL, 1L);
	easy_setopt(conn_handle->curl, CURLOPT_TIMEOUT_MS, (long)vl_timeout);
	/* allow self-signed certs */
	easy_setopt(conn_handle->curl, CURLOPT_SSL_VERIFYPEER, 0L);
	if(vl_auth_usrpwd.s) {
		easy_setopt(conn_handle->curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
		easy_setopt(conn_handle->curl, CURLOPT_USERPWD, vl_auth_usrpwd.s);
	}

	conn_handle->headers = curl_slist_append(
			conn_handle->headers, "Content-Type: application/stream+json");
	conn_handle->headers = curl_slist_append(conn_handle->headers, "Expect:");
	conn_handle->headers = vl_add_header(
			conn_handle->headers, "VL-Stream-Fields", &vl_stream_fields);
	conn_handle->headers =
			vl_add_header(conn_handle->headers, "VL-Msg-Field", &vl_msg_field);
	conn_handle->headers = vl_add_header(
			conn_handle->headers, "VL-Time-Field", &vl_time_field);
	conn_handle->headers = vl_add_header(
			conn_handle->headers, "VL-Extra-Fields", &vl_extra_fields);
	if(vl_account_id > 0 || vl_project_id > 0) {
		conn_handle->headers = vl_add_header_int(
				conn_handle->headers, "AccountID", vl_account_id);
		conn_handle->headers = vl_add_header_int(
				conn_handle->headers, "ProjectID", vl_project_id);
	}
	easy_setopt(conn_handle->curl, CURLOPT_HTTPHEADER, conn_handle->headers);
	return 0;
}

int vl_curl_send_message(
		struct vl_target *target, struct vl_target *failover_target)
{
	struct curl_conn_handle *conn_handle;
	unsigned int count = 0;

	if(!get_stat_val(vl_stat_queue_size) || vl_current_batch_size)
		return 0;

	conn_handle = pkg_mallocxz(sizeof(struct curl_conn_handle));
	if(!conn_handle) {
		PKG_MEM_ERROR;
		return -1;
	}
	conn_handle->target = target;
	conn_handle->failover_target = failover_target;

	if(vl_build_body(&conn_handle->body, &count) < 0) {
		pkg_free(conn_handle);
		return -1;
	}

	conn_handle->curl = curl_easy_init();
	if(!conn_handle->curl) {
		LM_ERR("curl_easy_init() failed\n");
		pkg_free(conn_handle->body.s);
		pkg_free(conn_handle);
		return -1;
	}

	if(vl_curl_setup(conn_handle) < 0 || vl_conn_add(conn_handle) < 0) {
		vl_conn_remove(conn_handle);
		return -1;
	}

	vl_current_batch_size = count;
	return 0;
}

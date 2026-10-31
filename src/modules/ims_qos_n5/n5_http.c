/*
 * ims_qos_n5 - P-CSCF media authorisation over N5 (Npcf_PolicyAuthorization)
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

#include <string.h>
#include <strings.h>
#include <curl/curl.h>

#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "n5_http.h"

extern int n5_http_timeout;
extern int n5_http2;

/* One easy handle per process: libcurl keeps the HTTP/2 connection to the
 * PCF open on it between requests. */
static CURL *n5_curl = NULL;

typedef struct n5_buf
{
	char *s;
	size_t len;
} n5_buf_t;

static size_t n5_body_cb(char *ptr, size_t size, size_t nmemb, void *ud)
{
	n5_buf_t *b = (n5_buf_t *)ud;
	size_t n = size * nmemb;
	char *p = pkg_realloc(b->s, b->len + n + 1);
	if(p == NULL) {
		PKG_MEM_ERROR;
		return 0;
	}
	memcpy(p + b->len, ptr, n);
	b->s = p;
	b->len += n;
	b->s[b->len] = '\0';
	return n;
}

static size_t n5_header_cb(char *ptr, size_t size, size_t nmemb, void *ud)
{
	str *loc = (str *)ud;
	size_t n = size * nmemb;
	size_t k = sizeof("location:") - 1;
	char *v, *e;

	if(n <= k || strncasecmp(ptr, "location:", k) != 0)
		return n;
	v = ptr + k;
	e = ptr + n;
	while(v < e && (*v == ' ' || *v == '\t'))
		v++;
	while(e > v && (e[-1] == '\r' || e[-1] == '\n' || e[-1] == ' '))
		e--;
	if(loc->s)
		pkg_free(loc->s);
	loc->len = (int)(e - v);
	loc->s = pkg_malloc(loc->len + 1);
	if(loc->s == NULL) {
		PKG_MEM_ERROR;
		loc->len = 0;
		return 0;
	}
	memcpy(loc->s, v, loc->len);
	loc->s[loc->len] = '\0';
	return n;
}

int n5_http_mod_init(void)
{
	if(curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
		LM_ERR("curl_global_init failed\n");
		return -1;
	}
	return 0;
}

void n5_http_mod_destroy(void)
{
	if(n5_curl) {
		curl_easy_cleanup(n5_curl);
		n5_curl = NULL;
	}
	curl_global_cleanup();
}

void n5_http_res_free(n5_http_res_t *res)
{
	if(res->location.s)
		pkg_free(res->location.s);
	if(res->body.s)
		pkg_free(res->body.s);
	memset(res, 0, sizeof(*res));
}

/* One attempt. Returns 0 with a response, -1 on failure, -2 on a transport
 * failure on a reused connection that delivered no response. */
static int n5_http_once(const char *method, const char *url, const char *ctype,
		const char *body, const char *bearer, int fresh, n5_http_res_t *res)
{
	struct curl_slist *hdrs = NULL;
	n5_buf_t rb = {NULL, 0};
	char ct[128];
	char *auth = NULL;
	CURLcode rc;

	memset(res, 0, sizeof(*res));
	if(n5_curl == NULL) {
		n5_curl = curl_easy_init();
		if(n5_curl == NULL) {
			LM_ERR("curl_easy_init failed\n");
			return -1;
		}
	} else {
		curl_easy_reset(n5_curl); /* keeps the connection cache */
	}

	curl_easy_setopt(n5_curl, CURLOPT_URL, url);
	curl_easy_setopt(n5_curl, CURLOPT_CUSTOMREQUEST, method);
	curl_easy_setopt(n5_curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(n5_curl, CURLOPT_TIMEOUT_MS, (long)n5_http_timeout);
	curl_easy_setopt(n5_curl, CURLOPT_CONNECTTIMEOUT_MS, (long)n5_http_timeout);
	/* SBI is HTTP/2 (TS 29.500); producers on plain TCP expect prior
	 * knowledge, not an Upgrade. */
	if(n5_http2)
		curl_easy_setopt(n5_curl, CURLOPT_HTTP_VERSION,
				(long)CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
	/* Small requests on a reused connection: without TCP_NODELAY a request
	 * body can wait for the peer's delayed ACK (~40 ms). */
	curl_easy_setopt(n5_curl, CURLOPT_TCP_NODELAY, 1L);
	if(fresh)
		curl_easy_setopt(n5_curl, CURLOPT_FRESH_CONNECT, 1L);

	hdrs = curl_slist_append(hdrs, "Accept: application/json");
	if(bearer && bearer[0]) {
		size_t n = strlen(bearer) + sizeof("Authorization: Bearer ");
		auth = pkg_malloc(n);
		if(auth == NULL) {
			PKG_MEM_ERROR;
			curl_slist_free_all(hdrs);
			return -1;
		}
		snprintf(auth, n, "Authorization: Bearer %s", bearer);
		hdrs = curl_slist_append(hdrs, auth);
	}
	if(body) {
		snprintf(ct, sizeof(ct), "Content-Type: %s",
				ctype ? ctype : "application/json");
		hdrs = curl_slist_append(hdrs, ct);
		curl_easy_setopt(n5_curl, CURLOPT_POSTFIELDS, body);
		curl_easy_setopt(n5_curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
	} else if(strcmp(method, "POST") == 0) {
		curl_easy_setopt(n5_curl, CURLOPT_POSTFIELDS, "");
		curl_easy_setopt(n5_curl, CURLOPT_POSTFIELDSIZE, 0L);
	}
	curl_easy_setopt(n5_curl, CURLOPT_HTTPHEADER, hdrs);
	curl_easy_setopt(n5_curl, CURLOPT_WRITEFUNCTION, n5_body_cb);
	curl_easy_setopt(n5_curl, CURLOPT_WRITEDATA, &rb);
	curl_easy_setopt(n5_curl, CURLOPT_HEADERFUNCTION, n5_header_cb);
	curl_easy_setopt(n5_curl, CURLOPT_HEADERDATA, &res->location);

	rc = curl_easy_perform(n5_curl);
	curl_slist_free_all(hdrs);
	if(auth)
		pkg_free(auth);
	if(rc != CURLE_OK) {
		long code = 0, conns = 1;
		curl_easy_getinfo(n5_curl, CURLINFO_RESPONSE_CODE, &code);
		curl_easy_getinfo(n5_curl, CURLINFO_NUM_CONNECTS, &conns);
		if(rb.s)
			pkg_free(rb.s);
		n5_http_res_free(res);
		if(!fresh && conns == 0 && code == 0) {
			LM_DBG("%s %s: reused connection failed (%s)\n", method, url,
					curl_easy_strerror(rc));
			return -2;
		}
		LM_ERR("%s %s failed: %s\n", method, url, curl_easy_strerror(rc));
		return -1;
	}
	curl_easy_getinfo(n5_curl, CURLINFO_RESPONSE_CODE, &res->code);
	res->body.s = rb.s;
	res->body.len = (int)rb.len;
	LM_DBG("%s %s -> %ld\n", method, url, res->code);
	return 0;
}

int n5_http_request(const char *method, const char *url, const char *ctype,
		const char *body, const char *bearer, n5_http_res_t *res)
{
	int rc = n5_http_once(method, url, ctype, body, bearer, 0, res);

	/* Producers close idle HTTP/2 connections (free5GC after 1 ms by
	 * default, with GOAWAY). A request that failed on such a reused
	 * connection without any response is retried once on a new connection:
	 * streams above a GOAWAY's last stream id were not processed (RFC 9113
	 * clause 6.8). */
	if(rc == -2)
		rc = n5_http_once(method, url, ctype, body, bearer, 1, res);
	return rc < 0 ? -1 : 0;
}

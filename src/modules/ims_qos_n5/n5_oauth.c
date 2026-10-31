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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <jansson.h>

#include "../../core/dprint.h"
#include "../../core/str.h"
#include "n5_http.h"
#include "n5_oauth.h"

extern int n5_oauth;
extern str n5_nrf_uri;
extern str n5_nf_instance_id;
extern str n5_nf_type;
extern str n5_nf_ipv4;
extern int n5_heartbeat_interval;

/* per-process token cache */
typedef struct n5_token
{
	char value[2048];
	time_t expires;
} n5_token_t;

static n5_token_t n5_pcf_tok;
static n5_token_t n5_nrf_tok;

/* timer process state */
static int n5_registered = 0;
static time_t n5_next_hb = 0;
static int n5_hb_interval = 30;

int n5_oauth_mod_init(void)
{
	if(!n5_oauth)
		return 0;
	if(n5_nrf_uri.len <= 0 || n5_nf_instance_id.len <= 0) {
		LM_ERR("oauth needs nrf_uri and nf_instance_id\n");
		return -1;
	}
	while(n5_nrf_uri.len > 0 && n5_nrf_uri.s[n5_nrf_uri.len - 1] == '/')
		n5_nrf_uri.len--;
	if(n5_heartbeat_interval > 0)
		n5_hb_interval = n5_heartbeat_interval;
	return 0;
}

/* application/x-www-form-urlencoded value: unreserved characters kept,
 * space as '+', the rest percent-encoded */
static void n5_form_enc(char *out, size_t size, const char *in, int len)
{
	static const char hex[] = "0123456789ABCDEF";
	size_t n = 0;
	int i;

	for(i = 0; i < len && n + 4 < size; i++) {
		unsigned char c = (unsigned char)in[i];
		if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
				|| (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'
				|| c == '~') {
			out[n++] = (char)c;
		} else if(c == ' ') {
			out[n++] = '+';
		} else {
			out[n++] = '%';
			out[n++] = hex[c >> 4];
			out[n++] = hex[c & 0xf];
		}
	}
	out[n] = '\0';
}

/* AccessTokenReq (TS 29.510 clause 6.3.5.2.2) */
static const char *n5_get_token(
		n5_token_t *tok, const char *target_nf, const char *scope)
{
	char url[512], body[1024], id[256], type[64], sc[256];
	n5_http_res_t res;
	json_t *doc = NULL, *at, *exp;
	json_error_t jerr;
	time_t now = time(NULL);

	if(!n5_oauth)
		return NULL;
	if(tok->value[0] && now + 30 < tok->expires)
		return tok->value;

	n5_form_enc(id, sizeof(id), n5_nf_instance_id.s, n5_nf_instance_id.len);
	n5_form_enc(type, sizeof(type), n5_nf_type.s, n5_nf_type.len);
	n5_form_enc(sc, sizeof(sc), scope, (int)strlen(scope));
	snprintf(body, sizeof(body),
			"grant_type=client_credentials&nfInstanceId=%s&nfType=%s"
			"&targetNfType=%s&scope=%s",
			id, type, target_nf, sc);
	snprintf(url, sizeof(url), "%.*s/oauth2/token", n5_nrf_uri.len,
			n5_nrf_uri.s);
	if(n5_http_request("POST", url, "application/x-www-form-urlencoded", body,
			   NULL, &res)
			< 0)
		return NULL;
	if(res.code != 200 || res.body.len <= 0) {
		LM_ERR("access token for %s answered %ld: %.*s\n", target_nf, res.code,
				res.body.len, res.body.s ? res.body.s : "");
		n5_http_res_free(&res);
		return NULL;
	}
	doc = json_loadb(res.body.s, (size_t)res.body.len, 0, &jerr);
	n5_http_res_free(&res);
	at = doc ? json_object_get(doc, "access_token") : NULL;
	if(!json_is_string(at)
			|| strlen(json_string_value(at)) >= sizeof(tok->value)) {
		LM_ERR("no usable access_token in the NRF answer\n");
		if(doc)
			json_decref(doc);
		return NULL;
	}
	strcpy(tok->value, json_string_value(at));
	exp = json_object_get(doc, "expires_in");
	tok->expires = now + (json_is_integer(exp) ? json_integer_value(exp) : 60);
	json_decref(doc);
	LM_DBG("access token for %s valid %lds\n", target_nf,
			(long)(tok->expires - now));
	return tok->value;
}

const char *n5_oauth_pcf_token(void)
{
	return n5_get_token(&n5_pcf_tok, "PCF", "npcf-policyauthorization");
}

/* NFRegister (TS 29.510 clause 5.2.2.2): PUT the AF's NF profile */
static int n5_nrf_register(void)
{
	char url[512], *body;
	json_t *prof, *ips;
	n5_http_res_t res;
	int ret = -1;

	prof = json_object();
	json_object_set_new(prof, "nfInstanceId",
			json_stringn(n5_nf_instance_id.s, (size_t)n5_nf_instance_id.len));
	json_object_set_new(
			prof, "nfType", json_stringn(n5_nf_type.s, (size_t)n5_nf_type.len));
	json_object_set_new(prof, "nfStatus", json_string("REGISTERED"));
	json_object_set_new(prof, "heartBeatTimer", json_integer(n5_hb_interval));
	if(n5_nf_ipv4.len > 0) {
		ips = json_array();
		json_array_append_new(
				ips, json_stringn(n5_nf_ipv4.s, (size_t)n5_nf_ipv4.len));
		json_object_set_new(prof, "ipv4Addresses", ips);
	}
	body = json_dumps(prof, JSON_COMPACT);
	json_decref(prof);
	if(body == NULL)
		return -1;
	snprintf(url, sizeof(url), "%.*s/nnrf-nfm/v1/nf-instances/%.*s",
			n5_nrf_uri.len, n5_nrf_uri.s, n5_nf_instance_id.len,
			n5_nf_instance_id.s);
	if(n5_http_request("PUT", url, "application/json", body, NULL, &res) == 0) {
		if(res.code == 200 || res.code == 201) {
			json_error_t jerr;
			json_t *doc = json_loadb(res.body.s ? res.body.s : "",
					(size_t)res.body.len, 0, &jerr);
			json_t *hb = doc ? json_object_get(doc, "heartBeatTimer") : NULL;
			/* the NRF may grant a different heartbeat interval */
			if(json_is_integer(hb) && json_integer_value(hb) > 0)
				n5_hb_interval = (int)json_integer_value(hb);
			if(doc)
				json_decref(doc);
			LM_INFO("registered with the NRF as %.*s %.*s (heartbeat %ds)\n",
					n5_nf_type.len, n5_nf_type.s, n5_nf_instance_id.len,
					n5_nf_instance_id.s, n5_hb_interval);
			ret = 0;
		} else {
			LM_ERR("NF registration answered %ld: %.*s\n", res.code,
					res.body.len, res.body.s ? res.body.s : "");
		}
		n5_http_res_free(&res);
	}
	free(body); /* jansson allocates with malloc */
	return ret;
}

/* NFUpdate heartbeat (TS 29.510 clause 5.2.2.3.2): 404 means the NRF no
 * longer knows this instance, so register again. */
static int n5_nrf_heartbeat(void)
{
	char url[512];
	const char *tok;
	n5_http_res_t res;
	int ret = -1;
	static const char patch[] =
			"[{\"op\":\"replace\",\"path\":\"/nfStatus\",\"value\":"
			"\"REGISTERED\"}]";

	tok = n5_get_token(&n5_nrf_tok, "NRF", "nnrf-nfm");
	snprintf(url, sizeof(url), "%.*s/nnrf-nfm/v1/nf-instances/%.*s",
			n5_nrf_uri.len, n5_nrf_uri.s, n5_nf_instance_id.len,
			n5_nf_instance_id.s);
	if(n5_http_request(
			   "PATCH", url, "application/json-patch+json", patch, tok, &res)
			< 0)
		return -1;
	if(res.code == 200 || res.code == 204) {
		ret = 0;
	} else if(res.code == 404) {
		LM_WARN("NRF lost this NF instance; registering again\n");
		n5_registered = 0;
	} else {
		LM_ERR("NRF heartbeat answered %ld\n", res.code);
	}
	n5_http_res_free(&res);
	return ret;
}

void n5_nrf_timer(unsigned int ticks, void *param)
{
	time_t now = time(NULL);

	if(!n5_oauth || now < n5_next_hb)
		return;
	if(!n5_registered) {
		n5_registered = (n5_nrf_register() == 0);
		/* retry a failed registration soon, heartbeat otherwise */
		n5_next_hb = now + (n5_registered ? n5_hb_interval : 5);
		return;
	}
	n5_nrf_heartbeat();
	n5_next_hb = now + (n5_registered ? n5_hb_interval : 1);
}

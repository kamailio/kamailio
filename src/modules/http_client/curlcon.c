/*
 * Copyright (C) 2015 Olle E. Johansson, Edvina AB
 *
 * Based on code from sqlops and htable by Elena-Ramona:
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*! \file
 * \brief  Kamailio http_client :: Connection handling
 * \ingroup http_client
 */

#include <curl/curl.h>

#include "../../core/hashes.h"
#include "../../core/dprint.h"
#include "../../core/parser/parse_param.h"
#include "../../core/usr_avp.h"
#include "../../core/shm_init.h"
#include "../../core/cfg_parser.h"
#include "http_client.h"
#include "curlcon.h"

#define KEYVALUE_TYPE_NONE 0
#define KEYVALUE_TYPE_PARAMS 1


curl_con_t *_curl_con_root = NULL; /*!< Shared memory list of connections */
curl_con_pkg_t *_curl_con_pkg_root =
		NULL; /*!< Package memory list of connection states, last result */

/* Forward declaration */
curl_con_t *curl_init_con(str *name);

/* Temporary structure for holding info parsed from cfg file */
typedef struct raw_http_client_conn
{
	str name;

	str url;
	str username;
	str password;
	str failover;
	str useragent;
	str clientcert;
	str clientkey;
	str ciphersuites;
	str http_proxy;
	int http_proxy_port;
	int verify_peer;
	int verify_host;
	int tlsversion;
	int timeout;
	int maxdatasize;
	int http_follow_redirect;
	int authmethod;
	int keep_connections;

	struct raw_http_client_conn *next;
} raw_http_client_conn_t;

static raw_http_client_conn_t *raw_conn_list = NULL;

static cfg_option_t tls_versions[] = {
		{"DEFAULT", .val = 0}, /* CURL_SSLVERSION_DEFAULT */
		{"TLSv1", .val = 1},   /* CURL_SSLVERSION_TLSv1 */
		{"SSLv2", .val = 2},   /* CURL_SSLVERSION_SSLv2 */
		{"SSLv3", .val = 3},   /* CURL_SSLVERSION_SSLv3 */
		{"TLSv1.0",
				.val = 4}, /* CURL_SSLVERSION_TLSv1_0 - support after libcurl 7.34.0 */
		{"TLSv1.1",
				.val = 5}, /* CURL_SSLVERSION_TLSv1_1 - support after libcurl 7.34.0 */
		{"TLSv1.2",
				.val = 6}, /* CURL_SSLVERSION_TLSv1_2 - support after libcurl 7.34.0 */
		{0}};

static cfg_option_t http_client_options[] = {
		{"url", .f = cfg_parse_str_opt, .flags = CFG_STR_PKGMEM},		/* 0 */
		{"username", .f = cfg_parse_str_opt, .flags = CFG_STR_PKGMEM},  /* 1 */
		{"password", .f = cfg_parse_str_opt, .flags = CFG_STR_PKGMEM},  /* 2 */
		{"failover", .f = cfg_parse_str_opt, .flags = CFG_STR_PKGMEM},  /* 3 */
		{"useragent", .f = cfg_parse_str_opt, .flags = CFG_STR_PKGMEM}, /* 4 */
		{"verify_peer", .f = cfg_parse_bool_opt},						/* 5 */
		{"verify_host", .f = cfg_parse_bool_opt},						/* 6 */
		{"client_cert", .f = cfg_parse_str_opt,
				.flags = CFG_STR_PKGMEM},								 /* 7 */
		{"client_key", .f = cfg_parse_str_opt, .flags = CFG_STR_PKGMEM}, /* 8 */
		{"cipher_suites", .f = cfg_parse_str_opt,
				.flags = CFG_STR_PKGMEM},								/* 9 */
		{"tlsversion", .f = cfg_parse_enum_opt, .param = tls_versions}, /* 10 */
		{"timeout", .f = cfg_parse_int_opt},							/* 11 */
		{"maxdatasize", .f = cfg_parse_int_opt},						/* 12 */
		{"httpredirect", .f = cfg_parse_bool_opt},						/* 13 */
		{"httpproxy", .f = cfg_parse_str_opt, .flags = CFG_STR_PKGMEM}, /* 14 */
		{"httpproxyport", .f = cfg_parse_int_opt},						/* 15 */
		{"authmethod", .f = cfg_parse_int_opt},							/* 16 */
		{"keepconnections", .f = cfg_parse_int_opt},					/* 17 */
		{0}};

/*! Count the number of connections 
 */
unsigned int curl_connection_count()
{
	unsigned int i = 0;
	curl_con_t *cc;
	cc = _curl_con_root;
	while(cc) {
		i++;
		cc = cc->next;
	}
	return i;
}


/*! Check if CURL connection exists
 */
int http_connection_exists(str *name)
{
	if(curl_get_connection(name) != NULL) {
		return 1;
	}

	LM_DBG("curl_connection_exists no success in looking for httpcon: [%.*s]\n",
			name->len, name->s);
	return 0;
}

/*! Find CURL connection by name
 */
curl_con_t *curl_get_connection(str *name)
{
	curl_con_t *cc;
	unsigned int conid;

	conid = core_case_hash(name, 0, 0);
	LM_DBG("curl_get_connection looking for httpcon: [%.*s] ID %u\n", name->len,
			name->s, conid);

	cc = _curl_con_root;
	while(cc) {
		if(conid == cc->conid && cc->name.len == name->len
				&& strncmp(cc->name.s, name->s, name->len) == 0) {
			return cc;
		}
		cc = cc->next;
	}
	LM_DBG("curl_get_connection no success in looking for httpcon: [%.*s]\n",
			name->len, name->s);
	return NULL;
}

/*! Find package memory structure for a connection */
curl_con_pkg_t *curl_get_pkg_connection(curl_con_t *con)
{
	curl_con_pkg_t *ccp;

	ccp = _curl_con_pkg_root;
	while(ccp) {
		if(ccp->conid == con->conid) {
			return ccp;
		}
		ccp = ccp->next;
	}
	LM_ERR("curl_get_pkg_connection no success in looking for pkg memory for "
		   "httpcon: [%.*s]\n",
			con->name.len, con->name.s);
	return NULL;
}


/*! Parse the httpcon module parameter
 *
 *	Syntax:
 *		name => proto://user:password@server/url/url
 *		name => proto://server/url/url
 *		name => proto://server/url/url;param=value;param=value
 *
 *		the url is very much like CURLs syntax
 *		the url is a base url where you can add local address
 *	Parameters
 *		httpredirect
 *		timeout
 *		useragent
 *		failover
 *		maxdatasize
 *		verifypeer
 *		verifyhost
 *
 */
int curl_parse_param(char *val)
{
	str name = STR_NULL;
	str schema = STR_NULL;
	str url = STR_NULL;
	str username = STR_NULL;
	str password = STR_NULL;
	str params = STR_NULL;
	str failover = STR_NULL;

	str client_cert = default_tls_clientcert;
	str client_key = default_tls_clientkey;
	str ciphersuites = default_cipher_suite_list;
	str useragent = default_useragent;
	str http_proxy = default_http_proxy;

	unsigned int http_proxy_port = default_http_proxy_port;
	unsigned int maxdatasize = default_maxdatasize;
	unsigned int timeout = default_connection_timeout;
	unsigned int http_follow_redirect = default_http_follow_redirect;
	unsigned int verify_peer = default_tls_verify_peer;
	unsigned int verify_host = default_tls_verify_host;
	unsigned int tlsversion = default_tls_version;
	unsigned int authmethod = default_authmethod;
	unsigned int keep_connections = default_keep_connections;

	str in;
	char *p;
	char *u;
	param_t *conparams = NULL;
	curl_con_t *cc = NULL;

	LM_DBG("modparam httpcon: %s\n", val);
	LM_DBG(" *** Default httproxy: %s\n", http_proxy.s);

	if(!shm_initialized()) {
		LM_ERR("shared memory was not initialized\n");
		return -1;
	}

	/* parse: name=>http_url*/
	in.s = val;
	in.len = strlen(in.s);
	p = in.s;

	/* Skip white space */
	while(p < in.s + in.len
			&& (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
		p++;
	}
	if(p > in.s + in.len || *p == '\0') {
		goto error;
	}

	/* This is the connection name */
	name.s = p;
	/* Skip to whitespace */
	while(p < in.s + in.len) {
		if(*p == '=' || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
			break;
		}
		p++;
	}
	if(p > in.s + in.len || *p == '\0') {
		goto error;
	}
	name.len = p - name.s;
	if(*p != '=') {
		/* Skip whitespace */
		while(p < in.s + in.len
				&& (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
			p++;
		}
		if(p > in.s + in.len || *p == '\0' || *p != '=') {
			goto error;
		}
	}
	p++;
	if(*p != '>') {
		goto error;
	}
	p++;
	/* Skip white space again */
	while(p < in.s + in.len
			&& (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
		p++;
	}
	schema.s = p;
	/* Skip to colon ':' */
	while(p < in.s + in.len) {
		if(*p == ':') {
			break;
		}
		p++;
	}
	if(*p != ':') {
		goto error;
	}
	schema.len = p - schema.s;
	p++; /* Skip the colon */
	/* Skip two slashes */
	if(*p != '/') {
		goto error;
	}
	p++;
	if(*p != '/') {
		goto error;
	}
	p++;
	/* We are now at the first character after :// */
	url.s = p;
	url.len = in.len + (int)(in.s - p);
	u = p;

	/* Now check if there is a @ character. If so, we need to parse the username
	   and password */
	/* Skip to at-sign '@' */
	while(p < in.s + in.len) {
		if(*p == '@') {
			break;
		}
		p++;
	}
	if(*p == '@') {
		/* We have a username and possibly password - parse them out */
		username.s = u;
		while(u < p) {
			if(*u == ':') {
				break;
			}
			u++;
		}
		username.len = u - username.s;

		/* We either have a : or a @ */
		if(*u == ':') {
			u++;
			/* Go look for password */
			password.s = u;
			while(u < p) {
				u++;
			}
			password.len = u - password.s;
		}
		p++; /* Skip the at sign */
		url.s = p;
		url.len = in.len + (int)(in.s - p);
	}
	/* Reset P to beginning of URL and look for parameters - starting with ; */
	p = url.s;
	/* Skip to ';' or end of string */
	while(p < url.s + url.len) {
		if(*p == ';') {
			/* Cut off URL at the ; */
			url.len = (int)(p - url.s);
			break;
		}
		p++;
	}
	if(*p == ';') {
		/* We have parameters */
		str tok;
		param_t *pit = NULL;

		/* Adjust the URL length */

		p++; /* Skip the ; */
		params.s = p;
		params.len = in.len + (int)(in.s - p);
		param_hooks_t phooks;

		if(parse_params(&params, CLASS_ANY, &phooks, &conparams) < 0) {
			LM_ERR("CURL failed parsing httpcon parameters value\n");
			goto error;
		}

		/* Have parameters */
		for(pit = conparams; pit; pit = pit->next) {
			tok = pit->body;
			if(pit->name.len == 12
					&& strncmp(pit->name.s, "httpredirect", 12) == 0) {
				if(str2int(&tok, &http_follow_redirect) != 0) {
					/* Bad value */
					LM_WARN("curl connection [%.*s]: httpredirect bad value. "
							"Using default\n",
							name.len, name.s);
					http_follow_redirect = default_http_follow_redirect;
				}
				if(http_follow_redirect != 0 && http_follow_redirect != 1) {
					LM_WARN("curl connection [%.*s]: httpredirect bad value. "
							"Using default\n",
							name.len, name.s);
					http_follow_redirect = default_http_follow_redirect;
				}
				LM_DBG("curl [%.*s] - httpredirect [%d]\n", pit->name.len,
						pit->name.s, http_follow_redirect);
			} else if(pit->name.len == 7
					  && strncmp(pit->name.s, "timeout", 7) == 0) {
				if(str2int(&tok, &timeout) != 0) {
					/* Bad timeout */
					LM_WARN("curl connection [%.*s]: timeout bad value. Using "
							"default\n",
							name.len, name.s);
					timeout = default_connection_timeout;
				}
				LM_DBG("curl [%.*s] - timeout [%d]\n", pit->name.len,
						pit->name.s, timeout);
			} else if(pit->name.len == 9
					  && strncmp(pit->name.s, "useragent", 9) == 0) {
				useragent = tok;
				LM_DBG("curl [%.*s] - useragent [%.*s]\n", pit->name.len,
						pit->name.s, useragent.len, useragent.s);
			} else if(pit->name.len == 8
					  && strncmp(pit->name.s, "failover", 8) == 0) {
				failover = tok;
				LM_DBG("curl [%.*s] - failover [%.*s]\n", pit->name.len,
						pit->name.s, failover.len, failover.s);
			} else if(pit->name.len == 11
					  && strncmp(pit->name.s, "maxdatasize", 11) == 0) {
				if(str2int(&tok, &maxdatasize) != 0) {
					/* Bad timeout */
					LM_WARN("curl connection [%.*s]: maxdatasize bad value. "
							"Using default\n",
							name.len, name.s);
					maxdatasize = default_maxdatasize;
				}
				LM_DBG("curl [%.*s] - maxdatasize [%d]\n", pit->name.len,
						pit->name.s, maxdatasize);
			} else if(pit->name.len == 11
					  && strncmp(pit->name.s, "verify_peer", 11) == 0) {
				if(str2int(&tok, &verify_peer) != 0) {
					/* Bad integer */
					LM_WARN("curl connection [%.*s]: verify_peer bad value. "
							"Using default\n",
							name.len, name.s);
					verify_peer = default_tls_verify_peer;
				}
				if(verify_peer != 0 && verify_peer != 1) {
					LM_WARN("curl connection [%.*s]: verify_peer bad value. "
							"Using default\n",
							name.len, name.s);
					verify_peer = default_tls_verify_peer;
				}
				LM_DBG("curl [%.*s] - verify_peer [%d]\n", pit->name.len,
						pit->name.s, verify_peer);
			} else if(pit->name.len == 11
					  && strncmp(pit->name.s, "verify_host", 11) == 0) {
				if(str2int(&tok, &verify_host) != 0) {
					/* Bad integer */
					LM_WARN("curl connection [%.*s]: verify_host bad value. "
							"Using default\n",
							name.len, name.s);
					verify_host = default_tls_verify_host;
				}
				LM_DBG("curl [%.*s] - verify_host [%d]\n", pit->name.len,
						pit->name.s, verify_host);
			} else if(pit->name.len == 10
					  && strncmp(pit->name.s, "tlsversion", 10) == 0) {
				if(str2int(&tok, &tlsversion) != 0) {
					/* Bad integer */
					LM_WARN("curl connection [%.*s]: tlsversion bad value. "
							"Using default\n",
							name.len, name.s);
					tlsversion = default_tls_version;
				}
				if(tlsversion >= CURL_SSLVERSION_LAST) {
					LM_WARN("curl connection [%.*s]: tlsversion unsupported "
							"value. Using default\n",
							name.len, name.s);
					tlsversion = default_tls_version;
				}
				LM_DBG("curl [%.*s] - tlsversion [%d]\n", pit->name.len,
						pit->name.s, tlsversion);
			} else if(pit->name.len == 11
					  && strncmp(pit->name.s, "client_cert", 11) == 0) {
				client_cert = tok;
				LM_DBG("curl [%.*s] - client_cert [%.*s]\n", pit->name.len,
						pit->name.s, client_cert.len, client_cert.s);
			} else if(pit->name.len == 10
					  && strncmp(pit->name.s, "client_key", 10) == 0) {
				client_key = tok;
				LM_DBG("curl [%.*s] - client_key [%.*s]\n", pit->name.len,
						pit->name.s, client_key.len, client_key.s);
			} else if(pit->name.len == 13
					  && strncmp(pit->name.s, "cipher_suites", 13) == 0) {
				ciphersuites = tok;
				LM_DBG("curl [%.*s] - cipher_suites [%.*s]\n", pit->name.len,
						pit->name.s, ciphersuites.len, ciphersuites.s);
			} else {
				LM_ERR("curl Unknown parameter [%.*s] \n", pit->name.len,
						pit->name.s);
			}
		}
	}

	/* The URL ends either with nothing or parameters. Parameters start with ; */

	if(conparams != NULL) {
		free_params(conparams);
	}

	cc = curl_init_con(&name);
	if(cc == NULL) {
		return -1;
	}

	cc->username = username.s ? as_asciiz(&username) : NULL;
	cc->password = password.s ? as_asciiz(&password) : NULL;
	cc->schema = schema;
	cc->authmethod = authmethod;
	cc->keep_connections = keep_connections;
	cc->failover = failover;
	cc->useragent = as_asciiz(&useragent);
	cc->url = url;
	cc->clientcert = client_cert.s ? as_asciiz(&client_cert) : NULL;
	cc->clientkey = client_key.s ? as_asciiz(&client_key) : NULL;
	cc->ciphersuites = ciphersuites.s ? as_asciiz(&ciphersuites) : NULL;
	cc->tlsversion = tlsversion;
	cc->verify_peer = verify_peer;
	cc->verify_host = verify_host;
	cc->timeout = timeout;
	cc->maxdatasize = maxdatasize;
	if(http_proxy_port > 0) {
		cc->http_proxy_port = http_proxy_port;
		cc->http_proxy = http_proxy.s ? as_asciiz(&http_proxy) : NULL;
		LM_DBG("*** Setting HTTP proxy for connection to %s \n",
				cc->http_proxy);
	}
	cc->http_follow_redirect = http_follow_redirect;

	LM_DBG("cname: [%.*s] url: [%.*s] username [%s] password [%s] failover "
		   "[%.*s] timeout [%d] useragent [%s] maxdatasize [%d] "
		   "keep_connections [%d]\n",
			cc->name.len, cc->name.s, cc->url.len, cc->url.s,
			cc->username ? cc->username : "", cc->password ? cc->password : "",
			cc->failover.len, cc->failover.s, cc->timeout, cc->useragent,
			cc->maxdatasize, cc->keep_connections);
	LM_DBG("cname: [%.*s] client_cert [%s] client_key [%s] ciphersuites [%s] "
		   "tlsversion [%d] verify_peer [%d] verify_host [%d] authmethod "
		   "[%d]\n",
			cc->name.len, cc->name.s, cc->clientcert, cc->clientkey,
			cc->ciphersuites, cc->tlsversion, cc->verify_peer, cc->verify_host,
			cc->authmethod);
	if(cc->http_proxy_port > 0) {
		LM_DBG("cname: [%.*s] http_proxy [%s] http_proxy_port [%d]\n",
				cc->name.len, cc->name.s, cc->http_proxy, cc->http_proxy_port);
	}

	return 0;

error:
	LM_ERR("invalid curl parameter [%.*s] at [%d]\n", in.len, in.s,
			(int)(p - in.s));

	if(conparams != NULL) {
		free_params(conparams);
	}
	return -1;
}

int curl_parse_conn(void *param, cfg_parser_t *parser, unsigned int flags)
{
	str name = STR_NULL;

	raw_http_client_conn_t *raw_cc = NULL;
	int i, ret;
	cfg_token_t t;

	/* Get the name from the section header */

	ret = cfg_get_token(&t, parser, 0);
	if(ret < 0)
		return -1;
	if((ret > 0) || (t.type != CFG_TOKEN_ALPHA)) {
		LM_ERR("%s:%d:%d: Invalid or missing connection name\n", parser->file,
				t.start.line, t.start.col);
		return -1;
	}
	pkg_str_dup(&name, &t.val);
	ret = cfg_get_token(&t, parser, 0);
	if(ret < 0)
		return -1;
	if((ret > 0) || (t.type != ']')) {
		LM_ERR("%s:%d:%d: Syntax error, ']' expected\n", parser->file,
				t.start.line, t.start.col);
		return -1;
	}

	if(cfg_eat_eol(parser, flags))
		return -1;

	raw_cc = pkg_malloc(sizeof(raw_http_client_conn_t));
	if(raw_cc == NULL) {
		return -1;
	}
	memset(raw_cc, 0, sizeof(raw_http_client_conn_t));
	raw_cc->next = raw_conn_list;
	raw_conn_list = raw_cc;
	raw_cc->name = name;

	/* Set default values - memory freed if overridden */
	if(default_tls_clientcert.s != NULL)
		pkg_str_dup(&raw_cc->clientcert, &default_tls_clientcert);
	if(default_tls_clientkey.s != NULL)
		pkg_str_dup(&raw_cc->clientkey, &default_tls_clientkey);
	if(default_cipher_suite_list.s != NULL)
		pkg_str_dup(&raw_cc->ciphersuites, &default_cipher_suite_list);
	pkg_str_dup(&raw_cc->useragent, &default_useragent);
	if(default_http_proxy_port > 0) {
		raw_cc->http_proxy_port = default_http_proxy_port;
		if(default_http_proxy.s != NULL) {
			pkg_str_dup(&raw_cc->http_proxy, &default_http_proxy);
		}
	}
	raw_cc->verify_peer = default_tls_verify_peer;
	raw_cc->verify_host = default_tls_verify_host;
	raw_cc->maxdatasize = default_maxdatasize;
	raw_cc->timeout = default_connection_timeout;
	raw_cc->http_follow_redirect = default_http_follow_redirect;
	raw_cc->tlsversion = default_tls_version;
	raw_cc->authmethod = default_authmethod;
	raw_cc->keep_connections = default_keep_connections;

	for(i = 0; tls_versions[i].name; i++) {
		tls_versions[i].param = &raw_cc->tlsversion;
	}
	/* Index from above structure (see top of file) */
	http_client_options[0].param = &raw_cc->url;
	http_client_options[1].param = &raw_cc->username;
	http_client_options[2].param = &raw_cc->password;
	http_client_options[3].param = &raw_cc->failover;
	http_client_options[4].param = &raw_cc->useragent;
	http_client_options[5].param = &raw_cc->verify_peer;
	http_client_options[6].param = &raw_cc->verify_host;
	http_client_options[7].param = &raw_cc->clientcert;
	http_client_options[8].param = &raw_cc->clientkey;
	http_client_options[9].param = &raw_cc->ciphersuites;
	/* tlsversion is set using enum types */
	http_client_options[11].param = &raw_cc->timeout;
	http_client_options[12].param = &raw_cc->maxdatasize;
	http_client_options[13].param = &raw_cc->http_follow_redirect;
	http_client_options[14].param = &raw_cc->http_proxy;
	http_client_options[15].param = &raw_cc->http_proxy_port;
	http_client_options[16].param = &raw_cc->authmethod;
	http_client_options[17].param = &raw_cc->keep_connections;

	cfg_set_options(parser, http_client_options);

	return 1;
}

int fixup_raw_http_client_conn_list(void)
{
	raw_http_client_conn_t *raw_cc = NULL;
	curl_con_t *cc = NULL;
	str schema, url;
	char *pos, *end;
	int ret = 1;

	for(raw_cc = raw_conn_list; raw_cc != NULL; raw_cc = raw_cc->next) {
		cc = curl_init_con(&raw_cc->name);
		if(cc == NULL) {
			ret = -1;
			goto done;
		}
		/* Parse raw URL into schema + hostname/url */
		schema.s = raw_cc->url.s;
		pos = schema.s;
		end = raw_cc->url.s + raw_cc->url.len;
		while((pos < end) && (*pos != '\0')) {
			if(*pos == ':')
				break;
			pos++;
		}
		if(pos[0] != ':' || pos[1] != '/' || pos[2] != '/' || (end - pos < 4)) {
			LM_ERR("Invalid schema://url definition [%.*s]\n", raw_cc->url.len,
					raw_cc->url.s);
			ret = -1;
			goto done;
		}
		schema.len = (int)(pos - schema.s);

		url.s = pos + 3;
		url.len = end - url.s;

		pkg_str_dup(&cc->schema, &schema);
		pkg_str_dup(&cc->url, &url);

		cc->username = raw_cc->username.s ? as_asciiz(&raw_cc->username) : NULL;
		cc->password = raw_cc->password.s ? as_asciiz(&raw_cc->password) : NULL;
		cc->authmethod = raw_cc->authmethod;
		if(raw_cc->failover.s != NULL)
			pkg_str_dup(&cc->failover, &raw_cc->failover);
		cc->useragent = as_asciiz(&raw_cc->useragent);
		cc->clientcert =
				raw_cc->clientcert.s ? as_asciiz(&raw_cc->clientcert) : NULL;
		cc->clientkey =
				raw_cc->clientkey.s ? as_asciiz(&raw_cc->clientkey) : NULL;
		cc->ciphersuites = raw_cc->ciphersuites.s
								   ? as_asciiz(&raw_cc->ciphersuites)
								   : NULL;
		cc->tlsversion = raw_cc->tlsversion;
		if(cc->tlsversion >= CURL_SSLVERSION_LAST) {
			LM_WARN("curl connection [%.*s]: tlsversion %d unsupported value. "
					"Using default\n",
					cc->name.len, cc->name.s, cc->tlsversion);
			cc->tlsversion = default_tls_version;
		}
		cc->http_proxy_port = raw_cc->http_proxy_port;
		if(cc->http_proxy_port > 0 && raw_cc->http_proxy.s != NULL) {
			cc->http_proxy = raw_cc->http_proxy.s
									 ? as_asciiz(&raw_cc->http_proxy)
									 : NULL;
		}

		cc->verify_peer = raw_cc->verify_peer;
		cc->verify_host = raw_cc->verify_host;
		cc->timeout = raw_cc->timeout;
		cc->maxdatasize = raw_cc->maxdatasize;
		cc->http_follow_redirect = raw_cc->http_follow_redirect;
		cc->keep_connections = raw_cc->keep_connections;

		LM_DBG("cname: [%.*s] url: [%.*s] username [%s] password [%s] failover "
			   "[%.*s] timeout [%d] useragent [%s] maxdatasize [%d]\n",
				cc->name.len, cc->name.s, cc->url.len, cc->url.s,
				cc->username ? cc->username : "",
				cc->password ? cc->password : "", cc->failover.len,
				cc->failover.s, cc->timeout, cc->useragent, cc->maxdatasize);
		LM_DBG("cname: [%.*s] client_cert [%s] client_key [%s] ciphersuites "
			   "[%s] tlsversion [%d] verify_peer [%d] verify_host [%d] "
			   "keep_connections [%d]\n",
				cc->name.len, cc->name.s, cc->clientcert, cc->clientkey,
				cc->ciphersuites, cc->tlsversion, cc->verify_peer,
				cc->verify_host, cc->keep_connections);
		if(cc->http_proxy_port > 0) {
			LM_DBG("cname: [%.*s] http_proxy [%s] http_proxy_port [%d]\n",
					cc->name.len, cc->name.s, cc->http_proxy,
					cc->http_proxy_port);
		}
	}
done:
	while(raw_conn_list != NULL) {
		raw_cc = raw_conn_list;
		raw_conn_list = raw_conn_list->next;
		if(raw_cc->name.s)
			pkg_free(raw_cc->name.s);
		if(raw_cc->url.s)
			pkg_free(raw_cc->url.s);
		if(raw_cc->username.s)
			pkg_free(raw_cc->username.s);
		if(raw_cc->password.s)
			pkg_free(raw_cc->password.s);
		if(raw_cc->failover.s)
			pkg_free(raw_cc->failover.s);
		if(raw_cc->useragent.s)
			pkg_free(raw_cc->useragent.s);
		if(raw_cc->clientcert.s)
			pkg_free(raw_cc->clientcert.s);
		if(raw_cc->clientkey.s)
			pkg_free(raw_cc->clientkey.s);
		if(raw_cc->ciphersuites.s)
			pkg_free(raw_cc->ciphersuites.s);
		if(raw_cc->http_proxy.s)
			pkg_free(raw_cc->http_proxy.s);
		pkg_free(raw_cc);
	}
	return ret;
}

int http_client_load_config(str *config_file)
{
	cfg_parser_t *parser;
	str empty = STR_NULL;

	if((parser = cfg_parser_init(&empty, config_file)) == NULL) {
		LM_ERR("Failed to init http_client config file parser\n");
		goto error;
	}

	cfg_section_parser(parser, curl_parse_conn, NULL);
	if(sr_cfg_parse(parser))
		goto error;
	cfg_parser_close(parser);

	fixup_raw_http_client_conn_list();
	return 0;
error:
	return -1;
}

/*! Init connection structure and place it in structure
 */
curl_con_t *curl_init_con(str *name)
{
	curl_con_t *cc;
	curl_con_pkg_t *ccp;
	unsigned int conid;

	conid = core_case_hash(name, 0, 0);
	LM_DBG("curl_init_con httpcon: [%.*s] ID %u\n", name->len, name->s, conid);

	cc = _curl_con_root;
	while(cc) {
		if(conid == cc->conid && cc->name.len == name->len
				&& strncmp(cc->name.s, name->s, name->len) == 0) {
			LM_ERR("duplicate Curl connection name\n");
			return NULL;
		}
		cc = cc->next;
	}

	cc = (curl_con_t *)shm_malloc(sizeof(
			curl_con_t)); /* Connection structures are shared by all children processes */
	if(cc == NULL) {
		LM_ERR("no shm memory\n");
		return NULL;
	}

	/* Each structure is allocated in package memory so each process can write into it without
	   any locks or such stuff */
	ccp = (curl_con_pkg_t *)pkg_malloc(sizeof(curl_con_pkg_t));
	if(ccp == NULL) {
		/* We failed to allocate ccp, so let's free cc and quit */
		shm_free(cc);
		LM_ERR("no pkg memory available\n");
		return NULL;
	}

	memset(cc, 0, sizeof(curl_con_t));
	cc->next = _curl_con_root;
	cc->conid = conid;
	_curl_con_root = cc;
	cc->name = *name;

	/* Put the new ccp first in line */
	memset(ccp, 0, sizeof(curl_con_pkg_t));
	ccp->next = _curl_con_pkg_root;
	ccp->conid = conid;
	ccp->curl = NULL;
	_curl_con_pkg_root = ccp;

	LM_DBG("CURL: Added connection [%.*s]\n", name->len, name->s);
	return cc;
}

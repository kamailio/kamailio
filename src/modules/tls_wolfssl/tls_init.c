/*
 * TLS module
 *
 * Copyright (C) 2005,2006 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*! \defgroup tls Kamailio TLS support
 *
 * This module implements SIP over TCP with TLS encryption.
 * Make sure you read the README file that describes configuration
 * of TLS for single servers and servers hosting multiple domains,
 * and thus using multiple SSL/TLS certificates.
 *
 *
 */
/*!
 * \file
 * \brief Kamailio TLS support :: Initialization
 * \ingroup tls
 * Module: \ref tls
 */


#include <stdio.h>
#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <string.h>
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include "../../core/dprint.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/tcp_init.h"
#include "../../core/socket_info.h"
#include "../../core/pt.h"
#include "../../core/cfg/cfg.h"
#include "../../core/cfg/cfg_ctx.h"
#include "tls_verify.h"
#include "tls_domain.h"
#include "tls_util.h"
#include "tls_wolfssl_mod.h"
#include "tls_init.h"
#include "tls_ct_wrq.h"
#include "tls_cfg.h"

/* will be set to 1 when the TLS env is initialized to make destroy safe */
static int tls_mod_preinitialized = 0;
static int tls_mod_initialized = 0;

#define TLS_COMP_SUPPORT
#define TLS_KERBEROS_SUPPORT

sr_tls_methods_t sr_tls_methods[TLS_METHOD_MAX];

#ifdef NO_TLS_MALLOC_DBG
#undef TLS_MALLOC_DBG /* extra malloc debug info from openssl */
#endif				  /* NO_TLS_MALLOC_DBG */

/*
 * Wrappers around SER shared memory functions
 * (which can be macros)
 */
#ifdef TLS_MALLOC_DBG
#warning "tls module compiled with malloc debugging info (extra overhead)"
#include <execinfo.h>

/*
#define RAND_NULL_MALLOC (1024)
#define NULL_GRACE_PERIOD 10U
*/


inline static char *buf_append(char *buf, char *end, char *str, int str_len)
{
	if((buf + str_len) < end) {
		memcpy(buf, str, str_len);
		return buf + str_len;
	}
	return 0;
}


inline static int backtrace2str(char *buf, int size)
{
	void *bt[32];
	int bt_size, i;
	char **bt_strs;
	char *p;
	char *end;
	char *next;
	char *s;
	char *e;

	p = buf;
	end = buf + size;
	bt_size = backtrace(bt, sizeof(bt) / sizeof(bt[0]));
	bt_strs = backtrace_symbols(bt, bt_size);
	if(bt_strs) {
		p = buf;
		end = buf + size;
		/*if (bt_size>16) bt_size=16;*/ /* go up only 12 entries */
		for(i = 3; i < bt_size; i++) {
			/* try to isolate only the function name*/
			s = strchr(bt_strs[i], '(');
			if(s && ((e = strchr(s, ')')) != 0)) {
				s++;
			} else if((s = strchr(bt_strs[i], '[')) != 0) {
				e = s + strlen(s);
			} else {
				s = bt_strs[i];
				e = s + strlen(s); /* add the whole string */
			}
			next = buf_append(p, end, s, (int)(long)(e - s));
			if(next == 0)
				break;
			else
				p = next;
			if(p < end) {
				*p = ':'; /* separator */
				p++;
			} else
				break;
		}
		if(p == buf) {
			*p = 0;
			p++;
		} else
			*(p - 1) = 0;
		free(bt_strs);
	}
	return (int)(long)(p - buf);
}

static void *ser_malloc(size_t size, const char *file, int line)
{
	void *p;
	char bt_buf[1024];
	int s;
#ifdef RAND_NULL_MALLOC
	static ticks_t st = 0;

	/* start random null returns only after
	 * NULL_GRACE_PERIOD from first call */
	if(st == 0)
		st = get_ticks();
	if(((get_ticks() - st) < NULL_GRACE_PERIOD)
			|| (random() % RAND_NULL_MALLOC)) {
#endif
		s = backtrace2str(bt_buf, sizeof(bt_buf));
		/* ugly hack: keep the bt inside the alloc'ed fragment */
		p = _shm_malloc(size + s, file, "via ser_malloc", line);
		if(p == 0) {
			LM_CRIT("tls - ser_malloc(%d)[%s:%d]==null, bt: %s\n", size, file,
					line, bt_buf);
		} else {
			memcpy(p + size, bt_buf, s);
			((struct qm_frag *)((char *)p - sizeof(struct qm_frag)))->func =
					p + size;
		}
#ifdef RAND_NULL_MALLOC
	} else {
		p = 0;
		backtrace2str(bt_buf, sizeof(bt_buf));
		LM_CRIT("tls - random ser_malloc(%d)[%s:%d] returning null - bt: %s\n",
				size, file, line, bt_buf);
	}
#endif
	return p;
}


static void *ser_realloc(void *ptr, size_t size, const char *file, int line)
{
	void *p;
	char bt_buf[1024];
	int s;
#ifdef RAND_NULL_MALLOC
	static ticks_t st = 0;

	/* start random null returns only after
	 * NULL_GRACE_PERIOD from first call */
	if(st == 0)
		st = get_ticks();
	if(((get_ticks() - st) < NULL_GRACE_PERIOD)
			|| (random() % RAND_NULL_MALLOC)) {
#endif
		s = backtrace2str(bt_buf, sizeof(bt_buf));
		p = _shm_realloc(ptr, size + s, file, "via ser_realloc", line);
		if(p == 0) {
			LM_CRIT("tls - ser_realloc(%p, %d)[%s:%d]==null, bt: %s\n", ptr,
					size, file, line, bt_buf);
		} else {
			memcpy(p + size, bt_buf, s);
			((struct qm_frag *)((char *)p - sizeof(struct qm_frag)))->func =
					p + size;
		}
#ifdef RAND_NULL_MALLOC
	} else {
		p = 0;
		backtrace2str(bt_buf, sizeof(bt_buf));
		LM_CRIT("tls - random ser_realloc(%p, %d)[%s:%d]"
				" returning null - bt: %s\n",
				ptr, size, file, line, bt_buf);
	}
#endif
	return p;
}

#else /*TLS_MALLOC_DBG */
static void *ser_malloc(size_t size)
{
	return shm_malloc(size);
}

static void *ser_realloc(void *ptr, size_t size)
{
	return shm_realloc(ptr, size);
}
#endif

static void ser_free(void *ptr)
{
	if(ptr) {
		shm_free(ptr);
	}
}

#if 0
// up align memory allocations to 16 bytes for
// wolfSSL --enable-aligndata=yes (the default)
static const int MAX_ALIGN = __alignof__(max_align_t);

static void* ser_malloc(size_t size)
{
	char* ptr =  shm_malloc(size + MAX_ALIGN);
	int pad = MAX_ALIGN - ((long) ptr % MAX_ALIGN); // 8 or 16 bytes

	memset(ptr, pad, pad);
	return ptr + pad;
}

static void* ser_realloc(void *ptr, size_t new_size)
{
	if(!ptr) return ser_malloc(new_size);

	int pad = *((char*)ptr - 1); // 8 or 16 bytes
	char *real_ptr = (char*)ptr - pad;

	char *new_ptr = shm_realloc(real_ptr, new_size+MAX_ALIGN);
	int new_pad = MAX_ALIGN - ((long) new_ptr % MAX_ALIGN);
	if (new_pad != pad) {
		memmove(new_ptr + new_pad, new_ptr + pad, new_size);
		memset(new_ptr, new_pad, new_pad);
	}

	return new_ptr + new_pad;
}

static void ser_free(void *ptr)
{
	if (ptr) {
		int pad = *((unsigned char *)ptr - 1);
		shm_free((unsigned char*)ptr - pad);
	}
}
#endif

/*
 * Initialize TLS socket
 */
int tls_h_init_si_f(struct socket_info *si)
{
	int ret;
	/*
	 * reuse tcp initialization
	 */
	ret = tcp_init(si);
	if(ret != 0) {
		LM_ERR("Error while initializing TCP part of TLS socket %.*s:%d\n",
				si->address_str.len, si->address_str.s, si->port_no);
		goto error;
	}

	si->proto = PROTO_TLS;
	return 0;

error:
	if(si->socket != -1) {
		close(si->socket);
		si->socket = -1;
	}
	return ret;
}


/*
 * initialize ssl methods
 */
static void init_ssl_methods(void)
{
	/* openssl 1.1.0+ */
	memset(sr_tls_methods, 0, sizeof(sr_tls_methods));

	/* any SSL/TLS version */
	sr_tls_methods[TLS_USE_SSLv23_cli - 1].TLSMethod = TLS_client_method();
	sr_tls_methods[TLS_USE_SSLv23_srv - 1].TLSMethod = TLS_server_method();
	sr_tls_methods[TLS_USE_SSLv23 - 1].TLSMethod = TLS_method();

#ifndef OPENSSL_NO_SSL3_METHOD
	sr_tls_methods[TLS_USE_SSLv3_cli - 1].TLSMethod = TLS_client_method();
	sr_tls_methods[TLS_USE_SSLv3_cli - 1].TLSMethodMin = SSL3_VERSION;
	sr_tls_methods[TLS_USE_SSLv3_cli - 1].TLSMethodMax = SSL3_VERSION;
	sr_tls_methods[TLS_USE_SSLv3_srv - 1].TLSMethod = TLS_server_method();
	sr_tls_methods[TLS_USE_SSLv3_srv - 1].TLSMethodMin = SSL3_VERSION;
	sr_tls_methods[TLS_USE_SSLv3_srv - 1].TLSMethodMax = SSL3_VERSION;
	sr_tls_methods[TLS_USE_SSLv3 - 1].TLSMethod = TLS_method();
	sr_tls_methods[TLS_USE_SSLv3 - 1].TLSMethodMin = SSL3_VERSION;
	sr_tls_methods[TLS_USE_SSLv3 - 1].TLSMethodMax = SSL3_VERSION;
#endif

	sr_tls_methods[TLS_USE_TLSv1_cli - 1].TLSMethod = TLS_client_method();
	sr_tls_methods[TLS_USE_TLSv1_cli - 1].TLSMethodMin = TLS1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_cli - 1].TLSMethodMax = TLS1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_srv - 1].TLSMethod = TLS_server_method();
	sr_tls_methods[TLS_USE_TLSv1_srv - 1].TLSMethodMin = TLS1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_srv - 1].TLSMethodMax = TLS1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1 - 1].TLSMethod = TLS_method();
	sr_tls_methods[TLS_USE_TLSv1 - 1].TLSMethodMin = TLS1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1 - 1].TLSMethodMax = TLS1_VERSION;

	sr_tls_methods[TLS_USE_TLSv1_1_cli - 1].TLSMethod = TLS_client_method();
	sr_tls_methods[TLS_USE_TLSv1_1_cli - 1].TLSMethodMin = TLS1_1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_1_cli - 1].TLSMethodMax = TLS1_1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_1_srv - 1].TLSMethod = TLS_server_method();
	sr_tls_methods[TLS_USE_TLSv1_1_srv - 1].TLSMethodMin = TLS1_1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_1_srv - 1].TLSMethodMax = TLS1_1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_1 - 1].TLSMethod = TLS_method();
	sr_tls_methods[TLS_USE_TLSv1_1 - 1].TLSMethodMin = TLS1_1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_1 - 1].TLSMethodMax = TLS1_1_VERSION;

	sr_tls_methods[TLS_USE_TLSv1_2_cli - 1].TLSMethod = TLS_client_method();
	sr_tls_methods[TLS_USE_TLSv1_2_cli - 1].TLSMethodMin = TLS1_2_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_2_cli - 1].TLSMethodMax = TLS1_2_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_2_srv - 1].TLSMethod = TLS_server_method();
	sr_tls_methods[TLS_USE_TLSv1_2_srv - 1].TLSMethodMin = TLS1_2_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_2_srv - 1].TLSMethodMax = TLS1_2_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_2 - 1].TLSMethod = TLS_method();
	sr_tls_methods[TLS_USE_TLSv1_2 - 1].TLSMethodMin = TLS1_2_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_2 - 1].TLSMethodMax = TLS1_2_VERSION;

	sr_tls_methods[TLS_USE_TLSv1_3_cli - 1].TLSMethod = TLS_client_method();
	sr_tls_methods[TLS_USE_TLSv1_3_cli - 1].TLSMethodMin = TLS1_3_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_3_cli - 1].TLSMethodMax = TLS1_3_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_3_srv - 1].TLSMethod = TLS_server_method();
	sr_tls_methods[TLS_USE_TLSv1_3_srv - 1].TLSMethodMin = TLS1_3_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_3_srv - 1].TLSMethodMax = TLS1_3_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_3 - 1].TLSMethod = TLS_method();
	sr_tls_methods[TLS_USE_TLSv1_3 - 1].TLSMethodMin = TLS1_3_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_3 - 1].TLSMethodMax = TLS1_3_VERSION;

	/* ranges of TLS versions (require a minimum TLS version) */
	sr_tls_methods[TLS_USE_TLSv1_PLUS - 1].TLSMethod = TLS_method();
	sr_tls_methods[TLS_USE_TLSv1_PLUS - 1].TLSMethodMin = TLS1_VERSION;

	sr_tls_methods[TLS_USE_TLSv1_1_PLUS - 1].TLSMethod = TLS_method();
	sr_tls_methods[TLS_USE_TLSv1_1_PLUS - 1].TLSMethodMin = TLS1_1_VERSION;

	sr_tls_methods[TLS_USE_TLSv1_2_PLUS - 1].TLSMethod = TLS_method();
	sr_tls_methods[TLS_USE_TLSv1_2_PLUS - 1].TLSMethodMin = TLS1_2_VERSION;

	sr_tls_methods[TLS_USE_TLSv1_3_PLUS - 1].TLSMethod = TLS_method();
	sr_tls_methods[TLS_USE_TLSv1_3_PLUS - 1].TLSMethodMin = TLS1_3_VERSION;
}


/*
 * Fix openssl compression bugs if necessary
 */
static int init_tls_compression(void)
{
	return 0;
}


/**
 * tls pre-init function
 * - executed when module is loaded
 */
int tls_pre_init(void)
{
	void *(*mf)(size_t, const char *, int) = NULL;
	void *(*rf)(void *, size_t, const char *, int) = NULL;
	void (*ff)(void *, const char *, int) = NULL;

#ifdef KSR_LIBSSL_STATIC
	LM_INFO("libssl linked mode: static\n");
#endif

	/*
	 * this has to be called before any function calling CRYPTO_malloc,
	 * CRYPTO_malloc will set allow_customize in openssl to 0
	 */
	// CRYPTO_get_mem_functions(&mf, &rf, &ff);
	LM_DBG("initial memory functions - malloc: %p realloc: %p free: %p\n", mf,
			rf, ff);
	mf = NULL;
	rf = NULL;
	ff = NULL;
	if(wolfSSL_SetAllocators(ser_malloc, ser_free, ser_realloc)) {
		LM_ERR("Unable to set the memory allocation functions\n");
		// CRYPTO_get_mem_functions(&mf, &rf, &ff);
		LM_ERR("libssl current mem functions - m: %p r: %p f: %p\n", mf, rf,
				ff);
		LM_ERR("module mem functions - m: %p r: %p f: %p\n", ser_malloc,
				ser_realloc, ser_free);
		LM_ERR("Be sure tls module is loaded before any other module using"
			   " libssl (can be loaded first to be safe)\n");
		return -1;
	}
	LM_DBG("updated memory functions - malloc: %p realloc: %p free: %p\n",
			ser_malloc, ser_realloc, ser_free);

	init_tls_compression();
	return 0;
}

/**
 * tls mod pre-init function
 * - executed before any mod_init()
 */
int tls_h_mod_pre_init_f(void)
{
	if(tls_mod_preinitialized == 1) {
		LM_DBG("already mod pre-initialized\n");
		return 0;
	}
	LM_DBG("preparing tls env for modules initialization\n");

	LM_DBG("preparing tls env for modules initialization (libssl >=1.1)\n");
	wolfSSL_OPENSSL_init_ssl(0, NULL);
	wolfSSL_load_error_strings();
	tls_mod_preinitialized = 1;
	return 0;
}

/*
 * First step of TLS initialization
 */
int tls_h_mod_init_f(void)
{
	if(tls_mod_initialized == 1) {
		LM_DBG("already initialized\n");
		return 0;
	}
	LM_DBG("initializing tls system\n");

	init_ssl_methods();
	tls_mod_initialized = 1;
	return 0;
}


/*
 * Make sure that all server domains in the configuration have corresponding
 * listening socket in SER
 */
int tls_check_sockets(tls_domains_cfg_t *cfg)
{
	tls_domain_t *d;

	if(!cfg)
		return 0;

	d = cfg->srv_list;
	while(d) {
		if(d->ip.len && !find_si(&d->ip, d->port, PROTO_TLS)) {
			LM_ERR("%s: No listening socket found\n", tls_domain_str(d));
			return -1;
		}
		d = d->next;
	}
	return 0;
}


/*
 * TLS cleanup when application exits
 */
void tls_h_mod_destroy_f(void)
{
	LM_DBG("tls module final tls destroy\n");
	if(tls_mod_preinitialized > 0)
		ERR_free_strings();
	/* TODO: free all the ctx'es */
	tls_destroy_cfg();
	tls_ct_wq_destroy();
	/* explicit execution of libssl cleanup to avoid being executed again
	 * by atexit(), when shm is gone */
	LM_DBG("executing openssl v1.1+ cleanup\n");
	wolfSSL_Cleanup();
}

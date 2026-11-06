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
#include <openssl/ssl.h>

#if OPENSSL_VERSION_NUMBER >= 0x030000000L
#include <openssl/rand.h>
#endif

#include "../../core/dprint.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/tcp_init.h"
#include "../../core/socket_info.h"
#include "../../core/pt.h"
#include "../../core/cfg/cfg.h"
#include "tls_verify.h"
#include "tls_domain.h"
#include "tls_util.h"
#include "tls_mod.h"
#include "tls_init.h"
#include "tls_ct_wrq.h"
#include "tls_cfg.h"
#include "tls_openssl.h"

/* will be set to 1 when the TLS env is initialized to make destroy safe */
static int tls_mod_preinitialized = 0;
static int tls_mod_initialized = 0;

sr_tls_methods_t sr_tls_methods[TLS_METHOD_MAX];

#ifdef NO_TLS_MALLOC_DBG
#undef TLS_MALLOC_DBG /* extra malloc debug info from openssl */
#endif				  /* NO_TLS_MALLOC_DBG */

/* mode 0: libssl allocates from shm through these hooks */
#ifndef KSR_TLS_LIBSSL_COMPAT
#define KSR_TLS_MEM_HOOKS
#endif

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


#ifdef KSR_TLS_MEM_HOOKS
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
#endif

#ifdef RAND_NULL_MALLOC
	/* start random null returns only after
	 * NULL_GRACE_PERIOD from first call */
	if(st == 0)
		st = get_ticks();
	if(((get_ticks() - st) < NULL_GRACE_PERIOD)
			|| (random() % RAND_NULL_MALLOC)) {
#endif
		s = backtrace2str(bt_buf, sizeof(bt_buf));
		/* ugly hack: keep the bt inside the alloc'ed fragment */
		p = shm_mallocxp(size + s, file, "ser_malloc", line, "tls");
		if(p == 0) {
			LM_CRIT("tls - ser_malloc(%lu)[%s:%d]==null, bt: %s\n", size, file,
					line, bt_buf);
		} else {
			memcpy(p + size, bt_buf, s);
			shm_setfunc(p, p + size);
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
#endif

#ifdef RAND_NULL_MALLOC
	/* start random null returns only after
	 * NULL_GRACE_PERIOD from first call */
	if(st == 0)
		st = get_ticks();
	if(((get_ticks() - st) < NULL_GRACE_PERIOD)
			|| (random() % RAND_NULL_MALLOC)) {
#endif
		s = backtrace2str(bt_buf, sizeof(bt_buf));
		p = shm_reallocxp(ptr, size + s, file, "ser_realloc", line, "tls");
		if(p == 0) {
			LM_CRIT("tls - ser_realloc(%p, %lu)[%s:%d]==null, bt: %s\n", ptr,
					size, file, line, bt_buf);
		} else {
			memcpy(p + size, bt_buf, s);
			shm_setfunc(p, p + size);
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

static void ser_free(void *ptr, const char *fname, int fline)
{
	if(ptr) {
		shm_free(ptr);
	}
}

#endif /* KSR_TLS_MEM_HOOKS */

#else /*TLS_MALLOC_DBG */

#ifdef KSR_TLS_MEM_HOOKS

static void *ser_malloc(size_t size, const char *fname, int fline)
{
	void *p;
	p = shm_malloc(size);
	return p;
}


static void *ser_realloc(void *ptr, size_t size, const char *fname, int fline)
{
	void *p;
	p = shm_realloc(ptr, size);
	return p;
}

/* libssl calls free() with NULL; shm_free() aborts on it */
static void ser_free(void *ptr, const char *fname, int fline)
{
	if(ptr) {
		shm_free(ptr);
	}
}

#endif /* KSR_TLS_MEM_HOOKS */

#endif /* TLS_MALLOC_DBG */

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
	memset(sr_tls_methods, 0, sizeof(sr_tls_methods));

	/* any SSL/TLS version */
	sr_tls_methods[TLS_USE_SSLv23_cli - 1].TLSMethod = TLS_client_method();
	sr_tls_methods[TLS_USE_SSLv23_srv - 1].TLSMethod = TLS_server_method();
	sr_tls_methods[TLS_USE_SSLv23 - 1].TLSMethod = TLS_method();

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


/**
 * tls pre-init function
 * - executed when module is loaded
 */
int tls_pre_init(void)
{
#ifdef KSR_TLS_MEM_HOOKS
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
	CRYPTO_get_mem_functions(&mf, &rf, &ff);
	LM_DBG("initial memory functions - malloc: %p realloc: %p free: %p\n", mf,
			rf, ff);
	mf = NULL;
	rf = NULL;
	ff = NULL;

	if(ksr_tcp_main_threads == 0) {
		if(!CRYPTO_set_mem_functions(ser_malloc, ser_realloc, ser_free)) {
			LM_ERR("Unable to set the memory allocation functions\n");
			CRYPTO_get_mem_functions(&mf, &rf, &ff);
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
	}
#endif /* KSR_TLS_MEM_HOOKS */

	return 0;
}

/**
 * tls mod pre-init function
 * - executed before any mod_init()
 */
#if OPENSSL_VERSION_NUMBER >= 0x030000000L
/*
 * With late initialisation it is not necessary to
 * enable threading on the EVP_RAND_CTX. This function
 * left here in case more complex requirements arise in
 * OpenSSL >= 3.2.
 */
long tls_h_mod_randctx(void *param)
{
	do {
		OSSL_LIB_CTX *osslglobal = NULL;
		EVP_RAND_CTX *randctx = NULL;

		LM_DBG("enabling locking for rand ctx\n");

		osslglobal = OSSL_LIB_CTX_get0_global_default();
		if(osslglobal == NULL) {
			LM_ERR("failed to get lib ssl global ctx\n");
			return -1L;
		}

		randctx = RAND_get0_primary(osslglobal);
		if(randctx == NULL) {
			LM_ERR("primary rand ctx is null\n");
			return -1L;
		}
		EVP_RAND_enable_locking(randctx);

		randctx = RAND_get0_public(osslglobal);
		if(randctx == NULL) {
			LM_ERR("public rand ctx is null\n");
			return -1L;
		}
		EVP_RAND_enable_locking(randctx);

		randctx = RAND_get0_private(osslglobal);
		if(randctx == NULL) {
			LM_ERR("private rand ctx is null\n");
			return -1L;
		}
		EVP_RAND_enable_locking(randctx);
	} while(0);

	return 0L;
}
#endif /* OPENSSL_VERSION_NUMBER */

int tls_h_mod_pre_init_f(void)
{
	if(tls_mod_preinitialized == 1) {
		LM_DBG("already mod pre-initialized\n");
		return 0;
	}
	LM_DBG("preparing tls env for modules initialization\n");

#if 0
#if OPENSSL_VERSION_NUMBER >= 0x030000000L
        /*
         * With deferred initialisation it is not necessary to enable threading
         * on the EVP_RAND_CTX. We leave this block here as an example of how
         * to do it in case of future requirements.
         */
        pthread_t tid;
        long rl;
        pthread_create(&tid, NULL, (void *(*)(void *))tls_h_mod_randctx, NULL);
        pthread_join(tid, (void **)&rl);
        if ((int)rl)
            return (int)rl;
#endif /* OPENSSL_VERSION_NUMBER */
#endif /* 0 */

	tls_mod_preinitialized = 1;
	return 0;
}

/*
 * First step of TLS initialization
 */
int tls_h_mod_init_f(void)
{
	/*struct socket_info* si;*/
	long ssl_version;
	const char *ssl_version_txt;

	if(tls_mod_initialized == 1) {
		LM_DBG("already initialized\n");
		return 0;
	}
	LM_DBG("initializing tls system\n");

	ssl_version = OpenSSL_version_num();
	ssl_version_txt = OpenSSL_version(OPENSSL_VERSION);

	/* check if version have the same major minor and fix level
	 * (e.g. 0.9.8a & 0.9.8c are ok, but 0.9.8 and 0.9.9x are not)
	 * - values is represented as 0xMMNNFFPPS: major minor fix patch status
	 *   0x00090705f == 0.9.7e release */
	if((ssl_version >> 12) != (OPENSSL_VERSION_NUMBER >> 12)) {
		LM_CRIT("installed openssl library"
				" version is too different from the library the " NAME " tls"
				" module was compiled with: installed \"%s\" (0x%08lx),"
				" compiled \"%s\" (0x%08lx).\n"
				" Please make sure a compatible version is used"
				" (tls_force_run in kamailio.cfg will override this check)\n",
				ssl_version_txt, ssl_version, OPENSSL_VERSION_TEXT,
				(long)OPENSSL_VERSION_NUMBER);
		if(cfg_get(tls, tls_cfg, force_run))
			LM_WARN("tls_force_run turned on, ignoring "
					" openssl version mismatch\n");
		else
			return -1; /* safer to exit */
	}

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
	/* TODO: free all the ctx'es */
	tls_destroy_cfg();
	tls_ct_wq_destroy();
	/* explicit execution of libssl cleanup to avoid being executed again
	 * by atexit(), when shm is gone */
	LM_DBG("executing openssl cleanup\n");
	OPENSSL_cleanup();
}

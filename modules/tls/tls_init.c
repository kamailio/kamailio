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
 * This modules implements SIP over TCP with TLS encryption.
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
 
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../tcp_init.h"
#include "../../socket_info.h"
#include "../../pt.h"
#include "../../cfg/cfg.h"
#include "../../cfg/cfg_ctx.h"
#include "tls_verify.h"
#include "tls_domain.h"
#include "tls_util.h"
#include "tls_mod.h"
#include "tls_init.h"
#include "tls_locking.h"
#include "tls_ct_wrq.h"
#include "tls_cfg.h"

/* will be set to 1 when the TLS env is initialized to make destroy safe */
static int tls_mod_preinitialized = 0;
static int tls_mod_initialized = 0;

#if OPENSSL_VERSION_NUMBER < 0x00907000L
#    warning ""
#    warning "==============================================================="
#    warning " Your version of OpenSSL is < 0.9.7."
#    warning " Upgrade for better compatibility, features and security fixes!"
#    warning "==============================================================="
#    warning ""
#endif

/* replace openssl zlib compression with our version if necessary
 * (the openssl zlib compression uses the wrong malloc, see
 *  openssl #1468): 0.9.8-dev < version  <0.9.8e-beta1 */
#if OPENSSL_VERSION_NUMBER >= 0x00908000L  /* 0.9.8-dev */ && \
		OPENSSL_VERSION_NUMBER <  0x00908051L  /* 0.9.8.e-beta1 */
#    ifndef OPENSSL_NO_COMP
#        warning "openssl zlib compression bug workaround enabled"
#    endif
#    define TLS_FIX_ZLIB_COMPRESSION
#    include "fixed_c_zlib.h"
#endif

#ifdef TLS_KSSL_WORKARROUND
#if OPENSSL_VERSION_NUMBER < 0x00908050L
#	warning "openssl lib compiled with kerberos support which introduces a bug\
 (wrong malloc/free used in kssl.c) -- attempting workaround"
#	warning "NOTE: if you don't link libssl staticaly don't try running the \
compiled code on a system with a differently compiled openssl (it's safer \
to compile on the  _target_ system)"
#endif /* OPENSSL_VERSION_NUMBER */
#endif /* TLS_KSSL_WORKARROUND */

/* openssl < 1. 0 */
#if OPENSSL_VERSION_NUMBER < 0x01000000L
#	warning "openssl < 1.0: no TLS extensions or server name support"
#endif /* OPENSSL_VERION < 1.0 */



#ifndef OPENSSL_NO_COMP
#define TLS_COMP_SUPPORT
#else
#undef TLS_COMP_SUPPORT
#endif

#ifndef OPENSSL_NO_KRB5
#define TLS_KERBEROS_SUPPORT
#else
#undef TLS_KERBEROS_SUPPORT
#endif


#ifdef TLS_KSSL_WORKARROUND
int openssl_kssl_malloc_bug=0; /* is openssl bug #1467 present ? */
#endif

const SSL_METHOD* ssl_methods[TLS_METHOD_MAX];

#ifdef NO_TLS_MALLOC_DBG
#undef TLS_MALLOC_DBG /* extra malloc debug info from openssl */
#endif /* NO_TLS_MALLOC_DBG */

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



inline static char* buf_append(char* buf, char* end, char* str, int str_len)
{
	if ( (buf+str_len)<end){
		memcpy(buf, str, str_len);
		return buf+str_len;
	}
	return 0;
}


inline static int backtrace2str(char* buf, int size)
{
	void* bt[32];
	int bt_size, i;
	char** bt_strs;
	char* p;
	char* end;
	char* next;
	char* s;
	char* e;
	
	p=buf; end=buf+size;
	bt_size=backtrace(bt, sizeof(bt)/sizeof(bt[0]));
	bt_strs=backtrace_symbols(bt, bt_size);
	if (bt_strs){
		p=buf; end=buf+size;
		/*if (bt_size>16) bt_size=16;*/ /* go up only 12 entries */
		for (i=3; i< bt_size; i++){
			/* try to isolate only the function name*/
			s=strchr(bt_strs[i], '(');
			if (s && ((e=strchr(s, ')'))!=0)){
				s++;
			}else if ((s=strchr(bt_strs[i], '['))!=0){
				e=s+strlen(s);
			}else{
				s=bt_strs[i]; e=s+strlen(s); /* add thw whole string */
			}
			next=buf_append(p, end, s, (int)(long)(e-s));
			if (next==0) break;
			else p=next;
			if (p<end){
				*p=':'; /* separator */
				p++;
			}else break;
		}
		if (p==buf){
			*p=0;
			p++;
		}else
			*(p-1)=0;
		free(bt_strs);
	}
	return (int)(long)(p-buf);
}

static void* ser_malloc(size_t size, const char* file, int line)
{
	void  *p;
	char bt_buf[1024];
	int s;
#ifdef RAND_NULL_MALLOC
	static ticks_t st=0;

	/* start random null returns only after 
	 * NULL_GRACE_PERIOD from first call */
	if (st==0) st=get_ticks();
	if (((get_ticks()-st)<NULL_GRACE_PERIOD) || (random()%RAND_NULL_MALLOC)){
#endif
		s=backtrace2str(bt_buf, sizeof(bt_buf));
		/* ugly hack: keep the bt inside the alloc'ed fragment */
		p=_shm_malloc(size+s, file, "via ser_malloc", line);
		if (p==0){
			LOG(L_CRIT, "tsl: ser_malloc(%d)[%s:%d]==null, bt: %s\n", 
						size, file, line, bt_buf);
		}else{
			memcpy(p+size, bt_buf, s);
			((struct qm_frag*)((char*)p-sizeof(struct qm_frag)))->func=
				p+size;
		}
#ifdef RAND_NULL_MALLOC
	}else{
		p=0;
		backtrace2str(bt_buf, sizeof(bt_buf));
		LOG(L_CRIT, "tsl: random ser_malloc(%d)[%s:%d]"
				" returning null - bt: %s\n",
				size, file, line, bt_buf);
	}
#endif
	return p;
}


static void* ser_realloc(void *ptr, size_t size, const char* file, int line)
{
	void  *p;
	char bt_buf[1024];
	int s;
#ifdef RAND_NULL_MALLOC
	static ticks_t st=0;

	/* start random null returns only after 
	 * NULL_GRACE_PERIOD from first call */
	if (st==0) st=get_ticks();
	if (((get_ticks()-st)<NULL_GRACE_PERIOD) || (random()%RAND_NULL_MALLOC)){
#endif
		s=backtrace2str(bt_buf, sizeof(bt_buf));
		p=_shm_realloc(ptr, size+s, file, "via ser_realloc", line);
		if (p==0){
			LOG(L_CRIT, "tsl: ser_realloc(%p, %d)[%s:%d]==null, bt: %s\n",
					ptr, size, file, line, bt_buf);
		}else{
			memcpy(p+size, bt_buf, s);
			((struct qm_frag*)((char*)p-sizeof(struct qm_frag)))->func=
				p+size;
		}
#ifdef RAND_NULL_MALLOC
	}else{
		p=0;
		backtrace2str(bt_buf, sizeof(bt_buf));
		LOG(L_CRIT, "tsl: random ser_realloc(%p, %d)[%s:%d]"
					" returning null - bt: %s\n", ptr, size, file, line,
					bt_buf);
	}
#endif
	return p;
}

#else /*TLS_MALLOC_DBG */

static void* ser_malloc(size_t size)
{
	return shm_malloc(size);
}


static void* ser_realloc(void *ptr, size_t size)
{
		return shm_realloc(ptr, size);
}

#endif

static void ser_free(void *ptr)
{
	/* The memory functions provided to openssl needs to behave like standard
	 * memory functions, i.e. free(). Therefore, ser_free must accept NULL
	 * pointers, see: http://openssl.6102.n7.nabble.com/Custom-free-routine-is-invoked-with-NULL-argument-in-openssl-1-0-1-td25937.html
	 * As shm_free() aborts on null pointers, we have to check for null pointer
	 * here in the wrapper function.
	 */
	if (ptr) {
		shm_free(ptr);
	}
}


/*
 * Initialize TLS socket
 */
int tls_h_init_si(struct socket_info *si)
{
	int ret;
	     /*
	      * reuse tcp initialization 
	      */
	ret = tcp_init(si);
	if (ret != 0) {
		ERR("Error while initializing TCP part of TLS socket %.*s:%d\n",
		    si->address_str.len, si->address_str.s, si->port_no);
		goto error;
	}
	
	si->proto = PROTO_TLS;
	return 0;
	
 error:
	if (si->socket != -1) {
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
	memset(ssl_methods, 0, sizeof(ssl_methods));

	/* any SSL/TLS version */
	ssl_methods[TLS_USE_SSLv23_cli - 1] = SSLv23_client_method();
	ssl_methods[TLS_USE_SSLv23_srv - 1] = SSLv23_server_method();
	ssl_methods[TLS_USE_SSLv23 - 1] = SSLv23_method();

	/* only specific SSL or TLS version */
#ifndef OPENSSL_NO_SSL2
	ssl_methods[TLS_USE_SSLv2_cli - 1] = SSLv2_client_method();
	ssl_methods[TLS_USE_SSLv2_srv - 1] = SSLv2_server_method();
	ssl_methods[TLS_USE_SSLv2 - 1] = SSLv2_method();
#endif

	ssl_methods[TLS_USE_SSLv3_cli - 1] = SSLv3_client_method();
	ssl_methods[TLS_USE_SSLv3_srv - 1] = SSLv3_server_method();
	ssl_methods[TLS_USE_SSLv3 - 1] = SSLv3_method();

	ssl_methods[TLS_USE_TLSv1_cli - 1] = TLSv1_client_method();
	ssl_methods[TLS_USE_TLSv1_srv - 1] = TLSv1_server_method();
	ssl_methods[TLS_USE_TLSv1 - 1] = TLSv1_method();

#if OPENSSL_VERSION_NUMBER >= 0x1000100fL
	ssl_methods[TLS_USE_TLSv1_1_cli - 1] = TLSv1_1_client_method();
	ssl_methods[TLS_USE_TLSv1_1_srv - 1] = TLSv1_1_server_method();
	ssl_methods[TLS_USE_TLSv1_1 - 1] = TLSv1_1_method();
#endif

#if OPENSSL_VERSION_NUMBER >= 0x1000105fL
	ssl_methods[TLS_USE_TLSv1_2_cli - 1] = TLSv1_2_client_method();
	ssl_methods[TLS_USE_TLSv1_2_srv - 1] = TLSv1_2_server_method();
	ssl_methods[TLS_USE_TLSv1_2 - 1] = TLSv1_2_method();
#endif

	/* ranges of TLS versions (require a minimum TLS version) */
	ssl_methods[TLS_USE_TLSv1_PLUS - 1] = (void*)TLS_OP_TLSv1_PLUS;

#if OPENSSL_VERSION_NUMBER >= 0x1000100fL
	ssl_methods[TLS_USE_TLSv1_1_PLUS - 1] = (void*)TLS_OP_TLSv1_1_PLUS;
#endif

#if OPENSSL_VERSION_NUMBER >= 0x1000105fL
	ssl_methods[TLS_USE_TLSv1_2_PLUS - 1] = (void*)TLS_OP_TLSv1_2_PLUS;
#endif
}


/*
 * Fix openssl compression bugs if necessary
 */
static int init_tls_compression(void)
{
#if OPENSSL_VERSION_NUMBER >= 0x00908000L
	int n, r;
	STACK_OF(SSL_COMP)* comp_methods;
	SSL_COMP* zlib_comp;
	long ssl_version;
	
	/* disabling compression */
#	ifndef SSL_COMP_ZLIB_IDX
#		define SSL_COMP_ZLIB_IDX 1 /* openssl/ssl/ssl_ciph.c:84 */
#	endif 
	comp_methods = SSL_COMP_get_compression_methods();
	if (comp_methods == 0) {
		LOG(L_INFO, "tls: init_tls: compression support disabled in the"
					" openssl lib\n");
		goto end; /* nothing to do, exit */
	} else if (cfg_get(tls, tls_cfg, disable_compression)){
		LOG(L_INFO, "tls: init_tls: disabling compression...\n");
		sk_SSL_COMP_zero(comp_methods);
	}else{
		ssl_version=SSLeay();
		/* replace openssl zlib compression with our version if necessary
		 * (the openssl zlib compression uses the wrong malloc, see
		 *  openssl #1468): 0.9.8-dev < version  <0.9.8e-beta1 */
		if ((ssl_version >= 0x00908000L) && (ssl_version < 0x00908051L)){
			/* the above SSL_COMP_get_compression_methods() call has the side
			 * effect of initializing the compression stack (if not already
			 * initialized) => after it zlib is initialized and in the stack */
			/* find zlib_comp (cannot use ssl3_comp_find, not exported) */
			n = sk_SSL_COMP_num(comp_methods);
			zlib_comp = 0;
			for (r = 0; r < n; r++) {
				zlib_comp = sk_SSL_COMP_value(comp_methods, r);
				DBG("tls: init_tls: found compression method %p id %d\n",
						zlib_comp, zlib_comp->id);
				if (zlib_comp->id == SSL_COMP_ZLIB_IDX) {
					DBG("tls: init_tls: found zlib compression (%d)\n", 
							SSL_COMP_ZLIB_IDX);
					break /* found */;
				} else {
					zlib_comp = 0;
				}
			}
			if (zlib_comp == 0) {
				LOG(L_INFO, "tls: init_tls: no openssl zlib compression "
							"found\n");
			}else{
				LOG(L_WARN, "tls: init_tls: detected openssl lib with "
							"known zlib compression bug: \"%s\" (0x%08lx)\n",
							SSLeay_version(SSLEAY_VERSION), ssl_version);
#	ifdef TLS_FIX_ZLIB_COMPRESSION
				LOG(L_WARN, "tls: init_tls: enabling openssl zlib compression "
							"bug workaround (replacing zlib COMP method with "
							"our own version)\n");
				/* hack: make sure that the CRYPTO_EX_INDEX_COMP class is empty
				 * and it does not contain any free_ex_data from the 
				 * built-in zlib. This can happen if the current openssl
				 * zlib malloc fix patch is used (CRYPTO_get_ex_new_index() in
				 * COMP_zlib()). Unfortunately the only way
				 * to do this it to cleanup all the ex_data stuff.
				 * It should be safe if this is executed before SSL_init()
				 * (only the COMP class is initialized before).
				 */
				CRYPTO_cleanup_all_ex_data();
				
				if (fixed_c_zlib_init() != 0) {
					LOG(L_CRIT, "tls: init_tls: BUG: failed to initialize zlib"
							" compression fix, disabling compression...\n");
					sk_SSL_COMP_zero(comp_methods); /* delete compression */
					goto end;
				}
				/* "fix" it */
				zlib_comp->method = &zlib_method;
#	else
				LOG(L_WARN, "tls: init_tls: disabling openssl zlib "
							"compression \n");
				zlib_comp=sk_SSL_COMP_delete(comp_methods, r);
				if (zlib_comp)
					OPENSSL_free(zlib_comp);
#	endif
			}
		}
	}
end:
#endif /* OPENSSL_VERSION_NUMBER >= 0.9.8 */
	return 0;
}


/**
 * tls pre-init function
 * - executed when module is loaded
 */
int tls_pre_init(void)
{
	     /*
	      * this has to be called before any function calling CRYPTO_malloc,
	      * CRYPTO_malloc will set allow_customize in openssl to 0
	      */
#ifdef TLS_MALLOC_DBG
	if (!CRYPTO_set_mem_ex_functions(ser_malloc, ser_realloc, ser_free)) {
#else
	if (!CRYPTO_set_mem_functions(ser_malloc, ser_realloc, ser_free)) {
#endif
		ERR("Unable to set the memory allocation functions\n");
		return -1;
	}

	if (tls_init_locks()<0)
		return -1;

	init_tls_compression();

	return 0;
}

/**
 * tls mod pre-init function
 * - executed before any mod_init()
 */
int tls_mod_pre_init_h(void)
{
	if(tls_mod_preinitialized==1) {
		LM_DBG("already mod pre-initialized\n");
		return 0;
	}
	DBG("============= :preparing tls env for modules initialization\n");
	SSL_library_init();
	SSL_load_error_strings();
	tls_mod_preinitialized=1;
	return 0;
}

/*
 * First step of TLS initialization
 */
int init_tls_h(void)
{
	/*struct socket_info* si;*/
	long ssl_version;
	int lib_kerberos;
	int lib_zlib;
	int kerberos_support;
	int comp_support;
	const char* lib_cflags;
	int low_mem_threshold1;
	int low_mem_threshold2;
	str tls_grp;
	str s;
	cfg_ctx_t* cfg_ctx;

	if(tls_mod_initialized == 1) {
		LM_DBG("already initialized\n");
		return 0;
	}
	DBG("initializing tls system\n");

#if OPENSSL_VERSION_NUMBER < 0x00907000L
	WARN("You are using an old version of OpenSSL (< 0.9.7). Upgrade!\n");
#endif
	ssl_version=SSLeay();
	/* check if version have the same major minor and fix level
	 * (e.g. 0.9.8a & 0.9.8c are ok, but 0.9.8 and 0.9.9x are not) */
	if ((ssl_version>>8)!=(OPENSSL_VERSION_NUMBER>>8)){
		LOG(L_CRIT, "ERROR: tls: init_tls_h: installed openssl library "
				"version is too different from the library the Kamailio tls module "
				"was compiled with: installed \"%s\" (0x%08lx), compiled "
				"\"%s\" (0x%08lx).\n"
				" Please make sure a compatible version is used"
				" (tls_force_run in kamailio.cfg will override this check)\n",
				SSLeay_version(SSLEAY_VERSION), ssl_version,
				OPENSSL_VERSION_TEXT, (long)OPENSSL_VERSION_NUMBER);
		if (cfg_get(tls, tls_cfg, force_run))
			LOG(L_WARN, "tls: init_tls_h: tls_force_run turned on, ignoring "
						" openssl version mismatch\n");
		else
			return -1; /* safer to exit */
	}
#ifdef TLS_KERBEROS_SUPPORT
	kerberos_support=1;
#else
	kerberos_support=0;
#endif
#ifdef TLS_COMP_SUPPORT
	comp_support=1;
#else
	comp_support=0;
#endif
	/* attempt to guess if the library was compiled with kerberos or
	 * compression support from the cflags */
	lib_cflags=SSLeay_version(SSLEAY_CFLAGS);
	lib_kerberos=0;
	lib_zlib=0;
	if ((lib_cflags==0) || strstr(lib_cflags, "not available")){ 
		lib_kerberos=-1;
		lib_zlib=-1;
	}else{
		if (strstr(lib_cflags, "-DZLIB"))
			lib_zlib=1;
		if (strstr(lib_cflags, "-DKRB5_"))
			lib_kerberos=1;
	}
	LOG(L_INFO, "tls: _init_tls_h:  compiled  with  openssl  version " 
				"\"%s\" (0x%08lx), kerberos support: %s, compression: %s\n",
				OPENSSL_VERSION_TEXT, (long)OPENSSL_VERSION_NUMBER,
				kerberos_support?"on":"off", comp_support?"on":"off");
	LOG(L_INFO, "tls: init_tls_h: installed openssl library version "
				"\"%s\" (0x%08lx), kerberos support: %s, "
				" zlib compression: %s"
				"\n %s\n",
				SSLeay_version(SSLEAY_VERSION), ssl_version,
				(lib_kerberos==1)?"on":(lib_kerberos==0)?"off":"unknown",
				(lib_zlib==1)?"on":(lib_zlib==0)?"off":"unknown",
				SSLeay_version(SSLEAY_CFLAGS));
	if (lib_kerberos!=kerberos_support){
		if (lib_kerberos!=-1){
			LOG(L_CRIT, "ERROR: tls: init_tls_h: openssl compile options"
						" mismatch: library has kerberos support"
						" %s and Kamailio tls %s (unstable configuration)\n"
						" (tls_force_run in kamailio.cfg will override this"
						" check)\n",
						lib_kerberos?"enabled":"disabled",
						kerberos_support?"enabled":"disabled"
				);
			if (cfg_get(tls, tls_cfg, force_run))
				LOG(L_WARN, "tls: init_tls_h: tls_force_run turned on, "
						"ignoring kerberos support mismatch\n");
			else
				return -1; /* exit, is safer */
		}else{
			LOG(L_WARN, "WARNING: tls: init_tls_h: openssl  compile options"
						" missing -- cannot detect if kerberos support is"
						" enabled. Possible unstable configuration\n");
		}
	}

	#ifdef TLS_KSSL_WORKARROUND
	/* if openssl compiled with kerberos support, and openssl < 0.9.8e-dev
	 * or openssl between 0.9.9-dev and 0.9.9-beta1 apply workaround for
	 * openssl bug #1467 */
	if (ssl_version < 0x00908050L || 
			(ssl_version >= 0x00909000L && ssl_version < 0x00909001L)){
		openssl_kssl_malloc_bug=1;
		LOG(L_WARN, "tls: init_tls_h: openssl kerberos malloc bug detected, "
			" kerberos support will be disabled...\n");
	}
	#endif
	/* set free memory threshold for openssl bug #1491 workaround */
	low_mem_threshold1 = cfg_get(tls, tls_cfg, low_mem_threshold1);
	low_mem_threshold2 = cfg_get(tls, tls_cfg, low_mem_threshold2);
	if (low_mem_threshold1<0){
		/* default */
		low_mem_threshold1=512*1024*get_max_procs();
	}else
		low_mem_threshold1*=1024; /* KB */
	if (low_mem_threshold2<0){
		/* default */
		low_mem_threshold2=256*1024*get_max_procs();
	}else
		low_mem_threshold2*=1024; /* KB */
	if ((low_mem_threshold1==0) || (low_mem_threshold2==0))
		LOG(L_WARN, "tls: openssl bug #1491 (crash/mem leaks on low memory)"
					" workarround disabled\n");
	else
		LOG(L_WARN, "tls: openssl bug #1491 (crash/mem leaks on low memory)"
				" workaround enabled (on low memory tls operations will fail"
				" preemptively) with free memory thresholds %d and %d bytes\n",
				low_mem_threshold1, low_mem_threshold2);
	
	if (shm_available()==(unsigned long)(-1)){
		LOG(L_WARN, "tls: Kamailio is compiled without MALLOC_STATS support:"
				" the workaround for low mem. openssl bugs will _not_ "
				"work\n");
		low_mem_threshold1=0;
		low_mem_threshold2=0;
	}
	if ((low_mem_threshold1 != cfg_get(tls, tls_cfg, low_mem_threshold1)) ||
	    (low_mem_threshold2 != cfg_get(tls, tls_cfg, low_mem_threshold2))) {
		/* ugly hack to set the initial values for the mem tresholds */
		if (cfg_register_ctx(&cfg_ctx, 0)) {
			ERR("failed to register cfg context\n");
			return -1;
		}
		tls_grp.s = "tls";
		tls_grp.len = strlen(tls_grp.s);
		s.s = "low_mem_threshold1";
		s.len = strlen(s.s);
		if (low_mem_threshold1 != cfg_get(tls, tls_cfg, low_mem_threshold1) &&
				cfg_set_now_int(cfg_ctx, &tls_grp, NULL /* group id */, &s, low_mem_threshold1)) {
			ERR("failed to set tls.low_mem_threshold1 to %d\n",
					low_mem_threshold1);
			return -1;
		}
		s.s = "low_mem_threshold2";
		s.len = strlen(s.s);
		if (low_mem_threshold2 != cfg_get(tls, tls_cfg, low_mem_threshold2) &&
				cfg_set_now_int(cfg_ctx, &tls_grp, NULL /* group id */, &s, low_mem_threshold2)) {
			ERR("failed to set tls.low_mem_threshold1 to %d\n",
					low_mem_threshold2);
			return -1;
		}
	}
	
	init_ssl_methods();
	tls_mod_initialized = 1;
	return 0;
}


/*
 * Make sure that all server domains in the configuration have corresponding
 * listening socket in SER
 */
int tls_check_sockets(tls_domains_cfg_t* cfg)
{
	tls_domain_t* d;

	if (!cfg) return 0;

	d = cfg->srv_list;
	while(d) {
		if (d->ip.len && !find_si(&d->ip, d->port, PROTO_TLS)) {
			ERR("%s: No listening socket found\n", tls_domain_str(d));
			return -1;
		}
		d = d->next;
	}
	return 0;
}


/*
 * TLS cleanup when SER exits
 */
void destroy_tls_h(void)
{
	DBG("tls module final tls destroy\n");
	if(tls_mod_preinitialized > 0)
		ERR_free_strings();
	/* TODO: free all the ctx'es */
	tls_destroy_cfg();
	tls_destroy_locks();
	tls_ct_wq_destroy();
}

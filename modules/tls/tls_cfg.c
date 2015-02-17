/* 
 * Kamailio TLS module
 *
 * Copyright (C) 2010 iptelorg GmbH
 * Copyright (C) 2013 Motorola Solutions, Inc.
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

/**
 * Kamailio TLS support :: tls runtime configuration
 * @file tls_cfg.c
 * @ingroup tls
 * Module: @ref tls
 */

#include "tls_cfg.h"
#include "../../config.h"
#include "../../str.h"
#include "../../ut.h"
#include "../../pt.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"

struct cfg_group_tls default_tls_cfg = {
	0, /* tls_force_run */
	STR_STATIC_INIT("TLSv1"), /* method */
	STR_NULL, /* server name (sni) */
	0, /* verify_certificate */
	9, /* verify_depth */
	0, /* require_certificate */
	STR_NULL, /* private_key (default value set in fix_tls_cfg) */
	STR_NULL, /* ca_list (default value set in fix_tls_cfg) */
	STR_NULL, /* crl (default value set in fix_tls_cfg) */
	STR_NULL, /* certificate (default value set in fix_tls_cfg) */
	STR_NULL, /* cipher_list (default value set in fix_tls_cfg) */
	0, /* session_cache */
	STR_STATIC_INIT("kamailio-tls-4.x.y"), /* session_id */
	STR_NULL, /* config_file */
	3, /* log  (L_DBG)*/
	3, /* debug (L_DBG) */
	600, /* con_lifetime (s)*/
	1, /* disable_compression */
#if OPENSSL_VERSION_NUMBER >= 0x01000000L
	1, /* ssl_release_buffers (on, avoid extra buffering) */
#else
	-1, /* ssl_release_buffers: old openssl, leave it untouched */
#endif /* openssl >= 1.0.0 */
#if OPENSSL_VERSION_NUMBER >= 0x01000000L && ! defined OPENSSL_NO_BUF_FREELISTS
	0, /* ssl_freelist_max  (immediately free) */
#else
	-1, /* ssl_freelist_max: old openssl, leave it untouched */
#endif /* openssl >= 1.0.0 */
	-1, /* ssl_max_send_fragment (use the default: 16k), requires openssl
		   > 0.9.9 */
	0, /* ssl_read_ahead (off, not needed, we have our own buffering BIO)*/
	-1, /* low_mem_threshold1 */
	-1, /* low_mem_threshold2 */
	10*1024*1024, /* ct_wq_max: 10 Mb by default */
	64*1024, /* con_ct_wq_max: 64Kb by default */
	4096, /* ct_wq_blk_size */
	0 /* send_close_notify (off by default)*/
};

volatile void* tls_cfg = &default_tls_cfg;


/* if *to<0 to=default_val, else if to>max_val to=max_val */
static void fix_timeout(char* name, int* to, int default_val, unsigned max_val)
{
	if (*to < 0) *to=default_val;
	else if ((unsigned)*to > max_val){
		WARN("%s: timeout too big (%u), the maximum value is %u\n",
				name, *to, max_val);
		*to=max_val;
	}
}


static int fix_con_lt(void* cfg_h, str* gname, str* name, void** val)
{
	int v;
	v=S_TO_TICKS((int)(long)*val);
	fix_timeout("tls.connection_timeout", &v,
					MAX_TLS_CON_LIFETIME, MAX_TLS_CON_LIFETIME);
	*val=(void*)(long)v;
	return 0;
}



/** cfg framework callback for fixing pathnames. */
static int fix_rel_pathname(void* cfg_h, str* gname, str* name, void** val)
{
	str* f;
	str new_f;
	/* the cfg framework will immediately "clone" the value so
	   we can pass a pointer to a temporary static buffer to it
	   (using a dyn. alloc'd buffer would introduce the additional
	    problem of having to free it somehow) */
	static char path_buf[MAX_PATH_SIZE];

	f = *val;
	/* use absolute paths, except when the path starts with
	   '.' (in this case leave it as it is) */
	if (f && f->s && f->len && *f->s != '.' && *f->s != '/') {
		new_f.s = get_abs_pathname(0, f);
		if (new_f.s == 0)
			return -1;
		new_f.len = strlen(new_f.s);
		if (new_f.len >= MAX_PATH_SIZE) {
			ERR("%.*s.%.*s path too long (%d bytes): \"%.*s\"\n",
					gname->len, gname->s, name->len, name->s,
					new_f.len, new_f.len, new_f.s);
			pkg_free(new_f.s);
			return -1;
		}
		memcpy(path_buf, new_f.s, new_f.len);
		pkg_free(new_f.s);
		new_f.s = path_buf;
		*f = new_f;
	}
	return 0;
}



cfg_def_t	tls_cfg_def[] = {
	{"force_run", CFG_VAR_INT | CFG_READONLY, 0, 1, 0, 0,
		"force loading the tls module even when initial sanity checks fail"},
	{"method",   CFG_VAR_STR | CFG_READONLY, 0, 0, 0, 0,
		"TLS method used (TLSv1.2, TLSv1.1, TLSv1, SSLv3, SSLv2, SSLv23)"},
	{"server_name",   CFG_VAR_STR | CFG_READONLY, 0, 0, 0, 0,
		"Server name (SNI)"},
	{"verify_certificate", CFG_VAR_INT | CFG_READONLY, 0, 1, 0, 0,
		"if enabled the certificates will be verified" },
	{"verify_depth", CFG_VAR_INT | CFG_READONLY, 0, 100, 0, 0,
		"sets how far up the certificate chain will the certificate"
		" verification go in the search for a trusted CA" },
	{"require_certificate", CFG_VAR_INT | CFG_READONLY, 0, 1, 0, 0,
		"if enabled a certificate will be required from clients" },
	{"private_key", CFG_VAR_STR | CFG_READONLY, 0, 0, 0, 0,
		"name of the file containing the private key (pem format), if not"
		" contained in the certificate file" },
	{"ca_list", CFG_VAR_STR | CFG_READONLY, 0, 0, 0, 0,
		"name of the file containing the trusted CA list (pem format)" },
	{"crl", CFG_VAR_STR | CFG_READONLY, 0, 0, 0, 0,
		"name of the file containing the CRL  (certificare revocation list"
			" in pem format)" },
	{"certificate", CFG_VAR_STR | CFG_READONLY, 0, 0, 0, 0,
		"name of the file containing the certificate (pem format)" },
	{"cipher_list", CFG_VAR_STR | CFG_READONLY, 0, 0, 0, 0,
		"list of the accepted ciphers (strings separated by colons)" },
	{"session_cache", CFG_VAR_INT | CFG_READONLY, 0, 1, 0, 0,
		"enables or disables the session cache" },
	{"session_id", CFG_VAR_STR | CFG_READONLY, 0, 0, 0, 0,
		"string used for the session id" },
	{"config", CFG_VAR_STR, 0, 0, fix_rel_pathname, 0,
		"tls config file name (used for the per domain options)" },
	{"log", CFG_VAR_INT | CFG_ATOMIC, 0, 1000, 0, 0,
		"tls info messages log level" },
	{"debug", CFG_VAR_INT | CFG_ATOMIC, 0, 1000, 0, 0,
		"tls debug messages log level" },
	{"connection_timeout", CFG_VAR_INT | CFG_ATOMIC,
							-1, MAX_TLS_CON_LIFETIME, fix_con_lt, 0,
		"initial connection lifetime (in s) (obsolete)" },
	{"disable_compression", CFG_VAR_INT | CFG_READONLY, 0, 1, 0, 0,
		"if set disable the built-in OpenSSL compression" },
	{"ssl_release_buffers", CFG_VAR_INT | CFG_READONLY, -1, 1, 0, 0,
		"quickly release internal OpenSSL read or write buffers."
	    " Works only for OpenSSL >= 1.0."},
	{"ssl_free_list_max", CFG_VAR_INT | CFG_READONLY, -1, 1<<30, 0, 0,
		"maximum number of free/cached memory chunks that OpenSSL"
		" will keep per connection. Works only for OpenSSL >= 1.0."},
	{"ssl_max_send_fragment", CFG_VAR_INT | CFG_READONLY, -1, 65536, 0, 0,
		"sets the maximum number of bytes (clear text) send into one TLS"
		" record. Valid values are between 512 and 16384."
		" Works only for OpenSSL >= 0.9.9"},
	{"ssl_read_ahead", CFG_VAR_INT | CFG_READONLY, -1, 1, 0, 0,
		"Enables read ahead, reducing the number of BIO read calls done"
		" internally by the OpenSSL library. Note that in newer tls"
	    " module versions it is better to have read ahead disabled, since"
		" everything it is buffered in memory anyway"},
	{"low_mem_threshold1", CFG_VAR_INT | CFG_ATOMIC, -1, 1<<30, 0, 0,
		"sets the minimum amount of free memory for accepting new TLS"
		" connections (KB)"},
	{"low_mem_threshold2", CFG_VAR_INT | CFG_ATOMIC, -1, 1<<30, 0, 0,
		"sets the minimum amount of free memory after which no more TLS"
		" operations will be attempted (even on existing connections)" },
	{"ct_wq_max", CFG_VAR_INT | CFG_ATOMIC, 0, 1<<30, 0, 0,
		"maximum bytes queued globally for write when write has to wait due"
		" to TLS-level renegotiation (SSL_ERROR_WANT_READ) or initial TLS"
		" connection establishment (it is different from tcp.wq_max,"
		" which works at the TCP connection level)"},
	{"con_ct_wq_max", CFG_VAR_INT | CFG_ATOMIC, 0, 4*1024*1024, 0, 0,
		"maximum bytes queued for write per connection when write has to wait"
		" due to TLS-level renegotiation (SSL_ERROR_WANT_READ) or initial TLS"
		" connection establishment (it is different from tcp.conn_wq_max,"
		" which works at the TCP connection level)"},
	{"ct_wq_blk_size", CFG_VAR_INT | CFG_ATOMIC, 1, 65536, 0, 0,
		"internal TLS pre-write (clear-text) queue minimum block size"
		" (advanced tunning or debugging for now)"},
	{"send_close_notify", CFG_VAR_INT | CFG_ATOMIC, 0, 1, 0, 0,
		"enable/disable sending a close notify TLS shutdown alert"
			" before closing the corresponding TCP connection."
			"Note that having it enabled has a performance impact."},
	{0, 0, 0, 0, 0, 0}
};



/** fix pathnames.
 * to be used on start-up, with pkg_alloc'ed path names  (path->s).
 * @param path - path to be fixed. If it starts with '.' or '/' is left alone
 *               (forced "relative" or "absolute" path). Otherwise the path
 *               is considered to be relative to the main config file directory
 *               (e.g. for /etc/ser/ser.cfg => /etc/ser/\<path\>).
 * @param def - default value used if path->s is empty (path->s == 0).
 * @return  0 on success, -1 on error.
 */
static int fix_initial_pathname(str* path, char* def)
{
	str new_path;
	if (path->s && path->len && *path->s != '.' && *path->s != '/') {
		new_path.s = get_abs_pathname(0, path);
		if (new_path.s == 0) return -1;
		new_path.len = strlen(new_path.s);
		pkg_free(path->s);
		*path = new_path;
	} else if (path->s == 0 && def) {
		/* use defaults */
		new_path.len = strlen(def);
		new_path.s = def;
		new_path.s = get_abs_pathname(0, &new_path);
		if (new_path.s == 0) return -1;
		new_path.len = strlen(new_path.s);
		*path = new_path;
	}
	return 0;
}



/** fix the tls cfg, prior to registering.
  * @return < 0 on error, 0 on success
  */
int fix_tls_cfg(struct cfg_group_tls* cfg)
{
	cfg->con_lifetime = S_TO_TICKS(cfg->con_lifetime);
	fix_timeout("tls_connection_timeout", &cfg->con_lifetime, 
						MAX_TLS_CON_LIFETIME, MAX_TLS_CON_LIFETIME);
	/* Update relative paths of files configured through modparams, relative
	 * pathnames will be converted to absolute and the directory of the main
	 * SER configuration file will be used as reference.
	 */
	if (fix_initial_pathname(&cfg->config_file, 0) < 0)
		return -1;
	if (fix_initial_pathname(&cfg->private_key, TLS_PKEY_FILE) < 0)
		return -1;
	if (fix_initial_pathname(&cfg->ca_list, TLS_CA_FILE) < 0 )
		return -1;
	if (fix_initial_pathname(&cfg->crl, TLS_CRL_FILE) < 0 )
		return -1;
	if (fix_initial_pathname(&cfg->certificate, TLS_CERT_FILE) < 0)
		return -1;
	
	return 0;
}




/* vi: set ts=4 sw=4 tw=79:ai:cindent: */

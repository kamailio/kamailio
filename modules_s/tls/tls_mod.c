/*
 * $Id$
 *
 * TLS module interface
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * Copyright (C) 2005,2006 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 * 2003-04-05: default_uri #define used (jiri)
 * 2003-04-06: db connection closed in mod_init (janakj)
 * 2004-06-06  updated to the new DB api, cleanup: static dbf & handler,
 *              calls to domain_db_{bind,init,close,ver} (andrei)
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "../../sr_module.h"
#include "../../ip_addr.h"
#include "../../trim.h"
#include "../../transport.h"
#include "../../globals.h"
#include "tls_init.h"
#include "tls_server.h"
#include "tls_domain.h"
#include "tls_select.h"
#include "tls_mod.h"

/*
 * FIXME:
 * - How do we ask for secret key password ? Mod_init is called after
 *   daemonize and thus has no console access
 * - forward_tls and t_relay_to_tls should be here
 * add tls_log
 */


/*
 * Module management function prototypes
 */
static int mod_init(void);
static void destroy(void);
static int child_init(int rank);

static int set_method      (modparam_t type, char* param);
static int set_verify_cert (modparam_t type, char* param);
static int set_verify_depth(modparam_t type, char* param);
static int set_require_cert(modparam_t type, char* param);
static int set_pkey_file   (modparam_t type, char* param);
static int set_ca_list     (modparam_t type, char* param);
static int set_certificate (modparam_t type, char* param);
static int set_cipher_list (modparam_t type, char* param);

MODULE_VERSION

int tls_handshake_timeout = 120;
int tls_send_timeout = 120;
int tls_conn_timeout = 600;
int tls_log = 3;
int tls_session_cache = 0;
str tls_session_id = STR_STATIC_INIT("ser-tls-0.9.0");

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"method",              PARAM_STRING | PARAM_USE_FUNC, (void*)set_method      },
	{"verify_certificate",  PARAM_STRING | PARAM_USE_FUNC, (void*)set_verify_cert },
	{"verify_depth",        PARAM_STRING | PARAM_USE_FUNC, (void*)set_verify_depth},
	{"require_certificate", PARAM_STRING | PARAM_USE_FUNC, (void*)set_require_cert},
	{"private_key",         PARAM_STRING | PARAM_USE_FUNC, (void*)set_pkey_file   },
	{"ca_list",             PARAM_STRING | PARAM_USE_FUNC, (void*)set_ca_list     },
	{"certificate",         PARAM_STRING | PARAM_USE_FUNC, (void*)set_certificate },
	{"cipher_list",         PARAM_STRING | PARAM_USE_FUNC, (void*)set_cipher_list },
	{"handshake_timeout",   PARAM_INT,                     &tls_handshake_timeout },
	{"send_timeout",        PARAM_INT,                     &tls_send_timeout      },
	{"connection_timeout",  PARAM_INT,                     &tls_conn_timeout      },
	{"tls_log",             PARAM_INT,                     &tls_log               },
	{"session_cache",       PARAM_INT,                     &tls_session_cache     },
	{"session_id",          PARAM_STR,                     &tls_session_id        },
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"tls",
	cmds,       /* Exported functions */
	0,          /* RPC methods */
	params,     /* Exported parameters */
	mod_init,   /* module initialization function */
	0,          /* response function*/
	destroy,    /* destroy function */
	0,          /* cancel function */
	child_init  /* per-child init function */
};



transport_t tls_transport = {
	PROTO_TLS,
	STR_STATIC_INIT("TLS"),
	TRANSPORT_SECURE | TRANSPORT_DGRAM,
	{ 
		.tcp = {
			tls_tcpconn_init,
			tls_tcpconn_clean,
			tls_close,
			tls_blocking_write,
			tls_read,
			tls_fix_read_conn,
		}
	},
	0
};


static int mod_init(void)
{
	if (init_tls() < 0) return -1;
	
	tls = &tls_transport;
	register_select_table(tls_sel);
	return 0;
}


static int child_init(int rank)
{
	if (rank == 1 && init_tls_child() < 0) return -1;
	return 0;
}


static void destroy(void)
{
	destroy_tls();
}


/*
 * Parse TLS domain specifier, the function will modify the input
 * string
 */
static int parse_domain(int* type, struct ip_addr* ip, unsigned short* port, char** text, char* val)
{
	static char buf[1024];
	static char ip_buf[IP_ADDR_MAX_STR_SIZE];
	char* fmt, *comma;
	char backup = 0;
	str s;
	int ret;

	*type = 0;
	s.s = val;
	s.len = strlen(val);
	trim_leading(&s);
	if (s.len >= 1024) {
		ERR("Input text is too long\n");
		return -1;
	}

	if (*s.s == '@') {
		*type |= TLS_DOMAIN_CLI;
		s.s++;
		s.len--;
	} else {
		*type |= TLS_DOMAIN_SRV;
	}

	if (!strchr(s.s, '=')) {
		DBG("No TLS domain specifier found\n");
		*text = s.s;
		*type |= TLS_DOMAIN_DEF;
		return 0;
	}

	memset(ip, 0, sizeof(struct ip_addr));
	if (*s.s == '[') {
		comma = strchr(s.s, ']');
		if (comma) {
			backup = *comma;
			*comma = ' ';
		}
		ip->af = AF_INET6;
		ip->len = 16;
		fmt = "\[%s :%hd = %s";
	} else {
		comma = strchr(s.s, ':');
		if (comma) {
			backup = *comma;
			*comma = ' ';
		}
		ip->af = AF_INET;
		ip->len = 4;
		fmt = "%s %hd = %s";
	}

	ret = sscanf(s.s, fmt, ip_buf, port, buf);
       	if (comma) *comma = backup;
	if (ret < 3) {
		ERR("Error while parsing TLS domain specification: '%s'\n", s.s);
		return -1;
	}
	
	if (inet_pton(ip->af, ip_buf, ip->u.addr) <= 0) {
		ERR("Invalid IP address in TLS domain: '%s'\n", ip_buf);
		return -1;
	}

	*text = buf;
	DBG("Found TLS domain: <%s>:<%d>,<%s>\n", ip_addr2a(ip), *port, *text);
	return 0;
}


static tls_domain_t* lookup_domain(char** val, char* param)
{
	struct ip_addr ip;
	unsigned short port;
	tls_domain_t* d;
	int ret, flags;
	
	flags = TLS_DOMAIN_SRV;

        ret = parse_domain(&flags, &ip, &port, val, param);
	if (ret < 0) {
		ERR("Error while parsing TLS module parameter '%s'\n", param);
		return 0;
	}

	d = tls_find_domain(flags, &ip, port);
	if (!d && !(d = tls_new_domain(flags, &ip, port))) {
		ERR("Error while creating new TLS domain for '%s'\n", param);
		return 0;
	}
	return d;
}


static int set_method(modparam_t type, char* param)
{
	tls_domain_t* d;
	char* val;
	
	d = lookup_domain(&val, param);
	if (!d) return -1;
	
	if (!strcasecmp(val, "SSLv2")) {
		d->method = TLS_USE_SSLv2;
	} else if (!strcasecmp(val, "SSLv3")) {
		d->method = TLS_USE_SSLv3;
	} else if (!strcasecmp(val, "SSLv23")) {
		d->method = TLS_USE_SSLv23;
	} else if (!strcasecmp(val, "TLSv1")) {
		d->method = TLS_USE_TLSv1;
	} else {
		ERR("Invalid tls::method parameter value: %s\n", val);
		return -1;
	}
	return 0;
}

static int set_verify_cert(modparam_t type, char* param)
{
	tls_domain_t* d;
	char* val;

	d = lookup_domain(&val, param);
	if (!d) return -1;

	if (!strcasecmp(val, "yes")     || 
	    !strcasecmp(val, "true")    ||
	    !strcasecmp(val, "on")      ||
	    !strcasecmp(val, "enable")  ||
	    !strcasecmp(val, "enabled") ||
	    *val == '1') {
		d->verify_cert = 1;
		return 0;
	}
	
	if (!strcasecmp(val, "no")       || 
	    !strcasecmp(val, "false")    ||
	    !strcasecmp(val, "off")      ||
	    !strcasecmp(val, "disable")  ||
	    !strcasecmp(val, "disabled") ||
	    *val == '0') {
		d->verify_cert = 0;
		return 0;
	}
	ERR("Invalid tls::verify_certificate parameter value: '%s'\n", val);
	return -1;
}


static int set_verify_depth(modparam_t type, char* param)
{
	tls_domain_t* d;
	char* val;

	d = lookup_domain(&val, param);
	if (!d) return -1;

	d->verify_depth = atoi(val);
	return 0;
}


static int set_require_cert(modparam_t type, char* param)
{
	tls_domain_t* d;
	char* val;

	d = lookup_domain(&val, param);
	if (!d) return -1;

	if (!strcasecmp(val, "yes")     || 
	    !strcasecmp(val, "true")    ||
	    !strcasecmp(val, "on")      ||
	    !strcasecmp(val, "enable")  ||
	    !strcasecmp(val, "enabled") ||
	    *val == '1') {
		d->require_cert = 1;
		return 0;
	}
	
	if (!strcasecmp(val, "no")       || 
	    !strcasecmp(val, "false")    ||
	    !strcasecmp(val, "off")      ||
	    !strcasecmp(val, "disable")  ||
	    !strcasecmp(val, "disabled") ||
	    *val == '0') {
		d->require_cert = 0;
		return 0;
	}
	ERR("Invalid tls::require_certificate parameter value: '%s'\n", val);
	return -1;
}


static int set_pkey_file(modparam_t type, char* param)
{
	tls_domain_t* d;
	char* val;

	d = lookup_domain(&val, param);
	if (!d) return -1;
	
	d->pkey_file = val;
	return 0;
}


static int set_ca_list(modparam_t type, char* param)
{
	tls_domain_t* d;
	char* val;

	d = lookup_domain(&val, param);
	if (!d) return -1;
	
	d->ca_file = val;
	return 0;
}


static int set_certificate(modparam_t type, char* param)
{
	tls_domain_t* d;
	char* val;

	d = lookup_domain(&val, param);
	if (!d) return -1;
	
	d->cert_file = val;
	return 0;
}


static int set_cipher_list(modparam_t type, char* param)
{
	tls_domain_t* d;
	char* val;

	d = lookup_domain(&val, param);
	if (!d) return -1;

	d->cipher_list = val;
	return 0;
}

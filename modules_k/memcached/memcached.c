/*
 * $Id$
 *
 * Copyright (C) 2009 Henning Westerholt
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*!
 * \file
 * \brief memcached module
 */

#include "memcached.h"
#include "mcd_var.h"

#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../str.h"


MODULE_VERSION


#define DP_ALERT_TEXT    "ALERT:"
#define DP_ERR_TEXT      "ERROR:"
#define DP_WARN_TEXT     "WARNING:"
#define DP_NOTICE_TEXT   "NOTICE:"
#define DP_INFO_TEXT     "INFO:"


/*! server string */
char* memcached_srv_str = "localhost:11211";
/*! cache expire time in seconds */
unsigned int memcached_expire = 10800;
/*! cache storage mode, set or add */
unsigned int memcached_mode = 0;
/*! server timeout in ms*/
int memcached_timeout = 5000;
/*! memcached handle */
struct memcache* memcached_h = NULL;


static int mod_init(void);

static void mod_destroy(void);


/*!
 * Exported pseudo-variables
 */
static pv_export_t mod_pvs[] = {
	{ {"mct", sizeof("mct")-1}, PVT_OTHER, pv_get_mcd_value, pv_set_mcd_value,
		pv_parse_mcd_name, 0, 0, 0 },
	{ {"mcinc", sizeof("mcinc")-1}, PVT_OTHER, pv_get_mcd_value, pv_inc_mcd_value,
		pv_parse_mcd_name, 0, 0, 0 },
	{ {"mcdec", sizeof("mcdec")-1}, PVT_OTHER, pv_get_mcd_value, pv_dec_mcd_value,
		pv_parse_mcd_name, 0, 0, 0 },
	{ {"mctex", sizeof("mctex")-1}, PVT_OTHER, pv_get_null, pv_set_mcd_expire,
		pv_parse_mcd_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


/*!
 * Exported parameters
 */
static param_export_t params[] = {
	{"servers", STR_PARAM, &memcached_srv_str },
	{"expire",   INT_PARAM, &memcached_expire },
	{"timeout", INT_PARAM, &memcached_timeout },
	{"mode",    INT_PARAM, &memcached_mode },
	{0, 0, 0}
};


/*!
 * Module interface
 */
struct module_exports exports = {
	"memcached",
	DEFAULT_DLFLAGS,
	0,
	params,
	0,
	0,
	mod_pvs,
	0,
	mod_init,
	0,
	mod_destroy,
	0
};


/*!
 * \brief Wrapper functions around our internal memory management
 * \param mem freed memory
 * \see pkg_free
 */
static inline void memcached_free(void *mem) {
	pkg_free(mem);
}


/*!
 * \brief Wrapper functions around our internal memory management
 * \param size allocated size
 * \return allocated memory, or NULL on failure
 * \see pkg_malloc
 */
static inline void* memcached_malloc(const size_t size) {
	return pkg_malloc(size);
}


/*!
 * \brief Wrapper functions around our internal memory management
 * \param mem pointer to allocated memory
 * \param size new size of memory area
 * \return allocated memory, or NULL on failure
 * \see pkg_realloc
 */
static inline void* memcached_realloc(void *mem, const size_t size) {
 	return pkg_realloc(mem, size);
}


/*!
 * \brief Small wrapper around our internal logging function
 */
static int memcache_err_func(MCM_ERR_FUNC_ARGS) {

	const struct memcache_ctxt *ctxt;
	struct memcache_err_ctxt *ectxt;
	int error_level;
	const char * error_str;

	MCM_ERR_INIT_CTXT(ctxt, ectxt);

	switch (ectxt->severity) {
		case MCM_ERR_LVL_INFO:
			error_level = L_INFO;
			error_str = DP_INFO_TEXT;
			break;
		case MCM_ERR_LVL_NOTICE:
			error_level = L_NOTICE;
			error_str = DP_NOTICE_TEXT;
			break;
		case MCM_ERR_LVL_WARN:
			error_level = L_WARN;
			error_str = DP_WARN_TEXT;
			break;
		case MCM_ERR_LVL_ERR:
			error_level = L_ERR;
			error_str  = DP_ERR_TEXT;
			/* try to continue */
 			ectxt->cont = 'y';
			break;
		case MCM_ERR_LVL_FATAL:
  		default:
			error_level = L_ALERT;
			error_str = DP_ALERT_TEXT;
			ectxt->cont = 'y';
			break;
	}

	/*
	* ectxt->errmsg - per error message passed along via one of the MCM_*_MSG() macros (optional)
	* ectxt->errstr - memcache error string (optional, though almost always set)
	*/
	if (ectxt->errstr != NULL && ectxt->errmsg != NULL)
		LM_GEN1(error_level, "%s memcached: %s():%u: %s: %.*s\n", error_str, ectxt->funcname, ectxt->lineno, ectxt->errstr,
			(int)ectxt->errlen, ectxt->errmsg);
	else if (ectxt->errstr == NULL && ectxt->errmsg != NULL)
		LM_GEN1(error_level, "%s memcached: %s():%u: %.*s\n", error_str, ectxt->funcname, ectxt->lineno, (int)ectxt->errlen,
			ectxt->errmsg);
	else if (ectxt->errstr != NULL && ectxt->errmsg == NULL)
		LM_GEN1(error_level, "%s memcached: %s():%u: %s\n", error_str, ectxt->funcname, ectxt->lineno, ectxt->errstr);
	else
		LM_GEN1(error_level, "%s memcached: %s():%u\n", error_str, ectxt->funcname, ectxt->lineno);

	return 0;
}


/*!
 * \brief Module initialization function
 * \return 0 on success, -1 on failure
 */
static int mod_init(void) {
	char *server, *port;
	unsigned int len = 0;

	/* setup the callbacks to our internal memory manager */
	if (mcMemSetup(memcached_free, memcached_malloc,
			memcached_malloc, memcached_realloc) != 0) {
		LM_ERR("could not setup memory management callbacks\n");
		return -1;
	}

	if (mcErrSetup(memcache_err_func) != 0) {
		LM_ERR("could not setup error handler callback\n");
		return -1;
	}

	/*! delete eventual log filters */
	mc_err_filter_del(MCM_ERR_LVL_INFO);
	mc_err_filter_del(MCM_ERR_LVL_NOTICE);

	memcached_h = mc_new();
	if (memcached_h == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	
	if ((port = strchr(memcached_srv_str, ':')) != NULL) {
		port = port + 1;
		len = strlen(memcached_srv_str) - strlen(port) - 1;
	} else {
		LM_DBG("no port definition, using default port\n");
		port = "11211";
		len = strlen(memcached_srv_str) ;
	}
	

	server = pkg_malloc(len);
	if (server == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}

	strncpy(server, memcached_srv_str, len);
	server[len] = '\0';

	mc_timeout(memcached_h, 0, memcached_timeout);

	if (mc_server_add(memcached_h, server, port) != 0) {
		LM_ERR("could not add server %s:%s\n", server, port);
		return -1;
	}
	LM_INFO("connected to server %s:%s\n", server, port);
	pkg_free(server);

	LM_INFO("memcached client version is %s, released on %d\n", mc_version(), mc_reldate());
	return 0;
}


/*!
 * \brief Module shutdown function
 */
static void mod_destroy(void) {
	if (memcached_h != NULL)
		mc_server_disconnect_all(memcached_h);

	if (memcached_h != NULL)
		mc_free(memcached_h);
}

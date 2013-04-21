/*
 * Copyright (C) 2009, 2013 Henning Westerholt
 * Copyright (C) 2013 Charles Chance, sipcentric.com
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


/*! server string */
char* mcd_srv_str = "localhost:11211";
/*! cache (default) expire time in seconds */
unsigned int mcd_expire = 0;
/*! cache storage mode, set or add */
unsigned int mcd_mode = 0;
/*! server timeout in ms*/
int mcd_timeout = 5000;
/*! memcached handle */
struct memcached_st *memcached_h;
/*! memcached server list */
struct memcached_server_st *servers;


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
	{"servers", STR_PARAM, &mcd_srv_str },
	{"expire",   INT_PARAM, &mcd_expire },
	{"timeout", INT_PARAM, &mcd_timeout },
	{"mode",    INT_PARAM, &mcd_mode },
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
static inline void mcd_free(void *mem) {
	pkg_free(mem);
}


/*!
 * \brief Wrapper functions around our internal memory management
 * \param size allocated size
 * \return allocated memory, or NULL on failure
 * \see pkg_malloc
 */
static inline void* mcd_malloc(const size_t size) {
	return pkg_malloc(size);
}


/*!
 * \brief Wrapper functions around our internal memory management
 * \param mem pointer to allocated memory
 * \param size new size of memory area
 * \return allocated memory, or NULL on failure
 * \see pkg_realloc
 */
static inline void* mcd_realloc(void *mem, const size_t size) {
 	return pkg_realloc(mem, size);
}

/*!
 * \brief Module initialization function
 * \return 0 on success, -1 on failure
 */
static int mod_init(void) {
	char *server, *port;
	unsigned int len = 0;
	memcached_return rc;

	if ((port = strchr(mcd_srv_str, ':')) != NULL) {
		port = port + 1;
		len = strlen(mcd_srv_str) - strlen(port) - 1;
	} else {
		LM_DBG("no port definition, using default port\n");
		port = "11211";
		len = strlen(mcd_srv_str) ;
	}
	
	server = pkg_malloc(len);
	if (server == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}

	strncpy(server, mcd_srv_str, len);
	server[len] = '\0';

        servers = memcached_server_list_append(servers, server, atoi(port), &rc);

	memcached_h = memcached_create(NULL);
	if (memcached_h == NULL) {
		LM_ERR("could not create memcached structure\n");
		return -1;
	}
	LM_DBG("allocated new server handle at %p", memcached_h);
	
	#ifdef MEMCACHED_ENABLE_DEPRECATED
	/** 
	 * \note Set callbacks to our internal memory manager
	 * \bug this don't work for now
	 * \todo Move to new memcached_set_memory_allocators function, this deprecated since 0.32
	 * 
	 * MEMCACHED_CALLBACK_MALLOC_FUNCTION
	 * This alllows yout to pass in a customized version of malloc that
	 * will be used instead of the builtin malloc(3) call. The prototype
	 * for this is:
	 * void *(*memcached_malloc_function)(memcached_st *ptr, const size_t size);
	 * 
	 * MEMCACHED_CALLBACK_REALLOC_FUNCTION
	 * This alllows yout to pass in a customized version of realloc that
	 * will be used instead of the builtin realloc(3) call. The prototype
	 * for this is:
	 * void *(*memcached_realloc_function)(memcached_st *ptr, void *mem, const size_t size);
	 * 
	 * MEMCACHED_CALLBACK_FREE_FUNCTION
	 * This alllows yout to pass in a customized version of realloc that
	 * will be used instead of the builtin free(3) call. The prototype
	 * for this is:
	 * typedef void (*memcached_free_function)(memcached_st *ptr, void *mem);
	 */
	LM_DBG("set memory manager callbacks");	
	if (memcached_callback_set(memcached_h, MEMCACHED_CALLBACK_MALLOC_FUNCTION, mcd_free) != MEMCACHED_SUCCESS) {
		LM_ERR("could not set malloc callback handler");
		return -1;
	}
	if (memcached_callback_set(memcached_h, MEMCACHED_CALLBACK_REALLOC_FUNCTION, mcd_realloc) != MEMCACHED_SUCCESS) {
		LM_ERR("could not set realloc callback handler");
		return -1;
	}
	if (memcached_callback_set(memcached_h, MEMCACHED_CALLBACK_FREE_FUNCTION, mcd_free) != MEMCACHED_SUCCESS) {
		LM_ERR("could not set free callback handler");
		return -1;
	}
	LM_DBG("memory manager callbacks set");
	#endif
	
	if (memcached_behavior_set(memcached_h, MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT, mcd_timeout) != MEMCACHED_SUCCESS) {
		LM_ERR("could not set server connection timeout");
		return -1;
	}
	rc = memcached_server_push(memcached_h, servers);
	if (rc == MEMCACHED_SUCCESS) {
		LM_DBG("added server list to structure\n");
	} else {
		LM_ERR("attempt to add server list to structure returned %s.\n", memcached_strerror(memcached_h, rc));
		return -1;
	}

	pkg_free(server);
	
	LM_INFO("libmemcached version is %s\n", memcached_lib_version());
	return 0;
}


/*!
 * \brief Module shutdown function
 */
static void mod_destroy(void) {
	if (memcached_h != NULL)
		memcached_free(memcached_h);
	if (servers != NULL)
		memcached_server_list_free(servers);
}

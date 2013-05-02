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
static inline void mcd_free(memcached_st *ptr, void *mem, void *context) {
	pkg_free(mem);
}


/*!
 * \brief Wrapper functions around our internal memory management
 * \param size allocated size
 * \return allocated memory, or NULL on failure
 * \see pkg_malloc
 */
static inline void* mcd_malloc(memcached_st *ptr, const size_t size, void *context) {
	return pkg_malloc(size);
}


/*!
 * \brief Wrapper functions around our internal memory management
 * \param mem pointer to allocated memory
 * \param size new size of memory area
 * \return allocated memory, or NULL on failure
 * \see pkg_realloc
 */
static inline void* mcd_realloc(memcached_st *ptr, void *mem, const size_t size, void *context) {
 	return pkg_realloc(mem, size);
}


/*!
 * \brief Wrapper functions around our internal memory management
 * \param mem pointer to allocated memory
 * \param size new size of memory area
 * \return allocated memory, or NULL on failure
 * \see pkg_malloc
 * \todo this is not optimal, 	use internal calloc implemention which is not exported yet
 */
static inline void * mcd_calloc(memcached_st *ptr, size_t nelem, const size_t elsize, void *context) {
	void* tmp = NULL;
	tmp = pkg_malloc(nelem * elsize);
	if (tmp != NULL) {
		memset(tmp, 0, nelem * elsize);
	}
	return tmp;
}

/**
 * \brief Callback to check if we could connect successfully to a server
 * \param ptr memcached handler
 * \param server server instance
 * \param context context for callback
 * \return MEMCACHED_SUCCESS on success, MEMCACHED_CONNECTION_FAILURE on failure
 * \todo FIXME
static inline memcached_server_fn mcd_check_connection(const memcached_st *ptr, memcached_server_instance_st my_server, void *context) {
	if (my_server->fd < 0) {
		return MEMCACHED_CONNECTION_FAILURE;
	}
	return MEMCACHED_SUCCESS;
}
*/

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

	memcached_h = memcached_create(NULL);
	if (memcached_h == NULL) {
		LM_ERR("could not create memcached structure\n");
		return -1;
	}
	LM_DBG("allocated new server handle at %p", memcached_h);

	LM_DBG("set memory manager callbacks");
	rc = memcached_set_memory_allocators(memcached_h, (memcached_malloc_fn)mcd_malloc,
					     (memcached_free_fn)mcd_free, (memcached_realloc_fn)mcd_realloc,
					     (memcached_calloc_fn)mcd_calloc, NULL);
	if (rc == MEMCACHED_SUCCESS) {
		LM_DBG("memory manager callbacks set");
	} else {
		LM_ERR("memory manager callbacks not set, returned %s.\n", memcached_strerror(memcached_h, rc));
		return -1;
	}
	
        servers = memcached_server_list_append(servers, server, atoi(port), &rc);
	
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

	/** \todo FIXME logic to handle connection errors on startup
	memcached_server_cursor(memcached_h, (const memcached_server_fn*) &mcd_check_connection, NULL, 1);
	*/
	
	LM_INFO("libmemcached version is %s\n", memcached_lib_version());
	return 0;
}


/*!
 * \brief Module shutdown function
 */
static void mod_destroy(void) {
	memcached_return rc;
	
	if (servers != NULL)
		memcached_server_list_free(servers);
	
	/* unset custom memory manager to enable clean shutdown of in system memory allocated server structure */
	LM_DBG("remove memory manager callbacks");
	rc = memcached_set_memory_allocators(memcached_h, NULL, NULL, NULL, NULL, NULL);
	if (rc == MEMCACHED_SUCCESS) {
		LM_DBG("memory manager callbacks removed");
	} else {
		LM_ERR("memory manager callbacks not removed, returned %s but continue anyway.\n", memcached_strerror(memcached_h, rc));
	}
	
	if (memcached_h != NULL)
		memcached_free(memcached_h);
}

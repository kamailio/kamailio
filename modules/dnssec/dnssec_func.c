/*
 * DNSSEC module
 *
 * Copyright (C) 2013 mariuszbi@gmail.com
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
 *
 */
/*!
 * \brief DNSsec support
 * \ingroup DNSsec 
 * \author mariuszbi@gmail.com
 */
/*!
 * \defgroup DNSsec DNS security extensions support
 *
 */


#include <validator/validator-config.h>
#include <validator/validator.h>
#include <validator/resolver.h>

#include "../../dprint.h"
#include "dnssec_func.h"

static struct libval_context  *libval_ctx = NULL;
static unsigned int context_flags = 0;


unsigned int
set_context_flags(unsigned int flags) {
#define CHECK_AND_SET(flag) \
	if ((flag & flags) != 0) {\
			context_flags |= VAL_##flag;\
			LOG(L_INFO, "setting param %s\n", #flag);\
	}
	unsigned int old_flags = context_flags;
	context_flags = 0;

	CHECK_AND_SET(QUERY_DONT_VALIDATE);
	CHECK_AND_SET(QUERY_IGNORE_SKEW);
	CHECK_AND_SET(QUERY_AC_DETAIL);
	CHECK_AND_SET(QUERY_NO_DLV);  
	CHECK_AND_SET(QUERY_NO_EDNS0_FALLBACK);
	CHECK_AND_SET(QUERY_RECURSE);
 	CHECK_AND_SET(QUERY_SKIP_RESOLVER);
 	CHECK_AND_SET(QUERY_SKIP_CACHE);

	return old_flags;
}

static inline int
dnssec_init_context(void) {
  	if (libval_ctx == NULL) {
    	if (val_create_context(NULL, &libval_ctx) != VAL_NO_ERROR)
	  		return -1;
		if (context_flags != 0) {
	  		val_context_setqflags(libval_ctx, VAL_CTX_FLAG_SET, context_flags);
		}	
  	}
  	return 0;
}

struct hostent *
dnssec_gethostbyname(const char *name) {
  	val_status_t          val_status;
  	struct hostent *      res;

  	if (dnssec_init_context())
    	return NULL;

  	LOG(L_INFO, " gethostbyname(%s) called: wrapper\n", name);
  
  	res = val_gethostbyname(libval_ctx, name, &val_status);

  	if (val_istrusted(val_status) && !val_does_not_exist(val_status)) {
   		return res;
  	} 
  	return NULL; 
}


struct hostent *
dnssec_gethostbyname2(const char *name, int family) {
  	val_status_t          val_status;
  	struct hostent *      res;

  	if (dnssec_init_context())
    	return NULL;

  	LOG(L_INFO, " gethostbyname2(%s) called: wrapper\n", name);
  
  	res = val_gethostbyname2(libval_ctx, name, family,  &val_status);

  	if (val_istrusted(val_status) && !val_does_not_exist(val_status)) {
      	return res;
  	}
  	return NULL; 
}

int
dnssec_res_init(void) {
  	LOG(L_INFO, "res_init called: wrapper\n");

  	return dnssec_init_context();
}

int
dnssec_res_destroy(void) {
	LOG(L_INFO, "destroying dnssec context\n");
	val_free_context(libval_ctx);
	libval_ctx = NULL;
	return 0;
}


int
dnssec_res_search(const char *dname, int class_h, int type_h, 
	  unsigned char *answer, int anslen) {
  	val_status_t          val_status;
  	int ret;

  	if (dnssec_init_context())
    	return -1;

  	LOG(L_INFO, "res_query(%s,%d,%d) called: wrapper\n",
	  	dname, class_h, type_h);

  	ret = val_res_search(libval_ctx, dname, class_h, type_h, answer, anslen,
			&val_status);

  	if (val_istrusted(val_status) && !val_does_not_exist(val_status)) {
		return ret;
  	} else {
		LOG(L_INFO, "invalid domain %s reason %s\n", dname, p_val_status(val_status));
	}

 	return -1;
}


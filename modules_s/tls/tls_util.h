/*
 * $Id$
 *
 * TLS module - common functions
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * COpyright (C) 2005 iptelorg GmbH
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
 */

#ifndef _TLS_UTIL_H
#define _TLS_UTIL_H

#include <openssl/err.h>
#include "../../dprint.h"
#include "../../str.h"
#include "tls_domain.h"


#define TLS_ERR_RET(r, s)                               \
do {                                                    \
	long err;                                       \
        (r) = 0;                                        \
	if ((*tls_cfg)->srv_default->ctx &&             \
	    (*tls_cfg)->srv_default->ctx[0]) {          \
		while((err = ERR_get_error())) {        \
			(r) = 1;                        \
			ERR("%s%s\n", ((s)) ? (s) : "", \
			    ERR_error_string(err, 0));  \
		}                                       \
	}                                               \
} while(0)


#define TLS_ERR(s)           \
do {                         \
	int ret;             \
	TLS_ERR_RET(ret, s); \
} while(0)


/*
 * Make a shared memory copy of str string
 * Return value: -1 on error
 *                0 on success
 */
int shm_str_dup(char** dest, str* val);


/*
 * Make a shared memory copy of ASCII zero terminated string
 * Return value: -1 on error
 *                0 on success
 */
int shm_asciiz_dup(char** dest, char* val);


/*
 * Delete old TLS configuration that is not needed anymore
 */
void collect_garbage(void);


/*
 * Get full path name of file, if the parameter does
 * not start with / then the value of CFG_DIR will
 * be used as prefix
 * The string returned by the function must be
 * freed using pkg_free
 */
char* get_pathname(str* filename);


#endif /* _TLS_UTIL_H */

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

#define _GNU_SOURCE 1 /* Needed for strndup */

#include <string.h>
#include <libgen.h>
#include <malloc.h>
#include "../../mem/shm_mem.h"
#include "../../globals.h"
#include "tls_mod.h"
#include "tls_util.h"


/*
 * Make a shared memory copy of str string
 * Return value: -1 on error
 *                0 on success
 */
int shm_str_dup(char** dest, str* val)
{
	char* ret;

	if (!val) return 0;

	ret = shm_malloc(val->len + 1);
	if (!ret) {
		ERR("No memory left\n");
		return 1;
	}
	memcpy(ret, val->s, val->len);
	ret[val->len] = '\0';
	*dest = ret;
	return 0;
}


/*
 * Make a shared memory copy of ASCII zero terminated string
 * Return value: -1 on error
 *                0 on success
 */
int shm_asciiz_dup(char** dest, char* val)
{
	char* ret;
	int len;

	if (!val) return 0;

	len = strlen(val);
	ret = shm_malloc(len + 1);
	if (!ret) {
		ERR("No memory left\n");
		return -1;
	}
	memcpy(ret, val, len + 1);
	*dest = ret;
        return 0;
}


/*
 * Delete old TLS configuration that is not needed anymore
 */
void collect_garbage(void)
{
	tls_cfg_t* prev, *cur;

	     /* Make sure we do not run two garbage collectors
	      * at the same time
	      */
	lock_get(tls_cfg_lock);

	     /* Skip the current configuration, garbage starts
	      * with the 2nd element on the list
	      */
	prev = *tls_cfg;
	cur = (*tls_cfg)->next;

	while(cur) {
		if (cur->ref_count == 0) {
			     /* Not referenced by any existing connection */
			prev->next = cur->next;
			tls_free_cfg(cur);
		}

		prev = cur;
		cur = cur->next;
	}

	lock_release(tls_cfg_lock);
}

/** Get full pathname of file. This function returns the full pathname of a
 * file in parameter. If the parameter does not start with / then the pathname
 * of the file will be relative to the pathname of the main SER configuration
 * file.
 * @param filename A pathname to be converted to absolute.
 * @return A string containing absolute pathname, the string
 *         must be freed with free.
 */
char* get_pathname(str* file)
{
	char* buf, *dir, *res;
	int len;

	if (!file || !file->s || file->len <= 0 || !cfg_file) {
		BUG("tls: Cannot get full pathname of file\n");
		return NULL;
	}

	if (file->s[0] == '/') {
		/* This is an absolute pathname, make a zero terminated
		 * copy and use it as it is */
		if ((res = strndup(file->s, file->len)) == NULL) {
			ERR("tls: No memory left (strndup failed)\n");
		}
	} else {
		/* This is not an absolute pathname, make it relative
		 * to the location of the main SER configuration file
		 */
		/* Make a copy, function dirname may modify the string */
		if ((buf = strdup(cfg_file)) == NULL) {
			ERR("tls: No memory left (strdup failed)\n");
			return NULL;
		}
		dir = dirname(buf);

		len = strlen(dir);
		if ((res = malloc(len + 1 + file->len + 1)) == NULL) {
			ERR("tls: No memory left (malloc failed)\n");
			free(buf);
			return NULL;
		}
		memcpy(res, dir, len);
		res[len] = '/';
		memcpy(res + len + 1, file->s, file->len);
		res[len + 1 + file->len] = '\0';
		free(buf);
	}
	return res;
}

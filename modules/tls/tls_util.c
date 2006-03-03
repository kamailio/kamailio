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

#include <string.h>
#include "../../mem/shm_mem.h"
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


/*
 * Get full path name of file, if the parameter does
 * not start with / then the value of CFG_DIR will
 * be used as prefix
 * The string returned by the function must be
 * freed using pkg_free
 */
char* get_pathname(str* filename)
{
	char* res;
	int len;

	if (filename->s[0] == '/') {
		res = pkg_malloc(filename->len + 1);
		if (!res) {
			ERR("No memory left\n");
			return 0;
		}
		memcpy(res, filename->s, filename->len);
		res[filename->len] = '\0';
	} else {
		len = strlen(CFG_DIR) + filename->len;
		res = pkg_malloc(len + 1);
		if (!res) {
			ERR("No memory left\n");
			return 0;
		}
		memcpy(res, CFG_DIR, sizeof(CFG_DIR) - 1);
		memcpy(res + sizeof(CFG_DIR) - 1, filename->s, filename->len);
		res[sizeof(CFG_DIR) - 1 + filename->len] = '\0';
	}
	return res;
}
